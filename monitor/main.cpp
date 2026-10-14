#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("DS UDS CAN Monitor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    MainWindow window;
    window.show();
    return application.exec();
}
