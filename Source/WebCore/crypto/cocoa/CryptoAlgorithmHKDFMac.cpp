/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmHKDF.h"

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoKeyRaw.h"
#include "CryptoUtilitiesCocoa.h"
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwiftUtils.h>
#endif

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> platformDeriveBitsCC(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    CCDigestAlgorithm digestAlgorithm;
    getCommonCryptoDigestAlgorithm(parameters.hashIdentifier, digestAlgorithm);

    return deriveHDKFBits(digestAlgorithm, key.key().span(), parameters.saltVector().span(), parameters.infoVector().span(), length);
}

#if HAVE(SWIFT_CPP_INTEROP)
static ExceptionOr<Vector<uint8_t>> platformDeriveBitsCryptoKit(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    if (!isValidHashParameter(parameters.hashIdentifier))
        return Exception { ExceptionCode::OperationError };
    auto rv = PAL::HKDF::deriveBits(key.key().span(), parameters.saltVector().span(), parameters.infoVector().span(), length, toCKHashFunction(parameters.hashIdentifier));
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
}
#endif

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length, UseCryptoKit useCryptoKit)
{
#if HAVE(SWIFT_CPP_INTEROP)
    if (useCryptoKit == UseCryptoKit::Yes)
        return platformDeriveBitsCryptoKit(parameters, key, length);
#else
UNUSED_PARAM(useCryptoKit);
#endif
    return platformDeriveBitsCC(parameters, key, length);
}
} // namespace WebCore
