#include "pages.h"
#include "widgets/toolpage.h"
#include "core/texttools.h"
#include "iconprovider.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextObject>
#include <QClipboard>
#include <QApplication>
#include <qrencode.h>

namespace {

// ---------- 随机字符串 ----------
class RandomPage final : public TextToolPage {
public:
    RandomPage() : TextToolPage({{QStringLiteral("生成"), QStringLiteral("go"), true}}, false, false) {
        outputEdit()->setPlaceholderText(QStringLiteral("点击上方“生成”按钮…"));
        ToolPage::setMeta(QStringLiteral("zap"), QStringLiteral("随机字符串"),
                          QStringLiteral("密码 / 随机串批量生成"));
        m_len = new QSpinBox; m_len->setRange(1, 512); m_len->setValue(16);
        m_cnt = new QSpinBox; m_cnt->setRange(1, 200); m_cnt->setValue(5);
        addOptionWidget(ui::formRow(QStringLiteral("长度"), m_len));
        addOptionWidget(ui::formRow(QStringLiteral("数量"), m_cnt));
        m_lower = new QCheckBox(QStringLiteral("小写")); m_lower->setChecked(true);
        m_upper = new QCheckBox(QStringLiteral("大写")); m_upper->setChecked(true);
        m_digit = new QCheckBox(QStringLiteral("数字")); m_digit->setChecked(true);
        m_sym = new QCheckBox(QStringLiteral("符号"));
        m_noAmb = new QCheckBox(QStringLiteral("排除易混淆 (0O1lI)")); m_noAmb->setChecked(true);
        addOptionWidget(m_lower); addOptionWidget(m_upper); addOptionWidget(m_digit);
        addOptionWidget(m_sym); addOptionWidget(m_noAmb);
    }
protected:
    void run(const QString&) override {
        texttools::RandomOptions opt;
        opt.length = m_len->value();
        opt.count = m_cnt->value();
        opt.lower = m_lower->isChecked();
        opt.upper = m_upper->isChecked();
        opt.digits = m_digit->isChecked();
        opt.symbols = m_sym->isChecked();
        opt.excludeAmbiguous = m_noAmb->isChecked();
        showResult(opt.generate(), QStringLiteral("已生成 %1 条").arg(opt.count));
    }
    QSpinBox* m_len; QSpinBox* m_cnt;
    QCheckBox* m_lower; QCheckBox* m_upper; QCheckBox* m_digit; QCheckBox* m_sym; QCheckBox* m_noAmb;
};

// ---------- UUID ----------
class UuidPage final : public TextToolPage {
public:
    UuidPage() : TextToolPage({{QStringLiteral("生成"), QStringLiteral("go"), true}}, false, false) {
        outputEdit()->setPlaceholderText(QStringLiteral("点击上方“生成”按钮…"));
        ToolPage::setMeta(QStringLiteral("hash"), QStringLiteral("UUID 生成"), QStringLiteral("批量生成 UUID v4"));
        m_cnt = new QSpinBox; m_cnt->setRange(1, 500); m_cnt->setValue(5);
        addOptionWidget(ui::formRow(QStringLiteral("数量"), m_cnt));
        m_fmt = new QComboBox;
        m_fmt->addItems({QStringLiteral("标准（含连字符）"), QStringLiteral("大写"), QStringLiteral("无连字符"), QStringLiteral("花括号")});
        addOptionWidget(ui::formRow(QStringLiteral("格式"), m_fmt));
    }
protected:
    void run(const QString&) override {
        QStringList out;
        for (int i = 0; i < m_cnt->value(); ++i)
            out << texttools::uuidGenerate(m_fmt->currentIndex() == 1, m_fmt->currentIndex() == 3, m_fmt->currentIndex() == 2);
        showResult(out.join(QLatin1Char('\n')), QStringLiteral("已生成 %1 个").arg(out.size()));
    }
    QSpinBox* m_cnt; QComboBox* m_fmt;
};

// ---------- JSON ----------
class JsonPage final : public TextToolPage {
public:
    JsonPage() : TextToolPage({{QStringLiteral("格式化"), QStringLiteral("fmt"), true},
                               {QStringLiteral("压缩"), QStringLiteral("min"), false},
                               {QStringLiteral("校验"), QStringLiteral("chk"), false},
                               {QStringLiteral("转义"), QStringLiteral("esc"), false},
                               {QStringLiteral("去转义"), QStringLiteral("uesc"), false}}, false) {
        ToolPage::setMeta(QStringLiteral("code"), QStringLiteral("JSON 工具"),
                          QStringLiteral("格式化 / 压缩 / 校验 / 转义"));
        m_indent = new QComboBox;
        m_indent->addItems({QStringLiteral("2 空格缩进"), QStringLiteral("4 空格缩进")});
        addOptionWidget(m_indent);
    }
protected:
    void run(const QString& actionId) override {
        const QString in = inputEdit()->toPlainText();
        if (actionId == QStringLiteral("fmt")) {
            texttools::JsonResult r = texttools::jsonFormat(in, m_indent->currentIndex() == 0 ? 2 : 4);
            r.ok ? showResult(r.text, QStringLiteral("格式化完成")) : showError(r.error);
        } else if (actionId == QStringLiteral("min")) {
            texttools::JsonResult r = texttools::jsonCompact(in);
            r.ok ? showResult(r.text, QStringLiteral("压缩完成")) : showError(r.error);
        } else if (actionId == QStringLiteral("chk")) {
            texttools::JsonResult r = texttools::jsonFormat(in, 4);
            r.ok ? showResult(r.text, QStringLiteral("✓ JSON 有效")) : showError(r.error);
        } else if (actionId == QStringLiteral("esc")) {
            showResult(texttools::jsonEscape(in), QStringLiteral("已转义"));
        } else {
            showResult(texttools::jsonUnescape(in), QStringLiteral("已去转义"));
        }
    }
    QComboBox* m_indent;
};

// ---------- YAML/JSON ----------
class YamlPage final : public TextToolPage {
public:
    YamlPage() : TextToolPage({{QStringLiteral("YAML ⇒ JSON"), QStringLiteral("tojson"), true},
                               {QStringLiteral("JSON ⇒ YAML"), QStringLiteral("toyaml"), false}}, false) {
        ToolPage::setMeta(QStringLiteral("code"), QStringLiteral("YAML / JSON"),
                          QStringLiteral("YAML 与 JSON 双向转换"));
    }
protected:
    void run(const QString& actionId) override {
        const QString in = inputEdit()->toPlainText();
        texttools::JsonResult r = actionId == QStringLiteral("tojson") ? texttools::yamlToJson(in)
                                                                       : texttools::jsonToYaml(in);
        r.ok ? showResult(r.text, QStringLiteral("转换完成")) : showError(r.error);
    }
};

// ---------- 时间戳 ----------
class TimestampPage final : public ToolPage {
public:
    TimestampPage() {
        ToolPage::setMeta(QStringLiteral("clock"), QStringLiteral("时间戳转换"),
                          QStringLiteral("Unix 时间戳与日期互转"));
        auto* nowCard = new QWidget;
        auto* nowLay = new QHBoxLayout(nowCard);
        nowLay->setContentsMargins(0, 0, 0, 0);
        m_nowLabel = new QLabel;
        m_nowLabel->setFont(Icons::monoFont(10));
        auto* nowBtn = ui::button(QStringLiteral("填入当前"));
        connect(nowBtn, &QPushButton::clicked, this, [this] { m_input->setText(nowString()); convert(); });
        nowLay->addWidget(new QLabel(QStringLiteral("当前:")));
        nowLay->addWidget(m_nowLabel, 1);
        nowLay->addWidget(nowBtn);
        body()->addWidget(ui::card(QStringLiteral("当前时间"), nowCard));

        m_input = new QLineEdit;
        m_input->setPlaceholderText(QStringLiteral("输入 Unix 时间戳（秒或毫秒自动识别）或日期如 2026-01-01 12:00:00"));
        m_unit = new QComboBox;
        m_unit->addItems({QStringLiteral("自动识别"), QStringLiteral("秒"), QStringLiteral("毫秒")});
        connect(m_input, &QLineEdit::textChanged, this, &TimestampPage::convert);
        connect(m_unit, &QComboBox::currentIndexChanged, this, [this] { convert(); });
        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->addWidget(m_input, 1);
        rowLay->addWidget(new QLabel(QStringLiteral("单位:")));
        rowLay->addWidget(m_unit);
        body()->addWidget(ui::card(QStringLiteral("转换"), row));

        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("结果"), m_output, copyBtn, true), 1);

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this] { m_nowLabel->setText(nowString()); });
        m_timer->start(1000);
        m_nowLabel->setText(nowString());
    }
