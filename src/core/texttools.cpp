#include "texttools.h"
#include <QRandomGenerator>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QHash>
#include <QRegularExpression>
#include <yaml-cpp/yaml.h>

namespace texttools {

QString RandomOptions::generate() const {
    QString pool;
    if (lower) pool += QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    if (upper) pool += QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (digits) pool += QStringLiteral("0123456789");
    if (symbols) pool += QStringLiteral("!@#$%^&*()-_=+[]{};:,.<>?");
    if (!customChars.isEmpty()) pool += customChars;
    if (excludeAmbiguous) {
        for (const QChar& c : QStringLiteral("0O1lI|`'\";:,."))
            pool.remove(c);
    }
    if (pool.isEmpty()) return QStringLiteral("【请至少选择一种字符集】");
    QString out;
    QRandomGenerator* rng = QRandomGenerator::system();
    for (int i = 0; i < count; ++i) {
        QString line;
        for (int j = 0; j < length; ++j)
            line += pool.at(static_cast<int>(rng->bounded(static_cast<quint32>(pool.size()))));
        out += line + QLatin1Char('\n');
    }
    return out.trimmed();
}

QString uuidGenerate(bool upper, bool braces, bool noDash) {
    QString u = QUuid::createUuid().toString();
    if (noDash) u.remove(QLatin1Char('-'));
    if (braces && !noDash) u = QStringLiteral("{%1}").arg(u);
    if (upper) u = u.toUpper();
    return u;
}

static JsonResult fail(const QString& e) { JsonResult r; r.error = e; return r; }

JsonResult jsonFormat(const QString& input, int indent) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &err);
    if (doc.isNull())
        return fail(QStringLiteral("JSON 解析错误（偏移 %1）: %2").arg(err.offset).arg(err.errorString()));
    JsonResult r;
    r.ok = true;
    QString raw = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    if (indent == 2) {
        QStringList lines = raw.split(QLatin1Char('\n'));
        QStringList out;
        for (const QString& line : lines) {
            int spaces = 0;
            while (spaces < line.size() && line.at(spaces) == QLatin1Char(' ')) ++spaces;
            out += QString(QLatin1Char(' ')).repeated(spaces / 2) + line.mid(spaces);
        }
        raw = out.join(QLatin1Char('\n'));
    }
    r.text = raw;
    return r;
}

JsonResult jsonCompact(const QString& input) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &err);
    if (doc.isNull())
        return fail(QStringLiteral("JSON 解析错误（偏移 %1）: %2").arg(err.offset).arg(err.errorString()));
    JsonResult r;
    r.ok = true;
    r.text = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    return r;
}

QString jsonEscape(const QString& input) {
    QJsonDocument doc(QJsonObject{{QStringLiteral("v"), input}});
    QString s = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    qsizetype start = s.indexOf(QStringLiteral(":\"")) + 2;
    return s.mid(start, s.size() - start - 2);
}

QString jsonUnescape(const QString& input) {
    QString t = input.trimmed();
    QJsonDocument doc = QJsonDocument::fromJson(QStringLiteral("{\"v\":%1}").arg(t).toUtf8());
    if (!doc.isNull() && doc.object().value(QStringLiteral("v")).isString())
        return doc.object().value(QStringLiteral("v")).toString();
    return t;
}

// ---------- YAML <-> JSON ----------
static QJsonValue yamlNodeToJson(const YAML::Node& node);
static QJsonValue yamlSeqToJson(const YAML::Node& node) {
    QJsonArray arr;
    for (const auto& item : node) arr.append(yamlNodeToJson(item));
    return arr;
}
static QJsonValue yamlMapToJson(const YAML::Node& node) {
    QJsonObject obj;
    for (const auto& item : node) {
        QString key;
        try {
            key = QString::fromStdString(item.first.as<std::string>());
        } catch (...) {
            key = QStringLiteral("?");
        }
        obj.insert(key, yamlNodeToJson(item.second));
    }
    return obj;
}
static QJsonValue yamlNodeToJson(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Sequence:
            return yamlSeqToJson(node);
        case YAML::NodeType::Map:
            return yamlMapToJson(node);
        case YAML::NodeType::Null:
        default:
            return QJsonValue();
        case YAML::NodeType::Scalar: {
            std::string s;
            try {
                s = node.as<std::string>();
            } catch (...) {
                return QJsonValue(QString());
            }
            try {
                return QJsonValue(node.as<double>());
            } catch (...) {
            }
            try {
                bool b = node.as<bool>();
                return QJsonValue(b);
            } catch (...) {
            }
            return QJsonValue(QString::fromStdString(s));
        }
    }
}

