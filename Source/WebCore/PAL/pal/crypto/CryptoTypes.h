/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#include <cstdint>
#include <span>
#include <wtf/Vector.h>

namespace PAL::Crypto {

using VectorUInt8 = WTF::Vector<uint8_t>;

using SpanConstUInt8 = std::span<const uint8_t>;

enum class CryptoDigestHashFunction: int {
    SHA_1,
    DEPRECATED_SHA_224,
    SHA_256,
    SHA_384,
    SHA_512,
};

enum class Error: int {
    Success = 0,
    WrongTagSize,
    EncryptionFailed,
    EncryptionResultNil,
    InvalidArgument,
    TooBigArguments,
    DecryptionFailed,
    HashingFailed,
    PublicKeyProvidedToSign,
    FailedToSign,
    FailedToVerify,
    PrivateKeyProvidedForVerification,
    FailedToImport,
    FailedToDerive,
    FailedToExport,
    DefaultValue,
    UnsupportedAlgorithm,
};

struct CryptoOperationReturnValue {
    Error errorCode = Error::DefaultValue;
    VectorUInt8 result;
};

enum class ECNamedCurve : uint8_t {
    P256,
    P384,
    P521,
};

enum class EdSigningAlgorithm : uint8_t {
    ED25519,
    ED448,
};

enum class EdKeyAgreementAlgorithm : uint8_t {
    X25519,
    X448,
};

constexpr auto ed25519KeySize = 32;
constexpr auto ed25519SignatureSize = ed25519KeySize * 2;

} // namespace PAL::Crypto

#endif // __cplusplus
