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

#include <cstdint>
#include <optional>
#include <span>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace Cpp {

using VectorUInt8 = WTF::Vector<uint8_t>;
using SpanConstUInt8 = std::span<const uint8_t>;

enum class ErrorCodes: int {
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
    ErrorCodes errorCode = ErrorCodes::DefaultValue;
    VectorUInt8 result;
};

} // Cpp

#ifndef __swift__
// FIXME: This is incorrect, but the rest of this file is also incorrect for the time being, so alas.
#include <pal/crypto/CryptoDigestHashFunction.h>

#if !defined(CLANG_WEBKIT_BRANCH)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include "PALSwift-Generated.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

#endif // !__swift__
