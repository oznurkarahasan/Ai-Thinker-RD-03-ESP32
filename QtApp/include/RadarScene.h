#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QVector>

#include "RadarTypes.h"

class ZoneGridItem;
class TargetItem;

// Owns the static grid item and the live per-track TargetItems. Consumes RadarComm's signals
// directly (connect these in MainWindow) and translates radar-native mm coordinates into
// scene coordinates. Emits activeTargetCount()/lastOccupancyMask() accessors for the dashboard.
class RadarScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit RadarScene(QObject *parent = nullptr);

    int activeTargetCount() const { return m_targetItems.size(); }
    quint32 lastOccupancyMask() const { return m_lastMask; }
    const RadarConfig &config() const { return m_config; }

public slots:
    void onConfigReceived(const RadarConfig &config);
    void onTargetsUpdated(const QVector<TargetState> &targets, quint32 zoneOccupiedMask, quint32 seq);

signals:
    void frameRendered(int activeTargets, quint32 zoneOccupiedMask, quint32 seq);

private:
    static QPointF toScenePos(float xMm, float yMm) { return QPointF(xMm, -yMm); }

    ZoneGridItem *m_grid;
    QHash<quint8, TargetItem *> m_targetItems;
    RadarConfig m_config;
    quint32 m_lastMask = 0;
};
