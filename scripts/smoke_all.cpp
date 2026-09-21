// macOS 全量点击冒烟：构造注册表中全部工具页，逐个点击页面上的所有按钮，
// 再做两轮主题切换重刷高亮。崩溃时最后一行日志即为触发点。
// 构建同 lingwtools-smoke（CMake -DBUILD_SMOKE=ON，target: lingwtools-smoke-all）
#include <QApplication>
#include <QWidget>
#include <QAbstractButton>
#include <QTimer>
#include <cstdio>
#include "registry.h"
#include "widgets/toolpage.h"
#include "widgets/highlighters.h"

static QApplication* g_app = nullptr;
static QWidget* g_host = nullptr;

// 会弹出模态文件对话框/无意义的按钮，跳过（否则冒烟会卡在模态循环）
static bool skipButton(const QString& t) {
    static const QList<QString> kws = {
        QStringLiteral("保存"), QStringLiteral("导出"), QStringLiteral("浏览"),
        QStringLiteral("选择文件"), QStringLiteral("选取文件"), QStringLiteral("打开文件")};
    for (const QString& k : kws)
        if (t.contains(k)) return true;
    return false;
}

static void runTool(int i) {
    const auto& tools = ToolRegistry::all();
    if (i >= tools.size()) {
        std::printf("smoke: theme switch dark->light\n"); std::fflush(stdout);
        hl::setDark(false);
        hl::rehighlightAll();
        g_app->processEvents(QEventLoop::AllEvents, 500);
        std::printf("smoke: theme switch light->dark\n"); std::fflush(stdout);
        hl::setDark(true);
        hl::rehighlightAll();
        g_app->processEvents(QEventLoop::AllEvents, 500);
        std::printf("smoke: ALL DONE ok\n"); std::fflush(stdout);
        QTimer::singleShot(300, g_app, &QApplication::quit);
        return;
    }
    std::printf("smoke[%d/%d] page=%s id=%s constructing\n", i + 1, int(tools.size()),
                qPrintable(tools[i].name), qPrintable(tools[i].id));
    std::fflush(stdout);
    ToolPage* page = tools[i].create();
    std::printf("smoke[%d] created\n", i + 1); std::fflush(stdout);
    page->setParent(g_host);
    page->show();
    g_app->processEvents(QEventLoop::AllEvents, 300);

    const auto btns = page->findChildren<QAbstractButton*>();
    std::printf("smoke[%d] clicking %d buttons\n", i + 1, int(btns.size())); std::fflush(stdout);
    for (QAbstractButton* b : btns) {
        if (b->text().isEmpty()) continue;           // 图标按钮（如 ⟳ 刷新）也点
        if (skipButton(b->text())) {
            std::printf("smoke[%d] skip: %s\n", i + 1, qPrintable(b->text())); std::fflush(stdout);
            continue;
        }
        std::printf("smoke[%d] click: %s\n", i + 1, qPrintable(b->text())); std::fflush(stdout);
        b->click();
        g_app->processEvents(QEventLoop::AllEvents, 400);
    }
    // 结束可能启动的后台任务（Ping/HTTP 等流式页）
    for (QAbstractButton* b : page->findChildren<QAbstractButton*>()) {
        if (b->text().contains(QStringLiteral("停止")) && b->isEnabled()) {
            std::printf("smoke[%d] stop: %s\n", i + 1, qPrintable(b->text())); std::fflush(stdout);
            b->click();
            g_app->processEvents(QEventLoop::AllEvents, 300);
        }
    }
    std::printf("smoke[%d] page done\n", i + 1); std::fflush(stdout);
    page->deleteLater();
    g_app->processEvents(QEventLoop::AllEvents, 100);
    QTimer::singleShot(30, g_host, [i] { runTool(i + 1); });
}

int main(int argc, char** argv) {
    std::printf("smoke: boot\n"); std::fflush(stdout);
    QApplication app(argc, argv);
    g_app = &app;
    g_host = new QWidget;
    g_host->resize(1280, 900);
    g_host->show();
    std::printf("smoke: %d tools registered\n", int(ToolRegistry::all().size())); std::fflush(stdout);
    QTimer::singleShot(200, g_host, [] { runTool(0); });
    const int rc = QApplication::exec();
    std::printf("smoke: exit rc=%d\n", rc); std::fflush(stdout);
    return rc;
}
