#include "icmp.h"
#include <QHostInfo>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <atomic>
#include <functional>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#endif

namespace icmp {

static QString resolveHost(const QString& host, QString* err) {
    QHostAddress addr(host);
    if (!addr.isNull()) return addr.toString();
    QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        if (err) *err = QStringLiteral("域名解析失败: %1").arg(info.errorString());
        return QString();
    }
    // 优先 IPv4
    for (const QHostAddress& a : info.addresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol) return a.toString();
    return info.addresses().first().toString();
}

#ifdef Q_OS_WIN
// ---------------- Windows: iphlpapi IcmpSendEcho ----------------
static PingReply winPing(const QString& ip, int timeoutMs, int ttl) {
    PingReply r;
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) { r.error = QStringLiteral("IcmpCreateFile 失败"); return r; }

    char sendData[32];
    for (int i = 0; i < 32; ++i) sendData[i] = static_cast<char>('A' + (i % 26));
    IP_OPTION_INFORMATION opts{};
    opts.Ttl = static_cast<unsigned char>(ttl > 0 ? ttl : 128);
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + 64] = {};
    DWORD replySize = sizeof(replyBuf);

    IN_ADDR dest{};
    if (InetPtonA(AF_INET, ip.toLocal8Bit().constData(), &dest) != 1) {
        IcmpCloseHandle(h);
        r.error = QStringLiteral("IP 地址无效: %1").arg(ip);
        return r;
    }
    DWORD ret = IcmpSendEcho(h, dest.S_un.S_addr, sendData, sizeof(sendData), &opts, replyBuf, replySize, static_cast<DWORD>(timeoutMs));
    IcmpCloseHandle(h);
    if (ret == 0) {
        DWORD e = GetLastError();
        if (e == IP_REQ_TIMED_OUT) r.error = QStringLiteral("请求超时");
        else r.error = QStringLiteral("IcmpSendEcho 错误 %1").arg(e);
        return r;
    }
    auto* reply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuf);
    if (reply->Status != IP_SUCCESS) {
        if (reply->Status == IP_REQ_TIMED_OUT) r.error = QStringLiteral("请求超时");
        else if (reply->Status == IP_TTL_EXPIRED_TRANSIT) r.error = QStringLiteral("TTL 传输中超限");
        else if (reply->Status == IP_TTL_EXPIRED_REASSEM) r.error = QStringLiteral("TTL 重组超限");
        else if (reply->Status == IP_DEST_HOST_UNREACHABLE) r.error = QStringLiteral("目标主机不可达");
        else if (reply->Status == IP_DEST_NET_UNREACHABLE) r.error = QStringLiteral("目标网络不可达");
        else r.error = QStringLiteral("ICMP 状态码 %1").arg(reply->Status);
        r.ip = QHostAddress(ntohl(reply->Address)).toString();
        return r;
    }
    r.success = true;
    r.ip = QHostAddress(ntohl(reply->Address)).toString();
    r.ms = static_cast<double>(reply->RoundTripTime);
    r.ttl = opts.Ttl;
    return r;
}
#endif // Q_OS_WIN

#ifdef Q_OS_MAC
// ---------------- macOS: SOCK_DGRAM ICMP（非特权） ----------------
static unsigned short checksum(unsigned short* buf, int len) {
    unsigned long sum = 0;
    while (len > 1) { sum += *buf++; len -= 2; }
    if (len == 1) sum += *reinterpret_cast<unsigned char*>(buf);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<unsigned short>(~sum);
}

struct MacIcmpCtx {
    int fd = -1;
    sockaddr_in dest{};
    unsigned short id = 0;
    ~MacIcmpCtx() { if (fd >= 0) ::close(fd); }
};

