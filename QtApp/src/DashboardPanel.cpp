#include "DashboardPanel.h"

#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

DashboardPanel::DashboardPanel(QWidget *parent)
    : QWidget(parent)
    , m_statusLabel(new QLabel(tr("Disconnected")))
    , m_fpsLabel(new QLabel(tr("FPS: -")))
    , m_targetCountLabel(new QLabel(tr("Active targets: 0")))
    , m_seqLabel(new QLabel(tr("Seq: -")))
    , m_zoneList(new QListWidget)
{
    setMinimumWidth(220);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);

    auto *title = new QLabel(tr("Radar Dashboard"));
    title->setFont(titleFont);
    layout->addWidget(title);

    m_statusLabel->setStyleSheet("color:#e05555; font-weight:bold;");
    layout->addWidget(m_statusLabel);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    layout->addWidget(sep);

    layout->addWidget(m_fpsLabel);
    layout->addWidget(m_targetCountLabel);
    layout->addWidget(m_seqLabel);

    auto *zoneTitle = new QLabel(tr("Occupied zones"));
    zoneTitle->setFont(titleFont);
    layout->addWidget(zoneTitle);
    layout->addWidget(m_zoneList, /*stretch=*/1);

    m_fpsClock.start();
}

void DashboardPanel::setConfig(const RadarConfig &config)
{
    m_config = config;
}

void DashboardPanel::setConnectionState(bool connected, const QString &portName)
{
    if (connected) {
        m_statusLabel->setText(tr("Connected: %1").arg(portName));
        m_statusLabel->setStyleSheet("color:#4caf50; font-weight:bold;");
    } else {
        m_statusLabel->setText(tr("Disconnected"));
        m_statusLabel->setStyleSheet("color:#e05555; font-weight:bold;");
        m_fpsLabel->setText(tr("FPS: -"));
        m_targetCountLabel->setText(tr("Active targets: 0"));
        m_seqLabel->setText(tr("Seq: -"));
        m_zoneList->clear();
    }
}

void DashboardPanel::onFrameRendered(int activeTargets, quint32 zoneOccupiedMask, quint32 seq)
{
    ++m_framesSinceTick;
    const qint64 elapsed = m_fpsClock.elapsed();
    if (elapsed >= 500) { // update the displayed rate twice a second, not every frame
        m_currentFps = 1000.0 * m_framesSinceTick / elapsed;
        m_framesSinceTick = 0;
        m_fpsClock.restart();
        m_fpsLabel->setText(tr("FPS: %1").arg(m_currentFps, 0, 'f', 1));
    }

    m_targetCountLabel->setText(tr("Active targets: %1").arg(activeTargets));
    m_seqLabel->setText(tr("Seq: %1").arg(seq));
    refreshZoneList(zoneOccupiedMask);
}

void DashboardPanel::refreshZoneList(quint32 mask)
{
    m_zoneList->clear();
    for (int i = 0; i < m_config.zoneNames.size(); ++i) {
        if (mask & (1u << i))
            m_zoneList->addItem(m_config.zoneNames[i]);
    }
}
