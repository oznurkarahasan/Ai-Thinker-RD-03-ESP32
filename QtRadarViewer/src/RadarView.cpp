#include "RadarView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPaddingPx = 20.0;
constexpr int kRingCount = 6;
}

RadarView::RadarView(RadarModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    setMinimumSize(220, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

    m_clock.start();

    connect(m_model, &RadarModel::targetsChanged, this, [this] { update(); });
    connect(m_model, &RadarModel::zonesChanged, this, [this] { update(); });
    connect(m_model, &RadarModel::tracksChanged, this, &RadarView::onTracksChanged);

    // Decouple repaint cadence from the (potentially bursty) serial data
    // rate: repaint at a steady ~30 fps instead of once per model signal.
    auto *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, QOverload<>::of(&RadarView::update));
    repaintTimer->start(33);
}

void RadarView::setHalfFov(bool half) { m_half = half; }
void RadarView::setScaleMmPerPixel(double mmPerPx) { m_mmPerPx = std::max(1.0, mmPerPx); }
void RadarView::setShowTargets(bool show) { m_showTargets = show; }
void RadarView::setShowTrail(bool show) { m_showTrail = show; }
void RadarView::setShowTracks(bool show) { m_showTracks = show; }
void RadarView::setDimStationary(bool dim) { m_dimStationary = dim; }

void RadarView::onTracksChanged() {
    // Position history is collected unconditionally (not just when the
    // trail is visible) because stationary-target detection needs it too.
    const qint64 now = m_clock.elapsed();

    for (const Track &t : m_model->tracks()) {
        if (!t.hasPosition) continue;
        QVector<TrailPoint> &pts = m_trackTrails[t.id];
        const QPointF mm(t.x, t.y);
        // Skip exact duplicates: a zone-only update (e.g. "entered zone")
        // reuses the last known position and would otherwise pad the path
        // with a zero-length segment for every such event.
        if (!pts.isEmpty() && pts.constLast().mm == mm) continue;
        pts.append({now, mm});
    }
    pruneTrail(now);
}

void RadarView::pruneTrail(qint64 now) {
    for (auto it = m_trackTrails.begin(); it != m_trackTrails.end();) {
        QVector<TrailPoint> &pts = it.value();
        while (!pts.isEmpty() && (now - pts.constFirst().timestampMs) > kTrailMs) {
            pts.removeFirst();
        }
        if (pts.isEmpty()) it = m_trackTrails.erase(it);
        else ++it;
    }
}

bool RadarView::isTrackStationary(int trackId, qint64 now) const {
    const auto it = m_trackTrails.constFind(trackId);
    if (it == m_trackTrails.constEnd()) return false;
    const QVector<TrailPoint> &pts = it.value();
    if (pts.isEmpty()) return false;

    // Require the track to have existed for the full window already, so a
    // brand-new track isn't dimmed before it's had a chance to move.
    if (now - pts.constFirst().timestampMs < kStationaryWindowMs) return false;

    double cx = 0, cy = 0;
    int n = 0;
    for (const TrailPoint &p : pts) {
        if (now - p.timestampMs > kStationaryWindowMs) continue;
        cx += p.mm.x();
        cy += p.mm.y();
        ++n;
    }
    if (n == 0) return false;
    cx /= n;
    cy /= n;

    double maxDistSq = 0;
    for (const TrailPoint &p : pts) {
        if (now - p.timestampMs > kStationaryWindowMs) continue;
        const double dx = p.mm.x() - cx;
        const double dy = p.mm.y() - cy;
        maxDistSq = std::max(maxDistSq, dx * dx + dy * dy);
    }
    return maxDistSq <= (kStationaryRadiusMm * kStationaryRadiusMm);
}

double RadarView::stationaryOpacityFactor(int trackId, qint64 now) const {
    if (!m_dimStationary) return 1.0;
    return isTrackStationary(trackId, now) ? kStationaryOpacity : 1.0;
}

bool RadarView::targetInFov(double, double screenY) const {
    if (!m_half) return true;
    return screenY <= 0.0;
}

