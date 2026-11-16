/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include <WebCore/MediaSessionGroupIdentifier.h>
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/NowPlayingInfo.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PlatformMediaSessionTypes.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>

namespace WebKit {

struct RemoteMediaSessionState {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RemoteMediaSessionState);

    WebCore::PageIdentifier pageIdentifier;
    WebCore::MediaSessionIdentifier sessionIdentifier;
    uint64_t logIdentifier { 0 };

    WebCore::PlatformMediaSessionMediaType mediaType { WebCore::PlatformMediaSessionMediaType::None };
    WebCore::PlatformMediaSessionMediaType presentationType { WebCore::PlatformMediaSessionMediaType::None };
    WebCore::PlatformMediaSessionDisplayType displayType { WebCore::PlatformMediaSessionDisplayType::Normal };

    WebCore::PlatformMediaSessionState state { WebCore::PlatformMediaSessionState::Idle };
    WebCore::PlatformMediaSessionState stateToRestore { WebCore::PlatformMediaSessionState::Idle };
    WebCore::PlatformMediaSessionInterruptionType interruptionType { WebCore::PlatformMediaSessionInterruptionType::NoInterruption };

    MediaTime duration { MediaTime::invalidTime() };

    std::optional<WebCore::MediaSessionGroupIdentifier> groupIdentifier;
    std::optional<WebCore::NowPlayingInfo> nowPlayingInfo;

    bool shouldOverrideBackgroundLoadingRestriction { false };
    bool isPlayingToWirelessPlaybackTarget { false };
    bool isPlayingOnSecondScreen { false };
    bool hasMediaStreamSource { false };
    bool shouldOverridePauseDuringRouteChange { false };
    bool isNowPlayingEligible { false };
    bool canProduceAudio { false };
    bool isSuspended { false };
    bool isPlaying { false };
    bool isAudible { false };
    bool isEnded { false };
    bool canReceiveRemoteControlCommands { false };
    bool supportsSeeking { false };
    bool hasPlayedAudiblySinceLastInterruption { false };
    bool isLongEnoughForMainContent { false };
    bool blockedBySystemInterruption { false };
    bool activeAudioSessionRequired { false };
    bool preparingToPlay { false };
    bool isActiveNowPlayingSession { false };
#if PLATFORM(IOS_FAMILY)
    bool requiresPlaybackTargetRouteMonitoring { false };
#endif
};


} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
