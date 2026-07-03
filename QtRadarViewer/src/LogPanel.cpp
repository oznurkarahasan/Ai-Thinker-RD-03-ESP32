#include "LogPanel.h"

#include <QPlainTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QScrollBar>

LogPanel::LogPanel(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QHBoxLayout();
    m_filterCombo = new QComboBox();
    m_filterCombo->addItem(tr("Tümü (ham)"));
    m_filterCombo->addItem(tr("Sadece olaylar (bölge/takip/ayar)"));
    m_autoScrollCheck = new QCheckBox(tr("Otomatik kaydır"));
    m_autoScrollCheck->setChecked(true);
    auto *clearBtn = new QPushButton(tr("Temizle"));

    toolbar->addWidget(new QLabel(tr("Filtre:")));
    toolbar->addWidget(m_filterCombo);
    toolbar->addStretch(1);
    toolbar->addWidget(m_autoScrollCheck);
    toolbar->addWidget(clearBtn);
    root->addLayout(toolbar);

    m_view = new QPlainTextEdit();
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(kMaxDisplayedBlocks);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_view->setFont(mono);
    root->addWidget(m_view, 1);

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(100);
    connect(m_flushTimer, &QTimer::timeout, this, &LogPanel::flushPending);
    m_flushTimer->start();

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogPanel::onFilterChanged);
    connect(clearBtn, &QPushButton::clicked, this, &LogPanel::clear);
}

void LogPanel::appendRawLine(const QString &line) {
    m_history.append(line);
    while (m_history.size() > kMaxHistoryLines) m_history.removeFirst();
    m_pending.append(line);
}

void LogPanel::flushPending() {
    if (m_pending.isEmpty()) return;

    QStringList toAppend;
    toAppend.reserve(m_pending.size());
    for (const QString &line : m_pending) {
        if (passesCurrentFilter(line)) toAppend << line;
    }
    m_pending.clear();
    if (toAppend.isEmpty()) return;

    m_view->appendPlainText(toAppend.join('\n'));
    if (m_autoScrollCheck->isChecked()) {
        QScrollBar *sb = m_view->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

void LogPanel::onFilterChanged(int) {
    rerenderFromHistory();
}

void LogPanel::clear() {
    m_history.clear();
    m_pending.clear();
    m_view->clear();
}

void LogPanel::rerenderFromHistory() {
    QStringList filtered;
    filtered.reserve(m_history.size());
    for (const QString &line : m_history) {
        if (passesCurrentFilter(line)) filtered << line;
    }
    m_view->setPlainText(filtered.join('\n'));
    QScrollBar *sb = m_view->verticalScrollBar();
    sb->setValue(sb->maximum());
}

bool LogPanel::passesCurrentFilter(const QString &line) const {
    if (m_filterCombo->currentIndex() == 0) return true; // raw: everything
    return !isVerboseNoise(line);
}

bool LogPanel::isVerboseNoise(const QString &line) {
    // Per-point/per-frame debug traces that are only useful when actively
    // debugging zone/deadband logic; hidden from the curated "events" view
    // but always present in the raw view.
    static const QRegularExpression patterns[] = {
        QRegularExpression(R"(^\[\d+\]$)"),
        QRegularExpression(R"(^X \(mm\):)"),
        QRegularExpression(R"(^Y \(mm\):)"),
        QRegularExpression(R"(^Distance \(mm\):)"),
        QRegularExpression(R"(^Angle \(degrees\):)"),
        QRegularExpression(R"(^Speed \(cm/s\):)"),
        QRegularExpression(R"(^-{5,}$)"),
        QRegularExpression(R"(^Zone check for point)"),
        QRegularExpression(R"(^Point in zone:)"),
        QRegularExpression(R"(^Point \(.*\) in deadband of)"),
        QRegularExpression(R"(^No exact zone match)"),
        QRegularExpression(R"(^Track \d+ movement:)"),
        QRegularExpression(R"(^Track \d+ current_zone:)"),
        QRegularExpression(R"(^Track \d+ zone change:)"),
        QRegularExpression(R"(^Track \d+ in deadband of)"),
        QRegularExpression(R"(^Track \d+ overlaps)"),
        QRegularExpression(R"(^Track \d+ is in )"),
    };
    for (const QRegularExpression &re : patterns) {
        if (re.match(line).hasMatch()) return true;
    }
    return false;
}
