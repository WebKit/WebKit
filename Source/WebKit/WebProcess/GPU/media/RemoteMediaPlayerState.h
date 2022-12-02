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
};

} // namespace WebKit

#endif
