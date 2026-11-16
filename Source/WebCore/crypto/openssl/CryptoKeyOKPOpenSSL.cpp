/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CryptoKeyOKP.h"

#if HAVE(OPENSSL_OKP)

#include "ExceptionOr.h"
#include "OpenSSLUtilities.h"
#include <openssl/x509.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static constexpr size_t keySizeInBytes = 32;

static std::optional<Vector<uint8_t>> rawKey(EVP_PKEY* pkey, int (*getter)(const EVP_PKEY*, unsigned char*, size_t*))
{
    Vector<uint8_t> key(keySizeInBytes);
    size_t length = key.size();
    if (getter(pkey, key.mutableSpan().data(), &length) <= 0 || length != key.size())
        return std::nullopt;

    return key;
}

bool CryptoKeyOKP::supportsNamedCurve()
{
    return true;
}

std::optional<CryptoKeyPair> CryptoKeyOKP::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto context = EvpPKeyCtxPtr(EVP_PKEY_CTX_new_id(okpPKeyType(namedCurve), nullptr));
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0)
        return std::nullopt;

    EVP_PKEY* pkeyPointer = nullptr;
    if (EVP_PKEY_keygen(context.get(), &pkeyPointer) <= 0 || !pkeyPointer)
        return std::nullopt;
    auto pkey = EvpPKeyPtr(pkeyPointer);

    auto privateKeyData = rawKey(pkey.get(), EVP_PKEY_get_raw_private_key);
    auto publicKeyData = rawKey(pkey.get(), EVP_PKEY_get_raw_public_key);
    if (!privateKeyData || !publicKeyData)
        return std::nullopt;

    bool isPublicKeyExtractable = true;
    auto publicKey = CryptoKeyOKP::create(identifier, namedCurve, CryptoKeyType::Public, WTF::move(*publicKeyData), isPublicKeyExtractable, usages);
    auto privateKey = CryptoKeyOKP::create(identifier, namedCurve, CryptoKeyType::Private, WTF::move(*privateKeyData), extractable, usages);
    if (!publicKey || !privateKey)
        return std::nullopt;

    return CryptoKeyPair { WTF::move(publicKey), WTF::move(privateKey) };
}

bool CryptoKeyOKP::platformCheckPairedKeys(CryptoAlgorithmIdentifier, NamedCurve namedCurve, const Vector<uint8_t>& privateKey, const Vector<uint8_t>& publicKey)
{
    auto pkey = createOKPPrivateKey(namedCurve, privateKey.span());
    if (!pkey)
        return false;

    auto derivedPublicKey = rawKey(pkey.get(), EVP_PKEY_get_raw_public_key);
    return derivedPublicKey && *derivedPublicKey == publicKey;
}

