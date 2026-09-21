#include "highlighters.h"
#include <QList>
#include <QColor>

namespace {

struct Palette {
    QColor key, str, num, boolean, punct, ok, err, dim, accent;
    QColor diffAddBg, diffDelBg;
};

Palette pal() {
    if (hl::isDark()) {
        return {QColor(0x7C, 0xB7, 0xFF), QColor(0xE8, 0xB3, 0x68), QColor(0x56, 0xD3, 0x6A),
                QColor(0xD2, 0xA8, 0xFF), QColor(0x8B, 0x93, 0xA1), QColor(0x4C, 0xC5, 0x65),
                QColor(0xF5, 0x6B, 0x63), QColor(0x6E, 0x76, 0x81), QColor(0x6C, 0xA8, 0xFF),
                QColor(63, 185, 80, 34), QColor(248, 81, 73, 34)};
    }
    return {QColor(0x05, 0x50, 0xAE), QColor(0x95, 0x38, 0x00), QColor(0x11, 0x63, 0x29),
            QColor(0x82, 0x50, 0xDF), QColor(0x6E, 0x77, 0x81), QColor(0x1A, 0x7F, 0x37),
            QColor(0xCF, 0x22, 0x2E), QColor(0x8C, 0x95, 0x9F), QColor(0x25, 0x63, 0xEB),
            QColor(57, 200, 108, 42), QColor(255, 129, 130, 42)};
}

QList<QPointer<ThemedHighlighter>>& registry() {
    static QList<QPointer<ThemedHighlighter>> list;
    return list;
}

bool g_dark = true;

} // namespace

namespace hl {

bool isDark() { return g_dark; }

void setDark(bool dark) { g_dark = dark; }

void rehighlightAll() {
    for (const auto& h : registry())
        if (h) h->rehighlightThemed();
    // 清理失效指针
    registry().erase(std::remove_if(registry().begin(), registry().end(),
                                    [](const QPointer<ThemedHighlighter>& p) { return p.isNull(); }),
                     registry().end());
}

} // namespace hl

// ---------------- ThemedHighlighter ----------------
// 注意：基类构造期间不得调用纯虚 rebuildRules()——此时对象尚未成为派生类，
// 虚分派会落入纯虚槽：macOS clang/libc++ 直接 abort（闪退），
// Windows MinGW -O3 去虚化侥幸直调空定义。初始规则由各派生类构造体内 rebuild() 建立。
ThemedHighlighter::ThemedHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {
    registry().append(QPointer<ThemedHighlighter>(this));
}

ThemedHighlighter::~ThemedHighlighter() = default;

// 纯虚函数的定义（派生类各自覆写；MinGW 链接器需要此符号）
void ThemedHighlighter::rebuildRules() { }

void ThemedHighlighter::highlightBlock(const QString& text) {
    for (int i = 0; i < m_patterns.size(); ++i) {
        auto it = m_patterns[i].globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart()),
                      static_cast<int>(m.capturedLength()), m_formats[i]);
        }
    }
}

void ThemedHighlighter::rehighlightThemed() {
    rebuildRules();
}

// ---------------- CodeHighlighter ----------------
void CodeHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    auto add = [this](const QString& pattern, const QColor& color, bool bold = false) {
        QRegularExpression re(pattern);
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        m_patterns.append(re);
        m_formats.append(fmt);
    };

    // 顺序即优先级：后写入的规则覆盖先前的重叠区域（注释最后，保证注释内容不被二次着色）
    add(QStringLiteral("'[^'\\n]*'|\"[^\"]*\""), p.str);                              // 字符串
    add(QStringLiteral("\\b(?i)(select|from|where|and|or|not|null|like|in|between|as|on|left|right|inner|outer|join|group|order|by|asc|desc|limit|offset|insert|into|values|update|set|delete|distinct|union|all|having|case|when|then|else|end|is|exists|count|sum|avg|max|min)\\b"),
        p.key, true);                                                                  // SQL 关键字
    add(QStringLiteral("(?<=^|[\\s,{}\\[\\]])(\"(?:[^\"\\\\]|\\\\.)*\")(?=\\s*:)"), p.key); // JSON 键
    add(QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\""), p.str);                           // JSON 字符串
    add(QStringLiteral("\\b(?:0x[0-9a-fA-F]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b"), p.num); // 数字
    add(QStringLiteral("\\b(?:true|false|null|True|False|None)\\b"), p.boolean);       // 布尔/空
    add(QStringLiteral("(?<=[{,])[A-Za-z_][\\w.-]*(?=\\s*:)"), p.key);              // flow map 键 {cpu: 100m}
    add(QStringLiteral("^[ \\t]*-[ \\t]"), p.accent);                                  // YAML 列表
    add(QStringLiteral("^[ \\t]*[^:#\\n]{1,40}(?=:)"), p.key);                         // YAML 键
    add(QStringLiteral("--[^\\n]*|#[^\\n]*"), p.dim);                                 // SQL/YAML 注释（最后）
    rehighlight();
}

