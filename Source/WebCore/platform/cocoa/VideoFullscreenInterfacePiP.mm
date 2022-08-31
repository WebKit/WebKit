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

#import "config.h"
#import "VideoFullscreenInterfacePiP.h"

#if HAVE(PIP_CONTROLLER) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "Logging.h"
#import "PictureInPictureSupport.h"
#import "PlaybackSessionInterfaceAVKit.h"
#import "RuntimeApplicationChecks.h"
#import "TimeRanges.h"
#import "VideoFullscreenChangeObserver.h"
#import "VideoFullscreenModel.h"
#import "WebAVPlayerController.h"
#import "WebAVPlayerLayer.h"
#import <AVFoundation/AVTime.h>
#import <AVKit/AVPictureInPictureController.h>
#import <UIKit/UIKit.h>
#import <UIKit/UIWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureControllerContentSource)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureContentViewController)

@interface UIWindow ()
- (BOOL)_isHostedInAnotherProcess;
@end

@interface UIViewController ()
@property (nonatomic, assign, setter=_setIgnoreAppSupportedOrientations:) BOOL _ignoreAppSupportedOrientations;
@end

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

@interface WebAVPictureInPictureContentViewController : AVPictureInPictureContentViewController
- (instancetype)initWithController:(AVPlayerController *)controller;
@property (nonatomic, nonnull, readonly) AVPlayerController* controller;
@property (retain) AVPlayerLayer* playerLayer;
@end

static WebAVPictureInPictureContentViewController* WebAVPictureInPictureContentViewController_initWithController(id aSelf, SEL, AVPlayerController* controller)
{
    ASSERT(controller);

    WebAVPictureInPictureContentViewController *pipController = aSelf;
    objc_super superClass { pipController, getAVPictureInPictureContentViewControllerClass() };
    auto super_init = reinterpret_cast<id(*)(objc_super*, SEL)>(objc_msgSendSuper);
    aSelf = super_init(&superClass, @selector(init));
    if (!aSelf)
        return aSelf;

    [controller retain];
    object_setInstanceVariable(aSelf, "_controller", controller);
    return aSelf;
}

static AVPlayerController *WebAVPictureInPictureContentViewController_controller(id aSelf, SEL)
{
    void* controller;
    object_getInstanceVariable(aSelf, "_controller", &controller);
    return static_cast<AVPlayerController*>(controller);
}

static AVPlayerLayer *WebAVPictureInPictureContentViewController_playerLayer(id aSelf, SEL)
{
    void* layer;
    object_getInstanceVariable(aSelf, "_playerLayer", &layer);
    return static_cast<AVPlayerLayer*>(layer);
}

static void WebAVPictureInPictureContentViewController_setPlayerLayer(id aSelf, SEL, AVPlayerLayer* layer)
{
    WebAVPictureInPictureContentViewController *pipController = aSelf;
    auto oldPlayerLayer = [pipController playerLayer];
    if (oldPlayerLayer == layer)
        return;
    [oldPlayerLayer release];
    [layer retain];
    object_setInstanceVariable(aSelf, "_playerLayer", layer);
    [[pipController view].layer addSublayer:layer];
}

static void WebAVPictureInPictureContentViewController_viewWillLayoutSubviews(id aSelf, SEL)
{
    WebAVPictureInPictureContentViewController *pipController = aSelf;
    objc_super superClass { pipController, getAVPictureInPictureContentViewControllerClass() };
    auto super_viewWillLayoutSubviews = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_viewWillLayoutSubviews(&superClass, @selector(viewWillLayoutSubviews));
    [[pipController playerLayer] setFrame:[pipController view].bounds];
}

