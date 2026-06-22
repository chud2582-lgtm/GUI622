#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 测试改动
    QFont font("Microsoft YaHei", 9);
    app.setFont(font);
    
    MainWindow w;
    w.show();
    
    return app.exec();
}
