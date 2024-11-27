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

#import "config.h"
#import "VideoPresentationInterfaceIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "PictureInPictureSupport.h"
#import "TimeRanges.h"
#import "UIViewControllerUtilities.h"
#import "WebAVPlayerLayer.h"
#import "WebAVPlayerLayerView.h"
#import <UIKit/UIKit.h>
#import <UIKit/UIWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/BlockPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import <pal/ios/UIKitSoftLink.h>

@interface UIWindow ()
- (BOOL)_isHostedInAnotherProcess;
@end

@interface UIViewController ()
@property (nonatomic, assign, setter=_setIgnoreAppSupportedOrientations:) BOOL _ignoreAppSupportedOrientations;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceIOS);

static UIColor *clearUIColor()
{
    return (UIColor *)[PAL::getUIColorClass() clearColor];
}

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

static const Seconds defaultWatchdogTimerInterval { 1_s };
static bool ignoreWatchdogForDebugging = false;

static UIViewController *fallbackViewController(UIView *view)
{
    // FIXME: This logic to find a fallback view controller should move out of WebCore,
    // and into the client layer.
    for (UIView *currentView = view; currentView; currentView = currentView.superview) {
        if (auto controller = viewController(currentView)) {
            if (!controller.parentViewController)
                return controller;
        }
    }

    LOG_ERROR("Failed to find a view controller suitable to present fullscreen video");
    return nil;
}

UIViewController *VideoPresentationInterfaceIOS::presentingViewController()
{
    auto model = videoPresentationModel();
    auto *controller = model ? model->presentingViewController() : nil;
    if (!controller)
        controller = fallbackViewController(m_parentView.get());

    return controller;
}

VideoPresentationInterfaceIOS::VideoPresentationInterfaceIOS(PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : m_watchdogTimer(RunLoop::main(), this, &VideoPresentationInterfaceIOS::watchdogTimerFired)
    , m_playbackSessionInterface(playbackSessionInterface)
{
}

VideoPresentationInterfaceIOS::~VideoPresentationInterfaceIOS()
{
    if (auto model = videoPresentationModel())
        model->removeClient(*this);
}

void VideoPresentationInterfaceIOS::setVideoPresentationModel(VideoPresentationModel* model)
{
    if (auto oldModel = videoPresentationModel())
        oldModel->removeClient(*this);

    m_videoPresentationModel = model;

    if (model) {
        model->addClient(*this);
        model->requestRouteSharingPolicyAndContextUID([this, protectedThis = Ref { *this }] (RouteSharingPolicy policy, String contextUID) {
            m_routeSharingPolicy = policy;
            m_routingContextUID = contextUID;

            updateRouteSharingPolicy();
        });
    }

    hasVideoChanged(model ? model->hasVideo() : false);
    videoDimensionsChanged(model ? model->videoDimensions() : FloatSize());
}

void VideoPresentationInterfaceIOS::setupFullscreen(const FloatRect& initialRect, const FloatSize&, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::setupFullscreen(%p)", this);

    hasVideoChanged(true);

    m_changingStandbyOnly = mode == HTMLMediaElementEnums::VideoFullscreenModeNone && standby;
    m_allowsPictureInPicturePlayback = allowsPictureInPicturePlayback;
    m_parentView = parentView;
    m_parentWindow = parentView.window;

    m_targetStandby = standby;
    m_targetMode = mode;
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
    setInlineRect(initialRect, true);
    doSetup();
}

std::optional<MediaPlayerIdentifier>VideoPresentationInterfaceIOS::playerIdentifier() const
{
    return m_playbackSessionInterface->playerIdentifier();
}

void VideoPresentationInterfaceIOS::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    m_playbackSessionInterface->setPlayerIdentifier(WTFMove(identifier));
}