private:
    void convert() {
        const QString t = m_input->text().trimmed();
        m_output->clear();
        if (t.isEmpty()) return;
        QDateTime dt;
        bool isNum = true;
        for (const QChar& c : t)
            if (!c.isDigit()) { isNum = false; break; }
        if (isNum) {
            bool ok = false;
            qlonglong v = t.toLongLong(&ok);
            const int mode = m_unit->currentIndex();
            if (mode == 2 || (mode == 0 && v > 9999999999LL))
                dt = QDateTime::fromMSecsSinceEpoch(v);
            else
                dt = QDateTime::fromSecsSinceEpoch(v);
            if (!dt.isValid()) { m_output->setPlainText(QStringLiteral("无效时间戳")); return; }
        } else {
            QStringList formats = {QStringLiteral("yyyy-MM-dd HH:mm:ss"), QStringLiteral("yyyy-MM-dd HH:mm"),
                                   QStringLiteral("yyyy/MM/dd HH:mm:ss"), QStringLiteral("yyyy-MM-dd"),
                                   QStringLiteral("yyyy-MM-ddTHH:mm:ss")};
            for (const QString& f : formats) {
                dt = QDateTime::fromString(t, f);
                if (dt.isValid()) break;
            }
            if (!dt.isValid()) { m_output->setPlainText(QStringLiteral("无法识别的日期格式")); return; }
            dt.setTimeSpec(Qt::LocalTime);
        }
        QString out;
        out += QStringLiteral("Unix 秒:      %1\n").arg(dt.toSecsSinceEpoch());
        out += QStringLiteral("Unix 毫秒:    %1\n").arg(dt.toMSecsSinceEpoch());
        out += QStringLiteral("本地时间:     %1\n").arg(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        QDateTime utc = dt.toUTC();
        out += QStringLiteral("UTC 时间:     %1\n").arg(utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        out += QStringLiteral("ISO 8601:     %1\n").arg(dt.toUTC().toString(Qt::ISODate));
        out += QStringLiteral("星期:         %1\n").arg(dt.date().toString(QStringLiteral("dddd")));
        out += QStringLiteral("相对现在:     %1").arg(relative(dt));
        m_output->setPlainText(out);
    }
    QString nowString() const { return QStringLiteral("%1 | %2").arg(QDateTime::currentSecsSinceEpoch()).arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))); }
    QString relative(const QDateTime& dt) const {
        qint64 diff = dt.secsTo(QDateTime::currentDateTime());
        bool past = diff >= 0;
        diff = qAbs(diff);
        QString rel;
        if (diff < 60) rel = QStringLiteral("%1 秒").arg(diff);
        else if (diff < 3600) rel = QStringLiteral("%1 分钟").arg(diff / 60);
        else if (diff < 86400) rel = QStringLiteral("%1 小时").arg(diff / 3600);
        else rel = QStringLiteral("%1 天").arg(diff / 86400);
        return past ? QStringLiteral("%1 前").arg(rel) : QStringLiteral("%1 后").arg(rel);
    }
    QLabel* m_nowLabel;
    QLineEdit* m_input;
    QComboBox* m_unit;
    QPlainTextEdit* m_output;
    QTimer* m_timer;
};

