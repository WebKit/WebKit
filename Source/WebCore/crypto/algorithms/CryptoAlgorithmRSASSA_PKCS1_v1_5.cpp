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

#include "CryptoAlgorithmRsaHashedKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoAlgorithmRsaSsaParamsDeprecated.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"

namespace WebCore {

const char* const CryptoAlgorithmRSASSA_PKCS1_v1_5::s_name = "RSASSA-PKCS1-v1_5";

CryptoAlgorithmRSASSA_PKCS1_v1_5::CryptoAlgorithmRSASSA_PKCS1_v1_5()
{
}

CryptoAlgorithmRSASSA_PKCS1_v1_5::~CryptoAlgorithmRSASSA_PKCS1_v1_5()
{
}

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

void CryptoAlgorithmRSASSA_PKCS1_v1_5::generateKey(const std::unique_ptr<CryptoAlgorithmParameters>&& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext* context)
{
    const auto& rsaParameters = downcast<CryptoAlgorithmRsaHashedKeyGenParams>(*parameters);

    if (usages & (CryptoKeyUsageDecrypt | CryptoKeyUsageEncrypt | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey)) {
        exceptionCallback(SYNTAX_ERR);
        return;
    }

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair& pair) {
        pair.publicKey()->setUsagesBitmap(pair.publicKey()->usagesBitmap() & CryptoKeyUsageVerify);
        pair.privateKey()->setUsagesBitmap(pair.privateKey()->usagesBitmap() & CryptoKeyUsageSign);
        capturedCallback(nullptr, &pair);
    };
    auto failureCallback = [capturedCallback = WTFMove(exceptionCallback)]() {
        capturedCallback(OperationError);
    };
    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaParameters.hashIdentifier, true, rsaParameters.modulusLength, rsaParameters.publicExponentVector(), extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), context);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::sign(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParamsDeprecated& rsaSSAParameters = downcast<CryptoAlgorithmRsaSsaParamsDeprecated>(parameters);

    if (!keyAlgorithmMatches(rsaSSAParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformSign(rsaSSAParameters, downcast<CryptoKeyRSA>(key), data, WTFMove(callback), WTFMove(failureCallback), ec);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::verify(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParamsDeprecated& rsaSSAParameters = downcast<CryptoAlgorithmRsaSsaParamsDeprecated>(parameters);

    if (!keyAlgorithmMatches(rsaSSAParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformVerify(rsaSSAParameters,  downcast<CryptoKeyRSA>(key), signature, data, WTFMove(callback), WTFMove(failureCallback), ec);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::generateKey(const CryptoAlgorithmParametersDeprecated& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback&& callback, VoidCallback&& failureCallback, ExceptionCode&, ScriptExecutionContext* context)
{
    const CryptoAlgorithmRsaKeyGenParamsDeprecated& rsaParameters = downcast<CryptoAlgorithmRsaKeyGenParamsDeprecated>(parameters);

    auto keyPairCallback = [capturedCallback = WTFMove(callback)](CryptoKeyPair& pair) {
        capturedCallback(nullptr, &pair);
    };

    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaParameters.hash, rsaParameters.hasHash, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTFMove(keyPairCallback), WTFMove(failureCallback), context);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::importKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback&& callback, VoidCallback&& failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyParamsWithHashDeprecated& rsaKeyParameters = downcast<CryptoAlgorithmRsaKeyParamsWithHashDeprecated>(parameters);
    const CryptoKeyDataRSAComponents& rsaComponents = downcast<CryptoKeyDataRSAComponents>(keyData);

    RefPtr<CryptoKeyRSA> result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaKeyParameters.hash, rsaKeyParameters.hasHash, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return;
    }

    callback(*result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
