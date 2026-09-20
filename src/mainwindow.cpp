#include "mainwindow.h"
#include "registry.h"
#include "iconprovider.h"
#include "widgets/toolpage.h"
#include "widgets/flowlayout.h"
#include "widgets/collapsiblesection.h"
#include "widgets/highlighters.h"

#include <QWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QScrollArea>
#include <QShortcut>
#include <QSettings>
#include <QMessageBox>
#include <QStyle>
#include <QMouseEvent>
#include <QFile>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <algorithm>

MainWindow::MainWindow() {
    setWindowTitle(QStringLiteral("灵王工具箱 v%1").arg(QCoreApplication::applicationVersion()));
    resize(1280, 880);
    // 锁定下限为内容完整显示所需尺寸：窗口不可再缩小，杜绝挤压/滚动问题；
    // 仍可放大与最大化（放大只会更宽松），并兼容高分屏缩放
    setMinimumSize(1100, 780);
    setWindowIcon(Icons::pixmap(QStringLiteral("zap"), QColor(0x4F, 0x8C, 0xFF), 256));

    // 主题
    QSettings s;
    m_dark = s.value(QStringLiteral("theme/dark"), true).toBool();
    applyTheme(m_dark);

    // 顶层：顶栏 + 主体
    auto* central = new QWidget;
    auto* rootLay = new QVBoxLayout(central);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ---- 顶栏 ----
    auto* topbar = new QWidget;
    topbar->setObjectName(QStringLiteral("topbar"));
    topbar->setFixedHeight(52);
    auto* topLay = new QHBoxLayout(topbar);
    topLay->setContentsMargins(16, 0, 16, 0);
    topLay->setSpacing(10);

    auto* logo = new QLabel;
    logo->setPixmap(Icons::pixmap(QStringLiteral("zap"), QColor(0x4F, 0x8C, 0xFF), 22));
    auto* logoText = new QLabel(QStringLiteral("灵王工具箱"));
    logoText->setObjectName(QStringLiteral("logoText"));
    topLay->addWidget(logo);
    topLay->addWidget(logoText);
    topLay->addSpacing(6);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("搜索工具（Ctrl+K）"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(300);
    m_search->addAction(Icons::get(QStringLiteral("search"), QColor(0x9A, 0xA4, 0xB2)), QLineEdit::LeadingPosition);
    topLay->addWidget(m_search);
    topLay->addStretch();

    m_themeBtn = new QToolButton;
    m_themeBtn->setObjectName(QStringLiteral("iconBtn"));
    m_themeBtn->setToolTip(QStringLiteral("切换深色/浅色主题"));
    topLay->addWidget(m_themeBtn);

    auto* aboutBtn = new QToolButton;
    aboutBtn->setObjectName(QStringLiteral("iconBtn"));
    aboutBtn->setIcon(Icons::get(QStringLiteral("info"), QColor(0x9A, 0xA4, 0xB2)));
    aboutBtn->setToolTip(QStringLiteral("关于"));
    topLay->addWidget(aboutBtn);
    rootLay->addWidget(topbar);

    // ---- 主体：侧边栏 + 内容 ----
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    m_sidebar = makeSidebar();
    body->addWidget(m_sidebar);

    m_stack = new QStackedWidget;
    m_home = makeHome();
    m_stack->addWidget(m_home);
    body->addWidget(m_stack, 1);
    rootLay->addLayout(body, 1);
    setCentralWidget(central);

    // 信号
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::applySearch);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        for (const SideEntry& e : m_sideEntries) {
            if (e.btn && e.btn->isVisible() && !e.toolId.isEmpty()) {
                openTool(e.toolId);
                return;
            }
        }
    });
    auto* sc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    connect(sc, &QShortcut::activated, this, [this] { m_search->setFocus(); m_search->selectAll(); });
    connect(m_themeBtn, &QToolButton::clicked, this, &MainWindow::toggleTheme);
    connect(aboutBtn, &QToolButton::clicked, this, &MainWindow::showAbout);

    // 主题按钮图标
    m_themeBtn->setIcon(Icons::get(m_dark ? QStringLiteral("sun") : QStringLiteral("moon"), QColor(0x9A, 0xA4, 0xB2)));
}

