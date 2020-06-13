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
    MediaTime startDate;
    MediaTime startTime;
    String languageOfPrimaryAudioTrack;
    String wirelessPlaybackTargetName;
    WebCore::PlatformTimeRanges bufferedRanges;
    WebCore::MediaPlayerEnums::NetworkState networkState { WebCore::MediaPlayerEnums::NetworkState::Empty };
    WebCore::MediaPlayerEnums::ReadyState readyState { WebCore::MediaPlayerEnums::ReadyState::HaveNothing };
    WebCore::MediaPlayerEnums::MovieLoadType movieLoadType { WebCore::MediaPlayerEnums::MovieLoadType::Unknown };
    WebCore::MediaPlayerEnums::WirelessPlaybackTargetType wirelessPlaybackTargetType { WebCore::MediaPlayerEnums::WirelessPlaybackTargetType::TargetTypeNone };
    WebCore::FloatSize naturalSize;
    double maxFastForwardRate { 0 };
    double minFastReverseRate { 0 };
    double seekableTimeRangesLastModifiedTime { 0 };
    double liveUpdateInterval { 0 };
    uint64_t totalBytes { 0 };
    Optional<bool> wouldTaintDocumentSecurityOrigin { true };
    bool paused { true };
    bool loadingProgressed { false };
    bool canSaveMediaData { false };
    bool hasAudio { false };
    bool hasVideo { false };
    bool hasClosedCaptions { false };
    bool hasAvailableVideoFrame { false };
    bool wirelessVideoPlaybackDisabled { false };
    bool hasSingleSecurityOrigin { false };
    bool didPassCORSAccessCheck { false };
    bool requiresTextTrackRepresentation { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << currentTime;
        encoder << duration;
        encoder << minTimeSeekable;
        encoder << maxTimeSeekable;
        encoder << startDate;
        encoder << startTime;
        encoder << languageOfPrimaryAudioTrack;
        encoder << wirelessPlaybackTargetName;
        encoder << bufferedRanges;
        encoder << networkState;
        encoder << readyState;
        encoder << movieLoadType;
        encoder << wirelessPlaybackTargetType;
        encoder << naturalSize;
        encoder << maxFastForwardRate;
        encoder << minFastReverseRate;
        encoder << seekableTimeRangesLastModifiedTime;
        encoder << liveUpdateInterval;
        encoder << totalBytes;
        encoder << wouldTaintDocumentSecurityOrigin;
        encoder << paused;
        encoder << loadingProgressed;
        encoder << canSaveMediaData;
        encoder << hasAudio;
        encoder << hasVideo;
        encoder << hasClosedCaptions;
        encoder << hasAvailableVideoFrame;
        encoder << wirelessVideoPlaybackDisabled;
        encoder << hasSingleSecurityOrigin;
        encoder << didPassCORSAccessCheck;
        encoder << requiresTextTrackRepresentation;
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

        Optional<MediaTime> startDate;
        decoder >> startDate;
        if (!startDate)
            return WTF::nullopt;

        Optional<MediaTime> startTime;
        decoder >> startTime;
        if (!startTime)
            return WTF::nullopt;

        Optional<String> languageOfPrimaryAudioTrack;
        decoder >> languageOfPrimaryAudioTrack;
        if (!languageOfPrimaryAudioTrack)
            return WTF::nullopt;

        Optional<String> wirelessPlaybackTargetName;
        decoder >> wirelessPlaybackTargetName;
        if (!wirelessPlaybackTargetName)
            return WTF::nullopt;

        Optional<WebCore::PlatformTimeRanges> bufferedRanges;
        decoder >> bufferedRanges;
        if (!bufferedRanges)
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::NetworkState networkState;
        if (!decoder.decode(networkState))
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::ReadyState readyState;
        if (!decoder.decode(readyState))
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::MovieLoadType movieLoadType;
        if (!decoder.decode(movieLoadType))
            return WTF::nullopt;

        WebCore::MediaPlayerEnums::WirelessPlaybackTargetType wirelessPlaybackTargetType;
        if (!decoder.decode(wirelessPlaybackTargetType))
            return WTF::nullopt;

        Optional<WebCore::FloatSize> naturalSize;
        decoder >> naturalSize;
        if (!naturalSize)
            return WTF::nullopt;

        Optional<double> maxFastForwardRate;
        decoder >> maxFastForwardRate;
        if (!maxFastForwardRate)
            return WTF::nullopt;

        Optional<double> minFastReverseRate;
        decoder >> minFastReverseRate;
        if (!minFastReverseRate)
            return WTF::nullopt;

        Optional<double> seekableTimeRangesLastModifiedTime;
        decoder >> seekableTimeRangesLastModifiedTime;
        if (!seekableTimeRangesLastModifiedTime)
            return WTF::nullopt;

        Optional<double> liveUpdateInterval;
        decoder >> liveUpdateInterval;
        if (!liveUpdateInterval)
            return WTF::nullopt;

        Optional<uint64_t> totalBytes;
        decoder >> totalBytes;
        if (!totalBytes)
            return WTF::nullopt;

        Optional<Optional<bool>> wouldTaintDocumentSecurityOrigin;
        decoder >> wouldTaintDocumentSecurityOrigin;
        if (!wouldTaintDocumentSecurityOrigin)
            return WTF::nullopt;

        Optional<bool> paused;
        decoder >> paused;
        if (!paused)
            return WTF::nullopt;

        Optional<bool> loadingProgressed;
        decoder >> loadingProgressed;
        if (!loadingProgressed)
            return WTF::nullopt;

        Optional<bool> canSaveMediaData;
        decoder >> canSaveMediaData;
        if (!canSaveMediaData)
            return WTF::nullopt;

        Optional<bool> hasAudio;
        decoder >> hasAudio;
        if (!hasAudio)
            return WTF::nullopt;

        Optional<bool> hasVideo;
        decoder >> hasVideo;
        if (!hasVideo)
            return WTF::nullopt;

        Optional<bool> hasClosedCaptions;
        decoder >> hasClosedCaptions;
        if (!hasClosedCaptions)
            return WTF::nullopt;

        Optional<bool> hasAvailableVideoFrame;
        decoder >> hasAvailableVideoFrame;
        if (!hasAvailableVideoFrame)
            return WTF::nullopt;

        Optional<bool> wirelessVideoPlaybackDisabled;
        decoder >> wirelessVideoPlaybackDisabled;
        if (!wirelessVideoPlaybackDisabled)
            return WTF::nullopt;

        Optional<bool> hasSingleSecurityOrigin;
        decoder >> hasSingleSecurityOrigin;
        if (!hasSingleSecurityOrigin)
            return WTF::nullopt;

        Optional<bool> didPassCORSAccessCheck;
        decoder >> didPassCORSAccessCheck;
        if (!didPassCORSAccessCheck)
            return WTF::nullopt;

        Optional<bool> requiresTextTrackRepresentation;
        decoder >> requiresTextTrackRepresentation;
        if (!requiresTextTrackRepresentation)
            return WTF::nullopt;

        return {{
            WTFMove(*currentTime),
            WTFMove(*duration),
            WTFMove(*minTimeSeekable),
            WTFMove(*maxTimeSeekable),
            WTFMove(*startDate),
            WTFMove(*startTime),
            WTFMove(*languageOfPrimaryAudioTrack),
            WTFMove(*wirelessPlaybackTargetName),
            WTFMove(*bufferedRanges),
            networkState,
            readyState,
            movieLoadType,
            wirelessPlaybackTargetType,
            WTFMove(*naturalSize),
            *maxFastForwardRate,
            *minFastReverseRate,
            *seekableTimeRangesLastModifiedTime,
            *liveUpdateInterval,
            *totalBytes,
            WTFMove(*wouldTaintDocumentSecurityOrigin),
            *paused,
            *loadingProgressed,
            *canSaveMediaData,
            *hasAudio,
            *hasVideo,
            *hasClosedCaptions,
            *hasAvailableVideoFrame,
            *wirelessVideoPlaybackDisabled,
            *hasSingleSecurityOrigin,
            *didPassCORSAccessCheck,
            *requiresTextTrackRepresentation,
        }};
    }

};

} // namespace WebKit

#endif
