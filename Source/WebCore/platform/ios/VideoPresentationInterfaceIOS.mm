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
#import <UIKit/UIImage.h>
#import <UIKit/UIImageView.h>
#import <UIKit/UIKit.h>
#import <UIKit/UILabel.h>
#import <UIKit/UIView.h>
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

static UIColor *blackUIColor()
{
    return (UIColor *)[PAL::getUIColorClass() blackColor];
}

static UIColor *greyUIColor()
{
    return (UIColor *)[PAL::getUIColorClass() colorWithRed:164.0 / 255.0 green:164.0 / 255.0 blue:164.0 / 255.0 alpha:1];
}

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

void VideoPresentationInterfaceIOS::ensurePipPlacardIsShowing()
{
    if (m_pipPlacard) {
        [m_pipPlacard setHidden:NO];
        return;
    }

    RetainPtr pipPlacard = adoptNS([PAL::allocUIViewInstance() initWithFrame:[layerHostView() bounds]]);
    [pipPlacard setBackgroundColor:blackUIColor()];
    [pipPlacard setTranslatesAutoresizingMaskIntoConstraints:NO];

    RetainPtr image = [[[PAL::getUIImageClass() systemImageNamed:@"pip"] imageWithTintColor:greyUIColor() renderingMode:UIImageRenderingModeAlwaysOriginal] imageWithConfiguration:[PAL::getUIImageSymbolConfigurationClass() configurationWithWeight:UIImageSymbolWeightThin]];

    RetainPtr imageView = adoptNS([PAL::allocUIImageViewInstance() initWithImage:image.get()]);
    [imageView setContentMode:UIViewContentModeScaleAspectFit];
    [imageView setTranslatesAutoresizingMaskIntoConstraints:NO];

    [pipPlacard addSubview:imageView.get()];

    auto pipLabel = adoptNS([PAL::allocUILabelInstance() init]);
    [pipLabel setText:@"This video is playing in picture in picture."];
    [pipLabel setTextAlignment:NSTextAlignmentCenter];
    [pipLabel setTextColor:greyUIColor()];
    [pipLabel setFont:[PAL::getUIFontClass() systemFontOfSize:16]];
    [pipLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    [pipPlacard addSubview:pipLabel.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[imageView widthAnchor] constraintEqualToConstant:[image size].width * 8],
        [[imageView heightAnchor] constraintEqualToConstant:[image size].height * 8],
        [[imageView centerXAnchor] constraintEqualToAnchor:[pipPlacard centerXAnchor]],
        [[imageView centerYAnchor] constraintEqualToAnchor:[pipPlacard centerYAnchor]],
        [[pipLabel centerXAnchor] constraintEqualToAnchor:[pipPlacard centerXAnchor]],
        [[pipLabel topAnchor] constraintEqualToAnchor:[imageView bottomAnchor] constant:10],
    ]];

    CGFloat placardWidth = [pipPlacard frame].size.width;
    CGFloat placardHeight = [pipPlacard frame].size.height;

    if (placardWidth < 170 || placardHeight < 170)
        [imageView setHidden:YES];
    if (placardHeight < 100)
        [pipLabel setHidden:YES];

    UIView *parentView = layerHostView().superview;
    [parentView.superview insertSubview:pipPlacard.get() atIndex:0];
    [NSLayoutConstraint activateConstraints:@[
        [parentView.leadingAnchor constraintEqualToAnchor:[pipPlacard leadingAnchor]],
        [parentView.trailingAnchor constraintEqualToAnchor:[pipPlacard trailingAnchor]],
        [parentView.topAnchor constraintEqualToAnchor:[pipPlacard topAnchor]],
        [parentView.bottomAnchor constraintEqualToAnchor:[pipPlacard bottomAnchor]],
    ]];

    m_pipPlacard = pipPlacard;
}

