/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY) && ENABLE(VIDEO_PRESENTATION_MODE)

#include "EventListener.h"
#include "HTMLMediaElementEnums.h"
#include "PlatformLayer.h"
#include "PlaybackSessionInterfaceAVKit.h"
#include "VideoFullscreenModel.h"
#include <objc/objc.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS UIViewController;
OBJC_CLASS UIWindow;
OBJC_CLASS UIView;
OBJC_CLASS CALayer;
OBJC_CLASS WebAVPlayerController;
OBJC_CLASS WebAVPlayerLayerView;
OBJC_CLASS WebAVPlayerLayer;
OBJC_CLASS WebAVPlayerViewController;
OBJC_CLASS WebAVPlayerViewControllerDelegate;
OBJC_CLASS NSError;

namespace WebCore {
class IntRect;
class FloatSize;
class VideoFullscreenModel;
class VideoFullscreenChangeObserver;

class VideoFullscreenInterfaceAVKit final
    : public VideoFullscreenModelClient
    , public PlaybackSessionModelClient
    , public ThreadSafeRefCounted<VideoFullscreenInterfaceAVKit, WTF::DestructionThread::MainRunLoop>
    , public CanMakeWeakPtr<VideoFullscreenInterfaceAVKit> {
public:
    WEBCORE_EXPORT static Ref<VideoFullscreenInterfaceAVKit> create(PlaybackSessionInterfaceAVKit&);
    virtual ~VideoFullscreenInterfaceAVKit();
    WEBCORE_EXPORT void setVideoFullscreenModel(VideoFullscreenModel*);
    WEBCORE_EXPORT void setVideoFullscreenChangeObserver(VideoFullscreenChangeObserver*);
    PlaybackSessionInterfaceAVKit& playbackSessionInterface() const { return m_playbackSessionInterface.get(); }
    PlaybackSessionModel* playbackSessionModel() const { return m_playbackSessionInterface->playbackSessionModel(); }

    // VideoFullscreenModelClient
    WEBCORE_EXPORT void hasVideoChanged(bool) final;
    WEBCORE_EXPORT void videoDimensionsChanged(const FloatSize&) final;
    WEBCORE_EXPORT void modelDestroyed() final;

    // PlaybackSessionModelClient
    WEBCORE_EXPORT void externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) final;

    WEBCORE_EXPORT void setupFullscreen(UIView&, const IntRect& initialRect, UIView *, HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicturePlayback, bool standby);
    WEBCORE_EXPORT void enterFullscreen();
    WEBCORE_EXPORT void exitFullscreen(const IntRect& finalRect);
    WEBCORE_EXPORT void cleanupFullscreen();
    WEBCORE_EXPORT void invalidate();
    WEBCORE_EXPORT void requestHideAndExitFullscreen();
    WEBCORE_EXPORT void preparedToReturnToInline(bool visible, const IntRect& inlineRect);
    WEBCORE_EXPORT void preparedToExitFullscreen();
    WEBCORE_EXPORT void setHasVideoContentLayer(bool);
    WEBCORE_EXPORT void setInlineRect(const IntRect&, bool visible);

    enum class ExitFullScreenReason {
        DoneButtonTapped,
        FullScreenButtonTapped,
        PinchGestureHandled,
        RemoteControlStopEventReceived,
        PictureInPictureStarted
    };

    class Mode {
        HTMLMediaElementEnums::VideoFullscreenMode m_mode { HTMLMediaElementEnums::VideoFullscreenModeNone };

    public:
        Mode() = default;
        Mode(const Mode&) = default;
        Mode(HTMLMediaElementEnums::VideoFullscreenMode mode) : m_mode(mode) { }
        void operator=(HTMLMediaElementEnums::VideoFullscreenMode mode) { m_mode = mode; }
        HTMLMediaElementEnums::VideoFullscreenMode mode() const { return m_mode; }

        void setModeValue(HTMLMediaElementEnums::VideoFullscreenMode mode, bool value) { value ? setMode(mode) : clearMode(mode); }
        void setMode(HTMLMediaElementEnums::VideoFullscreenMode mode) { m_mode |= mode; }
        void clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode) { m_mode &= ~mode; }
        bool hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const { return m_mode & mode; }

        bool isPictureInPicture() const { return m_mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture; }
        bool isFullscreen() const { return m_mode == HTMLMediaElementEnums::VideoFullscreenModeStandard; }

        void setPictureInPicture(bool value) { setModeValue(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, value); }
        void setFullscreen(bool value) { setModeValue(HTMLMediaElementEnums::VideoFullscreenModeStandard, value); }