JsonResult yamlToJson(const QString& input) {
    try {
        YAML::Node root = YAML::Load(input.toStdString());
        QJsonDocument doc;
        QJsonValue v = yamlNodeToJson(root);
        if (v.isArray())
            doc = QJsonDocument(v.toArray());
        else if (v.isObject())
            doc = QJsonDocument(v.toObject());
        JsonResult r;
        r.ok = true;
        r.text = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        return r;
    } catch (const std::exception& e) {
        return fail(QStringLiteral("YAML 解析错误: %1").arg(QString::fromUtf8(e.what())));
    }
}

static void jsonToYamlNode(const QJsonValue& v, YAML::Node& node) {
    if (v.isObject()) {
        const QJsonObject obj = v.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            YAML::Node child;
            jsonToYamlNode(it.value(), child);
            node[it.key().toStdString()] = child;
        }
    } else if (v.isArray()) {
        for (const QJsonValue& item : v.toArray()) {
            YAML::Node child;
            jsonToYamlNode(item, child);
            node.push_back(child);
        }
    } else if (v.isDouble()) {
        double d = v.toDouble();
        if (d == static_cast<double>(static_cast<qlonglong>(d)))
            node = static_cast<qlonglong>(d);
        else
            node = d;
    } else if (v.isBool()) {
        node = v.toBool();
    } else if (v.isNull()) {
        node = YAML::Null;
    } else {
        node = v.toString().toStdString();
    }
}

JsonResult jsonToYaml(const QString& input) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &err);
    if (doc.isNull())
        return fail(QStringLiteral("JSON 解析错误（偏移 %1）: %2").arg(err.offset).arg(err.errorString()));
    YAML::Node root;
    if (doc.isObject())
        jsonToYamlNode(QJsonValue(doc.object()), root);
    else if (doc.isArray())
        jsonToYamlNode(QJsonValue(doc.array()), root);
    else
        return fail(QStringLiteral("仅支持 JSON 对象或数组"));
    YAML::Emitter em;
    em << root;
    JsonResult r;
    r.ok = true;
    r.text = QString::fromStdString(std::string(em.c_str())) + QLatin1Char('\n');
    return r;
}

// ---------- Cron ----------
namespace {

struct CronBits {
    bool min[60], hour[24], dom[32], month[13], dow[8];
    bool domRestricted = false, dowRestricted = false; // 是否为具体限定（非 *）
};

// 解析单个字段，支持 * , - / 数字与英文缩写
bool parseField(const QString& field, int minV, int maxV, const QHash<QString, int>& names, bool bits[], int bitsSize, bool* isWild) {
    for (int i = 0; i < bitsSize; ++i) bits[i] = false;
    *isWild = false;
    if (field == QStringLiteral("*")) {
        *isWild = true;
        for (int i = minV; i <= maxV; ++i) bits[i] = true;
        return true;
    }
    const QStringList parts = field.split(QLatin1Char(','));
    for (const QString& rawPart : parts) {
        QString part = rawPart.trimmed();
        if (part.isEmpty()) return false;
        int step = 1;
        qsizetype slash = part.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            bool ok = false;
            step = part.mid(slash + 1).toInt(&ok);
            if (!ok || step <= 0) return false;
            part = part.left(slash);
        }
        int from = minV, to = maxV;
        if (part != QStringLiteral("*")) {
            qsizetype dash = part.indexOf(QLatin1Char('-'));
            if (dash >= 0) {
                QString a = part.left(dash), b = part.mid(dash + 1);
                bool ok1 = false, ok2 = false;
                from = names.contains(a.toUpper()) ? names.value(a.toUpper()) : a.toInt(&ok1);
                to = names.contains(b.toUpper()) ? names.value(b.toUpper()) : b.toInt(&ok2);
                if ((!ok1 && !names.contains(a.toUpper())) || (!ok2 && !names.contains(b.toUpper()))) return false;
            } else {
                if (names.contains(part.toUpper()))
                    from = to = names.value(part.toUpper());
                else {
                    bool ok = false;
                    from = to = part.toInt(&ok);
                    if (!ok) return false;
                }
            }
        } else if (slash < 0) {
            *isWild = true;
        }
        if (from < minV || to > maxV || from > to) return false;
        if (slash >= 0 && part == QStringLiteral("*")) *isWild = true;
        for (int i = from; i <= to; i += step) {
            if (i < bitsSize) bits[i] = true;
        }
    }
    return true;
}

