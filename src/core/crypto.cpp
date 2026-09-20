#include "crypto.h"
#include <QCryptographicHash>
#include <QFile>
#include <QElapsedTimer>
#include <cstring>

#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

namespace crypto {

// ---------- 哈希 ----------
static QCryptographicHash::Algorithm algoByName(const QString& algo) {
    if (algo == QStringLiteral("MD5")) return QCryptographicHash::Md5;
    if (algo == QStringLiteral("SHA-1")) return QCryptographicHash::Sha1;
    if (algo == QStringLiteral("SHA-256")) return QCryptographicHash::Sha256;
    if (algo == QStringLiteral("SHA-512")) return QCryptographicHash::Sha512;
    if (algo == QStringLiteral("SHA3-256")) return QCryptographicHash::Sha3_256;
    return QCryptographicHash::Sha256;
}

QString hashText(const QString& text, const QString& algo) {
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), algoByName(algo)).toHex());
}

QString hashFile(const QString& path, const QString& algo) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(algoByName(algo));
    const qint64 chunk = 1 << 20;
    while (!f.atEnd()) {
        QByteArray part = f.read(chunk);
        if (part.isEmpty() && f.error() != QFileDevice::NoError) return QString();
        h.addData(part);
    }
    return QString::fromLatin1(h.result().toHex());
}

// ---------- AES ----------
static QByteArray decodeInput(const QString& text, const QString& fmt, bool* ok) {
    *ok = true;
    if (fmt == QStringLiteral("base64"))
        return QByteArray::fromBase64(text.trimmed().toLatin1());
    if (fmt == QStringLiteral("hex")) {
        QByteArray hex = QString(text).remove(QLatin1Char(' ')).trimmed().toLatin1();
        return QByteArray::fromHex(hex);
    }
    return text.toUtf8();
}

static QString encodeOutput(const QByteArray& data, const QString& fmt) {
    if (fmt == QStringLiteral("base64")) return QString::fromLatin1(data.toBase64());
    if (fmt == QStringLiteral("hex")) return QString::fromLatin1(data.toHex()).toUpper();
    return QString::fromUtf8(data);
}

static QByteArray deriveKey(const QString& keyText) {
    return QCryptographicHash::hash(keyText.toUtf8(), QCryptographicHash::Sha256); // 32 字节 AES-256
}

static QByteArray deriveIv(const QString& ivText) {
    QByteArray iv = ivText.toUtf8();
    if (iv.size() >= 16) return iv.left(16);
    // 不足 16 字节用 SHA256(ivText) 前缀补齐
    QByteArray h = QCryptographicHash::hash(iv, QCryptographicHash::Sha256);
    iv += h;
    return iv.left(16);
}

static QByteArray pkcs7Pad(const QByteArray& data, int blockSize) {
    int pad = blockSize - (data.size() % blockSize);
    return data + QByteArray(pad, static_cast<char>(pad));
}
static bool pkcs7Unpad(QByteArray& data, int blockSize) {
    if (data.isEmpty()) return false;
    int pad = static_cast<unsigned char>(data.back());
    if (pad <= 0 || pad > blockSize || pad > data.size()) return false;
    for (int i = data.size() - pad; i < data.size(); ++i)
        if (static_cast<unsigned char>(data.at(i)) != pad) return false;
    data.truncate(data.size() - pad);
    return true;
}

static QString mbedtlsErr(int ret) {
    char buf[256] = {};
    mbedtls_strerror(ret, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

AesResult aesEncrypt(const QString& plain, const QString& keyText, const QString& ivText,
                     const QString& mode, const QString& outFmt) {
    AesResult r;
    QByteArray key = deriveKey(keyText);
    QByteArray data = plain.toUtf8();

    if (mode == QStringLiteral("GCM")) {
        QByteArray nonce = ivText.toUtf8();
        if (nonce.size() < 12) nonce = deriveIv(ivText).left(12);
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                     reinterpret_cast<const unsigned char*>(key.constData()), 256);
        if (ret == 0) {
            QByteArray out(data.size(), 0);
            unsigned char tag[16] = {};
            ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, data.size(),
                                            reinterpret_cast<const unsigned char*>(nonce.constData()), nonce.size(),
                                            nullptr, 0,
                                            reinterpret_cast<const unsigned char*>(data.constData()),
                                            reinterpret_cast<unsigned char*>(out.data()), 16, tag);
            if (ret == 0) {
                out.append(reinterpret_cast<const char*>(tag), 16); // 密文 || tag
                r.text = encodeOutput(out, outFmt);
                r.ok = true;
            }
        }
        if (ret != 0) r.error = QStringLiteral("AES-GCM 加密失败: %1").arg(mbedtlsErr(ret));
        mbedtls_gcm_free(&ctx);
        return r;
    }

    if (mode == QStringLiteral("CBC")) {
        QByteArray iv = deriveIv(ivText);
        mbedtls_aes_context ctx;
        mbedtls_aes_init(&ctx);
        int ret = mbedtls_aes_setkey_enc(&ctx, reinterpret_cast<const unsigned char*>(key.constData()), 256);
        if (ret == 0) {
            QByteArray padded = pkcs7Pad(data, 16);
            QByteArray out(padded.size(), 0);
            ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded.size(), reinterpret_cast<unsigned char*>(iv.data()),
                                        reinterpret_cast<const unsigned char*>(padded.constData()),
                                        reinterpret_cast<unsigned char*>(out.data()));
            if (ret == 0) { r.text = encodeOutput(out, outFmt); r.ok = true; }
        }
        if (ret != 0) r.error = QStringLiteral("AES-CBC 加密失败: %1").arg(mbedtlsErr(ret));
        mbedtls_aes_free(&ctx);
        return r;
    }

    // ECB
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int ret = mbedtls_aes_setkey_enc(&ctx, reinterpret_cast<const unsigned char*>(key.constData()), 256);
    if (ret == 0) {
        QByteArray padded = pkcs7Pad(data, 16);
        QByteArray out(padded.size(), 0);
        for (int off = 0; off < padded.size() && ret == 0; off += 16)
            ret = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT,
                                        reinterpret_cast<const unsigned char*>(padded.constData() + off),
                                        reinterpret_cast<unsigned char*>(out.data() + off));
        if (ret == 0) { r.text = encodeOutput(out, outFmt); r.ok = true; }
    }
    if (ret != 0) r.error = QStringLiteral("AES-ECB 加密失败: %1").arg(mbedtlsErr(ret));
    mbedtls_aes_free(&ctx);
    return r;
}

