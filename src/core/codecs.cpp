#include "codecs.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUrl>
#include <QDateTime>
#include <QRegularExpression>

namespace codecs {

static QByteArray b64(const QString& text, bool urlSafe) {
    auto flags = QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors;
    if (urlSafe) flags = QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors;
    return QByteArray::fromBase64Encoding(text.toUtf8(), flags).decoded;
}

QString base64Encode(const QString& text, bool urlSafe) {
    QByteArray out = text.toUtf8().toBase64(QByteArray::Base64Encoding);
    if (urlSafe) out.replace('+', '-').replace('/', '_');
    return QString::fromLatin1(out);
}

Base64Result base64Decode(const QString& text, bool urlSafe) {
    Base64Result r;
    auto flags = QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors;
    if (urlSafe)
        flags = QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors;
    auto res = QByteArray::fromBase64Encoding(text.trimmed().toUtf8(), flags);
    if (res.decodingStatus != QByteArray::Base64DecodingStatus::Ok) {
        r.error = QStringLiteral("Base64 数据无效（含非法字符或长度不对）");
        return r;
    }
    r.ok = true;
    r.text = QString::fromUtf8(res.decoded);
    return r;
}

QString fileToBase64(const QString& path, bool dataUri, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = QStringLiteral("无法打开文件: %1").arg(f.errorString()); return {}; }
    QByteArray data = f.readAll();
    QByteArray b = data.toBase64();
    if (dataUri) {
        QString mime = QStringLiteral("application/octet-stream");
        const QString suffix = QFileInfo(path).suffix().toLower();
        static const QHash<QString, QString> mimes = {
            {QStringLiteral("png"), QStringLiteral("image/png")}, {QStringLiteral("jpg"), QStringLiteral("image/jpeg")},
            {QStringLiteral("jpeg"), QStringLiteral("image/jpeg")}, {QStringLiteral("gif"), QStringLiteral("image/gif")},
            {QStringLiteral("webp"), QStringLiteral("image/webp")}, {QStringLiteral("svg"), QStringLiteral("image/svg+xml")},
            {QStringLiteral("bmp"), QStringLiteral("image/bmp")}, {QStringLiteral("ico"), QStringLiteral("image/x-icon")},
            {QStringLiteral("pdf"), QStringLiteral("application/pdf")}, {QStringLiteral("txt"), QStringLiteral("text/plain")},
            {QStringLiteral("json"), QStringLiteral("application/json")}, {QStringLiteral("mp3"), QStringLiteral("audio/mpeg")},
            {QStringLiteral("mp4"), QStringLiteral("video/mp4")}, {QStringLiteral("woff"), QStringLiteral("font/woff")},
            {QStringLiteral("woff2"), QStringLiteral("font/woff2")}};
        if (mimes.contains(suffix)) mime = mimes.value(suffix);
        return QStringLiteral("data:%1;base64,").arg(mime) + QString::fromLatin1(b);
    }
    return QString::fromLatin1(b);
}

bool base64ToFile(const QString& b64text, const QString& path, QString* err) {
    // 去掉可能的 data URI 前缀
    QString t = b64text.trimmed();
    const int comma = t.indexOf(',');
    if (t.startsWith(QStringLiteral("data:")) && comma > 0) t = t.mid(comma + 1);
    QByteArray bin = b64(t, false);
    if (bin.isEmpty() && !t.isEmpty()) { if (err) *err = QStringLiteral("Base64 数据无效"); return false; }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { if (err) *err = QStringLiteral("无法写入文件: %1").arg(f.errorString()); return false; }
    f.write(bin);
    return true;
}

QString urlEncode(const QString& text, bool plus4Space) {
    QByteArray enc = QUrl::toPercentEncoding(text);
    if (plus4Space) enc.replace("%20", "+");
    return QString::fromLatin1(enc);
}

QString urlDecode(const QString& text, bool plus4Space) {
    QByteArray bin = text.toUtf8();
    if (plus4Space) bin.replace('+', ' ');
    return QString::fromUtf8(QByteArray::fromPercentEncoding(bin));
}

