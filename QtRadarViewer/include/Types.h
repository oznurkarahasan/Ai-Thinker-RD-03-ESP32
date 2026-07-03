#pragma once

#include <QString>
#include <QVector>

// One detection reported inside a "Targets: N" block from the firmware.
struct Target {
    int index = 0;
    int x = 0;      // mm
    int y = 0;      // mm
    double distance = 0.0; // mm
    double angle = 0.0;    // degrees
    double speed = 0.0;    // cm/s
};

// A tracked person/object as maintained by the firmware's tracker.
struct Track {
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    bool hasPosition = false;
    QString zone;      // empty or "NONE" when not in any zone
};

// Rectangular zone bounds in millimetres, as reported by the device (ZONES
// command / boot dump) or, until that arrives, the compile-time default that
// mirrors the firmware constants (TILE_SIZE=1000, 4x4 grid, A4/D4 excluded).
struct ZoneDef {
    QString name;
    double xMin = 0.0;
    double xMax = 0.0;
    double yMin = 0.0;
    double yMax = 0.0;
};

// Tri-state device setting: unknown until the firmware echoes it back.
enum class TriState { Unknown, Off, On };
