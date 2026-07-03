#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QVector>

#include "RadarTypes.h"

// Owns the QSerialPort connection to the ESP32 and turns newline-delimited JSON frames
// into typed Qt signals. All I/O is event-driven (readyRead) — nothing here blocks the
// caller's thread, so it is safe to use directly from the GUI thread.
class RadarComm : public QObject {
    Q_OBJECT

public:
    explicit RadarComm(QObject *parent = nullptr);
    ~RadarComm() override;

    bool connectToPort(const QString &portName, qint32 baudRate = 115200);
    void disconnectPort();
    bool isConnected() const;

    // Fire-and-forget text command (e.g. "DEBUG", "MULTI", "EMA", "ZONES", "CFG", "HELP").
    void sendCommand(const QString &cmd);

signals:
    void configReceived(const RadarConfig &config);
    void targetsUpdated(const QVector<TargetState> &targets, quint32 zoneOccupiedMask, quint32 seq);
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void processLine(const QByteArray &line);

    QSerialPort *m_serial;
    QByteArray m_buffer;
};
