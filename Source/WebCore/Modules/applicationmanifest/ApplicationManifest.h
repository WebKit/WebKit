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

#if ENABLE(APPLICATION_MANIFEST)

#include <wtf/EnumTraits.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>

namespace WebCore {

struct ApplicationManifest {
    enum class Display {
        Browser,
        MinimalUI,
        Standalone,
        Fullscreen,
    };

    String name;
    String shortName;
    String description;
    URL scope;
    Display display;
    URL startURL;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplicationManifest> decode(Decoder&);
};

template<class Encoder>
void ApplicationManifest::encode(Encoder& encoder) const
{
    encoder << name << shortName << description << scope << display << startURL;
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
    if (!decoder.decodeEnum(result.display))
        return std::nullopt;
    if (!decoder.decode(result.startURL))
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

} // namespace WTF;

#endif // ENABLE(APPLICATION_MANIFEST)

