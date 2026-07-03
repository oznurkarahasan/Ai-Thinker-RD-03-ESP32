#pragma once

#include <QWidget>

#include "RadarModel.h"

class QLabel;
class QTableWidget;
class QListWidget;

// Right-hand dock: everything the radar view can't show with color alone -
// a live track table (id/position/zone), the occupied-zone list, and the
// HomeKit presence/motion badges when that firmware build is in use.
class StatusPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatusPanel(RadarModel *model, QWidget *parent = nullptr);

private slots:
    void refreshTracks();
    void refreshZones();
    void refreshPresenceMotion();

private:
    static QString badgeStyle(TriState state);

    RadarModel *m_model;
    QLabel *m_activeTracksLabel;
    QLabel *m_presenceBadge;
    QLabel *m_motionBadge;
    QTableWidget *m_trackTable;
    QListWidget *m_zoneList;
};
