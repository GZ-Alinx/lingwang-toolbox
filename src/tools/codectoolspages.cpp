#include "pages.h"
#include "widgets/toolpage.h"
#include "core/codecs.h"
#include "iconprovider.h"

#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QFileInfo>

namespace {

using codecs::JwtInfo;

// ---------- Base64 ----------
class Base64Page final : public TextToolPage {
public:
    Base64Page() : TextToolPage({{QStringLiteral("编码"), QStringLiteral("enc"), true},
                                 {QStringLiteral("解码"), QStringLiteral("dec"), false}}, false) {
        m_urlSafe = new QCheckBox(QStringLiteral("URL 安全（-_/）"));
        addOptionWidget(m_urlSafe);
        auto* fileBtn = ui::button(QStringLiteral("文件⇒Base64"));
        connect(fileBtn, &QPushButton::clicked, this, [this] {
            QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"));
            if (path.isEmpty()) return;
            QString err;
            QString out = codecs::fileToBase64(path, m_dataUri->isChecked(), &err);
            if (!err.isEmpty()) { showError(err); return; }
            showResult(out, QStringLiteral("已编码（%1 字节）").arg(QFileInfo(path).size()));
        });
        m_dataUri = new QCheckBox(QStringLiteral("data URI 前缀"));
        addOptionWidget(m_dataUri);
        addOptionWidget(fileBtn);
        auto* saveBtn = ui::button(QStringLiteral("Base64⇒文件"));
        connect(saveBtn, &QPushButton::clicked, this, [this] {
            QString text = inputEdit()->toPlainText().trimmed();
            if (text.isEmpty()) { showError(QStringLiteral("请先在输入框粘贴 Base64 数据")); return; }
            QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"));
            if (path.isEmpty()) return;
            QString err;
            if (codecs::base64ToFile(text, path, &err))
                showResult(outputEdit()->toPlainText(), QStringLiteral("已保存: %1").arg(path));
            else
                showError(err);
        });
        addOptionWidget(saveBtn);
    }
protected:
    void run(const QString& actionId) override {
        bool urlSafe = m_urlSafe->isChecked();
        if (actionId == QStringLiteral("enc")) {
            showResult(codecs::base64Encode(inputEdit()->toPlainText(), urlSafe), QStringLiteral("编码完成"));
        } else {
            codecs::Base64Result r = codecs::base64Decode(inputEdit()->toPlainText(), urlSafe);
            r.ok ? showResult(r.text, QStringLiteral("解码完成")) : showError(r.error);
        }
    }
    QCheckBox* m_urlSafe = nullptr;
    QCheckBox* m_dataUri = nullptr;
};

// ---------- URL ----------
class UrlPage final : public TextToolPage {
public:
    UrlPage() : TextToolPage({{QStringLiteral("编码"), QStringLiteral("enc"), true},
                              {QStringLiteral("解码"), QStringLiteral("dec"), false}}, false) {
        m_plus = new QCheckBox(QStringLiteral("空格转 +"));
        addOptionWidget(m_plus);
    }
protected:
    void run(const QString& actionId) override {
        bool plus = m_plus->isChecked();
        if (actionId == QStringLiteral("enc"))
            showResult(codecs::urlEncode(inputEdit()->toPlainText(), plus), QStringLiteral("编码完成"));
        else
            showResult(codecs::urlDecode(inputEdit()->toPlainText(), plus), QStringLiteral("解码完成"));
    }
    QCheckBox* m_plus = nullptr;
};

// ---------- 进制转换 ----------
class RadixPage final : public TextToolPage {
public:
    RadixPage() : TextToolPage({{QStringLiteral("转换"), QStringLiteral("go"), true}}, true) {
        m_from = new QComboBox;
        m_from->addItems({QStringLiteral("十进制"), QStringLiteral("十六进制"), QStringLiteral("二进制"), QStringLiteral("八进制")});
        connect(m_from, &QComboBox::currentIndexChanged, this, [this] { run(QStringLiteral("go")); });
        addOptionWidget(m_from);
    }
protected:
    void run(const QString&) override {
        static const int bases[] = {10, 16, 2, 8};
        codecs::RadixResult r = codecs::radixConvert(inputEdit()->toPlainText(), bases[m_from->currentIndex()]);
        if (!r.ok) { showError(r.error); return; }
        showResult(QStringLiteral("二进制:  %1\n八进制:  %2\n十进制:  %3\n十六进制: %4").arg(r.bin, r.oct, r.dec, r.hex),
                   r.error.contains(QStringLiteral("字符")) ? r.error : QStringLiteral("转换完成"));
    }
    QComboBox* m_from = nullptr;
};

// ---------- Unicode ----------
class UnicodePage final : public TextToolPage {
public:
    UnicodePage() : TextToolPage({{QStringLiteral("转 \\uXXXX"), QStringLiteral("enc"), true},
                                  {QStringLiteral("\\uXXXX 转文本"), QStringLiteral("dec"), false}}, false) {}
protected:
    void run(const QString& actionId) override {
        if (actionId == QStringLiteral("enc"))
            showResult(codecs::unicodeEncode(inputEdit()->toPlainText()), QStringLiteral("转换完成"));
        else
            showResult(codecs::unicodeDecode(inputEdit()->toPlainText()), QStringLiteral("转换完成"));
    }
};

// ---------- JWT ----------
class JwtPage final : public TextToolPage {
public:
    JwtPage() : TextToolPage({{QStringLiteral("解析"), QStringLiteral("go"), true}}, true) {
        outputEdit()->setPlaceholderText(QStringLiteral("Header / Payload 将显示在这里…"));
    }
protected:
    void run(const QString&) override {
        JwtInfo info = codecs::jwtParse(inputEdit()->toPlainText());
        if (!info.ok) { showError(info.error); return; }
        QString out = QStringLiteral("── Header ──\n%1\n\n── Payload ──\n%2").arg(info.header, info.payload);
        if (!info.signature.isEmpty())
            out += QStringLiteral("\n\n── Signature ──\n%1").arg(info.signature);
        if (!info.timeNotes.isEmpty())
            out += QStringLiteral("\n\n── 时间 ──\n%1").arg(info.timeNotes);
        showResult(out, QStringLiteral("解析成功"));
    }
};

// ---------- HTML 实体 ----------
class HtmlEntityPage final : public TextToolPage {
public:
    HtmlEntityPage() : TextToolPage({{QStringLiteral("编码"), QStringLiteral("enc"), true},
                                     {QStringLiteral("解码"), QStringLiteral("dec"), false}}, true) {}
protected:
    void run(const QString& actionId) override {
        if (actionId == QStringLiteral("enc"))
            showResult(codecs::htmlEncode(inputEdit()->toPlainText()), QStringLiteral("编码完成"));
        else
            showResult(codecs::htmlDecode(inputEdit()->toPlainText()), QStringLiteral("解码完成"));
    }
};

} // namespace