static bool macSendEcho(MacIcmpCtx& c, unsigned short seq, int ttl, int timeoutMs, PingReply& out) {
    icmp hdr{};
    hdr.icmp_type = ICMP_ECHO;
    hdr.icmp_code = 0;
    hdr.icmp_id = c.id;
    hdr.icmp_seq = htons(seq);
    char packet[64];
    memcpy(packet, &hdr, sizeof(hdr));
    const char magic[] = "LWBOXPING";
    memcpy(packet + sizeof(hdr), magic, sizeof(magic));
    const int pktLen = static_cast<int>(sizeof(hdr) + sizeof(magic));
    reinterpret_cast<icmp*>(packet)->icmp_cksum = 0;
    reinterpret_cast<icmp*>(packet)->icmp_cksum = checksum(reinterpret_cast<unsigned short*>(packet), pktLen);

    if (ttl > 0) setsockopt(c.fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    QElapsedTimer timer;
    timer.start();
    if (sendto(c.fd, packet, pktLen, 0, reinterpret_cast<sockaddr*>(&c.dest), sizeof(c.dest)) < 0) {
        out.error = QStringLiteral("发送失败: %1").arg(strerror(errno));
        return false;
    }

    char buf[1500];
    qint64 deadline = timeoutMs;
    while (deadline > 0) {
        pollfd p{c.fd, POLLIN, 0};
        int pr = ::poll(&p, 1, static_cast<int>(deadline));
        if (pr <= 0) break;
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(c.fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) break;
        double ms = static_cast<double>(timer.elapsed());
        auto* iph = reinterpret_cast<ip*>(buf);
        int ipHdrLen = iph->ip_hl * 4;
        if (n < ipHdrLen + static_cast<ssize_t>(sizeof(icmp))) continue;
        auto* icp = reinterpret_cast<icmp*>(buf + ipHdrLen);
        char srcIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &iph->ip_src, srcIp, sizeof(srcIp));
        if (icp->icmp_type == ICMP_ECHOREPLY && ntohs(icp->icmp_id) == c.id && ntohs(icp->icmp_seq) == seq) {
            out.success = true;
            out.ip = QString::fromLatin1(srcIp);
            out.ms = ms;
            return true;
        }
        if (icp->icmp_type == ICMP_TIMXCEED || icp->icmp_type == ICMP_UNREACH) {
            // 差错报文内嵌原始 IP 头 + 前 8 字节，校验 our magic
            const char* inner = buf + ipHdrLen + 8;
            int innerIphl = reinterpret_cast<ip*>(const_cast<char*>(inner))->ip_hl * 4;
            const char* innerPayload = inner + innerIphl + 8;
            if (memcmp(innerPayload, magic, sizeof(magic)) == 0) {
                out.success = false;
                out.ip = QString::fromLatin1(srcIp);
                out.ms = ms;
                out.error = icp->icmp_type == ICMP_TIMXCEED ? QStringLiteral("传输中超时(TTL)") : QStringLiteral("不可达");
                return true; // 有响应（虽非 ECHOREPLY）
            }
        }
        deadline = timeoutMs - static_cast<int>(timer.elapsed());
    }
    out.error = QStringLiteral("请求超时");
    return false;
}

static PingReply macPing(const QString& ip, int timeoutMs, int ttl, unsigned short seq) {
    PingReply r;
    MacIcmpCtx c;
    c.id = static_cast<unsigned short>((::getpid() & 0xFFFF) | 0x4000);
    c.fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (c.fd < 0) { r.error = QStringLiteral("ICMP socket 创建失败(将尝试系统命令)"); return r; }
    memset(&c.dest, 0, sizeof(c.dest));
    c.dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.toLocal8Bit().constData(), &c.dest.sin_addr) != 1) {
        r.error = QStringLiteral("IP 地址无效");
        return r;
    }
    macSendEcho(c, seq, ttl, timeoutMs, r);
    return r;
}
#endif // Q_OS_MAC

