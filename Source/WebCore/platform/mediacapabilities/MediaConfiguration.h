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

#include "AudioConfiguration.h"
#include "VideoConfiguration.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

struct MediaConfiguration {
    std::optional<VideoConfiguration> video;
    std::optional<AudioConfiguration> audio;

    std::optional<Vector<String>> allowedMediaContainerTypes;
    std::optional<Vector<String>> allowedMediaCodecTypes;

    MediaConfiguration isolatedCopy() const &;
    MediaConfiguration isolatedCopy() &&;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaConfiguration> decode(Decoder&);
};

inline MediaConfiguration MediaConfiguration::isolatedCopy() const &
{
    return { crossThreadCopy(video),  crossThreadCopy(audio), crossThreadCopy(allowedMediaContainerTypes), crossThreadCopy(allowedMediaCodecTypes) };
}

inline MediaConfiguration MediaConfiguration::isolatedCopy() &&
{
    return { crossThreadCopy(WTFMove(video)),  crossThreadCopy(WTFMove(audio)), crossThreadCopy(WTFMove(allowedMediaContainerTypes)), crossThreadCopy(WTFMove(allowedMediaCodecTypes)) };
}

template<class Encoder>
void MediaConfiguration::encode(Encoder& encoder) const
{
    encoder << video;
    encoder << audio;
    encoder << allowedMediaContainerTypes;
    encoder << allowedMediaCodecTypes;
}

template<class Decoder>
std::optional<MediaConfiguration> MediaConfiguration::decode(Decoder& decoder)
{
    std::optional<std::optional<VideoConfiguration>> video;
    decoder >> video;
    if (!video)
        return std::nullopt;

    std::optional<std::optional<AudioConfiguration>> audio;
    decoder >> audio;
    if (!audio)
        return std::nullopt;

    std::optional<std::optional<Vector<String>>> allowedMediaContainerTypes;
    decoder >> allowedMediaContainerTypes;
    if (!allowedMediaContainerTypes)
        return std::nullopt;

    std::optional<std::optional<Vector<String>>> allowedMediaCodecTypes;
    decoder >> allowedMediaCodecTypes;
    if (!allowedMediaCodecTypes)
        return std::nullopt;

    return {{
        *video,
        *audio,
        *allowedMediaContainerTypes,
        *allowedMediaCodecTypes,
    }};
}

} // namespace WebCore

