#pragma once

#include <QMainWindow>
#include <QSettings>

class SerialManager;
class RadarProtocol;
class RadarModel;
class RadarView;
class ControlPanel;
class StatusPanel;
class LogPanel;

// Top-level window: wires SerialManager -> RadarProtocol -> RadarModel ->
// (RadarView, ControlPanel, StatusPanel, LogPanel) and owns the docking
// layout. Docks are movable/floatable/closable so the layout adapts to
// whatever the user needs (a small window, an ultrawide monitor, a second
// screen for just the log, ...); geometry and dock arrangement persist
// across runs via QSettings.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void wireSignals();
    void restoreSettings();
    void saveSettings();

    SerialManager *m_serial;
    RadarProtocol *m_protocol;
    RadarModel *m_model;

    RadarView *m_radarView;
    ControlPanel *m_controlPanel;
    StatusPanel *m_statusPanel;
    LogPanel *m_logPanel;

    QSettings m_settings;
};
