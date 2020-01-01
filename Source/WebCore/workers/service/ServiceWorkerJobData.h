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
#include "ServiceWorkerClientIdentifier.h"
#include "ServiceWorkerJobDataIdentifier.h"
#include "ServiceWorkerJobType.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerRegistrationOptions.h"
#include "ServiceWorkerTypes.h"
#include <wtf/URL.h>

namespace WebCore {

struct ServiceWorkerJobData {
    using Identifier = ServiceWorkerJobDataIdentifier;
    ServiceWorkerJobData(SWServerConnectionIdentifier, const DocumentOrWorkerIdentifier& sourceContext);

    SWServerConnectionIdentifier connectionIdentifier() const { return m_identifier.connectionIdentifier; }

    bool isEquivalent(const ServiceWorkerJobData&) const;

    URL scriptURL;
    URL clientCreationURL;
    SecurityOriginData topOrigin;
    URL scopeURL;
    ServiceWorkerOrClientIdentifier sourceContext;
    ServiceWorkerJobType type;

    ServiceWorkerRegistrationOptions registrationOptions;

    Identifier identifier() const { return m_identifier; }
    WEBCORE_EXPORT ServiceWorkerRegistrationKey registrationKey() const;
    ServiceWorkerJobData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerJobData> decode(Decoder&);

private:
    ServiceWorkerJobData() = default;

    Identifier m_identifier;
};

template<class Encoder>
void ServiceWorkerJobData::encode(Encoder& encoder) const
{
    encoder << identifier() << scriptURL << clientCreationURL << topOrigin << scopeURL << sourceContext;
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
Optional<ServiceWorkerJobData> ServiceWorkerJobData::decode(Decoder& decoder)
{
    Optional<ServiceWorkerJobDataIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;

    ServiceWorkerJobData jobData;
    jobData.m_identifier = *identifier;

    if (!decoder.decode(jobData.scriptURL))
        return WTF::nullopt;
    if (!decoder.decode(jobData.clientCreationURL))
        return WTF::nullopt;

    Optional<SecurityOriginData> topOrigin;
    decoder >> topOrigin;
    if (!topOrigin)
        return WTF::nullopt;
    jobData.topOrigin = WTFMove(*topOrigin);

    if (!decoder.decode(jobData.scopeURL))
        return WTF::nullopt;
    if (!decoder.decode(jobData.sourceContext))
        return WTF::nullopt;
    if (!decoder.decodeEnum(jobData.type))
        return WTF::nullopt;

    switch (jobData.type) {
    case ServiceWorkerJobType::Register: {
        Optional<ServiceWorkerRegistrationOptions> registrationOptions;
        decoder >> registrationOptions;
        if (!registrationOptions)
            return WTF::nullopt;
        jobData.registrationOptions = WTFMove(*registrationOptions);
        break;
    }
    case ServiceWorkerJobType::Unregister:
    case ServiceWorkerJobType::Update:
        break;
    }

    return jobData;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
