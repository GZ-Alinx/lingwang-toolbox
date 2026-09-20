#include "pages.h"
#include "widgets/toolpage.h"
#include "core/nettools.h"
#include "core/icmp.h"
#include "core/async.h"
#include "iconprovider.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QDnsLookup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QThread>
#include <QUrlQuery>

namespace {

// ---------- Ping ----------
class PingPage final : public StreamPage {
public:
    PingPage() {
        ToolPage::setMeta(QStringLiteral("wifi"), QStringLiteral("Ping"),
                          QStringLiteral("连通性测试，统计丢包率与延迟"));
        m_host = new QLineEdit; m_host->setPlaceholderText(QStringLiteral("域名或 IP，如 baidu.com"));
        m_cnt = new QSpinBox; m_cnt->setRange(1, 100); m_cnt->setValue(4);
        m_timeout = new QSpinBox; m_timeout->setRange(100, 10000); m_timeout->setValue(2000);
        m_timeout->setSuffix(QStringLiteral(" ms"));
        paramsLayout()->addWidget(ui::formRow(QStringLiteral("目标"), m_host));
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(new QLabel(QStringLiteral("次数:"))); lay->addWidget(m_cnt);
        lay->addWidget(new QLabel(QStringLiteral("超时:"))); lay->addWidget(m_timeout);
        lay->addStretch();
        paramsLayout()->addWidget(row);
    }
protected:
    void onStart() override {
        const QString host = m_host->text().trimmed();
        if (host.isEmpty()) { emit jobStatus(QStringLiteral("请输入目标地址"), QStringLiteral("err")); return; }
        const int count = m_cnt->value();
        const int timeout = m_timeout->value();
        beginJob([this, host, count, timeout](const std::atomic<bool>& cancel) {
            int sent = 0, recv = 0;
            double total = 0, minMs = 1e9, maxMs = 0;
            bool nativeBroken = false;
            for (int i = 1; i <= count; ++i) {
                if (cancel.load()) break;
                icmp::PingReply rep = icmp::pingOnce(host, timeout, 0, static_cast<unsigned short>(i));
                if (rep.error.contains(QStringLiteral("不支持")) || rep.error.contains(QStringLiteral("socket"))) {
                    nativeBroken = true;
                    break;
                }
                ++sent;
                if (rep.success) {
                    ++recv;
                    total += rep.ms;
                    minMs = qMin(minMs, rep.ms);
                    maxMs = qMax(maxMs, rep.ms);
                    emit logLine(QStringLiteral("#%1  来自 %2: 时间=%3ms")
                                     .arg(i).arg(rep.ip).arg(rep.ms, 0, 'f', rep.ms < 10 ? 2 : 0));
                } else {
                    emit logLine(QStringLiteral("#%1  %2 (%3)").arg(i).arg(rep.error, rep.ip.isEmpty() ? QStringLiteral("-") : rep.ip));
                }
                if (i < count)
                    for (int w = 0; w < 20 && !cancel.load(); ++w) QThread::msleep(50);
            }
            if (nativeBroken || sent == 0) {
                emit logLine(QStringLiteral("—— 原生 ICMP 不可用，回退系统 ping ——"));
                icmp::systemPing(host, count, timeout,
                                 [this](const QString& l) { emit logLine(l); }, cancel);
                return;
            }
            emit logLine(QString());
            emit logLine(QStringLiteral("—— 统计 ——"));
            emit logLine(QStringLiteral("发送: %1  接收: %2  丢包率: %3%")
                             .arg(sent).arg(recv).arg(sent ? QString::number(100.0 * (sent - recv) / sent, 'f', 1) : QStringLiteral("0.0")));
            if (recv > 0)
                emit logLine(QStringLiteral("最小: %1ms  平均: %2ms  最大: %3ms")
                                 .arg(minMs, 0, 'f', 2).arg(total / recv, 0, 'f', 2).arg(maxMs, 0, 'f', 2));
        });
    }
    QLineEdit* m_host; QSpinBox* m_cnt; QSpinBox* m_timeout;
};

// ---------- 路由追踪 ----------
class TraceroutePage final : public StreamPage {
public:
    TraceroutePage() {
        ToolPage::setMeta(QStringLiteral("globe"), QStringLiteral("路由追踪"),
                          QStringLiteral("Traceroute 查看到目标网络的每一跳"));
        m_host = new QLineEdit; m_host->setPlaceholderText(QStringLiteral("域名或 IP"));
        m_hops = new QSpinBox; m_hops->setRange(1, 64); m_hops->setValue(30);
        m_timeout = new QSpinBox; m_timeout->setRange(200, 10000); m_timeout->setValue(1500);
        m_timeout->setSuffix(QStringLiteral(" ms"));
        paramsLayout()->addWidget(ui::formRow(QStringLiteral("目标"), m_host));
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(new QLabel(QStringLiteral("最大跳数:"))); lay->addWidget(m_hops);
        lay->addWidget(new QLabel(QStringLiteral("每跳超时:"))); lay->addWidget(m_timeout);
        lay->addStretch();
        paramsLayout()->addWidget(row);
    }
protected:
    void onStart() override {
        const QString host = m_host->text().trimmed();
        if (host.isEmpty()) { emit jobStatus(QStringLiteral("请输入目标地址"), QStringLiteral("err")); return; }
        const int hops = m_hops->value();
        const int timeout = m_timeout->value();
        beginJob([this, host, hops, timeout](const std::atomic<bool>& cancel) {
            emit logLine(QStringLiteral("追踪到 %1 （最多 %2 跳）…").arg(host).arg(hops));
            bool nativeOk = icmp::traceroute(host, hops, timeout,
                                             [this](int hop, const QString& ip, double ms, bool reached) {
                                                 QString line = QStringLiteral("%1\t%2\t%3").arg(hop, 2).arg(ip, 20);
                                                 line += ms >= 0 ? QStringLiteral("%1ms").arg(ms, 0, 'f', ms < 10 ? 2 : 0)
                                                                 : QStringLiteral("*");
                                                 if (reached) line += QStringLiteral("  ← 已到达");
                                                 emit logLine(line);
                                             }, cancel);
            if (!nativeOk) {
                emit logLine(QStringLiteral("—— 原生 ICMP 不可用，回退系统 tracert ——"));
                icmp::systemTraceroute(host, hops, [this](const QString& l) { emit logLine(l); }, cancel);
            } else {
                emit logLine(QStringLiteral("追踪完成"));
            }
        });
    }
    QLineEdit* m_host; QSpinBox* m_hops; QSpinBox* m_timeout;
};

// ---------- 端口扫描 ----------
class PortScanPage final : public StreamPage {
public:
    PortScanPage() {
        ToolPage::setMeta(QStringLiteral("terminal"), QStringLiteral("端口扫描"),
                          QStringLiteral("TCP 端口连通性检测与并发扫描"));
        m_host = new QLineEdit; m_host->setPlaceholderText(QStringLiteral("域名或 IP"));
        m_ports = new QLineEdit; m_ports->setPlaceholderText(QStringLiteral("如 80,443,3389 或 1-1000"));
        m_ports->setText(QStringLiteral("21,22,23,25,53,80,110,135,139,143,443,445,993,995,1433,3306,3389,5432,6379,8080,8443,9090,27017"));
        m_timeout = new QSpinBox; m_timeout->setRange(50, 10000); m_timeout->setValue(800);
        m_timeout->setSuffix(QStringLiteral(" ms"));
        m_conc = new QSpinBox; m_conc->setRange(1, 256); m_conc->setValue(64);
        paramsLayout()->addWidget(ui::formRow(QStringLiteral("目标"), m_host));
        paramsLayout()->addWidget(ui::formRow(QStringLiteral("端口"), m_ports));
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(new QLabel(QStringLiteral("超时:"))); lay->addWidget(m_timeout);
        lay->addWidget(new QLabel(QStringLiteral("并发:"))); lay->addWidget(m_conc);
        lay->addStretch();
        paramsLayout()->addWidget(row);
    }
protected:
    void onStart() override {
        const QString host = m_host->text().trimmed();
        QString err;
        const QList<int> ports = nettools::parsePortSpec(m_ports->text(), &err);
        if (host.isEmpty()) { emit jobStatus(QStringLiteral("请输入目标地址"), QStringLiteral("err")); return; }
        if (!err.isEmpty()) { emit jobStatus(err, QStringLiteral("err")); return; }
        const int timeout = m_timeout->value();
        const int conc = m_conc->value();
        beginJob([this, host, ports, timeout, conc](const std::atomic<bool>& cancel) {
            emit logLine(QStringLiteral("扫描 %1 共 %2 个端口（并发 %3）…").arg(host).arg(ports.size()).arg(conc));
            QElapsedTimer timer;
            timer.start();
            nettools::portScan(host, ports, timeout, conc,
                               [this](const nettools::PortResult& pr) {
                                   if (pr.open)
                                       emit logLine(QStringLiteral("✓ %1 端口开放%2")
                                                        .arg(pr.port)
                                                        .arg(pr.service.isEmpty() ? QString() : QStringLiteral("  (%1)").arg(pr.service)));
                               },
                               [this](int done, int total) {
                                   emit jobStatus(QStringLiteral("进度 %1/%2").arg(done).arg(total), QStringLiteral("info"));
                               }, cancel);
            emit logLine(QStringLiteral("扫描完成，耗时 %1ms").arg(timer.elapsed()));
        });
    }
    QLineEdit* m_host; QLineEdit* m_ports; QSpinBox* m_timeout; QSpinBox* m_conc;
};

// ---------- DNS ----------
class DnsPage final : public ToolPage {
public:
    DnsPage() {
        ToolPage::setMeta(QStringLiteral("globe"), QStringLiteral("DNS 解析"),
                          QStringLiteral("域名解析 A / AAAA / CNAME / MX / TXT / NS / SRV"));
        m_domain = new QLineEdit; m_domain->setPlaceholderText(QStringLiteral("如 baidu.com"));
        m_type = new QComboBox;
        m_type->addItems({QStringLiteral("A"), QStringLiteral("AAAA"), QStringLiteral("CNAME"), QStringLiteral("MX"),
                          QStringLiteral("TXT"), QStringLiteral("NS"), QStringLiteral("SRV")});
        m_server = new QComboBox;
        m_server->addItems({QStringLiteral("系统默认"), QStringLiteral("8.8.8.8 (Google)"), QStringLiteral("1.1.1.1 (Cloudflare)"),
                            QStringLiteral("223.5.5.5 (阿里)"), QStringLiteral("114.114.114.114")});
        m_server->setEditable(true);
        auto* btn = ui::button(QStringLiteral("查询"), "primary");

        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_domain, 1);
        lay->addWidget(new QLabel(QStringLiteral("类型:"))); lay->addWidget(m_type);
        lay->addWidget(new QLabel(QStringLiteral("DNS:"))); lay->addWidget(m_server);
        lay->addWidget(btn);
        body()->addWidget(ui::card(QStringLiteral("查询"), row));

        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("解析结果"), m_output, copyBtn, true), 1);

        connect(btn, &QPushButton::clicked, this, &DnsPage::lookup);
        connect(m_domain, &QLineEdit::returnPressed, this, &DnsPage::lookup);
    }
