#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>

namespace texttools {

// 随机字符串
struct RandomOptions {
    int length = 16;
    int count = 1;
    bool lower = true, upper = true, digits = true, symbols = false;
    bool excludeAmbiguous = true; // 去掉 0O1lI| 等
    QString customChars;
    QString generate() const;
};

// UUID
QString uuidGenerate(bool upper, bool braces, bool noDash);

// JSON
struct JsonResult { bool ok = false; QString text, error; };
JsonResult jsonFormat(const QString& input, int indent);
JsonResult jsonCompact(const QString& input);
QString jsonEscape(const QString& input);   // 转为带引号的转义字符串
QString jsonUnescape(const QString& input);

// YAML <-> JSON
JsonResult yamlToJson(const QString& input);
JsonResult jsonToYaml(const QString& input);

// Cron
struct CronInfo {
    bool ok = false;
    QString error;
    QString describe;                 // 各字段含义
    QList<QDateTime> nextRuns;        // 之后 5 次执行时间
};
CronInfo cronParse(const QString& expr, const QDateTime& from);

// Diff（行级）
struct DiffResult { bool ok = false; QString error; struct Line { char op; QString text; }; QList<Line> lines; };
DiffResult diffLines(const QString& a, const QString& b);

// 文本批处理
struct TextStats { int lines = 0, chars = 0, words = 0, bytes = 0; };
QString textProcess(const QString& input, const QString& op);
TextStats textStats(const QString& input);

// 基础 SQL 格式化
JsonResult sqlFormat(const QString& input);

} // namespace texttools
