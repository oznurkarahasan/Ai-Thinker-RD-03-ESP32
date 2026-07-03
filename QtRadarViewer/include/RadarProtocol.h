#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

#include "Types.h"

// Stateful line-oriented parser for everything the ESP32_RD03D /
// ESP32_RD03D_HOMEKIT firmware prints on its USB serial port.
//
// The firmware never emits JSON; it prints plain human-readable lines
// (see ESP32_RD03D.ino). This class turns that text stream into typed
// Qt signals so the rest of the app never has to know about the wire
// format. Every line is also re-emitted verbatim via rawLine() so no
// data is ever silently dropped, even lines this parser doesn't
// recognize (e.g. per-point debug traces).
class RadarProtocol : public QObject {
    Q_OBJECT
public:
    explicit RadarProtocol(QObject *parent = nullptr);

public slots:
    void feedLine(const QString &line);
    void reset();

signals:
    void rawLine(const QString &line);

    // One full "Targets: N ... -----" block finished parsing.
    void targetsUpdated(const QVector<Target> &targets);

    // Zone occupancy. resetAll() semantics: emitted right before a fresh
    // "=== ZONE STATUS ===" dump so the model can clear stale state before
    // re-applying the zones that are still active.
    void zoneStatusResetRequested();
    void zoneOccupancyChanged(const QString &zoneName, bool occupied);
    void activeTrackCountUpdated(int count);

    // Zone geometry, as reported by the device itself (boot dump or the
    // ZONES command), in millimetres.
    void zoneDefinitionUpdated(const ZoneDef &zone);

    // Track lifecycle / live position.
    void trackCreated(int id, double x, double y);
    void trackLost(int id);
    void trackPositionUpdated(int id, double x, double y, const QString &zone);

    // DEBUG / MULTI / EMA toggles, echoed back by the firmware.
    void deviceSettingChanged(const QString &key, bool on);

    // HomeKit build only.
    void presenceChanged(bool detected);
    void motionChanged(bool detected);

    // Response to the HELP command.
    void helpTextReceived(const QStringList &lines);

private:
    void handleTargetBlockLine(const QString &trimmed, bool &handled);

    bool m_inTargetBlock = false;
    int m_expectedTargets = 0;
    QVector<Target> m_buildingTargets;
    std::optional<Target> m_currentTarget;

    bool m_collectingHelp = false;
    QStringList m_helpLines;
};
