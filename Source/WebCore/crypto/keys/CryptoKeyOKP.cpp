/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "CryptoAlgorithmRegistry.h"
#include "JsonWebKey.h"
#include <wtf/text/Base64.h>

namespace WebCore {

static const ASCIILiteral X25519 { "X25519"_s };
static const ASCIILiteral Ed25519 { "Ed25519"_s };

static constexpr size_t keySizeInBytesFromNamedCurve(CryptoKeyOKP::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyOKP::NamedCurve::X25519:
    case CryptoKeyOKP::NamedCurve::Ed25519:
        return 32;
    }
    return 32;
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::create(CryptoAlgorithmIdentifier identifier, NamedCurve curve, CryptoKeyType type, KeyMaterial&& platformKey, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (platformKey.size() != keySizeInBytesFromNamedCurve(curve))
        return nullptr;
    return adoptRef(*new CryptoKeyOKP(identifier, curve, type, WTFMove(platformKey), extractable, usages));
}

CryptoKeyOKP::CryptoKeyOKP(CryptoAlgorithmIdentifier identifier, NamedCurve curve, CryptoKeyType type, KeyMaterial&& data, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_curve(curve)
    , m_data(WTFMove(data))
{
}

ExceptionOr<CryptoKeyPair> CryptoKeyOKP::generatePair(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!supportsNamedCurve())
        return Exception { ExceptionCode::NotSupportedError };

    auto result = platformGeneratePair(identifier, namedCurve, extractable, usages);
    if (!result)
        return Exception { ExceptionCode::OperationError };

    return WTFMove(*result);
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importRaw(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!supportsNamedCurve())
        return nullptr;

    // FIXME: The Ed25519 spec states that import in raw format must be used only for Verify.
    return create(identifier, namedCurve, usages & CryptoKeyUsageSign ? CryptoKeyType::Private : CryptoKeyType::Public, WTFMove(keyData), extractable, usages);
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importJwk(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, JsonWebKey&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!supportsNamedCurve())
        return nullptr;

    switch (namedCurve) {
    case NamedCurve::Ed25519:
        // FIXME: this is already done in the Algorithm's importKey method for each format, so it seems we can remoev this duplicated code.
        if (!keyData.d.isEmpty()) {
            if (usages & (CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey))
                return nullptr;
        } else {
            if (usages & (CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt | CryptoKeyUsageSign | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey))
                return nullptr;
        }
        if (keyData.crv != "Ed25519"_s)
            return nullptr;
        // FIXME: Do we have tests for these checks ?
        if (!keyData.alg.isEmpty() && keyData.alg != "EdDSA"_s)
            return nullptr;
        if (usages && !keyData.use.isEmpty() && keyData.use != "sign"_s)
            return nullptr;
        if (keyData.key_ops && ((keyData.usages & usages) != usages))
            return nullptr;
        if (keyData.ext && !keyData.ext.value() && extractable)
            return nullptr;
        break;
    case NamedCurve::X25519:
        if (keyData.crv != "X25519"_s)
            return nullptr;
        if (keyData.key_ops && ((keyData.usages & usages) != usages))
            return nullptr;
        if (keyData.ext && !keyData.ext.value() && extractable)
            return nullptr;
        break;
    }

    if (keyData.kty != "OKP"_s)
        return nullptr;

    if (keyData.x.isNull())
        return nullptr;

    auto x = base64URLDecode(keyData.x);
    if (!x)
        return nullptr;

    if (!keyData.d.isNull()) {
        auto d = base64URLDecode(keyData.d);
        if (!d || !platformCheckPairedKeys(identifier, namedCurve, *d, *x))
            return nullptr;
        return create(identifier, namedCurve, CryptoKeyType::Private, WTFMove(*d), extractable, usages);
    }

    return create(identifier, namedCurve, CryptoKeyType::Public, WTFMove(*x), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportRaw() const
{
    if (type() != CryptoKey::Type::Public)
        return Exception { ExceptionCode::InvalidAccessError };

    auto result = platformExportRaw();
    if (result.isEmpty())
        return Exception { ExceptionCode::OperationError };
    return result;
}

ExceptionOr<JsonWebKey> CryptoKeyOKP::exportJwk() const
{
    JsonWebKey result;
    result.kty = "OKP"_s;
    switch (m_curve) {
    case NamedCurve::X25519:
        result.crv = X25519;
        break;
    case NamedCurve::Ed25519:
        result.crv = Ed25519;
        break;
    }

    result.key_ops = usages();
    result.usages = usagesBitmap();
    result.ext = extractable();

    switch (type()) {
    case CryptoKeyType::Private:
        result.d = generateJwkD();
        result.x = generateJwkX();
        break;
    case CryptoKeyType::Public:
        result.x = generateJwkX();
        break;
    case CryptoKeyType::Secret:
        return Exception { ExceptionCode::OperationError };
    }

    return result;
}

std::optional<CryptoKeyOKP::NamedCurve> CryptoKeyOKP::namedCurveFromString(const String& curveString)
{
    if (curveString == X25519)
        return NamedCurve::X25519;

    if (curveString == Ed25519)
        return NamedCurve::Ed25519;

    return std::nullopt;
}

String CryptoKeyOKP::namedCurveString() const
{
    switch (m_curve) {
    case NamedCurve::X25519:
        return X25519;
    case NamedCurve::Ed25519:
        return Ed25519;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

bool CryptoKeyOKP::isValidOKPAlgorithm(CryptoAlgorithmIdentifier algorithm)
{
    return algorithm == CryptoAlgorithmIdentifier::Ed25519;
}

auto CryptoKeyOKP::algorithm() const -> KeyAlgorithm
{
    return CryptoKeyAlgorithm { CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier()) };
}

CryptoKey::Data CryptoKeyOKP::data() const
{
    auto key = platformKey();
    return CryptoKey::Data {
        CryptoKeyClass::OKP,
        algorithmIdentifier(),
        extractable(),
        usagesBitmap(),
        WTFMove(key),
        std::nullopt,
        std::nullopt,
        namedCurveString(),
        std::nullopt,
        type()
    };
}

#if !PLATFORM(COCOA) && !USE(GCRYPT)

bool CryptoKeyOKP::supportsNamedCurve()
{
    return false;
}

std::optional<CryptoKeyPair> CryptoKeyOKP::platformGeneratePair(CryptoAlgorithmIdentifier, NamedCurve, bool, CryptoKeyUsageBitmap)
{
    return { };
}

bool CryptoKeyOKP::platformCheckPairedKeys(CryptoAlgorithmIdentifier, NamedCurve, const Vector<uint8_t>&, const Vector<uint8_t>&)
{
    return true;
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    // FIXME: Implement it.
    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportSpki() const
{
    return Exception { ExceptionCode::NotSupportedError };
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    // FIXME: Implement it.
    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportPkcs8() const
{
    return Exception { ExceptionCode::NotSupportedError };
}

String CryptoKeyOKP::generateJwkD() const
{
    return { };
}

String CryptoKeyOKP::generateJwkX() const
{
    return { };
}

Vector<uint8_t> CryptoKeyOKP::platformExportRaw() const
{
    return { };
}
#endif

} // namespace WebCore
