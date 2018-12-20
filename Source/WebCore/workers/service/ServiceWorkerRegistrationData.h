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

    Optional<ServiceWorkerData> installingWorker;
    Optional<ServiceWorkerData> waitingWorker;
    Optional<ServiceWorkerData> activeWorker;

    ServiceWorkerRegistrationData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerRegistrationData> decode(Decoder&);
};


template<class Encoder>
void ServiceWorkerRegistrationData::encode(Encoder& encoder) const
{
    encoder << key << identifier << scopeURL << updateViaCache << lastUpdateTime.secondsSinceEpoch().value() << installingWorker << waitingWorker << activeWorker;
}

template<class Decoder>
Optional<ServiceWorkerRegistrationData> ServiceWorkerRegistrationData::decode(Decoder& decoder)
{
    Optional<ServiceWorkerRegistrationKey> key;
    decoder >> key;
    if (!key)
        return WTF::nullopt;
    
    Optional<ServiceWorkerRegistrationIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;

    Optional<URL> scopeURL;
    decoder >> scopeURL;
    if (!scopeURL)
        return WTF::nullopt;

    Optional<ServiceWorkerUpdateViaCache> updateViaCache;
    decoder >> updateViaCache;
    if (!updateViaCache)
        return WTF::nullopt;

    Optional<double> rawWallTime;
    decoder >> rawWallTime;
    if (!rawWallTime)
        return WTF::nullopt;

    Optional<Optional<ServiceWorkerData>> installingWorker;
    decoder >> installingWorker;
    if (!installingWorker)
        return WTF::nullopt;

    Optional<Optional<ServiceWorkerData>> waitingWorker;
    decoder >> waitingWorker;
    if (!waitingWorker)
        return WTF::nullopt;

    Optional<Optional<ServiceWorkerData>> activeWorker;
    decoder >> activeWorker;
    if (!activeWorker)
        return WTF::nullopt;

    return { { WTFMove(*key), WTFMove(*identifier), WTFMove(*scopeURL), WTFMove(*updateViaCache), WallTime::fromRawSeconds(*rawWallTime), WTFMove(*installingWorker), WTFMove(*waitingWorker), WTFMove(*activeWorker) } };
}

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
