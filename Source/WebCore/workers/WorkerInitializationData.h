/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerData.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct WorkerInitializationData {
#if ENABLE(SERVICE_WORKER)
    std::optional<ServiceWorkerData> serviceWorkerData;
#endif
    std::optional<ScriptExecutionContextIdentifier> clientIdentifier;
    String userAgent;

    WorkerInitializationData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<WorkerInitializationData> decode(Decoder&);
};

inline WorkerInitializationData WorkerInitializationData::isolatedCopy() const
{
    return {
#if ENABLE(SERVICE_WORKER)
        crossThreadCopy(serviceWorkerData),
#endif
        clientIdentifier,
        userAgent.isolatedCopy()
    };
}


template<class Encoder>
void WorkerInitializationData::encode(Encoder& encoder) const
{
#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkerData;
#endif
    encoder << clientIdentifier << userAgent;
}

template<class Decoder>
std::optional<WorkerInitializationData> WorkerInitializationData::decode(Decoder& decoder)
{
#if ENABLE(SERVICE_WORKER)
    std::optional<std::optional<ServiceWorkerData>> serviceWorkerData;
    decoder >> serviceWorkerData;
    if (!serviceWorkerData)
        return { };
#endif

    std::optional<std::optional<ScriptExecutionContextIdentifier>> clientIdentifier;
    decoder >> clientIdentifier;
    if (!clientIdentifier)
        return { };

    std::optional<String> userAgent;
    decoder >> userAgent;
    if (!userAgent)
        return { };

    return WorkerInitializationData {
#if ENABLE(SERVICE_WORKER)
        WTFMove(*serviceWorkerData),
#endif
        WTFMove(*clientIdentifier),
        WTFMove(*userAgent)
    };
}

} // namespace WebCore
