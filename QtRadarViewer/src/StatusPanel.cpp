#include "StatusPanel.h"

#include <QLabel>
#include <QTableWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QGroupBox>

StatusPanel::StatusPanel(RadarModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    auto *root = new QVBoxLayout(this);

    auto *summaryGroup = new QGroupBox(tr("Durum"));
    auto *summaryLayout = new QVBoxLayout(summaryGroup);
    m_activeTracksLabel = new QLabel(tr("Aktif takip: —"));
    summaryLayout->addWidget(m_activeTracksLabel);

    auto *badgeRow = new QVBoxLayout();
    m_presenceBadge = new QLabel(tr("Presence: —"));
    m_motionBadge = new QLabel(tr("Motion: —"));
    m_presenceBadge->setStyleSheet(badgeStyle(TriState::Unknown));
    m_motionBadge->setStyleSheet(badgeStyle(TriState::Unknown));
    m_presenceBadge->setVisible(false);
    m_motionBadge->setVisible(false);
    badgeRow->addWidget(m_presenceBadge);
    badgeRow->addWidget(m_motionBadge);
    summaryLayout->addLayout(badgeRow);
    root->addWidget(summaryGroup);

    auto *tracksGroup = new QGroupBox(tr("Takipler (Tracks)"));
    auto *tracksLayout = new QVBoxLayout(tracksGroup);
    m_trackTable = new QTableWidget(0, 4);
    m_trackTable->setHorizontalHeaderLabels({tr("ID"), tr("X (mm)"), tr("Y (mm)"), tr("Bölge")});
    m_trackTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_trackTable->verticalHeader()->setVisible(false);
    m_trackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trackTable->setSelectionMode(QAbstractItemView::NoSelection);
    tracksLayout->addWidget(m_trackTable);
    root->addWidget(tracksGroup, 1);

    auto *zonesGroup = new QGroupBox(tr("Dolu Bölgeler"));
    auto *zonesLayout = new QVBoxLayout(zonesGroup);
    m_zoneList = new QListWidget();
    zonesLayout->addWidget(m_zoneList);
    root->addWidget(zonesGroup, 1);

    connect(m_model, &RadarModel::tracksChanged, this, &StatusPanel::refreshTracks);
    connect(m_model, &RadarModel::zonesChanged, this, &StatusPanel::refreshZones);
    connect(m_model, &RadarModel::presenceMotionChanged, this, &StatusPanel::refreshPresenceMotion);

    refreshTracks();
    refreshZones();
    refreshPresenceMotion();
}

QString StatusPanel::badgeStyle(TriState state) {
    switch (state) {
        case TriState::On:
            return "padding:4px 8px; border-radius:4px; background:#ef4444; color:white; font-weight:600;";
        case TriState::Off:
            return "padding:4px 8px; border-radius:4px; background:#2a2a2a; color:#9ca3af;";
        default:
            return "padding:4px 8px; border-radius:4px; background:#1a1a1a; color:#666;";
    }
}

void StatusPanel::refreshTracks() {
    const int active = m_model->activeTrackCount();
    m_activeTracksLabel->setText(active >= 0 ? tr("Aktif takip: %1").arg(active) : tr("Aktif takip: —"));

    const QVector<Track> tracks = m_model->tracks();
    m_trackTable->setRowCount(tracks.size());
    for (int row = 0; row < tracks.size(); ++row) {
        const Track &t = tracks[row];
        m_trackTable->setItem(row, 0, new QTableWidgetItem(QString::number(t.id)));
        m_trackTable->setItem(row, 1, new QTableWidgetItem(t.hasPosition ? QString::number(t.x, 'f', 0) : "—"));
        m_trackTable->setItem(row, 2, new QTableWidgetItem(t.hasPosition ? QString::number(t.y, 'f', 0) : "—"));
        m_trackTable->setItem(row, 3, new QTableWidgetItem(t.zone.isEmpty() ? "—" : t.zone));
    }
}

void StatusPanel::refreshZones() {
    m_zoneList->clear();
    for (const ZoneDef &z : m_model->zoneDefs()) {
        if (m_model->isZoneOccupied(z.name)) {
            m_zoneList->addItem(z.name);
        }
    }
    if (m_zoneList->count() == 0) {
        auto *item = new QListWidgetItem(tr("(boş)"));
        item->setFlags(Qt::NoItemFlags);
        m_zoneList->addItem(item);
    }
}

void StatusPanel::refreshPresenceMotion() {
    const TriState presence = m_model->presenceState();
    const TriState motion = m_model->motionState();

    const bool anyHomeKitData = presence != TriState::Unknown || motion != TriState::Unknown;
    m_presenceBadge->setVisible(anyHomeKitData);
    m_motionBadge->setVisible(anyHomeKitData);

    m_presenceBadge->setText(tr("Presence: %1").arg(presence == TriState::On ? tr("VAR") : tr("YOK")));
    m_motionBadge->setText(tr("Motion: %1").arg(motion == TriState::On ? tr("VAR") : tr("YOK")));
    m_presenceBadge->setStyleSheet(badgeStyle(presence));
    m_motionBadge->setStyleSheet(badgeStyle(motion));
}
