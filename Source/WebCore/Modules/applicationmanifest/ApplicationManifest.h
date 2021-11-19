/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLICATION_MANIFEST)

#include "Color.h"
#include <wtf/EnumTraits.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ApplicationManifest {
    enum class Display {
        Browser,
        MinimalUI,
        Standalone,
        Fullscreen,
    };

    struct Icon {
        enum class Purpose : uint8_t  {
            Any = 1 << 0,
            Monochrome = 1 << 1,
            Maskable = 1 << 2,
        };

        URL src;
        Vector<String> sizes;
        String type;
        OptionSet<Purpose> purposes;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<ApplicationManifest::Icon> decode(Decoder&);
    };

    String name;
    String shortName;
    String description;
    URL scope;
    Display display;
    URL startURL;
    Color themeColor;
    Vector<Icon> icons;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplicationManifest> decode(Decoder&);
};

template<class Encoder>
void ApplicationManifest::encode(Encoder& encoder) const
{
    encoder << name << shortName << description << scope << display << startURL << themeColor << icons;
}

template<class Decoder>
std::optional<ApplicationManifest> ApplicationManifest::decode(Decoder& decoder)
{
    ApplicationManifest result;

    if (!decoder.decode(result.name))
        return std::nullopt;
    if (!decoder.decode(result.shortName))
        return std::nullopt;
    if (!decoder.decode(result.description))
        return std::nullopt;
    if (!decoder.decode(result.scope))
        return std::nullopt;
    if (!decoder.decode(result.display))
        return std::nullopt;
    if (!decoder.decode(result.startURL))
        return std::nullopt;
    if (!decoder.decode(result.themeColor))
        return std::nullopt;
    if (!decoder.decode(result.icons))
        return std::nullopt;

    return result;
}

template<class Encoder>
void ApplicationManifest::Icon::encode(Encoder& encoder) const
{
    encoder << src << sizes << type << purposes;
}

template<class Decoder>
std::optional<ApplicationManifest::Icon> ApplicationManifest::Icon::decode(Decoder& decoder)
{
    ApplicationManifest::Icon result;
    if (!decoder.decode(result.src))
        return std::nullopt;
    if (!decoder.decode(result.sizes))
        return std::nullopt;
    if (!decoder.decode(result.type))
        return std::nullopt;
    if (!decoder.decode(result.purposes))
        return std::nullopt;

    return result;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ApplicationManifest::Display> {
    using values = EnumValues<
        WebCore::ApplicationManifest::Display,
        WebCore::ApplicationManifest::Display::Browser,
        WebCore::ApplicationManifest::Display::MinimalUI,
        WebCore::ApplicationManifest::Display::Standalone,
        WebCore::ApplicationManifest::Display::Fullscreen
    >;
};

template<> struct EnumTraits<WebCore::ApplicationManifest::Icon::Purpose> {
    using values = EnumValues<
        WebCore::ApplicationManifest::Icon::Purpose,
        WebCore::ApplicationManifest::Icon::Purpose::Any,
        WebCore::ApplicationManifest::Icon::Purpose::Monochrome,
        WebCore::ApplicationManifest::Icon::Purpose::Maskable
    >;
};

} // namespace WTF;

#endif // ENABLE(APPLICATION_MANIFEST)

