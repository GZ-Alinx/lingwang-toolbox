// K8s 命令生成器专项冒烟：offscreen 实例化页面，观察
//   1) kubectl 探测与连接状态条（k8sDiag 原文）
//   2) context 下拉是否载入 kubeconfig
//   3) 选择可达 context 后命名空间下拉是否填充真实列表
//   4) 资源名称选择器（可编辑下拉）是否填充
// 用法：QT_QPA_PLATFORM=offscreen ./lingwtools-smoke-k8s [context名]
// 构建：CMake -DBUILD_SMOKE=ON，target: lingwtools-smoke-k8s
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include "tools/pages.h"
#include "widgets/toolpage.h"

static void dump(QWidget* page, const char* tag) {
    auto* ctx = page->findChild<QComboBox*>(QStringLiteral("k8sCtx"));
    auto* ns = page->findChild<QComboBox*>(QStringLiteral("k8sNs"));
    auto* diag = page->findChild<QLabel*>(QStringLiteral("k8sDiag"));
    std::printf("[%s] ctx-items=%d cur='%s'\n", tag, ctx->count(), qPrintable(ctx->currentText()));
    std::printf("[%s] ns-items=%d cur='%s'\n", tag, ns->count(), qPrintable(ns->currentText()));
    for (int i = 0; i < ns->count(); ++i)
        std::printf("[%s]   ns[%d]=%s\n", tag, i, qPrintable(ns->itemText(i)));
    std::printf("[%s] diag=%s\n", tag, qPrintable(diag ? diag->text() : QStringLiteral("(null)")));
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString wantCtx = argc > 1 ? QString::fromUtf8(argv[1]) : QString();
    const bool fast = argc > 2;   // fast[=毫秒]：构造后很快切换（复现 smoke_all 时序）
    const int tSwitch = fast ? (qstrcmp(argv[2], "fast") == 0 ? 500 : atoi(argv[2])) : 4000;
    // HOST=1：复现 smoke_all 的宿主窗口 + setParent 托管方式
    QWidget* page = pages::createK8sCmd();
    if (!qEnvironmentVariable("HOST").isEmpty()) {
        auto* host = new QWidget;
        host->resize(1280, 900);
        host->show();
        page->setParent(host);
    }
    page->show();

    if (!qEnvironmentVariable("EVENTS").isEmpty()) {
        // EVENTS=1：用嵌套 processEvents 等待（复现 smoke_all 的等待方式，对照用）
        QTimer::singleShot(tSwitch, &app, [&app, page, wantCtx, tSwitch] {
            dump(page, "events-pre-switch");
            auto* ctx = page->findChild<QComboBox*>(QStringLiteral("k8sCtx"));
            ctx->setCurrentIndex(ctx->findText(wantCtx));
            for (int s = 0; s < 26; ++s) app.processEvents(QEventLoop::AllEvents, 500);
            dump(page, "events-after-13s");
            std::printf("K8S SMOKE DONE\n"); std::fflush(stdout);
            app.quit();
        });
        return app.exec();
    }
    QTimer::singleShot(tSwitch, &app, [&app, page, wantCtx, fast] {
        dump(page, fast ? "pre-switch(fast)" : "startup");
        auto* ctx = page->findChild<QComboBox*>(QStringLiteral("k8sCtx"));
        const int idx = wantCtx.isEmpty() ? ctx->count() - 1 : ctx->findText(wantCtx);
        if (idx < 0) {
            std::printf("context '%s' not found; keep current\n", qPrintable(wantCtx));
        } else {
            std::printf("switch ctx -> '%s'\n", qPrintable(ctx->itemText(idx)));
            ctx->setCurrentIndex(idx);
        }
        std::fflush(stdout);
    });
    QTimer::singleShot(tSwitch + 3000, &app, [&app, page] { dump(page, "after+3s"); });
    QTimer::singleShot(tSwitch + 8000, &app, [&app, page] { dump(page, "after+8s"); });
    QTimer::singleShot(tSwitch + 13000, &app, [&app, page] {
        dump(page, "after+13s");
        const auto combos = page->findChildren<QComboBox*>();
        for (QComboBox* c : combos) {
            if (c->isEditable() && c->objectName() != QLatin1String("k8sNs")) {
                std::printf("[respick] items=%d cur='%s' placeholder='%s'\n",
                            c->count(), qPrintable(c->currentText()),
                            qPrintable(c->placeholderText()));
                for (int i = 0; i < c->count() && i < 5; ++i)
                    std::printf("[respick]   %s\n", qPrintable(c->itemText(i)));
            }
        }
        // 行内提示条（拉取中/空资源/失败原因）
        const auto hints = page->findChildren<QLabel*>(QStringLiteral("resPickHint"));
        std::printf("[hints] %d 个\n", int(hints.size()));
        for (QLabel* h : hints)
            std::printf("[hint] visible=%s text='%s'\n",
                        h->isVisible() ? "yes" : "no", qPrintable(h->text()));
        // ---- 执行控制台验证：点「执行」→ 观察输出 → 停止 ----
        QPushButton* execB = nullptr;
        const auto btns = page->findChildren<QPushButton*>();
        for (QPushButton* b : btns)
            if (b->text().contains(QStringLiteral("执行"))) { execB = b; break; }
        if (execB && execB->isEnabled()) {
            std::printf("[exec] click 执行: '%s'\n", qPrintable(execB->text()));
            execB->click();
            {
                QEventLoop loop;
                QTimer::singleShot(6000, &loop, &QEventLoop::quit);
                loop.exec();
            }
            // m_out 先创建、m_execOut 后创建：第二个 mono 文本框即执行输出
            const auto edits = page->findChildren<QPlainTextEdit*>();
            if (edits.size() >= 2) {
                const QString out = edits.at(1)->toPlainText();
                std::printf("[exec] output %d chars:\n", int(out.size()));
                for (const QString& ln : out.split(QLatin1Char('\n')))
                    if (!ln.trimmed().isEmpty()) std::printf("[exec]   %s\n", qPrintable(ln.left(90)));
            }
            for (QPushButton* b : page->findChildren<QPushButton*>())
                if (b->text().contains(QStringLiteral("停止")) && b->isEnabled()) {
                    std::printf("[exec] click 停止\n");
                    b->click();
                    break;
                }
            {
                QEventLoop loop;
                QTimer::singleShot(800, &loop, &QEventLoop::quit);
                loop.exec();
            }
        } else {
            std::printf("[exec] skip (button not found/disabled)\n");
        }
        std::printf("K8S SMOKE DONE\n");
        std::fflush(stdout);
        app.quit();
    });
    return app.exec();
}
