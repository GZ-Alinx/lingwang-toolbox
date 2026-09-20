#pragma once
#include <QString>
#include <QByteArray>

namespace codecs {

// Base64
struct Base64Result {
    bool ok = false;
    QString error, text;
};
QString base64Encode(const QString& text, bool urlSafe);
Base64Result base64Decode(const QString& text, bool urlSafe);
QString fileToBase64(const QString& path, bool dataUri, QString* err);
bool base64ToFile(const QString& b64, const QString& path, QString* err);

// URL
QString urlEncode(const QString& text, bool plus4Space);
QString urlDecode(const QString& text, bool plus4Space);

// 进制转换
struct RadixResult {
    bool ok = false;
    QString bin, oct, dec, hex, error;
};
RadixResult radixConvert(const QString& value, int fromBase);

// Unicode 转义
QString unicodeEncode(const QString& text);   // 中文 -> \uXXXX
QString unicodeDecode(const QString& text);   // \uXXXX -> 中文

// HTML 实体
QString htmlEncode(const QString& text);
QString htmlDecode(const QString& text);

// JWT 解析
struct JwtInfo {
    bool ok = false;
    QString error;
    QString header;      // pretty JSON
    QString payload;     // pretty JSON
    QString signature;
    QString timeNotes;   // iat/nbf/exp 的可读时间说明
};
JwtInfo jwtParse(const QString& token);

} // namespace codecs
