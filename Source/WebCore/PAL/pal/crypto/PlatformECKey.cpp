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

#include "config.h"
#include "PlatformECKey.h"

#include "PALSwift-Generated.h"

namespace PAL::Crypto {

PlatformECKey::PlatformECKey(NamedCurve curve)
    : PlatformECKey(pal::ECKey::init(curve))
{
}

PlatformECKey::PlatformECKey(const pal::ECKey& key)
    : m_key(makeUniqueRefWithoutFastMallocCheck<pal::ECKey>(key))
{
}

PlatformECKey::PlatformECKey(PlatformECKey&&) = default;

PlatformECKey& PlatformECKey::operator=(PlatformECKey&&) = default;

PlatformECKey::~PlatformECKey() = default;

CryptoOperationReturnValue PlatformECKey::deriveBits(const PlatformECKey& publicKey) const
{
    return m_key->deriveBits(publicKey.m_key.get());
}

CryptoOperationReturnValue PlatformECKey::sign(SpanConstUInt8 message, CryptoDigestHashFunction hashFunction) const
{
    return m_key->sign(message, hashFunction);
}

CryptoOperationReturnValue PlatformECKey::doVerify(SpanConstUInt8 message, SpanConstUInt8 signature, CryptoDigestHashFunction hashFunction) const
{
    return m_key->verify(message, signature, hashFunction);
}

PlatformECKey PlatformECKey::toPub() const
{
    return { m_key->toPub() };
}

CryptoOperationReturnValue PlatformECKey::exportX963Pub() const
{
    return m_key->exportX963Pub();
}

CryptoOperationReturnValue PlatformECKey::exportX963Private() const
{
    return m_key->exportX963Private();
}

std::optional<PlatformECKey> PlatformECKey::importX963Pub(SpanConstUInt8 data, NamedCurve curve)
{
    auto result = pal::ECKey::importX963Pub(data, curve);
    if (result)
        return { { result.get() } };

    return std::nullopt;
}

std::optional<PlatformECKey> PlatformECKey::importX963Private(SpanConstUInt8 data, NamedCurve curve)
{
    auto result = pal::ECKey::importX963Private(data, curve);
    if (result)
        return { { result.get() } };

    return std::nullopt;
}

std::optional<PlatformECKey> PlatformECKey::importCompressedPub(SpanConstUInt8 data, NamedCurve curve)
{
    auto result = pal::ECKey::importCompressedPub(data, curve);
    if (result)
        return { { result.get() } };

    return std::nullopt;
}

}
