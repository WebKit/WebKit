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
#include "ServiceWorkerTypes.h"
#include <wtf/URL.h>
#include "WorkerType.h"

namespace WebCore {

struct ServiceWorkerData {
    ServiceWorkerIdentifier identifier;
    URL scriptURL;
    ServiceWorkerState state;
    WorkerType type;
    ServiceWorkerRegistrationIdentifier registrationIdentifier;

    ServiceWorkerData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerData> decode(Decoder&);
};

template<class Encoder>
void ServiceWorkerData::encode(Encoder& encoder) const
{
    encoder << identifier << scriptURL << state << type << registrationIdentifier;
}

template<class Decoder>
std::optional<ServiceWorkerData> ServiceWorkerData::decode(Decoder& decoder)
{
    std::optional<ServiceWorkerIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<URL> scriptURL;
    decoder >> scriptURL;
    if (!scriptURL)
        return std::nullopt;

    std::optional<ServiceWorkerState> state;
    decoder >> state;
    if (!state)
        return std::nullopt;

    std::optional<WorkerType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<ServiceWorkerRegistrationIdentifier> registrationIdentifier;
    decoder >> registrationIdentifier;
    if (!registrationIdentifier)
        return std::nullopt;

    return { { WTFMove(*identifier), WTFMove(*scriptURL), WTFMove(*state), WTFMove(*type), WTFMove(*registrationIdentifier) } };
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
