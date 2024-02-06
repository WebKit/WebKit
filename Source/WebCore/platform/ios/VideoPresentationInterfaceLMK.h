/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(VISION)

#include "PlaybackSessionInterfaceLMK.h"
#include "VideoPresentationInterfaceIOS.h"

namespace WebCore {

class VideoPresentationInterfaceLMK : public VideoPresentationInterfaceIOS {

public:
    WEBCORE_EXPORT static Ref<VideoPresentationInterfaceLMK> create(PlaybackSessionInterfaceLMK&);
#if !RELEASE_LOG_DISABLED
    const char* logClassName() const { return "VideoPresentationInterfaceLMK"; };
#endif
    WEBCORE_EXPORT ~VideoPresentationInterfaceLMK();
    WEBCORE_EXPORT void videoDimensionsChanged(const FloatSize&) final;
    WEBCORE_EXPORT void setupFullscreen(UIView&, const FloatRect&, const FloatSize&, UIView*, HTMLMediaElementEnums::VideoFullscreenMode, bool, bool, bool);
    WebAVPlayerLayerView *playerLayerView() const;
    WEBCORE_EXPORT void setVideoPresentationModel(VideoPresentationModel*);
    WEBCORE_EXPORT bool exitFullscreen(const FloatRect&);
    WEBCORE_EXPORT bool pictureInPictureWasStartedWhenEnteringBackground() const;
    WEBCORE_EXPORT AVPlayerViewController *avPlayerViewController() const;

    WEBCORE_EXPORT void hasVideoChanged(bool);
    WEBCORE_EXPORT void externalPlaybackChanged(bool, PlaybackSessionModel::ExternalPlaybackTargetType, const String&);
    WEBCORE_EXPORT void cleanupFullscreen();
    WEBCORE_EXPORT void setPlayerIdentifier(std::optional<MediaPlayerIdentifier>) final;
    WEBCORE_EXPORT void requestHideAndExitFullscreen();
    WEBCORE_EXPORT void preparedToReturnToInline(bool, const FloatRect&);
    WEBCORE_EXPORT bool mayAutomaticallyShowVideoPictureInPicture() const;
    void willStartPictureInPicture();
    void didStartPictureInPicture();
    void failedToStartPictureInPicture();
    void didStopPictureInPicture();
    void prepareForPictureInPictureStopWithCompletionHandler(void (^)(BOOL));
    bool shouldExitFullscreenWithReason(ExitFullScreenReason);
    WEBCORE_EXPORT void setInlineRect(const FloatRect&, bool);
    bool isPlayingVideoInEnhancedFullscreen() const;
private:
    WEBCORE_EXPORT VideoPresentationInterfaceLMK(PlaybackSessionInterfaceLMK&);
    void doSetup();
    void doEnterFullscreen();
    void doExitFullscreen();
    void exitFullscreenHandler(BOOL, NSError *, NextActions = NextActions());
    void enterFullscreenHandler(BOOL, NSError *, NextActions = NextActions());
    void watchdogTimerFired();

};
} // namespace WebCore

#endif // PLATFORM(VISION)
