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

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include <WebCore/AudioSession.h>

namespace WebKit {

struct RemoteAudioSessionConfiguration {
    String routingContextUID;
    float sampleRate { 0 };
    size_t bufferSize { 0 };
    size_t numberOfOutputChannels { 0 };
    size_t maximumNumberOfOutputChannels { 0 };
    size_t preferredBufferSize { 0 };
    bool isMuted { false };
    bool isActive { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << routingContextUID;
        encoder << sampleRate;
        encoder << bufferSize;
        encoder << numberOfOutputChannels;
        encoder << maximumNumberOfOutputChannels;
        encoder << preferredBufferSize;
        encoder << isMuted;
        encoder << isActive;
    }

    template <class Decoder>
    static std::optional<RemoteAudioSessionConfiguration> decode(Decoder& decoder)
    {
        std::optional<String> routingContextUID;
        decoder >> routingContextUID;
        if (!routingContextUID)
            return std::nullopt;

        std::optional<float> sampleRate;
        decoder >> sampleRate;
        if (!sampleRate)
            return std::nullopt;

        std::optional<size_t> bufferSize;
        decoder >> bufferSize;
        if (!bufferSize)
            return std::nullopt;

        std::optional<size_t> numberOfOutputChannels;
        decoder >> numberOfOutputChannels;
        if (!numberOfOutputChannels)
            return std::nullopt;

        std::optional<size_t> maximumNumberOfOutputChannels;
        decoder >> maximumNumberOfOutputChannels;
        if (!maximumNumberOfOutputChannels)
            return std::nullopt;

        std::optional<size_t> preferredBufferSize;
        decoder >> preferredBufferSize;
        if (!preferredBufferSize)
            return std::nullopt;

        std::optional<bool> isMuted;
        decoder >> isMuted;
        if (!isMuted)
            return std::nullopt;

        std::optional<bool> isActive;
        decoder >> isActive;
        if (!isActive)
            return std::nullopt;

        return {{
            WTFMove(*routingContextUID),
            *sampleRate,
            *bufferSize,
            *numberOfOutputChannels,
            *maximumNumberOfOutputChannels,
            *preferredBufferSize,
            *isMuted,
            *isActive,
        }};
    }
};

} // namespace WebKit

#endif

