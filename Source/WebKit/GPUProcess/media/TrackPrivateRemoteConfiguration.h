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

#if ENABLE(GPU_PROCESS)

#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/MediaTime.h>

namespace WebKit {

struct TrackPrivateRemoteConfiguration {
    AtomString trackId;
    AtomString label;
    AtomString language;
    MediaTime startTimeVariance { MediaTime::zeroTime() };
    int trackIndex;

    bool enabled;
    WebCore::AudioTrackPrivate::Kind audioKind { WebCore::AudioTrackPrivate::Kind::None };

    bool selected;
    WebCore::VideoTrackPrivate::Kind videoKind { WebCore::VideoTrackPrivate::Kind::None };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << trackId;
        encoder << label;
        encoder << language;
        encoder << startTimeVariance;
        encoder << trackIndex;
        encoder << enabled;
        encoder << audioKind;
        encoder << selected;
        encoder << videoKind;
    }

    template <class Decoder>
    static std::optional<TrackPrivateRemoteConfiguration> decode(Decoder& decoder)
    {
        std::optional<AtomString> trackId;
        decoder >> trackId;
        if (!trackId)
            return std::nullopt;

        std::optional<AtomString> label;
        decoder >> label;
        if (!label)
            return std::nullopt;

        std::optional<AtomString> language;
        decoder >> language;
        if (!language)
            return std::nullopt;

        std::optional<MediaTime> startTimeVariance;
        decoder >> startTimeVariance;
        if (!startTimeVariance)
            return std::nullopt;

        std::optional<int> trackIndex;
        decoder >> trackIndex;
        if (!trackIndex)
            return std::nullopt;

        std::optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return std::nullopt;

        std::optional<WebCore::AudioTrackPrivate::Kind> audioKind;
        decoder >> audioKind;
        if (!audioKind)
            return std::nullopt;

        std::optional<bool> selected;
        decoder >> selected;
        if (!selected)
            return std::nullopt;

        std::optional<WebCore::VideoTrackPrivate::Kind> videoKind;
        decoder >> videoKind;
        if (!videoKind)
            return std::nullopt;

        return {{
            WTFMove(*trackId),
            WTFMove(*label),
            WTFMove(*language),
            WTFMove(*startTimeVariance),
            *trackIndex,
            *enabled,
            *audioKind,
            *selected,
            *videoKind,
        }};
    }
};

} // namespace WebKit

#endif
