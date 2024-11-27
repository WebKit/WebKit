/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "VideoPresentationInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "IntRect.h"
#import "Logging.h"
#import "PictureInPictureSupport.h"
#import "PlaybackSessionInterfaceMac.h"
#import "TimeRanges.h"
#import "VideoPresentationModel.h"
#import "WebAVPlayerLayer.h"
#import "WebPlaybackControlsManager.h"
#import <AVFoundation/AVTime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/mac/PIPSPI.h>
#import <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(PIP)
SOFT_LINK_CLASS_OPTIONAL(PIP, PIPViewController)

@class WebVideoViewContainer;

@protocol WebVideoViewContainerDelegate <NSObject>

- (void)boundsDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer;
- (void)superviewDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer;

@end

using WebCore::VideoPresentationModel;
using WebCore::HTMLMediaElementEnums;
using WebCore::MediaPlayerEnums;
using WebCore::VideoPresentationInterfaceMac;
using WebCore::PlaybackSessionModel;

@interface WebVideoViewContainer : NSView
@property (nonatomic, weak) id<WebVideoViewContainerDelegate> videoViewContainerDelegate;
@end

@implementation WebVideoViewContainer

- (void)resizeWithOldSuperviewSize:(NSSize)oldBoundsSize
{
    [super resizeWithOldSuperviewSize:oldBoundsSize];

    [_videoViewContainerDelegate boundsDidChangeForVideoViewContainer:self];
}

- (void)viewDidMoveToSuperview
{
    [super viewDidMoveToSuperview];

    [_videoViewContainerDelegate superviewDidChangeForVideoViewContainer:self];
}

@end

enum class PIPState {
    NotInPIP,
    EnteringPIP,
    InPIP,
    ExitingPIP
};

@interface WebVideoPresentationInterfaceMacObjC : NSObject <PIPViewControllerDelegate, WebVideoViewContainerDelegate> {
    WebCore::VideoPresentationInterfaceMac* _videoPresentationInterfaceMac;
    NSSize _videoDimensions;
    RetainPtr<PIPViewController> _pipViewController;
    RetainPtr<NSViewController> _videoViewContainerController;
    RetainPtr<WebVideoViewContainer> _videoViewContainer;
    RetainPtr<WebAVPlayerLayer> _playerLayer;
    PIPState _pipState;
    RetainPtr<NSWindow> _returningWindow;
    NSRect _returningRect;
    BOOL _playing;
    BOOL _exitingToStandardFullscreen;
}

- (instancetype)initWithVideoPresentationInterfaceMac:(WebCore::VideoPresentationInterfaceMac*)videoPresentationInterfaceMac;
- (void)invalidateFullscreenState;
- (void)invalidate;

// Tracking video playback state
@property (nonatomic) NSSize videoDimensions;
@property (nonatomic, getter=isPlaying) BOOL playing;
- (void)updateIsPlaying:(BOOL)isPlaying newPlaybackRate:(float)playbackRate;

// Handling PIP transitions
@property (nonatomic, getter=isExitingToStandardFullscreen) BOOL exitingToStandardFullscreen;

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window;
- (void)enterPIP;
- (void)exitPIP;
- (void)exitPIPAnimatingToRect:(NSRect)rect inWindow:(NSWindow *)window;

@end

@implementation WebVideoPresentationInterfaceMacObjC

@synthesize playing = _playing;
@synthesize videoDimensions = _videoDimensions;
@synthesize exitingToStandardFullscreen = _exitingToStandardFullscreen;

- (instancetype)initWithVideoPresentationInterfaceMac:(WebCore::VideoPresentationInterfaceMac*)videoPresentationInterfaceMac
{
    if (!(self = [super init]))
        return nil;

    _videoPresentationInterfaceMac = videoPresentationInterfaceMac;
    _pipState = PIPState::NotInPIP;

    return self;
}