QDateTime cronNextAfter(const CronBits& c, QDateTime t) {
    t = t.addSecs(60);
    t = QDateTime(QDate(t.date().year(), t.date().month(), t.date().day()), QTime(0, 0));
    const QDateTime limit = t.addYears(4);
    while (t <= limit) {
        if (!c.month[t.date().month()]) {
            // 跳到下个月 1 日
            int y = t.date().year(), m = t.date().month() + 1;
            if (m > 12) { m = 1; ++y; }
            t = QDateTime(QDate(y, m, 1), QTime(0, 0));
            continue;
        }
        // 日 与 星期 混合规则：两者都受限时取并集（模拟经典 cron）
        bool domOk = c.dom[t.date().day()];
        int dow = t.date().dayOfWeek() % 7; // Qt: 1=周一..7=周日; cron: 0=周日
        bool dowOk = c.dow[dow];
        bool dayOk;
        if (c.domRestricted && c.dowRestricted)
            dayOk = domOk || dowOk;
        else if (c.domRestricted)
            dayOk = domOk;
        else if (c.dowRestricted)
            dayOk = dowOk;
        else
            dayOk = true;
        if (!dayOk) {
            t = t.addDays(1);
            continue;
        }
        for (int h = 0; h < 24; ++h) {
            if (!c.hour[h]) continue;
            for (int mi = 0; mi < 60; ++mi) {
                if (c.min[mi]) {
                    QDateTime cand(t.date(), QTime(h, mi));
                    // 只返回严格晚于当前时刻的执行点，避免给出今天已过去的时间
                    if (cand > QDateTime::currentDateTime())
                        return cand;
                }
            }
        }
        t = t.addDays(1);
    }
    return QDateTime();
}

} // namespace