RadixResult radixConvert(const QString& value, int fromBase) {
    RadixResult r;
    QString v = value.trimmed();
    if (v.isEmpty()) { r.error = QStringLiteral("请输入数值"); return r; }
    bool ok = false;
    quint64 u = 0;
    bool negative = false;
    if (fromBase == 10) {
        // 允许有符号十进制
        qlonglong s = v.toLongLong(&ok);
        if (ok) {
            negative = s < 0;
            u = negative ? quint64(-s) : quint64(s);
        } else {
            u = v.toULongLong(&ok);
        }
    } else {
        QString hexish = v;
        if (hexish.startsWith(QStringLiteral("0x")) || hexish.startsWith(QStringLiteral("0X"))) hexish = hexish.mid(2);
        u = hexish.toULongLong(&ok, fromBase);
    }
    if (!ok) { r.error = QStringLiteral("无法按 %1 进制解析输入").arg(fromBase); return r; }
    r.ok = true;
    auto pad = [](const QString& s, int width) {
        QString p = s;
        while (p.size() < width) p.prepend(QLatin1Char('0'));
        return p;
    };
    r.bin  = pad(QString::number(u, 2), 4);
    r.oct  = QString::number(u, 8);
    r.dec  = (negative ? QStringLiteral("-") : QString()) + QString::number(u, 10);
    r.hex  = QStringLiteral("0x") + QString::number(u, 16).toUpper();
    if (u <= 0x10FFFF && u >= 32) {
        char32_t ch = static_cast<char32_t>(u);
        if (QChar::requiresSurrogates(ch) || true) {
            // 仅展示可打印字符
            QString s = QString::fromUcs4(reinterpret_cast<const char32_t*>(&ch), 1);
            if (!s.isEmpty() && s.at(0).isPrint()) r.error = QStringLiteral("ASCII/Unicode 字符: %1").arg(s);
        }
    }
    return r;
}

QString unicodeEncode(const QString& text) {
    QString out;
    for (const QChar& ch : text) {
        char16_t u = ch.unicode();
        if (u < 128)
            out += ch;
        else
            out += QStringLiteral("\\u%1").arg(QString::number(u, 16).rightJustified(4, QLatin1Char('0')));
    }
    return out;
}

QString unicodeDecode(const QString& text) {
    QString out;
    static const QRegularExpression re(QStringLiteral("\\\\u([0-9a-fA-F]{4})"));
    qsizetype pos = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        out += text.mid(pos, m.capturedStart() - pos);
        bool ok = false;
        char16_t u = static_cast<char16_t>(m.captured(1).toUShort(&ok, 16));
        if (ok) out += QChar(u);
        pos = m.capturedEnd();
    }
    out += text.mid(pos);
    return out;
}

QString htmlEncode(const QString& text) {
    QString out;
    for (const QChar& ch : text) {
        switch (ch.unicode()) {
            case '&': out += QStringLiteral("&amp;"); break;
            case '<': out += QStringLiteral("&lt;"); break;
            case '>': out += QStringLiteral("&gt;"); break;
            case '"': out += QStringLiteral("&quot;"); break;
            case '\'': out += QStringLiteral("&#39;"); break;
            default: out += ch;
        }
    }
    return out;
}

static const QHash<QString, QChar> kHtmlEntities = {
    {QStringLiteral("amp"), QLatin1Char('&')}, {QStringLiteral("lt"), QLatin1Char('<')},
    {QStringLiteral("gt"), QLatin1Char('>')}, {QStringLiteral("quot"), QLatin1Char('"')},
    {QStringLiteral("apos"), QLatin1Char('\'')}, {QStringLiteral("nbsp"), QChar(0x00A0)},
    {QStringLiteral("copy"), QChar(0x00A9)}, {QStringLiteral("reg"), QChar(0x00AE)},
    {QStringLiteral("trade"), QChar(0x2122)}, {QStringLiteral("mdash"), QChar(0x2014)},
    {QStringLiteral("ndash"), QChar(0x2013)}, {QStringLiteral("hellip"), QChar(0x2026)},
    {QStringLiteral("times"), QChar(0x00D7)}, {QStringLiteral("divide"), QChar(0x00F7)},
    {QStringLiteral("deg"), QChar(0x00B0)}, {QStringLiteral("plusmn"), QChar(0x00B1)},
    {QStringLiteral("laquo"), QChar(0x00AB)}, {QStringLiteral("raquo"), QChar(0x00BB)},
    {QStringLiteral("middot"), QChar(0x00B7)}, {QStringLiteral("para"), QChar(0x00B6)},
    {QStringLiteral("sect"), QChar(0x00A7)}, {QStringLiteral("bull"), QChar(0x2022)},
    {QStringLiteral("rarr"), QChar(0x2192)}, {QStringLiteral("larr"), QChar(0x2190)},
    {QStringLiteral("uarr"), QChar(0x2191)}, {QStringLiteral("darr"), QChar(0x2193)},
};

