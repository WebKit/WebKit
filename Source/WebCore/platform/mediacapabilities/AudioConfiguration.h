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

#include <wtf/text/WTFString.h>

namespace WebCore {

struct AudioConfiguration {
    String contentType;
    String channels;
    std::optional<uint64_t> bitrate;
    std::optional<uint32_t> samplerate;
    std::optional<bool> spatialRendering;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<AudioConfiguration> decode(Decoder&);
};

template<class Encoder>
void AudioConfiguration::encode(Encoder& encoder) const
{
    encoder << contentType;
    encoder << channels;
    encoder << bitrate;
    encoder << samplerate;
    encoder << spatialRendering;
}

template<class Decoder>
std::optional<AudioConfiguration> AudioConfiguration::decode(Decoder& decoder)
{
    std::optional<String> contentType;
    decoder >> contentType;
    if (!contentType)
        return std::nullopt;

    std::optional<String> channels;
    decoder >> channels;
    if (!channels)
        return std::nullopt;

    std::optional<std::optional<uint64_t>> bitrate;
    decoder >> bitrate;
    if (!bitrate)
        return std::nullopt;

    std::optional<std::optional<uint32_t>> sampleRate;
    decoder >> sampleRate;
    if (!sampleRate)
        return std::nullopt;

    std::optional<std::optional<bool>> spatialRendering;
    decoder >> spatialRendering;
    if (!spatialRendering)
        return std::nullopt;

    return {{
        *contentType,
        *channels,
        *bitrate,
        *sampleRate,
        *spatialRendering
    }};
}

} // namespace WebCore
