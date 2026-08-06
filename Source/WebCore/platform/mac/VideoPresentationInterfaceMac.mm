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
#import <pal/cocoa/AVFoundationSoftLink.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(AVKit, AVValueTiming)

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(PIP)
SOFT_LINK_CLASS_OPTIONAL(PIP, PIPViewController)
SOFT_LINK_CLASS_OPTIONAL(PIP, PIPPrerollAttributes)

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

    [retainPtr(_videoViewContainerDelegate) boundsDidChangeForVideoViewContainer:self];
}

- (void)viewDidMoveToSuperview
{
    [super viewDidMoveToSuperview];

    [retainPtr(_videoViewContainerDelegate) superviewDidChangeForVideoViewContainer:self];
}

@end

enum class PIPState {
    NotInPIP,
    EnteringPIP,
    InPIP,
    ExitingPIP
};

@interface WebVideoPresentationInterfaceMacObjC : NSObject <PIPViewControllerDelegate, WebVideoViewContainerDelegate> {
    CheckedPtr<WebCore::VideoPresentationInterfaceMac> _videoPresentationInterfaceMac;
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
    double _rate;
    AVPlayerTimeControlStatus _timeControlStatus;
    NSTimeInterval _duration;
    NSTimeInterval _anchorTime;
    NSTimeInterval _elapsedTime;
}

- (instancetype)initWithVideoPresentationInterfaceMac:(WebCore::VideoPresentationInterfaceMac*)videoPresentationInterfaceMac;
- (void)invalidateFullscreenState;
- (void)invalidate;

// Tracking video playback state
@property (nonatomic) NSSize videoDimensions;
@property (nonatomic, getter=isPlaying) BOOL playing;
#if HAVE(PIP_SKIP_PREROLL)
@property (readonly, getter=isPlaybackStateEnabled) BOOL playbackStateEnabled;
@property (nonatomic) BOOL canSkipAd;
- (void)updateCanSkipAd:(BOOL)canSkipAd;
#endif
// Handling PIP transitions
@property (nonatomic, getter=isExitingToStandardFullscreen) BOOL exitingToStandardFullscreen;

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window;
- (void)enterPIP;
- (void)exitPIP;
- (void)exitPIPAnimatingToRect:(NSRect)rect inWindow:(NSWindow *)window;
@property (readonly, nonatomic) NSTimeInterval estimatedElapsedTime;
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

- (void)updateRate:(double)rate andTimeControlStatus:(AVPlayerTimeControlStatus)timeControlStatus
{
    if (_rate == rate
        && _timeControlStatus == timeControlStatus)
        return;

    _elapsedTime = self.estimatedElapsedTime;
    _anchorTime = [[NSProcessInfo processInfo] systemUptime];

    _rate = rate;
    _timeControlStatus = timeControlStatus;
    _playing = (timeControlStatus == AVPlayerTimeControlStatusPlaying) && _rate;

#if HAVE(PIP_SKIP_PREROLL)
    if (self.isPlaybackStateEnabled) {
        [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
            [playbackState setPlaybackRate:_rate elapsedTime:_elapsedTime timeControlStatus:_timeControlStatus];
        }];
        return;
    }
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_pipViewController setPlaying:_playing];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)setVideoDimensions:(NSSize)videoDimensions
{
    _videoDimensions = videoDimensions;

    [_playerLayer setVideoDimensions:_videoDimensions];
    [_pipViewController setAspectRatio:_videoDimensions];
}
#if HAVE(PIP_SKIP_PREROLL)
- (BOOL)isPlaybackStateEnabled
{
    return _videoPresentationInterfaceMac && _videoPresentationInterfaceMac->isPlaybackStateEnabled();
}

- (void)updateCanSkipAd:(BOOL)canSkipAd
{
    if (canSkipAd == _canSkipAd)
        return;
    _canSkipAd = canSkipAd;
    [self updatePrerollAttributes];
}
- (void)updatePrerollAttributes
{
    if (!_pipViewController)
        return;

    [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
        if (!_canSkipAd)
            playbackState.prerollAttributes = nil;
        else
            playbackState.prerollAttributes = [getPIPPrerollAttributesClassSingleton() prerollAttributesForAdContentWithRequiredLinearPlaybackEndTime:0 preferredTintColor:nil];
    }];
}
#endif