// ---------------- YamlHighlighter ----------------
// 配色对标 VS Code Dark+ 的 YAML 视觉（键亮绿 / 数字与量值蓝 / 注释绿灰 / 字符串橙）
void YamlHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();

    const bool dark = hl::isDark();
    // VS Code 风格调色
    const QColor keyC   = dark ? QColor(0x7E, 0xE7, 0x87) : QColor(0x0F, 0x7B, 0x3F);   // 键：亮绿
    const QColor numC   = dark ? QColor(0x56, 0x9C, 0xD6) : QColor(0x0B, 0x69, 0xA3);   // 数字/量值：蓝
    const QColor comC   = dark ? QColor(0x6A, 0x99, 0x55) : QColor(0x54, 0x7A, 0x33);   // 注释：绿灰
    const QColor strC   = dark ? QColor(0xCE, 0x91, 0x78) : QColor(0xA3, 0x15, 0x15);   // 引号串：橙
    const QColor boolC  = dark ? QColor(0x56, 0x9C, 0xD6) : QColor(0x0B, 0x69, 0xA3);   // 布尔：蓝
    const QColor dashC  = dark ? QColor(0xE8, 0xB3, 0x68) : QColor(0x8F, 0x5A, 0x00);   // 列表符：琥珀
    const QColor docC   = dark ? QColor(0x80, 0x80, 0x80) : QColor(0x8C, 0x8C, 0x8C);   // 文档分隔

    auto add = [this](const QString& pattern, const QColor& color, bool bold = false) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        m_patterns.append(QRegularExpression(pattern));
        m_formats.append(fmt);
    };

    add(QStringLiteral("'[^']*'|\"[^\"]*\""), strC);                                  // 引号串
    add(QStringLiteral("(?<=[{,])[A-Za-z_][\\w.-]*(?=\\s*:)"), keyC);                 // flow 键 {cpu: ..}
    add(QStringLiteral("^[ \\t]*-?[ \\t]*\\K[^\\s:#{}\\[\\]][^:\\n]{0,60}(?=:(\\s|$))"), keyC, true);  // 键（含 "- name:"；\K 跳过缩进，只着色键名）
    add(QStringLiteral("\\b(?:true|false|null|True|False|yes|no|on|off)\\b"), boolC);            // 布尔
    add(QStringLiteral("\\b\\d+(?:\\.\\d+)?(?:m|Mi|Gi|Ki|MB|GB|KB|ms|s|m|h)?\\b"), numC);  // 数字与带单位量值
    add(QStringLiteral("^[ \\t]*-[ \\t]+"), dashC);                                   // 列表符
    add(QStringLiteral("(?<=^|\\s)---(?=\\s|$)"), docC);                              // 文档分隔
    add(QStringLiteral("[&*][A-Za-z_]\\w*"), dashC);                                  // anchor/alias
    add(QStringLiteral("#[^\\n]*"), comC);                                            // 注释（放最后：注释里的数字/引号/键不得被重新着色）
    rehighlight();
}

// ---------------- ShellHighlighter ----------------
void ShellHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    auto add = [this](const QString& pattern, const QColor& color, bool bold = false) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        m_patterns.append(QRegularExpression(pattern));
        m_formats.append(fmt);
    };

    add(QStringLiteral("'[^']*'|\"[^\"]*\""), p.str);                        // 引号串
    add(QStringLiteral("(?<=\\s)--?[A-Za-z][\\w-]*"), p.accent);             // -n / --context 等 flag
    add(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), p.num);                // 数字
    add(QStringLiteral("^kubectl\\b"), p.key, true);                         // kubectl
    add(QStringLiteral("\\b(pods?|deployments?|services?|ingresses?|configmaps?|secrets?|namespaces?|nodes?|serviceaccounts?|clusterroles?|clusterrolebindings?|roles?|rolebindings?|statefulsets?|daemonsets?|jobs|cronjobs|replicasets?|horizontalpodautoscalers?|customresourcedefinitions?|persistentvolumeclaims?|all)\\b"),
        p.boolean, true);                                                     // 资源类型
    add(QStringLiteral("#[^\\n]*"), p.dim);                                  // 注释（最后）
    rehighlight();
}

// ---------------- LogHighlighter ----------------
void LogHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    auto add = [this](const QString& pattern, const QColor& color, bool bold = false) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        m_patterns.append(QRegularExpression(pattern));
        m_formats.append(fmt);
    };

    add(QStringLiteral("✓.*|已到达|成功"), p.ok, true);
    add(QStringLiteral("✗.*|(?:失败|错误|超时|不可达|无效|已过期).*"), p.err);
    add(QStringLiteral("——.*|^追踪完成|^扫描完成|^解析完成|^查询中"), p.dim);
    add(QStringLiteral("\\b\\d+(?:\\.\\d+)?ms\\b"), p.accent);
    rehighlight();
}

// ---------------- DiffHighlighter ----------------
void DiffHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    QTextCharFormat addFmt;
    addFmt.setForeground(p.ok);
    addFmt.setBackground(p.diffAddBg);
    m_patterns.append(QRegularExpression(QStringLiteral("^\\+.*")));
    m_formats.append(addFmt);

    QTextCharFormat delFmt;
    delFmt.setForeground(p.err);
    delFmt.setBackground(p.diffDelBg);
    m_patterns.append(QRegularExpression(QStringLiteral("^-.*")));
    m_formats.append(delFmt);

    QTextCharFormat dimFmt;
    dimFmt.setForeground(p.dim);
    m_patterns.append(QRegularExpression(QStringLiteral("^  .*|^（无差异）.*")));
    m_formats.append(dimFmt);
    rehighlight();
}

// ---------------- KeyValueHighlighter ----------------
void KeyValueHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    QTextCharFormat labelFmt;
    labelFmt.setForeground(p.dim);
    m_patterns.append(QRegularExpression(QStringLiteral("^[ \\t]*[^:\\n]{1,28}(?=:)")));
    m_formats.append(labelFmt);

    QTextCharFormat arrowFmt;
    arrowFmt.setForeground(p.accent);
    m_patterns.append(QRegularExpression(QStringLiteral("→[^\\n]*")));
    m_formats.append(arrowFmt);
    rehighlight();
}