private:
    void lookup() {
        const QString domain = m_domain->text().trimmed();
        if (domain.isEmpty()) return;
        static const QDnsLookup::Type types[] = {
            QDnsLookup::A, QDnsLookup::AAAA, QDnsLookup::CNAME, QDnsLookup::MX,
            QDnsLookup::TXT, QDnsLookup::NS, QDnsLookup::SRV};
        if (m_lookup) { m_lookup->deleteLater(); m_lookup = nullptr; }
        m_lookup = new QDnsLookup(types[m_type->currentIndex()], domain, this);
        const QString server = m_server->currentText().split(QLatin1Char(' ')).first();
        if (m_server->currentIndex() != 0) {
            QHostAddress addr(server);
            if (!addr.isNull()) m_lookup->setNameserver(addr);
        }
        connect(m_lookup, &QDnsLookup::finished, this, [this] { render(); });
        m_output->setPlainText(QStringLiteral("查询中…"));
        m_lookup->lookup();
    }
    void render() {
        if (!m_lookup) return;
        if (m_lookup->error() != QDnsLookup::NoError) {
            m_output->setPlainText(QStringLiteral("查询失败: %1").arg(m_lookup->errorString()));
            return;
        }
        QStringList out;
        for (const QDnsHostAddressRecord& r : m_lookup->hostAddressRecords())
            out << QStringLiteral("%1  →  %2").arg(r.name(), r.value().toString());
        for (const QDnsMailExchangeRecord& r : m_lookup->mailExchangeRecords())
            out << QStringLiteral("MX  %1  (优先级 %2)").arg(r.exchange()).arg(r.preference());
        for (const QDnsServiceRecord& r : m_lookup->serviceRecords())
            out << QStringLiteral("SRV %1:%2  (优先级 %3 权重 %4)").arg(r.target()).arg(r.port()).arg(r.priority()).arg(r.weight());
        for (const QDnsTextRecord& r : m_lookup->textRecords()) {
            for (const QByteArray& t : r.values())
                out << QStringLiteral("TXT  %1").arg(QString::fromUtf8(t));
        }
        for (const QDnsDomainNameRecord& r : m_lookup->nameServerRecords())
            out << QStringLiteral("%1  →  %2").arg(r.name(), r.value());
        m_output->setPlainText(out.isEmpty() ? QStringLiteral("（无记录）") : out.join(QLatin1Char('\n')));
    }
    QLineEdit* m_domain; QComboBox* m_type; QComboBox* m_server;
    QPlainTextEdit* m_output;
    QDnsLookup* m_lookup = nullptr;
};

