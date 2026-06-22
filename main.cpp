#include <QApplication>
#include <QFont>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("打磨机器人控制软件"));

    QFont font(QStringLiteral("Microsoft YaHei"), 9);
    app.setFont(font);

    MainWindow w;
    w.show();

    return app.exec();
}
