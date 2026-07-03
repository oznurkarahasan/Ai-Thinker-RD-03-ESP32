#include "MainWindow.h"

#include <QDockWidget>
#include <QCloseEvent>
#include <QStatusBar>

#include "SerialManager.h"
#include "RadarProtocol.h"
#include "RadarModel.h"
#include "RadarView.h"
#include "ControlPanel.h"
#include "StatusPanel.h"
#include "LogPanel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(QStringLiteral("RD03D"), QStringLiteral("RadarViewer")) {
    setWindowTitle(tr("RD-03D Radar Viewer"));

    m_serial = new SerialManager(this);
    m_protocol = new RadarProtocol(this);
    m_model = new RadarModel(this);

    m_radarView = new RadarView(m_model, this);
    setCentralWidget(m_radarView);

    m_controlPanel = new ControlPanel(this);
    auto *controlDock = new QDockWidget(tr("Ayarlar"), this);
    controlDock->setObjectName("controlDock");
    controlDock->setWidget(m_controlPanel);
    controlDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, controlDock);

    m_statusPanel = new StatusPanel(m_model, this);
    auto *statusDock = new QDockWidget(tr("Bölgeler ve Takipler"), this);
    statusDock->setObjectName("statusDock");
    statusDock->setWidget(m_statusPanel);
    statusDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, statusDock);

    m_logPanel = new LogPanel(this);
    auto *logDock = new QDockWidget(tr("Mesaj Günlüğü"), this);
    logDock->setObjectName("logDock");
    logDock->setWidget(m_logPanel);
    logDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    statusBar()->showMessage(tr("Hazır"));

    resize(1280, 860);

    wireSignals();
    restoreSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::wireSignals() {
    // Serial -> protocol / log (every line, understood or not, reaches the log)
    connect(m_serial, &SerialManager::lineReceived, m_protocol, &RadarProtocol::feedLine);
    connect(m_serial, &SerialManager::lineReceived, m_logPanel, &LogPanel::appendRawLine);

    connect(m_serial, &SerialManager::connected, this, [this](const QString &port, qint32 baud) {
        m_controlPanel->setConnectionState(true);
        m_controlPanel->setStatusText(tr("Bağlı: %1 @ %2 baud").arg(port).arg(baud));
        statusBar()->showMessage(tr("%1 portuna %2 baud ile bağlanıldı").arg(port, QString::number(baud)), 4000);
        m_settings.setValue("lastPort", port);
        m_settings.setValue("lastBaud", QString::number(baud));
    });
    connect(m_serial, &SerialManager::disconnected, this, [this] {
        m_controlPanel->setConnectionState(false);
        m_controlPanel->setStatusText(tr("Bağlı değil"));
        statusBar()->showMessage(tr("Bağlantı kesildi"), 4000);
        m_protocol->reset();
        m_model->onDisconnected();
    });
    connect(m_serial, &SerialManager::errorOccurred, this, [this](const QString &message) {
        m_controlPanel->setStatusText(message);
        statusBar()->showMessage(message, 6000);
    });

    // Protocol -> model
    connect(m_protocol, &RadarProtocol::targetsUpdated, m_model, &RadarModel::onTargetsUpdated);
    connect(m_protocol, &RadarProtocol::zoneStatusResetRequested, m_model, &RadarModel::onZoneStatusResetRequested);
    connect(m_protocol, &RadarProtocol::zoneOccupancyChanged, m_model, &RadarModel::onZoneOccupancyChanged);
    connect(m_protocol, &RadarProtocol::activeTrackCountUpdated, m_model, &RadarModel::onActiveTrackCountUpdated);
    connect(m_protocol, &RadarProtocol::zoneDefinitionUpdated, m_model, &RadarModel::onZoneDefinitionUpdated);
    connect(m_protocol, &RadarProtocol::trackCreated, m_model, &RadarModel::onTrackCreated);
    connect(m_protocol, &RadarProtocol::trackLost, m_model, &RadarModel::onTrackLost);
    connect(m_protocol, &RadarProtocol::trackPositionUpdated, m_model, &RadarModel::onTrackPositionUpdated);
    connect(m_protocol, &RadarProtocol::deviceSettingChanged, m_model, &RadarModel::onDeviceSettingChanged);
    connect(m_protocol, &RadarProtocol::presenceChanged, m_model, &RadarModel::onPresenceChanged);
    connect(m_protocol, &RadarProtocol::motionChanged, m_model, &RadarModel::onMotionChanged);
    connect(m_protocol, &RadarProtocol::helpTextReceived, m_model, &RadarModel::onHelpTextReceived);

    // Model -> control panel (device toggle echoes reflected in the checkboxes)
    connect(m_model, &RadarModel::settingChanged, m_controlPanel, &ControlPanel::setDeviceSetting);
    connect(m_model, &RadarModel::helpReceived, this, [this](const QStringList &lines) {
        statusBar()->showMessage(tr("Cihazdan yardım metni alındı (%1 satır) — Mesaj Günlüğü'nde görüntülenebilir").arg(lines.size()), 5000);
    });

    // Control panel -> serial
    connect(m_controlPanel, &ControlPanel::connectRequested, m_serial, &SerialManager::connectTo);
    connect(m_controlPanel, &ControlPanel::disconnectRequested, m_serial, &SerialManager::disconnectPort);
    connect(m_controlPanel, &ControlPanel::commandRequested, m_serial, &SerialManager::sendCommand);

    // Control panel -> radar view (local, no device round-trip)
    connect(m_controlPanel, &ControlPanel::halfFovChanged, m_radarView, &RadarView::setHalfFov);
    connect(m_controlPanel, &ControlPanel::scaleChanged, m_radarView, &RadarView::setScaleMmPerPixel);
    connect(m_controlPanel, &ControlPanel::showTargetsChanged, m_radarView, &RadarView::setShowTargets);
    connect(m_controlPanel, &ControlPanel::showTrailChanged, m_radarView, &RadarView::setShowTrail);
    connect(m_controlPanel, &ControlPanel::showTracksChanged, m_radarView, &RadarView::setShowTracks);
    connect(m_controlPanel, &ControlPanel::dimStationaryChanged, m_radarView, &RadarView::setDimStationary);

    // Persist view preferences as they change.
    connect(m_controlPanel, &ControlPanel::halfFovChanged, this, [this](bool v) { m_settings.setValue("view/halfFov", v); });
    connect(m_controlPanel, &ControlPanel::scaleChanged, this, [this](double v) { m_settings.setValue("view/scale", v); });
    connect(m_controlPanel, &ControlPanel::showTargetsChanged, this, [this](bool v) { m_settings.setValue("view/targets", v); });
    connect(m_controlPanel, &ControlPanel::showTrailChanged, this, [this](bool v) { m_settings.setValue("view/trail", v); });
    connect(m_controlPanel, &ControlPanel::showTracksChanged, this, [this](bool v) { m_settings.setValue("view/tracks", v); });
    connect(m_controlPanel, &ControlPanel::dimStationaryChanged, this, [this](bool v) { m_settings.setValue("view/dimStationary", v); });
}

void MainWindow::restoreSettings() {
    if (m_settings.contains("geometry")) restoreGeometry(m_settings.value("geometry").toByteArray());
    if (m_settings.contains("windowState")) restoreState(m_settings.value("windowState").toByteArray());

    m_controlPanel->setPortHint(m_settings.value("lastPort").toString());
    m_controlPanel->setBaudHint(m_settings.value("lastBaud").toString());

    m_controlPanel->setHalfFovChecked(m_settings.value("view/halfFov", true).toBool());
    m_controlPanel->setScaleValue(m_settings.value("view/scale", 10.0).toDouble());
    m_controlPanel->setShowTargetsChecked(m_settings.value("view/targets", false).toBool());
    m_controlPanel->setShowTrailChecked(m_settings.value("view/trail", true).toBool());
    m_controlPanel->setShowTracksChecked(m_settings.value("view/tracks", true).toBool());
    m_controlPanel->setDimStationaryChecked(m_settings.value("view/dimStationary", true).toBool());
}

void MainWindow::saveSettings() {
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}
