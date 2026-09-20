#pragma once
#include <QThread>
#include <atomic>
#include <functional>

// 后台任务线程：构造时传入工作函数，函数内通过 cancel 引擎感知取消请求。
// 工作函数中可以直接 emit 页面信号（跨线程自动走 Queued 连接）。
class JobThread : public QThread {
public:
    using Fn = std::function<void(const std::atomic<bool>& cancel)>;
    explicit JobThread(Fn fn, QObject* parent = nullptr)
        : QThread(parent), m_fn(std::move(fn)) {}
    void run() override {
        if (m_fn) m_fn(m_cancel);
    }
    void cancel() { m_cancel = true; }

protected:
    std::atomic<bool> m_cancel{false};
    Fn m_fn;
};