static void WebAVPictureInPictureContentViewController_dealloc(id aSelf, SEL)
{
    WebAVPictureInPictureContentViewController *pipController = aSelf;
    [[pipController controller] release];
    [[pipController playerLayer] release];
    objc_super superClass { pipController, getAVPictureInPictureContentViewControllerClass() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

static WebAVPictureInPictureContentViewController *allocWebAVPictureInPictureContentViewControllerInstance()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(getAVPictureInPictureContentViewControllerClass(), "WebAVPictureInPictureContentViewController", 0);
        class_addMethod(theClass, @selector(initWithController:), (IMP)WebAVPictureInPictureContentViewController_initWithController, "v@:@");
        class_addMethod(theClass, @selector(controller), (IMP)WebAVPictureInPictureContentViewController_controller, "@@:");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPictureInPictureContentViewController_controller, "@@:");
        class_addMethod(theClass, @selector(playerLayer), (IMP)WebAVPictureInPictureContentViewController_playerLayer, "@@:");
        class_addMethod(theClass, @selector(setPlayerLayer:), (IMP)WebAVPictureInPictureContentViewController_setPlayerLayer, "v@:@");
        class_addMethod(theClass, @selector(viewWillLayoutSubviews), (IMP)WebAVPictureInPictureContentViewController_viewWillLayoutSubviews, "v@:");
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPictureInPictureContentViewController_dealloc, "v@:");

        class_addIvar(theClass, "_controller", sizeof(AVPlayerController*), log2(sizeof(AVPlayerController*)), "@");
        class_addIvar(theClass, "_playerLayer", sizeof(AVPlayerLayer*), log2(sizeof(AVPlayerLayer*)), "@");
        objc_registerClassPair(theClass);
    });

    return (WebAVPictureInPictureContentViewController *)[theClass alloc];
}

@interface WebAVPictureInPictureController : NSObject <AVPictureInPictureControllerDelegate> {
    WeakPtr<VideoFullscreenInterfacePiP> _fullscreenInterface;
    RetainPtr<NSTimer> _startPictureInPictureTimer;
    RetainPtr<AVPictureInPictureController> _pip;
    RetainPtr<WebAVPictureInPictureContentViewController> _controller;
}
@property (assign) VideoFullscreenInterfacePiP* fullscreenInterface;
@end

@implementation WebAVPictureInPictureController

static const NSTimeInterval startPictureInPictureTimeInterval = 5.0;

- (VideoFullscreenInterfacePiP*)fullscreenInterface
{
    ASSERT(isMainThread());
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(VideoFullscreenInterfacePiP*)fullscreenInterface
{
    ASSERT(isMainThread());
    _fullscreenInterface = *fullscreenInterface;
}

- (instancetype)initWithFullscreenInterface:(VideoFullscreenInterfacePiP *)interface
{
    if (!(self = [super init]))
        return nil;

    [self setFullscreenInterface:interface];

    auto* playerController = static_cast<AVPlayerController*>(interface->playerController());
    _controller = adoptNS([allocWebAVPictureInPictureContentViewControllerInstance() initWithController:playerController]);
    LOG(Fullscreen, "initWithFullscreenInterface isPlaying:%d", [playerController isPlaying]);

    auto source = adoptNS([allocAVPictureInPictureControllerContentSourceInstance() initWithSourceView:static_cast<UIView*>(interface->playerLayerView()) contentViewController:_controller.get() playerController:playerController]);

    _pip = adoptNS([allocAVPictureInPictureControllerInstance() initWithContentSource:source.get()]);
    [_pip setDelegate:self];

    return self;
}

- (void)dealloc
{
    if (_startPictureInPictureTimer) {
        [self removeObserver];
        [_startPictureInPictureTimer invalidate];
        _startPictureInPictureTimer = nil;
    }
    _controller = nil;
    _pip = nil;
    [super dealloc];
}

- (void)tryToStartPictureInPicture
{
    if (_startPictureInPictureTimer)
        return;

    if (_pip.get().pictureInPicturePossible) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_pip startPictureInPicture];
        });
        return;
    }

    _startPictureInPictureTimer = [NSTimer scheduledTimerWithTimeInterval:startPictureInPictureTimeInterval repeats:NO block:^(NSTimer *_Nonnull) {
        [self removeObserver];
        self->_startPictureInPictureTimer = nil;
        if (_fullscreenInterface)
            _fullscreenInterface->failedToStartPictureInPicture();
    }];

    [self initObserver];
}

- (void)stopPictureInPicture
{
    [_startPictureInPictureTimer invalidate];
    _startPictureInPictureTimer = nil;
    [self removeObserver];
    [_pip stopPictureInPicture];
}