// ---------- HTTP 请求 ----------
class HttpPage final : public ToolPage {
public:
    HttpPage() {
        ToolPage::setMeta(QStringLiteral("globe"), QStringLiteral("HTTP 请求"),
                          QStringLiteral("发送 HTTP 请求，查看响应头、响应体与耗时"));
        m_method = new QComboBox;
        m_method->addItems({QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"), QStringLiteral("DELETE"),
                            QStringLiteral("HEAD"), QStringLiteral("PATCH"), QStringLiteral("OPTIONS")});
        m_url = new QLineEdit; m_url->setPlaceholderText(QStringLiteral("https://httpbin.org/get"));
        m_url->setFont(Icons::monoFont(10));
        m_follow = new QCheckBox(QStringLiteral("跟随重定向")); m_follow->setChecked(true);
        m_noTls = new QCheckBox(QStringLiteral("忽略 TLS 错误"));
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_method);
        lay->addWidget(m_url, 1);
        lay->addWidget(m_follow);
        lay->addWidget(m_noTls);
        body()->addWidget(ui::card(QStringLiteral("请求"), row));

        m_headers = new QPlainTextEdit;
        m_headers->setObjectName(QStringLiteral("mono"));
        m_headers->setPlaceholderText(QStringLiteral("每行一条: Key: Value"));
        m_headers->setFixedHeight(90);
        m_body = new QPlainTextEdit;
        m_body->setObjectName(QStringLiteral("mono"));
        m_body->setPlaceholderText(QStringLiteral("请求体（POST/PUT 时填写）"));
        m_body->setFixedHeight(110);
        auto* two = new QWidget;
        auto* twoLay = new QHBoxLayout(two);
        twoLay->setContentsMargins(0, 0, 0, 0);
        twoLay->setSpacing(12);
        twoLay->addWidget(ui::card(QStringLiteral("请求头"), m_headers), 1);
        twoLay->addWidget(ui::card(QStringLiteral("请求体"), m_body), 1);
        body()->addWidget(two);

