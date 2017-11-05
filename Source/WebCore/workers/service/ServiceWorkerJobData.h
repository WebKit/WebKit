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
#include "ServiceWorkerJobType.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerRegistrationOptions.h"
#include "URL.h"
#include <wtf/Identified.h>

namespace WebCore {

struct ServiceWorkerJobData : public ThreadSafeIdentified<ServiceWorkerJobData> {
public:
    explicit ServiceWorkerJobData(uint64_t connectionIdentifier);
    ServiceWorkerJobData(const ServiceWorkerJobData&) = default;
    ServiceWorkerJobData() = default;

    uint64_t connectionIdentifier() const { return m_connectionIdentifier; }

    URL scriptURL;
    URL clientCreationURL;
    SecurityOriginData topOrigin;
    URL scopeURL;
    ServiceWorkerJobType type;

    RegistrationOptions registrationOptions;

    ServiceWorkerRegistrationKey registrationKey() const;
    ServiceWorkerJobData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerJobData> decode(Decoder&);

private:
    WEBCORE_EXPORT ServiceWorkerJobData(uint64_t jobIdentifier, uint64_t connectionIdentifier);

    uint64_t m_connectionIdentifier { 0 };
};

template<class Encoder>
void ServiceWorkerJobData::encode(Encoder& encoder) const
{
    encoder << identifier() << m_connectionIdentifier << scriptURL << clientCreationURL << topOrigin << scopeURL;
    encoder.encodeEnum(type);
    switch (type) {
    case ServiceWorkerJobType::Register:
        encoder << registrationOptions;
        break;
    case ServiceWorkerJobType::Unregister:
    case ServiceWorkerJobType::Update:
        break;
    }
}

template<class Decoder>
std::optional<ServiceWorkerJobData> ServiceWorkerJobData::decode(Decoder& decoder)
{
    uint64_t jobIdentifier;
    if (!decoder.decode(jobIdentifier))
        return std::nullopt;

    uint64_t connectionIdentifier;
    if (!decoder.decode(connectionIdentifier))
        return std::nullopt;

    std::optional<ServiceWorkerJobData> jobData = { { jobIdentifier, connectionIdentifier } };

    if (!decoder.decode(jobData->scriptURL))
        return std::nullopt;
    if (!decoder.decode(jobData->clientCreationURL))
        return std::nullopt;

    std::optional<SecurityOriginData> topOrigin;
    decoder >> topOrigin;
    if (!topOrigin)
        return std::nullopt;
    jobData->topOrigin = WTFMove(*topOrigin);

    if (!decoder.decode(jobData->scopeURL))
        return std::nullopt;
    if (!decoder.decodeEnum(jobData->type))
        return std::nullopt;

    switch (jobData->type) {
    case ServiceWorkerJobType::Register:
        if (!decoder.decode(jobData->registrationOptions))
            return std::nullopt;
        break;
    case ServiceWorkerJobType::Unregister:
    case ServiceWorkerJobType::Update:
        break;
    }

    return jobData;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