- (void)cleanup
{
    [_pip setDelegate:nil];
    [self stopPictureInPicture];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:@"pictureInPicturePossible"]);

    if (![keyPath isEqualToString:@"pictureInPicturePossible"] || ![self isPictureInPicturePossible])
        return;

    if (!_startPictureInPictureTimer)
        return;
    [_startPictureInPictureTimer invalidate];
    _startPictureInPictureTimer = nil;

    [self removeObserver];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_pip startPictureInPicture];
    });
}

- (void)initObserver
{
    [_pip.get() addObserver:self forKeyPath:@"pictureInPicturePossible" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)removeObserver
{
    [_pip removeObserver:self forKeyPath:@"pictureInPicturePossible" context:nil];
}

- (void)pictureInPictureControllerWillStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    UNUSED_PARAM(pictureInPictureController);
    [_controller setPlayerLayer:[self.fullscreenInterface->playerLayerView() playerLayer]];
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStartPictureInPicture();
}

- (void)pictureInPictureControllerDidStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    UNUSED_PARAM(pictureInPictureController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStartPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController failedToStartPictureInPictureWithError:(NSError *)error
{
    UNUSED_PARAM(pictureInPictureController);
    UNUSED_PARAM(error);
    if (self.fullscreenInterface)
        self.fullscreenInterface->failedToStartPictureInPicture();
}

- (void)pictureInPictureControllerWillStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    UNUSED_PARAM(pictureInPictureController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStopPictureInPicture();
}

- (void)pictureInPictureControllerDidStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    UNUSED_PARAM(pictureInPictureController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStopPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(pictureInPictureController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}

@end

@interface WebAVPlayerLayerView : __AVPlayerLayerView
@end

static Class WebAVPlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static AVPlayerController *WebAVPlayerLayerView_playerController(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayer = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayer playerLayer];
    return [webAVPlayerLayer playerController];
}

static void WebAVPlayerLayerView_setPlayerController(id aSelf, SEL, AVPlayerController *playerController)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setPlayerController: playerController];
}

static AVPlayerLayer *WebAVPlayerLayerView_playerLayer(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayerView = aSelf;

    if ([get__AVPlayerLayerViewClass() instancesRespondToSelector:@selector(playerLayer)]) {
        objc_super superClass { playerLayerView, get__AVPlayerLayerViewClass() };
        auto superClassMethod = reinterpret_cast<AVPlayerLayer *(*)(objc_super *, SEL)>(objc_msgSendSuper);
        return superClassMethod(&superClass, @selector(playerLayer));
    }

    return (AVPlayerLayer *)[playerLayerView layer];
}

#pragma mark - Methods

static WebAVPlayerLayerView *allocWebAVPlayerLayerViewInstance()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ASSERT(get__AVPlayerLayerViewClass());
        theClass = objc_allocateClassPair(get__AVPlayerLayerViewClass(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(setPlayerController:), (IMP)WebAVPlayerLayerView_setPlayerController, "v@:@");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPlayerLayerView_playerController, "@@:");
        class_addMethod(theClass, @selector(playerLayer), (IMP)WebAVPlayerLayerView_playerLayer, "@@:");

        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPlayerLayerView_layerClass, "@@:");
    });
    return (WebAVPlayerLayerView *)[theClass alloc];
}

Ref<VideoFullscreenInterfacePiP> VideoFullscreenInterfacePiP::create(PlaybackSessionInterfaceAVKit& playbackSessionInterface)
{
    return adoptRef(*new VideoFullscreenInterfacePiP(playbackSessionInterface));
}

VideoFullscreenInterfacePiP::VideoFullscreenInterfacePiP(PlaybackSessionInterfaceAVKit& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
{
}

VideoFullscreenInterfacePiP::~VideoFullscreenInterfacePiP()
{
    WebAVPlayerController* playerController = this->playerController();
    if (playerController && playerController.externalPlaybackActive)
        externalPlaybackChanged(false, PlaybackSessionModel::ExternalPlaybackTargetType::TargetTypeNone, emptyString());
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);
}

WebAVPlayerController *VideoFullscreenInterfacePiP::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

