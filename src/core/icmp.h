#pragma once
#include <QString>
#include <QStringList>

// 跨平台 ICMP：Windows 用 iphlpapi IcmpSendEcho（免管理员），macOS 用 SOCK_DGRAM ICMP（免提权）；
// 原生方式不可用时自动回退到系统命令（ping/tracert）解析。
namespace neticmp {

struct PingReply {
    bool success = false;
    QString ip;          // 应答来源
    double ms = 0;       // 往返毫秒
    int ttl = 0;
    QString error;       // 失败原因（超时/不可达等）
};

// 单次 ping。host 可为域名或 IP。
PingReply pingOnce(const QString& host, int timeoutMs, int ttl = 0, unsigned short seq = 1);

// 路由追踪：onHop(hop序号, ip, ms或-1, 是否到达目标)
using HopCallback = std::function<void(int hop, const QString& ip, double ms, bool reached)>;
// 返回 false 表示该方式完全不可用
bool traceroute(const QString& host, int maxHops, int timeoutMs, const HopCallback& onHop,
                const std::atomic<bool>& cancel);

// 系统命令回退实现（用于原生不可用时的兜底）
void systemPing(const QString& host, int count, int timeoutMs,
                const std::function<void(const QString&)>& onLine, const std::atomic<bool>& cancel);
void systemTraceroute(const QString& host, int maxHops,
                      const std::function<void(const QString&)>& onLine, const std::atomic<bool>& cancel);

} // namespace neticmp
