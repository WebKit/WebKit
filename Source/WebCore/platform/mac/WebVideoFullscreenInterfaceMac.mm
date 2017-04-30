/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "WebVideoFullscreenInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "AVKitSPI.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaTimeAVFoundation.h"
#import "PIPSPI.h"
#import "TimeRanges.h"
#import "WebPlaybackControlsManager.h"
#import "WebPlaybackSessionInterfaceMac.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(PIP)
SOFT_LINK_CLASS_OPTIONAL(PIP, PIPViewController)

using namespace WebCore;

@class WebVideoViewContainer;

@protocol WebVideoViewContainerDelegate <NSObject>

- (void)boundsDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer;
- (void)superviewDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer;

@end

@interface WebVideoViewContainer : NSView {
    id <WebVideoViewContainerDelegate> _videoViewContainerDelegate;
}

@property (nonatomic, assign) id <WebVideoViewContainerDelegate> videoViewContainerDelegate;

@end

@implementation WebVideoViewContainer

@synthesize videoViewContainerDelegate=_videoViewContainerDelegate;

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
    InPIP,
    ExitingPIP
};

@interface WebVideoFullscreenInterfaceMacObjC : NSObject <PIPViewControllerDelegate, WebVideoViewContainerDelegate> {
    WebCore::WebVideoFullscreenInterfaceMac* _webVideoFullscreenInterfaceMac;
    NSSize _videoDimensions;
    RetainPtr<PIPViewController> _pipViewController;
    RetainPtr<NSViewController> _videoViewContainerController;
    RetainPtr<WebVideoViewContainer> _videoViewContainer;
    PIPState _pipState;
    RetainPtr<NSWindow> _returningWindow;
    NSRect _returningRect;
    BOOL _playing;
    BOOL _didRequestExitingPIP;
    BOOL _exitingToStandardFullscreen;
}

- (instancetype)initWithWebVideoFullscreenInterfaceMac:(WebCore::WebVideoFullscreenInterfaceMac*)webVideoFullscreenInterfaceMac;
- (void)invalidateFullscreenState;
- (void)invalidate;

// Tracking video playback state
@property (nonatomic) NSSize videoDimensions;
@property (nonatomic, getter=isPlaying) BOOL playing;
- (void)updateIsPlaying:(BOOL)isPlaying newPlaybackRate:(float)playbackRate;

// Handling PIP transitions
@property (nonatomic, readonly) BOOL didRequestExitingPIP;
@property (nonatomic, getter=isExitingToStandardFullscreen) BOOL exitingToStandardFullscreen;

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window;
- (void)enterPIP;
- (void)exitPIP;
- (void)exitPIPAnimatingToRect:(NSRect)rect inWindow:(NSWindow *)window;

@end

@implementation WebVideoFullscreenInterfaceMacObjC

@synthesize playing=_playing;
@synthesize videoDimensions=_videoDimensions;
@synthesize didRequestExitingPIP=_didRequestExitingPIP;
@synthesize exitingToStandardFullscreen=_exitingToStandardFullscreen;

- (instancetype)initWithWebVideoFullscreenInterfaceMac:(WebCore::WebVideoFullscreenInterfaceMac*)webVideoFullscreenInterfaceMac
{
    if (!(self = [super init]))
        return nil;

    _webVideoFullscreenInterfaceMac = webVideoFullscreenInterfaceMac;
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
    _pipState = PIPState::NotInPIP;
    _didRequestExitingPIP = NO;
    _exitingToStandardFullscreen = NO;
    _returningWindow = nil;
    _returningRect = NSZeroRect;
}

- (void)invalidate
{
    [self invalidateFullscreenState];
    _webVideoFullscreenInterfaceMac = nullptr;
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

    [_pipViewController setAspectRatio:_videoDimensions];
}

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window
{
    ASSERT(!_pipViewController);
    ASSERT(!_videoViewContainerController);
    ASSERT(!_videoViewContainer);

    _pipViewController = adoptNS([allocPIPViewControllerInstance() init]);
    [_pipViewController setDelegate:self];
    [_pipViewController setUserCanResize:YES];
    [_pipViewController setPlaying:_playing];
    [self setVideoDimensions:NSEqualSizes(_videoDimensions, NSZeroSize) ? frame.size : _videoDimensions];
    if (_webVideoFullscreenInterfaceMac && _webVideoFullscreenInterfaceMac->webVideoFullscreenModel())
        _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()->setVideoLayerGravity(WebVideoFullscreenModel::VideoGravityResizeAspectFill);

    _videoViewContainer = adoptNS([[WebVideoViewContainer alloc] initWithFrame:frame]);
    [_videoViewContainer setVideoViewContainerDelegate:self];
    [_videoViewContainer addSubview:videoView];
    videoView.frame = [_videoViewContainer bounds];
    videoView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _videoViewContainerController = adoptNS([[NSViewController alloc] init]);
    [_videoViewContainerController setView:_videoViewContainer.get()];
    [window.contentView addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];
}

