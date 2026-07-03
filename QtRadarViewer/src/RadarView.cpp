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

    connect(m_model, &RadarModel::targetsChanged, this, &RadarView::onTargetsChanged);
    connect(m_model, &RadarModel::zonesChanged, this, [this] { update(); });
    connect(m_model, &RadarModel::tracksChanged, this, [this] { update(); });

    // Decouple repaint cadence from the (potentially bursty) serial data
    // rate: repaint at a steady ~30 fps instead of once per model signal.
    auto *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, QOverload<>::of(&RadarView::update));
    repaintTimer->start(33);
}

void RadarView::setHalfFov(bool half) { m_half = half; }
void RadarView::setScaleMmPerPixel(double mmPerPx) { m_mmPerPx = std::max(1.0, mmPerPx); }
void RadarView::setShowTargets(bool show) { m_showTargets = show; }
void RadarView::setShowTrail(bool show) {
    m_showTrail = show;
    if (!show) m_trail.clear();
}
void RadarView::setShowTracks(bool show) { m_showTracks = show; }

void RadarView::onTargetsChanged() {
    if (!m_showTrail) return;
    const QVector<Target> targets = m_model->targets();
    if (targets.isEmpty()) return;

    TrailFrame frame;
    frame.timestampMs = m_clock.elapsed();
    frame.pointsMm.reserve(targets.size());
    for (const Target &t : targets) frame.pointsMm.append(QPointF(t.x, t.y));
    m_trail.append(frame);

    const qint64 now = frame.timestampMs;
    while (!m_trail.isEmpty() && (now - m_trail.first().timestampMs) > kTrailMs) {
        m_trail.removeFirst();
    }
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
    if (m_showTrail) drawTrail(painter, k);
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
    for (int i = 1; i < m_trail.size(); ++i) {
        const TrailFrame &prev = m_trail[i - 1];
        const TrailFrame &curr = m_trail[i];
        const int n = std::min(prev.pointsMm.size(), curr.pointsMm.size());
        const double age = double(now - prev.timestampMs) / double(kTrailMs);
        const int alpha = std::clamp(int(160 * (1.0 - age) + 20), 0, 160);
        painter.setPen(QPen(QColor(0, 180, 255, alpha), 2));

        for (int j = 0; j < n; ++j) {
            const QPointF a(prev.pointsMm[j].x() * k, -prev.pointsMm[j].y() * k);
            const QPointF b(curr.pointsMm[j].x() * k, -curr.pointsMm[j].y() * k);
            if (targetInFov(a.x(), a.y()) || targetInFov(b.x(), b.y())) {
                painter.drawLine(a, b);
            }
        }
    }
}

void RadarView::drawTargets(QPainter &painter, double k) const {
    painter.setFont(QFont(painter.font().family(), 9));
    const QVector<Target> targets = m_model->targets();
    for (int i = 0; i < targets.size(); ++i) {
        const Target &t = targets[i];
        if (t.y == 0 && t.x == 0) continue;
        const QPointF p(t.x * k, -t.y * k);
        if (!targetInFov(p.x(), p.y())) continue;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 220, 255));
        painter.drawEllipse(p, 5, 5);

        const QString label = QStringLiteral("#%1 x:%2 y:%3 d:%4mm a:%5° s:%6cm/s")
                                   .arg(i)
                                   .arg(t.x)
                                   .arg(t.y)
                                   .arg(t.distance, 0, 'f', 0)
                                   .arg(t.angle, 0, 'f', 1)
                                   .arg(t.speed, 0, 'f', 1);
        painter.setPen(QColor(220, 220, 220));
        painter.drawText(p + QPointF(8, -8), label);
    }
}

void RadarView::drawTracks(QPainter &painter, double k) const {
    painter.setFont(QFont(painter.font().family(), 9, QFont::Bold));
    for (const Track &t : m_model->tracks()) {
        if (!t.hasPosition) continue;
        const QPointF p(t.x * k, -t.y * k);
        if (!targetInFov(p.x(), p.y())) continue;

        painter.setPen(QPen(QColor(255, 200, 0), 2));
        painter.setBrush(Qt::NoBrush);
        const double s = 7;
        painter.drawLine(p + QPointF(-s, 0), p + QPointF(s, 0));
        painter.drawLine(p + QPointF(0, -s), p + QPointF(0, s));

        const QString zoneLabel = t.zone.isEmpty() ? QStringLiteral("-") : t.zone;
        painter.setPen(QColor(255, 200, 0));
        painter.drawText(p + QPointF(10, 12), QStringLiteral("T%1 [%2]").arg(t.id).arg(zoneLabel));
    }
}