void VideoFullscreenInterfacePiP::setVideoFullscreenModel(VideoFullscreenModel* model)
{
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);

    m_videoFullscreenModel = model;

    if (m_videoFullscreenModel) {
        m_videoFullscreenModel->addClient(*this);
        m_videoFullscreenModel->requestRouteSharingPolicyAndContextUID([this, protectedThis = Ref { *this }] (RouteSharingPolicy policy, String contextUID) {
            m_routeSharingPolicy = policy;
            m_routingContextUID = contextUID;
        });
    }

    hasVideoChanged(m_videoFullscreenModel ? m_videoFullscreenModel->hasVideo() : false);
    videoDimensionsChanged(m_videoFullscreenModel ? m_videoFullscreenModel->videoDimensions() : FloatSize());
}

void VideoFullscreenInterfacePiP::setVideoFullscreenChangeObserver(VideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void VideoFullscreenInterfacePiP::hasVideoChanged(bool hasVideo)
{
    [playerController() setHasEnabledVideo:hasVideo];
    [playerController() setHasVideo:hasVideo];
}

void VideoFullscreenInterfacePiP::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (videoDimensions.isZero())
        return;

    auto* playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setVideoDimensions:videoDimensions];
    [playerController() setContentDimensions:videoDimensions];
    [m_playerLayerView setNeedsLayout];
}

void VideoFullscreenInterfacePiP::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    [m_playerLayerView setHidden:enabled];
}

bool VideoFullscreenInterfacePiP::pictureInPictureWasStartedWhenEnteringBackground() const
{
    return false;
}

void VideoFullscreenInterfacePiP::applicationDidBecomeActive()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::applicationDidBecomeActive(%p)", this);
}

void VideoFullscreenInterfacePiP::setupFullscreen(UIView& videoView, const FloatRect& initialRect, const FloatSize& videoDimensions, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::setupFullscreen(%p)", this);

    m_changingStandbyOnly = mode == HTMLMediaElementEnums::VideoFullscreenModeNone && standby;

    [playerController() setHasEnabledVideo:true];
    [playerController() setHasVideo:true];
    [playerController() setContentDimensions:videoDimensions];

    m_allowsPictureInPicturePlayback = allowsPictureInPicturePlayback;
    m_videoView = &videoView;
    m_parentView = parentView;
    m_parentWindow = parentView.window;

    m_targetStandby = standby;
    m_targetMode = mode;
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
    setInlineRect(initialRect, true);
    doSetup();
}

void VideoFullscreenInterfacePiP::enterFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::enterFullscreen(%p) %d", this, mode());

    doEnterFullscreen();
}

bool VideoFullscreenInterfacePiP::exitFullscreen(const FloatRect& finalRect)
{
    // VideoFullscreenManager may ask a video to exit standby while the video
    // is entering picture-in-picture. We need to ignore the request in that case.
    if (m_standby && m_enteringPictureInPicture)
        return false;

    m_changingStandbyOnly = !m_currentMode.hasVideo() && m_standby;

    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;

    setInlineRect(finalRect, true);
    doExitFullscreen();

    return true;
}

void VideoFullscreenInterfacePiP::cleanupFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::cleanupFullscreen(%p)", this);

    m_cleanupNeedsReturnVideoContentLayer = true;
    if (m_hasVideoContentLayer && m_fullscreenChangeObserver) {
        m_fullscreenChangeObserver->returnVideoContentLayer();
        return;
    }
    m_cleanupNeedsReturnVideoContentLayer = false;

    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
    }

    if (m_currentMode.hasPictureInPicture())
        [m_pipController cleanup];
    m_pipController = nil;

    [m_playerLayerView removeFromSuperview];
    [[m_viewController view] removeFromSuperview];

    m_playerLayerView = nil;
    m_window = nil;
    m_videoView = nil;
    m_parentView = nil;
    m_parentWindow = nil;

    [playerController() setHasEnabledVideo:false];
    [playerController() setHasVideo:false];

    if (m_exitingPictureInPicture) {
        m_exitingPictureInPicture = false;
        if (m_videoFullscreenModel)
            m_videoFullscreenModel->didExitPictureInPicture();
    }

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didCleanupFullscreen();
}

