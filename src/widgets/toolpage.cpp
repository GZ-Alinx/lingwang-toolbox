#include "toolpage.h"
#include "flowlayout.h"
#include "core/async.h"
#include "iconprovider.h"
#include <QFrame>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QStyle>
#include <QFile>
#include <QGraphicsDropShadowEffect>

namespace ui {

void addShadow(QWidget* w, int blur, int alpha, int yoff) {
    auto* eff = new QGraphicsDropShadowEffect(w);
    eff->setBlurRadius(blur);
    eff->setOffset(0, yoff);
    eff->setColor(QColor(8, 12, 24, alpha));
    w->setGraphicsEffect(eff);
}

QFrame* card(const QString& title, QWidget* inner, QWidget* headerExtra, bool stretchInner) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    auto* lay = new QVBoxLayout(frame);
    lay->setContentsMargins(14, 10, 14, 14);
    lay->setSpacing(8);
    if (!title.isEmpty() || headerExtra) {
        auto* header = new QHBoxLayout;
        auto* lbl = new QLabel(title);
        lbl->setObjectName(QStringLiteral("cardTitle"));
        header->addWidget(lbl);
        header->addStretch();
        if (headerExtra) header->addWidget(headerExtra);
        lay->addLayout(header);
    }
    if (inner) lay->addWidget(inner, stretchInner ? 1 : 0);
    addShadow(frame, 24, 55, 4);
    return frame;
}

QPushButton* button(const QString& text, const char* objectName) {
    auto* b = new QPushButton(text);
    if (objectName && *objectName) b->setObjectName(QString::fromLatin1(objectName));
    return b;
}

QLabel* chip() {
    auto* lbl = new QLabel;
    lbl->setObjectName(QStringLiteral("statusChip"));
    lbl->hide();
    return lbl;
}

void setChip(QLabel* chip, const QString& text, const QString& state) {
    if (!chip) return;
    chip->setText(text);
    chip->setProperty("state", state);
    chip->style()->unpolish(chip);
    chip->style()->polish(chip);
    chip->show();
}

void copyText(const QString& text) {
    QApplication::clipboard()->setText(text);
}

QWidget* formRow(const QString& label, QWidget* w) {
    auto* row = new QWidget;
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    auto* lbl = new QLabel(label);
    lbl->setObjectName(QStringLiteral("fieldLabel"));
    lay->addWidget(lbl);
    lay->addWidget(w, 1);
    return row;
}

} // namespace ui

// ---------------- ToolPage ----------------
ToolPage::ToolPage(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 18, 24, 24);
    outer->setSpacing(12);
    m_body = outer;
}

void ToolPage::setMeta(const QString& icon, const QString& title, const QString& desc) {
    m_headerIconName = icon;
    auto* header = new QHBoxLayout;
    m_headerIcon = new QLabel;
    m_headerIcon->setFixedSize(42, 42);
    m_headerIcon->setAlignment(Qt::AlignCenter);
    m_headerIcon->setPixmap(Icons::pixmap(icon, QColor(0x4F, 0x8C, 0xFF), 22));
    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName(QStringLiteral("pageTitle"));
    auto* subLbl = new QLabel(desc);
    subLbl->setObjectName(QStringLiteral("pageDesc"));
    header->addWidget(m_headerIcon);
    header->addSpacing(4);
    header->addWidget(titleLbl);
    header->addSpacing(10);
    header->addWidget(subLbl, 1);
    m_body->insertLayout(0, header);   // 子类构造时基类内容已就位，页头始终插到最前
}

void ToolPage::setHeaderAccent(const QColor& color) {
    if (!m_headerIcon) return;
    m_headerIcon->setStyleSheet(QStringLiteral(
        "background: rgba(%1, %2, %3, 0.15); border-radius: 11px;")
        .arg(color.red()).arg(color.green()).arg(color.blue()));
    m_headerIcon->setPixmap(Icons::pixmap(m_headerIconName, color, 22));
}

QLabel* ToolPage::addChipTo(QWidget* host) {
    auto* chip = ui::chip();
    if (auto* lay = qobject_cast<QHBoxLayout*>(host->layout()))
        lay->insertWidget(0, chip);
    return chip;
}

