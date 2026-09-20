#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>
#include <QList>
#include "core/async.h"

class QFrame;
class QTimer;

namespace ui {
// 柔和投影
void addShadow(QWidget* w, int blur, int alpha, int yoff);
// 带标题卡片
QFrame* card(const QString& title, QWidget* inner, QWidget* headerExtra = nullptr, bool stretchInner = false);
// 通用按钮
QPushButton* button(const QString& text, const char* objectName = "");
// 状态标签（state: "ok"|"err"|"info"）
QLabel* chip();
void setChip(QLabel* chip, const QString& text, const QString& state);
// 复制到剪贴板
void copyText(const QString& text);
// 表单行：label + widget
QWidget* formRow(const QString& label, QWidget* w);
} // namespace ui

// 工具页基类：头部（标题+描述）+ 内容区
class ToolPage : public QWidget {
    Q_OBJECT
public:
    ToolPage(QWidget* parent = nullptr);
    void setMeta(const QString& icon, const QString& title, const QString& desc);
    void setHeaderAccent(const QColor& color);   // 用分类色点亮页头图标底色
    QVBoxLayout* body() const { return m_body; }

protected:
    QLabel* addChipTo(QWidget* layoutHost); // 在按钮行创建状态 chip

private:
    QVBoxLayout* m_body;
    QLabel* m_headerIcon = nullptr;
    QString m_headerIconName;
};

// 「输入 → 处理 → 输出」通用页
class TextToolPage : public ToolPage {
    Q_OBJECT
public:
    struct Action {
        QString label;
        QString id;
        bool primary = false;
    };
    // hasInput=false：纯生成器工具（随机字符串/UUID 等），隐藏输入区，不校验输入
    TextToolPage(const QList<Action>& actions, bool live = false, bool hasInput = true);
    QPlainTextEdit* inputEdit() const { return m_input; }
    QPlainTextEdit* outputEdit() const { return m_output; }
    void addOptionWidget(QWidget* w);           // 输入卡片内的一行选项
    void setCopyOutputEnabled(bool on);
    QLabel* statusChip() const { return m_chip; }
    void showResult(const QString& text, const QString& okMsg = QString());
    void showError(const QString& err);

protected:
    virtual void run(const QString& actionId) = 0;

protected:
    void execute(const QString& actionId, bool fromLive = false);

    QPlainTextEdit* m_input;
    QPlainTextEdit* m_output;
    QHBoxLayout* m_optionsLay = nullptr;
    QPushButton* m_swapBtn = nullptr;
    bool m_hasInput = true;
    QLabel* m_chip = nullptr;
    QList<Action> m_actions;
    QTimer* m_debounce = nullptr;
};

// 「参数 → 启动 → 流式日志」通用页（ping/路由追踪/端口扫描等）
class StreamPage : public ToolPage {
    Q_OBJECT
public:
    StreamPage();
    ~StreamPage() override;
    bool running() const { return m_thread != nullptr; }

signals:
    // 工作线程通过 emit 这些信号安全地更新界面（自动 Queued）
    void logLine(const QString& line);
    void jobStatus(const QString& text, const QString& state);

protected:
    QVBoxLayout* paramsLayout() const { return m_params; }
    QPlainTextEdit* logEdit() const { return m_log; }
    QLabel* statusChip() const { return m_chip; }
    void appendLog(const QString& line);
    void beginJob(JobThread::Fn fn);  // 启动任务并管理按钮状态
    void stopJob();
    virtual void onStart() = 0;        // 点击开始（应调用 beginJob）
    void setRunning(bool on);

private:
    QVBoxLayout* m_params;
    QPlainTextEdit* m_log;
    QLabel* m_chip = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    JobThread* m_thread = nullptr;
};