- (void)setDuration:(NSTimeInterval)duration
{
    if (duration == _duration)
        return;

    _duration = duration;

#if HAVE(PIP_SKIP_PREROLL)
    if (!self.isPlaybackStateEnabled)
        return;

    [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
        playbackState.contentDuration = _duration;
    }];
#endif
}

- (void)updateRate:(double)rate currentTime:(NSTimeInterval)currentTime atAnchorTime:(NSTimeInterval)anchorTime
{
    if (rate == _rate
        && currentTime == _elapsedTime
        && anchorTime == _anchorTime)
        return;

    _rate = rate;
    _elapsedTime = currentTime;
    _anchorTime = anchorTime;

#if HAVE(PIP_SKIP_PREROLL)
    if (!self.isPlaybackStateEnabled)
        return;

    [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
        [playbackState setPlaybackRate:_rate elapsedTime:_elapsedTime timeControlStatus:_timeControlStatus];
    }];
#endif
}

- (void)updateCurrentTime:(NSTimeInterval)currentTime atAnchorTime:(NSTimeInterval)anchorTime
{
    if (currentTime == _elapsedTime
        && anchorTime == _anchorTime)
        return;

    _elapsedTime = currentTime;
    _anchorTime = anchorTime;

#if HAVE(PIP_SKIP_PREROLL)
    if (!self.isPlaybackStateEnabled)
        return;

    [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
        [playbackState setPlaybackRate:_rate elapsedTime:_elapsedTime timeControlStatus:_timeControlStatus];
    }];
#endif
}

- (NSTimeInterval)estimatedElapsedTime
{
    auto currentHostTime = [[NSProcessInfo processInfo] systemUptime];
    if (_timeControlStatus != AVPlayerTimeControlStatusPlaying)
        return _elapsedTime;
    return _elapsedTime + _rate * (currentHostTime - _anchorTime);
}