AesResult aesDecrypt(const QString& cipherText, const QString& keyText, const QString& ivText,
                     const QString& mode, const QString& inFmt) {
    AesResult r;
    QByteArray key = deriveKey(keyText);
    bool ok = false;
    QByteArray data = decodeInput(cipherText, inFmt, &ok);
    if (data.isEmpty()) { r.error = QStringLiteral("密文为空或格式无效"); return r; }

    if (mode == QStringLiteral("GCM")) {
        if (data.size() < 17) { r.error = QStringLiteral("GCM 密文过短（应至少含 1 字节密文 + 16 字节 tag）"); return r; }
        QByteArray nonce = ivText.toUtf8();
        if (nonce.size() < 12) nonce = deriveIv(ivText).left(12);
        QByteArray body = data.left(data.size() - 16);
        QByteArray tag = data.right(16);
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                     reinterpret_cast<const unsigned char*>(key.constData()), 256);
        QByteArray out(body.size(), 0);
        if (ret == 0)
            ret = mbedtls_gcm_auth_decrypt(&ctx, body.size(),
                                           reinterpret_cast<const unsigned char*>(nonce.constData()), nonce.size(),
                                           nullptr, 0,
                                           reinterpret_cast<const unsigned char*>(tag.constData()), 16,
                                           reinterpret_cast<const unsigned char*>(body.constData()),
                                           reinterpret_cast<unsigned char*>(out.data()));
        if (ret == 0) { r.text = QString::fromUtf8(out); r.ok = true; }
        else r.error = QStringLiteral("解密失败（密钥/IV 不匹配或数据损坏）");
        mbedtls_gcm_free(&ctx);
        return r;
    }

    if (mode != QStringLiteral("ECB") && mode != QStringLiteral("CBC")) { r.error = QStringLiteral("未知模式"); return r; }
    if (data.size() % 16 != 0) { r.error = QStringLiteral("密文长度必须是 16 字节的整数倍"); return r; }

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int ret = mbedtls_aes_setkey_dec(&ctx, reinterpret_cast<const unsigned char*>(key.constData()), 256);
    QByteArray out(data.size(), 0);
    if (ret == 0) {
        if (mode == QStringLiteral("CBC")) {
            QByteArray iv = deriveIv(ivText);
            ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, data.size(), reinterpret_cast<unsigned char*>(iv.data()),
                                        reinterpret_cast<const unsigned char*>(data.constData()),
                                        reinterpret_cast<unsigned char*>(out.data()));
        } else {
            for (int off = 0; off < data.size() && ret == 0; off += 16)
                ret = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT,
                                            reinterpret_cast<const unsigned char*>(data.constData() + off),
                                            reinterpret_cast<unsigned char*>(out.data() + off));
        }
    }
    if (ret == 0) {
        if (!pkcs7Unpad(out, 16)) { r.error = QStringLiteral("填充校验失败（密钥错误或数据损坏）"); }
        else { r.text = QString::fromUtf8(out); r.ok = true; }
    } else {
        r.error = QStringLiteral("AES 解密失败: %1").arg(mbedtlsErr(ret));
    }
    mbedtls_aes_free(&ctx);
    return r;
}

// ---------- RSA ----------
RsaResult rsaGenerate(int bits) {
    RsaResult r;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char* pers = "lingwang_toolbox_rsa";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    reinterpret_cast<const unsigned char*>(pers), strlen(pers));
    if (ret == 0)
        ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret == 0)
        ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random, &ctr_drbg, static_cast<unsigned int>(bits), 65537);
    if (ret != 0) {
        r.error = QStringLiteral("RSA 密钥生成失败: %1").arg(mbedtlsErr(ret));
    } else {
        unsigned char buf[16000] = {};
        size_t len = 0;
        if (mbedtls_pk_write_key_pem(&pk, buf, sizeof(buf)) == 0) {
            len = strlen(reinterpret_cast<char*>(buf));
            r.privateKey = QString::fromLatin1(reinterpret_cast<char*>(buf), static_cast<int>(len));
        }
        memset(buf, 0, sizeof(buf));
        if (mbedtls_pk_write_pubkey_pem(&pk, buf, sizeof(buf)) == 0) {
            len = strlen(reinterpret_cast<char*>(buf));
            r.publicKey = QString::fromLatin1(reinterpret_cast<char*>(buf), static_cast<int>(len));
        }
        r.ok = !r.privateKey.isEmpty();
    }
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return r;
}

} // namespace crypto