// ---------- 正则 ----------
class RegexPage final : public TextToolPage {
public:
    RegexPage() : TextToolPage({{QStringLiteral("匹配"), QStringLiteral("go"), true}}, true) {
        ToolPage::setMeta(QStringLiteral("terminal"), QStringLiteral("正则测试"),
                          QStringLiteral("正则表达式实时匹配与分组捕获"));
        m_pattern = new QLineEdit;
        m_pattern->setPlaceholderText(QStringLiteral("输入正则表达式，如 (\\d+)-(\\d+)"));
        m_pattern->setFont(Icons::monoFont(10));
        connect(m_pattern, &QLineEdit::textChanged, this, [this] { m_debounceTimer(); });
        addOptionWidget(ui::formRow(QStringLiteral("正则"), m_pattern));
        m_case = new QCheckBox(QStringLiteral("忽略大小写"));
        m_multi = new QCheckBox(QStringLiteral("多行 ^$"));
        m_dotAll = new QCheckBox(QStringLiteral("点号匹配换行"));
        addOptionWidget(m_case); addOptionWidget(m_multi); addOptionWidget(m_dotAll);
    }
    void m_debounceTimer() { run(QStringLiteral("go")); }
protected:
    void run(const QString&) override {
        const QString pat = m_pattern->text();
        if (pat.isEmpty()) { outputEdit()->clear(); return; }
        QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
        if (m_case->isChecked()) opts |= QRegularExpression::CaseInsensitiveOption;
        if (m_multi->isChecked()) opts |= QRegularExpression::MultilineOption;
        if (m_dotAll->isChecked()) opts |= QRegularExpression::DotMatchesEverythingOption;
        QRegularExpression re(pat, opts);
        if (!re.isValid()) { showError(QStringLiteral("正则无效: %1").arg(re.errorString())); return; }
        const QString text = inputEdit()->toPlainText();
        auto it = re.globalMatch(text);
        QStringList out;
        int count = 0;
        while (it.hasNext() && count < 1000) {
            auto m = it.next();
            ++count;
            out << QStringLiteral("#%1 [位置 %2-%3] %4")
                       .arg(count).arg(m.capturedStart()).arg(m.capturedEnd())
                       .arg(m.captured(0));
            for (int g = 1; g < m.lastCapturedIndex() + 1; ++g) {
                QString cap = m.captured(g);
                out << QStringLiteral("    组%1: %2").arg(g).arg(cap.isNull() ? QStringLiteral("（未匹配）") : cap);
            }
        }
        showResult(out.isEmpty() ? QStringLiteral("（无匹配）") : out.join(QLatin1Char('\n')),
                   QStringLiteral("共 %1 处匹配").arg(count));
    }
    QLineEdit* m_pattern;
    QCheckBox* m_case; QCheckBox* m_multi; QCheckBox* m_dotAll;
};

