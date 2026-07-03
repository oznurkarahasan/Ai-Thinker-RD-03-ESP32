#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "RadarTypes.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
QT_END_NAMESPACE

// Side panel: connection status, render FPS, active target count and the live list of
// occupied zone names. FPS is measured from actual frameRendered() emissions (i.e. real
// telemetry arrival rate), not a UI redraw timer, so it reflects true end-to-end latency
// health rather than just how fast Qt can repaint.
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPanel(QWidget *parent = nullptr);

    void setConfig(const RadarConfig &config);

public slots:
    void setConnectionState(bool connected, const QString &portName = QString());
    void onFrameRendered(int activeTargets, quint32 zoneOccupiedMask, quint32 seq);

private:
    void refreshZoneList(quint32 mask);

    QLabel *m_statusLabel;
    QLabel *m_fpsLabel;
    QLabel *m_targetCountLabel;
    QLabel *m_seqLabel;
    QListWidget *m_zoneList;

    RadarConfig m_config;
    QElapsedTimer m_fpsClock;
    int m_framesSinceTick = 0;
    double m_currentFps = 0.0;
};