- (void)enterPIP
{
    if (_pipState == PIPState::InPIP)
        return;

    [_videoViewContainerController view].layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    [_pipViewController presentViewControllerAsPictureInPicture:_videoViewContainerController.get()];
    _pipState = PIPState::InPIP;
}

- (void)exitPIP
{
    if (_pipState != PIPState::InPIP || !_pipViewController || !_videoViewContainerController)
        return;

    _didRequestExitingPIP = YES;
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

    if (_webVideoFullscreenInterfaceMac && _webVideoFullscreenInterfaceMac->webVideoFullscreenModel())
        _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()->setVideoLayerFrame([_videoViewContainer bounds]);
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

    if (!_webVideoFullscreenInterfaceMac || !_webVideoFullscreenInterfaceMac->webVideoFullscreenChangeObserver())
        return YES;

    _didRequestExitingPIP = YES;
    _webVideoFullscreenInterfaceMac->webVideoFullscreenChangeObserver()->fullscreenMayReturnToInline();

    return NO;
}

- (void)pipDidClose:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (_webVideoFullscreenInterfaceMac && _webVideoFullscreenInterfaceMac->webVideoFullscreenModel() && _videoViewContainer && _returningWindow && !NSEqualRects(_returningRect, NSZeroRect)) {
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
            context.allowsImplicitAnimation = NO;
            [_videoViewContainer setFrame:_returningRect];
            _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()->setVideoLayerFrame([_videoViewContainer bounds]);
            _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()->setVideoLayerGravity(WebVideoFullscreenModel::VideoGravityResizeAspect);

            [[_returningWindow contentView] addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];
        } completionHandler:nil];
    }

    if (_webVideoFullscreenInterfaceMac) {
        if (!self.isExitingToStandardFullscreen) {
            if (WebVideoFullscreenModel* videoFullscreenModel = _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()) {
                videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
                videoFullscreenModel->setVideoLayerGravity(WebVideoFullscreenModel::VideoGravityResizeAspect);
            }
        }

        _webVideoFullscreenInterfaceMac->clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);

        if (WebVideoFullscreenChangeObserver* fullscreenChangeObserver = _webVideoFullscreenInterfaceMac->webVideoFullscreenChangeObserver())
            fullscreenChangeObserver->didExitFullscreen();
    }
}

- (void)pipActionPlay:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (_webVideoFullscreenInterfaceMac && _webVideoFullscreenInterfaceMac->webPlaybackSessionModel())
        _webVideoFullscreenInterfaceMac->webPlaybackSessionModel()->play();
}

- (void)pipActionPause:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (_webVideoFullscreenInterfaceMac && _webVideoFullscreenInterfaceMac->webPlaybackSessionModel())
        _webVideoFullscreenInterfaceMac->webPlaybackSessionModel()->pause();
}

- (void)pipActionStop:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    if (!_webVideoFullscreenInterfaceMac)
        return;

    if (WebPlaybackSessionModel* playbackSessionModel = _webVideoFullscreenInterfaceMac->webPlaybackSessionModel())
        playbackSessionModel->pause();

    // FIXME 25096170: Should animate only if the page with the video is unobscured. For now, always close without animation.
    [self exitPIP];
}

@end

namespace WebCore {

WebVideoFullscreenInterfaceMac::WebVideoFullscreenInterfaceMac(WebPlaybackSessionInterfaceMac& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
{
    ASSERT(m_playbackSessionInterface->webPlaybackSessionModel());
    auto model = m_playbackSessionInterface->webPlaybackSessionModel();
    model->addClient(*this);
    [videoFullscreenInterfaceObjC() updateIsPlaying:model->isPlaying() newPlaybackRate:model->playbackRate()];
}

WebVideoFullscreenInterfaceMac::~WebVideoFullscreenInterfaceMac()
{
    if (m_playbackSessionInterface->webPlaybackSessionModel())
        m_playbackSessionInterface->webPlaybackSessionModel()->removeClient(*this);
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);
    m_videoFullscreenModel = model;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->addClient(*this);
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceMac::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceMac::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceMac::rateChanged(bool isPlaying, float playbackRate)
{
    [videoFullscreenInterfaceObjC() updateIsPlaying:isPlaying newPlaybackRate:playbackRate];
}

void WebVideoFullscreenInterfaceMac::ensureControlsManager()
{
    m_playbackSessionInterface->ensureControlsManager();
}

WebVideoFullscreenInterfaceMacObjC *WebVideoFullscreenInterfaceMac::videoFullscreenInterfaceObjC()
{
    if (!m_webVideoFullscreenInterfaceObjC)
        m_webVideoFullscreenInterfaceObjC = adoptNS([[WebVideoFullscreenInterfaceMacObjC alloc] initWithWebVideoFullscreenInterfaceMac:this]);

    return m_webVideoFullscreenInterfaceObjC.get();
}

void WebVideoFullscreenInterfaceMac::setupFullscreen(NSView& layerHostedView, const IntRect& initialRect, NSWindow *parentWindow, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::setupFullscreen(%p), initialRect:{%d, %d, %d, %d}, parentWindow:%p, mode:%d", this, initialRect.x(), initialRect.y(), initialRect.width(), initialRect.height(), parentWindow, mode);

    UNUSED_PARAM(allowsPictureInPicturePlayback);
    ASSERT(mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);

    m_mode = mode;

    [videoFullscreenInterfaceObjC() setUpPIPForVideoView:&layerHostedView withFrame:(NSRect)initialRect inWindow:parentWindow];

    RefPtr<WebVideoFullscreenInterfaceMac> protectedThis(this);
    dispatch_async(dispatch_get_main_queue(), [protectedThis, this] {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didSetupFullscreen();
    });
}

void WebVideoFullscreenInterfaceMac::enterFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::enterFullscreen(%p)", this);

    if (mode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) {
        [m_webVideoFullscreenInterfaceObjC enterPIP];

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
        [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:YES];
#endif

        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didEnterFullscreen();
    }
}

void WebVideoFullscreenInterfaceMac::exitFullscreen(const IntRect& finalRect, NSWindow *parentWindow)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::exitFullscreen(%p), finalRect:{%d, %d, %d, %d}, parentWindow:%p", this, finalRect.x(), finalRect.y(), finalRect.width(), finalRect.height(), parentWindow);

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:NO];
#endif

    if ([m_webVideoFullscreenInterfaceObjC didRequestExitingPIP])
        return;

    if (finalRect.isEmpty())
        [m_webVideoFullscreenInterfaceObjC exitPIP];
    else
        [m_webVideoFullscreenInterfaceObjC exitPIPAnimatingToRect:finalRect inWindow:parentWindow];
}

