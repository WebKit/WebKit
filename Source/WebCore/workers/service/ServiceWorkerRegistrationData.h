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

#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"

namespace WebCore {

enum class ServiceWorkerUpdateViaCache;

struct ServiceWorkerRegistrationData {
    ServiceWorkerRegistrationKey key;
    uint64_t identifier;
    URL scopeURL;
    URL scriptURL;
    ServiceWorkerUpdateViaCache updateViaCache;

    std::optional<ServiceWorkerIdentifier> installingServiceWorkerIdentifier;
    std::optional<ServiceWorkerIdentifier> waitingServiceWorkerIdentifier;
    std::optional<ServiceWorkerIdentifier> activeServiceWorkerIdentifier;

    ServiceWorkerRegistrationData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerRegistrationData> decode(Decoder&);
};


template<class Encoder>
void ServiceWorkerRegistrationData::encode(Encoder& encoder) const
{
    encoder << key << identifier << scopeURL << scriptURL << updateViaCache << installingServiceWorkerIdentifier << waitingServiceWorkerIdentifier << activeServiceWorkerIdentifier;
}

template<class Decoder>
std::optional<ServiceWorkerRegistrationData> ServiceWorkerRegistrationData::decode(Decoder& decoder)
{
    std::optional<ServiceWorkerRegistrationKey> key;
    decoder >> key;
    if (!key)
        return std::nullopt;
    
    std::optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<URL> scopeURL;
    decoder >> scopeURL;
    if (!scopeURL)
        return std::nullopt;

    std::optional<URL> scriptURL;
    decoder >> scriptURL;
    if (!scriptURL)
        return std::nullopt;

    std::optional<ServiceWorkerUpdateViaCache> updateViaCache;
    decoder >> updateViaCache;
    if (!updateViaCache)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerIdentifier>> installingServiceWorkerIdentifier;
    decoder >> installingServiceWorkerIdentifier;
    if (!installingServiceWorkerIdentifier)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerIdentifier>> waitingServiceWorkerIdentifier;
    decoder >> waitingServiceWorkerIdentifier;
    if (!waitingServiceWorkerIdentifier)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerIdentifier>> activeServiceWorkerIdentifier;
    decoder >> activeServiceWorkerIdentifier;
    if (!activeServiceWorkerIdentifier)
        return std::nullopt;

    return { { WTFMove(*key), WTFMove(*identifier), WTFMove(*scopeURL), WTFMove(*scriptURL), WTFMove(*updateViaCache), WTFMove(*installingServiceWorkerIdentifier), WTFMove(*waitingServiceWorkerIdentifier), WTFMove(*activeServiceWorkerIdentifier) } };
}

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