static ToolPage* meta(ToolPage* p, const char* icon, const QString& title, const QString& desc) {
    p->ToolPage::setMeta(QString::fromLatin1(icon), title, desc);
    return p;
}

namespace pages {
ToolPage* createBase64() {
    return meta(new Base64Page, "code", QStringLiteral("Base64 编解码"), QStringLiteral("文本/文件 Base64 编码与解码"));
}
ToolPage* createUrlCodec() {
    return meta(new UrlPage, "code", QStringLiteral("URL 编解码"), QStringLiteral("URL 百分号编码与解码"));
}
ToolPage* createRadix() {
    return meta(new RadixPage, "hash", QStringLiteral("进制转换"), QStringLiteral("二 / 八 / 十 / 十六进制互转"));
}
ToolPage* createUnicode() {
    return meta(new UnicodePage, "code", QStringLiteral("Unicode 转换"), QStringLiteral("中文与 \\uXXXX 转义互转"));
}
ToolPage* createJwt() {
    return meta(new JwtPage, "lock", QStringLiteral("JWT 解析"), QStringLiteral("解析 JWT 结构与过期时间（仅解码，不验签）"));
}
ToolPage* createHtmlEntity() {
    return meta(new HtmlEntityPage, "code", QStringLiteral("HTML 实体"), QStringLiteral("HTML 实体编码与解码"));
}
} // namespace pages
