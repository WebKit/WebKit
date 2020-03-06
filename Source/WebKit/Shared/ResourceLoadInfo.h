/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "NetworkResourceLoadIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>

namespace WebKit {

struct ResourceLoadInfo {

    enum class Type : uint8_t {
        ApplicationManifest,
        Beacon,
        CSPReport,
        Document,
        Fetch,
        Font,
        Image,
        Media,
        Object,
        Other,
        Ping,
        Script,
        Stylesheet,
        XMLHTTPRequest,
        XSLT
    };
    
    NetworkResourceLoadIdentifier resourceLoadID;
    Optional<WebCore::FrameIdentifier> frameID;
    Optional<WebCore::FrameIdentifier> parentFrameID;
    URL originalURL;
    String originalHTTPMethod;
    WallTime eventTimestamp;
    bool loadedFromCache { false };
    Type type { Type::Other };

    void encode(IPC::Encoder& encoder) const
    {
        encoder << resourceLoadID;
        encoder << frameID;
        encoder << parentFrameID;
        encoder << originalURL;
        encoder << originalHTTPMethod;
        encoder << eventTimestamp;
        encoder << loadedFromCache;
        encoder << type;
    }

    static Optional<ResourceLoadInfo> decode(IPC::Decoder& decoder)
    {
        Optional<NetworkResourceLoadIdentifier> resourceLoadID;
        decoder >> resourceLoadID;
        if (!resourceLoadID)
            return WTF::nullopt;

        Optional<Optional<WebCore::FrameIdentifier>> frameID;
        decoder >> frameID;
        if (!frameID)
            return WTF::nullopt;

        Optional<Optional<WebCore::FrameIdentifier>> parentFrameID;
        decoder >> parentFrameID;
        if (!parentFrameID)
            return WTF::nullopt;

        Optional<URL> originalURL;
        decoder >> originalURL;
        if (!originalURL)
            return WTF::nullopt;

        Optional<String> originalHTTPMethod;
        decoder >> originalHTTPMethod;
        if (!originalHTTPMethod)
            return WTF::nullopt;

        Optional<WallTime> eventTimestamp;
        decoder >> eventTimestamp;
        if (!eventTimestamp)
            return WTF::nullopt;

        Optional<bool> loadedFromCache;
        decoder >> loadedFromCache;
        if (!loadedFromCache)
            return WTF::nullopt;

        Optional<Type> type;
        decoder >> type;
        if (!type)
            return WTF::nullopt;

        return {{
            WTFMove(*resourceLoadID),
            WTFMove(*frameID),
            WTFMove(*parentFrameID),
            WTFMove(*originalURL),
            WTFMove(*originalHTTPMethod),
            WTFMove(*eventTimestamp),
            WTFMove(*loadedFromCache),
            WTFMove(*type),
        }};
    }
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::ResourceLoadInfo::Type> {
    using values = EnumValues<
        WebKit::ResourceLoadInfo::Type,
        WebKit::ResourceLoadInfo::Type::ApplicationManifest,
        WebKit::ResourceLoadInfo::Type::Beacon,
        WebKit::ResourceLoadInfo::Type::CSPReport,
        WebKit::ResourceLoadInfo::Type::Document,
        WebKit::ResourceLoadInfo::Type::Fetch,
        WebKit::ResourceLoadInfo::Type::Font,
        WebKit::ResourceLoadInfo::Type::Image,
        WebKit::ResourceLoadInfo::Type::Media,
        WebKit::ResourceLoadInfo::Type::Object,
        WebKit::ResourceLoadInfo::Type::Other,
        WebKit::ResourceLoadInfo::Type::Ping,
        WebKit::ResourceLoadInfo::Type::Script,
        WebKit::ResourceLoadInfo::Type::Stylesheet,
        WebKit::ResourceLoadInfo::Type::XMLHTTPRequest,
        WebKit::ResourceLoadInfo::Type::XSLT
    >;
};

} // namespace WTF
