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
    connect(m_comm, &RadarComm::targetsUpdated, m_scene, &RadarScene::onTargetsUpdated);
    connect(m_comm, &RadarComm::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_comm, &RadarComm::errorOccurred, this, &MainWindow::onCommError);
    connect(m_scene, &RadarScene::frameRendered, m_dashboard, &DashboardPanel::onFrameRendered);

    refreshPorts();
    // The view's logical area never changes after this (see RadarScene::lockedViewRect()), so
    // one fitInView() up front is all that's needed beyond handling actual widget resizes.
    fitGridInView();
}

void MainWindow::setupUi()
{
    auto *toolbar = new QToolBar(tr("Connection"), this);
    toolbar->setMovable(false);
    addToolBar(toolbar);

    // Shared across every toolbar button: a clear, high-contrast "pressed" flash so momentary
    // commands (Refresh, Clear, ZONES, CFG, Connect) give instant click feedback, plus a bright
    // "checked" state reserved for the true persistent toggles (DEBUG/MULTI/EMA below). Applied
    // once here via descendant selector rather than per-button, so every button — checkable or
    // not — gets consistent styling.
    toolbar->setStyleSheet(
        "QPushButton { background-color: #2b2f36; color: #cfd6e4; border: 1px solid #454b57;"
        " border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:pressed { background-color: #00e5ff; color: #062b13; border: 1px solid #00e5ff; }"
        "QPushButton:checked { background-color: #39ff14; color: #062b13; border: 1px solid #39ff14;"
        " font-weight: bold; }");

    toolbar->addWidget(new QLabel(tr("Port:"), this));
    m_portCombo->setMinimumWidth(160);
    toolbar->addWidget(m_portCombo);

    auto *refreshBtn = new QPushButton(tr("Refresh"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    toolbar->addWidget(refreshBtn);

    toolbar->addWidget(m_connectButton);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    toolbar->addSeparator();

    auto *clearBtn = new QPushButton(tr("Clear"), this);
    clearBtn->setToolTip(tr("Wipe all targets, trails and zone occupancy from the view"));
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearScene);
    toolbar->addWidget(clearBtn);

    toolbar->addSeparator();

    // DEBUG/MULTI/EMA are firmware-side persistent toggles, so they're checkable and land in
    // the bright ":checked" state above. The firmware only echoes these as human-readable text
    // (not JSON), which RadarComm deliberately ignores, so there's no live readback —
    // initialChecked below just mirrors ESP32_RD03D.ino's boot defaults (DEBUG_RAW_TARGETS=false,
    // MULTI_TARGET=true, EMA_ENABLED=true) as a best-effort starting point, not a synced device
    // state. ZONES/CFG are deliberately NOT checkable: on the firmware they're one-shot
    // "resend/dump now" commands with no persisted on/off state, so a checked state for them
    // would just be a lie that sticks after a single click — the shared :pressed flash above
    // already gives them clear, honest click feedback.
    const struct { const char *label; const char *cmd; bool checkable; bool initialChecked; } cmds[] = {
        {"DEBUG", "DEBUG", true, false},
        {"MULTI", "MULTI", true, true},
        {"EMA", "EMA", true, true},
        {"ZONES", "ZONES", false, false},
        {"CFG", "CFG", false, false},
    };
    for (const auto &c : cmds) {
        auto *btn = new QPushButton(tr(c.label), this);
        const QString cmd = QString::fromLatin1(c.cmd);
        connect(btn, &QPushButton::clicked, this, [this, cmd]() { m_comm->sendCommand(cmd); });
        if (c.checkable) {
            btn->setCheckable(true);
            btn->setChecked(c.initialChecked);
        }
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
    // The view's logical area is fixed (see RadarScene::lockedViewRect()); scrollbars would
    // only let the user pan into permanently-blank space, so they're pointless here.
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void MainWindow::fitGridInView()
{
    // Deliberately fit against the scene's fixed logical rect, not any dynamically computed
    // bounds (e.g. scene()->itemsBoundingRect()) — a stray target far outside normal sensor
    // range must never change the view's zoom/transform.
    m_view->fitInView(RadarScene::lockedViewRect(), Qt::KeepAspectRatio);
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

void MainWindow::clearScene()
{
    m_scene->clearScene();
    statusBar()->showMessage(tr("Cleared all targets and zone occupancy"), 3000);
}