- (void)invalidateFullscreenState
{
    [_pipViewController setDelegate:nil];
    _pipViewController = nil;
    [_videoViewContainer removeFromSuperview];
    [_videoViewContainer setVideoViewContainerDelegate:nil];
    _videoViewContainer = nil;
    _videoViewContainerController = nil;
    _playerLayer = nil;
    _pipState = PIPState::NotInPIP;
    _exitingToStandardFullscreen = NO;
    _returningWindow = nil;
    _returningRect = NSZeroRect;
}

- (void)invalidate
{
    [self invalidateFullscreenState];
    _videoPresentationInterfaceMac = nullptr;
    _videoDimensions = NSZeroSize;
}

- (void)updateIsPlaying:(BOOL)isPlaying newPlaybackRate:(float)playbackRate
{
    _playing = isPlaying && playbackRate;

    [_pipViewController setPlaying:_playing];
}

- (void)setVideoDimensions:(NSSize)videoDimensions
{
    _videoDimensions = videoDimensions;

    [_playerLayer setVideoDimensions:_videoDimensions];
    [_pipViewController setAspectRatio:_videoDimensions];
}

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window
{
    ASSERT(!_pipViewController);
    ASSERT(!_videoViewContainerController);
    ASSERT(!_videoViewContainer);
    ASSERT(!_playerLayer);

    _pipViewController = adoptNS([allocPIPViewControllerInstance() init]);
    [_pipViewController setDelegate:self];
    [_pipViewController setUserCanResize:YES];
    [_pipViewController setPlaying:_playing];
    [self setVideoDimensions:NSEqualSizes(_videoDimensions, NSZeroSize) ? frame.size : _videoDimensions];
    auto model = _videoPresentationInterfaceMac ? _videoPresentationInterfaceMac->videoPresentationModel() : nullptr;
    if (model)
        model->setVideoLayerGravity(MediaPlayerEnums::VideoGravity::ResizeAspectFill);

    _videoViewContainer = adoptNS([[WebVideoViewContainer alloc] initWithFrame:frame]);
    [_videoViewContainer setVideoViewContainerDelegate:self];
    [_videoViewContainer setLayer:[CALayer layer]];
    [_videoViewContainer setWantsLayer:YES];
    [_videoViewContainer setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

    _playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);
    [[_videoViewContainer layer] addSublayer:_playerLayer.get()];
    [_playerLayer setFrame:[_videoViewContainer layer].bounds];
    [_playerLayer setPresentationModel:model.get()];
    [_playerLayer setVideoSublayer:videoView.layer];
    [_playerLayer setVideoDimensions:_videoDimensions];
    [_playerLayer setAutoresizingMask:(kCALayerWidthSizable | kCALayerHeightSizable)];

    [videoView.layer removeFromSuperlayer];
    [_playerLayer addSublayer:videoView.layer];

    _videoViewContainerController = adoptNS([[NSViewController alloc] init]);
    [_videoViewContainerController setView:_videoViewContainer.get()];
    [window.contentView addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];
}

- (void)enterPIP
{
    if (_pipState == PIPState::EnteringPIP || _pipState == PIPState::InPIP)
        return;

    [_videoViewContainerController view].layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    [_pipViewController presentViewControllerAsPictureInPicture:_videoViewContainerController.get()];
    _pipState = PIPState::EnteringPIP;
}

- (void)exitPIP
{
    if (_pipState != PIPState::InPIP || !_pipViewController || !_videoViewContainerController)
        return;

    [_videoViewContainerController view].layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
    [_pipViewController dismissViewController:_videoViewContainerController.get()];
    _pipState = PIPState::ExitingPIP;
}

- (void)exitPIPAnimatingToRect:(NSRect)rect inWindow:(NSWindow *)window
{
    _returningWindow = window;
    _returningRect = rect;

    [_pipViewController setReplacementRect:rect];
    [_pipViewController setReplacementWindow:window];

    [self exitPIP];
}

// WebVideoViewContainerDelegate

