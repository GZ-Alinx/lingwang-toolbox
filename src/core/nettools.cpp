#include "nettools.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace nettools {

// ---------- 子网计算 ----------
// QHostAddress 与 quint32 互转均按数值序（首字节为最高位），直接使用即可
static quint32 ipToU32(const QString& ip, bool* ok) {
    QHostAddress a(ip);
    if (a.isNull()) { if (ok) *ok = false; return 0; }
    quint32 v = a.toIPv4Address();
    if (ok) *ok = v != 0 || ip.trimmed() == QStringLiteral("0.0.0.0");
    return v;
}
static QString u32ToIp(quint32 v) { return QHostAddress(v).toString(); }

SubnetInfo subnetCalc(const QString& ipText, const QString& maskOrPrefix) {
    SubnetInfo info;
    bool ok = false;
    quint32 ip = ipToU32(ipText.trimmed(), &ok);
    if (!ok) { info.error = QStringLiteral("IP 地址无效: %1").arg(ipText); return info; }

    QString m = maskOrPrefix.trimmed();
    int prefix = -1;
    if (m.startsWith(QLatin1Char('/'))) m = m.mid(1);
    if (m.contains(QLatin1Char('.'))) {
        bool mok = false;
        quint32 mask = ipToU32(m, &mok);
        if (!mok) { info.error = QStringLiteral("子网掩码无效: %1").arg(maskOrPrefix); return info; }
        prefix = 0;
        bool inHost = false;
        for (int i = 31; i >= 0; --i) {
            if ((mask >> i) & 1) {
                if (inHost) { info.error = QStringLiteral("子网掩码不连续"); return info; }
                ++prefix;
            } else inHost = true;
        }
    } else {
        bool pok = false;
        prefix = m.toInt(&pok);
        if (!pok || prefix < 0 || prefix > 32) { info.error = QStringLiteral("前缀长度无效（0-32）"); return info; }
    }

    quint32 maskU = prefix == 0 ? 0u : (0xFFFFFFFFu << (32 - prefix));
    quint32 net = ip & maskU;
    quint32 bcast = net | (~maskU);

    info.ok = true;
    info.prefix = prefix;
    info.network = u32ToIp(net);
    info.broadcast = u32ToIp(bcast);
    info.mask = u32ToIp(maskU);
    info.wildcard = u32ToIp(~maskU);

    if (prefix >= 31) {
        info.firstHost = QStringLiteral("-");
        info.lastHost = QStringLiteral("-");
        info.hostCount = prefix == 32 ? QStringLiteral("1（单机）") : QStringLiteral("2（/31 点对点）");
    } else {
        info.firstHost = u32ToIp(net + 1);
        info.lastHost = u32ToIp(bcast - 1);
        quint64 hosts = 1ULL << (32 - prefix);
        info.hostCount = QString::number(hosts - 2);
    }
    quint64 total = 1ULL << (32 - prefix);
    info.totalAddresses = QString::number(total);

    // 分类
    quint32 first = (ip >> 24) & 0xFF;
    if (first == 127) info.ipClass = QStringLiteral("回环");
    else if (first < 128) info.ipClass = QStringLiteral("A 类");
    else if (first < 192) info.ipClass = QStringLiteral("B 类");
    else if (first < 224) info.ipClass = QStringLiteral("C 类");
    else if (first < 240) info.ipClass = QStringLiteral("D 类（组播）");
    else info.ipClass = QStringLiteral("E 类（保留）");

    quint32 second = (ip >> 8) & 0xFFFF;
    bool isPrivate = (first == 10) || (first == 172 && second >= 0x1000 && second <= 0x1FFF) ||
                     (first == 192 && second == 0xC0A8) || (first == 169 && second == 0xA9FE);
    info.ipType = isPrivate ? QStringLiteral("私有/保留地址") : QStringLiteral("公网地址");

    auto bits = [](quint32 v) {
        QString s;
        for (int i = 31; i >= 0; --i) {
            s += ((v >> i) & 1) ? QLatin1Char('1') : QLatin1Char('0');
            if (i % 8 == 0 && i) s += QLatin1Char('.');
        }
        return s;
    };
    info.binaryIp = bits(ip);
    info.binaryMask = bits(maskU);
    return info;
}

// ---------- 端口扫描 ----------
QString serviceName(int port) {
    static const QHash<int, QString> names = {
        {21, QStringLiteral("FTP")}, {22, QStringLiteral("SSH")}, {23, QStringLiteral("Telnet")},
        {25, QStringLiteral("SMTP")}, {53, QStringLiteral("DNS")}, {67, QStringLiteral("DHCP")},
        {80, QStringLiteral("HTTP")}, {110, QStringLiteral("POP3")}, {135, QStringLiteral("RPC")},
        {139, QStringLiteral("NetBIOS")}, {143, QStringLiteral("IMAP")}, {443, QStringLiteral("HTTPS")},
        {445, QStringLiteral("SMB")}, {993, QStringLiteral("IMAPS")}, {995, QStringLiteral("POP3S")},
        {1433, QStringLiteral("SQL Server")}, {1521, QStringLiteral("Oracle")}, {1723, QStringLiteral("PPTP")},
        {3306, QStringLiteral("MySQL")}, {3389, QStringLiteral("RDP")}, {5432, QStringLiteral("PostgreSQL")},
        {5900, QStringLiteral("VNC")}, {6379, QStringLiteral("Redis")}, {8080, QStringLiteral("HTTP-Alt")},
        {8443, QStringLiteral("HTTPS-Alt")}, {8888, QStringLiteral("HTTP-Alt")}, {9090, QStringLiteral("Manage")},
        {9200, QStringLiteral("Elasticsearch")}, {11211, QStringLiteral("Memcached")}, {27017, QStringLiteral("MongoDB")}};
    return names.value(port, QString());
}

