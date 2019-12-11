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

#if ENABLE(WEB_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoKeyRaw.h"

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    Vector<uint8_t> result(length / 8);
    CCDigestAlgorithm digestAlgorithm;
    getCommonCryptoDigestAlgorithm(parameters.hashIdentifier, digestAlgorithm);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // <rdar://problem/32439455> Currently, when key data is empty, CCKeyDerivationHMac will bail out.
    // <rdar://problem/48896021> Reminder: Switch to CCDeriveKey now that CCKeyDerivationHMac is deprecated.
    if (CCKeyDerivationHMac(kCCKDFAlgorithmHKDF, digestAlgorithm, 0, key.key().data(), key.key().size(), 0, 0, parameters.infoVector().data(), parameters.infoVector().size(), 0, 0, parameters.saltVector().data(), parameters.saltVector().size(), result.data(), result.size()))
        return Exception { OperationError };
    ALLOW_DEPRECATED_DECLARATIONS_END
    return WTFMove(result);
}

}

#endif // ENABLE(WEB_CRYPTO)
