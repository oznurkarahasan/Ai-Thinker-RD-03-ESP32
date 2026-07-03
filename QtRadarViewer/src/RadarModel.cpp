#include "RadarModel.h"

#include <cmath>

namespace {
// Mirrors the firmware constants in ESP32_RD03D.ino / the web GUI's
// sketch.js: a 4x4 grid of 1000mm tiles centred on the radar, with the two
// far corners (A4, D4) excluded. Used as a placeholder until the device
// reports its real geometry (boot dump or the ZONES command).
constexpr double kTileSizeMm = 1000.0;
constexpr int kGridWidth = 4;
constexpr int kGridHeight = 4;
}

RadarModel::RadarModel(QObject *parent) : QObject(parent) {
    seedDefaultZones();
}

void RadarModel::seedDefaultZones() {
    for (int row = 0; row < kGridHeight; ++row) {
        for (int col = 0; col < kGridWidth; ++col) {
            const bool excluded = (col == 0 && row == 3) || (col == 3 && row == 3);
            if (excluded) continue;

            ZoneDef z;
            z.xMin = (col - 2) * kTileSizeMm;
            z.xMax = z.xMin + kTileSizeMm;
            z.yMin = row * kTileSizeMm;
            z.yMax = z.yMin + kTileSizeMm;
            z.name = QString(QChar('A' + col)) + QString::number(row + 1);

            m_zoneDefs.insert(z.name, z);
            m_zoneOccupied.insert(z.name, false);
        }
    }
}

QVector<ZoneDef> RadarModel::zoneDefs() const {
    return QVector<ZoneDef>(m_zoneDefs.cbegin(), m_zoneDefs.cend());
}

bool RadarModel::isZoneOccupied(const QString &zoneName) const {
    return m_zoneOccupied.value(zoneName, false);
}

QVector<Track> RadarModel::tracks() const {
    return QVector<Track>(m_tracks.cbegin(), m_tracks.cend());
}

void RadarModel::onTargetsUpdated(const QVector<Target> &targets) {
    m_targets = targets;
    emit targetsChanged();
}

void RadarModel::onZoneStatusResetRequested() {
    for (auto it = m_zoneOccupied.begin(); it != m_zoneOccupied.end(); ++it) {
        it.value() = false;
    }
    emit zonesChanged();
}

void RadarModel::onZoneOccupancyChanged(const QString &zoneName, bool occupied) {
    m_zoneOccupied[zoneName] = occupied;
    emit zonesChanged();
}

void RadarModel::onActiveTrackCountUpdated(int count) {
    m_activeTrackCount = count;
    emit tracksChanged();
}

void RadarModel::onZoneDefinitionUpdated(const ZoneDef &zone) {
    m_zoneDefs.insert(zone.name, zone);
    if (!m_zoneOccupied.contains(zone.name)) {
        m_zoneOccupied.insert(zone.name, false);
    }
    emit zonesChanged();
}

void RadarModel::onTrackCreated(int id, double x, double y) {
    Track t;
    t.id = id;
    t.x = x;
    t.y = y;
    t.hasPosition = true;
    m_tracks.insert(id, t);
    emit tracksChanged();
}

void RadarModel::onTrackLost(int id) {
    m_tracks.remove(id);
    emit tracksChanged();
}

void RadarModel::onTrackPositionUpdated(int id, double x, double y, const QString &zone) {
    Track &t = m_tracks[id]; // inserts a fresh Track if unknown (e.g. app connected mid-session)
    t.id = id;
    if (!std::isnan(x) && !std::isnan(y)) {
        t.x = x;
        t.y = y;
        t.hasPosition = true;
    }
    t.zone = zone;
    emit tracksChanged();
}

void RadarModel::onDeviceSettingChanged(const QString &key, bool on) {
    const TriState state = on ? TriState::On : TriState::Off;
    if (key == QLatin1String("DEBUG")) m_debugRaw = state;
    else if (key == QLatin1String("MULTI")) m_multiTarget = state;
    else if (key == QLatin1String("EMA")) m_ema = state;
    emit settingChanged(key, state);
}

void RadarModel::onPresenceChanged(bool detected) {
    m_presence = detected ? TriState::On : TriState::Off;
    emit presenceMotionChanged();
}

void RadarModel::onMotionChanged(bool detected) {
    m_motion = detected ? TriState::On : TriState::Off;
    emit presenceMotionChanged();
}

void RadarModel::onHelpTextReceived(const QStringList &lines) {
    emit helpReceived(lines);
}

void RadarModel::onDisconnected() {
    m_targets.clear();
    m_tracks.clear();
    m_activeTrackCount = -1;
    for (auto it = m_zoneOccupied.begin(); it != m_zoneOccupied.end(); ++it) {
        it.value() = false;
    }
    m_debugRaw = TriState::Unknown;
    m_multiTarget = TriState::Unknown;
    m_ema = TriState::Unknown;
    m_presence = TriState::Unknown;
    m_motion = TriState::Unknown;

    emit targetsChanged();
    emit tracksChanged();
    emit zonesChanged();
    emit settingChanged(QStringLiteral("DEBUG"), TriState::Unknown);
    emit settingChanged(QStringLiteral("MULTI"), TriState::Unknown);
    emit settingChanged(QStringLiteral("EMA"), TriState::Unknown);
    emit presenceMotionChanged();
}
