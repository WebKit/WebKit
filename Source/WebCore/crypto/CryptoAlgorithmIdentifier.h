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

#pragma once

#include <pal/crypto/CryptoDigestHashFunction.h>

namespace WebCore {

constexpr auto sha224DeprecationMessage = "SHA224 is not supported";
enum class CryptoAlgorithmIdentifier : uint8_t {
    RSAES_PKCS1_v1_5 = 1,
    RSASSA_PKCS1_v1_5,
    RSA_PSS,
    RSA_OAEP,
    ECDSA,
    ECDH,
    AES_CTR,
    AES_CBC,
    AES_GCM,
    AES_CFB,
    AES_KW,
    HMAC,
    SHA_1,
    DEPRECATED_SHA_224,
    SHA_256,
    SHA_384,
    SHA_512,
    HKDF,
    PBKDF2,
    Ed25519,
    X25519
};

inline PAL::CryptoDigestHashFunction toCKHashFunction(CryptoAlgorithmIdentifier hash)
{
    switch (hash) {
    case CryptoAlgorithmIdentifier::SHA_256:
        return PAL::CryptoDigestHashFunction::SHA_256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return PAL::CryptoDigestHashFunction::SHA_384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return PAL::CryptoDigestHashFunction::SHA_512;
    case CryptoAlgorithmIdentifier::SHA_1:
        return PAL::CryptoDigestHashFunction::SHA_1;
    default:
        ASSERT_NOT_REACHED();
        return PAL::CryptoDigestHashFunction::SHA_512;
    }
}

inline bool isValidHashParameter(CryptoAlgorithmIdentifier hash)
{
    return hash == CryptoAlgorithmIdentifier::SHA_1 || hash == CryptoAlgorithmIdentifier::SHA_256 || hash == CryptoAlgorithmIdentifier::SHA_512 || hash == CryptoAlgorithmIdentifier::SHA_384;
}

} // namespace WebCore
