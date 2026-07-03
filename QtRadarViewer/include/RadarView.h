#pragma once

#include <QWidget>
#include <QVector>
#include <QMap>
#include <QPointF>
#include <QElapsedTimer>

#include "RadarModel.h"

// Custom-painted polar radar display: distance rings, the 14-cell zone
// grid (colored by occupancy), live target dots and a fading motion trail.
// This is the Qt/QPainter equivalent of GUI/p5js-rd03-tiles/sketch.js.
class RadarView : public QWidget {
    Q_OBJECT
public:
    explicit RadarView(RadarModel *model, QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(560, 560); }
    QSize minimumSizeHint() const override { return QSize(220, 220); }

public slots:
    void setHalfFov(bool half);
    void setScaleMmPerPixel(double mmPerPx);
    void setShowTargets(bool show);
    void setShowTrail(bool show);
    void setShowTracks(bool show);
    void setDimStationary(bool dim);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTracksChanged();

private:
    bool targetInFov(double screenX, double screenY) const;
    void drawRings(QPainter &painter, double maxRadiusPx) const;
    void drawZones(QPainter &painter, double k) const;
    void drawTrail(QPainter &painter, double k) const;
    void drawTargets(QPainter &painter, double k) const;
    void drawTracks(QPainter &painter, double k) const;
    void pruneTrail(qint64 now);

    // A track that has barely moved for kStationaryWindowMs is more likely
    // to be a static clutter/multipath reflection than a person - the
    // RD-03D and similar cheap FMCW modules are known to occasionally latch
    // onto a fixed reflector and report it at ~0 speed indefinitely. Such
    // tracks are dimmed (not deleted) so a real, very-still person is still
    // visible, just visually de-emphasized and labelled.
    bool isTrackStationary(int trackId, qint64 now) const;
    double stationaryOpacityFactor(int trackId, qint64 now) const;

    RadarModel *m_model;
    bool m_half = true;
    double m_mmPerPx = 10.0;
    bool m_showTargets = false;
    bool m_showTrail = true;
    bool m_showTracks = true;
    bool m_dimStationary = true;

    // One fading path per track ID (not per array index!). Connecting
    // points by their position in the firmware's "Targets: N" list instead
    // of by the track's own ID would draw a line between two physically
    // unrelated detections whenever a target appears/disappears/reorders -
    // that's what caused stray diagonal lines across the whole view.
    struct TrailPoint {
        qint64 timestampMs;
        QPointF mm;
    };
    QMap<int, QVector<TrailPoint>> m_trackTrails;
    QElapsedTimer m_clock;
    static constexpr qint64 kTrailMs = 30000;
    static constexpr qint64 kStationaryWindowMs = 4000;
    static constexpr double kStationaryRadiusMm = 60.0;
    static constexpr double kStationaryOpacity = 0.22;
    static constexpr double kTargetTrackAssociationMm = 600.0;
};
