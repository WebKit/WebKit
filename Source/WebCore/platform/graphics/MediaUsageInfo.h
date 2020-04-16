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

#include "MediaSessionIdentifier.h"
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MediaUsageInfo {
    URL mediaURL;
    bool isPlaying { false };
    bool canShowControlsManager { false };
    bool canShowNowPlayingControls { false };
    bool isSuspended { false };
    bool isInActiveDocument { false };
    bool isFullscreen { false };
    bool isMuted { false };
    bool isMediaDocumentInMainFrame { false };
    bool isVideo { false };
    bool isAudio { false };
    bool hasVideo { false };
    bool hasAudio { false };
    bool hasRenderer { false };
    bool audioElementWithUserGesture { false };
    bool userHasPlayedAudioBefore { false };
    bool isElementRectMostlyInMainFrame { false };
    bool playbackPermitted { false };
    bool pageMediaPlaybackSuspended { false };
    bool isMediaDocumentAndNotOwnerElement { false };
    bool pageExplicitlyAllowsElementToAutoplayInline { false };
    bool requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted { false };
    bool hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture { false };
    bool isVideoAndRequiresUserGestureForVideoRateChange { false };
    bool isAudioAndRequiresUserGestureForAudioRateChange { false };
    bool isVideoAndRequiresUserGestureForVideoDueToLowPowerMode { false };
    bool noUserGestureRequired { false };
    bool requiresPlaybackAndIsNotPlaying { false };
    bool hasEverNotifiedAboutPlaying { false };
    bool outsideOfFullscreen { false };
    bool isLargeEnoughForMainContent { false };

    bool operator==(const MediaUsageInfo& other) const
    {
        return mediaURL == other.mediaURL
            && isPlaying == other.isPlaying
            && canShowControlsManager == other.canShowControlsManager
            && canShowNowPlayingControls == other.canShowNowPlayingControls
            && isSuspended == other.isSuspended
            && isInActiveDocument == other.isInActiveDocument
            && isFullscreen == other.isFullscreen
            && isMuted == other.isMuted
            && isMediaDocumentInMainFrame == other.isMediaDocumentInMainFrame
            && isVideo == other.isVideo
            && isAudio == other.isAudio
            && hasAudio == other.hasAudio
            && hasVideo == other.hasVideo
            && hasRenderer == other.hasRenderer
            && audioElementWithUserGesture == other.audioElementWithUserGesture
            && userHasPlayedAudioBefore == other.userHasPlayedAudioBefore
            && isElementRectMostlyInMainFrame == other.isElementRectMostlyInMainFrame
            && playbackPermitted == other.playbackPermitted
            && pageMediaPlaybackSuspended == other.pageMediaPlaybackSuspended
            && isMediaDocumentAndNotOwnerElement == other.isMediaDocumentAndNotOwnerElement
            && pageExplicitlyAllowsElementToAutoplayInline == other.pageExplicitlyAllowsElementToAutoplayInline
            && requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted == other.requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted
            && hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture == other.hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture
            && isVideoAndRequiresUserGestureForVideoRateChange == other.isVideoAndRequiresUserGestureForVideoRateChange
            && isAudioAndRequiresUserGestureForAudioRateChange == other.isAudioAndRequiresUserGestureForAudioRateChange
            && isVideoAndRequiresUserGestureForVideoDueToLowPowerMode == other.isVideoAndRequiresUserGestureForVideoDueToLowPowerMode
            && noUserGestureRequired == other.noUserGestureRequired
            && requiresPlaybackAndIsNotPlaying == other.requiresPlaybackAndIsNotPlaying
            && hasEverNotifiedAboutPlaying == other.hasEverNotifiedAboutPlaying
            && outsideOfFullscreen == other.outsideOfFullscreen
            && isLargeEnoughForMainContent == other.isLargeEnoughForMainContent;
    }

    bool operator!=(const MediaUsageInfo other) const
    {
        return !(*this == other);
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<MediaUsageInfo> decode(Decoder&);
};

template<class Encoder> inline void MediaUsageInfo::encode(Encoder& encoder) const
{
    encoder << mediaURL;
    encoder << isPlaying;
    encoder << canShowControlsManager;
    encoder << canShowNowPlayingControls;
    encoder << isSuspended;
    encoder << isInActiveDocument;
    encoder << isFullscreen;
    encoder << isMuted;
    encoder << isMediaDocumentInMainFrame;
    encoder << isVideo;
    encoder << isAudio;
    encoder << hasVideo;
    encoder << hasAudio;
    encoder << hasRenderer;
    encoder << audioElementWithUserGesture;
    encoder << userHasPlayedAudioBefore;
    encoder << isElementRectMostlyInMainFrame;
    encoder << playbackPermitted;
    encoder << pageMediaPlaybackSuspended;
    encoder << isMediaDocumentAndNotOwnerElement;
    encoder << pageExplicitlyAllowsElementToAutoplayInline;
    encoder << requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted;
    encoder << hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture;
    encoder << isVideoAndRequiresUserGestureForVideoRateChange;
    encoder << isAudioAndRequiresUserGestureForAudioRateChange;
    encoder << isVideoAndRequiresUserGestureForVideoDueToLowPowerMode;
    encoder << noUserGestureRequired;
    encoder << requiresPlaybackAndIsNotPlaying;
    encoder << hasEverNotifiedAboutPlaying;
    encoder << outsideOfFullscreen;
    encoder << isLargeEnoughForMainContent;
}

template<class Decoder> inline Optional<MediaUsageInfo> MediaUsageInfo::decode(Decoder& decoder)
{
    MediaUsageInfo info;

    if (!decoder.decode(info.mediaURL))
        return { };

    if (!decoder.decode(info.isPlaying))
        return { };

    if (!decoder.decode(info.canShowControlsManager))
        return { };

    if (!decoder.decode(info.canShowNowPlayingControls))
        return { };

    if (!decoder.decode(info.isSuspended))
        return { };

    if (!decoder.decode(info.isInActiveDocument))
        return { };

    if (!decoder.decode(info.isFullscreen))
        return { };

    if (!decoder.decode(info.isMuted))
        return { };

    if (!decoder.decode(info.isMediaDocumentInMainFrame))
        return { };

    if (!decoder.decode(info.isVideo))
        return { };

    if (!decoder.decode(info.isAudio))
        return { };

    if (!decoder.decode(info.hasVideo))
        return { };

    if (!decoder.decode(info.hasAudio))
        return { };

    if (!decoder.decode(info.hasRenderer))
        return { };

    if (!decoder.decode(info.audioElementWithUserGesture))
        return { };

    if (!decoder.decode(info.userHasPlayedAudioBefore))
        return { };

    if (!decoder.decode(info.isElementRectMostlyInMainFrame))
        return { };

    if (!decoder.decode(info.playbackPermitted))
        return { };

    if (!decoder.decode(info.pageMediaPlaybackSuspended))
        return { };

    if (!decoder.decode(info.isMediaDocumentAndNotOwnerElement))
        return { };

    if (!decoder.decode(info.pageExplicitlyAllowsElementToAutoplayInline))
        return { };

    if (!decoder.decode(info.requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted))
        return { };

    if (!decoder.decode(info.hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture))
        return { };

    if (!decoder.decode(info.isVideoAndRequiresUserGestureForVideoRateChange))
        return { };

    if (!decoder.decode(info.isAudioAndRequiresUserGestureForAudioRateChange))
        return { };

    if (!decoder.decode(info.isVideoAndRequiresUserGestureForVideoDueToLowPowerMode))
        return { };

    if (!decoder.decode(info.noUserGestureRequired))
        return { };

    if (!decoder.decode(info.requiresPlaybackAndIsNotPlaying))
        return { };

    if (!decoder.decode(info.hasEverNotifiedAboutPlaying))
        return { };

    if (!decoder.decode(info.outsideOfFullscreen))
        return { };

    if (!decoder.decode(info.isLargeEnoughForMainContent))
        return { };

    return info;
}

} // namespace WebCore