// ---------- Diff ----------
class DiffPage final : public TextToolPage {
public:
    DiffPage() : TextToolPage({{QStringLiteral("对比"), QStringLiteral("go"), true}}, false) {
        ToolPage::setMeta(QStringLiteral("file"), QStringLiteral("文本对比"),
                          QStringLiteral("两段文本差异高亮对比（原始版在左，新版在输入框）"));
        m_sideA = new QPlainTextEdit;
        m_sideA->setObjectName(QStringLiteral("mono"));
        m_sideA->setPlaceholderText(QStringLiteral("原始版本…"));
        m_sideA->setFixedHeight(150);
        addOptionWidget(m_sideA);
        inputEdit()->setPlaceholderText(QStringLiteral("新版本（与上方原始版本对比）…"));
    }
protected:
    void run(const QString&) override {
        texttools::DiffResult r = texttools::diffLines(m_sideA->toPlainText(), inputEdit()->toPlainText());
        if (!r.ok) { showError(r.error); return; }
        QStringList out;
        for (const auto& line : r.lines) {
            if (line.op == '+') out << QStringLiteral("+ %1").arg(line.text);
            else if (line.op == '-') out << QStringLiteral("- %1").arg(line.text);
            else out << QStringLiteral("  %1").arg(line.text);
        }
        showResult(out.join(QLatin1Char('\n')), QStringLiteral("对比完成"));
        // 高亮 +/-
        highlight();
    }
    void highlight() {
        QList<QTextEdit::ExtraSelection> sels;
        QTextDocument* doc = outputEdit()->document();
        QTextCharFormat addFmt, delFmt;
        addFmt.setForeground(QColor(0x3F, 0xB9, 0x50));
        addFmt.setBackground(QColor(63, 185, 80, 30));
        delFmt.setForeground(QColor(0xF8, 0x51, 0x49));
        delFmt.setBackground(QColor(248, 81, 73, 30));
        for (QTextBlock it = doc->begin(); it.isValid(); it = it.next()) {
            const QString t = it.text();
            if (!t.startsWith(QLatin1Char('+')) && !t.startsWith(QLatin1Char('-'))) continue;
            QTextEdit::ExtraSelection sel;
            sel.cursor = QTextCursor(it);
            sel.format = t.startsWith(QLatin1Char('+')) ? addFmt : delFmt;
            sels.append(sel);
        }
        outputEdit()->setExtraSelections(sels);
    }
    QPlainTextEdit* m_sideA;
};

