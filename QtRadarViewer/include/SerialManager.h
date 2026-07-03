#pragma once

#include <QObject>
#include <QByteArray>
#include <QSerialPort>

// Owns the QSerialPort connection to the ESP32, splits the incoming byte
// stream into lines (device uses Serial.println -> \n terminated) and
// exposes a small command-writing API. Contains no protocol knowledge.
class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);

    bool isConnected() const;
    QString currentPortName() const { return m_port.portName(); }

public slots:
    void connectTo(const QString &portName, qint32 baudRate);
    void disconnectPort();
    void sendCommand(const QString &command);

signals:
    void lineReceived(const QString &line);
    void connected(const QString &portName, qint32 baudRate);
    void disconnected();
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onPortError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_port;
    QByteArray m_buffer;
};
