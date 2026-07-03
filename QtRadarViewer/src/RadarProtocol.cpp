#include "RadarProtocol.h"

#include <QRegularExpression>
#include <limits>

RadarProtocol::RadarProtocol(QObject *parent) : QObject(parent) {}

void RadarProtocol::reset() {
    m_inTargetBlock = false;
    m_expectedTargets = 0;
    m_buildingTargets.clear();
    m_currentTarget.reset();
    m_collectingHelp = false;
    m_helpLines.clear();
}

void RadarProtocol::feedLine(const QString &lineIn) {
    QString trimmed = lineIn.trimmed();
    emit rawLine(trimmed.isEmpty() ? lineIn : trimmed);

    if (trimmed.isEmpty()) {
        if (m_collectingHelp) {
            emit helpTextReceived(m_helpLines);
            m_collectingHelp = false;
            m_helpLines.clear();
        }
        return;
    }

    // ---- HELP command output: "=== COMMANDS ===" then "NAME - description" lines.
    static const QRegularExpression reCommandsHeader(R"(^===\s*COMMANDS\s*===$)");
    static const QRegularExpression reHelpLine(R"(^[A-Z]+\s*-\s*.+$)");
    if (reCommandsHeader.match(trimmed).hasMatch()) {
        m_collectingHelp = true;
        m_helpLines.clear();
        return;
    }
    if (m_collectingHelp) {
        if (reHelpLine.match(trimmed).hasMatch()) {
            m_helpLines << trimmed;
            return;
        }
        emit helpTextReceived(m_helpLines);
        m_collectingHelp = false;
        m_helpLines.clear();
        // fall through, this line still needs normal processing
    }

    // ---- "Targets: N" ... per-target fields ... "-------------------------"
    bool handled = false;
    handleTargetBlockLine(trimmed, handled);
    if (handled) return;

    // ---- Zone occupancy status block, refreshed every ~2s by the firmware.
    static const QRegularExpression reZoneStatusHeader(R"(^===\s*ZONE STATUS\s*===$)");
    static const QRegularExpression reZoneStatusActive(R"(^\[X\]\s*([A-D][1-4])$)");
    static const QRegularExpression reActiveTracks(R"(^Active tracks:\s*(\d+)$)");
    if (reZoneStatusHeader.match(trimmed).hasMatch()) {
        emit zoneStatusResetRequested();
        return;
    }
    if (auto m = reZoneStatusActive.match(trimmed); m.hasMatch()) {
        emit zoneOccupancyChanged(m.captured(1), true);
        return;
    }
    if (auto m = reActiveTracks.match(trimmed); m.hasMatch()) {
        emit activeTrackCountUpdated(m.captured(1).toInt());
        return;
    }

    // ---- Real-time zone ON/OFF transitions (hysteresis in firmware).
    static const QRegularExpression reZoneOn(R"(^Zone ON:\s*([A-D][1-4])$)");
    static const QRegularExpression reZoneOff(R"(^Zone OFF:\s*([A-D][1-4])$)");
    if (auto m = reZoneOn.match(trimmed); m.hasMatch()) {
        emit zoneOccupancyChanged(m.captured(1), true);
        return;
    }
    if (auto m = reZoneOff.match(trimmed); m.hasMatch()) {
        emit zoneOccupancyChanged(m.captured(1), false);
        return;
    }

    // ---- Zone geometry, from either the boot dump ("Zone 0 (A1): X=[...] Y=[...]")
    // or the ZONES command reply ("A1: X=[...] Y=[...]").
    static const QRegularExpression reZoneDef(
        R"(^(?:Zone\s+\d+\s+\()?([A-D][1-4])\)?:\s*X=\[(-?[\d.]+),(-?[\d.]+)\]\s*Y=\[(-?[\d.]+),(-?[\d.]+)\]$)");
    if (auto m = reZoneDef.match(trimmed); m.hasMatch()) {
        ZoneDef z;
        z.name = m.captured(1);
        z.xMin = m.captured(2).toDouble();
        z.xMax = m.captured(3).toDouble();
        z.yMin = m.captured(4).toDouble();
        z.yMax = m.captured(5).toDouble();
        emit zoneDefinitionUpdated(z);
        return;
    }

    // ---- Track lifecycle.
    static const QRegularExpression reNewTrack(R"(^New track (\d+) at \((-?[\d.]+),\s*(-?[\d.]+)\)$)");
    static const QRegularExpression reLostTrack(R"(^Lost track (\d+)$)");
    static const QRegularExpression reCurrentTrackLine(
        R"(^Track (\d+):\s*\((-?[\d.]+),\s*(-?[\d.]+)\)\s*Zone:\s*(NONE|[A-D][1-4])$)");
    static const QRegularExpression reTrackIsInZone(
        R"(^Track (\d+) is in (?:zone ([A-D][1-4])|NO ZONE) at \((-?[\d.]+),\s*(-?[\d.]+)\)$)");
    static const QRegularExpression reEnteredZone(R"(^Track (\d+) entered zone:\s*([A-D][1-4])$)");

    if (auto m = reNewTrack.match(trimmed); m.hasMatch()) {
        emit trackCreated(m.captured(1).toInt(), m.captured(2).toDouble(), m.captured(3).toDouble());
        return;
    }
    if (auto m = reLostTrack.match(trimmed); m.hasMatch()) {
        emit trackLost(m.captured(1).toInt());
        return;
    }
    if (auto m = reCurrentTrackLine.match(trimmed); m.hasMatch()) {
        const QString zone = m.captured(4) == QLatin1String("NONE") ? QString() : m.captured(4);
        emit trackPositionUpdated(m.captured(1).toInt(), m.captured(2).toDouble(), m.captured(3).toDouble(), zone);
        return;
    }
    if (auto m = reTrackIsInZone.match(trimmed); m.hasMatch()) {
        emit trackPositionUpdated(m.captured(1).toInt(), m.captured(3).toDouble(), m.captured(4).toDouble(),
                                   m.captured(2));
        return;
    }
    if (auto m = reEnteredZone.match(trimmed); m.hasMatch()) {
        // Position unknown from this line alone; only the zone label changed.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        emit trackPositionUpdated(m.captured(1).toInt(), nan, nan, m.captured(2));
        return;
    }

    // ---- Device setting echoes (DEBUG / MULTI / EMA toggles + boot-time mode report).
    static const QRegularExpression reDebugSetting(R"(^Debug raw targets:\s*(ON|OFF)$)");
    static const QRegularExpression reMultiSetting(R"(^Multi-target mode:\s*(ON|OFF)$)");
    static const QRegularExpression reEmaSetting(R"(^EMA smoothing:\s*(ON|OFF)$)");
    static const QRegularExpression reModeEnabled(R"(^(Multi|Single)-target mode enabled$)");
    if (auto m = reDebugSetting.match(trimmed); m.hasMatch()) {
        emit deviceSettingChanged(QStringLiteral("DEBUG"), m.captured(1) == QLatin1String("ON"));
        return;
    }
    if (auto m = reMultiSetting.match(trimmed); m.hasMatch()) {
        emit deviceSettingChanged(QStringLiteral("MULTI"), m.captured(1) == QLatin1String("ON"));
        return;
    }
    if (auto m = reEmaSetting.match(trimmed); m.hasMatch()) {
        emit deviceSettingChanged(QStringLiteral("EMA"), m.captured(1) == QLatin1String("ON"));
        return;
    }
    if (auto m = reModeEnabled.match(trimmed); m.hasMatch()) {
        emit deviceSettingChanged(QStringLiteral("MULTI"), m.captured(1) == QLatin1String("Multi"));
        return;
    }

    // ---- HomeKit build extras.
    static const QRegularExpression rePresence(R"(^Presence:\s*(DETECTED|NOT DETECTED)$)");
    static const QRegularExpression reMotion(R"(^Motion:\s*(DETECTED|NOT DETECTED)$)");
    if (auto m = rePresence.match(trimmed); m.hasMatch()) {
        emit presenceChanged(m.captured(1) == QLatin1String("DETECTED"));
        return;
    }
    if (auto m = reMotion.match(trimmed); m.hasMatch()) {
        emit motionChanged(m.captured(1) == QLatin1String("DETECTED"));
        return;
    }

    // Anything else (boot banners, per-point debug traces, closest-zone
    // fallback notes, ...) is intentionally left unclassified here - it is
    // still visible in full via rawLine().
}

