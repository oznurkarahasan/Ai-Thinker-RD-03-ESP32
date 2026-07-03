#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStringList>

#include "Types.h"

// Single source of truth for everything known about the radar right now.
// RadarProtocol feeds it via slots; RadarView / ControlPanel / StatusPanel
// read it and react to its change signals. Keeping this separate from the
// parser means the UI never touches raw serial text.
class RadarModel : public QObject {
    Q_OBJECT
public:
    explicit RadarModel(QObject *parent = nullptr);

    QVector<Target> targets() const { return m_targets; }
    QVector<ZoneDef> zoneDefs() const;
    bool isZoneOccupied(const QString &zoneName) const;
    QVector<Track> tracks() const;
    int activeTrackCount() const { return m_activeTrackCount; }

    TriState debugRawState() const { return m_debugRaw; }
    TriState multiTargetState() const { return m_multiTarget; }
    TriState emaState() const { return m_ema; }
    TriState presenceState() const { return m_presence; }
    TriState motionState() const { return m_motion; }

signals:
    void targetsChanged();
    void zonesChanged();
    void tracksChanged();
    void settingChanged(const QString &key, TriState state);
    void presenceMotionChanged();
    void helpReceived(const QStringList &lines);

public slots:
    void onTargetsUpdated(const QVector<Target> &targets);
    void onZoneStatusResetRequested();
    void onZoneOccupancyChanged(const QString &zoneName, bool occupied);
    void onActiveTrackCountUpdated(int count);
    void onZoneDefinitionUpdated(const ZoneDef &zone);
    void onTrackCreated(int id, double x, double y);
    void onTrackLost(int id);
    void onTrackPositionUpdated(int id, double x, double y, const QString &zone);
    void onDeviceSettingChanged(const QString &key, bool on);
    void onPresenceChanged(bool detected);
    void onMotionChanged(bool detected);
    void onHelpTextReceived(const QStringList &lines);

    // Called when the serial connection drops: clears everything that only
    // makes sense while live data is flowing, but keeps known zone geometry.
    void onDisconnected();

private:
    void seedDefaultZones();

    QVector<Target> m_targets;
    QMap<QString, ZoneDef> m_zoneDefs;      // keyed by zone name, e.g. "A1"
    QMap<QString, bool> m_zoneOccupied;
    QMap<int, Track> m_tracks;              // keyed by track id
    int m_activeTrackCount = -1;

    TriState m_debugRaw = TriState::Unknown;
    TriState m_multiTarget = TriState::Unknown;
    TriState m_ema = TriState::Unknown;
    TriState m_presence = TriState::Unknown;
    TriState m_motion = TriState::Unknown;
};
