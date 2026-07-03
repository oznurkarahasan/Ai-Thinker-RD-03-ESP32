#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QRectF>
#include <QVector>

#include "RadarTypes.h"

class ZoneGridItem;
class RangeRingsItem;
class TargetItem;

// Owns the static grid/range-ring items and the live per-track TargetItems. Consumes
// RadarComm's signals directly (connect these in MainWindow) and translates radar-native mm
// coordinates into scene coordinates.
//
// The scene rect is fixed at construction (see lockedViewRect()) and never changes afterward —
// deliberately decoupled from telemetry/track positions, config, or item bounding boxes. A
// stray "ghost" target far outside the sensor's normal range must never change the view's
// zoom/transform; it should simply render off-screen (or get clipped) instead. MainWindow's
// fitInView() call should always target lockedViewRect(), not sceneRect() dynamically, so this
// holds regardless of any QGraphicsScene auto-grow behavior.
class RadarScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit RadarScene(QObject *parent = nullptr);

    int activeTargetCount() const { return m_targetItems.size(); }
    quint32 lastOccupancyMask() const { return m_lastMask; }
    const RadarConfig &config() const { return m_config; }

    // Fixed logical viewing area: X in [-4000, +4000] mm, Y in [0, 8000] mm (radar-forward),
    // expressed in scene coordinates (Y negated). Covers the full RD-03D range (rings out to
    // 8000mm) regardless of the zone grid's own (smaller) footprint.
    static QRectF lockedViewRect() { return QRectF(-4000.0, -8000.0, 8000.0, 8000.0); }

public slots:
    void onConfigReceived(const RadarConfig &config);
    void onTargetsUpdated(const QVector<TargetState> &targets, quint32 zoneOccupiedMask, quint32 seq);

    // Fully resets visible state: destroys every TargetItem (trails included) and clears zone
    // occupancy highlighting. Does not touch the connection or the cached grid config.
    void clearScene();

signals:
    void frameRendered(int activeTargets, quint32 zoneOccupiedMask, quint32 seq);

private:
    static QPointF toScenePos(float xMm, float yMm) { return QPointF(xMm, -yMm); }

    ZoneGridItem *m_grid;
    RangeRingsItem *m_rings;
    QHash<quint8, TargetItem *> m_targetItems;
    RadarConfig m_config;
    quint32 m_lastMask = 0;
};
