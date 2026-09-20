#include "pages.h"
#include "widgets/toolpage.h"
#include "core/crypto.h"
#include "core/async.h"
#include "iconprovider.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QElapsedTimer>

namespace {

// ---------- 哈希（文本） ----------
class HashPage final : public TextToolPage {
public:
    HashPage() : TextToolPage({{QStringLiteral("计算"), QStringLiteral("go"), true}}, true) {
        ToolPage::setMeta(QStringLiteral("hash"), QStringLiteral("哈希计算"),
                          QStringLiteral("MD5 / SHA1 / SHA256 / SHA512 / SHA3 摘要"));
        m_algos = {QStringLiteral("MD5"), QStringLiteral("SHA-1"), QStringLiteral("SHA-256"),
                   QStringLiteral("SHA-512"), QStringLiteral("SHA3-256")};
        for (const QString& a : m_algos) {
            auto* cb = new QCheckBox(a);
            cb->setChecked(a == QStringLiteral("MD5") || a == QStringLiteral("SHA-256"));
            m_checks << cb;
            addOptionWidget(cb);
        }
    }
protected:
    void run(const QString&) override {
        const QString text = inputEdit()->toPlainText();
        QStringList out;
        for (int i = 0; i < m_algos.size(); ++i)
            if (m_checks[i]->isChecked())
                out << QStringLiteral("%1:  %2").arg(m_algos[i], crypto::hashText(text, m_algos[i]));
        if (out.isEmpty()) { showError(QStringLiteral("请至少选择一种算法")); return; }
        showResult(out.join(QLatin1Char('\n')), QStringLiteral("计算完成"));
    }
    QStringList m_algos;
    QList<QCheckBox*> m_checks;
};

// ---------- 文件校验 ----------
class FileHashPage final : public ToolPage {
    Q_OBJECT
public:
    FileHashPage() {
        ToolPage::setMeta(QStringLiteral("file"), QStringLiteral("文件校验"),
                          QStringLiteral("计算文件哈希并比对校验值"));
        m_chip = ui::chip();
        m_path = new QLineEdit;
        m_path->setPlaceholderText(QStringLiteral("选择文件后自动计算所有算法"));
        m_path->setReadOnly(true);
        auto* pick = ui::button(QStringLiteral("选择文件…"), "primary");
        connect(pick, &QPushButton::clicked, this, [this] {
            QString p = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"));
            if (!p.isEmpty()) { m_path->setText(p); compute(p); }
        });
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_path, 1);
        lay->addWidget(pick);

        m_compare = new QLineEdit;
        m_compare->setPlaceholderText(QStringLiteral("粘贴期望的哈希值进行比对（可选）"));
        connect(m_compare, &QLineEdit::textChanged, this, &FileHashPage::mark);
        auto* row2 = ui::formRow(QStringLiteral("比对值"), m_compare);

        auto* inner = new QWidget;
        auto* inLay = new QVBoxLayout(inner);
        inLay->setContentsMargins(0, 0, 0, 0);
        inLay->setSpacing(8);
        inLay->addWidget(row);
        inLay->addWidget(row2);
        body()->addWidget(ui::card(QStringLiteral("文件"), inner, m_chip));

        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("哈希"), m_output, copyBtn, true), 1);
    }
signals:
    void done(const QString& text);

private slots:
    void compute(const QString& path) {
        m_output->setPlainText(QStringLiteral("计算中…"));
        ui::setChip(m_chip, QStringLiteral("计算中…"), QStringLiteral("info"));
        auto* worker = new JobThread([this, path](const std::atomic<bool>&) {
            QElapsedTimer t;
            t.start();
            static const QStringList algos = {QStringLiteral("MD5"), QStringLiteral("SHA-1"),
                                              QStringLiteral("SHA-256"), QStringLiteral("SHA-512")};
            QStringList out;
            out << QStringLiteral("文件: %1").arg(path);
            out << QStringLiteral("大小: %1 字节").arg(QFileInfo(path).size());
            for (const QString& a : algos) {
                QString h = crypto::hashFile(path, a);
                out << QStringLiteral("%1:  %2").arg(a, h.isEmpty() ? QStringLiteral("计算失败") : h);
            }
            out << QStringLiteral("耗时: %1ms").arg(t.elapsed());
            emit done(out.join(QLatin1Char('\n')));
        }, this);
        connect(worker, &JobThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }
    void mark() {
        const QString cmp = m_compare->text().trimmed().toLower();
        if (cmp.isEmpty()) { m_chip->hide(); return; }
        const QString text = m_output->toPlainText().toLower();
        const bool found = !cmp.isEmpty() && text.contains(cmp);
        ui::setChip(m_chip, found ? QStringLiteral("✓ 校验值匹配") : QStringLiteral("✗ 与文件哈希不匹配"),
                    found ? QStringLiteral("ok") : QStringLiteral("err"));
    }

private:
    QLineEdit* m_path;
    QLineEdit* m_compare;
    QPlainTextEdit* m_output;
    QLabel* m_chip = nullptr;
};

