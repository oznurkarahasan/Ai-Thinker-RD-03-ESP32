#include "MainWindow.h"

#include <QComboBox>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSerialPortInfo>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>

#include "DashboardPanel.h"
#include "RadarComm.h"
#include "RadarScene.h"

namespace {
constexpr qint32 kBaudRate = 115200; // ESP32 USB-serial link to the PC (radar<->ESP32 UART runs at 256000 separately)
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_comm(new RadarComm(this))
    , m_scene(new RadarScene(this))
    , m_dashboard(new DashboardPanel(this))
    , m_view(new QGraphicsView(this))
    , m_portCombo(new QComboBox(this))
    , m_connectButton(new QPushButton(tr("Connect"), this))
{
    setWindowTitle(tr("RD-03D Radar Desktop"));
    resize(1100, 720);

    setupUi();
    setupView();

    connect(m_comm, &RadarComm::configReceived, m_scene, &RadarScene::onConfigReceived);
    connect(m_comm, &RadarComm::configReceived, m_dashboard, &DashboardPanel::setConfig);
    connect(m_comm, &RadarComm::configReceived, this, [this](const RadarConfig &) { fitGridInView(); });
    connect(m_comm, &RadarComm::targetsUpdated, m_scene, &RadarScene::onTargetsUpdated);
    connect(m_comm, &RadarComm::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_comm, &RadarComm::errorOccurred, this, &MainWindow::onCommError);
    connect(m_scene, &RadarScene::frameRendered, m_dashboard, &DashboardPanel::onFrameRendered);

    refreshPorts();
}

void MainWindow::setupUi()
{
    auto *toolbar = new QToolBar(tr("Connection"), this);
    toolbar->setMovable(false);
    addToolBar(toolbar);

    toolbar->addWidget(new QLabel(tr("Port:"), this));
    m_portCombo->setMinimumWidth(160);
    toolbar->addWidget(m_portCombo);

    auto *refreshBtn = new QPushButton(tr("Refresh"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    toolbar->addWidget(refreshBtn);

    toolbar->addWidget(m_connectButton);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    toolbar->addSeparator();
    const struct { const char *label; const char *cmd; } cmds[] = {
        {"DEBUG", "DEBUG"}, {"MULTI", "MULTI"}, {"EMA", "EMA"}, {"ZONES", "ZONES"}, {"CFG", "CFG"},
    };
    for (const auto &c : cmds) {
        auto *btn = new QPushButton(tr(c.label), this);
        const QString cmd = QString::fromLatin1(c.cmd);
        connect(btn, &QPushButton::clicked, this, [this, cmd]() { m_comm->sendCommand(cmd); });
        toolbar->addWidget(btn);
    }

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, /*stretch=*/1);
    layout->addWidget(m_dashboard);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupView()
{
    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    // The grid is static and repainted every frame only for occupancy color; caching the
    // background avoids re-rasterizing the (unchanging) scene backdrop every frame.
    m_view->setCacheMode(QGraphicsView::CacheBackground);
    m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_view->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setDragMode(QGraphicsView::NoDrag);
}

void MainWindow::fitGridInView()
{
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    fitGridInView();
}

void MainWindow::refreshPorts()
{
    const QString previous = m_portCombo->currentText();
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        m_portCombo->addItem(info.portName());
    const int idx = m_portCombo->findText(previous);
    if (idx >= 0)
        m_portCombo->setCurrentIndex(idx);
}

void MainWindow::toggleConnection()
{
    if (m_comm->isConnected()) {
        m_comm->disconnectPort();
        return;
    }
    const QString port = m_portCombo->currentText();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("No port selected"), tr("Select a serial port first."));
        return;
    }
    m_comm->connectToPort(port, kBaudRate);
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_dashboard->setConnectionState(connected, m_portCombo->currentText());
    statusBar()->showMessage(connected ? tr("Connected to %1 @ %2 baud").arg(m_portCombo->currentText()).arg(kBaudRate)
                                        : tr("Disconnected"));
}

void MainWindow::onCommError(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}