QList<int> parsePortSpec(const QString& spec, QString* err) {
    QList<int> ports;
    QString s = spec.trimmed();
    s.remove(QLatin1Char(' '));
    if (s.isEmpty()) { if (err) *err = QStringLiteral("请输入端口"); return ports; }
    for (const QString& part : s.split(QLatin1Char(','))) {
        if (part.isEmpty()) continue;
        if (part.contains(QLatin1Char('-'))) {
            QStringList ab = part.split(QLatin1Char('-'));
            bool ok1 = false, ok2 = false;
            int a = ab.value(0).toInt(&ok1), b = ab.value(1).toInt(&ok2);
            if (!ok1 || !ok2 || a < 1 || b > 65535 || a > b) {
                if (err) *err = QStringLiteral("端口范围无效: %1").arg(part);
                return {};
            }
            for (int p = a; p <= b; ++p) ports << p;
        } else {
            bool ok = false;
            int p = part.toInt(&ok);
            if (!ok || p < 1 || p > 65535) {
                if (err) *err = QStringLiteral("端口无效: %1").arg(part);
                return {};
            }
            ports << p;
        }
    }
    return ports;
}

void portScan(const QString& host, const QList<int>& ports, int timeoutMs, int concurrency,
              const std::function<void(const PortResult&)>& onResult,
              const std::function<void(int, int)>& onProgress,
              const std::atomic<bool>& cancel) {
    std::atomic<size_t> idx{0};
    std::atomic<int> done{0};
    const int total = ports.size();
    QMutex resultMutex;

    auto worker = [&]() {
        for (;;) {
            size_t i = idx.fetch_add(1);
            if (cancel.load() || i >= static_cast<size_t>(total)) return;
            int port = ports[static_cast<int>(i)];
            PortResult pr;
            pr.port = port;
            QTcpSocket sock;
            sock.connectToHost(host, static_cast<quint16>(port));
            pr.open = (sock.state() == QTcpSocket::ConnectedState) || sock.waitForConnected(timeoutMs);
            if (pr.open) pr.service = serviceName(port);
            {
                QMutexLocker lk(&resultMutex);
                onResult(pr);
            }
            int d = done.fetch_add(1) + 1;
            if (d % 5 == 0 || d == total) onProgress(d, total);
        }
    };

    int n = qBound(1, concurrency, 256);
    std::vector<std::thread> threads;
    threads.reserve(n);
    for (int i = 0; i < n; ++i) threads.emplace_back(worker);
    for (auto& t : threads) if (t.joinable()) t.join();
    onProgress(done.load(), total);
}

// ---------- Whois ----------
static bool whoisServerQuery(const QString& server, const QString& query, QString* out, QString* err) {
    QTcpSocket sock;
    sock.connectToHost(server, 43);
    if (!sock.waitForConnected(8000)) { *err = QStringLiteral("连接 %1:43 失败").arg(server); return false; }
    sock.write((query + QStringLiteral("\r\n")).toUtf8());
    if (!sock.waitForBytesWritten(5000)) { *err = QStringLiteral("发送查询失败"); return false; }
    while (sock.waitForReadyRead(8000)) {}
    *out = QString::fromUtf8(sock.readAll());
    return true;
}

WhoisResult whoisQuery(const QString& domain) {
    WhoisResult r;
    QString text, err;
    if (!whoisServerQuery(QStringLiteral("whois.iana.org"), domain, &text, &err)) {
        // IANA 失败时尝试直接查主服务器
        if (!whoisServerQuery(QStringLiteral("whois.verisign-grs.com"), domain, &text, &err)) {
            r.error = err;
            return r;
        }
    }
    // 查找 referral
    QString referral;
    static const QStringList keys = {QStringLiteral("refer:"), QStringLiteral("whois:"), QStringLiteral("Registrar WHOIS Server:")};
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        for (const QString& k : keys) {
            if (line.trimmed().startsWith(k, Qt::CaseInsensitive)) {
                QString v = line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
                if (!v.isEmpty() && !v.contains(QLatin1Char(' '))) referral = v;
            }
        }
    }
    if (!referral.isEmpty() && referral.compare(QStringLiteral("whois.iana.org"), Qt::CaseInsensitive) != 0) {
        QString text2, err2;
        if (whoisServerQuery(referral, domain, &text2, &err2)) {
            r.text = QStringLiteral("===== %1 =====\n%2\n\n===== %3 =====\n%4")
                         .arg(QStringLiteral("whois.iana.org"), text, referral, text2);
        } else {
            r.text = text + QStringLiteral("\n\n[二次查询 %1 失败: %2]").arg(referral, err2);
        }
    } else {
        r.text = text;
    }
    r.ok = true;
    return r;
}

QStringList localIPv4List() {
    QStringList out;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& ni : ifaces) {
        if (!(ni.flags() & QNetworkInterface::IsUp) || (ni.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& e : ni.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) out << e.ip().toString();
        }
    }
    return out;
}

} // namespace nettools