CronInfo cronParse(const QString& expr, const QDateTime& from) {
    CronInfo info;
    QString e = expr.trimmed();
    if (e.isEmpty()) { info.error = QStringLiteral("请输入 Cron 表达式"); return info; }
    QStringList f = e.split(QRegularExpression(QStringLiteral("\\s+")));
    if (f.size() != 5) { info.error = QStringLiteral("需要 5 个字段: 分 时 日 月 周"); return info; }

    QHash<QString, int> monthNames{{QStringLiteral("JAN"),1},{QStringLiteral("FEB"),2},{QStringLiteral("MAR"),3},{QStringLiteral("APR"),4},
        {QStringLiteral("MAY"),5},{QStringLiteral("JUN"),6},{QStringLiteral("JUL"),7},{QStringLiteral("AUG"),8},
        {QStringLiteral("SEP"),9},{QStringLiteral("OCT"),10},{QStringLiteral("NOV"),11},{QStringLiteral("DEC"),12}};
    QHash<QString, int> dowNames{{QStringLiteral("SUN"),0},{QStringLiteral("MON"),1},{QStringLiteral("TUE"),2},{QStringLiteral("WED"),3},
        {QStringLiteral("THU"),4},{QStringLiteral("FRI"),5},{QStringLiteral("SAT"),6}};

    // bool w1/w2 声明于上方；w3/w4 由 parseField 输出
    bool w1 = false, w2 = false, w3 = false, w4 = false;
    CronBits c{};
    for (QString& s : f) {
        if (s == QStringLiteral("?")) s = QStringLiteral("*");
    }
    if (!parseField(f[0], 0, 59, {}, c.min, 60, &w1)) { info.error = QStringLiteral("分钟字段无效: %1").arg(f[0]); return info; }
    if (!parseField(f[1], 0, 23, {}, c.hour, 24, &w2)) { info.error = QStringLiteral("小时字段无效: %1").arg(f[1]); return info; }
    if (!parseField(f[2], 1, 31, {}, c.dom, 32, &w3)) { info.error = QStringLiteral("日字段无效: %1").arg(f[2]); return info; }
    if (!parseField(f[3], 1, 12, monthNames, c.month, 13, &w4)) { info.error = QStringLiteral("月字段无效: %1").arg(f[3]); return info; }
    // 周字段 0-7（7 视为 0）
    {
        QString d = f[4];
        bool isWild = false;
        if (!parseField(d, 0, 7, dowNames, c.dow, 8, &isWild)) { info.error = QStringLiteral("周字段无效: %1").arg(f[4]); return info; }
        if (c.dow[7]) c.dow[0] = true;
        c.dow[7] = false;
        c.dowRestricted = !isWild;
    }
    c.domRestricted = !w3;

    info.describe = QStringLiteral("分钟: %1\n小时: %2\n日: %3\n月: %4\n星期: %5")
                        .arg(f[0], f[1], f[2], f[3], f[4]);

    QDateTime t = from;
    for (int i = 0; i < 5; ++i) {
        QDateTime next = cronNextAfter(c, t);
        if (!next.isValid()) break;
        info.nextRuns << next;
        t = next;
    }
    info.ok = true;
    return info;
}

// ---------- Diff ----------
DiffResult diffLines(const QString& aText, const QString& bText) {
    DiffResult r;
    QStringList a = aText.isEmpty() ? QStringList() : aText.split(QLatin1Char('\n'));
    QStringList b = bText.isEmpty() ? QStringList() : bText.split(QLatin1Char('\n'));

    // 去公共前后缀，减小 LCS 规模
    int pre = 0;
    while (pre < a.size() && pre < b.size() && a.at(pre) == b.at(pre)) ++pre;
    int suf = 0;
    while (suf < a.size() - pre && suf < b.size() - pre && a.at(a.size() - 1 - suf) == b.at(b.size() - 1 - suf)) ++suf;

    QStringList ca = a.mid(pre, a.size() - pre - suf);
    QStringList cb = b.mid(pre, b.size() - pre - suf);

    const int n = ca.size(), m = cb.size();
    if (static_cast<qlonglong>(n) * m > 4000000)
        return DiffResult{false, QStringLiteral("文本过长（差异核心超过 2000×2000 行），请缩小比较范围"), {}};

    // LCS 动态规划
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = n - 1; i >= 0; --i)
        for (int j = m - 1; j >= 0; --j)
            dp[i][j] = (ca.at(i) == cb.at(j)) ? dp[i + 1][j + 1] + 1 : std::max(dp[i + 1][j], dp[i][j + 1]);

    QList<DiffResult::Line> core;
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (ca.at(i) == cb.at(j)) { core.append({' ', ca.at(i)}); ++i; ++j; }
        else if (dp[i + 1][j] >= dp[i][j + 1]) { core.append({'-', ca.at(i)}); ++i; }
        else { core.append({'+', cb.at(j)}); ++j; }
    }
    while (i < n) core.append({'-', ca.at(i++)});
    while (j < m) core.append({'+', cb.at(j++)});

    for (int k = 0; k < pre; ++k) r.lines.append({' ', a.at(k)});
    r.lines += core;
    for (int k = 0; k < suf; ++k) r.lines.append({' ', a.at(a.size() - suf + k)});

    r.ok = true;
    return r;
}

