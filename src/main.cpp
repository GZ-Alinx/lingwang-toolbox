#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFile>
#include <QSettings>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("LingWang"));
    QApplication::setApplicationName(QStringLiteral("LingWangToolbox"));
    QApplication::setApplicationDisplayName(QStringLiteral("灵王工具箱"));
    QApplication::setApplicationVersion(QStringLiteral("1.3.2"));

    // 界面字体
    QFont uiFont(QStringLiteral("Microsoft YaHei UI"), 10);
#ifdef Q_OS_MAC
    uiFont = QFont(QStringLiteral("PingFang SC"), 13);
#endif
    if (!QFontDatabase::families().contains(uiFont.family()))
        uiFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    app.setFont(uiFont);

    MainWindow win;
    win.show();
    return QApplication::exec();
}
