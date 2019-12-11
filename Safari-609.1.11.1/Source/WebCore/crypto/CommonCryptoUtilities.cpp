/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "CommonCryptoUtilities.h"

#if ENABLE(WEB_CRYPTO)

namespace WebCore {

bool getCommonCryptoDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction, CCDigestAlgorithm& algorithm)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        algorithm = kCCDigestSHA1;
        ALLOW_DEPRECATED_DECLARATIONS_END
        return true;
    case CryptoAlgorithmIdentifier::SHA_224:
        algorithm = kCCDigestSHA224;
        return true;
    case CryptoAlgorithmIdentifier::SHA_256:
        algorithm = kCCDigestSHA256;
        return true;
    case CryptoAlgorithmIdentifier::SHA_384:
        algorithm = kCCDigestSHA384;
        return true;
    case CryptoAlgorithmIdentifier::SHA_512:
        algorithm = kCCDigestSHA512;
        return true;
    default:
        return false;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