// ---------- 文本批处理 ----------
QString textProcess(const QString& input, const QString& op) {
    QStringList lines = input.split(QLatin1Char('\n'));
    if (op == QStringLiteral("dedupe")) {
        QSet<QString> seen;
        QStringList out;
        for (const QString& l : lines)
            if (!seen.contains(l)) { seen.insert(l); out << l; }
        return out.join(QLatin1Char('\n'));
    }
    if (op == QStringLiteral("sort")) { std::sort(lines.begin(), lines.end()); return lines.join(QLatin1Char('\n')); }
    if (op == QStringLiteral("rsort")) { std::sort(lines.begin(), lines.end(), std::greater<QString>()); return lines.join(QLatin1Char('\n')); }
    if (op == QStringLiteral("reverse")) { std::reverse(lines.begin(), lines.end()); return lines.join(QLatin1Char('\n')); }
    if (op == QStringLiteral("trim")) {
        for (QString& l : lines) l = l.trimmed();
        return lines.join(QLatin1Char('\n'));
    }
    if (op == QStringLiteral("norempty")) {
        QStringList out;
        for (const QString& l : lines)
            if (!l.trimmed().isEmpty()) out << l;
        return out.join(QLatin1Char('\n'));
    }
    if (op == QStringLiteral("number")) {
        QStringList out;
        for (int i = 0; i < lines.size(); ++i) out << QStringLiteral("%1. %2").arg(i + 1).arg(lines.at(i));
        return out.join(QLatin1Char('\n'));
    }
    if (op == QStringLiteral("upper")) return input.toUpper();
    if (op == QStringLiteral("lower")) return input.toLower();
    if (op == QStringLiteral("capitalize")) {
        QStringList out;
        for (const QString& l : lines) {
            QString t = l.toLower();
            if (!t.isEmpty()) t[0] = t.at(0).toUpper();
            out << t;
        }
        return out.join(QLatin1Char('\n'));
    }
    return input;
}

TextStats textStats(const QString& input) {
    TextStats s;
    s.lines = input.isEmpty() ? 0 : input.count(QLatin1Char('\n')) + (input.endsWith(QLatin1Char('\n')) ? 0 : 1);
    s.chars = input.size();
    s.bytes = input.toUtf8().size();
    static const QRegularExpression wordRe(QStringLiteral("[A-Za-z0-9_]+|[ä¸-é¾¥]+"));
    qsizetype words = 0;
    auto wit = wordRe.globalMatch(input);
    while (wit.hasNext()) { wit.next(); ++words; }
    s.words = static_cast<int>(words);
    return s;
}

