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

#include "CommonCryptoUtilities.h"
#include "JsonWebKey.h"
#include <wtf/text/Base64.h>

namespace WebCore {

static unsigned char InitialOctet = 0x04; // Per Section 2.3.3 of http://www.secg.org/sec1-v2.pdf

// Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
// We only support uncompressed point format.
static bool doesUncompressedPointMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return size == 65;
    case CryptoKeyEC::NamedCurve::P384:
        return size == 97;
    }

    ASSERT_NOT_REACHED();
    return false;
}

// Per Section 2.3.5 of http://www.secg.org/sec1-v2.pdf
static bool doesFieldElementMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return size == 32;
    case CryptoKeyEC::NamedCurve::P384:
        return size == 48;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static size_t getKeySizeFromNamedCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

CryptoKeyEC::~CryptoKeyEC()
{
    CCECCryptorRelease(m_platformKey);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    int result = CCECGetKeySize(m_platformKey);
    return result ? result : 0;
}

Vector<uint8_t> CryptoKeyEC::exportRaw() const
{
    Vector<uint8_t> result(keySizeInBits() / 4 + 1); // Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    size_t size = result.size();
    CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPublic, m_platformKey);
    return result;
}

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    size_t size = getKeySizeFromNamedCurve(curve);
    CCECCryptorRef ccPublicKey;
    CCECCryptorRef ccPrivateKey;
    if (CCECCryptorGeneratePair(size, &ccPublicKey, &ccPrivateKey))
        return std::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, ccPublicKey, true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, ccPrivateKey, extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesUncompressedPointMatchNamedCurve(curve, keyData.size()))
        return nullptr;

    CCECCryptorRef ccPublicKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyData.data(), keyData.size(), ccECKeyPublic, &ccPublicKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, ccPublicKey, extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesFieldElementMatchNamedCurve(curve, x.size()) || !doesFieldElementMatchNamedCurve(curve, y.size()))
        return nullptr;

    size_t size = getKeySizeFromNamedCurve(curve);
    CCECCryptorRef ccPublicKey;
    if (CCECCryptorCreateFromData(size, x.data(), x.size(), y.data(), y.size(), &ccPublicKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, ccPublicKey, extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesFieldElementMatchNamedCurve(curve, x.size()) || !doesFieldElementMatchNamedCurve(curve, y.size()) || !doesFieldElementMatchNamedCurve(curve, d.size()))
        return nullptr;

    // A hack to CommonCrypto since it doesn't provide API for creating private keys directly from x, y, d.
    // BinaryInput = InitialOctet + X + Y + D
    Vector<uint8_t> binaryInput;
    binaryInput.append(InitialOctet);
    binaryInput.appendVector(x);
    binaryInput.appendVector(y);
    binaryInput.appendVector(d);

    CCECCryptorRef ccPrivateKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, binaryInput.data(), binaryInput.size(), ccECKeyPrivate, &ccPrivateKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Private, ccPrivateKey, extractable, usages);
}

void CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    size_t size = getKeySizeFromNamedCurve(m_curve);
    size_t sizeInBytes = size / 8;
    Vector<uint8_t> x(sizeInBytes);
    size_t xSize = x.size();
    Vector<uint8_t> y(sizeInBytes);
    size_t ySize = y.size();
    Vector<uint8_t> d(sizeInBytes);
    size_t dSize = d.size();

    CCECCryptorGetKeyComponents(m_platformKey, &size, x.data(), &xSize, y.data(), &ySize, d.data(), &dSize);
    jwk.x = base64URLEncode(x);
    jwk.y = base64URLEncode(y);
    if (type() == Type::Private)
        jwk.d = base64URLEncode(d);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
