#include "ControlPanel.h"

#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QSerialPortInfo>

ControlPanel::ControlPanel(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);

    // ---- Connection ----
    auto *connGroup = new QGroupBox(tr("Bağlantı"));
    auto *connLayout = new QFormLayout(connGroup);

    auto *portRow = new QHBoxLayout();
    m_portCombo = new QComboBox();
    m_portCombo->setEditable(true); // allow typing a device path directly (e.g. /dev/ttyUSB0)
    auto *refreshBtn = new QPushButton(tr("⟳"));
    refreshBtn->setToolTip(tr("Port listesini yenile"));
    refreshBtn->setFixedWidth(32);
    portRow->addWidget(m_portCombo, 1);
    portRow->addWidget(refreshBtn);
    connLayout->addRow(tr("Port"), portRow);

    m_baudCombo = new QComboBox();
    m_baudCombo->setEditable(true);
    m_baudCombo->addItems({"256000", "115200", "921600"});
    m_baudCombo->setCurrentText("256000");
    m_baudCombo->setValidator(new QIntValidator(1, 4000000, m_baudCombo));
    connLayout->addRow(tr("Baud"), m_baudCombo);

    m_connectBtn = new QPushButton(tr("Bağlan"));
    connLayout->addRow(m_connectBtn);

    m_statusLabel = new QLabel(tr("Bağlı değil"));
    m_statusLabel->setStyleSheet("color: #9ca3af;");
    m_statusLabel->setWordWrap(true);
    connLayout->addRow(m_statusLabel);

    root->addWidget(connGroup);

    // ---- View options (local only, do not touch the device) ----
    auto *viewGroup = new QGroupBox(tr("Görünüm"));
    auto *viewLayout = new QFormLayout(viewGroup);

    m_halfFovCheck = new QCheckBox(tr("Yarım görüş açısı (180°)"));
    m_halfFovCheck->setChecked(true);
    viewLayout->addRow(m_halfFovCheck);

    m_scaleSpin = new QDoubleSpinBox();
    m_scaleSpin->setRange(1.0, 200.0);
    m_scaleSpin->setValue(10.0);
    m_scaleSpin->setSuffix(tr(" mm/px"));
    viewLayout->addRow(tr("Ölçek"), m_scaleSpin);

    m_targetsCheck = new QCheckBox(tr("Hedef etiketleri"));
    viewLayout->addRow(m_targetsCheck);

    m_trailCheck = new QCheckBox(tr("Hareket izi"));
    m_trailCheck->setChecked(true);
    viewLayout->addRow(m_trailCheck);

    m_tracksCheck = new QCheckBox(tr("Takip (track) işaretleri"));
    m_tracksCheck->setChecked(true);
    viewLayout->addRow(m_tracksCheck);

    root->addWidget(viewGroup);

    // ---- Device settings (sent as serial commands, echoed back by the firmware) ----
    auto *deviceGroup = new QGroupBox(tr("Cihaz Ayarları"));
    auto *deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->addWidget(makeDeviceToggle(tr("Ham hedef debug çıktısı (DEBUG)"), "DEBUG"));
    deviceLayout->addWidget(makeDeviceToggle(tr("Çoklu hedef modu (MULTI)"), "MULTI"));
    deviceLayout->addWidget(makeDeviceToggle(tr("EMA yumuşatma (EMA)"), "EMA"));
    auto *hint = new QLabel(tr("Üçüncü durum (—) cihazdan henüz onay gelmedi demektir."));
    hint->setStyleSheet("color: #9ca3af; font-size: 11px;");
    hint->setWordWrap(true);
    deviceLayout->addWidget(hint);
    root->addWidget(deviceGroup);

    // ---- One-shot commands ----
    auto *cmdGroup = new QGroupBox(tr("Komutlar"));
    auto *cmdLayout = new QHBoxLayout(cmdGroup);
    auto *zonesBtn = new QPushButton(tr("ZONES"));
    zonesBtn->setToolTip(tr("Bölge tanımlarını ve aktif takipleri iste"));
    auto *helpBtn = new QPushButton(tr("HELP"));
    helpBtn->setToolTip(tr("Cihazdan komut listesini iste"));
    auto *homekitBtn = new QPushButton(tr("HOMEKIT"));
    homekitBtn->setToolTip(tr("Yalnız HomeKit yazılımında: presence/motion durumunu iste"));
    cmdLayout->addWidget(zonesBtn);
    cmdLayout->addWidget(helpBtn);
    cmdLayout->addWidget(homekitBtn);
    root->addWidget(cmdGroup);

    root->addStretch(1);

    // ---- wiring ----
    connect(refreshBtn, &QPushButton::clicked, this, &ControlPanel::refreshPorts);
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (m_connected) {
            emit disconnectRequested();
        } else {
            emit connectRequested(m_portCombo->currentText().trimmed(), m_baudCombo->currentText().toInt());
        }
    });

    connect(m_halfFovCheck, &QCheckBox::toggled, this, &ControlPanel::halfFovChanged);
    connect(m_scaleSpin, &QDoubleSpinBox::valueChanged, this, &ControlPanel::scaleChanged);
    connect(m_targetsCheck, &QCheckBox::toggled, this, &ControlPanel::showTargetsChanged);
    connect(m_trailCheck, &QCheckBox::toggled, this, &ControlPanel::showTrailChanged);
    connect(m_tracksCheck, &QCheckBox::toggled, this, &ControlPanel::showTracksChanged);

    connect(zonesBtn, &QPushButton::clicked, this, [this] { emit commandRequested("ZONES"); });
    connect(helpBtn, &QPushButton::clicked, this, [this] { emit commandRequested("HELP"); });
    connect(homekitBtn, &QPushButton::clicked, this, [this] { emit commandRequested("HOMEKIT"); });

    refreshPorts();
    setConnectionState(false);
}

