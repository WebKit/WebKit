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
#include "CryptoAlgorithmRSAES_PKCS1_v1_5.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRsaKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include <wtf/Variant.h>

namespace WebCore {

static const char* const ALG = "RSA1_5";

Ref<CryptoAlgorithm> CryptoAlgorithmRSAES_PKCS1_v1_5::create()
{
    return adoptRef(*new CryptoAlgorithmRSAES_PKCS1_v1_5);
}

CryptoAlgorithmIdentifier CryptoAlgorithmRSAES_PKCS1_v1_5::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmRSAES_PKCS1_v1_5::keyAlgorithmMatches(const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(is<CryptoKeyRSA>(key));

    return true;
}

void CryptoAlgorithmRSAES_PKCS1_v1_5::encrypt(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    if (key->type() != CryptoKeyType::Public) {
        exceptionCallback(InvalidAccessError);
        return;
    }
    platformEncrypt(WTFMove(key), WTFMove(plainText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSAES_PKCS1_v1_5::decrypt(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    if (key->type() != CryptoKeyType::Private) {
        exceptionCallback(InvalidAccessError);
        return;
    }
    platformDecrypt(WTFMove(key), WTFMove(cipherText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmRSAES_PKCS1_v1_5::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context)
{
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParams>(parameters);

    if (usages & (CryptoKeyUsageSign | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey)) {
        exceptionCallback(SyntaxError);
        return;
    }

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        pair.publicKey->setUsagesBitmap(pair.publicKey->usagesBitmap() & CryptoKeyUsageEncrypt);
        pair.privateKey->setUsagesBitmap(pair.privateKey->usagesBitmap() & CryptoKeyUsageDecrypt);
        capturedCallback(WTFMove(pair));
    };
    auto failureCallback = [capturedCallback = WTFMove(exceptionCallback)]() {
        capturedCallback(OperationError);
    };
    // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect.
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5, CryptoAlgorithmIdentifier::SHA_1, false, rsaParameters.modulusLength, rsaParameters.publicExponentVector(), extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
}

void CryptoAlgorithmRSAES_PKCS1_v1_5::importKey(SubtleCrypto::KeyFormat format, KeyData&& data, const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    ASSERT(parameters);
    RefPtr<CryptoKeyRSA> result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey key = WTFMove(WTF::get<JsonWebKey>(data));
        if (usages && ((!key.d.isNull() && (usages ^ CryptoKeyUsageDecrypt)) || (key.d.isNull() && (usages ^ CryptoKeyUsageEncrypt)))) {
            exceptionCallback(SyntaxError);
            return;
        }
        if (usages && !key.use.isNull() && key.use != "enc") {
            exceptionCallback(DataError);
            return;
        }
        if (!key.alg.isNull() && key.alg != ALG) {
            exceptionCallback(DataError);
            return;
        }
        result = CryptoKeyRSA::importJwk(parameters->identifier, std::nullopt, WTFMove(key), extractable, usages);
        break;
    }
    case SubtleCrypto::KeyFormat::Spki: {
        if (usages && (usages ^ CryptoKeyUsageEncrypt)) {
            exceptionCallback(SyntaxError);
            return;
        }
        result = CryptoKeyRSA::importSpki(parameters->identifier, std::nullopt, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    }
    case SubtleCrypto::KeyFormat::Pkcs8: {
        if (usages && (usages ^ CryptoKeyUsageDecrypt)) {
            exceptionCallback(SyntaxError);
            return;
        }
        result = CryptoKeyRSA::importPkcs8(parameters->identifier, std::nullopt, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
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

void CryptoAlgorithmRSAES_PKCS1_v1_5::exportKey(SubtleCrypto::KeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
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
        jwk.alg = String(ALG);
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

ExceptionOr<void> CryptoAlgorithmRSAES_PKCS1_v1_5::encrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    if (!keyAlgorithmMatches(key))
        return Exception { NotSupportedError };
    return platformEncrypt(downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSAES_PKCS1_v1_5::decrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    if (!keyAlgorithmMatches(key))
        return Exception { NotSupportedError };
    return platformDecrypt(downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmRSAES_PKCS1_v1_5::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext& context)
{
    auto& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParamsDeprecated>(parameters);
    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair&& pair) {
        capturedCallback(WTFMove(pair));
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5, rsaParameters.hash, rsaParameters.hasHash, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), &context);
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSAES_PKCS1_v1_5::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsageBitmap usage, KeyCallback&& callback, VoidCallback&& failureCallback)
{
    auto& rsaParameters = downcast<CryptoAlgorithmRsaKeyParamsWithHashDeprecated>(parameters);
    auto& rsaComponents = downcast<CryptoKeyDataRSAComponents>(keyData);
    auto result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5, rsaParameters.hash, rsaParameters.hasHash, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return { };
    }
    callback(*result);
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
