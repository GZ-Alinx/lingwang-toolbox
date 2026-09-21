// macOS 闪退排查冒烟测试：构造 K8s 命令生成器页面并跑事件循环
// 用法：lingwtools-smoke [页面序号]
//   0 = K8s 命令生成器（默认，用户报告闪退页面）
//   1 = K8s YAML 模板（对照）
// 构造：cmake -DBUILD_SMOKE=ON
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <cstdio>
#include "tools/pages.h"
#include "widgets/toolpage.h"

int main(int argc, char** argv) {
    std::printf("smoke: boot\n"); std::fflush(stdout);
    QApplication app(argc, argv);
    std::printf("smoke: QApplication ok\n"); std::fflush(stdout);

    const int which = argc > 1 ? std::atoi(argv[1]) : 0;
    QWidget host;
    ToolPage* page = nullptr;
    if (which == 0) {
        std::printf("smoke: creating K8sCmdPage...\n"); std::fflush(stdout);
        page = pages::createK8sCmd();
        std::printf("smoke: K8sCmdPage created\n"); std::fflush(stdout);
    } else {
        std::printf("smoke: creating K8sYamlPage...\n"); std::fflush(stdout);
        page = pages::createK8sYaml();
        std::printf("smoke: K8sYamlPage created\n"); std::fflush(stdout);
    }
    page->setParent(&host);
    host.resize(1200, 860);
    host.show();
    std::printf("smoke: page shown, entering event loop\n"); std::fflush(stdout);

    // 事件循环期间模拟用户操作：切换几次场景下拉（触发 buildForm 重建）
    QTimer::singleShot(600, &app, [page] {
        std::printf("smoke: event loop alive\n"); std::fflush(stdout);
        (void)page;
    });
    QTimer::singleShot(1500, &app, [&app, page] {
        std::printf("smoke: deleting page\n"); std::fflush(stdout);
        page->deleteLater();
        QTimer::singleShot(600, &app, &QCoreApplication::quit);
    });
    const int rc = QApplication::exec();
    std::printf("smoke: done rc=%d\n", rc); std::fflush(stdout);
    return rc;
}
