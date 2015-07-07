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
#include "CryptoAlgorithmHmacParams.h"
#include "CryptoKeyDataOctetSequence.h"
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"

namespace WebCore {

const char* const CryptoAlgorithmHMAC::s_name = "HMAC";

CryptoAlgorithmHMAC::CryptoAlgorithmHMAC()
{
}

CryptoAlgorithmHMAC::~CryptoAlgorithmHMAC()
{
}

std::unique_ptr<CryptoAlgorithm> CryptoAlgorithmHMAC::create()
{
    return std::unique_ptr<CryptoAlgorithm>(new CryptoAlgorithmHMAC);
}

CryptoAlgorithmIdentifier CryptoAlgorithmHMAC::identifier() const
{
    return s_identifier;
}

bool CryptoAlgorithmHMAC::keyAlgorithmMatches(const CryptoAlgorithmHmacParams& parameters, const CryptoKey& key) const
{
    if (key.algorithmIdentifier() != s_identifier)
        return false;
    ASSERT(isCryptoKeyHMAC(key));

    if (toCryptoKeyHMAC(key).hashAlgorithmIdentifier() != parameters.hash)
        return false;

    return true;
}

void CryptoAlgorithmHMAC::sign(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmHmacParams& hmacParameters = toCryptoAlgorithmHmacParams(parameters);

    if (!keyAlgorithmMatches(hmacParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformSign(hmacParameters, toCryptoKeyHMAC(key), data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmHMAC::verify(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    const CryptoAlgorithmHmacParams& hmacParameters = toCryptoAlgorithmHmacParams(parameters);

    if (!keyAlgorithmMatches(hmacParameters, key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    platformVerify(hmacParameters, toCryptoKeyHMAC(key), expectedSignature, data, WTF::move(callback), WTF::move(failureCallback), ec);
}

void CryptoAlgorithmHMAC::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsage usages, KeyOrKeyPairCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    const CryptoAlgorithmHmacKeyParams& hmacParameters = toCryptoAlgorithmHmacKeyParams(parameters);

    RefPtr<CryptoKeyHMAC> result = CryptoKeyHMAC::generate(hmacParameters.hasLength ? hmacParameters.length : 0, hmacParameters.hash, extractable, usages);
    if (!result) {
        failureCallback();
        return;
    }

    callback(result.get(), nullptr);
}

void CryptoAlgorithmHMAC::importKey(const CryptoAlgorithmParameters& parameters, const CryptoKeyData& keyData, bool extractable, CryptoKeyUsage usage, KeyCallback callback, VoidCallback, ExceptionCode& ec)
{
    if (keyData.format() != CryptoKeyData::Format::OctetSequence) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyDataOctetSequence& keyDataOctetSequence = toCryptoKeyDataOctetSequence(keyData);

    const CryptoAlgorithmHmacParams& hmacParameters = toCryptoAlgorithmHmacParams(parameters);

    RefPtr<CryptoKeyHMAC> result = CryptoKeyHMAC::create(keyDataOctetSequence.octetSequence(), hmacParameters.hash, extractable, usage);
    callback(*result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