PingReply pingOnce(const QString& host, int timeoutMs, int ttl, unsigned short seq) {
    PingReply r;
    QString ip = resolveHost(host, &r.error);
    if (ip.isEmpty()) return r;
#ifdef Q_OS_WIN
    r = winPing(ip, timeoutMs, ttl);
#elif defined(Q_OS_MAC)
    r = macPing(ip, timeoutMs, ttl, seq);
    if (!r.success && r.error.contains(QStringLiteral("socket 创建失败")))
        return r; // 交由上层回退系统命令
#else
    Q_UNUSED(ttl);
    Q_UNUSED(seq);
    r.error = QStringLiteral("当前平台不支持原生 ICMP");
#endif
    if (!r.ip.isEmpty()) r.ip = ip;
    return r;
}

// ---------------- 系统命令回退 ----------------
static QRegularExpression ipRe(QStringLiteral("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})"));

void systemPing(const QString& host, int count, int timeoutMs,
                const std::function<void(const QString&)>& onLine, const std::atomic<bool>& cancel) {
    QProcess p;
#ifdef Q_OS_WIN
    QStringList args{QStringLiteral("-n"), QString::number(count), QStringLiteral("-w"), QString::number(timeoutMs), host};
#else
    QStringList args{QStringLiteral("-c"), QString::number(count), QStringLiteral("-W"), QString::number(qMax(1, timeoutMs / 1000)), host};
#endif
    p.start(QStringLiteral("ping"), args);
    while (!cancel.load() && p.state() != QProcess::NotRunning) {
        if (!p.waitForReadyRead(200)) {
            if (p.waitForFinished(200)) break;
            continue;
        }
        while (p.canReadLine())
            onLine(QString::fromLocal8Bit(p.readLine()).trimmed());
    }
    if (cancel.load()) { p.kill(); p.waitForFinished(1000); }
    onLine(QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed());
}

void systemTraceroute(const QString& host, int maxHops,
                      const std::function<void(const QString&)>& onLine, const std::atomic<bool>& cancel) {
#ifdef Q_OS_WIN
    QStringList args{QStringLiteral("-d"), QStringLiteral("-h"), QString::number(maxHops), QStringLiteral("-w"), QStringLiteral("800"), host};
    const QString program = QStringLiteral("tracert");
#else
    QStringList args{QStringLiteral("-n"), QStringLiteral("-m"), QString::number(maxHops), QStringLiteral("-w"), QStringLiteral("1"), host};
    const QString program = QStringLiteral("traceroute");
#endif
    QProcess p;
    p.start(program, args);
    while (!cancel.load() && p.state() != QProcess::NotRunning) {
        if (!p.waitForReadyRead(200)) {
            if (p.waitForFinished(200)) break;
            continue;
        }
        while (p.canReadLine())
            onLine(QString::fromLocal8Bit(p.readLine()).trimmed());
    }
    if (cancel.load()) { p.kill(); p.waitForFinished(1000); }
    onLine(QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed());
}

bool traceroute(const QString& host, int maxHops, int timeoutMs, const HopCallback& onHop,
                const std::atomic<bool>& cancel) {
    // 探测目标（用于判断到达）
    QString ip = resolveHost(host, nullptr);
    if (ip.isEmpty()) return false;
#ifdef Q_OS_WIN
    // iphlpapi 始终可用
#elif defined(Q_OS_MAC)
    {
        int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        if (fd < 0) return false;
        ::close(fd);
    }
#else
    return false;
#endif
    for (int ttlv = 1; ttlv <= maxHops; ++ttlv) {
        if (cancel.load()) return true;
        PingReply rep = pingOnce(host, timeoutMs, ttlv, static_cast<unsigned short>(ttlv));
        bool reached = rep.success && (rep.ip == ip);
        onHop(ttlv, rep.success || !rep.ip.isEmpty() ? rep.ip : QStringLiteral("*"),
              (rep.success || !rep.error.contains(QStringLiteral("超时"))) && rep.ms > 0 ? rep.ms : -1.0, reached);
        if (reached) break;
    }
    return true;
}

} // namespace icmp