void VideoPresentationInterfaceIOS::requestHideAndExitFullscreen()
{
    if (m_currentMode.hasPictureInPicture())
        return;

    LOG(Fullscreen, "VideoPresentationInterfaceIOS::requestHideAndExitFullscreen(%p)", this);

    [m_window setHidden:YES];
    playerViewController().view.hidden = YES;

    auto model = videoPresentationModel();
    if (playbackSessionModel() && model) {
        playbackSessionModel()->pause();
        model->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void VideoPresentationInterfaceIOS::preparedToReturnToInline(bool visible, const FloatRect& inlineRect)
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    setInlineRect(inlineRect, visible);
    [playerViewController().view setNeedsLayout];
    [playerViewController().view layoutIfNeeded];
    if (m_prepareToInlineCallback) {
        WTF::Function<void(bool)> callback = WTFMove(m_prepareToInlineCallback);
        callback(visible);
    }
}

void VideoPresentationInterfaceIOS::doSetup()
{
    if (m_currentMode.hasVideo() && m_targetMode.hasVideo()) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "both targetMode and currentMode haveVideo, bailing");
        m_standby = m_targetStandby;
        finalizeSetup();
        return;
    }

    auto model = videoPresentationModel();
    if (!m_hasUpdatedInlineRect && model) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "!hasUpdatedInlineRect, bailing");
        m_setupNeedsInlineRect = true;
        model->requestUpdateInlineRect();
        return;
    }

    m_setupNeedsInlineRect = false;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

#if !PLATFORM(WATCHOS)
    if (![[m_parentView window] _isHostedInAnotherProcess] && !m_window && !PAL::currentUserInterfaceIdiomIsVision()) {
        m_window = adoptNS([PAL::allocUIWindowInstance() initWithWindowScene:[[m_parentView window] windowScene]]);
        [m_window setBackgroundColor:clearUIColor()];
        if (!m_viewController)
            m_viewController = adoptNS([PAL::allocUIViewControllerInstance() init]);
        [[m_viewController view] setFrame:[m_window bounds]];
        [m_viewController _setIgnoreAppSupportedOrientations:YES];
        [m_window setRootViewController:m_viewController.get()];
        auto textEffectsWindowLevel = [&] {
            auto *textEffectsWindow = [PAL::getUITextEffectsWindowClass() sharedTextEffectsWindowForWindowScene:[m_window windowScene]];
            return textEffectsWindow ? textEffectsWindow.windowLevel : PAL::get_UIKit_UITextEffectsBeneathStatusBarWindowLevel();
        }();
        [m_window setWindowLevel:textEffectsWindowLevel - 1];
        [m_window makeKeyAndVisible];
    }
#endif // !PLATFORM(WATCHOS)

    RetainPtr playerLayerView = this->playerLayerView();
    [playerLayerView setHidden:isExternalPlaybackActive()];
    [playerLayerView setBackgroundColor:clearUIColor()];

    setupPlayerViewController();

    if (UIViewController *playerViewController = this->playerViewController()) {
        if (m_viewController) {
            [m_viewController addChildViewController:playerViewController];
            [[m_viewController view] addSubview:playerViewController.view];
            [playerViewController didMoveToParentViewController:m_viewController.get()];
        } else
            [m_parentView addSubview:playerViewController.view];

        playerViewController.view.frame = [m_parentView convertRect:m_inlineRect toView:playerViewController.view.superview];
        playerViewController.view.backgroundColor = clearUIColor();
        playerViewController.view.autoresizingMask = (UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin);

        [playerViewController.view setNeedsLayout];
        [playerViewController.view layoutIfNeeded];

        if (m_targetStandby && !m_currentMode.hasVideo() && !m_returningToStandby) {
            [m_window setHidden:YES];
            [playerViewController.view setHidden:YES];
        }
    }

    [CATransaction commit];

    finalizeSetup();
}

void VideoPresentationInterfaceIOS::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (videoDimensions.isZero())
        return;

    playerLayer().videoDimensions = videoDimensions;
    setContentDimensions(videoDimensions);
    [playerLayerView() setNeedsLayout];

#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView() pictureInPicturePlayerLayerView];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [pipPlayerLayer setVideoDimensions:playerLayer().videoDimensions];
    [pipView setNeedsLayout];
#endif
}

void VideoPresentationInterfaceIOS::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    [playerLayerView() setHidden:enabled];
}

void VideoPresentationInterfaceIOS::setInlineRect(const FloatRect& inlineRect, bool visible)
{
    m_inlineRect = inlineRect;
    m_inlineIsVisible = visible;
    m_hasUpdatedInlineRect = true;

    if (playerViewController() && m_parentView) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        playerViewController().view.frame = [m_parentView convertRect:inlineRect toView:playerViewController().view.superview];
        [CATransaction commit];
    }

    if (m_setupNeedsInlineRect)
        doSetup();

    if (m_exitFullscreenNeedInlineRect)
        doExitFullscreen();
}


WebAVPlayerController *VideoPresentationInterfaceIOS::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

void VideoPresentationInterfaceIOS::applicationDidBecomeActive()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::applicationDidBecomeActive(%p)", this);
}

