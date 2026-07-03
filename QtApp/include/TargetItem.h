#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPointF>
#include <QString>
#include <QVector>

// A single live radar track: current position (as a real-world-scaled dot + footprint circle)
// plus a fading trail of recent EMA-smoothed positions. One item per track id; RadarScene owns
// the id -> TargetItem map and handles creation/removal as tracks appear/disappear.
//
// Positions passed in via update() are in scene coordinates (mm, Y already negated — see
// ZoneGridItem's coordinate-convention note), so the trail's geometry is spatially accurate
// and naturally scales with the view's zoom. Labels are kept a constant pixel size regardless
// of zoom by counter-scaling the painter just for the text draw.
class TargetItem : public QGraphicsItem {
public:
    explicit TargetItem(quint8 id, QGraphicsItem *parent = nullptr);

    quint8 id() const { return m_id; }

    // Push a fresh sample (called once per telemetry frame this track was present in).
    void updateSample(const QPointF &scenePos, float speedCmS, const QString &zoneLabel, qint64 nowMs);

    // Track absent from the latest frame: fade towards removal instead of vanishing instantly,
    // which smooths over single dropped packets. Returns true once it has aged past the grace
    // window and should be deleted by the owner.
    bool markStaleAndCheckExpired(qint64 nowMs);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    struct Sample {
        QPointF pos;
        qint64 tMs;
    };

    void pruneHistory(qint64 nowMs);
    QColor colorForId(quint8 id) const;

    quint8 m_id;
    QColor m_color;
    QVector<Sample> m_history; // oldest first
    float m_speedCmS = 0.0f;
    QString m_zoneLabel;
    qint64 m_lastSeenMs = 0;
    bool m_stale = false;
    QRectF m_boundingRect;

    static constexpr qint64 kTrailWindowMs = 2500;
    static constexpr qint64 kStaleGraceMs = 600;
    static constexpr float kFootprintRadiusMm = 125.0f; // matches TARGET_RADIUS_MM on the ESP32
};
