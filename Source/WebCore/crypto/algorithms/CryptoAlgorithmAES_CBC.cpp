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
#include "CryptoAlgorithmAES_CBC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmAesCbcParams.h"
#include "CryptoAlgorithmAesCbcParamsDeprecated.h"
#include "CryptoAlgorithmAesKeyGenParams.h"
#include "CryptoAlgorithmAesKeyGenParamsDeprecated.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyDataOctetSequence.h"
#include "ExceptionCode.h"

namespace WebCore {

static const char* const ALG128 = "A128CBC";
static const char* const ALG192 = "A192CBC";
static const char* const ALG256 = "A256CBC";
static const size_t IVSIZE = 16;

static inline bool usagesAreInvalidForCryptoAlgorithmAES_CBC(CryptoKeyUsageBitmap usages)
{
    return usages & (CryptoKeyUsageSign | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits);
}

Ref<CryptoAlgorithm> CryptoAlgorithmAES_CBC::create()
{
    return adoptRef(*new CryptoAlgorithmAES_CBC);
}

CryptoAlgorithmIdentifier CryptoAlgorithmAES_CBC::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmAES_CBC::keyAlgorithmMatches(const CryptoAlgorithmAesCbcParamsDeprecated&, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(is<CryptoKeyAES>(key));
    return true;
}

void CryptoAlgorithmAES_CBC::encrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    ASSERT(parameters);
    auto& aesParameters = downcast<CryptoAlgorithmAesCbcParams>(*parameters);
    if (aesParameters.ivVector().size() != IVSIZE) {
        exceptionCallback(OperationError);
        return;
    }
    platformEncrypt(WTFMove(parameters), WTFMove(key), WTFMove(plainText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmAES_CBC::decrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    ASSERT(parameters);
    auto& aesParameters = downcast<CryptoAlgorithmAesCbcParams>(*parameters);
    if (aesParameters.ivVector().size() != IVSIZE) {
        exceptionCallback(OperationError);
        return;
    }
    platformDecrypt(WTFMove(parameters), WTFMove(key), WTFMove(cipherText), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmAES_CBC::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&)
{
    const auto& aesParameters = downcast<CryptoAlgorithmAesKeyGenParams>(parameters);

    if (usagesAreInvalidForCryptoAlgorithmAES_CBC(usages)) {
        exceptionCallback(SYNTAX_ERR);
        return;
    }

    auto result = CryptoKeyAES::generate(CryptoAlgorithmIdentifier::AES_CBC, aesParameters.length, extractable, usages);
    if (!result) {
        exceptionCallback(OperationError);
        return;
    }

    callback(WTFMove(result));
}

void CryptoAlgorithmAES_CBC::importKey(SubtleCrypto::KeyFormat format, KeyData&& data, const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    ASSERT(parameters);
    if (usagesAreInvalidForCryptoAlgorithmAES_CBC(usages)) {
        exceptionCallback(SYNTAX_ERR);
        return;
    }

    RefPtr<CryptoKeyAES> result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Raw:
        result = CryptoKeyAES::importRaw(parameters->identifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    case SubtleCrypto::KeyFormat::Jwk: {
        auto checkAlgCallback = [](size_t length, const String& alg) -> bool {
            switch (length) {
            case CryptoKeyAES::s_length128:
                return alg.isNull() || alg == ALG128;
            case CryptoKeyAES::s_length192:
                return alg.isNull() || alg == ALG192;
            case CryptoKeyAES::s_length256:
                return alg.isNull() || alg == ALG256;
            }
            return false;
        };
        result = CryptoKeyAES::importJwk(parameters->identifier, WTFMove(WTF::get<JsonWebKey>(data)), extractable, usages, WTFMove(checkAlgCallback));
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

void CryptoAlgorithmAES_CBC::exportKey(SubtleCrypto::KeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    const auto& aesKey = downcast<CryptoKeyAES>(key.get());

    if (aesKey.key().isEmpty()) {
        exceptionCallback(OperationError);
        return;
    }

    KeyData result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Raw:
        result = Vector<uint8_t>(aesKey.key());
        break;
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey jwk = aesKey.exportJwk();
        switch (aesKey.key().size() * 8) {
        case CryptoKeyAES::s_length128:
            jwk.alg = String(ALG128);
            break;
        case CryptoKeyAES::s_length192:
            jwk.alg = String(ALG192);
            break;
        case CryptoKeyAES::s_length256:
            jwk.alg = String(ALG256);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        result = WTFMove(jwk);
        break;
    }
    default:
        exceptionCallback(NOT_SUPPORTED_ERR);
        return;
    }

    callback(format, WTFMove(result));
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::encrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& aesCBCParameters = downcast<CryptoAlgorithmAesCbcParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(aesCBCParameters, key))
        return Exception { NOT_SUPPORTED_ERR };
    return platformEncrypt(aesCBCParameters, downcast<CryptoKeyAES>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::decrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& aesCBCParameters = downcast<CryptoAlgorithmAesCbcParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(aesCBCParameters, key))
        return Exception { NOT_SUPPORTED_ERR };
    return platformDecrypt(aesCBCParameters, downcast<CryptoKeyAES>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext&)
{
    auto& aesParameters = downcast<CryptoAlgorithmAesKeyGenParamsDeprecated>(parameters);

    auto result = CryptoKeyAES::generate(CryptoAlgorithmIdentifier::AES_CBC, aesParameters.length, extractable, usages);
    if (!result) {
        failureCallback();
        return { };
    }

    callback(WTFMove(result));
    return { };
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::importKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsageBitmap usage, KeyCallback&& callback, VoidCallback&&)
{
    if (!is<CryptoKeyDataOctetSequence>(keyData))
        return Exception { NOT_SUPPORTED_ERR };
    auto& keyDataOctetSequence = downcast<CryptoKeyDataOctetSequence>(keyData);
    callback(CryptoKeyAES::create(CryptoAlgorithmIdentifier::AES_CBC, keyDataOctetSequence.octetSequence(), extractable, usage));
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