void VideoPresentationInterfaceIOS::enterFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::enterFullscreen(%p) %d", this, mode());

    doEnterFullscreen();
}

void VideoPresentationInterfaceIOS::doEnterFullscreen()
{
    m_standby = m_targetStandby;

    [playerViewController().view layoutIfNeeded];
    if (m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen()) {
        [m_window setHidden:NO];
        presentFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
            enterFullscreenHandler(success, error, NextAction::NeedsEnterFullScreen);
        });
        return;
    }

    if (m_targetMode.hasPictureInPicture() && !m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsEnterPictureInPicture = true;
        tryToStartPictureInPicture();
        return;
    }

    m_enterFullscreenNeedsEnterPictureInPicture = false;
    if (!m_targetMode.hasFullscreen() && m_currentMode.hasFullscreen()) {
        dismissFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
            exitFullscreenHandler(success, error, NextAction::NeedsEnterFullScreen);
        });
        return;
    }

    if (!m_targetMode.hasPictureInPicture() && m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsExitPictureInPicture = true;
        stopPictureInPicture();
        return;
    }
    m_enterFullscreenNeedsExitPictureInPicture = false;

    auto model = videoPresentationModel();
    if (!model)
        return;

    FloatSize size;
#if HAVE(PICTUREINPICTUREPLAYERLAYERVIEW)
    if (m_currentMode.hasPictureInPicture()) {
        auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView() pictureInPicturePlayerLayerView];
        auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
        auto videoFrame = [pipPlayerLayer calculateTargetVideoFrame];
        size = FloatSize(videoFrame.size());
    }
#endif
    model->didEnterFullscreen(size);
    m_enteringPictureInPicture = false;
    m_changingStandbyOnly = false;
    if (m_currentMode.hasPictureInPicture())
        model->didEnterPictureInPicture();
}

void VideoPresentationInterfaceIOS::enterFullscreenHandler(BOOL success, NSError *error, NextActions nextActions)
{
    if (!success) {
        WTFLogAlways("-[AVPlayerViewController enterFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);
        ASSERT_NOT_REACHED();
        return;
    }

    LOG(Fullscreen, "VideoPresentationInterfaceIOS::enterFullscreenStandard - lambda(%p)", this);
    if (!m_standby)
        setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, !nextActions.contains(NextAction::NeedsEnterFullScreen));

    // NOTE: During a "returnToStandby" operation, this will cause the AVKit controls
    // to be visible if the user taps on the fullscreen presentation before the Element
    // Fullscreen presentation is fully restored. This is intentional; in the case that
    // the Element Fullscreen presentation fails for any reason, this gives the user
    // the ability to dismiss AVKit fullscreen.
    setShowsPlaybackControls(true);

    if (nextActions.contains(NextAction::NeedsEnterFullScreen))
        doEnterFullscreen();
}

bool VideoPresentationInterfaceIOS::exitFullscreen(const FloatRect& finalRect)
{
    m_watchdogTimer.stop();

    // VideoPresentationManager may ask a video to exit standby while the video
    // is entering picture-in-picture. We need to ignore the request in that case.
    if (m_standby && m_enteringPictureInPicture)
        return false;

    m_changingStandbyOnly = !m_currentMode.hasVideo() && m_standby;

    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;

    setInlineRect(finalRect, true);
    doExitFullscreen();
    m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason = true;

    return true;
}

void VideoPresentationInterfaceIOS::doExitFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::doExitFullscreen(%p)", this);

    auto model = videoPresentationModel();
    if (m_currentMode.hasVideo() && !m_hasUpdatedInlineRect && model) {
        m_exitFullscreenNeedInlineRect = true;
        model->requestUpdateInlineRect();
        return;
    }
    m_exitFullscreenNeedInlineRect = false;

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        dismissFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
            exitFullscreenHandler(success, error, NextAction::NeedsExitFullScreen);
        });
        return;
    }

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        m_exitFullscreenNeedsExitPictureInPicture = true;
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;
        [m_window setHidden:NO];
        stopPictureInPicture();
        return;
    }
    m_exitFullscreenNeedsExitPictureInPicture = false;

    if (m_hasVideoContentLayer && model) {
        m_exitFullscreenNeedsReturnContentLayer = true;
        model->returnVideoContentLayer();
        return;
    }
    m_exitFullscreenNeedsReturnContentLayer = false;

    m_standby = false;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (auto model = videoPresentationModel())
            model->didExitFullscreen();
        m_changingStandbyOnly = false;
    });
}

