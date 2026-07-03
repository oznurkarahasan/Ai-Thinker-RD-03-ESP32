#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

// One track as reported by a single "trk" telemetry frame from the ESP32.
// Positions are in millimetres, radar-native coordinates: X+ right, Y+ forward (away from radar).
struct TargetState {
    quint8 id = 0;
    float x = 0.0f;      // mm
    float y = 0.0f;      // mm
    float speed = 0.0f;  // cm/s (signed: sign indicates approach/recede per RD-03D convention)
    quint8 zone = 0xFF;  // index into RadarConfig::zoneNames; 0xFF = no zone
};

// Grid/zone layout, sent once by the ESP32 at boot (and on-demand via the CFG serial command).
struct RadarConfig {
    float tileSizeMm = 1000.0f;
    quint8 gridWidth = 4;
    quint8 gridHeight = 4;
    QVector<QString> zoneNames; // order matches the bit order of the zocc occupancy mask
};

Q_DECLARE_METATYPE(TargetState)
Q_DECLARE_METATYPE(RadarConfig)