- (void)setUpPIPForVideoView:(NSView *)videoView withFrame:(NSRect)frame inWindow:(NSWindow *)window
{
    ASSERT(!_pipViewController);
    ASSERT(!_videoViewContainerController);
    ASSERT(!_videoViewContainer);
    ASSERT(!_playerLayer);

    _pipViewController = adoptNS([allocPIPViewControllerInstance() init]);
    [_pipViewController setDelegate:self];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_pipViewController setUserCanResize:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto model = _videoPresentationInterfaceMac ? CheckedPtr { _videoPresentationInterfaceMac }->videoPresentationModel() : nullptr;
    // _videoDimensions is normally kept up to date by videoDimensionsChanged(), but that notification
    // can be dropped or arrive late relative to this call (e.g. it's delivered over an async,
    // cross-process message that can race with the one that leads here, particularly on the video's
    // first fullscreen/PiP entry). Always prefer the model's current natural size when it has one,
    // rather than only catching up when _videoDimensions is still zero: since _videoDimensions persists
    // across PiP sessions (it's only reset on full teardown), latching a stale/wrong value once — e.g.
    // falling back to the possibly CSS-cropped inline frame size below because the model race was lost —
    // would otherwise stick for every subsequent PiP entry on this video.
    auto modelVideoDimensions = model ? model->videoDimensions() : WebCore::FloatSize();
    if (!modelVideoDimensions.isZero())
        [self setVideoDimensions:static_cast<NSSize>(modelVideoDimensions)];
    else if (NSEqualSizes(_videoDimensions, NSZeroSize))
        [self setVideoDimensions:frame.size];
    if (model)
        model->setVideoLayerGravity(MediaPlayerEnums::VideoGravity::ResizeAspectFill);

    _videoViewContainer = adoptNS([[WebVideoViewContainer alloc] initWithFrame:frame]);
    [_videoViewContainer setVideoViewContainerDelegate:self];
    [_videoViewContainer setLayer:[CALayer layer]];
    [_videoViewContainer setWantsLayer:YES];
    [_videoViewContainer setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

    _playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);
    [retainPtr([_videoViewContainer layer]) addSublayer:_playerLayer.get()];
    [_playerLayer setFrame:[_videoViewContainer layer].bounds];
    [_playerLayer setPresentationModel:model.get()];
    [_playerLayer setVideoSublayer:videoView.layer];
    // Use plain Resize gravity (no internal aspect-fit/crop) while the fly-to-PiP animation is in
    // flight: WebAVPlayerLayer's -layoutSublayers otherwise reshapes videoView.layer's *current*
    // bounds to fit _videoDimensions' aspect ratio before computing its scale-to-target-frame
    // transform, assuming that reshape is a simple centered crop. For an object-view-box-cropped
    // video, videoView.layer's actual bounds are WebProcess's internal (possibly asymmetric,
    // non-natural-aspect) representation of the crop, not a centered crop of the natural frame —
    // fitting to _videoDimensions' aspect ratio then picks the wrong sub-rect of that buffer,
    // which is the "crops upper region" artifact this avoids. Resize sidesteps the whole problem:
    // it never tries to select a sub-rect, it just uniformly stretches whatever's already correctly
    // composited to fill the animating frame. -boundsDidChangeForVideoViewContainer: restores
    // AspectFill once the transition has settled and the crop bypass has taken effect.
    [_playerLayer setVideoGravity:AVLayerVideoGravityResize];
    [_playerLayer setAutoresizingMask:(kCALayerWidthSizable | kCALayerHeightSizable)];

    [retainPtr(videoView.layer) removeFromSuperlayer];
    [_playerLayer addSublayer:retainPtr(videoView.layer).get()];

    _videoViewContainerController = adoptNS([[NSViewController alloc] init]);
    [_videoViewContainerController setView:_videoViewContainer.get()];
    [retainPtr(window.contentView) addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];

#if HAVE(PIP_SKIP_PREROLL)
    [self updatePrerollAttributes];
    if (self.isPlaybackStateEnabled) {
        [_pipViewController updatePlaybackStateUsingBlock:^(PIPMutablePlaybackState *playbackState) {
            // "Infinte" duration means "Live Video" by convention.
            playbackState.contentType = std::isinf(_duration) ? PIPContentTypeLiveBroadcast : PIPContentTypeVideoOnDemand;
            playbackState.contentDuration = _duration;
            [playbackState setPlaybackRate:_rate elapsedTime:_elapsedTime timeControlStatus:_timeControlStatus];
        }];
        return;
    }
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_pipViewController setPlaying:_playing];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)enterPIP
{
    if (_pipState == PIPState::EnteringPIP || _pipState == PIPState::InPIP)
        return;

    [_videoViewContainerController view].layer.backgroundColor = RetainPtr { CGColorGetConstantColor(kCGColorBlack) }.get();
    [_pipViewController presentViewControllerAsPictureInPicture:_videoViewContainerController.get()];
    _pipState = PIPState::EnteringPIP;
#if HAVE(PIP_SKIP_PREROLL)
    [self updatePrerollAttributes];
#endif
}

- (void)exitPIP
{
    if (_pipState != PIPState::InPIP || !_pipViewController || !_videoViewContainerController)
        return;

    [_videoViewContainerController view].layer.backgroundColor = RetainPtr { CGColorGetConstantColor(kCGColorClear) }.get();
    // Mirror -setUpPIPForVideoView:'s Resize gravity for the fly-from-PiP animation: the same
    // -layoutSublayers aspect-crop-to-_videoDimensions issue applies here, and would otherwise
    // crop object-view-box-cropped content while animating back. -pipDidClose: restores
    // AspectFill once the animation (and this dismissal) has finished.
    [_playerLayer setVideoGravity:AVLayerVideoGravityResize];
    [_pipViewController dismissViewController:_videoViewContainerController.get()];
    _pipState = PIPState::ExitingPIP;
}

- (void)exitPIPAnimatingToRect:(NSRect)rect inWindow:(NSWindow *)window
{
    _returningWindow = window;
    _returningRect = rect;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_pipViewController setReplacementRect:rect];
    [_pipViewController setReplacementWindow:window];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [self exitPIP];
}

