#include "pages.h"
#include "widgets/toolpage.h"
#include "core/sysinfo.h"
#include "iconprovider.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

namespace {

// ---------- 系统信息 ----------
class SysInfoPage final : public ToolPage {
public:
    SysInfoPage() {
        ToolPage::setMeta(QStringLiteral("cpu"), QStringLiteral("系统信息"),
                          QStringLiteral("操作系统、CPU、内存与应用环境概览"));
        auto* table = new QTableWidget(0, 2, this);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setColumnWidth(0, 180);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setVisible(false);
        body()->addWidget(ui::card(QStringLiteral("环境"), table, nullptr, true), 1);

        sysinfo::InfoList info = sysinfo::collect();
        table->setRowCount(info.size());
        for (int i = 0; i < info.size(); ++i) {
            table->setItem(i, 0, new QTableWidgetItem(info[i].first));
            table->setItem(i, 1, new QTableWidgetItem(info[i].second));
        }
    }
};

// ---------- URL 解析 ----------
class UrlParserPage final : public ToolPage {
public:
    UrlParserPage() {
        ToolPage::setMeta(QStringLiteral("globe"), QStringLiteral("URL 解析"),
                          QStringLiteral("拆解 URL 的协议、主机、路径与参数"));
        m_url = new QLineEdit;
        m_url->setPlaceholderText(QStringLiteral("粘贴完整 URL，实时解析"));
        m_url->setFont(Icons::monoFont(10));
        body()->addWidget(ui::card(QStringLiteral("URL"), m_url));

        m_table = new QTableWidget(0, 2, this);
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setColumnWidth(0, 160);
        m_table->verticalHeader()->setVisible(false);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setAlternatingRowColors(true);
        m_table->horizontalHeader()->setVisible(false);
        body()->addWidget(ui::card(QStringLiteral("组成"), m_table, nullptr, true), 1);

        connect(m_url, &QLineEdit::textChanged, this, &UrlParserPage::parse);
    }
private:
    void parse() {
        m_table->setRowCount(0);
        const QUrl url = QUrl::fromUserInput(m_url->text().trimmed());
        if (m_url->text().trimmed().isEmpty() || url.host().isEmpty() && url.scheme().isEmpty()) return;
        auto add = [this](const QString& k, const QString& v) {
            const int r = m_table->rowCount();
            m_table->insertRow(r);
            m_table->setItem(r, 0, new QTableWidgetItem(k));
            m_table->setItem(r, 1, new QTableWidgetItem(v));
        };
        add(QStringLiteral("协议 scheme"), url.scheme());
        add(QStringLiteral("用户名"), url.userName());
        add(QStringLiteral("密码"), url.password());
        add(QStringLiteral("主机 host"), url.host());
        add(QStringLiteral("端口 port"), url.port() > 0 ? QString::number(url.port()) : QStringLiteral("（默认）"));
        add(QStringLiteral("路径 path"), url.path());
        const QUrlQuery q(url);
        for (const auto& item : q.queryItems(QUrl::FullyDecoded))
            add(QStringLiteral("参数 %1").arg(item.first), item.second);
        if (!url.fragment().isEmpty())
            add(QStringLiteral("锚点 fragment"), url.fragment());
    }
    QLineEdit* m_url;
    QTableWidget* m_table;
};

// ---------- 颜色转换 ----------
class ColorPage final : public ToolPage {
public:
    ColorPage() {
        ToolPage::setMeta(QStringLiteral("grid"), QStringLiteral("颜色转换"),
                          QStringLiteral("HEX / RGB / HSL / CMYK 互转与预览"));
        m_input = new QLineEdit;
        m_input->setPlaceholderText(QStringLiteral("支持 #4F8CFF / rgb(79,140,255) / 79,140,255"));
        m_input->setFont(Icons::monoFont(10));
        connect(m_input, &QLineEdit::textChanged, this, &ColorPage::convert);
        body()->addWidget(ui::card(QStringLiteral("颜色值"), m_input));

        m_swatch = new QLabel;
        m_swatch->setMinimumHeight(70);
        m_swatch->setAlignment(Qt::AlignCenter);
        m_swatch->setStyleSheet(QStringLiteral("background:#4F8CFF; border-radius:8px;"));
        body()->addWidget(ui::card(QStringLiteral("预览"), m_swatch));

        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        m_output->setFixedHeight(150);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("各格式"), m_output, copyBtn));
        body()->addStretch();
    }
