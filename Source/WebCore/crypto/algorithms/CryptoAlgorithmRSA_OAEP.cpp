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

#include "CryptoAlgorithmRsaHashedKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoAlgorithmRsaOaepParamsDeprecated.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"

namespace WebCore {

const char* const CryptoAlgorithmRSA_OAEP::s_name = "RSA-OAEP";

CryptoAlgorithmRSA_OAEP::CryptoAlgorithmRSA_OAEP()
{
}

CryptoAlgorithmRSA_OAEP::~CryptoAlgorithmRSA_OAEP()
{
}

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

void CryptoAlgorithmRSA_OAEP::generateKey(const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext* context)
{
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedKeyGenParams>(*parameters);

    if (usages & (CryptoKeyUsageSign | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits)) {
        exceptionCallback(SYNTAX_ERR);
        return;
    }

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair& pair) {
        pair.publicKey()->setUsagesBitmap(pair.publicKey()->usagesBitmap() & (CryptoKeyUsageEncrypt | CryptoKeyUsageWrapKey));
        pair.privateKey()->setUsagesBitmap(pair.privateKey()->usagesBitmap() & (CryptoKeyUsageDecrypt | CryptoKeyUsageUnwrapKey));
        capturedCallback(nullptr, &pair);
    };
    auto failureCallback = [capturedCallback = WTFMove(exceptionCallback)]() {
        capturedCallback(OperationError);
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSA_OAEP, rsaParameters.hashIdentifier, true, rsaParameters.modulusLength, rsaParameters.publicExponentVector(), extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), context);
}

void CryptoAlgorithmRSA_OAEP::encrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaOaepParamsDeprecated& rsaOAEPParameters = downcast<CryptoAlgorithmRsaOaepParamsDeprecated>(parameters);

    if (!keyAlgorithmMatches(rsaOAEPParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformEncrypt(rsaOAEPParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback), ec);
}

void CryptoAlgorithmRSA_OAEP::decrypt(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaOaepParamsDeprecated& rsaOAEPParameters = downcast<CryptoAlgorithmRsaOaepParamsDeprecated>(parameters);

    if (!keyAlgorithmMatches(rsaOAEPParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformDecrypt(rsaOAEPParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback), ec);
}

void CryptoAlgorithmRSA_OAEP::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ExceptionCode&, ScriptExecutionContext* context)
{
    const CryptoAlgorithmRsaKeyGenParamsDeprecated& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParamsDeprecated>(parameters);

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair& pair) {
        capturedCallback(nullptr, &pair);
    };

    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSA_OAEP, rsaParameters.hash, rsaParameters.hasHash, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), context);
}

void CryptoAlgorithmRSA_OAEP::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback&& callback, VoidCallback&& failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyParamsWithHashDeprecated& rsaKeyParameters = downcast<CryptoAlgorithmRsaKeyParamsWithHashDeprecated>(parameters);
    const CryptoKeyDataRSAComponents& rsaComponents = downcast<CryptoKeyDataRSAComponents>(keyData);

    RefPtr<CryptoKeyRSA> result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSA_OAEP, rsaKeyParameters.hash, rsaKeyParameters.hasHash, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return;
    }

    callback(*result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
