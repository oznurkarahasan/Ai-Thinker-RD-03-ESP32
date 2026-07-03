#pragma once

#include <QGraphicsItem>
#include <QRectF>
#include <QVector>

#include "RadarTypes.h"

// Draws the static NxM zone grid in scene coordinates and highlights zones marked occupied
// by the latest telemetry frame. Zone geometry is *derived* from the zone name itself
// (e.g. "B3" -> column B, row 3) rather than hardcoded, so any grid size/exclusion pattern
// the firmware reports (via RadarConfig) renders correctly without matching source changes
// on both sides. Any (column,row) cell whose name is absent from RadarConfig::zoneNames
// (e.g. the far corners A4/D4 on a 4x4 grid) is drawn grayed-out and non-interactive.
//
// Coordinate convention used throughout this app: scene X == radar X (mm), scene Y == -radar Y
// (mm). The radar sits at scene (0,0); since Qt's Y axis points down the screen, negating
// radar-Y means targets moving away from the radar move toward the top of the view.
class ZoneGridItem : public QGraphicsItem {
public:
    explicit ZoneGridItem(QGraphicsItem *parent = nullptr);

    void setConfig(const RadarConfig &config);
    void setOccupancyMask(quint32 mask);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    struct Zone {
        QString name;
        QRectF rect;      // scene coordinates
        int bitIndex = -1; // index into RadarConfig::zoneNames / occupancy mask; -1 = excluded
    };

    void rebuildZones();

    RadarConfig m_config;
    QVector<Zone> m_zones;
    quint32 m_occupiedMask = 0;
    QRectF m_boundingRect;
};