- (void)boundsDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer
{
    if (!_videoViewContainer || !_pipViewController)
        return;

    ASSERT_UNUSED(videoViewContainer, videoViewContainer == _videoViewContainer);

    if (!_videoPresentationInterfaceMac)
        return;

    if (_pipState == PIPState::EnteringPIP) {
        // FIXME(rdar://problem/42250952)
        // Currently, -[PIPViewController presentViewControllerAsPictureInPicture:] does not
        // take a completionHandler parameter, so we use the first bounds change event
        // as an indication that entering picture-in-picture is completed.
        _pipState = PIPState::InPIP;

        if (auto model = _videoPresentationInterfaceMac->videoPresentationModel()) {
            model->didEnterPictureInPicture();
            model->didEnterFullscreen((WebCore::FloatSize)[_videoViewContainer bounds].size);
        }
    }
}

- (void)superviewDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer
{
    if (!_videoViewContainer || !_pipViewController)
        return;

    ASSERT(videoViewContainer == _videoViewContainer);

    if (![videoViewContainer isDescendantOf:[_pipViewController view]])
        return;

    // Once the view is moved into the pip view, make sure it resizes with the pip view.
    videoViewContainer.frame = [videoViewContainer superview].bounds;
    videoViewContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
}

// PIPViewControllerDelegate

- (BOOL)pipShouldClose:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (!_videoPresentationInterfaceMac)
        return YES;
    
    if (auto model = _videoPresentationInterfaceMac->videoPresentationModel())
        model->fullscreenMayReturnToInline();

    _videoPresentationInterfaceMac->requestHideAndExitPiP();

    return NO;
}

- (void)pipDidClose:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (!_videoPresentationInterfaceMac)
        return;

    if (_pipState != PIPState::ExitingPIP) {
        // We got told to close without going through -pipActionStop, nor by exlicitly being asked to in -exitPiP:.
        // Call -pipActionStop: here in order to set the fullscreen state to an expected value.
        [self pipActionStop:pip];
    }

    if (auto model = _videoPresentationInterfaceMac->videoPresentationModel()) {
        if (_videoViewContainer && _returningWindow && !NSEqualRects(_returningRect, NSZeroRect)) {
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
                context.allowsImplicitAnimation = NO;
                [_videoViewContainer setFrame:_returningRect];
                [[_returningWindow contentView] addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];
            } completionHandler:nil];
        }

        if (!self.isExitingToStandardFullscreen) {
            model->didExitPictureInPicture();
            model->setVideoLayerGravity(MediaPlayerEnums::VideoGravity::ResizeAspect);
        }

        model->didExitFullscreen();
        model->setRequiresTextTrackRepresentation(false);
    }

    _videoPresentationInterfaceMac->clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
}

- (void)pipActionPlay:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (_videoPresentationInterfaceMac && _videoPresentationInterfaceMac->playbackSessionModel())
        _videoPresentationInterfaceMac->playbackSessionModel()->play();
}

- (void)pipActionPause:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (_videoPresentationInterfaceMac && _videoPresentationInterfaceMac->playbackSessionModel())
        _videoPresentationInterfaceMac->playbackSessionModel()->pause();
}

- (void)pipActionStop:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (!_videoPresentationInterfaceMac)
        return;

    if (PlaybackSessionModel* playbackSessionModel = _videoPresentationInterfaceMac->playbackSessionModel())
        playbackSessionModel->pause();

    _videoPresentationInterfaceMac->requestHideAndExitPiP();
    _pipState = PIPState::ExitingPIP;
}

@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceMac);

