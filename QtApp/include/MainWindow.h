#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QComboBox;
class QGraphicsView;
class QPushButton;
QT_END_NAMESPACE

class RadarComm;
class RadarScene;
class DashboardPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void toggleConnection();
    void onConnectionStateChanged(bool connected);
    void onCommError(const QString &message);
    void clearScene();

private:
    void setupUi();
    void setupView();
    void fitGridInView();

    void resizeEvent(QResizeEvent *event) override;

    RadarComm *m_comm;
    RadarScene *m_scene;
    DashboardPanel *m_dashboard;

    QGraphicsView *m_view;
    QComboBox *m_portCombo;
    QPushButton *m_connectButton;
};