void RadarProtocol::handleTargetBlockLine(const QString &trimmed, bool &handled) {
    handled = false;
    static const QRegularExpression reTargetsHeader(R"(^Targets:\s*(\d+)$)");
    static const QRegularExpression reIndex(R"(^\[(\d+)\]$)");
    static const QRegularExpression reX(R"(^X \(mm\):\s*(-?\d+)$)");
    static const QRegularExpression reY(R"(^Y \(mm\):\s*(-?\d+)$)");
    static const QRegularExpression reDist(R"(^Distance \(mm\):\s*(-?[\d.]+)$)");
    static const QRegularExpression reAngle(R"(^Angle \(degrees\):\s*(-?[\d.]+)$)");
    static const QRegularExpression reSpeed(R"(^Speed \(cm/s\):\s*(-?[\d.]+)$)");
    static const QRegularExpression reDashes(R"(^-{5,}$)");

    if (auto m = reTargetsHeader.match(trimmed); m.hasMatch()) {
        m_expectedTargets = m.captured(1).toInt();
        m_buildingTargets.clear();
        m_currentTarget.reset();
        m_inTargetBlock = true;
        handled = true;
        return;
    }

    if (!m_inTargetBlock) return;

    if (auto m = reIndex.match(trimmed); m.hasMatch()) {
        if (m_currentTarget) m_buildingTargets.push_back(*m_currentTarget);
        m_currentTarget = Target{};
        m_currentTarget->index = m.captured(1).toInt();
        handled = true;
        return;
    }
    if (m_currentTarget) {
        if (auto m = reX.match(trimmed); m.hasMatch()) { m_currentTarget->x = m.captured(1).toInt(); handled = true; return; }
        if (auto m = reY.match(trimmed); m.hasMatch()) { m_currentTarget->y = m.captured(1).toInt(); handled = true; return; }
        if (auto m = reDist.match(trimmed); m.hasMatch()) { m_currentTarget->distance = m.captured(1).toDouble(); handled = true; return; }
        if (auto m = reAngle.match(trimmed); m.hasMatch()) { m_currentTarget->angle = m.captured(1).toDouble(); handled = true; return; }
        if (auto m = reSpeed.match(trimmed); m.hasMatch()) { m_currentTarget->speed = m.captured(1).toDouble(); handled = true; return; }
    }
    if (reDashes.match(trimmed).hasMatch()) {
        if (m_currentTarget) {
            m_buildingTargets.push_back(*m_currentTarget);
            m_currentTarget.reset();
        }
        m_inTargetBlock = false;
        handled = true;
        emit targetsUpdated(m_buildingTargets);
        return;
    }
}
