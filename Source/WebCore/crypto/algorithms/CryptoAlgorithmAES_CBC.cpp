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
#include "CryptoAlgorithmAesKeyGenParams.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyDataOctetSequence.h"
#include "ExceptionCode.h"

namespace WebCore {

const char* const CryptoAlgorithmAES_CBC::s_name = "AES-CBC";

CryptoAlgorithmAES_CBC::CryptoAlgorithmAES_CBC()
{
}

CryptoAlgorithmAES_CBC::~CryptoAlgorithmAES_CBC()
{
}

std::unique_ptr<CryptoAlgorithm> CryptoAlgorithmAES_CBC::create()
{
    return std::unique_ptr<CryptoAlgorithm>(new CryptoAlgorithmAES_CBC);
}

CryptoAlgorithmIdentifier CryptoAlgorithmAES_CBC::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmAES_CBC::keyAlgorithmMatches(const CryptoAlgorithmAesCbcParams&, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(isCryptoKeyAES(key));

    return true;
}

void CryptoAlgorithmAES_CBC::encrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmAesCbcParams& aesCBCParameters = toCryptoAlgorithmAesCbcParams(parameters);

    if (!keyAlgorithmMatches(aesCBCParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformEncrypt(aesCBCParameters, toCryptoKeyAES(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmAES_CBC::decrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmAesCbcParams& aesCBCParameters = toCryptoAlgorithmAesCbcParams(parameters);

    if (!keyAlgorithmMatches(aesCBCParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformDecrypt(aesCBCParameters, toCryptoKeyAES(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmAES_CBC::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmAesKeyGenParams& aesParameters = toCryptoAlgorithmAesKeyGenParams(parameters);

    RefPtr<CryptoKeyAES> result = CryptoKeyAES::generate(CryptoAlgorithmIdentifier::AES_CBC, aesParameters.length, extractable, usages);
    if (!result) {
        failureCallback();
        return;
    }

    callback(result.get(), nullptr);
}

void CryptoAlgorithmAES_CBC::importKey(const CryptoAlgorithmParameters&, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback callback, VoidCallback, ExceptionCode& ec)
{
    if (keyData.format() != CryptoKeyData::Format::OctetSequence) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyDataOctetSequence& keyDataOctetSequence = toCryptoKeyDataOctetSequence(keyData);
    RefPtr<CryptoKeyAES> result = CryptoKeyAES::create(CryptoAlgorithmIdentifier::AES_CBC, keyDataOctetSequence.octetSequence(), extractable, usage);
    callback(*result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