QWidget* MainWindow::makeSidebar() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("sidebar"));
    scroll->setFixedWidth(226);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(10, 12, 10, 12);
    lay->setSpacing(4);

    // 首页按钮
    auto* homeBtn = new QPushButton(QStringLiteral("  首页"));
    homeBtn->setObjectName(QStringLiteral("navBtn"));
    homeBtn->setIcon(Icons::get(QStringLiteral("home"), QColor(0x9A, 0xA4, 0xB2)));
    homeBtn->setCheckable(true);
    homeBtn->setChecked(true);
    homeBtn->setProperty("toolId", QString());
    lay->addWidget(homeBtn);
    connect(homeBtn, &QPushButton::clicked, this, [this, homeBtn] {
        m_stack->setCurrentWidget(m_home);
        for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) it.value()->setChecked(false);
        homeBtn->setChecked(true);
    });

    auto openHandler = [this](const QString& id, QPushButton* btn) {
        openTool(id);
        for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) it.value()->setChecked(false);
        btn->setChecked(true);
    };

    QSettings st;
    for (const QString& cat : ToolRegistry::categories()) {
        auto* section = new CollapsibleSection(cat);
        m_sections << section;
        m_sectionByCat.insert(cat, section);
        for (const ToolMeta& m : ToolRegistry::all()) {
            if (m.category != cat) continue;
            auto* btn = new QPushButton(QStringLiteral("  ") + m.name);
            btn->setObjectName(QStringLiteral("navBtn"));
            btn->setIcon(Icons::get(m.icon, ToolRegistry::categoryColor(cat)));
            btn->setCheckable(true);
            btn->setToolTip(m.desc);
            section->addButton(btn);
            m_toolButtons.insert(m.id, btn);
            connect(btn, &QPushButton::clicked, this, [openHandler, m, btn] { openHandler(m.id, btn); });

            SideEntry e;
            e.btn = btn;
            e.toolId = m.id;
            e.category = cat;
            m_sideEntries << e;
        }
        lay->addWidget(section);
    }
    // 恢复上次展开状态（默认全部收起）
    for (CollapsibleSection* sec : m_sections) {
        const QString cat = m_sectionByCat.key(sec);
        sec->setExpanded(st.value(QStringLiteral("sidebar/expanded/") + cat, false).toBool(), false);
        connect(sec, &CollapsibleSection::expandedChanged, this, [this, cat](bool on) {
            QSettings s;
            s.setValue(QStringLiteral("sidebar/expanded/") + cat, on);
        });
    }
    lay->addStretch();

    // 版本
    auto* ver = new QLabel(QStringLiteral("v%1").arg(QCoreApplication::applicationVersion()));
    ver->setObjectName(QStringLiteral("sideVersion"));
    ver->setAlignment(Qt::AlignHCenter);
    lay->addWidget(ver);

    scroll->setWidget(content);
    return scroll;
}

// 可点击的首页卡片
class HomeCard : public QFrame {
    Q_OBJECT
public:
    HomeCard(const ToolMeta& m, const QColor& accent, QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("toolCard"));
        setFixedSize(240, 74);
        setCursor(Qt::PointingHandCursor);
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(12, 10, 12, 10);
        auto* icon = new QLabel;
        icon->setPixmap(Icons::pixmap(m.icon, accent, 26));
        lay->addWidget(icon, 0, Qt::AlignTop);
        auto* col = new QVBoxLayout;
        col->setSpacing(3);
        auto* name = new QLabel(m.name);
        name->setObjectName(QStringLiteral("toolCardName"));
        auto* desc = new QLabel(m.desc);
        desc->setObjectName(QStringLiteral("toolCardDesc"));
        desc->setWordWrap(true);
        col->addWidget(name);
        col->addWidget(desc);
        lay->addLayout(col, 1);
        auto* eff = new QGraphicsDropShadowEffect(this);
        eff->setBlurRadius(22);
        eff->setOffset(0, 3);
        eff->setColor(QColor(8, 12, 24, 70));
        setGraphicsEffect(eff);
    }
    void mousePressEvent(QMouseEvent*) override { emit clicked(); }
