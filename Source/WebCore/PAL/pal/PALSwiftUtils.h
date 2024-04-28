/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(SWIFT_CPP_INTEROP)
#include "PALSwift.h"

namespace WebCore {
inline PAL::HashFunction toCKHashFunction(CryptoAlgorithmIdentifier hash)
{
    switch (hash) {
    case CryptoAlgorithmIdentifier::SHA_256:
        return PAL::HashFunction::sha256();
        break;
    case CryptoAlgorithmIdentifier::SHA_384:
        return PAL::HashFunction::sha384();
        break;
    case CryptoAlgorithmIdentifier::SHA_512:
        return PAL::HashFunction::sha512();
        break;
    case CryptoAlgorithmIdentifier::SHA_1:
        return PAL::HashFunction::sha1();
        break;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return PAL::HashFunction::sha512();
}

inline bool isValidHashParameter(CryptoAlgorithmIdentifier hash)
{
    return hash == CryptoAlgorithmIdentifier::SHA_1 || hash == CryptoAlgorithmIdentifier::SHA_256 || hash == CryptoAlgorithmIdentifier::SHA_512 || hash == CryptoAlgorithmIdentifier::SHA_384;
}

} // WebCore
#endif
