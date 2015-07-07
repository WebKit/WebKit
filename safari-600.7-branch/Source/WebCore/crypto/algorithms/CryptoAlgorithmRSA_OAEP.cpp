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

#include "CryptoAlgorithmRsaKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyParamsWithHash.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoKeyDataRSAComponents.h"
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

std::unique_ptr<CryptoAlgorithm> CryptoAlgorithmRSA_OAEP::create()
{
    return std::unique_ptr<CryptoAlgorithm>(new CryptoAlgorithmRSA_OAEP);
}

CryptoAlgorithmIdentifier CryptoAlgorithmRSA_OAEP::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmRSA_OAEP::keyAlgorithmMatches(const CryptoAlgorithmRsaOaepParams& algorithmParameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(isCryptoKeyRSA(key));

    CryptoAlgorithmIdentifier keyHash;
    if (toCryptoKeyRSA(key).isRestrictedToHash(keyHash) && keyHash != algorithmParameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmRSA_OAEP::encrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaOaepParams& rsaOAEPParameters = toCryptoAlgorithmRsaOaepParams(parameters);

    if (!keyAlgorithmMatches(rsaOAEPParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformEncrypt(rsaOAEPParameters, toCryptoKeyRSA(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmRSA_OAEP::decrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaOaepParams& rsaOAEPParameters = toCryptoAlgorithmRsaOaepParams(parameters);

    if (!keyAlgorithmMatches(rsaOAEPParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformDecrypt(rsaOAEPParameters, toCryptoKeyRSA(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmRSA_OAEP::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyGenParams& rsaParameters = toCryptoAlgorithmRsaKeyGenParams(parameters);

    auto keyPairCallback = [callback](CryptoKeyPair& pair) {
        callback(nullptr, &pair);
    };

    CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier::RSA_OAEP, rsaParameters.modulusLength, rsaParameters.publicExponent, extractable, usages, WTF::move(keyPairCallback), WTF::move(failureCallback));
}

void CryptoAlgorithmRSA_OAEP::importKey(const CryptoAlgorithmParameters& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmRsaKeyParamsWithHash& rsaKeyParameters = toCryptoAlgorithmRsaKeyParamsWithHash(parameters);
    const CryptoKeyDataRSAComponents& rsaComponents = toCryptoKeyDataRSAComponents(keyData);

    RefPtr<CryptoKeyRSA> result = CryptoKeyRSA::create(CryptoAlgorithmIdentifier::RSA_OAEP, rsaComponents, extractable, usage);
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
