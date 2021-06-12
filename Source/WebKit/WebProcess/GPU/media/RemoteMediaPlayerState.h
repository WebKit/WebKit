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
#include <WebCore/VideoPlaybackQualityMetrics.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>

namespace WebKit {

struct RemoteMediaPlayerState {
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
    std::optional<WebCore::VideoPlaybackQualityMetrics> videoMetrics;
    std::optional<bool> wouldTaintDocumentSecurityOrigin { true };
    bool paused { true };
    bool canSaveMediaData { false };
    bool hasAudio { false };
    bool hasVideo { false };
    bool hasClosedCaptions { false };
    bool hasAvailableVideoFrame { false };
    bool wirelessVideoPlaybackDisabled { false };
    bool hasSingleSecurityOrigin { false };
    bool didPassCORSAccessCheck { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
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
        encoder << videoMetrics;
        encoder << wouldTaintDocumentSecurityOrigin;
        encoder << paused;
        encoder << canSaveMediaData;
        encoder << hasAudio;
        encoder << hasVideo;
        encoder << hasClosedCaptions;
        encoder << hasAvailableVideoFrame;
        encoder << wirelessVideoPlaybackDisabled;
        encoder << hasSingleSecurityOrigin;
        encoder << didPassCORSAccessCheck;
    }

    template <class Decoder>
    static std::optional<RemoteMediaPlayerState> decode(Decoder& decoder)
    {
        std::optional<MediaTime> duration;
        decoder >> duration;
        if (!duration)
            return std::nullopt;

        std::optional<MediaTime> minTimeSeekable;
        decoder >> minTimeSeekable;
        if (!minTimeSeekable)
            return std::nullopt;

        std::optional<MediaTime> maxTimeSeekable;
        decoder >> maxTimeSeekable;
        if (!maxTimeSeekable)
            return std::nullopt;

        std::optional<MediaTime> startDate;
        decoder >> startDate;
        if (!startDate)
            return std::nullopt;

        std::optional<MediaTime> startTime;
        decoder >> startTime;
        if (!startTime)
            return std::nullopt;

        std::optional<String> languageOfPrimaryAudioTrack;
        decoder >> languageOfPrimaryAudioTrack;
        if (!languageOfPrimaryAudioTrack)
            return std::nullopt;

        std::optional<String> wirelessPlaybackTargetName;
        decoder >> wirelessPlaybackTargetName;
        if (!wirelessPlaybackTargetName)
            return std::nullopt;

        std::optional<WebCore::PlatformTimeRanges> bufferedRanges;
        decoder >> bufferedRanges;
        if (!bufferedRanges)
            return std::nullopt;

        WebCore::MediaPlayerEnums::NetworkState networkState;
        if (!decoder.decode(networkState))
            return std::nullopt;

        WebCore::MediaPlayerEnums::ReadyState readyState;
        if (!decoder.decode(readyState))
            return std::nullopt;

        WebCore::MediaPlayerEnums::MovieLoadType movieLoadType;
        if (!decoder.decode(movieLoadType))
            return std::nullopt;

        WebCore::MediaPlayerEnums::WirelessPlaybackTargetType wirelessPlaybackTargetType;
        if (!decoder.decode(wirelessPlaybackTargetType))
            return std::nullopt;

        std::optional<WebCore::FloatSize> naturalSize;
        decoder >> naturalSize;
        if (!naturalSize)
            return std::nullopt;

        std::optional<double> maxFastForwardRate;
        decoder >> maxFastForwardRate;
        if (!maxFastForwardRate)
            return std::nullopt;

        std::optional<double> minFastReverseRate;
        decoder >> minFastReverseRate;
        if (!minFastReverseRate)
            return std::nullopt;

        std::optional<double> seekableTimeRangesLastModifiedTime;
        decoder >> seekableTimeRangesLastModifiedTime;
        if (!seekableTimeRangesLastModifiedTime)
            return std::nullopt;

        std::optional<double> liveUpdateInterval;
        decoder >> liveUpdateInterval;
        if (!liveUpdateInterval)
            return std::nullopt;

        std::optional<uint64_t> totalBytes;
        decoder >> totalBytes;
        if (!totalBytes)
            return std::nullopt;

        std::optional<std::optional<WebCore::VideoPlaybackQualityMetrics>> videoMetrics;
        decoder >> videoMetrics;
        if (!videoMetrics)
            return std::nullopt;

        std::optional<std::optional<bool>> wouldTaintDocumentSecurityOrigin;
        decoder >> wouldTaintDocumentSecurityOrigin;
        if (!wouldTaintDocumentSecurityOrigin)
            return std::nullopt;

        std::optional<bool> paused;
        decoder >> paused;
        if (!paused)
            return std::nullopt;

        std::optional<bool> canSaveMediaData;
        decoder >> canSaveMediaData;
        if (!canSaveMediaData)
            return std::nullopt;

        std::optional<bool> hasAudio;
        decoder >> hasAudio;
        if (!hasAudio)
            return std::nullopt;

        std::optional<bool> hasVideo;
        decoder >> hasVideo;
        if (!hasVideo)
            return std::nullopt;

        std::optional<bool> hasClosedCaptions;
        decoder >> hasClosedCaptions;
        if (!hasClosedCaptions)
            return std::nullopt;

        std::optional<bool> hasAvailableVideoFrame;
        decoder >> hasAvailableVideoFrame;
        if (!hasAvailableVideoFrame)
            return std::nullopt;

        std::optional<bool> wirelessVideoPlaybackDisabled;
        decoder >> wirelessVideoPlaybackDisabled;
        if (!wirelessVideoPlaybackDisabled)
            return std::nullopt;

        std::optional<bool> hasSingleSecurityOrigin;
        decoder >> hasSingleSecurityOrigin;
        if (!hasSingleSecurityOrigin)
            return std::nullopt;

        std::optional<bool> didPassCORSAccessCheck;
        decoder >> didPassCORSAccessCheck;
        if (!didPassCORSAccessCheck)
            return std::nullopt;

        return {{
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
            WTFMove(*videoMetrics),
            WTFMove(*wouldTaintDocumentSecurityOrigin),
            *paused,
            *canSaveMediaData,
            *hasAudio,
            *hasVideo,
            *hasClosedCaptions,
            *hasAvailableVideoFrame,
            *wirelessVideoPlaybackDisabled,
            *hasSingleSecurityOrigin,
            *didPassCORSAccessCheck
        }};
    }

};

} // namespace WebKit

#endif