void RadarView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(18, 18, 18));

    const double cx = width() / 2.0;
    const double cy = m_half ? (height() - kPaddingPx) : (height() / 2.0);
    painter.translate(cx, cy);

    double maxR;
    if (m_half) {
        maxR = std::max(0.0, std::min(width() / 2.0 - kPaddingPx, height() - 2 * kPaddingPx));
    } else {
        maxR = std::max(0.0, std::min(width(), height()) / 2.0 - kPaddingPx);
    }

    drawRings(painter, maxR);
    const double k = 1.0 / m_mmPerPx; // mm -> px
    drawZones(painter, k);
    if (m_showTrail) {
        pruneTrail(m_clock.elapsed()); // let paths fade out even if tracks stop updating
        drawTrail(painter, k);
    }
    if (m_showTracks) drawTracks(painter, k);
    if (m_showTargets) drawTargets(painter, k);
}

void RadarView::drawRings(QPainter &painter, double maxR) const {
    painter.setPen(QPen(QColor(50, 50, 50), 1));

    if (maxR > 0) {
        const double stepPx = maxR / kRingCount;
        for (double r = stepPx; r <= maxR + 1.0; r += stepPx) {
            painter.setBrush(Qt::NoBrush);
            if (m_half) {
                QRectF box(-r, -r, r * 2, r * 2);
                painter.drawArc(box, 0 * 16, 180 * 16); // top semicircle
            } else {
                painter.drawEllipse(QPointF(0, 0), r, r);
            }

            const double distMm = r * m_mmPerPx;
            QString label;
            if (distMm >= 1000.0) {
                const double m = distMm / 1000.0;
                label = (m >= 10.0 ? QString::number(m, 'f', 0) : QString::number(m, 'f', 1)) + " m";
            } else {
                label = QString::number(std::round(distMm / 10.0)) + " cm";
            }

            painter.setPen(QColor(180, 180, 180));
            painter.setFont(QFont(painter.font().family(), 9));
            QFontMetricsF fm(painter.font());
            const QRectF textRect(-r - fm.horizontalAdvance(label), -r - fm.height() - 2, 2 * r + 2 * fm.horizontalAdvance(label), fm.height());
            painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, label);
            painter.setPen(QPen(QColor(50, 50, 50), 1));
        }
    }

    painter.setPen(QPen(QColor(40, 40, 40), 1));
    painter.drawLine(QPointF(-width() / 2.0, 0), QPointF(width() / 2.0, 0));
    if (!m_half) {
        painter.drawLine(QPointF(0, -height() / 2.0), QPointF(0, height() / 2.0));
    }
}

void RadarView::drawZones(QPainter &painter, double k) const {
    painter.setFont(QFont(painter.font().family(), 10));
    for (const ZoneDef &zone : m_model->zoneDefs()) {
        const double x1 = zone.xMin * k;
        const double x2 = zone.xMax * k;
        const double y1 = -zone.yMin * k; // flip Y: mm-forward is screen-up
        const double y2 = -zone.yMax * k;

        if (m_half && y2 > 0) continue; // fully outside the 180 deg FOV

        const QRectF box = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized();
        const bool occupied = m_model->isZoneOccupied(zone.name);

        if (occupied) {
            painter.setBrush(QColor(255, 50, 50, 60));
            painter.setPen(QPen(QColor(255, 100, 100), 1));
        } else {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(80, 80, 80), 1));
        }
        painter.drawRect(box);

        painter.setPen(occupied ? QColor(255, 255, 255) : QColor(150, 150, 150));
        painter.drawText(box, Qt::AlignCenter, zone.name);
    }
}

void RadarView::drawTrail(QPainter &painter, double k) const {
    const qint64 now = m_clock.elapsed();
    // Each track ID owns its own path, so a segment only ever connects two
    // positions that belonged to the same physical target over time.
    for (auto it = m_trackTrails.cbegin(); it != m_trackTrails.cend(); ++it) {
        const QVector<TrailPoint> &pts = it.value();
        const double factor = stationaryOpacityFactor(it.key(), now);
        for (int i = 1; i < pts.size(); ++i) {
            const TrailPoint &prev = pts[i - 1];
            const TrailPoint &curr = pts[i];
            const double age = double(now - prev.timestampMs) / double(kTrailMs);
            const int alpha = std::clamp(int((160 * (1.0 - age) + 20) * factor), 0, 160);
            painter.setPen(QPen(QColor(0, 180, 255, alpha), 2));

            const QPointF a(prev.mm.x() * k, -prev.mm.y() * k);
            const QPointF b(curr.mm.x() * k, -curr.mm.y() * k);
            if (targetInFov(a.x(), a.y()) || targetInFov(b.x(), b.y())) {
                painter.drawLine(a, b);
            }
        }
    }
}