void VideoFullscreenInterfacePiP::invalidate()
{
    m_videoFullscreenModel = nullptr;
    m_fullscreenChangeObserver = nullptr;

    m_enteringPictureInPicture = false;
    cleanupFullscreen();
}

void VideoFullscreenInterfacePiP::modelDestroyed()
{
    ASSERT(isUIThread());
    invalidate();
}

void VideoFullscreenInterfacePiP::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    m_playerIdentifier = identifier;
}

void VideoFullscreenInterfacePiP::requestHideAndExitFullscreen()
{
    if (m_currentMode.hasPictureInPicture())
        return;

    LOG(Fullscreen, "VideoFullscreenInterfacePiP::requestHideAndExitFullscreen(%p)", this);

    [m_window setHidden:YES];
    if (playbackSessionModel() && m_videoFullscreenModel) {
        playbackSessionModel()->pause();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void VideoFullscreenInterfacePiP::preparedToReturnToInline(bool visible, const FloatRect& inlineRect)
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    setInlineRect(inlineRect, visible);
    if (m_prepareToInlineCallback) {
        WTF::Function<void(bool)> callback = WTFMove(m_prepareToInlineCallback);
        callback(visible);
    }
}

void VideoFullscreenInterfacePiP::preparedToExitFullscreen()
{
}

bool VideoFullscreenInterfacePiP::mayAutomaticallyShowVideoPictureInPicture() const
{
    return [playerController() isPlaying] && (m_standby || m_currentMode.isFullscreen()) && supportsPictureInPicture();
}

void VideoFullscreenInterfacePiP::prepareForPictureInPictureStop(WTF::Function<void(bool)>&& callback)
{
    m_prepareToInlineCallback = WTFMove(callback);
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->fullscreenMayReturnToInline();
}

void VideoFullscreenInterfacePiP::willStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::willStartPictureInPicture(%p)", this);
    m_enteringPictureInPicture = true;

    if (m_standby && !m_currentMode.hasVideo())
        [m_window setHidden:NO];

    if (!m_hasVideoContentLayer)
        m_fullscreenChangeObserver->requestVideoContentLayer();
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->willEnterPictureInPicture();
}

void VideoFullscreenInterfacePiP::didStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::didStartPictureInPicture(%p)", this);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_enterFullscreenNeedsEnterPictureInPicture);
    [m_viewController _setIgnoreAppSupportedOrientations:NO];

    if (m_currentMode.hasFullscreen())
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;
    else {
        if (m_standby && !m_blocksReturnToFullscreenFromPictureInPicture)
            m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;

        [m_window setHidden:YES];
    }

    if (m_enterFullscreenNeedsEnterPictureInPicture)
        doEnterFullscreen();
}

void VideoFullscreenInterfacePiP::failedToStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::failedToStartPictureInPicture(%p)", this);

    m_targetMode.setPictureInPicture(false);
    if (m_currentMode.hasFullscreen())
        return;

    if (m_videoFullscreenModel) {
        m_videoFullscreenModel->failedToEnterPictureInPicture();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void VideoFullscreenInterfacePiP::willStopPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::willStopPictureInPicture(%p)", this);

    m_exitingPictureInPicture = true;
    m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

    if (m_currentMode.hasFullscreen())
        return;

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->willExitPictureInPicture();
}

void VideoFullscreenInterfacePiP::didStopPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::didStopPictureInPicture(%p)", this);
    m_targetMode.setPictureInPicture(false);
    [m_viewController _setIgnoreAppSupportedOrientations:YES];

    if (m_returningToStandby) {
        m_exitingPictureInPicture = false;
        m_enteringPictureInPicture = false;
        if (m_videoFullscreenModel)
            m_videoFullscreenModel->didExitPictureInPicture();

        return;
    }

    if (m_currentMode.hasFullscreen()) {
        clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_exitFullscreenNeedsExitPictureInPicture);
        [m_window makeKeyWindow];
        if (m_exitFullscreenNeedsExitPictureInPicture)
            doExitFullscreen();
        else if (m_exitingPictureInPicture) {
            m_exitingPictureInPicture = false;
            if (m_videoFullscreenModel)
                m_videoFullscreenModel->didExitPictureInPicture();
        }

        if (m_enterFullscreenNeedsExitPictureInPicture)
            doEnterFullscreen();
        return;
    }

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_exitFullscreenNeedsExitPictureInPicture);

    [m_playerLayerView setBackgroundColor:clearUIColor()];

    if (m_enterFullscreenNeedsExitPictureInPicture)
        doEnterFullscreen();

    if (m_exitFullscreenNeedsExitPictureInPicture)
        doExitFullscreen();
}

