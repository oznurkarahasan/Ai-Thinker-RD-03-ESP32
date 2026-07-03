#pragma once

#include <QWidget>
#include <QVector>
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

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTargetsChanged();

private:
    bool targetInFov(double screenX, double screenY) const;
    void drawRings(QPainter &painter, double maxRadiusPx) const;
    void drawZones(QPainter &painter, double k) const;
    void drawTrail(QPainter &painter, double k) const;
    void drawTargets(QPainter &painter, double k) const;
    void drawTracks(QPainter &painter, double k) const;

    RadarModel *m_model;
    bool m_half = true;
    double m_mmPerPx = 10.0;
    bool m_showTargets = false;
    bool m_showTrail = true;
    bool m_showTracks = true;

    struct TrailFrame {
        qint64 timestampMs;
        QVector<QPointF> pointsMm;
    };
    QVector<TrailFrame> m_trail;
    QElapsedTimer m_clock;
    static constexpr qint64 kTrailMs = 30000;
};