// ---------- SQL 格式化（基础版） ----------
JsonResult sqlFormat(const QString& input) {
    if (input.trimmed().isEmpty()) return fail(QStringLiteral("请输入 SQL"));
    QStringList major = {QStringLiteral("FROM"), QStringLiteral("WHERE"), QStringLiteral("GROUP BY"), QStringLiteral("HAVING"),
                         QStringLiteral("ORDER BY"), QStringLiteral("LIMIT"), QStringLiteral("OFFSET"), QStringLiteral("UNION"),
                         QStringLiteral("UNION ALL"), QStringLiteral("VALUES"), QStringLiteral("SET"), QStringLiteral("RETURNING")};
    QStringList joins = {QStringLiteral("LEFT JOIN"), QStringLiteral("RIGHT JOIN"), QStringLiteral("INNER JOIN"),
                         QStringLiteral("FULL JOIN"), QStringLiteral("OUTER JOIN"), QStringLiteral("CROSS JOIN"), QStringLiteral("JOIN")};
    QString s = input;
    // 归一化空白
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    s = s.trimmed();

    // 用正则在关键字前断行（大小写不敏感、避免匹配字符串字面量中的情况——基础版忽略该细节）
    QStringList keys = joins + major;
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& k : keys) {
        static const QHash<QString, QRegularExpression> cache;
        QRegularExpression re(QStringLiteral("\\s+(?i\\b%1\\b)\\s*").arg(QRegularExpression::escape(k)));
        s = s.replace(re, QStringLiteral("\n%1 ").arg(k));
    }
    s = s.replace(QRegularExpression(QStringLiteral("(?i)\\s*\\bSELECT\\b\\s*")), QStringLiteral("SELECT "));

    QStringList lines = s.split(QLatin1Char('\n'));
    QStringList out;
    int indent = 0;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        // SELECT 列表按逗号断行
        bool isSelect = line.startsWith(QStringLiteral("SELECT "), Qt::CaseInsensitive);
        if (isSelect) {
            out << QStringLiteral("SELECT");
            ++indent;
            QString rest = line.mid(6).trimmed();
            QStringList cols = rest.split(QLatin1Char(','));
            for (int i = 0; i < cols.size(); ++i)
                out << QString(QLatin1Char(' ')).repeated(indent * 2) + cols.at(i).trimmed() + (i < cols.size() - 1 ? QStringLiteral(",") : QString());
            --indent;
            continue;
        }
        bool isJoin = false;
        for (const QString& j : joins)
            if (line.startsWith(j + QLatin1Char(' '), Qt::CaseInsensitive)) { isJoin = true; break; }
        int thisIndent = indent;
        if (line.startsWith(QStringLiteral("FROM "), Qt::CaseInsensitive)) thisIndent = 0;
        else if (line.startsWith(QStringLiteral("WHERE "), Qt::CaseInsensitive)) { thisIndent = 0; indent = 1; }
        else if (isJoin) thisIndent = 1;
        else if (line.startsWith(QStringLiteral("GROUP BY"), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("ORDER BY"), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("LIMIT"), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("HAVING"), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("UNION"), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("VALUES"), Qt::CaseInsensitive))
            thisIndent = 0;
        else if (line.startsWith(QStringLiteral("AND "), Qt::CaseInsensitive) ||
                 line.startsWith(QStringLiteral("OR "), Qt::CaseInsensitive))
            thisIndent = 1;
        out << QString(QLatin1Char(' ')).repeated(thisIndent * 2) + line;
    }
    JsonResult r;
    r.ok = true;
    r.text = out.join(QLatin1Char('\n'));
    // 关键字统一大写（跳过引号字符串，逐词处理，多词关键字如 GROUP/BY 各自成词）
    static const QSet<QString> kws = {
        QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"), QStringLiteral("and"),
        QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("null"), QStringLiteral("like"),
        QStringLiteral("in"), QStringLiteral("between"), QStringLiteral("as"), QStringLiteral("on"),
        QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("inner"), QStringLiteral("outer"),
        QStringLiteral("join"), QStringLiteral("group"), QStringLiteral("order"), QStringLiteral("by"),
        QStringLiteral("asc"), QStringLiteral("desc"), QStringLiteral("limit"), QStringLiteral("offset"),
        QStringLiteral("insert"), QStringLiteral("into"), QStringLiteral("values"), QStringLiteral("update"),
        QStringLiteral("set"), QStringLiteral("delete"), QStringLiteral("distinct"), QStringLiteral("union"),
        QStringLiteral("all"), QStringLiteral("having"), QStringLiteral("case"), QStringLiteral("when"),
        QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("end"), QStringLiteral("is"),
        QStringLiteral("exists"), QStringLiteral("count"), QStringLiteral("sum"), QStringLiteral("avg"),
        QStringLiteral("max"), QStringLiteral("min")};
    QString src = r.text;
    QString result;
    qsizetype i = 0;
    const qsizetype n = src.size();
    while (i < n) {
        QChar ch = src.at(i);
        if (ch == QLatin1Char('\'')) {
            qsizetype j = i + 1;
            while (j < n && src.at(j) != QLatin1Char('\'')) ++j;
            result += src.mid(i, qMin(j + 1, n) - i);
            i = j + 1;
            continue;
        }
        if (ch.isLetter() || ch == QLatin1Char('_')) {
            qsizetype j = i;
            while (j < n && (src.at(j).isLetterOrNumber() || src.at(j) == QLatin1Char('_'))) ++j;
            QString word = src.mid(i, j - i);
            result += kws.contains(word.toLower()) ? word.toUpper() : word;
            i = j;
            continue;
        }
        result += ch;
        ++i;
    }
    r.text = result;
    return r;
}

} // namespace texttools