void VideoFullscreenInterfacePiP::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::prepareForPictureInPictureStopWithCompletionHandler(%p)", this);

    if (m_shouldReturnToFullscreenWhenStoppingPictureInPicture) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

        [m_window setHidden:NO];

        if (m_standby)
            m_returningToStandby = true;

        return;
    }

    prepareForPictureInPictureStop([protectedThis = Ref { *this }, strongCompletionHandler = adoptNS([completionHandler copy])](bool restored)  {
        LOG(Fullscreen, "VideoFullscreenInterfacePiP::prepareForPictureInPictureStopWithCompletionHandler lambda(%p) - restored(%s)", protectedThis.ptr(), boolString(restored));
        ((void (^)(BOOL))strongCompletionHandler.get())(restored);
    });
}

void VideoFullscreenInterfacePiP::setHasVideoContentLayer(bool value)
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

void VideoFullscreenInterfacePiP::setInlineRect(const FloatRect& inlineRect, bool visible)
{
    m_inlineRect = inlineRect;
    m_inlineIsVisible = visible;
    m_hasUpdatedInlineRect = true;

    if (m_setupNeedsInlineRect)
        doSetup();

    if (m_exitFullscreenNeedInlineRect)
        doExitFullscreen();
}

void VideoFullscreenInterfacePiP::doSetup()
{
    if (m_currentMode.hasVideo() && m_targetMode.hasVideo()) {
        m_standby = m_targetStandby;
        finalizeSetup();
        return;
    }

    if (!m_hasUpdatedInlineRect && m_fullscreenChangeObserver) {
        m_setupNeedsInlineRect = true;
        m_fullscreenChangeObserver->requestUpdateInlineRect();
        return;
    }
    m_setupNeedsInlineRect = false;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (![[m_parentView window] _isHostedInAnotherProcess] && !m_window) {
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

    if (!m_playerLayerView)
        m_playerLayerView = adoptNS([allocWebAVPlayerLayerViewInstance() init]);
    [m_playerLayerView setHidden:[playerController() isExternalPlaybackActive]];
    [m_playerLayerView setBackgroundColor:clearUIColor()];

    if (!m_currentMode.hasPictureInPicture()) {
        auto *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];
        [playerLayer setVideoSublayer:[m_videoView layer]];
        [m_playerLayerView addSubview:m_videoView.get()];
        // The PiP controller requires the player view to be immediately descending from a UIWindow
        [m_window addSubview:m_playerLayerView.get()];
    }

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    auto modelVideoLayerFrame = CGRectMake(0, 0, m_inlineRect.width(), m_inlineRect.height());
    [m_playerLayerView setFrame:modelVideoLayerFrame];
    [playerLayer setModelVideoLayerFrame:modelVideoLayerFrame];
    [playerLayer setVideoDimensions:[playerController() contentDimensions]];
    playerLayer.fullscreenInterface = this;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->setVideoLayerFrame(modelVideoLayerFrame);

    [playerController() setAllowsPictureInPicture:m_allowsPictureInPicturePlayback];

    if (!m_pipController)
        m_pipController = adoptNS([[WebAVPictureInPictureController alloc] initWithFullscreenInterface:this]);

    if (m_targetStandby && !m_currentMode.hasVideo())
        [m_window setHidden:YES];

    [CATransaction commit];

    finalizeSetup();
}

void VideoFullscreenInterfacePiP::preparedToReturnToStandby()
{
    if (!m_returningToStandby)
        return;

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, true);
}

void VideoFullscreenInterfacePiP::finalizeSetup()
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (m_fullscreenChangeObserver) {
            if (!m_hasVideoContentLayer && m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsVideoContentLayer = true;
                m_fullscreenChangeObserver->requestVideoContentLayer();
                return;
            }
            m_finalizeSetupNeedsVideoContentLayer = false;
            if (m_hasVideoContentLayer && !m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsReturnVideoContentLayer = true;
                m_fullscreenChangeObserver->returnVideoContentLayer();
                return;
            }
            m_finalizeSetupNeedsReturnVideoContentLayer = false;
            m_fullscreenChangeObserver->didSetupFullscreen();
        }
    });
}

