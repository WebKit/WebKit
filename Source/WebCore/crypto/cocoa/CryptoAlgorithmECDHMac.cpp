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
#include "CryptoAlgorithmECDH.h"

#include "CommonCryptoUtilities.h"
#include "CryptoKeyEC.h"
#include <pal/PALSwift.h>

namespace WebCore {

static std::optional<Vector<uint8_t>> platformDeriveBitsCryptoKit(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    auto rv = baseKey.platformKey()->deriveBits(publicKey.platformKey());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return std::nullopt;
    return std::make_optional(WTFMove(rv.result));
#else
    UNUSED_PARAM(baseKey);
    UNUSED_PARAM(publicKey);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    return platformDeriveBitsCryptoKit(baseKey, publicKey);
}

} // namespace WebCore
