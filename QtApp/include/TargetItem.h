#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPointF>
#include <QString>
#include <QVector>

// Small head marker (crisp dot + "#id zone speed" label) for a track's current position.
// Uses ItemIgnoresTransformations so it always renders at a constant pixel size regardless of
// view zoom, and — critically — so Qt's own dirty-region tracking for ignores-transformations
// items stays correct: everything drawn in paint() is expressed in real device-pixel units
// local to this item, matching what boundingRect() declares. (The previous implementation
// hand-rolled constant-size text by inverse-scaling the painter inside a regularly-transformed
// item; that broke Qt's paint-region invalidation whenever the view zoom changed, since the
// item's declared boundingRect() no longer matched where pixels actually landed — the cause of
// the text "smearing" left behind on the trail.)
class TargetHeadItem : public QGraphicsItem {
public:
    explicit TargetHeadItem(QGraphicsItem *parent);

    void setAppearance(const QColor &color, const QString &label, float alpha);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QColor m_color;
    QString m_label;
    float m_alpha = 1.0f;
};

// A single live radar track: current position (head dot + footprint circle) plus a fading
// trail of recent EMA-smoothed positions. One item per track id; RadarScene owns the
// id -> TargetItem map and handles creation/removal as tracks appear/disappear.
//
// Positions passed in via updateSample() are in scene coordinates (mm, Y already negated —
// see ZoneGridItem's coordinate-convention note), so the trail and footprint circle are
// spatially accurate and naturally scale with the view's zoom. The head dot/label are drawn
// by a child TargetHeadItem (see above) so they stay a constant pixel size and never smear.
class TargetItem : public QGraphicsItem {
public:
    explicit TargetItem(quint8 id, QGraphicsItem *parent = nullptr);

    quint8 id() const { return m_id; }

    // Push a fresh sample (called once per telemetry frame this track was present in).
    void updateSample(const QPointF &scenePos, float speedCmS, const QString &zoneLabel, qint64 nowMs);

    // True once more than kStaleGraceMs has elapsed (real wall-clock time, not telemetry frames)
    // since the last updateSample() call. The owner (RadarScene) is expected to poll this from
    // an independent QTimer and delete the item the moment it returns true — cleanup no longer
    // depends on telemetry continuing to arrive at all.
    bool isExpired(qint64 nowMs) const { return (nowMs - m_lastSeenMs) > kStaleGraceMs; }

    // Periodic refresh from RadarScene's sweep timer, called for every *non*-expired item on
    // every tick regardless of whether a new sample arrived. This is what keeps the fade-out
    // animating in real time between telemetry frames (paint() itself also reads the live clock,
    // but Qt only repaints when something calls update() — without this, an idle item would
    // just sit there showing whatever it looked like at its last real sample forever until the
    // moment it's deleted, i.e. exactly the "frozen ghost" bug this exists to fix).
    void tick(qint64 nowMs);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    struct Sample {
        QPointF pos;
        qint64 tMs;
    };

    void pruneHistory(qint64 nowMs);
    void refreshHead(qint64 nowMs);
    float liveAlphaAt(qint64 nowMs) const;
    QColor colorForId(quint8 id) const;

    quint8 m_id;
    QColor m_color;
    QVector<Sample> m_history; // oldest first
    float m_speedCmS = 0.0f;
    QString m_zoneLabel;
    qint64 m_lastSeenMs = 0;
    QRectF m_boundingRect;
    TargetHeadItem *m_head;

    static constexpr qint64 kTrailWindowMs = 2500; // unchanged from before — this request only tightened expiry/fade timing
    static constexpr qint64 kStaleGraceMs = 300; // real elapsed time since last sample, not frames
    static constexpr float kFootprintRadiusMm = 125.0f; // matches TARGET_RADIUS_MM on the ESP32
};
