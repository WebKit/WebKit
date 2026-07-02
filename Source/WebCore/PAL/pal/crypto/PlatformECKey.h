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

#include <pal/crypto/CryptoTypes.h>
#include <wtf/UniqueRef.h>

namespace pal {
class ECKey;
};

namespace PAL::Crypto {

class PlatformECKey {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PlatformECKey);
    WTF_MAKE_NONCOPYABLE(PlatformECKey);
public:
    using NamedCurve = ECNamedCurve;

    PlatformECKey(NamedCurve);

    PlatformECKey(PlatformECKey&&);

    PlatformECKey& operator=(PlatformECKey&&);

    ~PlatformECKey();

    CryptoOperationReturnValue deriveBits(const PlatformECKey& publicKey) const;

    CryptoOperationReturnValue sign(SpanConstUInt8 message, CryptoDigestHashFunction) const;

    CryptoOperationReturnValue doVerify(SpanConstUInt8 message, SpanConstUInt8 signature, CryptoDigestHashFunction) const;

    PlatformECKey toPub() const;

    CryptoOperationReturnValue exportX963Pub() const;

    CryptoOperationReturnValue exportX963Private() const;

    static std::optional<PlatformECKey> importX963Pub(SpanConstUInt8, NamedCurve);

    static std::optional<PlatformECKey> importX963Private(SpanConstUInt8, NamedCurve);

    static std::optional<PlatformECKey> importCompressedPub(SpanConstUInt8, NamedCurve);

private:
    PlatformECKey(const pal::ECKey&);

    UniqueRef<pal::ECKey> m_key;
};

} // namespace PAL::Crypto
