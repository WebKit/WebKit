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

#ifndef CryptoAlgorithmRsaKeyParamsWithHash_h
#define CryptoAlgorithmRsaKeyParamsWithHash_h

#include "CryptoAlgorithmIdentifier.h"
#include "CryptoAlgorithmParameters.h"

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

// This parameters class is currently not specified in WebCrypto.
// It is necessary to support import from JWK, which treats hash function as part of algorithm
// identifier, so we need to remember it to compare with one passed to sign or verify functions.
class CryptoAlgorithmRsaKeyParamsWithHash final : public CryptoAlgorithmParameters {
public:
    CryptoAlgorithmRsaKeyParamsWithHash()
        : hasHash(false)
    {
    }

    // The hash algorithm to use.
    bool hasHash;
    CryptoAlgorithmIdentifier hash;

    virtual Class parametersClass() const override { return Class::RsaKeyParamsWithHash; }
};

CRYPTO_ALGORITHM_PARAMETERS_CASTS(RsaKeyParamsWithHash)

}

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoAlgorithmRsaKeyParamsWithHash_h