        m_send = ui::button(QStringLiteral("发送请求"), "primary");
        connect(m_send, &QPushButton::clicked, this, &HttpPage::send);
        auto* btnRow = new QHBoxLayout;
        btnRow->addWidget(m_send);
        btnRow->addStretch();
        m_chip = ui::chip();
        btnRow->addWidget(m_chip);
        auto* btnWrap = new QWidget;
        btnWrap->setLayout(btnRow);
        body()->addWidget(btnWrap);

        m_respMeta = new QLabel(QStringLiteral("—"));
        m_respMeta->setObjectName(QStringLiteral("pageDesc"));
        m_respHeaders = new QPlainTextEdit;
        m_respHeaders->setObjectName(QStringLiteral("mono"));
        m_respHeaders->setReadOnly(true);
        m_respHeaders->setFixedHeight(120);
        m_respBody = new QPlainTextEdit;
        m_respBody->setObjectName(QStringLiteral("mono"));
        m_respBody->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制响应体"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_respBody->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("响应"), m_respBody, copyBtn, true), 1);
        auto* metaWrap = new QWidget;
        auto* metaLay = new QVBoxLayout(metaWrap);
        metaLay->setContentsMargins(0, 0, 0, 0);
        metaLay->setSpacing(8);
        metaLay->addWidget(m_respMeta);
        metaLay->addWidget(m_respHeaders);
        body()->insertWidget(body()->count() - 1, ui::card(QStringLiteral("响应信息"), metaWrap, nullptr, false));
    }