private:
    void convert() {
        const QString t = m_input->text().trimmed();
        m_output->clear();
        if (t.isEmpty()) return;
        QColor c;
        QString norm = t;
        if (QRegularExpression(QStringLiteral("^\\d+\\s*,\\s*\\d+\\s*,\\s*\\d+$")).match(norm).hasMatch())
            norm = QStringLiteral("rgb(") + norm + QStringLiteral(")");
        c.setNamedColor(norm.toLower());
        if (!c.isValid() && norm.startsWith(QStringLiteral("rgb"))) {
            static const QRegularExpression re(QStringLiteral("(\\d+)[,\\s]+(\\d+)[,\\s]+(\\d+)"));
            auto m = re.match(norm);
            if (m.hasMatch())
                c = QColor(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
        }
        if (!c.isValid()) {
            m_output->setPlainText(QStringLiteral("无法识别的颜色值"));
            return;
        }
        QString hex = c.name(QColor::HexRgb).toUpper();
        QString hext8 = c.name(QColor::HexArgb).toUpper();
        // RGB -> HSL 手动计算
        const double rf = c.red() / 255.0, gf = c.green() / 255.0, bf = c.blue() / 255.0;
        const double mx = qMax(rf, qMax(gf, bf)), mn = qMin(rf, qMin(gf, bf));
        double h = 0, s = 0;
        double l = (mx + mn) / 2.0;
        if (mx != mn) {
            double d = mx - mn;
            s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
            if (mx == rf) h = (gf - bf) / d + (gf < bf ? 6.0 : 0.0);
            else if (mx == gf) h = (bf - rf) / d + 2.0;
            else h = (rf - gf) / d + 4.0;
            h /= 6.0;
        }
        int hh = qRound(h * 360), ss = qRound(s * 100), ll = qRound(l * 100);
        // CMYK
        double r = rf, g = gf, b = bf;
        qreal k = 1 - qMax(r, qMax(g, b));
        qreal cc = k >= 1 ? 0 : (1 - r - k) / (1 - k);
        qreal mm = k >= 1 ? 0 : (1 - g - k) / (1 - k);
        qreal yy = k >= 1 ? 0 : (1 - b - k) / (1 - k);
        QString out;
        out += QStringLiteral("HEX:   #%1\n").arg(hex.mid(1));
        out += QStringLiteral("HEXA:  #%1\n").arg(hext8.mid(1));
        out += QStringLiteral("RGB:   rgb(%1, %2, %3)\n").arg(c.red()).arg(c.green()).arg(c.blue());
        out += QStringLiteral("HSL:   hsl(%1, %2%, %3%)\n").arg(hh).arg(ss).arg(ll);
        out += QStringLiteral("CMYK:  cmyk(%1%, %2%, %3%, %4%)\n")
                   .arg(qRound(cc * 100)).arg(qRound(mm * 100)).arg(qRound(yy * 100)).arg(qRound(k * 100));
        out += QStringLiteral("亮度:  %1（0 黑 - 255 白）").arg(qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()));
        m_output->setPlainText(out);
        bool dark = c.lightness() < 140;
        m_swatch->setStyleSheet(QStringLiteral("background:%1; border-radius:8px; color:%2;")
                                    .arg(c.name(), dark ? QStringLiteral("white") : QStringLiteral("#222222")));
        m_swatch->setText(hex);
    }
    QLineEdit* m_input;
    QLabel* m_swatch;
    QPlainTextEdit* m_output;
};

} // namespace

namespace pages {
ToolPage* createSysInfo() { return new SysInfoPage; }
ToolPage* createUrlParser() { return new UrlParserPage; }
ToolPage* createColor() { return new ColorPage; }
} // namespace pages