// ---------------- TextToolPage ----------------
TextToolPage::TextToolPage(const QList<Action>& actions, bool live, bool hasInput)
    : m_hasInput(hasInput) {
    m_actions = actions;
    m_input = new QPlainTextEdit;
    m_input->setObjectName(QStringLiteral("mono"));
    m_input->setPlaceholderText(QStringLiteral("输入内容…"));
    m_input->setFixedHeight(150);

    // 选项行（初始隐藏，addOptionWidget 时显示）
    auto* options = new QWidget;
    m_optionsLay = new QHBoxLayout(options);
    m_optionsLay->setContentsMargins(0, 0, 0, 0);
    m_optionsLay->setSpacing(10);
    options->hide();

    if (m_hasInput) {
        auto* inputInner = new QWidget;
        auto* inLay = new QVBoxLayout(inputInner);
        inLay->setContentsMargins(0, 0, 0, 0);
        inLay->setSpacing(8);
        inLay->addWidget(options);
        inLay->addWidget(m_input);
        body()->addWidget(ui::card(QStringLiteral("输入"), inputInner));
    } else {
        body()->addWidget(options);   // 纯生成器：没有输入概念，选项直接放页顶
    }

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    for (const Action& a : actions) {
        auto* b = ui::button(a.label, a.primary ? "primary" : "");
        connect(b, &QPushButton::clicked, this, [this, a] { execute(a.id); });
        btnRow->addWidget(b);
    }
    btnRow->addStretch();
    m_chip = ui::chip();
    btnRow->addWidget(m_chip);
    auto* clearBtn = ui::button(QStringLiteral("清空"));
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_input->clear();
        m_output->clear();
        m_chip->hide();
    });
    btnRow->addWidget(clearBtn);
    m_swapBtn = ui::button(QStringLiteral("输出⇒输入"));
    connect(m_swapBtn, &QPushButton::clicked, this, [this] {
        if (!m_output->toPlainText().isEmpty()) {
            m_input->setPlainText(m_output->toPlainText());
            m_output->clear();
        }
    });
    btnRow->addWidget(m_swapBtn);
    if (!m_hasInput)
        m_swapBtn->hide();   // 没有输入就没有“回输”
    body()->addLayout(btnRow);

    // 输出卡片（带复制按钮）
    m_output = new QPlainTextEdit;
    m_output->setObjectName(QStringLiteral("mono"));
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("结果将显示在这里…"));
    auto* copyBtn = ui::button(QStringLiteral("复制"));
    connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
    body()->addWidget(ui::card(QStringLiteral("输出"), m_output, copyBtn, /*stretchInner*/ true), 1);

    if (live) {
        m_debounce = new QTimer(this);
        m_debounce->setSingleShot(true);
        m_debounce->setInterval(300);
        connect(m_debounce, &QTimer::timeout, this, [this] { execute(m_actions.first().id, true); });
        connect(m_input, &QPlainTextEdit::textChanged, this, [this] {
            m_chip->hide();
            m_debounce->start();
        });
        // 选项变化也触发实时刷新
        for (auto* combo : this->findChildren<QComboBox*>())
            connect(combo, &QComboBox::currentIndexChanged, this, [this] { if (m_debounce) m_debounce->start(); });
    }
}

void TextToolPage::addOptionWidget(QWidget* w) {
    m_optionsLay->addWidget(w);
    m_optionsLay->addStretch();
    QWidget* options = m_optionsLay->parentWidget();
    if (options) options->show();
}

void TextToolPage::execute(const QString& actionId, bool fromLive) {
    if (m_hasInput && m_input->toPlainText().isEmpty()) {
        if (fromLive) {
            // 实时模式下清空输入不应弹提示，安静地清空结果
            m_output->clear();
            m_chip->hide();
        } else {
            ui::setChip(m_chip, QStringLiteral("请输入内容"), QStringLiteral("info"));
        }
        return;
    }
    try {
        run(actionId);
    } catch (const std::exception& e) {
        showError(QString::fromUtf8(e.what()));
    }
}

void TextToolPage::showResult(const QString& text, const QString& okMsg) {
    m_output->setPlainText(text);
    if (!okMsg.isEmpty())
        ui::setChip(m_chip, okMsg, QStringLiteral("ok"));
}

void TextToolPage::showError(const QString& err) {
    m_output->setPlainText(QString());
    ui::setChip(m_chip, err, QStringLiteral("err"));
}

// ---------------- StreamPage ----------------
StreamPage::StreamPage() {
    QWidget* params = new QWidget;
    m_params = new QVBoxLayout(params);
    m_params->setContentsMargins(0, 0, 0, 0);
    m_params->setSpacing(8);
    body()->addWidget(ui::card(QStringLiteral("参数"), params));

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    m_startBtn = ui::button(QStringLiteral("开始"), "primary");
    m_stopBtn = ui::button(QStringLiteral("停止"));
    m_stopBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    m_chip = ui::chip();
    btnRow->addWidget(m_chip);
    body()->addLayout(btnRow);

    m_log = new QPlainTextEdit;
    m_log->setObjectName(QStringLiteral("mono"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    body()->addWidget(ui::card(QStringLiteral("结果"), m_log, nullptr, true), 1);

    // 跨线程日志与状态：工作线程 emit → 队列投递到主线程
    connect(this, &StreamPage::logLine, this, [this](const QString& line) { m_log->appendPlainText(line); },
            Qt::QueuedConnection);
    connect(this, &StreamPage::jobStatus, this,
            [this](const QString& text, const QString& state) { ui::setChip(m_chip, text, state); },
            Qt::QueuedConnection);

    connect(m_startBtn, &QPushButton::clicked, this, [this] { onStart(); });
    connect(m_stopBtn, &QPushButton::clicked, this, [this] { stopJob(); });
}

StreamPage::~StreamPage() {
    if (m_thread) {
        m_thread->cancel();
        m_thread->wait(3000);
    }
}

void StreamPage::appendLog(const QString& line) {
    m_log->appendPlainText(line);
}

void StreamPage::setRunning(bool on) {
    m_startBtn->setEnabled(!on);
    m_stopBtn->setEnabled(on);
}

void StreamPage::beginJob(JobThread::Fn fn) {
    if (m_thread) return;
    m_log->clear();
    m_chip->hide();
    setRunning(true);
    m_thread = new JobThread(std::move(fn), this);
    connect(m_thread, &JobThread::finished, this, [this] {
        m_thread->deleteLater();
        m_thread = nullptr;
        setRunning(false);
        ui::setChip(m_chip, QStringLiteral("完成"), QStringLiteral("ok"));
    });
    m_thread->start();
}

void StreamPage::stopJob() {
    if (m_thread) m_thread->cancel();
}
