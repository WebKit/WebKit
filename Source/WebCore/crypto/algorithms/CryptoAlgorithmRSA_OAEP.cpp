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
#include "CryptoAlgorithmRSA_OAEP.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRsaHashedImportParams.h"
#include "CryptoAlgorithmRsaHashedKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoAlgorithmRsaOaepParamsDeprecated.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"
#include <wtf/Variant.h>

namespace WebCore {

static const char* const ALG1 = "RSA-OAEP";
static const char* const ALG224 = "RSA-OAEP-224";
static const char* const ALG256 = "RSA-OAEP-256";
static const char* const ALG384 = "RSA-OAEP-384";
static const char* const ALG512 = "RSA-OAEP-512";

Ref<CryptoAlgorithm> CryptoAlgorithmRSA_OAEP::create()
{
    return adoptRef(*new CryptoAlgorithmRSA_OAEP);
}

CryptoAlgorithmIdentifier CryptoAlgorithmRSA_OAEP::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmRSA_OAEP::keyAlgorithmMatches(const CryptoAlgorithmRsaOaepParamsDeprecated& algorithmParameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;

    CryptoAlgorithmIdentifier keyHash;
    if (downcast<CryptoKeyRSA>(key).isRestrictedToHash(keyHash) && keyHash != algorithmParameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmRSA_OAEP::encrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    ASSERT(parameters);
    if (key->type() != CryptoKeyType::Public) {
        exceptionCallback(INVALID_ACCESS_ERR);
        return;
    }
    platformEncrypt(WTFMove(parameters), WTFMove(key), WTFMove(plainText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSA_OAEP::decrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    ASSERT(parameters);
    if (key->type() != CryptoKeyType::Private) {
        exceptionCallback(INVALID_ACCESS_ERR);
        return;
    }
    platformDecrypt(WTFMove(parameters), WTFMove(key), WTFMove(cipherText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSA_OAEP::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context)
{
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedKeyGenParams>(parameters);

    if (usages & (CryptoKeyUsageSign | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits)) {
        exceptionCallback(SYNTAX_ERR);
        return;
    }

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        pair.publicKey->setUsagesBitmap(pair.publicKey->usagesBitmap() & (CryptoKeyUsageEncrypt | CryptoKeyUsageWrapKey));
        pair.privateKey->setUsagesBitmap(pair.privateKey->usagesBitmap() & (CryptoKeyUsageDecrypt | CryptoKeyUsageUnwrapKey));
        capturedCallback(WTFMove(pair));
    };
    auto failureCallback = [capturedCallback = WTFMove(exceptionCallback)]() {
        capturedCallback(OperationError);
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSA_OAEP, rsaParameters.hashIdentifier, true, rsaParameters.modulusLength, rsaParameters.publicExponentVector(), extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
}

void CryptoAlgorithmRSA_OAEP::importKey(SubtleCrypto::KeyFormat format, KeyData&& data, const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    ASSERT(parameters);
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedImportParams>(*parameters);

    RefPtr<CryptoKeyRSA> result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey key = WTFMove(WTF::get<JsonWebKey>(data));

        bool isUsagesAllowed = false;
        if (!key.d.isNull()) {
            isUsagesAllowed = isUsagesAllowed || !(usages ^ CryptoKeyUsageDecrypt);
            isUsagesAllowed = isUsagesAllowed || !(usages ^ CryptoKeyUsageUnwrapKey);
            isUsagesAllowed = isUsagesAllowed || !(usages ^ (CryptoKeyUsageDecrypt | CryptoKeyUsageUnwrapKey));
        } else {
            isUsagesAllowed = isUsagesAllowed || !(usages ^ CryptoKeyUsageEncrypt);
            isUsagesAllowed = isUsagesAllowed || !(usages ^ CryptoKeyUsageWrapKey);
            isUsagesAllowed = isUsagesAllowed || !(usages ^ (CryptoKeyUsageEncrypt | CryptoKeyUsageWrapKey));
        }
        isUsagesAllowed = isUsagesAllowed || !usages;
        if (!isUsagesAllowed) {
            exceptionCallback(SYNTAX_ERR);
            return;
        }

        if (usages && !key.use.isNull() && key.use != "enc") {
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
        if (usages && (usages ^ CryptoKeyUsageEncrypt) && (usages ^ CryptoKeyUsageWrapKey) && (usages ^ (CryptoKeyUsageEncrypt | CryptoKeyUsageWrapKey))) {
            exceptionCallback(SYNTAX_ERR);
            return;
        }
        // FIXME: <webkit.org/b/165436>
        result = CryptoKeyRSA::importSpki(rsaParameters.identifier, rsaParameters.hashIdentifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    }
    case SubtleCrypto::KeyFormat::Pkcs8: {
        if (usages && (usages ^ CryptoKeyUsageDecrypt) && (usages ^ CryptoKeyUsageUnwrapKey) && (usages ^ (CryptoKeyUsageDecrypt | CryptoKeyUsageUnwrapKey))) {
            exceptionCallback(SYNTAX_ERR);
            return;
        }
        // FIXME: <webkit.org/b/165436>
        result = CryptoKeyRSA::importPkcs8(parameters->identifier, rsaParameters.hashIdentifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    }
    default:
        exceptionCallback(NOT_SUPPORTED_ERR);
        return;
    }
    if (!result) {
        exceptionCallback(DataError);
        return;
    }

    callback(*result);
}

void CryptoAlgorithmRSA_OAEP::exportKey(SubtleCrypto::KeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
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
        // FIXME: <webkit.org/b/165437>
        auto spki = rsaKey.exportSpki();
        if (spki.hasException()) {
            exceptionCallback(spki.releaseException().code());
            return;
        }
        result = spki.releaseReturnValue();
        break;
    }
    case SubtleCrypto::KeyFormat::Pkcs8: {
        // FIXME: <webkit.org/b/165437>
        auto pkcs8 = rsaKey.exportPkcs8();
        if (pkcs8.hasException()) {
            exceptionCallback(pkcs8.releaseException().code());
            return;
        }
        result = pkcs8.releaseReturnValue();
        break;
    }
    default:
        exceptionCallback(NOT_SUPPORTED_ERR);
        return;
    }

    callback(format, WTFMove(result));
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::encrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaOAEPParameters = downcast<CryptoAlgorithmRsaOaepParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(rsaOAEPParameters, key))
        return Exception { NOT_SUPPORTED_ERR };
    return platformEncrypt(rsaOAEPParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::decrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaOAEPParameters = downcast<CryptoAlgorithmRsaOaepParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(rsaOAEPParameters, key))
        return Exception { NOT_SUPPORTED_ERR };
    return platformDecrypt(rsaOAEPParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext& context)
{
    auto& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParamsDeprecated>(parameters);
    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        capturedCallback(WTFMove(pair));
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSA_OAEP, rsaParameters.hash, rsaParameters.hasHash, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsageBitmap usage, KeyCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaKeyParameters = downcast<CryptoAlgorithmRsaKeyParamsWithHashDeprecated>(parameters);
    auto& rsaComponents = downcast<CryptoKeyDataRSAComponents>(keyData);
    auto result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSA_OAEP, rsaKeyParameters.hash, rsaKeyParameters.hasHash, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return { };
    }
    callback(*result);
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