void VideoPresentationInterfaceIOS::exitFullscreenHandler(BOOL success, NSError* error, NextActions nextActions)
{
    if (!success)
        WTFLogAlways("-[AVPlayerViewController exitFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);

    LOG(Fullscreen, "VideoPresentationInterfaceIOS::didExitFullscreen(%p) - %d", this, success);

    clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, false);

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        [m_window setHidden:YES];
        [playerViewController().view setHidden:YES];
    } else {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [playerLayerView() setBackgroundColor:clearUIColor()];
        [playerViewController().view setBackgroundColor:clearUIColor()];
        [CATransaction commit];
    }

    if (nextActions.contains(NextAction::NeedsEnterFullScreen))
        doEnterFullscreen();

    if (nextActions.contains(NextAction::NeedsExitFullScreen))
        doExitFullscreen();
}

void VideoPresentationInterfaceIOS::cleanupFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::cleanupFullscreen(%p)", this);
    m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason = false;

    m_cleanupNeedsReturnVideoContentLayer = true;
    RefPtr model = videoPresentationModel();
    if (m_hasVideoContentLayer && model) {
        model->returnVideoContentLayer();
        return;
    }
    m_cleanupNeedsReturnVideoContentLayer = false;

    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
    }

    RetainPtr playerViewController = this->playerViewController();

    invalidatePlayerViewController();

    if (m_currentMode.hasPictureInPicture())
        stopPictureInPicture();

    if (m_currentMode.hasFullscreen()) {
        [[playerViewController view] layoutIfNeeded];
        dismissFullscreen(false, [](BOOL success, NSError *error) {
            if (!success)
                WTFLogAlways("-[AVPlayerViewController exitFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);
        });
    }

    [playerViewController willMoveToParentViewController:nil];
    [[playerViewController view] removeFromSuperview];
    [playerViewController removeFromParentViewController];

    [[m_viewController view] removeFromSuperview];

    m_window = nil;
    m_parentView = nil;
    m_parentWindow = nil;

    hasVideoChanged(false);

    if (m_exitingPictureInPicture) {
        m_exitingPictureInPicture = false;
        if (model)
            model->didExitPictureInPicture();
    }

    if (model)
        model->didCleanupFullscreen();
}

void VideoPresentationInterfaceIOS::exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT_UNUSED(mode, mode == HTMLMediaElementEnums::VideoFullscreenModeNone);
    m_watchdogTimer.stop();
    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    m_currentMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    cleanupFullscreen();
}

void VideoPresentationInterfaceIOS::invalidate()
{
    m_videoPresentationModel = nullptr;
    m_watchdogTimer.stop();
    m_enteringPictureInPicture = false;
    cleanupFullscreen();
}

void VideoPresentationInterfaceIOS::preparedToExitFullscreen()
{
#if PLATFORM(WATCHOS)
    if (!m_waitingForPreparedToExit)
        return;

    m_waitingForPreparedToExit = false;
    auto model = videoPresentationModel();
    if (model)
        model->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, true);
#endif
}

void VideoPresentationInterfaceIOS::prepareForPictureInPictureStop(WTF::Function<void(bool)>&& callback)
{
    m_prepareToInlineCallback = WTFMove(callback);
    if (auto model = videoPresentationModel())
        model->fullscreenMayReturnToInline();
}

void VideoPresentationInterfaceIOS::willStartPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::willStartPictureInPicture(%p)", this);
    m_enteringPictureInPicture = true;

    if (m_standby && !m_currentMode.hasVideo()) {
        [m_window setHidden:NO];
        playerViewController().view.hidden = NO;
    }

    if (auto model = videoPresentationModel()) {
        if (!m_hasVideoContentLayer)
            model->requestVideoContentLayer();
        model->setRequiresTextTrackRepresentation(true);
        model->willEnterPictureInPicture();
    }
}

void VideoPresentationInterfaceIOS::didStartPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::didStartPictureInPicture(%p)", this);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_enterFullscreenNeedsEnterPictureInPicture);
    setShowsPlaybackControls(true);
    [m_viewController _setIgnoreAppSupportedOrientations:NO];

    if (m_currentMode.hasFullscreen()) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;
        [playerViewController().view layoutIfNeeded];
        dismissFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
            exitFullscreenHandler(success, error);
        });
    } else {
        if (m_standby && !m_blocksReturnToFullscreenFromPictureInPicture)
            m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;

        [m_window setHidden:YES];
        playerViewController().view.hidden = YES;
    }

    if (m_enterFullscreenNeedsEnterPictureInPicture)
        doEnterFullscreen();
}