signals:
    void clicked();
};

QWidget* MainWindow::makeHome() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(4, 4, 16, 16);
    lay->setSpacing(8);

    // Hero 区
    auto* hero = new QWidget;
    auto* heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(8, 10, 8, 12);
    heroLay->setSpacing(14);
    auto* heroIcon = new QLabel;
    heroIcon->setPixmap(Icons::pixmap(QStringLiteral("zap"), QColor(0x4F, 0x8C, 0xFF), 26));
    heroIcon->setStyleSheet(QStringLiteral(
        "background: rgba(79,140,255,0.14); border-radius: 12px;"));
    heroIcon->setFixedSize(50, 50);
    heroIcon->setAlignment(Qt::AlignCenter);
    auto* heroCol = new QVBoxLayout;
    heroCol->setSpacing(3);
    auto* heroTitle = new QLabel(QStringLiteral("灵王工具箱"));
    heroTitle->setObjectName(QStringLiteral("heroTitle"));
    auto* heroSub = new QLabel(QStringLiteral("开发 · 运维 · 网络 — %1 个常用工具，双击即用")
                                   .arg(ToolRegistry::all().size()));
    heroSub->setObjectName(QStringLiteral("heroSub"));
    heroCol->addWidget(heroTitle);
    heroCol->addWidget(heroSub);
    heroLay->addWidget(heroIcon, 0, Qt::AlignVCenter);
    heroLay->addLayout(heroCol);
    heroLay->addStretch();
    lay->addWidget(hero);
    auto* hline = new QFrame;
    hline->setObjectName(QStringLiteral("hline"));
    hline->setFixedHeight(1);
    lay->addWidget(hline);
    lay->addSpacing(8);

    for (const QString& cat : ToolRegistry::categories()) {
        auto* caption = new QLabel;
        caption->setTextFormat(Qt::RichText);
        caption->setText(QStringLiteral("<span style=\"color:%1;\">●</span>&nbsp; %2")
                             .arg(ToolRegistry::categoryColor(cat).name(), cat));
        caption->setObjectName(QStringLiteral("homeCaption"));
        lay->addWidget(caption);
        m_homeCaptions << caption;
        auto* flowWrap = new QWidget;
        auto* flow = new FlowLayout(flowWrap, 0, 10, 10);
        for (const ToolMeta& m : ToolRegistry::all()) {
            if (m.category != cat) continue;
            auto* cardW = new HomeCard(m, ToolRegistry::categoryColor(m.category));
            connect(cardW, &HomeCard::clicked, this, [this, m] { openTool(m.id); });
            flow->addWidget(cardW);
            m_homeCards.insert(m.id, cardW);
        }
        lay->addWidget(flowWrap);
    }

    m_noResult = new QLabel(QStringLiteral("未找到匹配的工具"));
    m_noResult->setObjectName(QStringLiteral("noResult"));
    m_noResult->setAlignment(Qt::AlignCenter);
    m_noResult->hide();
    lay->addWidget(m_noResult);

    lay->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void MainWindow::openTool(const QString& id) {
    const ToolMeta* meta = ToolRegistry::find(id);
    if (!meta) return;
    QWidget* page = m_stack->findChild<QWidget*>(QStringLiteral("tool_") + id, Qt::FindDirectChildrenOnly);
    if (!page) {
        page = meta->create();
        page->setObjectName(QStringLiteral("tool_") + id);
        m_stack->addWidget(page);
    }
    if (auto* tp = qobject_cast<ToolPage*>(page))
        tp->setHeaderAccent(ToolRegistry::categoryColor(meta->category));
    m_stack->setCurrentWidget(page);
    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) it.value()->setChecked(false);
    if (m_toolButtons.contains(id)) m_toolButtons.value(id)->setChecked(true);
    // 展开工具所在分类，便于看到当前位置
    if (CollapsibleSection* sec = m_sectionByCat.value(meta->category, nullptr))
        sec->setExpanded(true, false);
}