void VideoFullscreenInterfacePiP::doEnterFullscreen()
{
    m_standby = m_targetStandby;

    if (m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen()) {
        [m_window setHidden:NO];
        return;
    }

    if (m_targetMode.hasPictureInPicture() && !m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsEnterPictureInPicture = true;
        [m_pipController tryToStartPictureInPicture];
        return;
    }
    m_enterFullscreenNeedsEnterPictureInPicture = false;

    if (!m_targetMode.hasFullscreen() && m_currentMode.hasFullscreen())
        return;

    if (!m_targetMode.hasPictureInPicture() && m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsExitPictureInPicture = true;
        [m_pipController stopPictureInPicture];
    }
    m_enterFullscreenNeedsExitPictureInPicture = false;

    if (m_fullscreenChangeObserver) {
        FloatSize size;
        if (m_currentMode.hasPictureInPicture()) {
            auto playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];
            auto videoFrame = [playerLayer calculateTargetVideoFrame];
            size = FloatSize(videoFrame.size());
        }
        m_fullscreenChangeObserver->didEnterFullscreen(size);
        m_enteringPictureInPicture = false;
        m_changingStandbyOnly = false;
    }

    if (m_currentMode.hasPictureInPicture() && m_videoFullscreenModel)
        m_videoFullscreenModel->didEnterPictureInPicture();
}

void VideoFullscreenInterfacePiP::doExitFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfacePiP::doExitFullscreen(%p)", this);

    if (m_currentMode.hasVideo() && !m_hasUpdatedInlineRect && m_fullscreenChangeObserver) {
        m_exitFullscreenNeedInlineRect = true;
        m_fullscreenChangeObserver->requestUpdateInlineRect();
        return;
    }
    m_exitFullscreenNeedInlineRect = false;

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return;

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        m_exitFullscreenNeedsExitPictureInPicture = true;
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;
        [m_window setHidden:NO];
        [m_pipController stopPictureInPicture];
        return;
    }
    m_exitFullscreenNeedsExitPictureInPicture = false;

    if (m_hasVideoContentLayer && m_fullscreenChangeObserver) {
        m_exitFullscreenNeedsReturnContentLayer = true;
        m_fullscreenChangeObserver->returnVideoContentLayer();
        return;
    }
    m_exitFullscreenNeedsReturnContentLayer = false;

    m_standby = false;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didExitFullscreen();
        m_changingStandbyOnly = false;
    });
}

void VideoFullscreenInterfacePiP::returnToStandby()
{
    m_returningToStandby = false;
    [m_window setHidden:YES];

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didSetupFullscreen();
}

void VideoFullscreenInterfacePiP::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.setMode(mode);
    // Mode::mode() can be 3 (VideoFullscreenModeStandard | VideoFullscreenModePictureInPicture).
    // HTMLVideoElement does not expect such a value in the fullscreenModeChanged() callback.
    if (m_videoFullscreenModel && shouldNotifyModel)
        m_videoFullscreenModel->fullscreenModeChanged(mode);
}

void VideoFullscreenInterfacePiP::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((~m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.clearMode(mode);
    if (m_videoFullscreenModel && shouldNotifyModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_currentMode.mode());
}

bool VideoFullscreenInterfacePiP::isPlayingVideoInEnhancedFullscreen() const
{
    return hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && [playerController() isPlaying];
}

static std::optional<bool> isPictureInPictureSupported;

void WebCore::setSupportsPictureInPicture(bool isSupported)
{
    isPictureInPictureSupported = isSupported;
}

bool WebCore::supportsPictureInPicture()
{
    if (isPictureInPictureSupported.has_value())
        return *isPictureInPictureSupported;
    return [getAVPictureInPictureControllerClass() isPictureInPictureSupported];
}

#endif // HAVE(PIP_CONTROLLER) && ENABLE(VIDEO_PRESENTATION_MODE)
