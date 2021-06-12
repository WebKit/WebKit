/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "MediaCapabilitiesInfo.h"
#include "MediaDecodingConfiguration.h"

namespace WebCore {

struct MediaCapabilitiesDecodingInfo : MediaCapabilitiesInfo {
    // FIXME(C++17): remove the following constructors once all compilers support extended
    // aggregate initialization:
    // <http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0017r1.html>
    MediaCapabilitiesDecodingInfo() = default;
    MediaCapabilitiesDecodingInfo(MediaDecodingConfiguration&& supportedConfiguration)
        : MediaCapabilitiesDecodingInfo({ }, WTFMove(supportedConfiguration))
    {
    }
    MediaCapabilitiesDecodingInfo(MediaCapabilitiesInfo&& info, MediaDecodingConfiguration&& supportedConfiguration)
        : MediaCapabilitiesInfo(WTFMove(info))
        , supportedConfiguration(WTFMove(supportedConfiguration))
    {
    }

    MediaDecodingConfiguration supportedConfiguration;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaCapabilitiesDecodingInfo> decode(Decoder&);
};

template<class Encoder>
void MediaCapabilitiesDecodingInfo::encode(Encoder& encoder) const
{
    MediaCapabilitiesInfo::encode(encoder);
    encoder << supportedConfiguration;
}

template<class Decoder>
std::optional<MediaCapabilitiesDecodingInfo> MediaCapabilitiesDecodingInfo::decode(Decoder& decoder)
{
    auto info = MediaCapabilitiesInfo::decode(decoder);
    if (!info)
        return std::nullopt;

    std::optional<MediaDecodingConfiguration> supportedConfiguration;
    decoder >> supportedConfiguration;
    if (!supportedConfiguration)
        return std::nullopt;

    return MediaCapabilitiesDecodingInfo(
        WTFMove(*info),
        WTFMove(*supportedConfiguration)
    );
}

} // namespace WebCore

