/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoKeyEC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyData.h"
#include "JsonWebKey.h"
#include <wtf/text/Base64.h>

namespace WebCore {

CryptoEcKeyAlgorithm EcKeyAlgorithm::dictionary() const
{
    CryptoEcKeyAlgorithm result;
    result.name = this->name();
    result.namedCurve = this->namedCurve();
    return result;
}

static const char* const P256 = "P-256";
static const char* const P384 = "P-384";

static std::optional<CryptoKeyEC::NamedCurve> toNamedCurve(const String& curve)
{
    if (curve == P256)
        return CryptoKeyEC::NamedCurve::P256;
    if (curve == P384)
        return CryptoKeyEC::NamedCurve::P384;

    return std::nullopt;
}

CryptoKeyEC::CryptoKeyEC(CryptoAlgorithmIdentifier identifier, NamedCurve curve, CryptoKeyType type, PlatformECKey platformKey, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_platformKey(platformKey)
    , m_curve(curve)
{
}

ExceptionOr<CryptoKeyPair> CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier identifier, const String& curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto namedCurve = toNamedCurve(curve);
    if (!namedCurve)
        return Exception { NotSupportedError };

    auto result = platformGeneratePair(identifier, *namedCurve, extractable, usages);
    if (!result)
        return Exception { OperationError };

    return WTFMove(*result);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier identifier, const String& curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto namedCurve = toNamedCurve(curve);
    if (!namedCurve)
        return nullptr;

    return platformImportRaw(identifier, *namedCurve, WTFMove(keyData), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::importJwk(CryptoAlgorithmIdentifier identifier, const String& curve, JsonWebKey&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.kty != "EC")
        return nullptr;
    if (keyData.key_ops && ((keyData.usages & usages) != usages))
        return nullptr;
    if (keyData.ext && !keyData.ext.value() && extractable)
        return nullptr;

    if (keyData.crv.isNull() || curve != keyData.crv)
        return nullptr;
    auto namedCurve = toNamedCurve(keyData.crv);
    if (!namedCurve)
        return nullptr;

    if (keyData.x.isNull() || keyData.y.isNull())
        return nullptr;
    Vector<uint8_t> x;
    if (!WTF::base64URLDecode(keyData.x, x))
        return nullptr;
    Vector<uint8_t> y;
    if (!WTF::base64URLDecode(keyData.y, y))
        return nullptr;
    if (keyData.d.isNull()) {
        // import public key
        return platformImportJWKPublic(identifier, *namedCurve, WTFMove(x), WTFMove(y), extractable, usages);
    }

    Vector<uint8_t> d;
    if (!WTF::base64URLDecode(keyData.d, d))
        return nullptr;
    // import private key
    return platformImportJWKPrivate(identifier, *namedCurve, WTFMove(x), WTFMove(y), WTFMove(d), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::importSpki(CryptoAlgorithmIdentifier identifier, const String& curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto namedCurve = toNamedCurve(curve);
    if (!namedCurve)
        return nullptr;

    return platformImportSpki(identifier, *namedCurve, WTFMove(keyData), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::importPkcs8(CryptoAlgorithmIdentifier identifier, const String& curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto namedCurve = toNamedCurve(curve);
    if (!namedCurve)
        return nullptr;

    return platformImportPkcs8(identifier, *namedCurve, WTFMove(keyData), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyEC::exportRaw() const
{
    if (type() != CryptoKey::Type::Public)
        return Exception { InvalidAccessError };

    return platformExportRaw();
}

JsonWebKey CryptoKeyEC::exportJwk() const
{
    JsonWebKey result;
    result.kty = "EC";
    switch (m_curve) {
    case NamedCurve::P256:
        result.crv = P256;
        break;
    case NamedCurve::P384:
        result.crv = P384;
        break;
    }
    result.key_ops = usages();
    result.ext = extractable();
    platformAddFieldElements(result);
    return result;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyEC::exportSpki() const
{
    if (type() != CryptoKey::Type::Public)
        return Exception { InvalidAccessError };

    return platformExportSpki();
}

ExceptionOr<Vector<uint8_t>> CryptoKeyEC::exportPkcs8() const
{
    if (type() != CryptoKey::Type::Private)
        return Exception { InvalidAccessError };

    return platformExportPkcs8();
}

String CryptoKeyEC::namedCurveString() const
{
    switch (m_curve) {
    case NamedCurve::P256:
        return String(P256);
    case NamedCurve::P384:
        return String(P384);
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

bool CryptoKeyEC::isValidECAlgorithm(CryptoAlgorithmIdentifier algorithm)
{
    return algorithm == CryptoAlgorithmIdentifier::ECDSA || algorithm == CryptoAlgorithmIdentifier::ECDH;
}

std::unique_ptr<KeyAlgorithm> CryptoKeyEC::buildAlgorithm() const
{
    String name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    switch (m_curve) {
    case NamedCurve::P256:
        return std::make_unique<EcKeyAlgorithm>(name, String(P256));
    case NamedCurve::P384:
        return std::make_unique<EcKeyAlgorithm>(name, String(P384));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

std::unique_ptr<CryptoKeyData> CryptoKeyEC::exportData() const
{
    // A dummy implementation for now.
    return std::make_unique<CryptoKeyData>(CryptoKeyData::Format::OctetSequence);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