private:
    void send() {
        const QUrl url = QUrl::fromUserInput(m_url->text().trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            ui::setChip(m_chip, QStringLiteral("URL 无效"), QStringLiteral("err"));
            return;
        }
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         m_follow->isChecked() ? QNetworkRequest::NoLessSafeRedirectPolicy
                                               : QNetworkRequest::ManualRedirectPolicy);
        req.setTransferTimeout(30000);
        for (const QString& line : m_headers->toPlainText().split(QLatin1Char('\n'))) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0)
                req.setRawHeader(line.left(colon).trimmed().toUtf8(), line.mid(colon + 1).trimmed().toUtf8());
        }
        const QString method = m_method->currentText();
        const QByteArray payload = m_body->toPlainText().toUtf8();
        QNetworkReply* reply = nullptr;
        if (method == QStringLiteral("GET")) reply = m_nam.get(req);
        else if (method == QStringLiteral("HEAD")) reply = m_nam.head(req);
        else if (method == QStringLiteral("POST")) reply = m_nam.post(req, payload);
        else if (method == QStringLiteral("PUT")) reply = m_nam.put(req, payload);
        else reply = m_nam.sendCustomRequest(req, method.toLatin1(), payload);
        if (m_noTls->isChecked())
            connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });

        m_send->setEnabled(false);
        ui::setChip(m_chip, QStringLiteral("请求中…"), QStringLiteral("info"));
        QElapsedTimer* timer = new QElapsedTimer;
        timer->start();
        connect(reply, &QNetworkReply::finished, this, [this, reply, timer] {
            m_send->setEnabled(true);
            const qint64 ms = timer->elapsed();
            delete timer;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const qint64 size = reply->size();
            if (reply->error() != QNetworkReply::NoError && status == 0) {
                ui::setChip(m_chip, QStringLiteral("请求失败: %1").arg(reply->errorString()), QStringLiteral("err"));
                m_respMeta->setText(QStringLiteral("—"));
                reply->deleteLater();
                return;
            }
            const bool ok = status < 400;
            ui::setChip(m_chip, QStringLiteral("HTTP %1 · %2ms · %3")
                                    .arg(status)
                                    .arg(ms)
                                    .arg(size < 1024 ? QStringLiteral("%1 B").arg(size)
                                                     : QStringLiteral("%1 KB").arg(QString::number(size / 1024.0, 'f', 1))),
                        ok ? QStringLiteral("ok") : QStringLiteral("err"));
            QStringList hs;
            for (const QNetworkReply::RawHeaderPair& p : reply->rawHeaderPairs())
                hs << QStringLiteral("%1: %2").arg(QString::fromLatin1(p.first), QString::fromLatin1(p.second));
            m_respHeaders->setPlainText(hs.join(QLatin1Char('\n')));
            QByteArray body = reply->readAll();
            m_respMeta->setText(QStringLiteral("%1 %2 %3").arg(status)
                                    .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString())
                                    .arg(reply->url().toString()));
            QJsonParseError jerr{};
            QJsonDocument jdoc = QJsonDocument::fromJson(body, &jerr);
            m_respBody->setPlainText(jdoc.isNull() ? QString::fromUtf8(body)
                                                   : QString::fromUtf8(jdoc.toJson(QJsonDocument::Indented)));
            reply->deleteLater();
        });
    }
    QComboBox* m_method; QLineEdit* m_url; QCheckBox* m_follow; QCheckBox* m_noTls;
    QPlainTextEdit* m_headers; QPlainTextEdit* m_body;
    QPlainTextEdit* m_respHeaders; QPlainTextEdit* m_respBody;
    QLabel* m_respMeta;
    QPushButton* m_send;
    QLabel* m_chip = nullptr;
    QNetworkAccessManager m_nam;
};

