/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include "AccessibilityMediaHelpers.h"

#include "AccessibilityNodeObjectInlines.h"
#include "AXObjectCache.h"
#include "AccessibilityRenderObject.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "LocalizedStrings.h"
#include "MediaElementSession.h"
#include "UserGestureIndicator.h"

namespace WebCore {

void AccessibilityMediaHelpers::enterFullscreen(HTMLVideoElement* videoElement)
{
    if (videoElement)
        videoElement->enterFullscreen();
}

void AccessibilityMediaHelpers::toggleMute(HTMLMediaElement* mediaElement)
{
    if (mediaElement)
        mediaElement->setMuted(!mediaElement->muted());
}

String AccessibilityMediaHelpers::interactiveVideoDuration(HTMLMediaElement* mediaElement)
{
    return mediaElement ? localizedMediaTimeDescription(mediaElement->duration()) : String();
}

bool AccessibilityMediaHelpers::isPlaying(HTMLMediaElement* mediaElement)
{
    return mediaElement && mediaElement->isPlaying();
}

bool AccessibilityMediaHelpers::isAutoplayEnabled(HTMLMediaElement* mediaElement)
{
    return mediaElement && mediaElement->autoplay();
}

bool AccessibilityMediaHelpers::isMuted(HTMLMediaElement* mediaElement)
{
    return mediaElement && mediaElement->muted();
}

bool AccessibilityMediaHelpers::press(HTMLMediaElement& mediaElement)
{
    // We can safely call the internal togglePlayState method, which doesn't check restrictions,
    // because this method is only called from user interaction.
    mediaElement.togglePlayState();
    return true;
}

void AccessibilityMediaHelpers::increment(HTMLMediaElement& mediaElement)
{
    mediaSeek(mediaElement, AXSeekDirection::Forward);
}

void AccessibilityMediaHelpers::decrement(HTMLMediaElement& mediaElement)
{
    mediaSeek(mediaElement, AXSeekDirection::Backward);
}

void AccessibilityMediaHelpers::mediaSeek(HTMLMediaElement& mediaElement, AXSeekDirection direction)
{
    // Step 5% each time.
    constexpr double seekStep = .05;
    double current = mediaElement.currentTime();
    double duration = mediaElement.duration();
    double timeDelta = ceil(duration * seekStep);

    double time = direction == AXSeekDirection::Forward ? std::min(current + timeDelta, duration) : std::max(current - timeDelta, 0.0);
    mediaElement.setCurrentTime(time);
}

#if PLATFORM(MAC)
// Matches the skip interval HTMLMediaElement uses for the skip media key.
static constexpr double skipInterval = 15;

Vector<std::pair<String, AccessibilityMediaHelpers::Action>> AccessibilityMediaHelpers::availableActions(HTMLMediaElement& mediaElement)
{
    Vector<std::pair<String, Action>> actions;
    actions.append({ mediaElement.paused() ? AXMediaPlayActionText() : AXMediaPauseActionText(), Action::TogglePlayPause });

    if (mediaElement.supportsSeeking() && std::isfinite(mediaElement.duration())) {
        actions.append({ AXMediaSkipBackActionText(), Action::SkipBackward });
        actions.append({ AXMediaSkipForwardActionText(), Action::SkipForward });
    }

    if (mediaElement.hasAudio())
        actions.append({ mediaElement.muted() ? AXMediaUnmuteActionText() : AXMediaMuteActionText(), Action::ToggleMute });

    if (mediaElement.hasClosedCaptions())
        actions.append({ mediaElement.closedCaptionsVisible() ? AXMediaHideCaptionsActionText() : AXMediaShowCaptionsActionText(), Action::ToggleCaptions });

    if (mediaElement.isVideo() && mediaElement.supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        actions.append({ mediaElement.isStandardFullscreen() ? AXMediaExitFullScreenActionText() : AXMediaEnterFullScreenActionText(), Action::ToggleFullscreen });

    return actions;
}

bool AccessibilityMediaHelpers::perform(HTMLMediaElement& mediaElement, Action action)
{
    // Play, pause and skip go through the media session, exactly like the keyboard media keys or
    // Control Center. That honors any navigator.mediaSession action handlers the page registered,
    // so custom players see the same input they'd get from those controls.
    auto sendRemoteCommand = [&](PlatformMediaSession::RemoteControlCommandType command, PlatformMediaSession::RemoteCommandArgument argument = { }) {
        PlatformMediaSession& session = mediaElement.mediaSession();
        session.didReceiveRemoteControlCommand(command, argument);
        return true;
    };

    switch (action) {
    case Action::TogglePlayPause:
        return sendRemoteCommand(PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand);
    case Action::SkipBackward:
        return sendRemoteCommand(PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand, { skipInterval, std::nullopt });
    case Action::SkipForward:
        return sendRemoteCommand(PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand, { skipInterval, std::nullopt });
    case Action::ToggleMute:
        // Same as the media context menu's Mute item.
        mediaElement.setMuted(!mediaElement.muted());
        return true;
    case Action::ToggleCaptions:
        if (!mediaElement.hasClosedCaptions())
            return false;
        mediaElement.setClosedCaptionsVisible(!mediaElement.closedCaptionsVisible());
        return true;
    case Action::ToggleFullscreen: {
        // Same as the media context menu's Enter/Exit Full Screen item.
        if (!mediaElement.isVideo() || !mediaElement.supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard))
            return false;
        UserGestureIndicator indicator(IsProcessingUserGesture::Yes, protect(mediaElement.document()).ptr());
        mediaElement.toggleStandardFullscreenState();
        return true;
    }
    }
    return false;
}
#endif // PLATFORM(MAC)

} // namespace WebCore

#endif // PLATFORM(COCOA) && ENABLE(VIDEO)
