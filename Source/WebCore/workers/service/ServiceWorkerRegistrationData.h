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

#include "ServiceWorkerData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include "ServiceWorkerUpdateViaCache.h"
#include <wtf/WallTime.h>

namespace WebCore {

enum class ServiceWorkerUpdateViaCache : uint8_t;

struct ServiceWorkerRegistrationData {
    ServiceWorkerRegistrationKey key;
    ServiceWorkerRegistrationIdentifier identifier;
    URL scopeURL;
    ServiceWorkerUpdateViaCache updateViaCache;
    WallTime lastUpdateTime;

    std::optional<ServiceWorkerData> installingWorker;
    std::optional<ServiceWorkerData> waitingWorker;
    std::optional<ServiceWorkerData> activeWorker;

    ServiceWorkerRegistrationData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerRegistrationData> decode(Decoder&);
};


template<class Encoder>
void ServiceWorkerRegistrationData::encode(Encoder& encoder) const
{
    encoder << key << identifier << scopeURL << updateViaCache << lastUpdateTime.secondsSinceEpoch().value() << installingWorker << waitingWorker << activeWorker;
}

template<class Decoder>
std::optional<ServiceWorkerRegistrationData> ServiceWorkerRegistrationData::decode(Decoder& decoder)
{
    std::optional<ServiceWorkerRegistrationKey> key;
    decoder >> key;
    if (!key)
        return std::nullopt;
    
    std::optional<ServiceWorkerRegistrationIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<URL> scopeURL;
    decoder >> scopeURL;
    if (!scopeURL)
        return std::nullopt;

    std::optional<ServiceWorkerUpdateViaCache> updateViaCache;
    decoder >> updateViaCache;
    if (!updateViaCache)
        return std::nullopt;

    std::optional<double> rawWallTime;
    decoder >> rawWallTime;
    if (!rawWallTime)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerData>> installingWorker;
    decoder >> installingWorker;
    if (!installingWorker)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerData>> waitingWorker;
    decoder >> waitingWorker;
    if (!waitingWorker)
        return std::nullopt;

    std::optional<std::optional<ServiceWorkerData>> activeWorker;
    decoder >> activeWorker;
    if (!activeWorker)
        return std::nullopt;

    return { { WTFMove(*key), WTFMove(*identifier), WTFMove(*scopeURL), WTFMove(*updateViaCache), WallTime::fromRawSeconds(*rawWallTime), WTFMove(*installingWorker), WTFMove(*waitingWorker), WTFMove(*activeWorker) } };
}

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
