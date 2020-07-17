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

#pragma once

#include "RegistrableDomain.h"
#include "SecurityOriginData.h"
#include <wtf/HashTraits.h>
#include <wtf/URL.h>

namespace WebCore {

struct ClientOrigin {
    static ClientOrigin emptyKey() { return { }; }

    unsigned hash() const;
    bool operator==(const ClientOrigin&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ClientOrigin> decode(Decoder&);

    ClientOrigin isolatedCopy() const;
    bool isRelated(const SecurityOriginData& other) const { return topOrigin == other || clientOrigin == other; }

    RegistrableDomain clientRegistrableDomain() const { return RegistrableDomain::uncheckedCreateFromHost(clientOrigin.host); }

    SecurityOriginData topOrigin;
    SecurityOriginData clientOrigin;
};

inline unsigned ClientOrigin::hash() const
{
    unsigned hashes[2];
    hashes[0] = SecurityOriginDataHash::hash(topOrigin);
    hashes[1] = SecurityOriginDataHash::hash(clientOrigin);

    return StringHasher::hashMemory(hashes, sizeof(hashes));
}

inline bool ClientOrigin::operator==(const ClientOrigin& other) const
{
    return topOrigin == other.topOrigin && clientOrigin == other.clientOrigin;
}

inline ClientOrigin ClientOrigin::isolatedCopy() const
{
    return { topOrigin.isolatedCopy(), clientOrigin.isolatedCopy() };
}

template<class Encoder> inline void ClientOrigin::encode(Encoder& encoder) const
{
    encoder << topOrigin;
    encoder << clientOrigin;
}

template<class Decoder> inline Optional<ClientOrigin> ClientOrigin::decode(Decoder& decoder)
{
    Optional<SecurityOriginData> topOrigin;
    Optional<SecurityOriginData> clientOrigin;
    decoder >> topOrigin;
    if (!topOrigin || topOrigin->isEmpty())
        return WTF::nullopt;
    decoder >> clientOrigin;
    if (!clientOrigin || clientOrigin->isEmpty())
        return WTF::nullopt;

    return ClientOrigin { WTFMove(*topOrigin), WTFMove(*clientOrigin) };
}

} // namespace WebCore

namespace WTF {

struct ClientOriginKeyHash {
    static unsigned hash(const WebCore::ClientOrigin& key) { return key.hash(); }
    static bool equal(const WebCore::ClientOrigin& a, const WebCore::ClientOrigin& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<> struct HashTraits<WebCore::ClientOrigin> : GenericHashTraits<WebCore::ClientOrigin> {
    static WebCore::ClientOrigin emptyValue() { return WebCore::ClientOrigin::emptyKey(); }

    static void constructDeletedValue(WebCore::ClientOrigin& slot) { slot.topOrigin = WebCore::SecurityOriginData(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::ClientOrigin& slot) { return slot.topOrigin.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::ClientOrigin> : ClientOriginKeyHash { };

} // namespace WTF