void RadarView::drawTargets(QPainter &painter, double k) const {
    painter.setFont(QFont(painter.font().family(), 9));
    const qint64 now = m_clock.elapsed();
    const QVector<Target> targets = m_model->targets();
    const QVector<Track> tracks = m_model->tracks();

    for (int i = 0; i < targets.size(); ++i) {
        const Target &t = targets[i];
        if (t.y == 0 && t.x == 0) continue;
        const QPointF p(t.x * k, -t.y * k);
        if (!targetInFov(p.x(), p.y())) continue;

        // The raw "Targets: N" list carries no ID, so to decide whether
        // *this* dot is likely the same stale reflection a nearby track is
        // reporting, associate it with the closest known track by distance
        // (same idea the firmware itself uses to keep track IDs stable).
        double factor = 1.0;
        bool stationary = false;
        double bestDistSq = kTargetTrackAssociationMm * kTargetTrackAssociationMm;
        int bestId = -1;
        for (const Track &tr : tracks) {
            if (!tr.hasPosition) continue;
            const double dx = tr.x - t.x, dy = tr.y - t.y;
            const double distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) { bestDistSq = distSq; bestId = tr.id; }
        }
        if (bestId != -1) {
            stationary = isTrackStationary(bestId, now);
            factor = m_dimStationary && stationary ? kStationaryOpacity : 1.0;
        }

        painter.setPen(Qt::NoPen);
        QColor dot(0, 220, 255);
        dot.setAlphaF(factor);
        painter.setBrush(dot);
        painter.drawEllipse(p, 5, 5);

        QString label = QStringLiteral("#%1 x:%2 y:%3 d:%4mm a:%5° s:%6cm/s")
                             .arg(i)
                             .arg(t.x)
                             .arg(t.y)
                             .arg(t.distance, 0, 'f', 0)
                             .arg(t.angle, 0, 'f', 1)
                             .arg(t.speed, 0, 'f', 1);
        if (stationary && m_dimStationary) label += QStringLiteral(" (durağan?)");
        QColor textColor(220, 220, 220);
        textColor.setAlphaF(std::max(factor, 0.35)); // keep the "(durağan?)" hint legible
        painter.setPen(textColor);
        painter.drawText(p + QPointF(8, -8), label);
    }
}

void RadarView::drawTracks(QPainter &painter, double k) const {
    painter.setFont(QFont(painter.font().family(), 9, QFont::Bold));
    const qint64 now = m_clock.elapsed();
    for (const Track &t : m_model->tracks()) {
        if (!t.hasPosition) continue;
        const QPointF p(t.x * k, -t.y * k);
        if (!targetInFov(p.x(), p.y())) continue;

        const bool stationary = isTrackStationary(t.id, now);
        const double factor = stationaryOpacityFactor(t.id, now);
        QColor markerColor(255, 200, 0);
        markerColor.setAlphaF(factor);

        painter.setPen(QPen(markerColor, 2));
        painter.setBrush(Qt::NoBrush);
        const double s = 7;
        painter.drawLine(p + QPointF(-s, 0), p + QPointF(s, 0));
        painter.drawLine(p + QPointF(0, -s), p + QPointF(0, s));

        const QString zoneLabel = t.zone.isEmpty() ? QStringLiteral("-") : t.zone;
        QString text = QStringLiteral("T%1 [%2]").arg(t.id).arg(zoneLabel);
        if (stationary && m_dimStationary) text += QStringLiteral(" (durağan)");
        QColor textColor(255, 200, 0);
        textColor.setAlphaF(std::max(factor, 0.35));
        painter.setPen(textColor);
        painter.drawText(p + QPointF(10, 12), text);
    }
}
