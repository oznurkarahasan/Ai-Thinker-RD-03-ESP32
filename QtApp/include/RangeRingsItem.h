#pragma once

#include <QGraphicsItem>

// Classic-radar concentric distance rings: semi-circles centered on the radar at scene (0,0),
// spanning the forward half-plane (toward -Y in scene coordinates, i.e. +Y-mm / away from the
// sensor). Drawn once and left static; it never depends on telemetry, so it's a plain
// QGraphicsItem with no update() calls after construction. Painted at a lower Z than
// ZoneGridItem so the rectangular zone grid renders on top of it, per the classic-radar look
// requested: rings underneath, grid overlaid.
class RangeRingsItem : public QGraphicsItem {
public:
    explicit RangeRingsItem(QGraphicsItem *parent = nullptr);

    void setMaxRangeMm(float maxRangeMm);
    void setRingStepMm(float stepMm);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    float m_maxRangeMm = 8000.0f;
    float m_ringStepMm = 1000.0f;
};