void VideoPresentationInterfaceIOS::failedToStartPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::failedToStartPictureInPicture(%p)", this);
    setShowsPlaybackControls(true);

    m_targetMode.setPictureInPicture(false);
    if (m_currentMode.hasFullscreen())
        return;

    if (auto model = videoPresentationModel()) {
        model->failedToEnterPictureInPicture();
        model->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
        model->fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenModeNone);
        model->failedToEnterFullscreen();
    }
    m_changingStandbyOnly = false;

    m_enterFullscreenNeedsExitPictureInPicture = false;
    m_exitFullscreenNeedsExitPictureInPicture = false;
}

void VideoPresentationInterfaceIOS::willStopPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::willStopPictureInPicture(%p)", this);

    m_exitingPictureInPicture = true;
    m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

    if (m_currentMode.hasFullscreen())
        return;

    if (auto model = videoPresentationModel())
        model->willExitPictureInPicture();
}

void VideoPresentationInterfaceIOS::didStopPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::didStopPictureInPicture(%p)", this);
    m_targetMode.setPictureInPicture(false);
    [m_viewController _setIgnoreAppSupportedOrientations:YES];

    if (m_returningToStandby) {
        m_exitingPictureInPicture = false;
        m_enteringPictureInPicture = false;
        if (auto model = videoPresentationModel())
            model->didExitPictureInPicture();

        return;
    }

    if (m_currentMode.hasFullscreen()) {
        clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_exitFullscreenNeedsExitPictureInPicture);
        [m_window makeKeyWindow];
        setShowsPlaybackControls(true);

        if (m_exitFullscreenNeedsExitPictureInPicture)
            doExitFullscreen();
        else if (m_exitingPictureInPicture) {
            m_exitingPictureInPicture = false;
            if (auto model = videoPresentationModel())
                model->didExitPictureInPicture();
        }

        if (m_enterFullscreenNeedsExitPictureInPicture)
            doEnterFullscreen();
        return;
    }

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_exitFullscreenNeedsExitPictureInPicture);

    [playerLayerView() setBackgroundColor:clearUIColor()];
    playerViewController().view.backgroundColor = clearUIColor();

    if (m_enterFullscreenNeedsExitPictureInPicture)
        doEnterFullscreen();

    if (m_exitFullscreenNeedsExitPictureInPicture)
        doExitFullscreen();

    if (!m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen() && !m_hasVideoContentLayer) {
        // We have just exited pip and not entered fullscreen in turn. To avoid getting
        // stuck holding the video content layer, explicitly return it here:
        returnVideoView();
    }
}

void VideoPresentationInterfaceIOS::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::prepareForPictureInPictureStopWithCompletionHandler(%p)", this);

    if (m_shouldReturnToFullscreenWhenStoppingPictureInPicture) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

        [m_window setHidden:NO];
        playerViewController().view.hidden = NO;

        [playerViewController().view layoutIfNeeded];
        presentFullscreen(true, [this, protectedThis = Ref { *this }, completionHandler = makeBlockPtr(completionHandler)](BOOL success, NSError *error) {
            enterFullscreenHandler(success, error);
            completionHandler(success);
        });

        if (m_standby) {
            m_returningToStandby = true;
            setAllowsPictureInPicturePlayback(false);
        }

        return;
    }

    prepareForPictureInPictureStop([this, protectedThis = Ref { *this }, completionHandler = makeBlockPtr(completionHandler)](bool restored)  {
        UNUSED_PARAM(this);
        LOG(Fullscreen, "VideoPresentationInterfaceIOS::prepareForPictureInPictureStopWithCompletionHandler lambda(%p) - restored(%s)", this, boolString(restored));
        completionHandler(restored);
    });
}

