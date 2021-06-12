/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include <wtf/Hasher.h>

namespace WebCore {

struct ServiceWorkerClientIdentifier {
    SWServerConnectionIdentifier serverConnectionIdentifier;
    DocumentIdentifier contextIdentifier;

    unsigned hash() const;

    String toString() const { return makeString(serverConnectionIdentifier.toUInt64(), '-', contextIdentifier.toUInt64()); }
    static std::optional<ServiceWorkerClientIdentifier> fromString(StringView);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerClientIdentifier> decode(Decoder&);
};

inline bool operator==(const ServiceWorkerClientIdentifier& a, const ServiceWorkerClientIdentifier& b)
{
    return a.serverConnectionIdentifier == b.serverConnectionIdentifier && a.contextIdentifier == b.contextIdentifier;
}

template<class Encoder>
void ServiceWorkerClientIdentifier::encode(Encoder& encoder) const
{
    encoder << serverConnectionIdentifier << contextIdentifier;
}

template<class Decoder>
std::optional<ServiceWorkerClientIdentifier> ServiceWorkerClientIdentifier::decode(Decoder& decoder)
{
    std::optional<SWServerConnectionIdentifier> serverConnectionIdentifier;
    decoder >> serverConnectionIdentifier;
    if (!serverConnectionIdentifier)
        return std::nullopt;

    std::optional<DocumentIdentifier> contextIdentifier;
    decoder >> contextIdentifier;
    if (!contextIdentifier)
        return std::nullopt;

    return { { *serverConnectionIdentifier, *contextIdentifier } };
}

inline unsigned ServiceWorkerClientIdentifier::hash() const
{
    return computeHash(serverConnectionIdentifier, contextIdentifier);
}

struct ServiceWorkerClientIdentifierHash {
    static unsigned hash(const WebCore::ServiceWorkerClientIdentifier& key) { return key.hash(); }
    static bool equal(const WebCore::ServiceWorkerClientIdentifier& a, const WebCore::ServiceWorkerClientIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::ServiceWorkerClientIdentifier> : GenericHashTraits<WebCore::ServiceWorkerClientIdentifier> {
    static WebCore::ServiceWorkerClientIdentifier emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::ServiceWorkerClientIdentifier& slot) { new (NotNull, &slot.serverConnectionIdentifier) WebCore::SWServerConnectionIdentifier(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::ServiceWorkerClientIdentifier& slot) { return slot.serverConnectionIdentifier.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::ServiceWorkerClientIdentifier> : WebCore::ServiceWorkerClientIdentifierHash { };

}

#endif // ENABLE(SERVICE_WORKER)
