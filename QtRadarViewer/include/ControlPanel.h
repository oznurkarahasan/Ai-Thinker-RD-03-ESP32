#pragma once

#include <QWidget>
#include <QMap>

#include "Types.h"

class QComboBox;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;

// Sidebar: connection controls, view options (local-only) and device
// commands/toggles (sent over serial). Kept dumb on purpose - it only
// emits intent; MainWindow wires it to SerialManager/RadarModel.
class ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit ControlPanel(QWidget *parent = nullptr);

public slots:
    void refreshPorts();
    void setConnectionState(bool connected);
    void setStatusText(const QString &text);
    void setDeviceSetting(const QString &key, TriState state);

    // Used to restore persisted UI preferences at startup.
    void setPortHint(const QString &port);
    void setBaudHint(const QString &baud);
    void setHalfFovChecked(bool half);
    void setScaleValue(double mmPerPx);
    void setShowTargetsChecked(bool show);
    void setShowTrailChecked(bool show);
    void setShowTracksChecked(bool show);
    void setDimStationaryChecked(bool dim);

signals:
    void connectRequested(const QString &portName, qint32 baudRate);
    void disconnectRequested();
    void commandRequested(const QString &command);

    void halfFovChanged(bool half);
    void scaleChanged(double mmPerPx);
    void showTargetsChanged(bool show);
    void showTrailChanged(bool show);
    void showTracksChanged(bool show);
    void dimStationaryChanged(bool dim);

private:
    QCheckBox *makeDeviceToggle(const QString &label, const QString &command);

    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_connectBtn;
    QLabel *m_statusLabel;

    QCheckBox *m_halfFovCheck;
    QDoubleSpinBox *m_scaleSpin;
    QCheckBox *m_targetsCheck;
    QCheckBox *m_trailCheck;
    QCheckBox *m_tracksCheck;
    QCheckBox *m_dimStationaryCheck;

    QMap<QString, QCheckBox *> m_deviceToggleBoxes; // key: DEBUG/MULTI/EMA
    QMap<QString, TriState> m_deviceToggleStates;

    bool m_connected = false;
};
