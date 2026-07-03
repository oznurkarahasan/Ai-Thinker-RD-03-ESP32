#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QVector>

#include "RadarTypes.h"

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

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
//
// Target removal is driven entirely by a periodic sweep timer (see m_sweepTimer), not by
// telemetry arrival: each TargetItem tracks its own last-updated wall-clock time, and the timer
// deletes anything that's gone quiet for too long. This is deliberately decoupled from whether
// or how often onTargetsUpdated() gets called, so cleanup keeps working even if the serial
// stream stalls, drops frames, or an id simply never appears in a "t" array again.
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
    //
    // The firmware has no concept of this UI-side clear, so it keeps sending whatever track ids
    // were already in flight (queued in the OS serial buffer, or mid-transit) at the moment the
    // button was clicked — without suppression those frames land a moment later and instantly
    // recreate the very ids we just destroyed, making Clear look like it did nothing. To avoid
    // that, every id present at clear time is remembered and dropped on arrival for a short
    // grace window; after the window elapses those ids are treated as genuinely new again.
    void clearScene();

signals:
    void frameRendered(int activeTargets, quint32 zoneOccupiedMask, quint32 seq);

private:
    static QPointF toScenePos(float xMm, float yMm) { return QPointF(xMm, -yMm); }

    // Runs on m_sweepTimer, independent of telemetry arrival: deletes any TargetItem that's
    // exceeded TargetItem::isExpired(), and calls tick() on everything still alive so the
    // fade-out actually animates in real time even between telemetry frames.
    void sweepStaleTargets();

    ZoneGridItem *m_grid;
    RangeRingsItem *m_rings;
    QHash<quint8, TargetItem *> m_targetItems;
    RadarConfig m_config;
    quint32 m_lastMask = 0;
    quint32 m_lastSeq = 0;

    QTimer *m_sweepTimer;
    static constexpr int kSweepIntervalMs = 50; // frequent enough for a smooth fade + fast expiry detection

    QSet<quint8> m_suppressedIds; // ids in flight at the moment Clear was clicked
    qint64 m_suppressUntilMs = 0;
    static constexpr qint64 kClearSuppressMs = 400; // a handful of frames at ~20Hz
};
