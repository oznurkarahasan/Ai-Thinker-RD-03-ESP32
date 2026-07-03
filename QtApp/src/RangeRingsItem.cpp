#include "RangeRingsItem.h"

#include <QPainter>

RangeRingsItem::RangeRingsItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setZValue(-10); // beneath the zone grid (0) and targets (10)
}

void RangeRingsItem::setMaxRangeMm(float maxRangeMm)
{
    prepareGeometryChange();
    m_maxRangeMm = maxRangeMm;
    update();
}

void RangeRingsItem::setRingStepMm(float stepMm)
{
    prepareGeometryChange();
    m_ringStepMm = stepMm;
    update();
}

QRectF RangeRingsItem::boundingRect() const
{
    return QRectF(-m_maxRangeMm, -m_maxRangeMm, 2 * m_maxRangeMm, 2 * m_maxRangeMm);
}

void RangeRingsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    // Bright "tactical radar" cyan, glowing via a wide/faint pass underneath a thin/bright one —
    // cheap two-draws-per-ring approximation of a glow without a QGraphicsEffect per item.
    static const QColor kRingColor(0x00, 0xE5, 0xFF);

    for (float r = m_ringStepMm; r <= m_maxRangeMm + 1.0f; r += m_ringStepMm) {
        const QRectF ringRect(-r, -r, 2 * r, 2 * r);
        // Qt's angle convention (0 deg = 3 o'clock, increasing counter-clockwise on screen)
        // means a 0..180 deg sweep traces the *top* semicircle, i.e. the forward half-plane.
        QColor glow = kRingColor;
        glow.setAlpha(35);
        painter->setPen(QPen(glow, 6.0, Qt::SolidLine, Qt::RoundCap));
        painter->drawArc(ringRect, 0, 180 * 16);

        QColor bright = kRingColor;
        bright.setAlpha(200);
        painter->setPen(QPen(bright, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter->drawArc(ringRect, 0, 180 * 16);

        // Distance label on a dark pill so it stays legible against the bright ring/grid below.
        const float meters = r / 1000.0f;
        const QString label = QString::number(meters, 'f', 1) + " m";
        const QRectF labelRect(-24, -r - 15, 48, 16);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(8, 12, 16, 210));
        painter->drawRoundedRect(labelRect, 4, 4);

        painter->setPen(kRingColor);
        painter->drawText(labelRect, Qt::AlignCenter, label);
    }
}