void VideoPresentationInterfaceIOS::setLayerHostView(RetainPtr<PlatformView>&& layerHostView)
{
    RetainPtr oldLayerHostView = this->layerHostView();
    if (layerHostView == oldLayerHostView)
        return;

    if (oldLayerHostView)
        [oldLayerHostView removeFromSuperview];

    VideoPresentationLayerProvider::setLayerHostView(WTFMove(layerHostView));

    RetainPtr newLayerHostView = this->layerHostView();
    if (newLayerHostView && playerLayerView())
        [playerLayerView() addSubview:newLayerHostView.get()];
}

void VideoPresentationInterfaceIOS::setPlayerLayerView(RetainPtr<WebAVPlayerLayerView>&& playerLayerView)
{
    RetainPtr oldPlayerLayerView = this->playerLayerView();
    if (playerLayerView == oldPlayerLayerView)
        return;

    if (oldPlayerLayerView) {
        [oldPlayerLayerView removeFromSuperview];
        if (videoView())
            [videoView() removeFromSuperview];
    }

    VideoPresentationLayerProvider::setPlayerLayerView(WTFMove(playerLayerView));

    if (RetainPtr newPlayerLayerView = this->playerLayerView()) {
        if (videoView())
            [videoView() addSubview:newPlayerLayerView.get()];

        if (layerHostView())
            [newPlayerLayerView addSubview:layerHostView()];
    }
}

void VideoPresentationInterfaceIOS::setVideoView(RetainPtr<PlatformView>&& videoView)
{
    RetainPtr oldVideoView = this->videoView();
    if (videoView == oldVideoView)
        return;

    if (oldVideoView && playerLayerView())
        [playerLayerView() removeFromSuperview];

    VideoPresentationLayerProvider::setVideoView(WTFMove(videoView));

    if (RetainPtr newVideoView = this->videoView(); newVideoView && playerLayerView())
        [newVideoView addSubview:playerLayerView()];
}

void VideoPresentationInterfaceIOS::setParentView(RetainPtr<PlatformView>&& parentView)
{
    m_parentView = WTFMove(parentView);
    m_parentWindow = [m_parentView window];
}

void VideoPresentationInterfaceIOS::togglePictureInPicture()
{
    if (m_currentMode.hasPictureInPicture()) {
        m_targetMode.clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        resolveModes();
    } else {
        m_targetMode.setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        resolveModes();
    }
}

void VideoPresentationInterfaceIOS::setupFullscreen(const FloatRect& initialRect, const FloatSize&, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    hasVideoChanged(true);

    m_changingStandbyOnly = mode == HTMLMediaElementEnums::VideoFullscreenModeNone && standby;
    m_allowsPictureInPicturePlayback = allowsPictureInPicturePlayback;

    m_targetStandby = standby;
    m_targetMode = mode;
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
    setInlineRect(initialRect, true);
    resolveModes();
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

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    returnVideoViewToInline();
    tearDownWindowIfNotNeeded();
}

void VideoPresentationInterfaceIOS::preparedToReturnToInline(bool visible, const FloatRect& inlineRect)
{
    UNUSED_PARAM(visible);
    UNUSED_PARAM(inlineRect);
}

void VideoPresentationInterfaceIOS::setUpWindowIfNeeded()
{
    if (!m_needsSetup)
        return;
    m_needsSetup = false;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

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

        playerViewController.view.frame = [playerLayerView convertRect:[playerLayerView frame] toView:playerViewController.view.superview];
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
}

void VideoPresentationInterfaceIOS::tearDownWindowIfNotNeeded()
{
    bool shouldTearDownWindow = [&] {
        if (m_currentMode.hasVideo() || m_standby || m_returningToStandby)
            return false;

        // We need an overlay window for the exit picture-in-picture operation
        if (m_currentMode.hasPictureInPicture() && !m_targetMode.hasPictureInPicture())
            return false;

        return true;
    }();

    if (shouldTearDownWindow)
        tearDownWindow();
}