// ---------- 子网计算 ----------
class SubnetPage final : public ToolPage {
public:
    SubnetPage() {
        ToolPage::setMeta(QStringLiteral("grid"), QStringLiteral("子网计算器"),
                          QStringLiteral("CIDR / 掩码换算，网段与可用主机计算"));
        m_ip = new QLineEdit; m_ip->setPlaceholderText(QStringLiteral("如 192.168.1.66"));
        m_mask = new QLineEdit; m_mask->setPlaceholderText(QStringLiteral("24 或 255.255.255.0"));
        m_mask->setText(QStringLiteral("24"));
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(new QLabel(QStringLiteral("IP:"))); lay->addWidget(m_ip, 2);
        lay->addWidget(new QLabel(QStringLiteral("掩码/前缀:"))); lay->addWidget(m_mask, 1);
        body()->addWidget(ui::card(QStringLiteral("输入"), row));

        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        body()->addWidget(ui::card(QStringLiteral("计算结果"), m_output, nullptr, true), 1);

        connect(m_ip, &QLineEdit::textChanged, this, &SubnetPage::calc);
        connect(m_mask, &QLineEdit::textChanged, this, &SubnetPage::calc);
    }
private:
    void calc() {
        nettools::SubnetInfo info = nettools::subnetCalc(m_ip->text(), m_mask->text());
        if (!info.ok) { m_output->setPlainText(info.error); return; }
        QStringList out;
        out << QStringLiteral("网络地址:     %1/%2").arg(info.network).arg(info.prefix);
        out << QStringLiteral("广播地址:     %1").arg(info.broadcast);
        out << QStringLiteral("子网掩码:     %1").arg(info.mask);
        out << QStringLiteral("通配符掩码:   %1").arg(info.wildcard);
        out << QStringLiteral("可用主机范围: %1 ~ %2").arg(info.firstHost, info.lastHost);
        out << QStringLiteral("可用主机数:   %1（总地址 %2）").arg(info.hostCount, info.totalAddresses);
        out << QStringLiteral("IP 类别:      %1 · %2").arg(info.ipClass, info.ipType);
        out << QStringLiteral("二进制 IP:    %1").arg(info.binaryIp);
        out << QStringLiteral("二进制掩码:   %1").arg(info.binaryMask);
        m_output->setPlainText(out.join(QLatin1Char('\n')));
    }
    QLineEdit* m_ip; QLineEdit* m_mask; QPlainTextEdit* m_output;
};

// ---------- Whois ----------
class WhoisPage final : public ToolPage {
    Q_OBJECT
public:
    WhoisPage() {
        ToolPage::setMeta(QStringLiteral("info"), QStringLiteral("Whois 查询"),
                          QStringLiteral("域名注册信息查询"));
        m_domain = new QLineEdit; m_domain->setPlaceholderText(QStringLiteral("如 example.com"));
        m_btn = ui::button(QStringLiteral("查询"), "primary");
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_domain, 1);
        lay->addWidget(m_btn);
        body()->addWidget(ui::card(QStringLiteral("域名"), row));
        m_output = new QPlainTextEdit;
        m_output->setObjectName(QStringLiteral("mono"));
        m_output->setReadOnly(true);
        auto* copyBtn = ui::button(QStringLiteral("复制"));
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_output->toPlainText()); });
        body()->addWidget(ui::card(QStringLiteral("Whois 信息"), m_output, copyBtn, true), 1);
        connect(this, &WhoisPage::resultReady, this, [this](const QString& text, bool ok) {
            m_output->setPlainText(text);
            m_btn->setEnabled(true);
            if (ok) m_output->setProperty("done", true);
        });
        connect(m_btn, &QPushButton::clicked, this, [this] {
            const QString d = m_domain->text().trimmed();
            if (d.isEmpty() || m_running) return;
            m_running = true;
            m_btn->setEnabled(false);
            m_output->setPlainText(QStringLiteral("查询中…"));
            auto* worker = new JobThread([this, d](const std::atomic<bool>&) {
                nettools::WhoisResult r = nettools::whoisQuery(d);
                emit resultReady(r.ok ? r.text : QStringLiteral("查询失败: %1").arg(r.error), r.ok);
            }, this);
            connect(worker, &JobThread::finished, worker, &QObject::deleteLater);
            worker->start();
        });
    }
signals:
    void resultReady(const QString& text, bool ok);

private:
    QLineEdit* m_domain;
    QPushButton* m_btn;
    QPlainTextEdit* m_output;
    bool m_running = false;
};

