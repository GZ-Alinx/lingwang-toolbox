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
#include <QFile>

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



// ---------- SSL 证书查看 ----------
class CertPage final : public ToolPage {
    Q_OBJECT
public:
    CertPage() {
        ToolPage::setMeta(QStringLiteral("lock"), QStringLiteral("SSL 证书查看"),
                          QStringLiteral("解析 PEM 证书：主题/颁发者/有效期/签名算法/SAN，到期预警"));
        auto* top = new QWidget;
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(8);
        auto* pick = ui::button(QStringLiteral("打开证书文件…"), "primary");
        connect(pick, &QPushButton::clicked, this, [this] {
            QString p = QFileDialog::getOpenFileName(this, QStringLiteral("选择证书"),
                                                     QString(), QStringLiteral("证书 (*.crt *.pem *.cer);;所有文件 (*)"));
            if (p.isEmpty()) return;
            QFile f(p);
            if (f.open(QIODevice::ReadOnly)) {
                m_input->setPlainText(QString::fromUtf8(f.readAll()));
                parse();
            }
        });
        auto* parseBtn = ui::button(QStringLiteral("解析"));
        connect(parseBtn, &QPushButton::clicked, this, [this] { parse(); });
        auto* clearBtn = ui::button(QStringLiteral("清空"));
        connect(clearBtn, &QPushButton::clicked, this, [this] { m_input->clear(); m_out->clear(); m_chip->hide(); });
        tl->addWidget(pick);
        tl->addWidget(parseBtn);
        tl->addWidget(clearBtn);
        tl->addStretch();
        m_chip = ui::chip();
        tl->addWidget(m_chip);
        body()->addWidget(ui::card(QStringLiteral("证书（粘贴 PEM 或打开文件）"), top));

        m_input = new QPlainTextEdit;
        m_input->setObjectName(QStringLiteral("mono"));
        m_input->setPlaceholderText(QStringLiteral("粘贴 PEM 证书（-----BEGIN CERTIFICATE----- 开头）…"));
        m_input->setFixedHeight(140);
        body()->addWidget(ui::card(QStringLiteral("PEM 内容"), m_input));

        m_out = new QPlainTextEdit;
        m_out->setObjectName(QStringLiteral("mono"));
        m_out->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_out->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("解析结果"), m_out, copyBtn, true), 1);
    }
private:
    void parse() {
        const QByteArray pem = m_input->toPlainText().toUtf8();
        if (pem.trimmed().isEmpty()) { ui::setChip(m_chip, QStringLiteral("请先粘贴证书"), QStringLiteral("info")); return; }
        crypto::CertInfo c = crypto::certParse(pem);
        if (!c.ok) {
            m_out->clear();
            ui::setChip(m_chip, c.error, QStringLiteral("err"));
            return;
        }
        QStringList out;
        out << QStringLiteral("主题 Subject:    %1").arg(c.subject);
        out << QStringLiteral("颁发者 Issuer:    %1").arg(c.issuer);
        out << QStringLiteral("序列号 Serial:    %1").arg(c.serial);
        out << QStringLiteral("签名算法:        %1").arg(c.sigAlg);
        out << QStringLiteral("公钥:            %1").arg(c.pubkey);
        out << QStringLiteral("生效时间:        %1 UTC").arg(c.notBefore.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        out << QStringLiteral("到期时间:        %1 UTC").arg(c.notAfter.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        if (!c.san.isEmpty())
            out << QStringLiteral("SAN 域名:        %1").arg(c.san.join(QStringLiteral(", ")));
        m_out->setPlainText(out.join(QLatin1Char('\n')));
        if (c.daysLeft < 0)
            ui::setChip(m_chip, QStringLiteral("✗ 已过期 %1 天").arg(-c.daysLeft), QStringLiteral("err"));
        else if (c.daysLeft <= 7)
            ui::setChip(m_chip, QStringLiteral("⚠ 仅剩 %1 天，请尽快更换！").arg(c.daysLeft), QStringLiteral("err"));
        else if (c.daysLeft <= 30)
            ui::setChip(m_chip, QStringLiteral("⚠ 剩余 %1 天，建议规划更换").arg(c.daysLeft), QStringLiteral("info"));
        else
            ui::setChip(m_chip, QStringLiteral("✓ 有效，剩余 %1 天").arg(c.daysLeft), QStringLiteral("ok"));
    }
    QPlainTextEdit* m_input = nullptr;
    QPlainTextEdit* m_out = nullptr;
    QLabel* m_chip = nullptr;
};

} // namespace

namespace pages {
ToolPage* createHash() { return new HashPage; }
ToolPage* createFileHash() { return new FileHashPage; }
ToolPage* createAes() { return new AesPage; }
ToolPage* createRsa() { return new RsaPage; }
ToolPage* createCert() { return new CertPage; }
} // namespace pages

#include "cryptoolspages.moc"