// WebVideoViewContainerDelegate

- (void)boundsDidChangeForVideoViewContainer:(WebVideoViewContainer *)videoViewContainer
{
    if (!_videoViewContainer || !_pipViewController)
        return;

    ASSERT_UNUSED(videoViewContainer, videoViewContainer == _videoViewContainer);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return;

    if (_pipState == PIPState::EnteringPIP) {
        // FIXME(rdar://problem/42250952)
        // Currently, -[PIPViewController presentViewControllerAsPictureInPicture:] does not
        // take a completionHandler parameter, so we use the first bounds change event
        // as an indication that entering picture-in-picture is completed.
        _pipState = PIPState::InPIP;

        // The fly-to-PiP animation has finished: restore AspectFill now that videoView.layer's
        // hosted content is expected to settle into its steady-state shape (natural video for
        // object-view-box crops, see the comment in -setUpPIPForVideoView:; already-correct
        // shape otherwise), so -layoutSublayers' aspect-ratio-based fit is meaningful again.
        [_playerLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];

        if (auto model = videoPresentationInterfaceMac->videoPresentationModel()) {
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

    if (![videoViewContainer isDescendantOf:retainPtr([_pipViewController view]).get()])
        return;

    // Once the view is moved into the pip view, make sure it resizes with the pip view.
    videoViewContainer.frame = [videoViewContainer superview].bounds;
    videoViewContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
}

// PIPViewControllerDelegate

- (BOOL)pipShouldClose:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return YES;
    
    if (auto model = videoPresentationInterfaceMac->videoPresentationModel())
        model->fullscreenMayReturnToInline();

    videoPresentationInterfaceMac->requestHideAndExitPiP();

    return NO;
}
#if HAVE(PIP_SKIP_PREROLL)
- (void)pipActionSkipPreroll:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return;

    videoPresentationInterfaceMac->skipAd();
    [self updateCanSkipAd:NO];
}
#endif
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)pipDidClose:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return;

    if (_pipState != PIPState::ExitingPIP) {
        // We got told to close without going through -pipActionStop, nor by exlicitly being asked to in -exitPiP:.
        // Call -pipActionStop: here in order to set the fullscreen state to an expected value.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [self pipActionStop:pip];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    // The fly-from-PiP animation has finished: restore AspectFill now that videoView.layer's
    // hosted content is expected to settle back into its steady-state shape; see the comment
    // in -exitPIP for why Resize was used during the animation. That assumption only holds when
    // the settled content is actually the full natural video: when returning all the way to
    // inline display (not on the way into standard fullscreen) for an object-view-box-cropped
    // video, videoView.layer's bounds represent that (non-natural-aspect) crop, not the natural
    // video, since the crop is not bypassed for plain inline display. Applying AspectFill's
    // natural-aspect-ratio-based fit to those bounds re-crops already-cropped content, producing
    // a brief, visible content shift right before the overlay is torn down. Keep Resize in that
    // case, matching RenderLayerBacking::updateVideoGravity()'s handling of the same content for
    // the underlying inline layer.
    if (!videoPresentationInterfaceMac->hasObjectViewBox() || self.isExitingToStandardFullscreen)
        [_playerLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];

    if (auto model = videoPresentationInterfaceMac->videoPresentationModel()) {
        if (_videoViewContainer && _returningWindow && !NSEqualRects(_returningRect, NSZeroRect)) {
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
                context.allowsImplicitAnimation = NO;
                [_videoViewContainer setFrame:_returningRect];
                [retainPtr([_returningWindow contentView]) addSubview:_videoViewContainer.get() positioned:NSWindowAbove relativeTo:nil];
            } completionHandler:nil];
        }

        if (!self.isExitingToStandardFullscreen) {
            model->didExitPictureInPicture();
            model->setVideoLayerGravity(MediaPlayerEnums::VideoGravity::ResizeAspect);
        }

        model->didExitFullscreen();
        model->setRequiresTextTrackRepresentation(false);
    }

    videoPresentationInterfaceMac->clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, VideoPresentationModel::ShouldNotifyMediaElement::Yes);
}

