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

#include "CryptoAlgorithmRsaKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyParamsWithHash.h"
#include "CryptoAlgorithmRsaSsaParams.h"
#include "CryptoKeyDataRSAComponents.h"
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

std::unique_ptr<CryptoAlgorithm> CryptoAlgorithmRSASSA_PKCS1_v1_5::create()
{
    return std::unique_ptr<CryptoAlgorithm>(new CryptoAlgorithmRSASSA_PKCS1_v1_5);
}

CryptoAlgorithmIdentifier CryptoAlgorithmRSASSA_PKCS1_v1_5::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmRSASSA_PKCS1_v1_5::keyAlgorithmMatches(const CryptoAlgorithmRsaSsaParams& algorithmParameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(isCryptoKeyRSA(key));

    CryptoAlgorithmIdentifier keyHash;
    if (toCryptoKeyRSA(key).isRestrictedToHash(keyHash) && keyHash != algorithmParameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::sign(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParams& rsaSSAParameters = toCryptoAlgorithmRsaSsaParams(parameters);

    if (!keyAlgorithmMatches(rsaSSAParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformSign(rsaSSAParameters, toCryptoKeyRSA(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::verify(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParams& rsaSSAParameters = toCryptoAlgorithmRsaSsaParams(parameters);

    if (!keyAlgorithmMatches(rsaSSAParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformVerify(rsaSSAParameters, toCryptoKeyRSA(key), signature, data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyGenParams& rsaParameters = toCryptoAlgorithmRsaKeyGenParams(parameters);

    auto keyPairCallback = [callback](CryptoKeyPair& pair) {
        callback(nullptr, &pair);
    };

    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTF::move(keyPairCallback), WTF::move(failureCallback));
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::importKey(const CryptoAlgorithmParameters& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyParamsWithHash& rsaKeyParameters = toCryptoAlgorithmRsaKeyParamsWithHash(parameters);
    const CryptoKeyDataRSAComponents& rsaComponents = toCryptoKeyDataRSAComponents(keyData);

    RefPtr<CryptoKeyRSA> result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5, rsaComponents, extractable, usage);
    if (!result) {
        failureCallback();
        return;
    }

    if (rsaKeyParameters.hasHash)
        result->restrictToHash(rsaKeyParameters.hash);

    callback(*result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
