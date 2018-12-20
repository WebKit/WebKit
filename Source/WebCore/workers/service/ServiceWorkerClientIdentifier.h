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

#if ENABLE(SERVICE_WORKER)

#include "DocumentIdentifier.h"
#include "ServiceWorkerTypes.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ServiceWorkerClientIdentifier {
    SWServerConnectionIdentifier serverConnectionIdentifier;
    DocumentIdentifier contextIdentifier;

    unsigned hash() const;

    String toString() const { return String::number(serverConnectionIdentifier.toUInt64()) + "-" +  String::number(contextIdentifier.toUInt64()); }
    static Optional<ServiceWorkerClientIdentifier> fromString(StringView);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerClientIdentifier> decode(Decoder&);
};

inline bool operator==(const ServiceWorkerClientIdentifier& a, const ServiceWorkerClientIdentifier& b)
{
    return a.serverConnectionIdentifier == b.serverConnectionIdentifier &&  a.contextIdentifier == b.contextIdentifier;
}

inline Optional<ServiceWorkerClientIdentifier> ServiceWorkerClientIdentifier::fromString(StringView string)
{
    ServiceWorkerClientIdentifier clientIdentifier;

    unsigned counter = 0;
    for (auto item : string.split('-')) {
        auto identifier = item.toUInt64Strict();
        if (!identifier || !*identifier)
            return WTF::nullopt;
        if (!counter++)
            clientIdentifier.serverConnectionIdentifier = makeObjectIdentifier<SWServerConnectionIdentifierType>(identifier.value());
        else if (counter == 2)
            clientIdentifier.contextIdentifier = makeObjectIdentifier<DocumentIdentifierType>(identifier.value());
    }
    return (counter == 2) ? makeOptional(WTFMove(clientIdentifier)) : WTF::nullopt;
}

template<class Encoder>
void ServiceWorkerClientIdentifier::encode(Encoder& encoder) const
{
    encoder << serverConnectionIdentifier << contextIdentifier;
}

template<class Decoder>
Optional<ServiceWorkerClientIdentifier> ServiceWorkerClientIdentifier::decode(Decoder& decoder)
{
    Optional<SWServerConnectionIdentifier> serverConnectionIdentifier;
    decoder >> serverConnectionIdentifier;
    if (!serverConnectionIdentifier)
        return WTF::nullopt;

    Optional<DocumentIdentifier> contextIdentifier;
    decoder >> contextIdentifier;
    if (!contextIdentifier)
        return WTF::nullopt;

    return { { WTFMove(*serverConnectionIdentifier), WTFMove(*contextIdentifier) } };
}

inline unsigned ServiceWorkerClientIdentifier::hash() const
{
    unsigned hashes[2];
    hashes[0] = WTF::intHash(serverConnectionIdentifier.toUInt64());
    hashes[1] = WTF::intHash(contextIdentifier.toUInt64());

    return StringHasher::hashMemory(hashes, sizeof(hashes));
}

} // namespace WebCore

namespace WTF {

struct ServiceWorkerClientIdentifierHash {
    static unsigned hash(const WebCore::ServiceWorkerClientIdentifier& key) { return key.hash(); }
    static bool equal(const WebCore::ServiceWorkerClientIdentifier& a, const WebCore::ServiceWorkerClientIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::ServiceWorkerClientIdentifier> : GenericHashTraits<WebCore::ServiceWorkerClientIdentifier> {
    static WebCore::ServiceWorkerClientIdentifier emptyValue() { return { }; }

    static void constructDeletedValue(WebCore::ServiceWorkerClientIdentifier& slot) { slot.serverConnectionIdentifier = makeObjectIdentifier<WebCore::SWServerConnectionIdentifierType>(std::numeric_limits<uint64_t>::max()); }

    static bool isDeletedValue(const WebCore::ServiceWorkerClientIdentifier& slot) { return slot.serverConnectionIdentifier.toUInt64() == std::numeric_limits<uint64_t>::max(); }
};

template<> struct DefaultHash<WebCore::ServiceWorkerClientIdentifier> {
    typedef ServiceWorkerClientIdentifierHash Hash;
};

}

#endif // ENABLE(SERVICE_WORKER)
