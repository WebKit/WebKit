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

#include "FrameIdentifier.h"
#include "PageIdentifier.h"
#include "ProcessQualified.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientType.h"
#include "ServiceWorkerTypes.h"
#include <wtf/URL.h>

namespace WebCore {

class SWClientConnection;
class ScriptExecutionContext;

enum class LastNavigationWasAppInitiated : bool { No, Yes };

struct ServiceWorkerClientData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ScriptExecutionContextIdentifier identifier;
    ServiceWorkerClientType type;
    ServiceWorkerClientFrameType frameType;
    URL url;
    URL ownerURL;
    std::optional<PageIdentifier> pageIdentifier;
    std::optional<FrameIdentifier> frameIdentifier;
    LastNavigationWasAppInitiated lastNavigationWasAppInitiated;
    bool isVisible { false };
    bool isFocused { false };
    uint64_t focusOrder { 0 };
    Vector<String> ancestorOrigins;

    WEBCORE_EXPORT ServiceWorkerClientData isolatedCopy() const &;
    WEBCORE_EXPORT ServiceWorkerClientData isolatedCopy() &&;

    WEBCORE_EXPORT static ServiceWorkerClientData from(ScriptExecutionContext&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerClientData> decode(Decoder&);
};

template<class Encoder>
void ServiceWorkerClientData::encode(Encoder& encoder) const
{
    encoder << identifier << type << frameType << url << ownerURL << pageIdentifier << frameIdentifier << lastNavigationWasAppInitiated << isVisible << isFocused << focusOrder << ancestorOrigins;
}

template<class Decoder>
std::optional<ServiceWorkerClientData> ServiceWorkerClientData::decode(Decoder& decoder)
{
    std::optional<ScriptExecutionContextIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<ServiceWorkerClientType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<ServiceWorkerClientFrameType> frameType;
    decoder >> frameType;
    if (!frameType)
        return std::nullopt;

    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    std::optional<URL> ownerURL;
    decoder >> ownerURL;
    if (!ownerURL)
        return std::nullopt;

    std::optional<std::optional<PageIdentifier>> pageIdentifier;
    decoder >> pageIdentifier;
    if (!pageIdentifier)
        return std::nullopt;

    std::optional<std::optional<FrameIdentifier>> frameIdentifier;
    decoder >> frameIdentifier;
    if (!frameIdentifier)
        return std::nullopt;

    std::optional<LastNavigationWasAppInitiated> lastNavigationWasAppInitiated;
    decoder >> lastNavigationWasAppInitiated;
    if (!lastNavigationWasAppInitiated)
        return std::nullopt;

    std::optional<bool> isVisible;
    decoder >> isVisible;
    if (!isVisible)
        return std::nullopt;

    std::optional<bool> isFocused;
    decoder >> isFocused;
    if (!isFocused)
        return std::nullopt;

    std::optional<uint64_t> focusOrder;
    decoder >> focusOrder;
    if (!focusOrder)
        return std::nullopt;

    std::optional<Vector<String>> ancestorOrigins;
    decoder >> ancestorOrigins;
    if (!ancestorOrigins)
        return std::nullopt;

    return { { WTFMove(*identifier), WTFMove(*type), WTFMove(*frameType), WTFMove(*url), WTFMove(*ownerURL), WTFMove(*pageIdentifier), WTFMove(*frameIdentifier), WTFMove(*lastNavigationWasAppInitiated), WTFMove(*isVisible), WTFMove(*isFocused), WTFMove(*focusOrder), WTFMove(*ancestorOrigins) } };
}

using ServiceWorkerClientsMatchAllCallback = CompletionHandler<void(Vector<ServiceWorkerClientData>&&)>;

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
