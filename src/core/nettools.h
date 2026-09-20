#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <atomic>
#include <functional>

namespace nettools {

// ---------- 子网计算 ----------
struct SubnetInfo {
    bool ok = false;
    QString error;
    QString network, broadcast, mask, wildcard;
    QString firstHost, lastHost;
    QString hostCount;        // 可用主机数
    QString totalAddresses;
    QString ipClass;          // A/B/C/D/E/回环
    QString ipType;           // 公网/私网
    QString binaryIp, binaryMask;
    int prefix = 0;
};
SubnetInfo subnetCalc(const QString& ipText, const QString& maskOrPrefix);

// ---------- 端口扫描 ----------
struct PortResult {
    int port = 0;
    bool open = false;
    QString service;
};
// 工作在调用线程（应为后台线程），通过 onResult 汇报（可跨线程 emit）
void portScan(const QString& host, const QList<int>& ports, int timeoutMs, int concurrency,
              const std::function<void(const PortResult&)>& onResult,
              const std::function<void(int, int)>& onProgress,
              const std::atomic<bool>& cancel);

QList<int> parsePortSpec(const QString& spec, QString* err);
QString serviceName(int port);

// ---------- Whois ----------
struct WhoisResult { bool ok = false; QString error, text; };
WhoisResult whoisQuery(const QString& domain); // 阻塞，须在后台线程调用

// ---------- 通用 ----------
QStringList localIPv4List();

} // namespace nettools