void VideoPresentationInterfaceIOS::tearDownWindow()
{
    m_needsSetup = true;

    if (!m_window)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Tearing down window");

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr playerViewController = this->playerViewController();

    if (videoViewIsFullscreen())
        returnVideoViewToInline();

    invalidatePlayerViewController();

    [playerViewController willMoveToParentViewController:nil];
    [[playerViewController view] removeFromSuperview];
    [playerViewController removeFromParentViewController];

    [[m_viewController view] removeFromSuperview];
    m_viewController = nil;

    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
        m_window = nil;
    }

    [CATransaction commit];
}

void VideoPresentationInterfaceIOS::showOrHideWindowIfNeeded()
{
    bool windowShouldBeVisible = m_currentMode.hasFullscreen()
        || m_targetMode.hasFullscreen()
        || m_returningToStandby;

    setWindowHidden(!windowShouldBeVisible);
}

void VideoPresentationInterfaceIOS::setWindowHidden(bool hidden)
{
    if (!m_window)
        return;

    if ([m_window isHidden] == hidden)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, hidden);

    if (hidden) {
        [m_window setHidden:YES];
        playerViewController().view.hidden = YES;
    } else {
        [m_window makeKeyAndVisible];
        playerViewController().view.hidden = NO;
    }
}

void VideoPresentationInterfaceIOS::resolveModes()
{
    if (m_currentMode == m_targetMode && m_standby == m_targetStandby)
        return;

    setUpWindowIfNeeded();
    showOrHideWindowIfNeeded();

    if (m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen()) {
        doEnterFullscreen();
        return;
    }

    if (!m_targetMode.hasFullscreen() && m_currentMode.hasFullscreen()) {
        doExitFullscreen();
        return;
    }

    if (m_targetMode.hasPictureInPicture() && !m_currentMode.hasPictureInPicture()) {
        setWindowHidden(false);
        tryToStartPictureInPicture();
        return;
    }

    if (!m_targetMode.hasPictureInPicture() && m_currentMode.hasPictureInPicture()) {
        setWindowHidden(false);
        stopPictureInPicture();
        return;
    }

    if (m_targetStandby && !m_standby) {
        startStandby();
        return;
    }

    if (!m_targetStandby && m_standby) {
        endStandby();
        return;
    }
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
    UNUSED_PARAM(inlineRect);
    UNUSED_PARAM(visible);
}

WebAVPlayerController *VideoPresentationInterfaceIOS::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

void VideoPresentationInterfaceIOS::applicationDidBecomeActive()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
}

void VideoPresentationInterfaceIOS::startStandby()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_standby = true;

    if (auto model = videoPresentationModel())
        model->didEnterFullscreen({ });

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();
}

void VideoPresentationInterfaceIOS::endStandby()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_standby = false;

    if (auto model = videoPresentationModel())
        model->didExitFullscreen();

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();
}

void VideoPresentationInterfaceIOS::enterFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode());

    m_targetMode.setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
    resolveModes();
}

void VideoPresentationInterfaceIOS::doEnterFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode());

    transferVideoViewToFullscreen();
    presentFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
        enterFullscreenHandler(success, error);
    });
}

void VideoPresentationInterfaceIOS::enterFullscreenHandler(BOOL success, NSError *error)
{
    if (!success) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "failed: ", error.localizedDescription);
        showOrHideWindowIfNeeded();
        tearDownWindowIfNotNeeded();
        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    // We may be entering fullscreen as part of a "returnToStandby" operation, so
    // only set the currentMode to "fullscreen" if that's what the targetMode is
    // set to.
    if (m_targetMode.hasFullscreen()) {
        setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, false);

        if (auto model = videoPresentationModel())
            model->didEnterFullscreen(fullscreenVideoViewSize());
    }

    // NOTE: During a "returnToStandby" operation, this will cause the AVKit controls
    // to be visible if the user taps on the fullscreen presentation before the Element
    // Fullscreen presentation is fully restored. This is intentional; in the case that
    // the Element Fullscreen presentation fails for any reason, this gives the user
    // the ability to dismiss AVKit fullscreen.
    setShowsPlaybackControls(true);

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();
    resolveModes();
}

