/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#include "EventListener.h"
#include "HTMLMediaElementEnums.h"
#include "MediaPlayerIdentifier.h"
#include "PlatformImage.h"
#include "PlatformLayer.h"
#include "PlaybackSessionInterfaceIOS.h"
#include "SpatialVideoMetadata.h"
#include "VideoFullscreenCaptions.h"
#include "VideoPresentationLayerProvider.h"
#include "VideoPresentationModel.h"
#include <objc/objc.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVPlayerViewController;
OBJC_CLASS LMPlayableViewController;
OBJC_CLASS UIImage;
OBJC_CLASS UIViewController;
OBJC_CLASS UIWindow;
OBJC_CLASS UIView;
OBJC_CLASS CALayer;
OBJC_CLASS NSError;
OBJC_CLASS WebAVPlayerController;

namespace WebCore {

class FloatRect;
class FloatSize;

class VideoPresentationInterfaceIOS
    : public VideoPresentationModelClient
    , public PlaybackSessionModelClient
    , public VideoFullscreenCaptions
    , public VideoPresentationLayerProvider
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<VideoPresentationInterfaceIOS, WTF::DestructionThread::MainRunLoop>
    , public CanMakeCheckedPtr<VideoPresentationInterfaceIOS> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(VideoPresentationInterfaceIOS, WEBCORE_EXPORT);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(VideoPresentationInterfaceIOS);
public:
    WEBCORE_EXPORT ~VideoPresentationInterfaceIOS();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    // VideoPresentationModelClient
    WEBCORE_EXPORT void videoDimensionsChanged(const FloatSize&) override;
    WEBCORE_EXPORT void setPlayerIdentifier(std::optional<MediaPlayerIdentifier>) override;

    // PlaybackSessionModelClient
    WEBCORE_EXPORT void externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) override;

    WEBCORE_EXPORT void setVideoPresentationModel(VideoPresentationModel*);
    PlaybackSessionInterfaceIOS& playbackSessionInterface() const { return m_playbackSessionInterface.get(); }
    PlaybackSessionModel* playbackSessionModel() const { return m_playbackSessionInterface->playbackSessionModel(); }
    virtual void setSpatialImmersive(bool) { }
    WEBCORE_EXPORT virtual void setupFullscreen(const FloatRect& initialRect, const FloatSize& videoDimensions, HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture);
    WEBCORE_EXPORT virtual AVPlayerViewController *avPlayerViewController() const = 0;
    WebAVPlayerController *playerController() const;
    WEBCORE_EXPORT void enterFullscreen();
    WEBCORE_EXPORT virtual bool exitFullscreen(const FloatRect& finalRect);
    WEBCORE_EXPORT void exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode);
    WEBCORE_EXPORT virtual void cleanupFullscreen();
    WEBCORE_EXPORT void invalidate();
    WEBCORE_EXPORT virtual void requestHideAndExitFullscreen();
    WEBCORE_EXPORT virtual void preparedToReturnToInline(bool visible, const FloatRect& inlineRect);
    WEBCORE_EXPORT void preparedToExitFullscreen();
    WEBCORE_EXPORT void setHasVideoContentLayer(bool);
    WEBCORE_EXPORT virtual void setInlineRect(const FloatRect&, bool visible);
    WEBCORE_EXPORT void preparedToReturnToStandby();
    bool changingStandbyOnly() { return m_changingStandbyOnly; }
    WEBCORE_EXPORT void failedToRestoreFullscreen();

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
        bool operator==(const Mode& other) const { return m_mode == other.m_mode; }
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

    RefPtr<VideoPresentationModel> videoPresentationModel() const { return m_videoPresentationModel.get(); }
    WEBCORE_EXPORT virtual bool shouldExitFullscreenWithReason(ExitFullScreenReason);
    HTMLMediaElementEnums::VideoFullscreenMode mode() const { return m_currentMode.mode(); }
    WEBCORE_EXPORT virtual bool mayAutomaticallyShowVideoPictureInPicture() const = 0;
    void prepareForPictureInPictureStop(Function<void(bool)>&& callback);
    WEBCORE_EXPORT void applicationDidBecomeActive();
    bool inPictureInPicture() const { return m_enteringPictureInPicture || m_currentMode.hasPictureInPicture(); }
    bool returningToStandby() const { return m_returningToStandby; }

    WEBCORE_EXPORT virtual void willStartPictureInPicture();
    WEBCORE_EXPORT virtual void didStartPictureInPicture(const FloatSize&);
    WEBCORE_EXPORT virtual void failedToStartPictureInPicture();
    void willStopPictureInPicture();
    WEBCORE_EXPORT virtual void didStopPictureInPicture();
    WEBCORE_EXPORT virtual void prepareForPictureInPictureStopWithCompletionHandler(void (^)(BOOL));
    virtual bool isPlayingVideoInEnhancedFullscreen() const = 0;

    WEBCORE_EXPORT void setMode(HTMLMediaElementEnums::VideoFullscreenMode, bool shouldNotifyModel);
    void clearMode(HTMLMediaElementEnums::VideoFullscreenMode, bool shouldNotifyModel);
    bool hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const { return m_currentMode.hasMode(mode); }
    WEBCORE_EXPORT UIViewController *presentingViewController();
    UIViewController *fullscreenViewController() const { return m_viewController.get(); }
    WEBCORE_EXPORT virtual bool pictureInPictureWasStartedWhenEnteringBackground() const = 0;

    WEBCORE_EXPORT std::optional<MediaPlayerIdentifier> playerIdentifier() const;

