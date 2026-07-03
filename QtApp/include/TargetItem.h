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
    void refreshHead();
    QColor colorForId(quint8 id) const;

    quint8 m_id;
    QColor m_color;
    QVector<Sample> m_history; // oldest first
    float m_speedCmS = 0.0f;
    QString m_zoneLabel;
    qint64 m_lastSeenMs = 0;
    bool m_stale = false;
    QRectF m_boundingRect;
    TargetHeadItem *m_head;

    static constexpr qint64 kTrailWindowMs = 2500;
    static constexpr qint64 kStaleGraceMs = 600;
    static constexpr float kFootprintRadiusMm = 125.0f; // matches TARGET_RADIUS_MM on the ESP32
};
