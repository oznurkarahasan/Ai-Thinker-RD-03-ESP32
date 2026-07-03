#include "RadarScene.h"

#include <QDateTime>
#include <QTimer>
#include <utility>

#include "RangeRingsItem.h"
#include "TargetItem.h"
#include "ZoneGridItem.h"

RadarScene::RadarScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_grid(new ZoneGridItem())
    , m_rings(new RangeRingsItem())
    , m_sweepTimer(new QTimer(this))
{
    setBackgroundBrush(QColor(18, 20, 24));

    m_rings->setMaxRangeMm(8000.0f);
    m_rings->setRingStepMm(1000.0f);
    addItem(m_rings); // zValue -10: drawn first, underneath the zone grid

    addItem(m_grid); // zValue 0: overlaid on top of the range rings

    // Fixed for the lifetime of the scene — see the class comment on lockedViewRect().
    setSceneRect(lockedViewRect());

    m_sweepTimer->setInterval(kSweepIntervalMs);
    connect(m_sweepTimer, &QTimer::timeout, this, &RadarScene::sweepStaleTargets);
    m_sweepTimer->start();
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
    m_lastSeq = seq;
    m_grid->setOccupancyMask(zoneOccupiedMask);

    if (!m_suppressedIds.isEmpty() && now >= m_suppressUntilMs)
        m_suppressedIds.clear(); // grace window elapsed; these ids are fair game again

    for (const TargetState &ts : targets) {
        if (m_suppressedIds.contains(ts.id))
            continue; // still-in-flight frame for an id that existed at Clear-time; drop it

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

    // Removal of ids missing from this frame is handled entirely by sweepStaleTargets() on
    // m_sweepTimer — see the class comment for why that's deliberately independent of telemetry
    // arrival rather than checked here against this frame's target list.

    emit frameRendered(m_targetItems.size(), zoneOccupiedMask, seq);
}

void RadarScene::sweepStaleTargets()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool anyRemoved = false;

    for (auto it = m_targetItems.begin(); it != m_targetItems.end();) {
        if (it.value()->isExpired(now)) {
            removeItem(it.value());
            delete it.value();
            it = m_targetItems.erase(it);
            anyRemoved = true;
        } else {
            it.value()->tick(now); // keeps the fade-out animating in real time, not just on new samples
            ++it;
        }
    }

    if (anyRemoved)
        emit frameRendered(m_targetItems.size(), m_lastMask, m_lastSeq);
}

void RadarScene::clearScene()
{
    // Remember every id currently on screen so frames already queued for them (in the OS serial
    // buffer or mid-transit over USB) get dropped instead of instantly respawning the very
    // tracks we're about to destroy — see the class comment on clearScene() for why.
    m_suppressedIds.clear();
    m_suppressedIds.reserve(m_targetItems.size());
    for (auto it = m_targetItems.constBegin(); it != m_targetItems.constEnd(); ++it)
        m_suppressedIds.insert(it.key());
    m_suppressUntilMs = QDateTime::currentMSecsSinceEpoch() + kClearSuppressMs;

    for (TargetItem *item : std::as_const(m_targetItems)) {
        removeItem(item);
        delete item;
    }
    m_targetItems.clear();

    m_lastMask = 0;
    m_grid->setOccupancyMask(0);

    emit frameRendered(0, 0, 0);
}