// Per https://www.rfc-editor.org/rfc/rfc8410 the AlgorithmIdentifier parameters MUST be absent;
// OpenSSL, BoringSSL and LibreSSL reject them when decoding both SPKI and PKCS#8.
RefPtr<CryptoKeyOKP> CryptoKeyOKP::importSpki(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    const uint8_t* ptr = keyData.span().data();
    auto pkey = EvpPKeyPtr(d2i_PUBKEY(nullptr, &ptr, keyData.size()));
    if (!pkey || ptr - keyData.span().data() != static_cast<ptrdiff_t>(keyData.size()))
        return nullptr;

    if (EVP_PKEY_id(pkey.get()) != okpPKeyType(namedCurve))
        return nullptr;

    auto publicKey = rawKey(pkey.get(), EVP_PKEY_get_raw_public_key);
    if (!publicKey)
        return nullptr;

    return create(identifier, namedCurve, CryptoKeyType::Public, WTF::move(*publicKey), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { ExceptionCode::InvalidAccessError };

    auto pkey = createOKPPublicKey(m_curve, m_data.span());
    if (!pkey)
        return Exception { ExceptionCode::OperationError };

    int length = i2d_PUBKEY(pkey.get(), nullptr);
    if (length <= 0)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> keyData(length);
    auto ptr = keyData.mutableSpan().data();
    if (i2d_PUBKEY(pkey.get(), &ptr) != length)
        return Exception { ExceptionCode::OperationError };

    return keyData;
}

// The spec parses the RFC 5208 PrivateKeyInfo, whose only version is 0, and its privateKey field as
// exactly one RFC 8410 CurvePrivateKey OCTET STRING. OpenSSL accepts any version and ignores trailing
// bytes in privateKey.
static bool isStrictPrivateKeyInfo(const Vector<uint8_t>& keyData)
{
    const uint8_t* ptr = keyData.span().data();
    auto privateKeyInfo = ASN1SequencePtr(d2i_ASN1_SEQUENCE_ANY(nullptr, &ptr, keyData.size()));
    if (!privateKeyInfo || sk_ASN1_TYPE_num(privateKeyInfo.get()) < 3)
        return false;

    auto* version = sk_ASN1_TYPE_value(privateKeyInfo.get(), 0);
    if (version->type != V_ASN1_INTEGER || ASN1_INTEGER_get(version->value.integer))
        return false;

    auto* privateKey = sk_ASN1_TYPE_value(privateKeyInfo.get(), 2);
    if (privateKey->type != V_ASN1_OCTET_STRING)
        return false;

    const uint8_t* curvePrivateKeyStart = ASN1_STRING_get0_data(privateKey->value.octet_string);
    int privateKeyLength = ASN1_STRING_length(privateKey->value.octet_string);
    auto curvePrivateKeyEnd = curvePrivateKeyStart;
    auto curvePrivateKey = ASN1OctetStringPtr(d2i_ASN1_OCTET_STRING(nullptr, &curvePrivateKeyEnd, privateKeyLength));
    return curvePrivateKey && curvePrivateKeyEnd - curvePrivateKeyStart == privateKeyLength;
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    const uint8_t* ptr = keyData.span().data();
    auto p8inf = PKCS8PrivKeyInfoPtr(d2i_PKCS8_PRIV_KEY_INFO(nullptr, &ptr, keyData.size()));
    if (!p8inf || ptr - keyData.span().data() != static_cast<ptrdiff_t>(keyData.size()))
        return nullptr;

    if (!isStrictPrivateKeyInfo(keyData))
        return nullptr;

    auto pkey = EvpPKeyPtr(EVP_PKCS82PKEY(p8inf.get()));
    if (!pkey || EVP_PKEY_id(pkey.get()) != okpPKeyType(namedCurve))
        return nullptr;

    auto privateKey = rawKey(pkey.get(), EVP_PKEY_get_raw_private_key);
    if (!privateKey)
        return nullptr;

    return create(identifier, namedCurve, CryptoKeyType::Private, WTF::move(*privateKey), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { ExceptionCode::InvalidAccessError };

    auto pkey = createOKPPrivateKey(m_curve, m_data.span());
    if (!pkey)
        return Exception { ExceptionCode::OperationError };

    auto p8inf = PKCS8PrivKeyInfoPtr(EVP_PKEY2PKCS8(pkey.get()));
    if (!p8inf)
        return Exception { ExceptionCode::OperationError };

    int length = i2d_PKCS8_PRIV_KEY_INFO(p8inf.get(), nullptr);
    if (length <= 0)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> keyData(length);
    auto ptr = keyData.mutableSpan().data();
    if (i2d_PKCS8_PRIV_KEY_INFO(p8inf.get(), &ptr) != length)
        return Exception { ExceptionCode::OperationError };

    return keyData;
}

String CryptoKeyOKP::generateJwkD() const
{
    ASSERT(type() == CryptoKeyType::Private);
    return base64URLEncodeToString(m_data);
}

String CryptoKeyOKP::generateJwkX() const
{
    if (type() == CryptoKeyType::Public)
        return base64URLEncodeToString(m_data);

    ASSERT(type() == CryptoKeyType::Private);

    auto pkey = createOKPPrivateKey(m_curve, m_data.span());
    if (!pkey)
        return { };

    auto publicKey = rawKey(pkey.get(), EVP_PKEY_get_raw_public_key);
    if (!publicKey)
        return { };

    return base64URLEncodeToString(*publicKey);
}

Vector<uint8_t> CryptoKeyOKP::platformExportRaw() const
{
    return m_data;
}

} // namespace WebCore

#endif // HAVE(OPENSSL_OKP)
