#include "RadarScene.h"

#include <QDateTime>
#include <QSet>
#include <utility>

#include "RangeRingsItem.h"
#include "TargetItem.h"
#include "ZoneGridItem.h"

RadarScene::RadarScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_grid(new ZoneGridItem())
    , m_rings(new RangeRingsItem())
{
    setBackgroundBrush(QColor(18, 20, 24));

    m_rings->setMaxRangeMm(8000.0f);
    m_rings->setRingStepMm(1000.0f);
    addItem(m_rings); // zValue -10: drawn first, underneath the zone grid

    addItem(m_grid); // zValue 0: overlaid on top of the range rings

    // Fixed for the lifetime of the scene — see the class comment on lockedViewRect().
    setSceneRect(lockedViewRect());
}

void RadarScene::onConfigReceived(const RadarConfig &config)
{
    m_config = config;
    m_grid->setConfig(config);
    // Deliberately NOT touching setSceneRect() here: the view's logical area is fixed at
    // construction and must stay independent of whatever grid dimensions the firmware reports.
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

void RadarScene::clearScene()
{
    for (TargetItem *item : std::as_const(m_targetItems)) {
        removeItem(item);
        delete item;
    }
    m_targetItems.clear();

    m_lastMask = 0;
    m_grid->setOccupancyMask(0);

    emit frameRendered(0, 0, 0);
}