        bool hasFullscreen() const { return hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard); }
        bool hasPictureInPicture() const { return hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture); }

        bool hasVideo() const { return m_mode & (HTMLMediaElementEnums::VideoFullscreenModeStandard | HTMLMediaElementEnums::VideoFullscreenModePictureInPicture); }
    };

    Mode m_currentMode;
    Mode m_targetMode;

    VideoFullscreenModel* videoFullscreenModel() const { return m_videoFullscreenModel; }
    bool shouldExitFullscreenWithReason(ExitFullScreenReason);
    HTMLMediaElementEnums::VideoFullscreenMode mode() const { return m_currentMode.mode(); }
    bool allowsPictureInPicturePlayback() const { return m_allowsPictureInPicturePlayback; }
    WEBCORE_EXPORT bool mayAutomaticallyShowVideoPictureInPicture() const;
    void fullscreenMayReturnToInline(WTF::Function<void(bool)>&& callback);
    bool wirelessVideoPlaybackDisabled() const;
    WEBCORE_EXPORT void applicationDidBecomeActive();

    void willStartPictureInPicture();
    void didStartPictureInPicture();
    void failedToStartPictureInPicture();
    void willStopPictureInPicture();
    void didStopPictureInPicture();
    void prepareForPictureInPictureStopWithCompletionHandler(void (^)(BOOL));
    void exitFullscreenHandler(BOOL success, NSError *);
    void enterFullscreenHandler(BOOL success, NSError *);
    bool isPlayingVideoInEnhancedFullscreen() const;

    WEBCORE_EXPORT void setMode(HTMLMediaElementEnums::VideoFullscreenMode);
    void clearMode(HTMLMediaElementEnums::VideoFullscreenMode);
    bool hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const { return m_currentMode.hasMode(mode); }

#if PLATFORM(IOS_FAMILY)
    UIViewController *presentingViewController();
    UIViewController *fullscreenViewController() const { return m_viewController.get(); }
    WebAVPlayerLayerView* playerLayerView() const { return m_playerLayerView.get(); }
    WEBCORE_EXPORT bool pictureInPictureWasStartedWhenEnteringBackground() const;
#endif

protected:
    WEBCORE_EXPORT VideoFullscreenInterfaceAVKit(PlaybackSessionInterfaceAVKit&);

    void doSetup();
    void finalizeSetup();
    void doExitFullscreen();
    void returnToStandby();
    void doEnterFullscreen();
    void watchdogTimerFired();
    WebAVPlayerController *playerController() const;

    Ref<PlaybackSessionInterfaceAVKit> m_playbackSessionInterface;
    RetainPtr<WebAVPlayerViewControllerDelegate> m_playerViewControllerDelegate;
    RetainPtr<WebAVPlayerViewController> m_playerViewController;
    VideoFullscreenModel* m_videoFullscreenModel { nullptr };
    VideoFullscreenChangeObserver* m_fullscreenChangeObserver { nullptr };

    // These are only used when fullscreen is presented in a separate window.
    RetainPtr<UIWindow> m_window;
    RetainPtr<UIViewController> m_viewController;
    RetainPtr<UIView> m_videoView;
    RetainPtr<UIView> m_parentView;
    RetainPtr<UIWindow> m_parentWindow;
    RetainPtr<WebAVPlayerLayerView> m_playerLayerView;
    WTF::Function<void(bool)> m_prepareToInlineCallback;
    RunLoop::Timer<VideoFullscreenInterfaceAVKit> m_watchdogTimer;
    FloatRect m_inlineRect;
    RouteSharingPolicy m_routeSharingPolicy { RouteSharingPolicy::Default };
    String m_routingContextUID;
    bool m_allowsPictureInPicturePlayback { false };
    bool m_wirelessVideoPlaybackDisabled { true };
    bool m_shouldReturnToFullscreenWhenStoppingPiP { false };
    bool m_restoringFullscreenForPictureInPictureStop { false };

    bool m_setupNeedsInlineRect { false };
    bool m_exitFullscreenNeedInlineRect { false };

    bool m_finalizeSetupNeedsVideoContentLayer { false };
    bool m_cleanupNeedsReturnVideoContentLayer { false };

    bool m_returnToStandbyNeedsReturnVideoContentLayer { false };
    bool m_finalizeSetupNeedsReturnVideoContentLayer { false };

    bool m_exitFullscreenNeedsExitFullscreen { false };
    bool m_exitFullscreenNeedsExitPictureInPicture { false };
    bool m_exitFullscreenNeedsReturnContentLayer { false };

    bool m_enterFullscreenNeedsEnterFullscreen { false };
    bool m_enterFullscreenNeedsExitFullscreen { false };
    bool m_enterFullscreenNeedsEnterPictureInPicture { false };
    bool m_enterFullscreenNeedsExitPictureInPicture { false };

    bool m_hasVideoContentLayer { false };

    bool m_hasUpdatedInlineRect { false };
    bool m_inlineIsVisible { false };
    bool m_standby { false };
    bool m_targetStandby { false };

    bool m_waitingForPreparedToExit { false };
    bool m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason { false };
};

}

#endif // PLATFORM(IOS_FAMILY)

