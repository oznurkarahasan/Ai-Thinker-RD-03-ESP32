#include "TargetItem.h"

#include <QDateTime>
#include <QPainter>
#include <algorithm>

// ===== TargetHeadItem =====

TargetHeadItem::TargetHeadItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setZValue(1); // above the parent TargetItem's trail/footprint paint
}

void TargetHeadItem::setAppearance(const QColor &color, const QString &label, float alpha)
{
    prepareGeometryChange();
    m_color = color;
    m_label = label;
    m_alpha = alpha;
    update();
}

QRectF TargetHeadItem::boundingRect() const
{
    // Fixed device-pixel-space rect: dot + generously wide label to the right of it.
    // ItemIgnoresTransformations means these are real pixels regardless of view zoom, so this
    // never needs to depend on the current scale — that dependency was the root cause of the
    // old smearing bug.
    return QRectF(-8, -22, 190, 34);
}

void TargetHeadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor dotColor = m_color;
    dotColor.setAlphaF(m_alpha);
    painter->setPen(Qt::NoPen);
    painter->setBrush(dotColor);
    painter->drawEllipse(QPointF(0, 0), 6.0, 6.0);

    QColor textColor = Qt::white;
    textColor.setAlphaF(m_alpha);
    painter->setPen(textColor);
    QFont f = painter->font();
    f.setPointSizeF(9.0);
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(QPointF(10, -8), m_label);
}

// ===== TargetItem =====

TargetItem::TargetItem(quint8 id, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_id(id)
    , m_color(colorForId(id))
    , m_head(new TargetHeadItem(this))
{
    setZValue(10); // above the zone grid and range rings
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
    pruneHistory(nowMs);

    QRectF r(scenePos, QSizeF(0, 0));
    for (const Sample &s : m_history)
        r |= QRectF(s.pos, QSizeF(0, 0));
    m_boundingRect = r.adjusted(-kFootprintRadiusMm, -kFootprintRadiusMm, kFootprintRadiusMm, kFootprintRadiusMm);

    m_head->setPos(scenePos);
    refreshHead(nowMs);
    update();
}

void TargetItem::tick(qint64 nowMs)
{
    // No new telemetry sample this tick, but re-push the head's live-computed alpha and force a
    // repaint anyway — paint() itself reads the wall clock (see below), so this is what actually
    // makes the fade progress in real time instead of freezing at whatever it looked like the
    // last time updateSample() ran.
    refreshHead(nowMs);
    update();
}

float TargetItem::liveAlphaAt(qint64 nowMs) const
{
    const qint64 age = nowMs - m_lastSeenMs;
    if (age <= 0)
        return 1.0f;
    return std::clamp(1.0f - float(age) / float(kStaleGraceMs), 0.0f, 1.0f);
}

void TargetItem::refreshHead(qint64 nowMs)
{
    const float alpha = liveAlphaAt(nowMs);
    const QString label = QStringLiteral("#%1  %2  %3 cm/s")
                               .arg(m_id)
                               .arg(m_zoneLabel.isEmpty() ? QStringLiteral("-") : m_zoneLabel)
                               .arg(m_speedCmS, 0, 'f', 1);
    m_head->setAppearance(m_color, label, alpha);
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

    // Read the actual wall clock here rather than the last sample's own timestamp. Using the
    // sample timestamp meant that once updateSample() stopped being called (i.e. the exact
    // moment a track needs to start visibly disappearing), every alpha computed below was frozen
    // at whatever it was at the last real update — the item looked identical on every repaint
    // until the instant it got deleted, which read as "stuck"/"frozen" rather than fading out.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const float overallAlpha = liveAlphaAt(nowMs); // 1.0 fresh -> 0.0 at kStaleGraceMs, live
    if (overallAlpha <= 0.0f)
        return; // about to be swept/deleted by RadarScene; nothing left to draw

    // Fading trail: one segment per consecutive sample pair, alpha falls off with age. This is
    // spatially accurate (drawn in real scene/mm coordinates) so it naturally scales with zoom.
    if (m_history.size() > 1) {
        for (int i = 1; i < m_history.size(); ++i) {
            const Sample &a = m_history[i - 1];
            const Sample &b = m_history[i];
            const qreal age = qreal(nowMs - b.tMs) / qreal(kTrailWindowMs);
            const qreal alpha = std::clamp(1.0 - age, 0.0, 1.0) * 0.8 * overallAlpha;
            if (alpha <= 0.02)
                continue;
            QColor c = m_color;
            c.setAlphaF(static_cast<float>(alpha));
            painter->setPen(QPen(c, 30.0, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(a.pos, b.pos);
        }
    }

    // Real-world-scaled footprint circle (matches the firmware's zone-occupancy radius).
    // The crisp head dot + text label are drawn by the m_head child item instead of here —
    // see TargetHeadItem's class comment for why.
    const QPointF dot = m_history.back().pos;
    QColor footprint = m_color;
    footprint.setAlphaF(0.22f * overallAlpha);
    painter->setPen(Qt::NoPen);
    painter->setBrush(footprint);
    painter->drawEllipse(dot, kFootprintRadiusMm, kFootprintRadiusMm);
}
