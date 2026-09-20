#include "registry.h"
#include "tools/pages.h"

#include <QHash>

static const QString CatNet = QStringLiteral("网络诊断");
static const QString CatCodec = QStringLiteral("编解码");
static const QString CatCrypto = QStringLiteral("加密哈希");
static const QString CatText = QStringLiteral("文本开发");
static const QString CatK8s = QStringLiteral("容器运维");
static const QString CatSys = QStringLiteral("系统信息");

QList<ToolMeta>& ToolRegistry::all() {
    static QList<ToolMeta> tools = [] {
        QList<ToolMeta> list;
        auto add = [&list](const QString& id, const QString& name, const QString& desc, const QString& category,
                           const QString& icon, const QStringList& kw, std::function<ToolPage*()> fn) {
            ToolMeta m;
            m.id = id; m.name = name; m.desc = desc; m.category = category;
            m.icon = icon; m.keywords = kw; m.create = std::move(fn);
            list << m;
        };

        // ---------- 网络诊断 ----------
        add(QStringLiteral("ipinfo"), QStringLiteral("IP 查询"), QStringLiteral("本机 IP、公网 IP 与归属地查询"), CatNet, QStringLiteral("wifi"),
            {"ip", "公网", "归属地", "geolocation"}, &pages::createIpInfo);
        add(QStringLiteral("dns"), QStringLiteral("DNS 解析"), QStringLiteral("域名解析 A/AAAA/CNAME/MX/TXT/NS/SRV"), CatNet, QStringLiteral("globe"),
            {"dns", "解析", "域名", "domain"}, &pages::createDns);
        add(QStringLiteral("ping"), QStringLiteral("Ping"), QStringLiteral("连通性测试，丢包率与延迟统计"), CatNet, QStringLiteral("wifi"),
            {"ping", "延迟", "丢包"}, &pages::createPing);
        add(QStringLiteral("traceroute"), QStringLiteral("路由追踪"), QStringLiteral("Traceroute 查看到目标的每一跳"), CatNet, QStringLiteral("globe"),
            {"traceroute", "tracert", "路由", "追踪"}, &pages::createTraceroute);
        add(QStringLiteral("portscan"), QStringLiteral("端口扫描"), QStringLiteral("TCP 端口连通性检测与扫描"), CatNet, QStringLiteral("terminal"),
            {"port", "端口", "扫描", "scan"}, &pages::createPortScan);
        add(QStringLiteral("http"), QStringLiteral("HTTP 请求"), QStringLiteral("发送 HTTP 请求，查看响应与耗时"), CatNet, QStringLiteral("globe"),
            {"http", "接口", "api", "post", "get", "请求"}, &pages::createHttpClient);
        add(QStringLiteral("subnet"), QStringLiteral("子网计算器"), QStringLiteral("CIDR/掩码换算，网段与可用主机计算"), CatNet, QStringLiteral("grid"),
            {"subnet", "子网", "掩码", "cidr", "网段"}, &pages::createSubnet);
        add(QStringLiteral("whois"), QStringLiteral("Whois 查询"), QStringLiteral("域名注册信息查询"), CatNet, QStringLiteral("info"),
            {"whois", "域名", "注册"}, &pages::createWhois);
        add(QStringLiteral("nic"), QStringLiteral("网卡信息"), QStringLiteral("本机网卡、IP 与 MAC 地址一览"), CatNet, QStringLiteral("cpu"),
            {"nic", "网卡", "mac", "ipconfig"}, &pages::createNicInfo);

        // ---------- 编解码 ----------
        add(QStringLiteral("base64"), QStringLiteral("Base64"), QStringLiteral("文本与文件 Base64 编解码"), CatCodec, QStringLiteral("code"),
            {"base64", "编码", "解码", "encode", "decode"}, &pages::createBase64);
        add(QStringLiteral("url"), QStringLiteral("URL 编解码"), QStringLiteral("URL 百分号编码与解码"), CatCodec, QStringLiteral("code"),
            {"url", "encode", "decode", "编码", "百分号"}, &pages::createUrlCodec);
        add(QStringLiteral("radix"), QStringLiteral("进制转换"), QStringLiteral("二/八/十/十六进制互转"), CatCodec, QStringLiteral("hash"),
            {"进制", "binary", "hex", "octal", "二进制", "十六进制"}, &pages::createRadix);
        add(QStringLiteral("unicode"), QStringLiteral("Unicode 转换"), QStringLiteral("中文与 \\uXXXX 转义互转"), CatCodec, QStringLiteral("code"),
            {"unicode", "utf-8", "转义", "编码"}, &pages::createUnicode);
        add(QStringLiteral("jwt"), QStringLiteral("JWT 解析"), QStringLiteral("解析 JWT 的 header/payload"), CatCodec, QStringLiteral("lock"),
            {"jwt", "token", "鉴权", "令牌"}, &pages::createJwt);
        add(QStringLiteral("htmlent"), QStringLiteral("HTML 实体"), QStringLiteral("HTML 实体编码与解码"), CatCodec, QStringLiteral("code"),
            {"html", "实体", "entity", "转义"}, &pages::createHtmlEntity);

        // ---------- 加密哈希 ----------
        add(QStringLiteral("hash"), QStringLiteral("哈希计算"), QStringLiteral("MD5/SHA1/SHA256/SHA512 摘要"), CatCrypto, QStringLiteral("hash"),
            {"hash", "md5", "sha", "摘要", "哈希"}, &pages::createHash);
        add(QStringLiteral("filehash"), QStringLiteral("文件校验"), QStringLiteral("计算文件哈希并比对校验值"), CatCrypto, QStringLiteral("file"),
            {"文件", "file", "校验", "hash", "checksum"}, &pages::createFileHash);
        add(QStringLiteral("aes"), QStringLiteral("AES 加解密"), QStringLiteral("AES-GCM/CBC/ECB 加密与解密"), CatCrypto, QStringLiteral("lock"),
            {"aes", "加密", "解密", "encrypt", "decrypt"}, &pages::createAes);
        add(QStringLiteral("rsa"), QStringLiteral("RSA 密钥生成"), QStringLiteral("生成 RSA 公私钥对（PEM）"), CatCrypto, QStringLiteral("lock"),
            {"rsa", "密钥", "key", "公钥", "私钥"}, &pages::createRsa);

        // ---------- 文本开发 ----------
        add(QStringLiteral("random"), QStringLiteral("随机字符串"), QStringLiteral("密码/随机串批量生成"), CatText, QStringLiteral("zap"),
            {"随机", "密码", "random", "password", "字符串"}, &pages::createRandom);
        add(QStringLiteral("uuid"), QStringLiteral("UUID 生成"), QStringLiteral("批量生成 UUID v4"), CatText, QStringLiteral("hash"),
            {"uuid", "guid", "唯一标识"}, &pages::createUuid);
        add(QStringLiteral("json"), QStringLiteral("JSON 工具"), QStringLiteral("格式化、压缩、校验、转义"), CatText, QStringLiteral("code"),
            {"json", "格式化", "format", "压缩"}, &pages::createJson);
        add(QStringLiteral("yaml"), QStringLiteral("YAML/JSON"), QStringLiteral("YAML 与 JSON 互转"), CatText, QStringLiteral("code"),
            {"yaml", "json", "互转", "k8s"}, &pages::createYaml);
        add(QStringLiteral("timestamp"), QStringLiteral("时间戳"), QStringLiteral("Unix 时间戳与日期互转"), CatText, QStringLiteral("clock"),
            {"时间戳", "timestamp", "unix", "日期"}, &pages::createTimestamp);
        add(QStringLiteral("regex"), QStringLiteral("正则测试"), QStringLiteral("正则表达式实时匹配与分组"), CatText, QStringLiteral("terminal"),
            {"正则", "regex", "regexp", "匹配"}, &pages::createRegex);
        add(QStringLiteral("diff"), QStringLiteral("文本对比"), QStringLiteral("两段文本差异高亮对比"), CatText, QStringLiteral("file"),
            {"diff", "对比", "比较", "差异"}, &pages::createDiff);
        add(QStringLiteral("cron"), QStringLiteral("Cron 解析"), QStringLiteral("Cron 表达式解析与预览"), CatText, QStringLiteral("clock"),
            {"cron", "定时", "表达式", "crontab"}, &pages::createCron);
        add(QStringLiteral("qrcode"), QStringLiteral("二维码生成"), QStringLiteral("文本/链接生成二维码图片"), CatText, QStringLiteral("grid"),
            {"二维码", "qrcode", "qr"}, &pages::createQrcode);
        add(QStringLiteral("textproc"), QStringLiteral("文本处理"), QStringLiteral("去重、排序、统计、批处理"), CatText, QStringLiteral("file"),
            {"文本", "去重", "排序", "统计", "text"}, &pages::createTextProc);
        add(QStringLiteral("sqlfmt"), QStringLiteral("SQL 格式化"), QStringLiteral("SQL 语句美化排版"), CatText, QStringLiteral("terminal"),
            {"sql", "格式化", "format", "美化"}, &pages::createSqlFormat);

        // ---------- 容器运维 ----------
        add(QStringLiteral("k8scmd"), QStringLiteral("K8s 命令生成器"),
            QStringLiteral("kubectl 可视化生成：查看/日志/事件/调试/发布/RBAC/集群"), CatK8s, QStringLiteral("box"),
            {"k8s", "kubernetes", "kubectl", "容器", "pod", "deploy", "命令", "运维"}, &pages::createK8sCmd);
        add(QStringLiteral("k8syaml"), QStringLiteral("K8s YAML 模板"),
            QStringLiteral("Deployment/Service/Ingress/RBAC 等常用资源清单模板"), CatK8s, QStringLiteral("file"),
            {"yaml", "模板", "k8s", "deployment", "ingress", "rbac", "清单"}, &pages::createK8sYaml);

        // ---------- 系统信息 ----------
        add(QStringLiteral("sysinfo"), QStringLiteral("系统信息"), QStringLiteral("操作系统、CPU、内存概览"), CatSys, QStringLiteral("cpu"),
            {"系统", "system", "cpu", "内存", "版本"}, &pages::createSysInfo);
        add(QStringLiteral("urlparser"), QStringLiteral("URL 解析"), QStringLiteral("拆解 URL 各组成部分"), CatSys, QStringLiteral("globe"),
            {"url", "解析", "参数", "query"}, &pages::createUrlParser);
        add(QStringLiteral("color"), QStringLiteral("颜色转换"), QStringLiteral("HEX/RGB/HSL 颜色互转"), CatSys, QStringLiteral("grid"),
            {"颜色", "color", "hex", "rgb", "hsl"}, &pages::createColor);

        return list;
    }();
    return tools;
}

const ToolMeta* ToolRegistry::find(const QString& id) {
    for (const ToolMeta& m : all())
        if (m.id == id) return &m;
    return nullptr;
}

QStringList ToolRegistry::categories() {
    return {CatK8s, CatNet, CatCodec, CatCrypto, CatText, CatSys};   // 容器运维置顶
}

QColor ToolRegistry::categoryColor(const QString& category) {
    static const QHash<QString, QColor> colors = {
        {QStringLiteral("网络诊断"), QColor(0x4F, 0x8C, 0xFF)},
        {QStringLiteral("编解码"),   QColor(0xA8, 0x78, 0xF0)},
        {QStringLiteral("加密哈希"), QColor(0x2E, 0xCC, 0x8F)},
        {QStringLiteral("文本开发"), QColor(0xFF, 0xA5, 0x4D)},
        {QStringLiteral("容器运维"), QColor(0xE8, 0x5D, 0x8A)},
        {QStringLiteral("系统信息"), QColor(0x2E, 0xC7, 0xD9)},
    };
    return colors.value(category, QColor(0x4F, 0x8C, 0xFF));
}
