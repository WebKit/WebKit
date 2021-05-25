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

#include "ServiceWorkerClientIdentifier.h"
#include "ServiceWorkerClientType.h"
#include "ServiceWorkerTypes.h"
#include <wtf/URL.h>

namespace WebCore {

class SWClientConnection;
class ScriptExecutionContext;

enum class LastNavigationWasAppBound : bool { No, Yes };

struct ServiceWorkerClientData {
    ServiceWorkerClientIdentifier identifier;
    ServiceWorkerClientType type;
    ServiceWorkerClientFrameType frameType;
    URL url;
    LastNavigationWasAppBound lastNavigationWasAppBound;

    ServiceWorkerClientData isolatedCopy() const;

    static ServiceWorkerClientData from(ScriptExecutionContext&, SWClientConnection&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerClientData> decode(Decoder&);
};

template<class Encoder>
void ServiceWorkerClientData::encode(Encoder& encoder) const
{
    encoder << identifier << type << frameType << url << lastNavigationWasAppBound;
}

template<class Decoder>
Optional<ServiceWorkerClientData> ServiceWorkerClientData::decode(Decoder& decoder)
{
    Optional<ServiceWorkerClientIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;

    Optional<ServiceWorkerClientType> type;
    decoder >> type;
    if (!type)
        return WTF::nullopt;

    Optional<ServiceWorkerClientFrameType> frameType;
    decoder >> frameType;
    if (!frameType)
        return WTF::nullopt;

    Optional<URL> url;
    decoder >> url;
    if (!url)
        return WTF::nullopt;

    Optional<LastNavigationWasAppBound> lastNavigationWasAppBound;
    decoder >> lastNavigationWasAppBound;
    if (!lastNavigationWasAppBound)
        return WTF::nullopt;

    return { { WTFMove(*identifier), WTFMove(*type), WTFMove(*frameType), WTFMove(*url), WTFMove(*lastNavigationWasAppBound) } };
}

using ServiceWorkerClientsMatchAllCallback = WTF::CompletionHandler<void(Vector<ServiceWorkerClientData>&&)>;

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