void WebVideoFullscreenInterfaceMac::exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::exitFullscreenWithoutAnimationToMode(%p), mode:%d", this, mode);

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [m_playbackSessionInterface->playBackControlsManager() setPictureInPictureActive:NO];
#endif

    if ([m_webVideoFullscreenInterfaceObjC didRequestExitingPIP])
        return;

    bool isExitingToStandardFullscreen = mode == HTMLMediaElementEnums::VideoFullscreenModeStandard;
    // On Mac, standard fullscreen is handled by the Fullscreen API and not by WebVideoFullscreenManager.
    // Just update m_mode directly to HTMLMediaElementEnums::VideoFullscreenModeStandard in that case to keep
    // m_mode in sync with the fullscreen mode in HTMLMediaElement.
    if (isExitingToStandardFullscreen)
        m_mode = HTMLMediaElementEnums::VideoFullscreenModeStandard;

    [m_webVideoFullscreenInterfaceObjC setExitingToStandardFullscreen:isExitingToStandardFullscreen];
    [m_webVideoFullscreenInterfaceObjC exitPIP];
}

void WebVideoFullscreenInterfaceMac::cleanupFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::cleanupFullscreen(%p)", this);

    [m_webVideoFullscreenInterfaceObjC exitPIP];
    [m_webVideoFullscreenInterfaceObjC invalidateFullscreenState];

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didCleanupFullscreen();
}

void WebVideoFullscreenInterfaceMac::invalidate()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::invalidate(%p)", this);

    m_videoFullscreenModel = nil;
    m_fullscreenChangeObserver = nil;

    cleanupFullscreen();

    [m_webVideoFullscreenInterfaceObjC invalidate];
    m_webVideoFullscreenInterfaceObjC = nil;
}

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

void WebVideoFullscreenInterfaceMac::preparedToReturnToInline(bool visible, const IntRect& inlineRect, NSWindow *parentWindow)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::preparedToReturnToInline(%p), visible:%s, inlineRect:{%d, %d, %d, %d}, parentWindow:%p", this, boolString(visible), inlineRect.x(), inlineRect.y(), inlineRect.width(), inlineRect.height(), parentWindow);

    if (!visible) {
        [m_webVideoFullscreenInterfaceObjC exitPIP];
        return;
    }

    ASSERT(parentWindow);
    [m_webVideoFullscreenInterfaceObjC exitPIPAnimatingToRect:(NSRect)inlineRect inWindow:parentWindow];
}

void WebVideoFullscreenInterfaceMac::externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::externalPlaybackChanged(%p), enabled:%s", this, boolString(enabled));

    if (enabled && m_mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        exitFullscreen(IntRect(), nil);
}

void WebVideoFullscreenInterfaceMac::hasVideoChanged(bool hasVideo)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::hasVideoChanged(%p):%s", this, boolString(hasVideo));

    if (!hasVideo)
        exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void WebVideoFullscreenInterfaceMac::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceMac::videoDimensionsChanged(%p), width:%.0f, height:%.0f", this, videoDimensions.width(), videoDimensions.height());

    // Width and height can be zero when we are transitioning from one video to another. Ignore zero values.
    if (!videoDimensions.isZero())
        [m_webVideoFullscreenInterfaceObjC setVideoDimensions:videoDimensions];
}

bool WebVideoFullscreenInterfaceMac::isPlayingVideoInEnhancedFullscreen() const
{
    return hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && [m_webVideoFullscreenInterfaceObjC isPlaying];
}

bool supportsPictureInPicture()
{
    return PIPLibrary() && getPIPViewControllerClass();
}

}

#endif
