#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("RadarDesktop");
    QApplication::setOrganizationName("RD03D");

    MainWindow window;
    window.show();

    return app.exec();
}
