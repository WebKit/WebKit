/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "CryptoKeyRSA.h"

#include "CryptoKeyDataRSAComponents.h"
#include "JsonWebKey.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <heap/HeapInlines.h>
#include <wtf/text/Base64.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

CryptoRsaKeyAlgorithm RsaKeyAlgorithm::dictionary() const
{
    CryptoRsaKeyAlgorithm result;
    result.name = this->name();
    result.modulusLength = this->modulusLength();
    result.publicExponent = Uint8Array::create(this->publicExponent().data(), this->publicExponent().size());
    return result;
}

CryptoRsaHashedKeyAlgorithm RsaHashedKeyAlgorithm::dictionary() const
{
    CryptoRsaHashedKeyAlgorithm result;
    result.name = this->name();
    result.modulusLength = this->modulusLength();
    result.publicExponent = Uint8Array::create(this->publicExponent().data(), this->publicExponent().size());
    result.hash.name = this->hash();
    return result;
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importJwk(CryptoAlgorithmIdentifier algorithm, std::optional<CryptoAlgorithmIdentifier> hash, JsonWebKey&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.kty != "RSA")
        return nullptr;
    if (keyData.key_ops && ((keyData.usages & usages) != usages))
        return nullptr;
    if (keyData.ext && !keyData.ext.value() && extractable)
        return nullptr;

    if (keyData.n.isNull() || keyData.e.isNull())
        return nullptr;
    Vector<uint8_t> modulus;
    if (!WTF::base64URLDecode(keyData.n, modulus))
        return nullptr;
    // Per RFC 7518 Section 6.3.1.1: https://tools.ietf.org/html/rfc7518#section-6.3.1.1
    if (!modulus.isEmpty() && !modulus[0])
        modulus.remove(0);
    Vector<uint8_t> exponent;
    if (!WTF::base64URLDecode(keyData.e, exponent))
        return nullptr;
    if (keyData.d.isNull()) {
        // import public key
        auto publicKeyComponents = CryptoKeyDataRSAComponents::createPublic(WTFMove(modulus), WTFMove(exponent));
        // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is std::nullopt.
        return CryptoKeyRSA::create(algorithm, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, *publicKeyComponents, extractable, usages);
    }

    // import private key
    Vector<uint8_t> privateExponent;
    if (!WTF::base64URLDecode(keyData.d, privateExponent))
        return nullptr;
    if (keyData.p.isNull() && keyData.q.isNull() && keyData.dp.isNull() && keyData.dp.isNull() && keyData.qi.isNull()) {
        auto privateKeyComponents = CryptoKeyDataRSAComponents::createPrivate(WTFMove(modulus), WTFMove(exponent), WTFMove(privateExponent));
        // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is std::nullopt.
        return CryptoKeyRSA::create(algorithm, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, *privateKeyComponents, extractable, usages);
    }

    if (keyData.p.isNull() || keyData.q.isNull() || keyData.dp.isNull() || keyData.dq.isNull() || keyData.qi.isNull())
        return nullptr;
    CryptoKeyDataRSAComponents::PrimeInfo firstPrimeInfo;
    CryptoKeyDataRSAComponents::PrimeInfo secondPrimeInfo;
    if (!WTF::base64URLDecode(keyData.p, firstPrimeInfo.primeFactor))
        return nullptr;
    if (!WTF::base64URLDecode(keyData.dp, firstPrimeInfo.factorCRTExponent))
        return nullptr;
    if (!WTF::base64URLDecode(keyData.q, secondPrimeInfo.primeFactor))
        return nullptr;
    if (!WTF::base64URLDecode(keyData.dq, secondPrimeInfo.factorCRTExponent))
        return nullptr;
    if (!WTF::base64URLDecode(keyData.qi, secondPrimeInfo.factorCRTCoefficient))
        return nullptr;
    if (!keyData.oth) {
        auto privateKeyComponents = CryptoKeyDataRSAComponents::createPrivateWithAdditionalData(WTFMove(modulus), WTFMove(exponent), WTFMove(privateExponent), WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), { });
        // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is std::nullopt.
        return CryptoKeyRSA::create(algorithm, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, *privateKeyComponents, extractable, usages);
    }

    Vector<CryptoKeyDataRSAComponents::PrimeInfo> otherPrimeInfos;
    for (auto value : keyData.oth.value()) {
        CryptoKeyDataRSAComponents::PrimeInfo info;
        if (!WTF::base64URLDecode(value.r, info.primeFactor))
            return nullptr;
        if (!WTF::base64URLDecode(value.d, info.factorCRTExponent))
            return nullptr;
        if (!WTF::base64URLDecode(value.t, info.factorCRTCoefficient))
            return nullptr;
        otherPrimeInfos.append(info);
    }

    auto privateKeyComponents = CryptoKeyDataRSAComponents::createPrivateWithAdditionalData(WTFMove(modulus), WTFMove(exponent), WTFMove(privateExponent), WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), WTFMove(otherPrimeInfos));
    // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is std::nullopt.
    return CryptoKeyRSA::create(algorithm, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, *privateKeyComponents, extractable, usages);
}

JsonWebKey CryptoKeyRSA::exportJwk() const
{
    JsonWebKey result;
    result.kty = "RSA";
    result.key_ops = usages();
    result.ext = extractable();

    auto keyData = exportData();
    const auto& rsaKeyData = downcast<CryptoKeyDataRSAComponents>(*keyData);
    // public key
    result.n = base64URLEncode(rsaKeyData.modulus());
    result.e = base64URLEncode(rsaKeyData.exponent());
    if (rsaKeyData.type() == CryptoKeyDataRSAComponents::Type::Public)
        return result;

    // private key
    result.d = base64URLEncode(rsaKeyData.privateExponent());
    if (!rsaKeyData.hasAdditionalPrivateKeyParameters())
        return result;

    result.p = base64URLEncode(rsaKeyData.firstPrimeInfo().primeFactor);
    result.q = base64URLEncode(rsaKeyData.secondPrimeInfo().primeFactor);
    result.dp = base64URLEncode(rsaKeyData.firstPrimeInfo().factorCRTExponent);
    result.dq = base64URLEncode(rsaKeyData.secondPrimeInfo().factorCRTExponent);
    result.qi = base64URLEncode(rsaKeyData.secondPrimeInfo().factorCRTCoefficient);
    if (rsaKeyData.otherPrimeInfos().isEmpty())
        return result;

    Vector<RsaOtherPrimesInfo> oth;
    for (auto info : rsaKeyData.otherPrimeInfos()) {
        RsaOtherPrimesInfo otherInfo;
        otherInfo.r = base64URLEncode(info.primeFactor);
        otherInfo.d = base64URLEncode(info.factorCRTExponent);
        otherInfo.t = base64URLEncode(info.factorCRTCoefficient);
        oth.append(WTFMove(otherInfo));
    }
    result.oth = WTFMove(oth);
    return result;
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
