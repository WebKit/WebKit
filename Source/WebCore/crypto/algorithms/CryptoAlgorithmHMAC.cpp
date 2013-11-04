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
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"
#include "JSDOMPromise.h"

namespace WebCore {

const char* const CryptoAlgorithmHMAC::s_name = "hmac";

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

void CryptoAlgorithmHMAC::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsage usages, std::unique_ptr<PromiseWrapper> promise, ExceptionCode&)
{
    const CryptoAlgorithmHmacKeyParams& hmacParameters = static_cast<const CryptoAlgorithmHmacKeyParams&>(parameters);

    RefPtr<CryptoKeyHMAC> result = CryptoKeyHMAC::generate(hmacParameters.hasLength ? hmacParameters.length : 0, hmacParameters.hash, extractable, usages);
    if (!result) {
        promise->reject(nullptr);
        return;
    }

    promise->fulfill(result.release());
}

void CryptoAlgorithmHMAC::importKey(const CryptoAlgorithmParameters& parameters, CryptoKeyFormat format, const CryptoOperationData& data, bool extractable, CryptoKeyUsage usage, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    if (format != CryptoKeyFormat::Raw) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoAlgorithmHmacParams& hmacParameters = static_cast<const CryptoAlgorithmHmacParams&>(parameters);
    Vector<char> keyData;
    keyData.append(data.first, data.second);
    RefPtr<CryptoKeyHMAC> result = CryptoKeyHMAC::create(keyData, hmacParameters.hash, extractable, usage);
    promise->fulfill(result.release());
}

void CryptoAlgorithmHMAC::exportKey(const CryptoAlgorithmParameters&, CryptoKeyFormat, const CryptoKey&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    // Not implemented yet.
    ec = NOT_SUPPORTED_ERR;
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
