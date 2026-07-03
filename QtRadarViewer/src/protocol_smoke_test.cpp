// Throwaway verification tool: feeds a real captured ESP32 serial log
// through RadarProtocol + RadarModel and prints a summary, so the parser
// can be checked against real hardware output instead of just compiling.
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

#include "RadarProtocol.h"
#include "RadarModel.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qWarning() << "usage: protocol_smoke_test <log file>";
        return 1;
    }

    QFile f(argv[1]);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "cannot open" << argv[1];
        return 1;
    }

    RadarProtocol protocol;
    RadarModel model;
    QObject::connect(&protocol, &RadarProtocol::targetsUpdated, &model, &RadarModel::onTargetsUpdated);
    QObject::connect(&protocol, &RadarProtocol::zoneStatusResetRequested, &model, &RadarModel::onZoneStatusResetRequested);
    QObject::connect(&protocol, &RadarProtocol::zoneOccupancyChanged, &model, &RadarModel::onZoneOccupancyChanged);
    QObject::connect(&protocol, &RadarProtocol::activeTrackCountUpdated, &model, &RadarModel::onActiveTrackCountUpdated);
    QObject::connect(&protocol, &RadarProtocol::zoneDefinitionUpdated, &model, &RadarModel::onZoneDefinitionUpdated);
    QObject::connect(&protocol, &RadarProtocol::trackCreated, &model, &RadarModel::onTrackCreated);
    QObject::connect(&protocol, &RadarProtocol::trackLost, &model, &RadarModel::onTrackLost);
    QObject::connect(&protocol, &RadarProtocol::trackPositionUpdated, &model, &RadarModel::onTrackPositionUpdated);
    QObject::connect(&protocol, &RadarProtocol::deviceSettingChanged, &model, &RadarModel::onDeviceSettingChanged);
    QObject::connect(&protocol, &RadarProtocol::presenceChanged, &model, &RadarModel::onPresenceChanged);
    QObject::connect(&protocol, &RadarProtocol::motionChanged, &model, &RadarModel::onMotionChanged);
    QObject::connect(&protocol, &RadarProtocol::helpTextReceived, &model, &RadarModel::onHelpTextReceived);

    int rawLineCount = 0, targetBlockCount = 0, trackCreatedCount = 0, trackLostCount = 0;
    QObject::connect(&protocol, &RadarProtocol::rawLine, [&](const QString &) { rawLineCount++; });
    QObject::connect(&protocol, &RadarProtocol::targetsUpdated, [&](const QVector<Target> &) { targetBlockCount++; });
    QObject::connect(&protocol, &RadarProtocol::trackCreated, [&](int, double, double) { trackCreatedCount++; });
    QObject::connect(&protocol, &RadarProtocol::trackLost, [&](int) { trackLostCount++; });

    QTextStream in(&f);
    static const QRegularExpression ansi(R"(\x1b(\[[0-9;?]*[a-zA-Z]|[\(\)][A-Za-z0-9]))");
    while (!in.atEnd()) {
        QString line = in.readLine();
        line.remove(ansi);
        protocol.feedLine(line);
    }

    qInfo() << "raw lines seen:" << rawLineCount;
    qInfo() << "target blocks parsed:" << targetBlockCount;
    qInfo() << "tracks created:" << trackCreatedCount << "lost:" << trackLostCount;
    qInfo() << "final target count:" << model.targets().size();
    for (const Target &t : model.targets()) {
        qInfo() << "  target" << t.index << "x=" << t.x << "y=" << t.y << "d=" << t.distance << "a=" << t.angle << "s=" << t.speed;
    }
    qInfo() << "final active track count:" << model.activeTrackCount();
    qInfo() << "final tracks:" << model.tracks().size();
    for (const Track &t : model.tracks()) {
        qInfo() << "  track" << t.id << "x=" << t.x << "y=" << t.y << "zone=" << t.zone << "hasPos=" << t.hasPosition;
    }
    int occupied = 0;
    for (const ZoneDef &z : model.zoneDefs()) {
        if (model.isZoneOccupied(z.name)) { occupied++; qInfo() << "  occupied zone:" << z.name; }
    }
    qInfo() << "zone def count:" << model.zoneDefs().size() << "occupied count:" << occupied;
    qInfo() << "DEBUG=" << int(model.debugRawState()) << "MULTI=" << int(model.multiTargetState()) << "EMA=" << int(model.emaState());

    return 0;
}
