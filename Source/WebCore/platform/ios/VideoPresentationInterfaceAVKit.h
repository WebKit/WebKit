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

#if PLATFORM(IOS_FAMILY) && HAVE(AVKIT)

#include "VideoPresentationInterfaceIOS.h"

OBJC_CLASS AVPlayerViewController;
OBJC_CLASS WebAVPlayerController;
OBJC_CLASS WebAVPlayerLayerView;
OBJC_CLASS WebAVPlayerLayer;
OBJC_CLASS WebAVPlayerViewController;
OBJC_CLASS WebAVPlayerViewControllerDelegate;

namespace WebCore {

class PlaybackSessionInterfaceAVKit;

class VideoPresentationInterfaceAVKit : public VideoPresentationInterfaceIOS {

public:
    WEBCORE_EXPORT static Ref<VideoPresentationInterfaceAVKit> create(PlaybackSessionInterfaceAVKit&);
    WEBCORE_EXPORT ~VideoPresentationInterfaceAVKit();
    WEBCORE_EXPORT AVPlayerViewController *avPlayerViewController() const;
    WEBCORE_EXPORT bool exitFullscreen(const FloatRect& finalRect);
    WEBCORE_EXPORT void externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName);

    WEBCORE_EXPORT void hasVideoChanged(bool) final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const { return "VideoPresentationInterfaceAVKit"; };
#endif

    WEBCORE_EXPORT void videoDimensionsChanged(const FloatSize&) final;

    WEBCORE_EXPORT void setupFullscreen(UIView& videoView, const FloatRect& initialRect, const FloatSize& videoDimensions, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture);

    WebAVPlayerLayerView *playerLayerView() const { return m_playerLayerView.get(); }
    WEBCORE_EXPORT void setVideoPresentationModel(VideoPresentationModel*);
    WEBCORE_EXPORT bool pictureInPictureWasStartedWhenEnteringBackground() const;
    WEBCORE_EXPORT void cleanupFullscreen();
    WEBCORE_EXPORT void setPlayerIdentifier(std::optional<MediaPlayerIdentifier>) final;
    WEBCORE_EXPORT void requestHideAndExitFullscreen();
    WEBCORE_EXPORT void preparedToReturnToInline(bool visible, const FloatRect& inlineRect);
    WEBCORE_EXPORT bool mayAutomaticallyShowVideoPictureInPicture() const;
    void willStartPictureInPicture();
    void didStartPictureInPicture();
    void failedToStartPictureInPicture();
    void didStopPictureInPicture();
    void prepareForPictureInPictureStopWithCompletionHandler(void (^)(BOOL));
    bool shouldExitFullscreenWithReason(ExitFullScreenReason);
    WEBCORE_EXPORT void setInlineRect(const FloatRect&, bool visible);
    bool isPlayingVideoInEnhancedFullscreen() const;
    bool changingStandbyOnly() { return m_changingStandbyOnly; }
    bool allowsPictureInPicturePlayback() const { return m_allowsPictureInPicturePlayback; }
private:
    WEBCORE_EXPORT VideoPresentationInterfaceAVKit(PlaybackSessionInterfaceAVKit&);
    void doSetup();
    void doEnterFullscreen();
    void doExitFullscreen();
    void exitFullscreenHandler(BOOL success, NSError *, NextActions = NextActions());
    void enterFullscreenHandler(BOOL success, NSError *, NextActions = NextActions());
    void watchdogTimerFired();

    RetainPtr<WebAVPlayerLayerView> m_playerLayerView;
    bool m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason { false };
    RetainPtr<WebAVPlayerViewControllerDelegate> m_playerViewControllerDelegate;
    RetainPtr<WebAVPlayerViewController> m_playerViewController;
    RetainPtr<UIWindow> m_parentWindow;
    bool m_changingStandbyOnly { false };
    bool m_allowsPictureInPicturePlayback { false };
    RetainPtr<UIView> m_videoView;
};
} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && HAVE(AVKIT)