QCheckBox *ControlPanel::makeDeviceToggle(const QString &label, const QString &command) {
    auto *box = new QCheckBox(label);
    box->setTristate(true);
    box->setCheckState(Qt::PartiallyChecked);
    m_deviceToggleBoxes.insert(command, box);
    m_deviceToggleStates.insert(command, TriState::Unknown);

    connect(box, &QCheckBox::clicked, this, [this, command, box](bool) {
        // The device toggles are relative (there is no "set to ON" command),
        // so don't let the checkbox jump ahead of reality: snap it straight
        // back to the last confirmed state and wait for the firmware's echo.
        const TriState last = m_deviceToggleStates.value(command, TriState::Unknown);
        const Qt::CheckState cs = last == TriState::On ? Qt::Checked
                                 : last == TriState::Off ? Qt::Unchecked
                                                          : Qt::PartiallyChecked;
        QSignalBlocker blocker(box);
        box->setCheckState(cs);
        emit commandRequested(command);
    });

    return box;
}

void ControlPanel::refreshPorts() {
    const QString previous = m_portCombo->currentText();
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        // Keep the editable field's text as a bare, connectable port name;
        // put the human-readable description in the tooltip instead so it
        // doesn't get sent to QSerialPort::setPortName() by mistake.
        const int idx = m_portCombo->count();
        m_portCombo->addItem(info.portName());
        if (!info.description().isEmpty()) {
            m_portCombo->setItemData(idx, info.description(), Qt::ToolTipRole);
        }
    }
    if (!previous.isEmpty()) m_portCombo->setCurrentText(previous);
}

void ControlPanel::setConnectionState(bool connected) {
    m_connected = connected;
    m_connectBtn->setText(connected ? tr("Bağlantıyı Kes") : tr("Bağlan"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
}

void ControlPanel::setStatusText(const QString &text) {
    m_statusLabel->setText(text);
}

void ControlPanel::setPortHint(const QString &port) {
    if (!port.isEmpty()) m_portCombo->setCurrentText(port);
}

void ControlPanel::setBaudHint(const QString &baud) {
    if (!baud.isEmpty()) m_baudCombo->setCurrentText(baud);
}

void ControlPanel::setHalfFovChecked(bool half) { m_halfFovCheck->setChecked(half); }
void ControlPanel::setScaleValue(double mmPerPx) { m_scaleSpin->setValue(mmPerPx); }
void ControlPanel::setShowTargetsChecked(bool show) { m_targetsCheck->setChecked(show); }
void ControlPanel::setShowTrailChecked(bool show) { m_trailCheck->setChecked(show); }
void ControlPanel::setShowTracksChecked(bool show) { m_tracksCheck->setChecked(show); }

void ControlPanel::setDeviceSetting(const QString &key, TriState state) {
    m_deviceToggleStates[key] = state;
    QCheckBox *box = m_deviceToggleBoxes.value(key, nullptr);
    if (!box) return;
    const Qt::CheckState cs = state == TriState::On ? Qt::Checked
                             : state == TriState::Off ? Qt::Unchecked
                                                       : Qt::PartiallyChecked;
    QSignalBlocker blocker(box);
    box->setCheckState(cs);
}
