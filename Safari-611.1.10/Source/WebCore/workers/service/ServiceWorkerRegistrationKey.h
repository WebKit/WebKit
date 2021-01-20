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

#include "SecurityOriginData.h"
#include <wtf/URL.h>

namespace WebCore {

class ServiceWorkerRegistrationKey {
public:
    ServiceWorkerRegistrationKey() = default;
    WEBCORE_EXPORT ServiceWorkerRegistrationKey(SecurityOriginData&& topOrigin, URL&& scope);

    static ServiceWorkerRegistrationKey emptyKey();
    unsigned hash() const;

    bool operator==(const ServiceWorkerRegistrationKey&) const;
    bool operator!=(const ServiceWorkerRegistrationKey& key) const { return !(*this == key); }
    bool isEmpty() const { return *this == emptyKey(); }
    WEBCORE_EXPORT bool isMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const;
    bool originIsMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const;
    size_t scopeLength() const { return m_scope.string().length(); }

    const SecurityOriginData& topOrigin() const { return m_topOrigin; }
    const URL& scope() const { return m_scope; }
    void setScope(URL&& scope) { m_scope = WTFMove(scope); }

    bool relatesToOrigin(const SecurityOriginData&) const;

    ServiceWorkerRegistrationKey isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerRegistrationKey> decode(Decoder&);

    String toDatabaseKey() const;
    static Optional<ServiceWorkerRegistrationKey> fromDatabaseKey(const String&);

#if !LOG_DISABLED
    String loggingString() const;
#endif

private:
    SecurityOriginData m_topOrigin;
    URL m_scope;
};

template<class Encoder>
void ServiceWorkerRegistrationKey::encode(Encoder& encoder) const
{
    RELEASE_ASSERT(!m_topOrigin.isEmpty());
    RELEASE_ASSERT(!m_scope.isNull());
    encoder << m_topOrigin << m_scope;
}

template<class Decoder>
Optional<ServiceWorkerRegistrationKey> ServiceWorkerRegistrationKey::decode(Decoder& decoder)
{
    Optional<SecurityOriginData> topOrigin;
    decoder >> topOrigin;
    if (!topOrigin)
        return WTF::nullopt;

    URL scope;
    if (!decoder.decode(scope))
        return WTF::nullopt;

    return ServiceWorkerRegistrationKey { WTFMove(*topOrigin), WTFMove(scope) };
}

} // namespace WebCore

namespace WTF {

struct ServiceWorkerRegistrationKeyHash {
    static unsigned hash(const WebCore::ServiceWorkerRegistrationKey& key) { return key.hash(); }
    static bool equal(const WebCore::ServiceWorkerRegistrationKey& a, const WebCore::ServiceWorkerRegistrationKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<> struct HashTraits<WebCore::ServiceWorkerRegistrationKey> : GenericHashTraits<WebCore::ServiceWorkerRegistrationKey> {
    static WebCore::ServiceWorkerRegistrationKey emptyValue() { return WebCore::ServiceWorkerRegistrationKey::emptyKey(); }

    static void constructDeletedValue(WebCore::ServiceWorkerRegistrationKey& slot) { slot.setScope(URL(HashTableDeletedValue)); }
    static bool isDeletedValue(const WebCore::ServiceWorkerRegistrationKey& slot) { return slot.scope().isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::ServiceWorkerRegistrationKey> : ServiceWorkerRegistrationKeyHash { };

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