- (void)pipActionPlay:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (videoPresentationInterfaceMac && videoPresentationInterfaceMac->playbackSessionModel())
        protect(videoPresentationInterfaceMac->playbackSessionModel())->play();
}

- (void)pipActionPause:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (videoPresentationInterfaceMac && videoPresentationInterfaceMac->playbackSessionModel())
        protect(videoPresentationInterfaceMac->playbackSessionModel())->pause();
}

- (void)pipActionStop:(PIPViewController *)pip
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return;

    if (CheckedPtr playbackSessionModel = videoPresentationInterfaceMac->playbackSessionModel())
        playbackSessionModel->pause();

    // The native PIP window chrome's own dismiss animation is about to start here, bypassing
    // -exitPIP (whose early-return guard would otherwise skip it once _pipState is updated
    // below) — so set the same Resize gravity directly; see the comment in -exitPIP.
    [_playerLayer setVideoGravity:AVLayerVideoGravityResize];
    videoPresentationInterfaceMac->requestHideAndExitPiP();
    _pipState = PIPState::ExitingPIP;
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
@end

@interface WebSeekableVideoPresentationInterfaceMacObjC : WebVideoPresentationInterfaceMacObjC
@end

@implementation WebSeekableVideoPresentationInterfaceMacObjC
- (void)pipAction:(PIPViewController *)pip skipInterval:(NSTimeInterval)interval
{
    ASSERT_UNUSED(pip, pip == _pipViewController);

    CheckedPtr videoPresentationInterfaceMac = _videoPresentationInterfaceMac;
    if (!videoPresentationInterfaceMac)
        return;

    if (CheckedPtr playbackSessionModel = videoPresentationInterfaceMac->playbackSessionModel())
        playbackSessionModel->seekToTime(self.estimatedElapsedTime + interval);
}
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceMac);

VideoPresentationInterfaceMac::VideoPresentationInterfaceMac(PlaybackSessionInterfaceMac& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
{
    ASSERT(m_playbackSessionInterface->playbackSessionModel());
    CheckedPtr model = m_playbackSessionInterface->playbackSessionModel();
    model->addClient(*this);
}

VideoPresentationInterfaceMac::~VideoPresentationInterfaceMac()
{
    if (CheckedPtr model = m_playbackSessionInterface->playbackSessionModel())
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

void VideoPresentationInterfaceMac::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, VideoPresentationModel::ShouldNotifyMediaElement)
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
        model->fullscreenModeChanged(m_mode, VideoPresentationModel::ShouldNotifyMediaElement::Yes);
}
#if HAVE(PIP_SKIP_PREROLL)
void VideoPresentationInterfaceMac::skipAd()
{
    m_playbackSessionInterface->skipAd();
}
#endif
void VideoPresentationInterfaceMac::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode, VideoPresentationModel::ShouldNotifyMediaElement)
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
        model->fullscreenModeChanged(m_mode, VideoPresentationModel::ShouldNotifyMediaElement::Yes);
}

void VideoPresentationInterfaceMac::durationChanged(double duration)
{
    [protect(videoPresentationInterfaceObjC()) setDuration:duration];
}

void VideoPresentationInterfaceMac::currentTimeChanged(double currentTime, double anchorTime)
{
    [protect(videoPresentationInterfaceObjC()) updateCurrentTime:currentTime atAnchorTime:anchorTime];
}

void VideoPresentationInterfaceMac::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double /* defaultPlaybackRate */)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "playbackState: ", playbackState, ", playbackRate: ", playbackRate);
    AVPlayerTimeControlStatus timeControlStatus = AVPlayerTimeControlStatusPaused;
    if (playbackState.containsAll({ PlaybackSessionModel::PlaybackState::Stalled, PlaybackSessionModel::PlaybackState::Playing }))
        timeControlStatus = AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate;
    else if (playbackState.contains(PlaybackSessionModel::PlaybackState::Playing))
        timeControlStatus = AVPlayerTimeControlStatusPlaying;

    [protect(videoPresentationInterfaceObjC()) updateRate:playbackRate andTimeControlStatus:timeControlStatus];
}

