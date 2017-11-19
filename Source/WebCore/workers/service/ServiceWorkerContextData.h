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

#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJobDataIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"
#include "URL.h"
#include "WorkerType.h"

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

struct ServiceWorkerContextData {
    ServiceWorkerJobDataIdentifier jobDataIdentifier;
    ServiceWorkerRegistrationKey registrationKey;
    ServiceWorkerIdentifier serviceWorkerIdentifier;
    String script;
    URL scriptURL;
    WorkerType workerType;
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerContextData> decode(Decoder&);
    
    ServiceWorkerContextData isolatedCopy() const;
};

template<class Encoder>
void ServiceWorkerContextData::encode(Encoder& encoder) const
{
    encoder << jobDataIdentifier << registrationKey << serviceWorkerIdentifier << script << scriptURL << workerType;
}

template<class Decoder>
std::optional<ServiceWorkerContextData> ServiceWorkerContextData::decode(Decoder& decoder)
{
    std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier;
    decoder >> jobDataIdentifier;
    if (!jobDataIdentifier)
        return std::nullopt;

    auto registrationKey = ServiceWorkerRegistrationKey::decode(decoder);
    if (!registrationKey)
        return std::nullopt;

    auto serviceWorkerIdentifier = ServiceWorkerIdentifier::decode(decoder);
    if (!serviceWorkerIdentifier)
        return std::nullopt;

    String script;
    if (!decoder.decode(script))
        return std::nullopt;
    
    URL scriptURL;
    if (!decoder.decode(scriptURL))
        return std::nullopt;
    
    WorkerType workerType;
    if (!decoder.decodeEnum(workerType))
        return std::nullopt;
    
    return {{ WTFMove(*jobDataIdentifier), WTFMove(*registrationKey), WTFMove(*serviceWorkerIdentifier), WTFMove(script), WTFMove(scriptURL), workerType }};
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