#if ENABLE(LINEAR_MEDIA_PLAYER)
    virtual LMPlayableViewController *playableViewController() { return nil; }
#endif

#if !RELEASE_LOG_DISABLED
    WEBCORE_EXPORT uint64_t logIdentifier() const;
    WEBCORE_EXPORT const Logger* loggerPtr() const;
    ASCIILiteral logClassName() const { return "VideoPresentationInterfaceIOS"_s; };
    WEBCORE_EXPORT WTFLogChannel& logChannel() const;
#endif

    WEBCORE_EXPORT void setLayerHostView(RetainPtr<PlatformView>&&) override;
    WEBCORE_EXPORT void setPlayerLayerView(RetainPtr<WebAVPlayerLayerView>&&) override;
    WEBCORE_EXPORT void setVideoView(RetainPtr<PlatformView>&&) override;
    WEBCORE_EXPORT void setParentView(RetainPtr<PlatformView>&&);

    WEBCORE_EXPORT void togglePictureInPicture();

protected:
    WEBCORE_EXPORT VideoPresentationInterfaceIOS(PlaybackSessionInterfaceIOS&);

    RunLoop::Timer m_watchdogTimer;
    RetainPtr<UIView> m_parentView;
    Mode m_targetMode;
    RouteSharingPolicy m_routeSharingPolicy { RouteSharingPolicy::Default };
    String m_routingContextUID;
    ThreadSafeWeakPtr<VideoPresentationModel> m_videoPresentationModel;
    bool m_needsSetup { true };
    bool m_blocksReturnToFullscreenFromPictureInPicture { false };
    bool m_targetStandby { false };
    bool m_standby { false };
    Mode m_currentMode;
    bool m_enteringPictureInPicture { false };
    RetainPtr<UIWindow> m_window;
    RetainPtr<UIViewController> m_viewController;
    bool m_shouldReturnToFullscreenWhenStoppingPictureInPicture { false };
    bool m_returningToStandby { false };
    bool m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason { false };
    bool m_changingStandbyOnly { false };
    bool m_allowsPictureInPicturePlayback { false };
    RetainPtr<UIWindow> m_parentWindow;

    virtual void finalizeSetup();
    virtual void updateRouteSharingPolicy() = 0;
    virtual void setupPlayerViewController() = 0;
    virtual void invalidatePlayerViewController() = 0;
    virtual UIViewController *playerViewController() const = 0;
    void setUpWindowIfNeeded();
    void tearDownWindowIfNotNeeded();
    void tearDownWindow();
    void showOrHideWindowIfNeeded();
    void setWindowHidden(bool);
    void resolveModes();

    void startStandby();
    void endStandby();
    WEBCORE_EXPORT void enterFullscreenHandler(BOOL success, NSError *);
    WEBCORE_EXPORT void exitFullscreenHandler(BOOL success, NSError *);
    void doEnterFullscreen();
    void doExitFullscreen();
    virtual void presentFullscreen(bool animated, Function<void(BOOL, NSError *)>&&) = 0;
    virtual void dismissFullscreen(bool animated, Function<void(BOOL, NSError *)>&&) = 0;
    virtual void tryToStartPictureInPicture() = 0;
    virtual void stopPictureInPicture() = 0;
    virtual void setShowsPlaybackControls(bool) = 0;
    virtual void setContentDimensions(const FloatSize&) = 0;
    virtual void setAllowsPictureInPicturePlayback(bool) = 0;
    virtual bool isExternalPlaybackActive() const = 0;
    virtual bool willRenderToLayer() const = 0;

    virtual bool videoViewIsFullscreen() const { return false; }
    virtual bool videoViewIsInline() const { return false; }
    virtual void transferVideoViewToFullscreen() { }
    virtual void returnVideoViewToInline() { }
    virtual FloatSize fullscreenVideoViewSize() const { return { }; }

#if PLATFORM(WATCHOS)
    bool m_waitingForPreparedToExit { false };
#endif
private:
    void returnToStandby();
    void watchdogTimerFired();
    void ensurePipPlacardIsShowing();

    Ref<PlaybackSessionInterfaceIOS> m_playbackSessionInterface;
    RetainPtr<UIView> m_pipPlacard;
};

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