bool VideoPresentationInterfaceIOS::exitFullscreen(const FloatRect& finalRect)
{
    UNUSED_PARAM(finalRect);
    m_watchdogTimer.stop();

    // VideoPresentationManager may ask a video to exit standby while the video
    // is entering picture-in-picture. We need to ignore the request in that case.
    if (m_standby && m_enteringPictureInPicture)
        return false;

    m_changingStandbyOnly = !m_currentMode.hasVideo() && m_standby;

    m_targetStandby = false;
    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    resolveModes();

    return true;
}

void VideoPresentationInterfaceIOS::doExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (auto model = videoPresentationModel())
        model->willExitFullscreen();

    dismissFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
        exitFullscreenHandler(success, error);
    });
}

void VideoPresentationInterfaceIOS::exitFullscreenHandler(BOOL success, NSError* error)
{
    if (!success) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "failed: ", error.localizedDescription);
        exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_watchdogTimer.stop();
    clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, false);

    if (auto model = videoPresentationModel())
        model->didExitFullscreen();

    if (!m_currentMode.hasVideo())
        returnVideoViewToInline();

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();

    resolveModes();
}

void VideoPresentationInterfaceIOS::cleanupFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
}

void VideoPresentationInterfaceIOS::exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT_UNUSED(mode, mode == HTMLMediaElementEnums::VideoFullscreenModeNone);
    m_watchdogTimer.stop();
    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    m_currentMode = HTMLMediaElementEnums::VideoFullscreenModeNone;

    tearDownWindow();
}

void VideoPresentationInterfaceIOS::invalidate()
{
    m_videoPresentationModel = nullptr;
    m_watchdogTimer.stop();
    m_enteringPictureInPicture = false;
    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    m_currentMode = HTMLMediaElementEnums::VideoFullscreenModeNone;

    tearDownWindow();
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
    if (auto model = videoPresentationModel())
        model->fullscreenMayReturnToInline([callback = WTFMove(callback)] (bool visible, FloatRect) {
            callback(visible);
        });
}

void VideoPresentationInterfaceIOS::willStartPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_enteringPictureInPicture = true;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    ensurePipPlacardIsShowing();
    [CATransaction commit];

    if (!m_targetMode.hasPictureInPicture()) {
        // Entering picture-in-picture through a platform control.
        // Ensure our target mode is up to date:
        m_targetMode.setPictureInPicture(true);
        setUpWindowIfNeeded();
    }

    if (auto model = videoPresentationModel()) {
        model->setRequiresTextTrackRepresentation(true);
        model->willEnterPictureInPicture();
    }

    if (videoViewIsInline())
        transferVideoViewToFullscreen();
}

void VideoPresentationInterfaceIOS::didStartPictureInPicture(const FloatSize& size)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, false);
    setShowsPlaybackControls(true);
    [m_viewController _setIgnoreAppSupportedOrientations:NO];

    if (m_currentMode.hasFullscreen()) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;
        dismissFullscreen(true, [this, protectedThis = Ref { *this }](BOOL success, NSError *error) {
            m_targetMode.setFullscreen(false);
            exitFullscreenHandler(success, error);
        });
    } else {
        if (m_standby && !m_blocksReturnToFullscreenFromPictureInPicture)
            m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;

        setWindowHidden(true);
    }

    if (auto model = videoPresentationModel())
        model->didEnterPictureInPicture(size);

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();

    resolveModes();
}

void VideoPresentationInterfaceIOS::failedToStartPictureInPicture()
{
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    setShowsPlaybackControls(true);

    m_targetMode.setPictureInPicture(false);

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();

    if (m_currentMode.hasFullscreen())
        return;

    if (auto model = videoPresentationModel())
        model->failedToEnterPictureInPicture();

    m_changingStandbyOnly = false;
}

