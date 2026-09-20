#pragma once
#include <QString>
#include <QByteArray>

namespace crypto {

// ---------- 哈希 ----------
// algo: "MD5" | "SHA-1" | "SHA-256" | "SHA-512" | "SHA3-256"
QString hashText(const QString& text, const QString& algo);
// 大文件流式哈希，返回空串表示失败
QString hashFile(const QString& path, const QString& algo);

// ---------- AES ----------
struct AesResult { bool ok = false; QString error, text; };
// keyText 经 SHA-256 派生为 256 位密钥；ivText 不足则派生补齐
// mode: "GCM" | "CBC" | "ECB"; inFmt/outFmt: "text"|"base64"|"hex"
AesResult aesEncrypt(const QString& plain, const QString& keyText, const QString& ivText,
                     const QString& mode, const QString& outFmt);
AesResult aesDecrypt(const QString& cipherText, const QString& keyText, const QString& ivText,
                     const QString& mode, const QString& inFmt);

// ---------- RSA ----------
struct RsaResult { bool ok = false; QString error, privateKey, publicKey; };
RsaResult rsaGenerate(int bits);

} // namespace crypto