bool VideoPresentationInterfaceIOS::shouldExitFullscreenWithReason(VideoPresentationInterfaceIOS::ExitFullScreenReason reason)
{
    // AVKit calls playerViewController:shouldExitFullScreenWithReason in the scenario that the exit fullscreen request
    // is from the web process (e.g., through Javascript API videoElement.webkitExitFullscreen()).
    // We have to ignore the callback in that case.
    if (m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason)
        return true;

    auto model = videoPresentationModel();
    if (!model)
        return true;

    if (reason == ExitFullScreenReason::PictureInPictureStarted)
        return false;

    if (playbackSessionModel() && (reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::RemoteControlStopEventReceived))
        playbackSessionModel()->pause();

    if (!m_watchdogTimer.isActive() && !ignoreWatchdogForDebugging)
        m_watchdogTimer.startOneShot(defaultWatchdogTimerInterval);

#if PLATFORM(WATCHOS)
    m_waitingForPreparedToExit = true;
    model->willExitFullscreen();
    return false;
#else
    BOOL finished = reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::PinchGestureHandled;
    model->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, finished);
    return false;
#endif
}

NO_RETURN_DUE_TO_ASSERT void VideoPresentationInterfaceIOS::watchdogTimerFired()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::watchdogTimerFired(%p) - no exit fullscreen response in %gs; forcing fullscreen hidden.", this, defaultWatchdogTimerInterval.value());
    ASSERT_NOT_REACHED();
    [m_window setHidden:YES];
    playerViewController().view.hidden = YES;
}

void VideoPresentationInterfaceIOS::setHasVideoContentLayer(bool value)
{
    m_hasVideoContentLayer = value;

    if (m_hasVideoContentLayer && m_finalizeSetupNeedsVideoContentLayer)
        finalizeSetup();
    if (!m_hasVideoContentLayer && m_cleanupNeedsReturnVideoContentLayer)
        cleanupFullscreen();
    if (!m_hasVideoContentLayer && m_finalizeSetupNeedsReturnVideoContentLayer && !m_returningToStandby)
        finalizeSetup();
    if (!m_hasVideoContentLayer && m_returningToStandby)
        returnToStandby();
    if (!m_hasVideoContentLayer && m_exitFullscreenNeedsReturnContentLayer)
        doExitFullscreen();
}

void VideoPresentationInterfaceIOS::preparedToReturnToStandby()
{
    if (!m_returningToStandby)
        return;

    returnToStandby();
}

void VideoPresentationInterfaceIOS::finalizeSetup()
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (auto model = videoPresentationModel()) {
            if (!m_hasVideoContentLayer && m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsVideoContentLayer = true;
                model->requestVideoContentLayer();
                model->setRequiresTextTrackRepresentation(true);
                return;
            }
            m_finalizeSetupNeedsVideoContentLayer = false;
            if (m_hasVideoContentLayer && !m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsReturnVideoContentLayer = true;
                model->returnVideoContentLayer();
                model->setRequiresTextTrackRepresentation(false);
                return;
            }
            m_finalizeSetupNeedsReturnVideoContentLayer = false;
            model->didSetupFullscreen();
        }
    });
}

void VideoPresentationInterfaceIOS::failedToRestoreFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void VideoPresentationInterfaceIOS::returnToStandby()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_returningToStandby = false;

    returnVideoView();

    // Continue processing exit picture-in-picture now that
    // it is safe to do so:
    didStopPictureInPicture();
}

void VideoPresentationInterfaceIOS::returnVideoView()
{
    if (auto model = videoPresentationModel())
        model->returnVideoView();
}

void VideoPresentationInterfaceIOS::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.setMode(mode);
    // Mode::mode() can be 3 (VideoFullscreenModeStandard | VideoFullscreenModePictureInPicture).
    // HTMLVideoElement does not expect such a value in the fullscreenModeChanged() callback.
    auto model = videoPresentationModel();
    if (!model)
        return;

    model->setRequiresTextTrackRepresentation(m_currentMode.hasVideo());

    if (shouldNotifyModel)
        model->fullscreenModeChanged(mode);
}

void VideoPresentationInterfaceIOS::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((~m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.clearMode(mode);
    auto model = videoPresentationModel();
    if (!model)
        return;

    model->setRequiresTextTrackRepresentation(m_currentMode.hasVideo());

    if (shouldNotifyModel)
        model->fullscreenModeChanged(m_currentMode.mode());
}

#if !RELEASE_LOG_DISABLED
uint64_t VideoPresentationInterfaceIOS::logIdentifier() const
{
    return m_playbackSessionInterface->logIdentifier();
}

const Logger* VideoPresentationInterfaceIOS::loggerPtr() const
{
    return m_playbackSessionInterface->loggerPtr();
}

WTFLogChannel& VideoPresentationInterfaceIOS::logChannel() const
{
    return LogFullscreen;
}
#endif

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
