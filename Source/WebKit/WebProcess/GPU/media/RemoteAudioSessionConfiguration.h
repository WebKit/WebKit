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

    WebCore::AudioSession::CategoryType category { WebCore::AudioSession::CategoryType::None };
    WebCore::RouteSharingPolicy routeSharingPolicy { WebCore::RouteSharingPolicy::Default };
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
        encoder << category;
        encoder << routeSharingPolicy;
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
    static Optional<RemoteAudioSessionConfiguration> decode(Decoder& decoder)
    {
        Optional<WebCore::AudioSession::CategoryType> category;
        decoder >> category;
        if (!category)
            return WTF::nullopt;

        Optional<WebCore::RouteSharingPolicy> routeSharingPolicy;
        decoder >> routeSharingPolicy;
        if (!routeSharingPolicy)
            return WTF::nullopt;

        Optional<String> routingContextUID;
        decoder >> routingContextUID;
        if (!routingContextUID)
            return WTF::nullopt;

        Optional<float> sampleRate;
        decoder >> sampleRate;
        if (!sampleRate)
            return WTF::nullopt;

        Optional<size_t> bufferSize;
        decoder >> bufferSize;
        if (!bufferSize)
            return WTF::nullopt;

        Optional<size_t> numberOfOutputChannels;
        decoder >> numberOfOutputChannels;
        if (!numberOfOutputChannels)
            return WTF::nullopt;

        Optional<size_t> maximumNumberOfOutputChannels;
        decoder >> maximumNumberOfOutputChannels;
        if (!maximumNumberOfOutputChannels)
            return WTF::nullopt;

        Optional<size_t> preferredBufferSize;
        decoder >> preferredBufferSize;
        if (!preferredBufferSize)
            return WTF::nullopt;

        Optional<bool> isMuted;
        decoder >> isMuted;
        if (!isMuted)
            return WTF::nullopt;

        Optional<bool> isActive;
        decoder >> isActive;
        if (!isActive)
            return WTF::nullopt;

        return {{
            WTFMove(*category),
            WTFMove(*routeSharingPolicy),
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

