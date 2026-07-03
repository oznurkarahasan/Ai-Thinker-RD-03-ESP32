#include "RadarScene.h"

#include <QDateTime>
#include <QSet>

#include "TargetItem.h"
#include "ZoneGridItem.h"

RadarScene::RadarScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_grid(new ZoneGridItem())
{
    setBackgroundBrush(QColor(18, 20, 24));
    addItem(m_grid);
    // Default scene rect until a "cfg" frame arrives (radar at bottom-center, 4x4m ahead).
    setSceneRect(-2000, -4000, 4000, 4000);
}

void RadarScene::onConfigReceived(const RadarConfig &config)
{
    m_config = config;
    m_grid->setConfig(config);
    setSceneRect(-config.gridWidth / 2.0 * config.tileSizeMm,
                 -static_cast<double>(config.gridHeight) * config.tileSizeMm,
                 config.gridWidth * config.tileSizeMm,
                 config.gridHeight * config.tileSizeMm);
}

void RadarScene::onTargetsUpdated(const QVector<TargetState> &targets, quint32 zoneOccupiedMask, quint32 seq)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastMask = zoneOccupiedMask;
    m_grid->setOccupancyMask(zoneOccupiedMask);

    QSet<quint8> present;
    present.reserve(targets.size());

    for (const TargetState &ts : targets) {
        present.insert(ts.id);
        auto it = m_targetItems.find(ts.id);
        TargetItem *item;
        if (it == m_targetItems.end()) {
            item = new TargetItem(ts.id);
            addItem(item);
            m_targetItems.insert(ts.id, item);
        } else {
            item = it.value();
        }

        const QString zoneLabel = (ts.zone < m_config.zoneNames.size()) ? m_config.zoneNames[ts.zone]
                                                                          : QString();
        item->updateSample(toScenePos(ts.x, ts.y), ts.speed, zoneLabel, now);
    }

    // Anything not in this frame either fades out (recent dropout) or gets removed (really gone).
    for (auto it = m_targetItems.begin(); it != m_targetItems.end();) {
        if (present.contains(it.key())) {
            ++it;
            continue;
        }
        if (it.value()->markStaleAndCheckExpired(now)) {
            removeItem(it.value());
            delete it.value();
            it = m_targetItems.erase(it);
        } else {
            ++it;
        }
    }

    emit frameRendered(m_targetItems.size(), zoneOccupiedMask, seq);
}
