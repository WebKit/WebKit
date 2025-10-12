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
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwift.h>
#endif

namespace WebCore {

#if !HAVE(SWIFT_CPP_INTEROP)
static std::optional<Vector<uint8_t>> platformDeriveBitsCC(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    std::optional<Vector<uint8_t>> result = std::nullopt;
    Vector<uint8_t> derivedKey(baseKey.keySizeInBytes()); // Per https://tools.ietf.org/html/rfc6090#section-4.
    size_t size = derivedKey.size();

    if (!CCECCryptorComputeSharedSecret(baseKey.platformKey().get(), publicKey.platformKey().get(), derivedKey.mutableSpan().data(), &size))
        result = std::make_optional(WTFMove(derivedKey));
    return result;
}
#else
static std::optional<Vector<uint8_t>> platformDeriveBitsCryptoKit(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    auto rv = baseKey.platformKey()->deriveBits(publicKey.platformKey());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return std::nullopt;
    return std::make_optional(WTFMove(rv.result));
}
#endif

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return platformDeriveBitsCryptoKit(baseKey, publicKey);
#else
    return platformDeriveBitsCC(baseKey, publicKey);
#endif
}

} // namespace WebCore
