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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmHmacKeyParams.h"
#include "CryptoAlgorithmHmacKeyParamsDeprecated.h"
#include "CryptoAlgorithmHmacParamsDeprecated.h"
#include "CryptoKeyDataOctetSequence.h"
#include "CryptoKeyHMAC.h"
#include <wtf/Variant.h>

namespace WebCore {

static const char* const ALG1 = "HS1";
static const char* const ALG224 = "HS224";
static const char* const ALG256 = "HS256";
static const char* const ALG384 = "HS384";
static const char* const ALG512 = "HS512";

static inline bool usagesAreInvalidForCryptoAlgorithmHMAC(CryptoKeyUsageBitmap usages)
{
    return usages & (CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey);
}

Ref<CryptoAlgorithm> CryptoAlgorithmHMAC::create()
{
    return adoptRef(*new CryptoAlgorithmHMAC);
}

CryptoAlgorithmIdentifier CryptoAlgorithmHMAC::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmHMAC::keyAlgorithmMatches(const CryptoAlgorithmHmacParamsDeprecated& parameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;

    if (downcast<CryptoKeyHMAC>(key).hashAlgorithmIdentifier() != parameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmHMAC::sign(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    platformSign(WTFMove(key), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmHMAC::verify(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&& key, Vector<uint8_t>&& signature, Vector<uint8_t>&& data, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    platformVerify(WTFMove(key), WTFMove(signature), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), context, workQueue);
}

void CryptoAlgorithmHMAC::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&)
{
    const auto& hmacParameters = downcast<CryptoAlgorithmHmacKeyParams>(parameters);

    if (usagesAreInvalidForCryptoAlgorithmHMAC(usages)) {
        exceptionCallback(SyntaxError);
        return;
    }

    if (hmacParameters.length && !hmacParameters.length.value()) {
        exceptionCallback(OperationError);
        return;
    }

    auto result = CryptoKeyHMAC::generate(hmacParameters.length.value_or(0), hmacParameters.hashIdentifier, extractable, usages);
    if (!result) {
        exceptionCallback(OperationError);
        return;
    }

    callback(WTFMove(result));
}

void CryptoAlgorithmHMAC::importKey(SubtleCrypto::KeyFormat format, KeyData&& data, const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    ASSERT(parameters);
    const auto& hmacParameters = downcast<CryptoAlgorithmHmacKeyParams>(*parameters);

    if (usagesAreInvalidForCryptoAlgorithmHMAC(usages)) {
        exceptionCallback(SyntaxError);
        return;
    }

    RefPtr<CryptoKeyHMAC> result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Raw:
        result = CryptoKeyHMAC::importRaw(hmacParameters.length.value_or(0), hmacParameters.hashIdentifier, WTFMove(WTF::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    case SubtleCrypto::KeyFormat::Jwk: {
        auto checkAlgCallback = [](CryptoAlgorithmIdentifier hash, const String& alg) -> bool {
            switch (hash) {
            case CryptoAlgorithmIdentifier::SHA_1:
                return alg.isNull() || alg == ALG1;
            case CryptoAlgorithmIdentifier::SHA_224:
                return alg.isNull() || alg == ALG224;
            case CryptoAlgorithmIdentifier::SHA_256:
                return alg.isNull() || alg == ALG256;
            case CryptoAlgorithmIdentifier::SHA_384:
                return alg.isNull() || alg == ALG384;
            case CryptoAlgorithmIdentifier::SHA_512:
                return alg.isNull() || alg == ALG512;
            default:
                return false;
            }
            return false;
        };
        result = CryptoKeyHMAC::importJwk(hmacParameters.length.value_or(0), hmacParameters.hashIdentifier, WTFMove(WTF::get<JsonWebKey>(data)), extractable, usages, WTFMove(checkAlgCallback));
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

void CryptoAlgorithmHMAC::exportKey(SubtleCrypto::KeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    const auto& hmacKey = downcast<CryptoKeyHMAC>(key.get());

    if (hmacKey.key().isEmpty()) {
        exceptionCallback(OperationError);
        return;
    }

    KeyData result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Raw:
        result = Vector<uint8_t>(hmacKey.key());
        break;
    case SubtleCrypto::KeyFormat::Jwk: {
        JsonWebKey jwk = hmacKey.exportJwk();
        switch (hmacKey.hashAlgorithmIdentifier()) {
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
    default:
        exceptionCallback(NotSupportedError);
        return;
    }

    callback(format, WTFMove(result));
}

ExceptionOr<size_t> CryptoAlgorithmHMAC::getKeyLength(const CryptoAlgorithmParameters& parameters)
{
    return CryptoKeyHMAC::getKeyLength(parameters);
}

ExceptionOr<void> CryptoAlgorithmHMAC::sign(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto& hmacParameters = downcast<CryptoAlgorithmHmacParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(hmacParameters, key))
        return Exception { NotSupportedError };
    return platformSign(hmacParameters, downcast<CryptoKeyHMAC>(key), data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmHMAC::verify(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback)
{
    auto& hmacParameters = downcast<CryptoAlgorithmHmacParamsDeprecated>(parameters);
    if (!keyAlgorithmMatches(hmacParameters, key))
        return Exception { NotSupportedError };
    return platformVerify(hmacParameters, downcast<CryptoKeyHMAC>(key), expectedSignature, data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithmHMAC::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext&)
{
    auto& hmacParameters = downcast<CryptoAlgorithmHmacKeyParamsDeprecated>(parameters);
    auto result = CryptoKeyHMAC::generate(hmacParameters.hasLength ? hmacParameters.length : 0, hmacParameters.hash, extractable, usages);
    if (!result) {
        failureCallback();
        return { };
    }
    callback(WTFMove(result));
    return { };
}

ExceptionOr<void> CryptoAlgorithmHMAC::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsageBitmap usage, KeyCallback&& callback, VoidCallback&&)
{
    if (!is<CryptoKeyDataOctetSequence>(keyData))
        return Exception { NotSupportedError };
    callback(CryptoKeyHMAC::create(downcast<CryptoKeyDataOctetSequence>(keyData).octetSequence(), downcast<CryptoAlgorithmHmacParamsDeprecated>(parameters).hash, extractable, usage));
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