// ---------- 网卡信息 ----------
class NicPage final : public ToolPage {
public:
    NicPage() {
        ToolPage::setMeta(QStringLiteral("cpu"), QStringLiteral("网卡信息"),
                          QStringLiteral("本机网卡、IP 与 MAC 地址一览"));
        auto* btn = ui::button(QStringLiteral("刷新"));
        auto* table = new QTableWidget(0, 5, this);
        table->setHorizontalHeaderLabels({QStringLiteral("网卡"), QStringLiteral("状态"), QStringLiteral("MAC 地址"),
                                          QStringLiteral("IPv4"), QStringLiteral("IPv6")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setColumnWidth(0, 150);
        table->setColumnWidth(1, 70);
        table->setColumnWidth(2, 150);
        table->setColumnWidth(3, 140);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(btn, &QPushButton::clicked, this, [this, table] { refresh(table); });
        body()->addWidget(ui::card(QStringLiteral("网络接口"), table, btn, true), 1);
        refresh(table);
    }
private:
    void refresh(QTableWidget* table) {
        const auto ifaces = QNetworkInterface::allInterfaces();
        table->setRowCount(0);
        for (const QNetworkInterface& ni : ifaces) {
            const int row = table->rowCount();
            table->insertRow(row);
            const bool up = ni.flags() & QNetworkInterface::IsUp;
            QString v4, v6;
            for (const QNetworkAddressEntry& e : ni.addressEntries()) {
                if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    if (!v4.isEmpty()) v4 += QStringLiteral(", ");
                    v4 += e.ip().toString();
                } else if (e.ip().protocol() == QAbstractSocket::IPv6Protocol) {
                    if (!v6.isEmpty()) v6 += QStringLiteral(", ");
                    v6 += e.ip().toString();
                }
            }
            table->setItem(row, 0, new QTableWidgetItem(ni.humanReadableName()));
            table->setItem(row, 1, new QTableWidgetItem(up ? QStringLiteral("已启用") : QStringLiteral("已禁用")));
            table->setItem(row, 2, new QTableWidgetItem(ni.hardwareAddress()));
            table->setItem(row, 3, new QTableWidgetItem(v4));
            table->setItem(row, 4, new QTableWidgetItem(v6));
        }
    }
};

// ---------- IP 查询 ----------
class IpInfoPage final : public ToolPage {
public:
    IpInfoPage() {
        ToolPage::setMeta(QStringLiteral("wifi"), QStringLiteral("IP 查询"),
                          QStringLiteral("本机 IP、公网 IP 与归属地查询"));
        // 公网信息
        m_query = new QLineEdit; m_query->setPlaceholderText(QStringLiteral("留空查询本机公网 IP，也可输入任意 IP"));
        auto* btn = ui::button(QStringLiteral("查询"), "primary");
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_query, 1);
        lay->addWidget(btn);
        m_pub = new QPlainTextEdit;
        m_pub->setObjectName(QStringLiteral("mono"));
        m_pub->setReadOnly(true);
        auto* inner = new QWidget;
        auto* inLay = new QVBoxLayout(inner);
        inLay->setContentsMargins(0, 0, 0, 0);
        inLay->setSpacing(8);
        inLay->addWidget(row);
        inLay->addWidget(m_pub);
        body()->addWidget(ui::card(QStringLiteral("公网信息"), inner));
        connect(btn, &QPushButton::clicked, this, [this] { fetch(m_query->text().trimmed()); });

