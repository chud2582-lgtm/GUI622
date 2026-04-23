#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置全局字体
    QFont font("Microsoft YaHei", 9);
    app.setFont(font);
    
    MainWindow w;
    w.show();
    
    return app.exec();
}
