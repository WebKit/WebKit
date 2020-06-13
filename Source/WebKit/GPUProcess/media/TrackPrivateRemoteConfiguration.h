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
    AtomString id;
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
        encoder << id;
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
    static Optional<TrackPrivateRemoteConfiguration> decode(Decoder& decoder)
    {
        Optional<AtomString> id;
        decoder >> id;
        if (!id)
            return WTF::nullopt;

        Optional<AtomString> label;
        decoder >> label;
        if (!label)
            return WTF::nullopt;

        Optional<AtomString> language;
        decoder >> language;
        if (!language)
            return WTF::nullopt;

        Optional<MediaTime> startTimeVariance;
        decoder >> startTimeVariance;
        if (!startTimeVariance)
            return WTF::nullopt;

        Optional<int> trackIndex;
        decoder >> trackIndex;
        if (!trackIndex)
            return WTF::nullopt;

        Optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return WTF::nullopt;

        Optional<WebCore::AudioTrackPrivate::Kind> audioKind;
        decoder >> audioKind;
        if (!audioKind)
            return WTF::nullopt;

        Optional<bool> selected;
        decoder >> selected;
        if (!selected)
            return WTF::nullopt;

        Optional<WebCore::VideoTrackPrivate::Kind> videoKind;
        decoder >> videoKind;
        if (!videoKind)
            return WTF::nullopt;

        return {{
            WTFMove(*id),
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
