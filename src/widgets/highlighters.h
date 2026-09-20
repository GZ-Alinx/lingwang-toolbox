#pragma once
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>
#include <QPointer>

namespace hl {
// 主题感知的高亮配色
bool isDark();
void setDark(bool dark);
void rehighlightAll();          // 主题切换后重刷所有活跃高亮器
} // namespace hl

// 主题化高亮器基类：自动注册，主题切换时重建规则并重刷
class ThemedHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit ThemedHighlighter(QTextDocument* doc);
    ~ThemedHighlighter() override;
    void rehighlightThemed();   // 主题切换入口（hl::rehighlightAll 调用）

protected:
    void highlightBlock(const QString& text) override;
    virtual void rebuildRules() = 0;
    void rebuild();              // 清空规则并重刷
    QVector<QRegularExpression> m_patterns;
    QVector<QTextCharFormat> m_formats;
};

// JSON + YAML + SQL 通用代码高亮（挂在所有文本工具的输出区）
class CodeHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};

// 流式日志高亮：✓ 成功绿 / 失败红 / 分隔线灰
class LogHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};

// 文本 Diff 高亮：+ 绿 / - 红
class DiffHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};

// Shell/kubectl 命令高亮（K8s 命令生成器）
class ShellHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};

// 「标签: 值」高亮：标签置灰（IP 查询 / HTTP 响应头 / 哈希输出等）
class KeyValueHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};
