#include "RadarComm.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSerialPortInfo>

RadarComm::RadarComm(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead, this, &RadarComm::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &RadarComm::onSerialError);
    m_buffer.reserve(512);
}

RadarComm::~RadarComm()
{
    disconnectPort();
}

bool RadarComm::connectToPort(const QString &portName, qint32 baudRate)
{
    if (m_serial->isOpen())
        disconnectPort();

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open %1: %2").arg(portName, m_serial->errorString()));
        return false;
    }

    m_buffer.clear();
    emit connectionStateChanged(true);
    return true;
}

void RadarComm::disconnectPort()
{
    if (!m_serial->isOpen())
        return;
    m_serial->close();
    m_buffer.clear();
    emit connectionStateChanged(false);
}

bool RadarComm::isConnected() const
{
    return m_serial->isOpen();
}

void RadarComm::sendCommand(const QString &cmd)
{
    if (!m_serial->isOpen())
        return;
    m_serial->write(cmd.toUtf8());
    m_serial->write("\n");
}

void RadarComm::onReadyRead()
{
    m_buffer.append(m_serial->readAll());

    // Split on '\n'; keep any trailing partial line buffered for the next readyRead.
    int nl;
    while ((nl = m_buffer.indexOf('\n')) != -1) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        if (!line.isEmpty())
            processLine(line);
    }

    // Guard against a runaway non-JSON producer (e.g. a firmware still in text-log mode)
    // filling the buffer forever with no newline.
    if (m_buffer.size() > 4096)
        m_buffer.clear();
}

void RadarComm::processLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return; // Silently ignore stray human-readable debug lines, boot banners, etc.

    const QJsonObject obj = doc.object();
    const QString type = obj.value(QLatin1String("type")).toString();

    if (type == QLatin1String("cfg")) {
        RadarConfig cfg;
        cfg.tileSizeMm = static_cast<float>(obj.value(QLatin1String("tile")).toDouble(1000.0));
        cfg.gridWidth = static_cast<quint8>(obj.value(QLatin1String("gw")).toInt(4));
        cfg.gridHeight = static_cast<quint8>(obj.value(QLatin1String("gh")).toInt(4));

        const QJsonArray zonesArr = obj.value(QLatin1String("zones")).toArray();
        cfg.zoneNames.clear();
        cfg.zoneNames.reserve(zonesArr.size());
        for (const QJsonValue &v : zonesArr)
            cfg.zoneNames.append(v.toString());

        emit configReceived(cfg);
        return;
    }

    if (type == QLatin1String("trk")) {
        const QJsonArray arr = obj.value(QLatin1String("t")).toArray();
        QVector<TargetState> targets;
        targets.reserve(arr.size());

        for (const QJsonValue &v : arr) {
            const QJsonObject to = v.toObject();
            TargetState ts;
            ts.id = static_cast<quint8>(to.value(QLatin1String("id")).toInt());
            ts.x = static_cast<float>(to.value(QLatin1String("x")).toDouble());
            ts.y = static_cast<float>(to.value(QLatin1String("y")).toDouble());
            ts.speed = static_cast<float>(to.value(QLatin1String("v")).toDouble());
            ts.zone = static_cast<quint8>(to.value(QLatin1String("z")).toInt(0xFF));
            targets.append(ts);
        }

        const quint32 zocc = static_cast<quint32>(obj.value(QLatin1String("zocc")).toInt(0));
        const quint32 seq = static_cast<quint32>(obj.value(QLatin1String("seq")).toInt(0));
        emit targetsUpdated(targets, zocc, seq);
        return;
    }
}

void RadarComm::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;
    emit errorOccurred(m_serial->errorString());
    if (error == QSerialPort::ResourceError) // device unplugged
        disconnectPort();
}
