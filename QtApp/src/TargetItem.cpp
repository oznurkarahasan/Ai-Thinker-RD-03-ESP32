#include "TargetItem.h"

#include <QPainter>
#include <algorithm>

TargetItem::TargetItem(quint8 id, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_id(id)
    , m_color(colorForId(id))
{
    setZValue(10); // above the zone grid
}

QColor TargetItem::colorForId(quint8 id) const
{
    // Evenly spaced hues so a handful of concurrent tracks stay visually distinct even as
    // ids increase monotonically (the firmware never reuses an id while active).
    static const QColor palette[] = {
        QColor(0, 220, 255),
        QColor(255, 190, 0),
        QColor(120, 255, 120),
        QColor(255, 100, 180),
        QColor(180, 140, 255),
    };
    return palette[id % (sizeof(palette) / sizeof(palette[0]))];
}

void TargetItem::updateSample(const QPointF &scenePos, float speedCmS, const QString &zoneLabel, qint64 nowMs)
{
    prepareGeometryChange();
    m_history.append({scenePos, nowMs});
    m_speedCmS = speedCmS;
    m_zoneLabel = zoneLabel;
    m_lastSeenMs = nowMs;
    m_stale = false;
    pruneHistory(nowMs);

    QRectF r(scenePos, QSizeF(0, 0));
    for (const Sample &s : m_history)
        r |= QRectF(s.pos, QSizeF(0, 0));
    m_boundingRect = r.adjusted(-kFootprintRadiusMm - 40, -kFootprintRadiusMm - 40,
                                kFootprintRadiusMm + 40, kFootprintRadiusMm + 40);
    update();
}

bool TargetItem::markStaleAndCheckExpired(qint64 nowMs)
{
    m_stale = true;
    update();
    return (nowMs - m_lastSeenMs) > kStaleGraceMs;
}

void TargetItem::pruneHistory(qint64 nowMs)
{
    while (!m_history.isEmpty() && (nowMs - m_history.front().tMs) > kTrailWindowMs)
        m_history.removeFirst();
}

QRectF TargetItem::boundingRect() const
{
    return m_boundingRect;
}

void TargetItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_history.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    const qint64 nowMs = m_history.back().tMs;
    const float staleFade = m_stale ? 0.35f : 1.0f; // dim while aging out after a dropped frame

    // Fading trail: one segment per consecutive sample pair, alpha falls off with age.
    if (m_history.size() > 1) {
        for (int i = 1; i < m_history.size(); ++i) {
            const Sample &a = m_history[i - 1];
            const Sample &b = m_history[i];
            const qreal age = qreal(nowMs - b.tMs) / qreal(kTrailWindowMs);
            const qreal alpha = std::clamp(1.0 - age, 0.0, 1.0) * 0.8 * staleFade;
            if (alpha <= 0.02)
                continue;
            QColor c = m_color;
            c.setAlphaF(static_cast<float>(alpha));
            painter->setPen(QPen(c, 30.0, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(a.pos, b.pos);
        }
    }

    const QPointF dot = m_history.back().pos;

    // Real-world-scaled footprint circle (matches the firmware's zone-occupancy radius).
    QColor footprint = m_color;
    footprint.setAlphaF(0.22f * staleFade);
    painter->setPen(Qt::NoPen);
    painter->setBrush(footprint);
    painter->drawEllipse(dot, kFootprintRadiusMm, kFootprintRadiusMm);

    // Crisp center dot + label, counter-scaled so they stay a constant pixel size at any zoom.
    qreal m11 = painter->worldTransform().m11();
    if (m11 <= 0.0001)
        m11 = 1.0;
    const qreal invScale = 1.0 / m11;

    painter->save();
    painter->translate(dot);
    painter->scale(invScale, invScale);

    QColor dotColor = m_color;
    dotColor.setAlphaF(staleFade);
    painter->setPen(Qt::NoPen);
    painter->setBrush(dotColor);
    painter->drawEllipse(QPointF(0, 0), 6.0, 6.0);

    QColor textColor = Qt::white;
    textColor.setAlphaF(staleFade);
    painter->setPen(textColor);
    QFont f = painter->font();
    f.setPointSizeF(9.0);
    f.setBold(true);
    painter->setFont(f);
    const QString label = QStringLiteral("#%1  %2  %3 cm/s")
                               .arg(m_id)
                               .arg(m_zoneLabel.isEmpty() ? QStringLiteral("-") : m_zoneLabel)
                               .arg(m_speedCmS, 0, 'f', 1);
    painter->drawText(QPointF(10, -8), label);

    painter->restore();
}