VideoPresentationInterfaceMac::VideoPresentationInterfaceMac(PlaybackSessionInterfaceMac& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
{
    ASSERT(m_playbackSessionInterface->playbackSessionModel());
    auto model = m_playbackSessionInterface->playbackSessionModel();
    model->addClient(*this);
    [videoPresentationInterfaceObjC() updateIsPlaying:model->isPlaying() newPlaybackRate:model->playbackRate()];
}

VideoPresentationInterfaceMac::~VideoPresentationInterfaceMac()
{
    if (auto* model = m_playbackSessionInterface->playbackSessionModel())
        model->removeClient(*this);
    if (auto model = videoPresentationModel())
        model->removeClient(*this);
}

void VideoPresentationInterfaceMac::setVideoPresentationModel(VideoPresentationModel* model)
{
    if (auto model = videoPresentationModel())
        model->removeClient(*this);
    m_videoPresentationModel = model;
    if (model)
        model->addClient(*this);
}

void VideoPresentationInterfaceMac::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;

    RefPtr model = videoPresentationModel();
    if (model)
        model->setRequiresTextTrackRepresentation(hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture));

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && !isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        return;

    if (model)
        model->fullscreenModeChanged(m_mode);
}

void VideoPresentationInterfaceMac::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;

    RefPtr model = videoPresentationModel();
    if (model)
        model->setRequiresTextTrackRepresentation(hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture));

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && !isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        return;

    if (model)
        model->fullscreenModeChanged(m_mode);
}

void VideoPresentationInterfaceMac::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double /* defaultPlaybackRate */)
{
    [videoPresentationInterfaceObjC() updateIsPlaying:playbackState.contains(PlaybackSessionModel::PlaybackState::Playing) newPlaybackRate:playbackRate];
}

void VideoPresentationInterfaceMac::ensureControlsManager()
{
    m_playbackSessionInterface->ensureControlsManager();
}

WebVideoPresentationInterfaceMacObjC *VideoPresentationInterfaceMac::videoPresentationInterfaceObjC()
{
    if (!m_webVideoPresentationInterfaceObjC)
        m_webVideoPresentationInterfaceObjC = adoptNS([[WebVideoPresentationInterfaceMacObjC alloc] initWithVideoPresentationInterfaceMac:this]);

    return m_webVideoPresentationInterfaceObjC.get();
}

void VideoPresentationInterfaceMac::setupFullscreen(const IntRect& initialRect, NSWindow *parentWindow, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::setupFullscreen(%p), initialRect:{%d, %d, %d, %d}, parentWindow:%p, mode:%d", this, initialRect.x(), initialRect.y(), initialRect.width(), initialRect.height(), parentWindow, mode);

    UNUSED_PARAM(allowsPictureInPicturePlayback);
    ASSERT(mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);

    m_mode |= mode;

    [videoPresentationInterfaceObjC() setUpPIPForVideoView:layerHostView() withFrame:(NSRect)initialRect inWindow:parentWindow];

    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (RefPtr model = videoPresentationModel()) {
            model->didSetupFullscreen();
            model->setRequiresTextTrackRepresentation(true);
        }
    });
}

void VideoPresentationInterfaceMac::enterFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::enterFullscreen(%p)", this);

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        if (auto model = videoPresentationModel())
            model->willEnterPictureInPicture();
        [m_webVideoPresentationInterfaceObjC enterPIP];

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
        [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:YES];
#endif
    }
}

bool VideoPresentationInterfaceMac::exitFullscreen(const IntRect& finalRect, NSWindow *parentWindow)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::exitFullscreen(%p), finalRect:{%d, %d, %d, %d}, parentWindow:%p", this, finalRect.x(), finalRect.y(), finalRect.width(), finalRect.height(), parentWindow);

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:NO];
#endif

    if (finalRect.isEmpty())
        [m_webVideoPresentationInterfaceObjC exitPIP];
    else
        [m_webVideoPresentationInterfaceObjC exitPIPAnimatingToRect:finalRect inWindow:parentWindow];

    return true;
}

