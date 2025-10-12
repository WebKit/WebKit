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
#include "CryptoAlgorithmPBKDF2.h"

#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoKeyRaw.h"
#include <CommonCrypto/CommonKeyDerivation.h>

namespace WebCore {

namespace CryptoAlgorithmPBKDF2MacInternal {
static CCPseudoRandomAlgorithm commonCryptoHMACAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return kCCPRFHmacAlgSHA1;
    case CryptoAlgorithmIdentifier::DEPRECATED_SHA_224:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE(sha224DeprecationMessage);
        return kCCPRFHmacAlgSHA256;
    case CryptoAlgorithmIdentifier::SHA_256:
        return kCCPRFHmacAlgSHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return kCCPRFHmacAlgSHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return kCCPRFHmacAlgSHA512;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmPBKDF2::platformDeriveBits(const CryptoAlgorithmPbkdf2Params& parameters, const CryptoKeyRaw& key, size_t length)
{
    Vector<uint8_t> result(length / 8);
    // CCKeyDerivationPBKDF returns an error if the key pointer is null, even if the key length is 0. For this reason, we need to make sure we pass the empty string
    // instead of a null pointer in this case.
    auto keySpan = spanReinterpretCast<const char>(key.key().span());
    if (CCKeyDerivationPBKDF(kCCPBKDF2, keySpan.data() ? keySpan.data() : "", key.key().size(), parameters.saltVector().span().data(), parameters.saltVector().size(), CryptoAlgorithmPBKDF2MacInternal::commonCryptoHMACAlgorithm(parameters.hashIdentifier), parameters.iterations, result.mutableSpan().data(), length / 8))
        return Exception { ExceptionCode::OperationError };
    return WTFMove(result);
}

} // namespace WebCore