        // 本机信息
        m_local = new QPlainTextEdit;
        m_local->setObjectName(QStringLiteral("mono"));
        m_local->setReadOnly(true);
        body()->addWidget(ui::card(QStringLiteral("本机网卡 IP"), m_local, nullptr, true), 1);
        renderLocal();
        fetch(QString());
    }
private:
    void renderLocal() {
        QStringList lines;
        const auto ifaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& ni : ifaces) {
            if (!(ni.flags() & QNetworkInterface::IsUp)) continue;
            for (const QNetworkAddressEntry& e : ni.addressEntries()) {
                if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
                lines << QStringLiteral("%1  %2  %3")
                             .arg(ni.humanReadableName(), -20)
                             .arg(e.ip().toString(), -16)
                             .arg(ni.flags() & QNetworkInterface::IsLoopBack ? QStringLiteral("(回环)") : QString());
            }
        }
        m_local->setPlainText(lines.join(QLatin1Char('\n')));
    }
    void fetch(const QString& ip) {
        m_pub->setPlainText(QStringLiteral("查询中…"));
        // 依次尝试多个接口
        const QList<QPair<QString, bool>> endpoints = {
            {ip.isEmpty() ? QStringLiteral("http://ip-api.com/json/?fields=status,message,country,regionName,city,isp,as,query&lang=zh-CN")
                          : QStringLiteral("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,as,query&lang=zh-CN").arg(ip),
             false},
            {ip.isEmpty() ? QStringLiteral("https://ipwho.is/") : QStringLiteral("https://ipwho.is/%1").arg(ip), true}};
        fetchNext(endpoints, 0, ip);
    }
    void fetchNext(const QList<QPair<QString, bool>>& endpoints, int idx, const QString& ip) {
        if (idx >= endpoints.size()) {
            m_pub->setPlainText(QStringLiteral("查询失败：所有接口均不可达（请检查网络）"));
            return;
        }
        QNetworkRequest req(QUrl(endpoints[idx].first));
        req.setTransferTimeout(8000);
        QNetworkReply* reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, endpoints, idx, ip] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) { fetchNext(endpoints, idx + 1, ip); return; }
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) { fetchNext(endpoints, idx + 1, ip); return; }
            QJsonObject o = doc.object();
            if (endpoints[idx].second) {
                // ipwho.is
                if (!o.value(QStringLiteral("success")).toBool(true)) { fetchNext(endpoints, idx + 1, ip); return; }
                QStringList out;
                out << QStringLiteral("IP 地址:  %1").arg(o.value(QStringLiteral("ip")).toString());
                out << QStringLiteral("国家:     %1").arg(o.value(QStringLiteral("country")).toString());
                out << QStringLiteral("省份/地区: %1").arg(o.value(QStringLiteral("region")).toString());
                out << QStringLiteral("城市:     %1").arg(o.value(QStringLiteral("city")).toString());
                QJsonObject conn = o.value(QStringLiteral("connection")).toObject();
                out << QStringLiteral("运营商:   %1").arg(conn.value(QStringLiteral("isp")).toString());
                out << QStringLiteral("ASN:      %1").arg(conn.value(QStringLiteral("asn")).toString());
                out << QStringLiteral("时区:     %1").arg(o.value(QStringLiteral("timezone")).toString());
                m_pub->setPlainText(out.join(QLatin1Char('\n')));
            } else {
                // ip-api.com
                if (o.value(QStringLiteral("status")).toString() != QStringLiteral("success")) {
                    m_pub->setPlainText(QStringLiteral("查询失败: %1").arg(o.value(QStringLiteral("message")).toString()));
                    return;
                }
                QStringList out;
                out << QStringLiteral("IP 地址:  %1").arg(o.value(QStringLiteral("query")).toString());
                out << QStringLiteral("国家:     %1").arg(o.value(QStringLiteral("country")).toString());
                out << QStringLiteral("省份/地区: %1").arg(o.value(QStringLiteral("regionName")).toString());
                out << QStringLiteral("城市:     %1").arg(o.value(QStringLiteral("city")).toString());
                out << QStringLiteral("运营商:   %1").arg(o.value(QStringLiteral("isp")).toString());
                out << QStringLiteral("ASN:      %1").arg(o.value(QStringLiteral("as")).toString());
                m_pub->setPlainText(out.join(QLatin1Char('\n')));
            }
        });
    }
    QLineEdit* m_query; QPlainTextEdit* m_pub; QPlainTextEdit* m_local;
    QNetworkAccessManager m_nam;
};

} // namespace

namespace pages {
ToolPage* createPing() { return new PingPage; }
ToolPage* createTraceroute() { return new TraceroutePage; }
ToolPage* createPortScan() { return new PortScanPage; }
ToolPage* createDns() { return new DnsPage; }
ToolPage* createHttpClient() { return new HttpPage; }
ToolPage* createSubnet() { return new SubnetPage; }
ToolPage* createWhois() { return new WhoisPage; }
ToolPage* createNicInfo() { return new NicPage; }
ToolPage* createIpInfo() { return new IpInfoPage; }
} // namespace pages

#include "nettoolspages.moc"
