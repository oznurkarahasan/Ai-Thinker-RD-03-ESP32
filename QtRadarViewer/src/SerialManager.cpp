#include "SerialManager.h"

SerialManager::SerialManager(QObject *parent) : QObject(parent) {
    connect(&m_port, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialManager::onPortError);
}

bool SerialManager::isConnected() const {
    return m_port.isOpen();
}

void SerialManager::connectTo(const QString &portName, qint32 baudRate) {
    if (m_port.isOpen()) {
        disconnectPort();
    }

    m_buffer.clear();
    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Port açılamadı: %1").arg(m_port.errorString()));
        return;
    }

    emit connected(portName, baudRate);
}

void SerialManager::disconnectPort() {
    if (!m_port.isOpen()) return;
    m_port.close();
    m_buffer.clear();
    emit disconnected();
}

void SerialManager::sendCommand(const QString &command) {
    if (!m_port.isOpen()) {
        emit errorOccurred(tr("Komut gönderilemedi: bağlı değil"));
        return;
    }
    const QByteArray data = (command + QLatin1Char('\n')).toUtf8();
    m_port.write(data);
}

void SerialManager::onReadyRead() {
    m_buffer.append(m_port.readAll());

    int newlineIdx;
    while ((newlineIdx = m_buffer.indexOf('\n')) != -1) {
        QByteArray lineBytes = m_buffer.left(newlineIdx);
        m_buffer.remove(0, newlineIdx + 1);
        if (!lineBytes.isEmpty() && lineBytes.endsWith('\r')) {
            lineBytes.chop(1);
        }
        emit lineReceived(QString::fromUtf8(lineBytes));
    }

    // Guard against a runaway device that never sends '\n' (garbage/noise
    // at the wrong baud rate) filling memory indefinitely.
    constexpr int kMaxPendingBytes = 1 << 16;
    if (m_buffer.size() > kMaxPendingBytes) {
        m_buffer.clear();
    }
}

void SerialManager::onPortError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;

    const QString message = m_port.errorString();
    if (error == QSerialPort::ResourceError && m_port.isOpen()) {
        // Typically means the device was unplugged.
        m_port.close();
        m_buffer.clear();
        emit errorOccurred(tr("Bağlantı kesildi: %1").arg(message));
        emit disconnected();
        return;
    }
    emit errorOccurred(message);
}