// ---------- AES ----------
class AesPage final : public TextToolPage {
public:
    AesPage() : TextToolPage({{QStringLiteral("加密"), QStringLiteral("enc"), true},
                              {QStringLiteral("解密"), QStringLiteral("dec"), false}}, false) {
        ToolPage::setMeta(QStringLiteral("lock"), QStringLiteral("AES 加解密"),
                          QStringLiteral("AES-256 GCM / CBC / ECB（密钥 = SHA256(口令)）"));
        m_key = new QLineEdit; m_key->setPlaceholderText(QStringLiteral("加密口令"));
        m_key->setEchoMode(QLineEdit::Password);
        m_iv = new QLineEdit; m_iv->setPlaceholderText(QStringLiteral("IV/Nonce（GCM 建议 12 字节以上）"));
        m_mode = new QComboBox;
        m_mode->addItems({QStringLiteral("GCM（推荐）"), QStringLiteral("CBC"), QStringLiteral("ECB")});
        m_inFmt = new QComboBox;
        m_inFmt->addItems({QStringLiteral("文本"), QStringLiteral("Base64"), QStringLiteral("Hex")});
        m_outFmt = new QComboBox;
        m_outFmt->addItems({QStringLiteral("Base64"), QStringLiteral("Hex"), QStringLiteral("文本")});
        addOptionWidget(ui::formRow(QStringLiteral("口令"), m_key));
        addOptionWidget(ui::formRow(QStringLiteral("IV"), m_iv));
        addOptionWidget(ui::formRow(QStringLiteral("模式"), m_mode));
        addOptionWidget(ui::formRow(QStringLiteral("输入格式"), m_inFmt));
        addOptionWidget(ui::formRow(QStringLiteral("输出格式"), m_outFmt));
    }
protected:
    void run(const QString& actionId) override {
        if (m_key->text().isEmpty()) { showError(QStringLiteral("请输入加密口令")); return; }
        const QString mode = m_mode->currentIndex() == 0 ? QStringLiteral("GCM")
                             : m_mode->currentIndex() == 1 ? QStringLiteral("CBC") : QStringLiteral("ECB");
        if (actionId == QStringLiteral("enc")) {
            const QString outFmt = m_outFmt->currentIndex() == 0 ? QStringLiteral("base64")
                                   : m_outFmt->currentIndex() == 1 ? QStringLiteral("hex") : QStringLiteral("text");
            crypto::AesResult r = crypto::aesEncrypt(inputEdit()->toPlainText(), m_key->text(), m_iv->text(), mode, outFmt);
            r.ok ? showResult(r.text, QStringLiteral("加密完成")) : showError(r.error);
        } else {
            const QString inFmt = m_inFmt->currentIndex() == 0 ? QStringLiteral("text")
                                  : m_inFmt->currentIndex() == 1 ? QStringLiteral("base64") : QStringLiteral("hex");
            crypto::AesResult r = crypto::aesDecrypt(inputEdit()->toPlainText(), m_key->text(), m_iv->text(), mode, inFmt);
            r.ok ? showResult(r.text, QStringLiteral("解密完成")) : showError(r.error);
        }
    }
    QLineEdit* m_key; QLineEdit* m_iv;
    QComboBox* m_mode; QComboBox* m_inFmt; QComboBox* m_outFmt;
};

// ---------- RSA ----------
class RsaPage final : public ToolPage {
    Q_OBJECT
public:
    RsaPage() {
        ToolPage::setMeta(QStringLiteral("lock"), QStringLiteral("RSA 密钥生成"),
                          QStringLiteral("生成 RSA 公私钥对（PEM 格式）"));
        m_bits = new QComboBox;
        m_bits->addItems({QStringLiteral("2048 位（常用）"), QStringLiteral("3072 位"), QStringLiteral("4096 位（更安全）")});
        m_btn = ui::button(QStringLiteral("生成密钥对"), "primary");
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(new QLabel(QStringLiteral("密钥长度:")));
        lay->addWidget(m_bits);
        lay->addWidget(m_btn);
        lay->addStretch();
        m_chip = ui::chip();
        lay->addWidget(m_chip);
        body()->addWidget(ui::card(QStringLiteral("参数"), row));

        m_priv = new QPlainTextEdit;
        m_priv->setObjectName(QStringLiteral("mono"));
        m_priv->setReadOnly(true);
        m_priv->setFixedHeight(220);
        m_pub = new QPlainTextEdit;
        m_pub->setObjectName(QStringLiteral("mono"));
        m_pub->setReadOnly(true);
        m_pub->setFixedHeight(130);
        body()->addWidget(ui::card(QStringLiteral("私钥（妥善保管）"), m_priv));
        body()->addWidget(ui::card(QStringLiteral("公钥（可分发）"), m_pub));
        body()->addStretch();

        connect(m_btn, &QPushButton::clicked, this, [this] {
            static const int bitsArr[] = {2048, 3072, 4096};
            const int bits = bitsArr[m_bits->currentIndex()];
            m_btn->setEnabled(false);
            ui::setChip(m_chip, QStringLiteral("生成中，可能需要数秒…"), QStringLiteral("info"));
            auto* worker = new JobThread([this, bits](const std::atomic<bool>&) {
                crypto::RsaResult r = crypto::rsaGenerate(bits);
                emit ready(r);
            }, this);
            connect(worker, &JobThread::finished, worker, &QObject::deleteLater);
            worker->start();
        });
        connect(this, &RsaPage::ready, this, [this](const crypto::RsaResult& r) {
            m_btn->setEnabled(true);
            if (r.ok) {
                m_priv->setPlainText(r.privateKey);
                m_pub->setPlainText(r.publicKey);
                ui::setChip(m_chip, QStringLiteral("生成成功"), QStringLiteral("ok"));
            } else {
                ui::setChip(m_chip, r.error, QStringLiteral("err"));
            }
        });
    }
signals:
    void ready(const crypto::RsaResult& r);

private:
    QComboBox* m_bits;
    QPushButton* m_btn;
    QPlainTextEdit* m_priv;
    QPlainTextEdit* m_pub;
    QLabel* m_chip = nullptr;
};

} // namespace

namespace pages {
ToolPage* createHash() { return new HashPage; }
ToolPage* createFileHash() { return new FileHashPage; }
ToolPage* createAes() { return new AesPage; }
ToolPage* createRsa() { return new RsaPage; }
} // namespace pages

#include "cryptoolspages.moc"