void VideoPresentationInterfaceMac::exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::exitFullscreenWithoutAnimationToMode(%p), mode:%d", this, mode);

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:NO];
#endif

    bool isExitingToStandardFullscreen = mode == HTMLMediaElementEnums::VideoFullscreenModeStandard;
    // On Mac, standard fullscreen is handled by the Fullscreen API and not by VideoPresentationManager.
    // Just update m_mode directly to HTMLMediaElementEnums::VideoFullscreenModeStandard in that case to keep
    // m_mode in sync with the fullscreen mode in HTMLMediaElement.
    if (isExitingToStandardFullscreen)
        m_mode = HTMLMediaElementEnums::VideoFullscreenModeStandard;

    [m_webVideoPresentationInterfaceObjC setExitingToStandardFullscreen:isExitingToStandardFullscreen];
    [m_webVideoPresentationInterfaceObjC exitPIP];
}

void VideoPresentationInterfaceMac::cleanupFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::cleanupFullscreen(%p)", this);

    [m_webVideoPresentationInterfaceObjC exitPIP];
    [m_webVideoPresentationInterfaceObjC invalidateFullscreenState];

    if (auto model = videoPresentationModel())
        model->didCleanupFullscreen();
}

void VideoPresentationInterfaceMac::invalidate()
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::invalidate(%p)", this);

    m_videoPresentationModel = nullptr;

    cleanupFullscreen();

    [m_webVideoPresentationInterfaceObjC invalidate];
    m_webVideoPresentationInterfaceObjC = nil;
}

void VideoPresentationInterfaceMac::requestHideAndExitPiP()
{
    auto model = videoPresentationModel();
    if (!model)
        return;

    if (m_documentIsVisible) {
        model->requestFullscreenMode(m_mode & ~HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        model->willExitPictureInPicture();
    } else {
        auto callback = [this, model] () {
            model->requestFullscreenMode(m_mode & ~HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
            model->willExitPictureInPicture();
        };
        setDocumentBecameVisibleCallback(WTFMove(callback));
    }

}

void VideoPresentationInterfaceMac::documentVisibilityChanged(bool isDocumentVisible)
{
    bool documentWasVisible = m_documentIsVisible;
    m_documentIsVisible = isDocumentVisible;

    if (!documentWasVisible && m_documentIsVisible && m_documentBecameVisibleCallback) {
        m_documentBecameVisibleCallback();
        m_documentBecameVisibleCallback = nullptr;
    }
}

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

void VideoPresentationInterfaceMac::preparedToReturnToInline(bool visible, const IntRect& inlineRect, NSWindow *parentWindow)
{
    UNUSED_PARAM(visible);
    UNUSED_PARAM(inlineRect);
    UNUSED_PARAM(parentWindow);
}

void VideoPresentationInterfaceMac::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::externalPlaybackChanged(%p), enabled:%s", this, boolString(enabled));

    if (enabled && m_mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        exitFullscreen(IntRect(), nil);
}

void VideoPresentationInterfaceMac::hasVideoChanged(bool hasVideo)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::hasVideoChanged(%p):%s", this, boolString(hasVideo));

    if (!hasVideo)
        exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void VideoPresentationInterfaceMac::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::videoDimensionsChanged(%p), width:%.0f, height:%.0f", this, videoDimensions.width(), videoDimensions.height());

    // Width and height can be zero when we are transitioning from one video to another. Ignore zero values.
    if (!videoDimensions.isZero())
        [m_webVideoPresentationInterfaceObjC setVideoDimensions:videoDimensions];
}

bool VideoPresentationInterfaceMac::isPlayingVideoInEnhancedFullscreen() const
{
    return hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && [m_webVideoPresentationInterfaceObjC isPlaying];
}

#if !RELEASE_LOG_DISABLED
uint64_t VideoPresentationInterfaceMac::logIdentifier() const
{
    return m_playbackSessionInterface->logIdentifier();
}

const Logger* VideoPresentationInterfaceMac::loggerPtr() const
{
    return m_playbackSessionInterface->loggerPtr();
}

WTFLogChannel& VideoPresentationInterfaceMac::logChannel() const
{
    return LogFullscreen;
}
#endif

bool supportsPictureInPicture()
{
    return PIPLibrary() && getPIPViewControllerClass();
}

}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