// ---------- Cron ----------
class CronPage final : public TextToolPage {
public:
    CronPage() : TextToolPage({{QStringLiteral("解析"), QStringLiteral("go"), true}}, true) {
        ToolPage::setMeta(QStringLiteral("clock"), QStringLiteral("Cron 解析"),
                          QStringLiteral("Cron 表达式解析与未来执行时间预览"));
        m_input->setPlaceholderText(QStringLiteral("如 */5 * * * * 或 0 9 * * 1-5"));
    }
protected:
    void run(const QString&) override {
        texttools::CronInfo info = texttools::cronParse(inputEdit()->toPlainText(), QDateTime::currentDateTime());
        if (!info.ok) { showError(info.error); return; }
        QStringList out;
        out << QStringLiteral("字段说明:");
        out << info.describe;
        out << QString();
        out << QStringLiteral("未来 5 次执行时间:");
        for (const QDateTime& dt : info.nextRuns)
            out << QStringLiteral("  %1  (%2)").arg(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                                   dt.date().toString(QStringLiteral("dddd")));
        showResult(out.join(QLatin1Char('\n')), QStringLiteral("表达式有效"));
    }
};

// ---------- 二维码 ----------
class QrPage final : public ToolPage {
public:
    QrPage() {
        ToolPage::setMeta(QStringLiteral("grid"), QStringLiteral("二维码生成"),
                          QStringLiteral("文本 / 链接生成二维码图片"));
        m_input = new QPlainTextEdit;
        m_input->setPlaceholderText(QStringLiteral("输入文本或链接（建议 ≤ 1000 字符）"));
        m_input->setFixedHeight(110);
        m_size = new QComboBox;
        m_size->addItems({QStringLiteral("256 px"), QStringLiteral("384 px"), QStringLiteral("512 px")});
        m_level = new QComboBox;
        m_level->addItems({QStringLiteral("L - 低"), QStringLiteral("M - 中"), QStringLiteral("Q - 较高"), QStringLiteral("H - 高")});
        m_level->setCurrentIndex(1);
        connect(m_input, &QPlainTextEdit::textChanged, this, [this] { m_deb.start(300); });
        connect(&m_deb, &QTimer::timeout, this, [this] { generate(); });

        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->addWidget(new QLabel(QStringLiteral("尺寸:"))); rowLay->addWidget(m_size);
        rowLay->addWidget(new QLabel(QStringLiteral("容错:"))); rowLay->addWidget(m_level);
        auto* inner = new QWidget;
        auto* inLay = new QVBoxLayout(inner);
        inLay->setContentsMargins(0, 0, 0, 0);
        inLay->addWidget(m_input);
        inLay->addWidget(row);
        body()->addWidget(ui::card(QStringLiteral("内容"), inner));

        m_preview = new QLabel;
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setMinimumHeight(300);
        auto* saveBtn = ui::button(QStringLiteral("保存 PNG"));
        auto* copyBtn = ui::button(QStringLiteral("复制图片"));
        connect(saveBtn, &QPushButton::clicked, this, [this] {
            QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存二维码"),
                                                        QStringLiteral("qrcode.png"), QStringLiteral("PNG (*.png)"));
            if (!path.isEmpty() && !m_preview->pixmap().isNull()) m_preview->pixmap().save(path, "PNG");
        });
        connect(copyBtn, &QPushButton::clicked, this, [this] {
            if (!m_preview->pixmap().isNull())
                QApplication::clipboard()->setPixmap(m_preview->pixmap());
        });
        auto* btnRow = new QWidget;
        auto* btnLay = new QHBoxLayout(btnRow);
        btnLay->setContentsMargins(0, 0, 0, 0);
        btnLay->addWidget(saveBtn); btnLay->addWidget(copyBtn); btnLay->addStretch();
        body()->addWidget(ui::card(QStringLiteral("预览"), m_preview, btnRow, true), 1);
    }