void VideoPresentationInterfaceMac::ensureControlsManager()
{
    m_playbackSessionInterface->ensureControlsManager();
}
#if HAVE(PIP_SKIP_PREROLL)
void VideoPresentationInterfaceMac::setPlaybackStateEnabled(bool enabled)
{
    if (m_playbackStateEnabled == enabled)
        return;

    m_playbackStateEnabled = enabled;
    m_webVideoPresentationInterfaceObjC = nil;
}

void VideoPresentationInterfaceMac::canSkipAdChanged(bool canSkipAd)
{
    [videoPresentationInterfaceObjC() updateCanSkipAd:canSkipAd];
}
#endif

WebVideoPresentationInterfaceMacObjC *VideoPresentationInterfaceMac::videoPresentationInterfaceObjC()
{
    if (!m_webVideoPresentationInterfaceObjC) {
#if HAVE(PIP_SKIP_PREROLL)
        if (isPlaybackStateEnabled())
            m_webVideoPresentationInterfaceObjC = adoptNS([[WebSeekableVideoPresentationInterfaceMacObjC alloc] initWithVideoPresentationInterfaceMac:this]);
        else
#endif
            m_webVideoPresentationInterfaceObjC = adoptNS([[WebVideoPresentationInterfaceMacObjC alloc] initWithVideoPresentationInterfaceMac:this]);

        CheckedPtr model = m_playbackSessionInterface->playbackSessionModel();

        durationChanged(model->duration());
        currentTimeChanged(model->currentTime(), [[NSProcessInfo processInfo] systemUptime]);
        rateChanged(model->playbackState(), model->playbackRate(), model->defaultPlaybackRate());
    }

    return m_webVideoPresentationInterfaceObjC.get();
}

void VideoPresentationInterfaceMac::setupFullscreen(const IntRect& initialRect, NSWindow *parentWindow, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool hasObjectViewBox)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::setupFullscreen(%p), initialRect:{%d, %d, %d, %d}, parentWindow:%p, mode:%d", this, initialRect.x(), initialRect.y(), initialRect.width(), initialRect.height(), parentWindow, mode);

    UNUSED_PARAM(allowsPictureInPicturePlayback);
    ASSERT(mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);

    m_mode |= mode;
    m_hasObjectViewBox = hasObjectViewBox;

    [protect(videoPresentationInterfaceObjC()) setUpPIPForVideoView:protect(layerHostView()).get() withFrame:(NSRect)initialRect inWindow:parentWindow];

    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, this] {
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
        [protect(m_playbackSessionInterface->playBackControlsManager()) setPictureInPictureActive:YES];
#endif
    }
}

bool VideoPresentationInterfaceMac::exitFullscreen(const IntRect& finalRect, NSWindow *parentWindow)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::exitFullscreen(%p), finalRect:{%d, %d, %d, %d}, parentWindow:%p", this, finalRect.x(), finalRect.y(), finalRect.width(), finalRect.height(), parentWindow);

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protect(m_playbackSessionInterface->playBackControlsManager()) setPictureInPictureActive:NO];
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
    [protect(m_playbackSessionInterface->playBackControlsManager()) setPictureInPictureActive:NO];
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
    RefPtr model = videoPresentationModel();
    if (!model)
        return;

    if (m_documentIsVisible) {
        model->requestFullscreenMode(m_mode & ~HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        model->willExitPictureInPicture();
    } else {
        auto callback = [model = WTF::move(model), mode = m_mode] () {
            model->requestFullscreenMode(mode & ~HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
            model->willExitPictureInPicture();
        };
        setDocumentBecameVisibleCallback(WTF::move(callback));
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

void VideoPresentationInterfaceMac::hasObjectViewBoxChanged(bool hasObjectViewBox)
{
    LOG(Fullscreen, "VideoPresentationInterfaceMac::hasObjectViewBoxChanged(%p):%s", this, boolString(hasObjectViewBox));

    m_hasObjectViewBox = hasObjectViewBox;
}

bool VideoPresentationInterfaceMac::isPlayingVideoInPictureInPicture() const
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
    return PIPLibrary() && getPIPViewControllerClassSingleton();
}

}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
