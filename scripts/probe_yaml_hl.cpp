// YAML 模板页高亮探针：offscreen 实例化 K8sYamlPage，读取 block layout 的 FormatRange
// （QSyntaxHighlighter 的着色在 QTextLayout::formats()，不在 fragment 的 charFormat）。
#include <QApplication>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <cstdio>
#include "tools/pages.h"
#include "widgets/toolpage.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget* page = pages::createK8sYaml();
    page->show();
    QTimer::singleShot(300, [&app, page] {
        auto* edit = page->findChild<QPlainTextEdit*>();
        if (!edit) { std::printf("no QPlainTextEdit found\n"); app.quit(); return; }
        const QTextDocument* doc = edit->document();
        std::printf("blocks=%d\n", doc->blockCount());
        int shown = 0, coloredLines = 0;
        for (int b = 0; b < doc->blockCount() && shown < 14; ++b) {
            QTextBlock block = doc->findBlockByNumber(b);
            const QString text = block.text();
            if (text.trimmed().isEmpty()) continue;
            ++shown;
            const QList<QTextLayout::FormatRange> rs = block.layout()->formats();
            if (!rs.isEmpty()) ++coloredLines;
            std::printf("L%02d | %-58s | %d 段: ", b, qPrintable(text.left(58)), int(rs.size()));
            for (const QTextLayout::FormatRange& r : rs) {
                const QColor c = r.format.foreground().color();
                std::printf("[%d+%d #%02x%02x%02x%s `%s`] ", r.start, r.length,
                            c.red(), c.green(), c.blue(),
                            r.format.fontWeight() == QFont::Bold ? " B" : "",
                            qPrintable(text.mid(r.start, qMin(r.length, 14))));
            }
            std::printf("\n");
        }
        std::printf("colored-lines=%d/%d\n", coloredLines, shown);
        std::printf("YAML HL PROBE DONE\n");
        std::fflush(stdout);
        app.quit();
    });
    return app.exec();
}
