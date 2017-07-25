/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRsaHashedImportParams.h"
#include "CryptoAlgorithmRsaHashedKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoAlgorithmRsaSsaParamsDeprecated.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include <wtf/Variant.h>

namespace WebCore {

static const char* const ALG1 = "RS1";
static const char* const ALG224 = "RS224";
static const char* const ALG256 = "RS256";
static const char* const ALG384 = "RS384";
static const char* const ALG512 = "RS512";

Ref<CryptoAlgorithm> CryptoAlgorithmRSASSA_PKCS1_v1_5::create()
{
    return adoptRef(*new CryptoAlgorithmRSASSA_PKCS1_v1_5);
}

CryptoAlgorithmIdentifier CryptoAlgorithmRSASSA_PKCS1_v1_5::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmRSASSA_PKCS1_v1_5::keyAlgorithmMatches(const CryptoAlgorithmRsaSsaParamsDeprecated& algorithmParameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;

    CryptoAlgorithmIdentifier keyHash;
    if (downcast<CryptoKeyRSA>(key).isRestrictedToHash(keyHash) && keyHash != algorithmParameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::sign(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    if (key->type() != CryptoKeyType::Private) {
        exceptionCallback(InvalidAccessError);
        return;
    }
    platformSign(WTFMove(key), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::verify(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& signature, Vector<uint8_t>&& data, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    if (key->type() != CryptoKeyType::Public) {
        exceptionCallback(InvalidAccessError);
        return;
    }
    platformVerify(WTFMove(key), WTFMove(signature), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context)
{
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedKeyGenParams>(parameters);

    if (usages & (CryptoKeyUsageDecrypt | CryptoKeyUsageEncrypt | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey)) {
        exceptionCallback(SyntaxError);
        return;
    }

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        pair.publicKey->setUsagesBitmap(pair.publicKey->usagesBitmap() & CryptoKeyUsageVerify);
        pair.privateKey->setUsagesBitmap(pair.privateKey->usagesBitmap() & CryptoKeyUsageSign);
        capturedCallback(WTFMove(pair));
    };
    auto failureCallback = [capturedCallback = WTFMove(exceptionCallback)]() {
        capturedCallback(OperationError);
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaParameters.hashIdentifier, true, rsaParameters.modulusLength, rsaParameters.publicExponentVector(), extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::importKey(SubtleCrypto::KeyFormat format, KeyData&& data, const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    ASSERT(parameters);
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedImportParams>(*parameters);

    RefPtr<CryptoKeyRSA> result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey key = WTFMove(WTF::get<JsonWebKey>(data));

        if (usages && ((!key.d.isNull() && (usages ^ CryptoKeyUsageSign)) || (key.d.isNull() && (usages ^ CryptoKeyUsageVerify)))) {
            exceptionCallback(SyntaxError);
            return;
        }
        if (usages && !key.use.isNull() && key.use != "sig") {
            exceptionCallback(DataError);
            return;
        }

        bool isMatched = false;
        switch (rsaParameters.hashIdentifier) {
        case CryptoAlgorithmIdentifier::SHA_1:
            isMatched = key.alg.isNull() || key.alg == ALG1;
            break;
        case CryptoAlgorithmIdentifier::SHA_224:
            isMatched = key.alg.isNull() || key.alg == ALG224;
            break;
        case CryptoAlgorithmIdentifier::SHA_256:
            isMatched = key.alg.isNull() || key.alg == ALG256;
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
            isMatched = key.alg.isNull() || key.alg == ALG384;
            break;
        case CryptoAlgorithmIdentifier::SHA_512:
            isMatched = key.alg.isNull() || key.alg == ALG512;
            break;
        default:
            break;
        }
        if (!isMatched) {
            exceptionCallback(DataError);
            return;
        }

        result = CryptoKeyRSA::importJwk(rsaParameters.identifier, rsaParameters.hashIdentifier, WTFMove(key), extractable, usages);
        break;
    }
    case SubtleCrypto::KeyFormat::Spki: {
        if (usages && (usages ^ CryptoKeyUsageVerify)) {
            exceptionCallback(SyntaxError);
            return;
        }
        // FIXME: <webkit.org/b/165436>
        result = CryptoKeyRSA::importSpki(rsaParameters.identifier, rsaParameters.hashIdentifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    }
    case SubtleCrypto::KeyFormat::Pkcs8: {
        if (usages && (usages ^ CryptoKeyUsageSign)) {
            exceptionCallback(SyntaxError);
            return;
        }
        // FIXME: <webkit.org/b/165436>
        result = CryptoKeyRSA::importPkcs8(parameters->identifier, rsaParameters.hashIdentifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    }
    default:
        exceptionCallback(NotSupportedError);
        return;
    }
    if (!result) {
        exceptionCallback(DataError);
        return;
    }

    callback(*result);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::exportKey(SubtleCrypto::KeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    const auto& rsaKey = downcast<CryptoKeyRSA>(key.get());

    if (!rsaKey.keySizeInBits()) {
        exceptionCallback(OperationError);
        return;
    }

    KeyData result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey jwk = rsaKey.exportJwk();
        switch (rsaKey.hashAlgorithmIdentifier()) {
        case CryptoAlgorithmIdentifier::SHA_1:
            jwk.alg = String(ALG1);
            break;
        case CryptoAlgorithmIdentifier::SHA_224:
            jwk.alg = String(ALG224);
            break;
        case CryptoAlgorithmIdentifier::SHA_256:
            jwk.alg = String(ALG256);
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
            jwk.alg = String(ALG384);
            break;
        case CryptoAlgorithmIdentifier::SHA_512:
            jwk.alg = String(ALG512);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        result = WTFMove(jwk);
        break;
    }
    case SubtleCrypto::KeyFormat::Spki: {
        auto spki = rsaKey.exportSpki();
        if (spki.hasException()) {
            exceptionCallback(spki.releaseException().code());
            return;
        }
        result = spki.releaseReturnValue();
        break;
    }
    case SubtleCrypto::KeyFormat::Pkcs8: {
        auto pkcs8 = rsaKey.exportPkcs8();
        if (pkcs8.hasException()) {
            exceptionCallback(pkcs8.releaseException().code());
            return;
        }
        result = pkcs8.releaseReturnValue();
        break;
    }
    default:
        exceptionCallback(NotSupportedError);
        return;
    }

    callback(format, WTFMove(result));
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::sign(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaSSAParameters = downcast<CryptoAlgorithmRsaSsaParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(rsaSSAParameters, key))
        return Exception { NotSupportedError };
    return platformSign(rsaSSAParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::verify(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaSSAParameters = downcast<CryptoAlgorithmRsaSsaParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(rsaSSAParameters, key))
        return Exception { NotSupportedError };
    return platformVerify(rsaSSAParameters,  downcast<CryptoKeyRSA>(key), signature, data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext& context)
{
    auto& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParamsDeprecated>(parameters);
    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        capturedCallback(WTFMove(pair));
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaParameters.hash, rsaParameters.hasHash, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsageBitmap usage, KeyCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaKeyParameters = downcast<CryptoAlgorithmRsaKeyParamsWithHashDeprecated>(parameters);
    auto& rsaComponents = downcast<CryptoKeyDataRSAComponents>(keyData);
    auto result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaKeyParameters.hash, rsaKeyParameters.hasHash, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return { };
    }
    callback(*result);
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
