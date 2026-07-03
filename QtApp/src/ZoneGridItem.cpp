#include "ZoneGridItem.h"

#include <QPainter>
#include <QPainterPath>

ZoneGridItem::ZoneGridItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
}

void ZoneGridItem::setConfig(const RadarConfig &config)
{
    prepareGeometryChange();
    m_config = config;
    rebuildZones();
    update();
}

void ZoneGridItem::setOccupancyMask(quint32 mask)
{
    if (mask == m_occupiedMask)
        return;
    m_occupiedMask = mask;
    update(); // occupancy change never affects geometry -> repaint only, no layout cost
}

void ZoneGridItem::rebuildZones()
{
    m_zones.clear();
    if (m_config.gridWidth == 0 || m_config.gridHeight == 0) {
        m_boundingRect = QRectF();
        return;
    }

    const float tile = m_config.tileSizeMm;
    for (quint8 row = 0; row < m_config.gridHeight; ++row) {
        for (quint8 col = 0; col < m_config.gridWidth; ++col) {
            Zone z;
            z.name = QString(QChar('A' + col)) + QString::number(row + 1);
            z.bitIndex = m_config.zoneNames.indexOf(z.name);

            const float xMin = (col - m_config.gridWidth / 2.0f) * tile;
            const float yMinMm = row * tile;
            const float yMaxMm = yMinMm + tile;
            // scene top-left = (xMin, -yMaxMm): the far edge (larger mm-Y) maps to the
            // smaller (more negative) scene-Y, i.e. visually higher on screen.
            z.rect = QRectF(xMin, -yMaxMm, tile, tile);

            m_zones.append(z);
        }
    }

    m_boundingRect = QRectF(-m_config.gridWidth / 2.0f * tile,
                             -static_cast<float>(m_config.gridHeight) * tile,
                             m_config.gridWidth * tile,
                             m_config.gridHeight * tile);
}

QRectF ZoneGridItem::boundingRect() const
{
    return m_boundingRect;
}

void ZoneGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    static const QColor kExcludedFill(70, 70, 78, 90);
    static const QColor kExcludedBorder(90, 90, 98);
    static const QColor kEmptyFill(40, 44, 52, 60);
    static const QColor kEmptyBorder(90, 100, 115);
    static const QColor kOccupiedFill(255, 70, 70, 90);
    static const QColor kOccupiedBorder(255, 120, 120);

    QFont labelFont = painter->font();
    labelFont.setPointSizeF(labelFont.pointSizeF() + 1.0);
    painter->setFont(labelFont);

    for (const Zone &z : m_zones) {
        const bool excluded = (z.bitIndex < 0);
        const bool occupied = !excluded && (m_occupiedMask & (1u << z.bitIndex));

        if (excluded) {
            painter->setPen(QPen(kExcludedBorder, 1, Qt::DashLine));
            painter->setBrush(kExcludedFill);
        } else if (occupied) {
            painter->setPen(QPen(kOccupiedBorder, 2));
            painter->setBrush(kOccupiedFill);
        } else {
            painter->setPen(QPen(kEmptyBorder, 1));
            painter->setBrush(kEmptyFill);
        }
        painter->drawRect(z.rect);

        painter->setPen(excluded ? QColor(140, 140, 148) : (occupied ? Qt::white : QColor(190, 195, 205)));
        painter->drawText(z.rect, Qt::AlignCenter, z.name);
    }

    // Radar origin marker: small triangle at scene (0,0) pointing toward +Y-mm (i.e. -Y-scene,
    // up on screen) to show sensor position/facing at a glance.
    QPainterPath marker;
    marker.moveTo(0, 10);
    marker.lineTo(-14, 26);
    marker.lineTo(14, 26);
    marker.closeSubpath();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 200, 255));
    painter->drawPath(marker);
}
