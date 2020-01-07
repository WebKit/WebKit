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

#include <WebCore/FloatSize.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformTimeRanges.h>
#include <wtf/MediaTime.h>

namespace WebKit {

struct RemoteMediaPlayerState {
    MediaTime currentTime;
    MediaTime duration;
    MediaTime minTimeSeekable;
    MediaTime maxTimeSeekable;
    WebCore::PlatformTimeRanges bufferedRanges;
    WebCore::MediaPlayerEnums::NetworkState networkState { WebCore::MediaPlayerEnums::NetworkState::Empty };
    WebCore::MediaPlayerEnums::ReadyState readyState { WebCore::MediaPlayerEnums::ReadyState::HaveNothing };
    WebCore::FloatSize naturalSize;
    bool paused { true };
    bool loadingProgressed { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << currentTime;
        encoder << duration;
        encoder << minTimeSeekable;
        encoder << maxTimeSeekable;
        encoder << bufferedRanges;
        encoder.encodeEnum(networkState);
        encoder.encodeEnum(readyState);
        encoder << naturalSize;
        encoder << paused;
        encoder << loadingProgressed;
    }

    template <class Decoder>
    static Optional<RemoteMediaPlayerState> decode(Decoder& decoder)
    {
        Optional<MediaTime> currentTime;
        decoder >> currentTime;
        if (!currentTime)
            return WTF::nullopt;

        Optional<MediaTime> duration;
        decoder >> duration;
        if (!duration)
            return WTF::nullopt;

        Optional<MediaTime> minTimeSeekable;
        decoder >> minTimeSeekable;
        if (!minTimeSeekable)
            return WTF::nullopt;

        Optional<MediaTime> maxTimeSeekable;
        decoder >> maxTimeSeekable;
        if (!maxTimeSeekable)
            return WTF::nullopt;

        Optional<WebCore::PlatformTimeRanges> bufferedRanges;
        decoder >> bufferedRanges;
        if (!bufferedRanges)
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::NetworkState networkState;
        if (!decoder.decodeEnum(networkState))
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::ReadyState readyState;
        if (!decoder.decodeEnum(readyState))
            return WTF::nullopt;

        Optional<WebCore::FloatSize> naturalSize;
        decoder >> naturalSize;
        if (!naturalSize)
            return WTF::nullopt;

        Optional<bool> paused;
        decoder >> paused;
        if (!paused)
            return WTF::nullopt;

        Optional<bool> loadingProgressed;
        decoder >> loadingProgressed;
        if (!loadingProgressed)
            return WTF::nullopt;

        return {{
            WTFMove(*currentTime),
            WTFMove(*duration),
            WTFMove(*minTimeSeekable),
            WTFMove(*maxTimeSeekable),
            WTFMove(*bufferedRanges),
            networkState,
            readyState,
            WTFMove(*naturalSize),
            *paused,
            *loadingProgressed }};
    }

};

} // namespace WebKit

#endif