void VideoPresentationInterfaceIOS::willStopPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

    if (m_currentMode.hasFullscreen())
        return;

    if (auto model = videoPresentationModel())
        model->willExitPictureInPicture();
}

void VideoPresentationInterfaceIOS::didStopPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_targetMode.setPictureInPicture(false);
    [m_viewController _setIgnoreAppSupportedOrientations:YES];

    if (m_pipPlacard) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [m_pipPlacard setHidden:YES];
        [CATransaction commit];
    }

    if (m_returningToStandby) {
        m_enteringPictureInPicture = false;
        if (auto model = videoPresentationModel())
            model->didExitPictureInPicture();

        return;
    }

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, false);

    if (auto model = videoPresentationModel())
        model->didExitPictureInPicture();

    if (m_currentMode.hasFullscreen()) {
        [m_window makeKeyWindow];
        setShowsPlaybackControls(true);
        return;
    }

    if (!m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen()) {
        // We have just exited pip and not entered fullscreen in turn. To avoid getting
        // stuck holding the video content layer, explicitly return it here:
        returnVideoViewToInline();
    }

    showOrHideWindowIfNeeded();
    tearDownWindowIfNotNeeded();

    resolveModes();
}

void VideoPresentationInterfaceIOS::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    if (m_targetMode.hasPictureInPicture()) {
        // Exiting picture-in-picture through a platform control.
        // Ensure our target mode is up to date:
        m_targetMode.setPictureInPicture(false);
        setUpWindowIfNeeded();
    }

    if (m_shouldReturnToFullscreenWhenStoppingPictureInPicture) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;
        m_targetMode.setFullscreen(true);
        if (auto model = videoPresentationModel())
            model->willEnterFullscreen();

        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Returning to fullscreen");
        presentFullscreen(true, [this, protectedThis = Ref { *this }, completionHandler = makeBlockPtr(completionHandler)](BOOL success, NSError *error) {
            enterFullscreenHandler(success, error);
            completionHandler(success);
        });

        if (m_standby)
            m_returningToStandby = true;

        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    prepareForPictureInPictureStop([this, protectedThis = Ref { *this }, completionHandler = makeBlockPtr(completionHandler), logIdentifier = LOGIDENTIFIER](bool restored)  {
        UNUSED_PARAM(this);
        ALWAYS_LOG_IF_POSSIBLE(logIdentifier, "lambda, restored: ", restored);
        completionHandler(restored);
    });
}

bool VideoPresentationInterfaceIOS::shouldExitFullscreenWithReason(VideoPresentationInterfaceIOS::ExitFullScreenReason reason)
{
    UNUSED_PARAM(reason);
    if (!m_watchdogTimer.isActive() && !ignoreWatchdogForDebugging)
        m_watchdogTimer.startOneShot(defaultWatchdogTimerInterval);

    m_targetMode.clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
    resolveModes();

    return false;
}

NO_RETURN_DUE_TO_ASSERT void VideoPresentationInterfaceIOS::watchdogTimerFired()
{
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "no exit fullscreen response in ", defaultWatchdogTimerInterval.value(), "s; forcing fullscreen hidden.");
    ASSERT_NOT_REACHED();
    tearDownWindow();
}

void VideoPresentationInterfaceIOS::setHasVideoContentLayer(bool value)
{
    UNUSED_PARAM(value);
}

void VideoPresentationInterfaceIOS::preparedToReturnToStandby()
{
    if (!m_returningToStandby)
        return;

    returnToStandby();
}

void VideoPresentationInterfaceIOS::finalizeSetup()
{
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

    returnVideoViewToInline();

    // Continue processing exit picture-in-picture now that
    // it is safe to do so:
    didStopPictureInPicture();
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
