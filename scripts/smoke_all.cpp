// macOS 全量点击冒烟：构造注册表中全部工具页，逐个点击页面上的所有按钮，
// 再做两轮主题切换重刷高亮。崩溃时最后一行日志即为触发点。
// 构建同 lingwtools-smoke（CMake -DBUILD_SMOKE=ON，target: lingwtools-smoke-all）
#include <QApplication>
#include <QWidget>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QEventLoop>
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
        QStringLiteral("选择文件"), QStringLiteral("选取文件"), QStringLiteral("打开文件"),
        QStringLiteral("一键安装"),   // 会真实下载 kubectl（约 60MB），冒烟不点
        QStringLiteral("文件")};   // macOS 原生文件对话框不进 Qt 模态体系，无法自动关闭，全部跳过
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
    g_app->processEvents(QEventLoop::AllEvents, 120);

    // ---- K8s 命令生成器专项：切换集群上下文，观察 ns 列表自动刷新（含失败/超时路径） ----
    if (tools[i].id == QLatin1String("k8scmd")) {
        auto* ctx = page->findChild<QComboBox*>(QStringLiteral("k8sCtx"));
        auto* nsBox = page->findChild<QComboBox*>(QStringLiteral("k8sNs"));
        if (ctx && nsBox && ctx->count() >= 2) {
            const QString prev = ctx->currentText();
            const int target = ctx->count() - 1;   // 最后一项 context
            std::printf("smoke[k8s] contexts=%d; switch '%s' -> '%s', watch ns items\n",
                        ctx->count(), qPrintable(prev), qPrintable(ctx->itemText(target)));
            std::fflush(stdout);
            ctx->setCurrentIndex(target);
            // 注意：必须用真实事件循环等待（QEventLoop::exec），不能用 processEvents 批处理——
            // 嵌套 processEvents 不会派发 QProcess 退出通知（socket notifier 饥饿），
            // 会误判“命名空间未刷新”（真实应用走 exec() 主循环，不存在此问题）
            {
                QEventLoop loop;
                QTimer::singleShot(13500, &loop, &QEventLoop::quit);   // 覆盖 10s 超时
                for (int s = 0; s < 27; ++s) {
                    QTimer::singleShot((s + 1) * 500, &loop, [&loop, page, nsBox, s] {
                        auto* diagLbl = page->findChild<QLabel*>(QStringLiteral("k8sDiag"));
                        std::printf("smoke[k8s] t=%.1fs ns-items=%d ns=%s diag=%s\n", (s + 1) * 0.5,
                                    nsBox->count(), qPrintable(nsBox->currentText()),
                                    qPrintable(diagLbl ? diagLbl->text() : QStringLiteral("(null)")));
                        std::fflush(stdout);
                        if (s == 26) loop.quit();
                    });
                }
                loop.exec();
            }
            const int back = ctx->findText(prev);
            ctx->setCurrentIndex(back >= 0 ? back : 0);
            {
                QEventLoop loop;
                QTimer::singleShot(1500, &loop, &QEventLoop::quit);
                loop.exec();
            }
            std::printf("smoke[k8s] switched back; ns-items=%d ns=%s\n",
                        nsBox->count(), qPrintable(nsBox->currentText()));
            std::fflush(stdout);
            // 资源名称下拉填充专项：等防抖+拉取完成后，枚举表单内全部可编辑下拉
            // （ns 之外的可编辑下拉即资源名称选择器），列表项数 >0 说明拉到了真实数据
            {
                QEventLoop loop;
                QTimer::singleShot(4500, &loop, &QEventLoop::quit);
                loop.exec();
            }
            const auto combos = page->findChildren<QComboBox*>();
            for (QComboBox* c : combos) {
                if (!c->isEditable() || c == nsBox) continue;
                std::printf("smoke[k8s] respick items=%d cur='%s'\n",
                            c->count(), qPrintable(c->currentText()));
                std::fflush(stdout);
            }
        } else {
            std::printf("smoke[k8s] skip ctx probe (contexts=%d)\n", ctx ? ctx->count() : -1);
            std::fflush(stdout);
        }
    }

    const auto btns = page->findChildren<QAbstractButton*>();
    std::printf("smoke[%d] clicking %d buttons\n", i + 1, int(btns.size())); std::fflush(stdout);
    for (QAbstractButton* b : btns) {
        if (!b->isEnabled()) continue;
        if (!b->text().isEmpty() && skipButton(b->text())) {
            std::printf("smoke[%d] skip: %s\n", i + 1, qPrintable(b->text())); std::fflush(stdout);
            continue;
        }
        std::printf("smoke[%d] click: %s\n", i + 1,
                    qPrintable(b->text().isEmpty() ? QStringLiteral("(icon)") : b->text()));
        std::fflush(stdout);
        b->click();
        g_app->processEvents(QEventLoop::AllEvents, 150);
    }
    // 结束可能启动的后台任务（Ping/HTTP 等流式页）
    for (QAbstractButton* b : page->findChildren<QAbstractButton*>()) {
        if (b->text().contains(QStringLiteral("停止")) && b->isEnabled()) {
            std::printf("smoke[%d] stop: %s\n", i + 1, qPrintable(b->text())); std::fflush(stdout);
            b->click();
            g_app->processEvents(QEventLoop::AllEvents, 120);
        }
    }
    std::printf("smoke[%d] page done\n", i + 1); std::fflush(stdout);
    page->deleteLater();
    g_app->processEvents(QEventLoop::AllEvents, 100);
    QTimer::singleShot(30, g_host, [i] { runTool(i + 1); });
}

// 模态驱逐：文件选择/消息框等模态弹窗出现后自动关闭（QDialog::exec 的事件循环里 timer 仍触发）
static void installModalKiller() {
    auto* t = new QTimer(g_app);
    QObject::connect(t, &QTimer::timeout, g_app, [] {
        if (QWidget* m = QApplication::activeModalWidget())
            m->close();
    });
    t->start(300);
}

int main(int argc, char** argv) {
    std::printf("smoke: boot\n"); std::fflush(stdout);
    QApplication app(argc, argv);
    g_app = &app;
    installModalKiller();
    g_host = new QWidget;
    g_host->resize(1280, 900);
    g_host->show();
    // SMOKE_ONLY=<工具id>：跳过其他页面，直接从指定工具开始（K8s 联动问题快速定位用）
    int start = 0;
    const QString only = qEnvironmentVariable("SMOKE_ONLY");
    if (!only.isEmpty()) {
        const auto& tools = ToolRegistry::all();
        for (int i = 0; i < int(tools.size()); ++i)
            if (tools[i].id == only) { start = i; break; }
    }
    std::printf("smoke: %d tools registered, start=%d\n", int(ToolRegistry::all().size()), start); std::fflush(stdout);
    QTimer::singleShot(200, g_host, [start] { runTool(start); });
    const int rc = QApplication::exec();
    std::printf("smoke: exit rc=%d\n", rc); std::fflush(stdout);
    return rc;
}