void MainWindow::toggleTheme() {
    m_dark = !m_dark;
    applyTheme(m_dark);
    QSettings s;
    s.setValue(QStringLiteral("theme/dark"), m_dark);
    m_themeBtn->setIcon(Icons::get(m_dark ? QStringLiteral("sun") : QStringLiteral("moon"), QColor(0x9A, 0xA4, 0xB2)));
}

void MainWindow::applyTheme(bool dark) {
    hl::setDark(dark);
    QFile f(dark ? QStringLiteral(":/theme/dark.qss") : QStringLiteral(":/theme/light.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    hl::rehighlightAll();   // 所有高亮器按新主题重配色
}

void MainWindow::showAbout() {
    QMessageBox::about(this, QStringLiteral("关于 灵王工具箱"),
                       QStringLiteral("<h3>灵王工具箱 v%1</h3>"
                                      "<p>开发 · 运维 · 网络一站式工具箱</p>"
                                      "<p>基于 Qt %2 构建</p>")
                           .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
}

void MainWindow::applySearch(const QString& text) {
    const QString q = text.trimmed();
    const bool empty = q.isEmpty();
    auto match = [q](const ToolMeta& m) {
        if (q.isEmpty()) return true;
        return m.name.contains(q, Qt::CaseInsensitive) || m.desc.contains(q, Qt::CaseInsensitive) ||
               m.id.contains(q, Qt::CaseInsensitive) ||
               std::any_of(m.keywords.cbegin(), m.keywords.cend(),
                           [&q](const QString& k) { return k.contains(q, Qt::CaseInsensitive); });
    };

    // 首页卡片过滤
    QHash<QString, bool> visible;
    int visCount = 0;
    for (const ToolMeta& m : ToolRegistry::all()) {
        const bool v = match(m);
        visible.insert(m.id, v);
        if (v) ++visCount;
    }

    for (auto it = m_homeCards.begin(); it != m_homeCards.end(); ++it)
        it.value()->setVisible(visible.value(it.key(), true));
    for (QLabel* cap : m_homeCaptions) {
        // 分类标题：任一子项可见才显示
        bool any = false;
        for (const ToolMeta& m : ToolRegistry::all())
            if (m.category == cap->text() && visible.value(m.id)) { any = true; break; }
        cap->setVisible(any);
    }

    // 侧边栏过滤（可折叠分类区块）
    QHash<QString, int> catCount;
    for (const SideEntry& e : m_sideEntries) {
        const ToolMeta* m = ToolRegistry::find(e.toolId);
        bool vis = m && visible.value(m->id, true);
        e.btn->setVisible(vis);
        if (vis) catCount[e.category]++;
    }
    // 进入搜索时记住展开状态，清空搜索后恢复
    if (!empty && !m_inSearch) {
        m_inSearch = true;
        m_savedExpanded.clear();
        for (CollapsibleSection* sec : m_sections)
            m_savedExpanded[m_sectionByCat.key(sec)] = sec->isExpanded();
    } else if (empty && m_inSearch) {
        m_inSearch = false;
    }
    for (CollapsibleSection* sec : m_sections) {
        const QString cat = m_sectionByCat.key(sec);
        sec->setVisible(catCount.value(cat, 0) > 0);
        if (empty)
            sec->setExpanded(m_savedExpanded.value(cat, sec->isExpanded()), false);
        else if (catCount.value(cat, 0) > 0)
            sec->setExpanded(true, false);   // 搜索命中时自动展开
    }
    if (!m_inSearch && !m_savedExpanded.isEmpty())
        m_savedExpanded.clear();

    // 无结果提示
    if (m_noResult) {
        const bool none = !empty && visCount == 0;
                m_noResult->setText(QStringLiteral("未找到与「%1」匹配的工具").arg(q));
        m_noResult->setVisible(none);
    }

    // 输入搜索时切到首页（结果网格）
    if (!empty) {
        m_stack->setCurrentWidget(m_home);
    }
}

#include "mainwindow.moc"
