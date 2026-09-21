// YAML 模板页探针：
// 1) 逐个模板用 yaml-cpp 校验 YAML 解析合法
// 2) 检查模板正文无流式 {} [] 写法（nginx.conf 等块标量文件内容除外）
// 3) 抽查高亮：每个模板前几个非空行有 FormatRange
#include <QApplication>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QTextBlock>
#include <QTextLayout>
#include <QRegularExpression>
#include <QTimer>
#include <cstdio>
#include <yaml-cpp/yaml.h>
#include "tools/pages.h"
#include "widgets/toolpage.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget* page = pages::createK8sYaml();
    page->show();
    QTimer::singleShot(300, [&app, page] {
        auto* edit = page->findChild<QPlainTextEdit*>();
        auto* kind = page->findChild<QComboBox*>();
        if (!edit || !kind) { std::printf("UI not found\n"); app.quit(); return; }
        int fail = 0;
        for (int i = 0; i < kind->count(); ++i) {
            kind->setCurrentIndex(i);                      // 触发 fill()
            app.processEvents();
            const QString name = kind->currentText();
            const QString text = edit->toPlainText();

            // 1) YAML 合法性
            bool ok = true;
            try {
                YAML::Load(text.toStdString());
            } catch (const std::exception& e) {
                ok = false;
                std::printf("[PARSE-FAIL] %s: %s\n", qPrintable(name), e.what());
            }

            // 2) 流式写法残留（跳过 | 块标量的内容行）
            static const QRegularExpression flowRe(QStringLiteral("\\{[^\\n]*\\}|\\[[^\\n]*\\]"));
            static const QRegularExpression nonSpace(QStringLiteral("\\S"));
            QStringList flowHits;
            int blockScalarIndent = -1;
            const auto lines = text.split(QLatin1Char('\n'));
            for (int ln = 0; ln < lines.size(); ++ln) {
                const QString& l = lines[ln];
                const int ind = l.indexOf(nonSpace);
                if (blockScalarIndent >= 0) {
                    if (l.trimmed().isEmpty() || ind >= blockScalarIndent) continue;
                    blockScalarIndent = -1;                 // 缩进回落：块标量结束
                }
                if (l.trimmed().endsWith(QLatin1Char('|'))) {
                    blockScalarIndent = ind + 2;
                    continue;
                }
                if (flowRe.match(l).hasMatch())
                    flowHits << QStringLiteral("L%1: %2").arg(ln + 1).arg(l.trimmed().left(50));
            }

            // 3) 高亮抽查：前 5 个非空行至少 3 行有 FormatRange
            int hl = 0;
            for (int b = 0, seen = 0; b < edit->document()->blockCount() && seen < 5; ++b) {
                QTextBlock blk = edit->document()->findBlockByNumber(b);
                if (blk.text().trimmed().isEmpty()) continue;
                ++seen;
                if (!blk.layout()->formats().isEmpty()) ++hl;
            }
            const bool hlOk = hl >= 3;
            if (!ok || !flowHits.isEmpty() || !hlOk) ++fail;
            std::printf("%-28s parse=%s flow=%d hl=%d/5%s\n", qPrintable(name),
                        ok ? "OK" : "FAIL", int(flowHits.size()), hl, hlOk ? "" : " <HL-LOW>");
            for (const QString& h : flowHits) std::printf("    flow残留 %s\n", qPrintable(h));
        }
        std::printf(fail == 0 ? "ALL TEMPLATES OK\n" : "%d TEMPLATES NEED FIX\n", fail);
        std::fflush(stdout);
        app.quit();
    });
    return app.exec();
}