private:
    void generate() {
        const QString text = m_input->toPlainText();
        if (text.isEmpty()) { m_preview->clear(); m_preview->setText(QStringLiteral("输入后自动生成")); return; }
        static const QRecLevel levels[] = {QR_ECLEVEL_L, QR_ECLEVEL_M, QR_ECLEVEL_Q, QR_ECLEVEL_H};
        QRcode* code = QRcode_encodeString(text.toUtf8().constData(), 0, levels[m_level->currentIndex()], QR_MODE_8, 1);
        if (!code) { m_preview->setText(QStringLiteral("生成失败（内容过长或编码错误）")); return; }
        const int quiet = 4;
        const int width = code->width;
        const int scale = qMax(2, m_size->currentText().split(QLatin1Char(' ')).first().toInt() / (width + quiet * 2));
        const int total = (width + quiet * 2) * scale;
        QImage img(total, total, QImage::Format_RGB32);
        img.fill(0xFFFFFFFF);
        for (int y = 0; y < width; ++y) {
            for (int x = 0; x < width; ++x) {
                if (code->data[y * width + x] & 1) {
                    for (int dy = 0; dy < scale; ++dy)
                        for (int dx = 0; dx < scale; ++dx)
                            img.setPixel((x + quiet) * scale + dx, (y + quiet) * scale + dy, 0xFF1A1A1A);
                }
            }
        }
        QRcode_free(code);
        m_preview->setPixmap(QPixmap::fromImage(img.scaled(m_size->currentText().split(QLatin1Char(' ')).first().toInt(),
                                                           m_size->currentText().split(QLatin1Char(' ')).first().toInt(),
                                                           Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    QPlainTextEdit* m_input;
    QLabel* m_preview;
    QComboBox* m_size; QComboBox* m_level;
    QTimer m_deb;
};

// ---------- 文本处理 ----------
class TextProcPage final : public TextToolPage {
public:
    TextProcPage() : TextToolPage({{QStringLiteral("去重"), QStringLiteral("dedupe"), true},
                                   {QStringLiteral("排序"), QStringLiteral("sort"), false},
                                   {QStringLiteral("倒序"), QStringLiteral("rsort"), false},
                                   {QStringLiteral("反转行序"), QStringLiteral("reverse"), false},
                                   {QStringLiteral("去空行"), QStringLiteral("norempty"), false},
                                   {QStringLiteral("加序号"), QStringLiteral("number"), false},
                                   {QStringLiteral("去首尾空格"), QStringLiteral("trim"), false},
                                   {QStringLiteral("统计"), QStringLiteral("stats"), false}}, false) {
        ToolPage::setMeta(QStringLiteral("file"), QStringLiteral("文本处理"),
                          QStringLiteral("按行去重 / 排序 / 批量处理 / 统计"));
    }
protected:
    void run(const QString& actionId) override {
        const QString in = inputEdit()->toPlainText();
        if (actionId == QStringLiteral("stats")) {
            texttools::TextStats s = texttools::textStats(in);
            showResult(QStringLiteral("行数: %1\n字符数: %2\n词数:  %3\n字节数: %4")
                           .arg(s.lines).arg(s.chars).arg(s.words).arg(s.bytes),
                       QStringLiteral("统计完成"));
            return;
        }
        showResult(texttools::textProcess(in, actionId), QStringLiteral("处理完成"));
    }
};

// ---------- SQL 格式化 ----------
class SqlPage final : public TextToolPage {
public:
    SqlPage() : TextToolPage({{QStringLiteral("格式化"), QStringLiteral("go"), true}}, false) {
        ToolPage::setMeta(QStringLiteral("terminal"), QStringLiteral("SQL 格式化"),
                          QStringLiteral("SQL 语句美化排版（基础版）"));
    }
protected:
    void run(const QString&) override {
        texttools::JsonResult r = texttools::sqlFormat(inputEdit()->toPlainText());
        r.ok ? showResult(r.text, QStringLiteral("格式化完成")) : showError(r.error);
    }
};

} // namespace

namespace pages {
ToolPage* createRandom() { return new RandomPage; }
ToolPage* createUuid() { return new UuidPage; }
ToolPage* createJson() { return new JsonPage; }
ToolPage* createYaml() { return new YamlPage; }
ToolPage* createTimestamp() { return new TimestampPage; }
ToolPage* createRegex() { return new RegexPage; }
ToolPage* createDiff() { return new DiffPage; }
ToolPage* createCron() { return new CronPage; }
ToolPage* createQrcode() { return new QrPage; }
ToolPage* createTextProc() { return new TextProcPage; }
ToolPage* createSqlFormat() { return new SqlPage; }
} // namespace pages