QString htmlDecode(const QString& text) {
    QString out;
    qsizetype i = 0;
    while (i < text.size()) {
        QChar ch = text.at(i);
        if (ch == QLatin1Char('&')) {
            qsizetype semi = text.indexOf(QLatin1Char(';'), i + 1);
            if (semi > i && semi - i <= 12) {
                QString body = text.mid(i + 1, semi - i - 1);
                if (body.startsWith(QLatin1Char('#'))) {
                    bool ok = false;
                    uint code = 0;
                    if (body.size() > 1 && (body.at(1) == QLatin1Char('x') || body.at(1) == QLatin1Char('X')))
                        code = body.mid(2).toUInt(&ok, 16);
                    else
                        code = body.mid(1).toUInt(&ok, 10);
                    if (ok && code > 0 && code <= 0x10FFFF) {
                        if (QChar::requiresSurrogates(code)) {
                            out += QChar(QChar::highSurrogate(code));
                            out += QChar(QChar::lowSurrogate(code));
                        } else {
                            out += QChar(char16_t(code));
                        }
                        i = semi + 1;
                        continue;
                    }
                } else if (kHtmlEntities.contains(body)) {
                    out += kHtmlEntities.value(body);
                    i = semi + 1;
                    continue;
                }
            }
        }
        out += ch;
        ++i;
    }
    return out;
}

static QString b64urlDecodeJson(const QString& seg, bool* ok) {
    QString padded = seg;
    padded.replace('-', '+').replace('_', '/');
    while (padded.size() % 4) padded += QLatin1Char('=');
    QByteArray raw = QByteArray::fromBase64(padded.toLatin1());
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    *ok = !raw.isEmpty();
    if (doc.isNull()) return QString::fromUtf8(raw);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

JwtInfo jwtParse(const QString& token) {
    JwtInfo info;
    QString t = token.trimmed();
    if (t.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) t = t.mid(7).trimmed();
    QStringList parts = t.split(QLatin1Char('.'));
    if (parts.size() < 2) { info.error = QStringLiteral("不是有效的 JWT（至少需要 header.payload 两段）"); return info; }
    bool ok = false;
    info.header = b64urlDecodeJson(parts[0], &ok);
    if (!ok) { info.error = QStringLiteral("Header 段解码失败"); return info; }
    info.payload = b64urlDecodeJson(parts[1], &ok);
    if (!ok) { info.error = QStringLiteral("Payload 段解码失败"); return info; }
    if (parts.size() > 2) info.signature = parts.mid(2).join(QLatin1Char('.'));
    info.ok = true;

    QJsonDocument doc = QJsonDocument::fromJson(info.payload.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QStringList notes;
        auto fmt = [](qlonglong sec) {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(sec);
            return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        };
        if (obj.contains(QStringLiteral("exp"))) {
            qlonglong exp = static_cast<qlonglong>(obj.value(QStringLiteral("exp")).toDouble());
            QDateTime dt = QDateTime::fromSecsSinceEpoch(exp);
            notes << QStringLiteral("exp(过期): %1 %2").arg(fmt(exp),
                     dt <= QDateTime::currentDateTime() ? QStringLiteral("【已过期】") : QStringLiteral("【有效】"));
        }
        if (obj.contains(QStringLiteral("iat")))
            notes << QStringLiteral("iat(签发): %1").arg(fmt(static_cast<qlonglong>(obj.value(QStringLiteral("iat")).toDouble())));
        if (obj.contains(QStringLiteral("nbf")))
            notes << QStringLiteral("nbf(生效): %1").arg(fmt(static_cast<qlonglong>(obj.value(QStringLiteral("nbf")).toDouble())));
        if (!notes.isEmpty()) info.timeNotes = notes.join(QLatin1Char('\n'));
    }
    return info;
}

} // namespace codecs
