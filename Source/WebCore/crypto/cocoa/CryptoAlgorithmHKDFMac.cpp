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
#include <pal/PALSwift.h>
#endif

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> platformDeriveBitsCryptoKit(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (!isValidHashParameter(parameters.hashIdentifier))
        return Exception { ExceptionCode::OperationError };
    auto rv = PAL::HKDF::deriveBits(key.key().span(), parameters.saltVector().span(), parameters.infoVector().span(), length, std::to_underlying(toCKHashFunction(parameters.hashIdentifier)));
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(key);
    UNUSED_PARAM(length);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    return platformDeriveBitsCryptoKit(parameters, key, length);
}
} // namespace WebCore
