#pragma once

#include <QWidget>
#include <QStringList>

class QPlainTextEdit;
class QComboBox;
class QCheckBox;
class QTimer;

// Footer message log. Every line the device sends arrives here via
// appendRawLine(), regardless of whether RadarProtocol understood it -
// nothing the hardware says is ever thrown away. To stay smooth even when
// DEBUG mode is producing hundreds of lines/second, incoming lines are
// batched and flushed to the widget on a timer instead of one UI update
// per line, and only a bounded amount of history is kept in memory.
class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget *parent = nullptr);

public slots:
    void appendRawLine(const QString &line);
    void clear();

private slots:
    void flushPending();
    void onFilterChanged(int index);

private:
    static bool isVerboseNoise(const QString &line);
    bool passesCurrentFilter(const QString &line) const;
    void rerenderFromHistory();

    QPlainTextEdit *m_view;
    QComboBox *m_filterCombo;
    QCheckBox *m_autoScrollCheck;

    QStringList m_pending;
    QStringList m_history; // bounded ring buffer of raw lines, for filter switches
    QTimer *m_flushTimer;

    static constexpr int kMaxHistoryLines = 5000;
    static constexpr int kMaxDisplayedBlocks = 5000;
};
