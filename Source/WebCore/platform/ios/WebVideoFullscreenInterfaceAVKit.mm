/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebVideoFullscreenInterfaceAVKit.h"

#if HAVE(AVKIT)

#import "AVKitSPI.h"
#import "GeometryUtilities.h"
#import "Logging.h"
#import "RuntimeApplicationChecks.h"
#import "TimeRanges.h"
#import "WebAVPlayerController.h"
#import "WebCoreSystemInterface.h"
#import "WebPlaybackSessionInterfaceAVKit.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>
#import <UIKit/UIKit.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

// Soft-linking headers must be included last since they #define functions, constants, etc.
#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResize, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspect, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspectFill, NSString *)

SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIApplication)
SOFT_LINK_CLASS(UIKit, UIScreen)
SOFT_LINK_CLASS(UIKit, UIWindow)
SOFT_LINK_CLASS(UIKit, UIView)
SOFT_LINK_CLASS(UIKit, UIViewController)
SOFT_LINK_CLASS(UIKit, UIColor)
SOFT_LINK_CONSTANT(UIKit, UITextEffectsBeneathStatusBarWindowLevel, UIWindowLevel)

static UIColor *clearUIColor()
{
    return (UIColor *)[getUIColorClass() clearColor];
}

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

static const Seconds defaultWatchdogTimerInterval { 1_s };

@class WebAVMediaSelectionOption;

@interface WebAVPlayerViewControllerDelegate : NSObject <AVPlayerViewControllerDelegate_WebKitOnly> {
    RefPtr<WebVideoFullscreenInterfaceAVKit> _fullscreenInterface;
}
@property (assign) WebVideoFullscreenInterfaceAVKit* fullscreenInterface;
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@implementation WebAVPlayerViewControllerDelegate
- (WebVideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(WebVideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    _fullscreenInterface = fullscreenInterface;
}

- (void)playerViewControllerWillStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStartPictureInPicture();
}

- (void)playerViewControllerDidStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStartPictureInPicture();
}

- (void)playerViewControllerFailedToStartPictureInPicture:(AVPlayerViewController *)playerViewController withError:(NSError *)error
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(error);
    self.fullscreenInterface->failedToStartPictureInPicture();
}

- (void)playerViewControllerWillStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStopPictureInPicture();
}

- (void)playerViewControllerDidStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStopPictureInPicture();
}

static WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason convertToExitFullScreenReason(AVPlayerViewControllerExitFullScreenReason reason)
{
    switch (reason) {
    case AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::DoneButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::FullScreenButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::PictureInPictureStarted;
    case AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::PinchGestureHandled;
    case AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::RemoteControlStopEventReceived;
    }
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    return self.fullscreenInterface->shouldExitFullscreenWithReason(convertToExitFullScreenReason(reason));
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}
@end

@interface WebAVPlayerLayer : CALayer
@property (nonatomic, retain) NSString *videoGravity;
@property (nonatomic, getter=isReadyForDisplay) BOOL readyForDisplay;
@property (nonatomic, assign) WebVideoFullscreenInterfaceAVKit* fullscreenInterface;
@property (nonatomic, retain) AVPlayerController *playerController;
@property (nonatomic, retain) CALayer *videoSublayer;
@property (nonatomic, copy, nullable) NSDictionary *pixelBufferAttributes;
@property CGSize videoDimensions;
@property CGRect modelVideoLayerFrame;
@end

@implementation WebAVPlayerLayer {
    RefPtr<WebVideoFullscreenInterfaceAVKit> _fullscreenInterface;
    RetainPtr<WebAVPlayerController> _avPlayerController;
    RetainPtr<CALayer> _videoSublayer;
    RetainPtr<NSString> _videoGravity;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setMasksToBounds:YES];
        _videoGravity = getAVLayerVideoGravityResizeAspect();
    }
    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [_pixelBufferAttributes release];
    [super dealloc];
}

- (WebVideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(WebVideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    _fullscreenInterface = fullscreenInterface;
}

- (AVPlayerController *)playerController
{
    return (AVPlayerController *)_avPlayerController.get();
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:[WebAVPlayerController class]]);
    _avPlayerController = (WebAVPlayerController *)playerController;
}

- (void)setVideoSublayer:(CALayer *)videoSublayer
{
    _videoSublayer = videoSublayer;
}

- (CALayer*)videoSublayer
{
    return _videoSublayer.get();
}

- (void)layoutSublayers
{
    if ([_videoSublayer superlayer] != self)
        return;

    if (![_avPlayerController delegate])
        return;

    [_videoSublayer setPosition:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];

    if (self.videoDimensions.height <= 0 || self.videoDimensions.width <= 0)
        return;

    FloatRect sourceVideoFrame;
    FloatRect targetVideoFrame;
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;
    
    if ([getAVLayerVideoGravityResize() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = self.modelVideoLayerFrame;
        targetVideoFrame = self.bounds;
    } else if ([getAVLayerVideoGravityResizeAspect() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.modelVideoLayerFrame);
        targetVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    } else if ([getAVLayerVideoGravityResizeAspectFill() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.modelVideoLayerFrame);
        self.modelVideoLayerFrame = CGRectMake(0, 0, sourceVideoFrame.width(), sourceVideoFrame.height());
        ASSERT(_fullscreenInterface->model());
        _fullscreenInterface->model()->setVideoLayerFrame(self.modelVideoLayerFrame);
        targetVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);
    } else
        ASSERT_NOT_REACHED();

    UIView *view = (UIView *)[_videoSublayer delegate];
    CGAffineTransform transform = CGAffineTransformMakeScale(targetVideoFrame.width() / sourceVideoFrame.width(), targetVideoFrame.height() / sourceVideoFrame.height());
    [view setTransform:transform];
    
    NSTimeInterval animationDuration = [CATransaction animationDuration];
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];

        [self performSelector:@selector(resolveBounds) withObject:nil afterDelay:animationDuration + 0.1];
    });
}

- (void)resolveBounds
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    if (![_avPlayerController delegate])
        return;
    
    if ([_videoSublayer superlayer] != self)
        return;
    
    if (CGRectEqualToRect(self.modelVideoLayerFrame, [self bounds]) && CGAffineTransformIsIdentity([(UIView *)[_videoSublayer delegate] transform]))
        return;
    
    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];
    
    if (!CGRectEqualToRect(self.modelVideoLayerFrame, [self bounds])) {
        self.modelVideoLayerFrame = [self bounds];
        ASSERT(_fullscreenInterface->model());
        _fullscreenInterface->model()->setVideoLayerFrame(self.modelVideoLayerFrame);
    }
    [(UIView *)[_videoSublayer delegate] setTransform:CGAffineTransformIdentity];
    
    [CATransaction commit];
}

- (void)setVideoGravity:(NSString *)videoGravity
{
    _videoGravity = videoGravity;
    
    if (![_avPlayerController delegate])
        return;

    WebCore::WebVideoFullscreenModel::VideoGravity gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    if (videoGravity == getAVLayerVideoGravityResize())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResize;
    if (videoGravity == getAVLayerVideoGravityResizeAspect())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    else if (videoGravity == getAVLayerVideoGravityResizeAspectFill())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspectFill;
    else
        ASSERT_NOT_REACHED();
    
    ASSERT(_fullscreenInterface->model());
    _fullscreenInterface->model()->setVideoLayerGravity(gravity);
}

- (NSString *)videoGravity
{
    return _videoGravity.get();
}

- (CGRect)videoRect
{
    if (self.videoDimensions.width <= 0 || self.videoDimensions.height <= 0)
        return self.bounds;
    
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([getAVLayerVideoGravityResizeAspect() isEqualToString:self.videoGravity])
        return largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    if ([getAVLayerVideoGravityResizeAspectFill() isEqualToString:self.videoGravity])
        return smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);

    return self.bounds;
}

+ (NSSet *)keyPathsForValuesAffectingVideoRect
{
    return [NSSet setWithObjects:@"videoDimensions", @"videoGravity", nil];
}

@end

@interface WebAVPictureInPicturePlayerLayerView : UIView
@end

static Class WebAVPictureInPicturePlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static Class getWebAVPictureInPicturePlayerLayerViewClass()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(getUIViewClass(), "WebAVPictureInPicturePlayerLayerView", 0);
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPictureInPicturePlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPictureInPicturePlayerLayerView_layerClass, "@@:");
    });
    
    return theClass;
}

@interface WebAVPlayerLayerView : __AVPlayerLayerView
@property (retain) UIView* videoView;
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

static UIView *WebAVPlayerLayerView_videoView(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayer = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayer playerLayer];
    CALayer* videoLayer = [webAVPlayerLayer videoSublayer];
    if (!videoLayer || !videoLayer.delegate)
        return nil;
    ASSERT([[videoLayer delegate] isKindOfClass:getUIViewClass()]);
    return (UIView *)[videoLayer delegate];
}

static void WebAVPlayerLayerView_setVideoView(id aSelf, SEL, UIView *videoView)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setVideoSublayer:[videoView layer]];
}

static void WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setVideoGravity:getAVLayerVideoGravityResizeAspect()];
    [pipPlayerLayer setVideoSublayer:playerLayer.videoSublayer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipPlayerLayer setVideoGravity:playerLayer.videoGravity];
    [pipPlayerLayer setModelVideoLayerFrame:playerLayer.modelVideoLayerFrame];
    [pipPlayerLayer setPlayerController:playerLayer.playerController];
    [pipPlayerLayer setFullscreenInterface:playerLayer.fullscreenInterface];
    [pipView addSubview:playerLayerView.videoView];
}

static void WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    if (UIView *videoView = playerLayerView.videoView)
        [playerLayerView addSubview:videoView];
    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];
    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setModelVideoLayerFrame:pipPlayerLayer.modelVideoLayerFrame];
}

static WebAVPictureInPicturePlayerLayerView *WebAVPlayerLayerView_pictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    WebAVPictureInPicturePlayerLayerView *pipView = [playerLayerView valueForKey:@"_pictureInPicturePlayerLayerView"];
    if (!pipView) {
        pipView = [[getWebAVPictureInPicturePlayerLayerViewClass() alloc] initWithFrame:CGRectZero];
        [playerLayerView setValue:pipView forKey:@"_pictureInPicturePlayerLayerView"];
    }
    return pipView;
}

static void WebAVPlayerLayerView_dealloc(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    RetainPtr<WebAVPictureInPicturePlayerLayerView> pipView = adoptNS([playerLayerView valueForKey:@"_pictureInPicturePlayerLayerView"]);
    [playerLayerView setValue:nil forKey:@"_pictureInPicturePlayerLayerView"];
    objc_super superClass { playerLayerView, get__AVPlayerLayerViewClass() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

#pragma mark - Methods

static Class getWebAVPlayerLayerViewClass()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(get__AVPlayerLayerViewClass(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPlayerLayerView_dealloc, "v@:");
        class_addMethod(theClass, @selector(setPlayerController:), (IMP)WebAVPlayerLayerView_setPlayerController, "v@:@");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPlayerLayerView_playerController, "@@:");
        class_addMethod(theClass, @selector(setVideoView:), (IMP)WebAVPlayerLayerView_setVideoView, "v@:@");
        class_addMethod(theClass, @selector(videoView), (IMP)WebAVPlayerLayerView_videoView, "@@:");
        class_addMethod(theClass, @selector(startRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(stopRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(pictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_pictureInPicturePlayerLayerView, "@@:");
        
        class_addIvar(theClass, "_pictureInPicturePlayerLayerView", sizeof(WebAVPictureInPicturePlayerLayerView *), log2(sizeof(WebAVPictureInPicturePlayerLayerView *)), "@");
        
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPlayerLayerView_layerClass, "@@:");
    });
    return theClass;
}

Ref<WebVideoFullscreenInterfaceAVKit> WebVideoFullscreenInterfaceAVKit::create(WebPlaybackSessionInterfaceAVKit& playbackSessionInterface)
{
    Ref<WebVideoFullscreenInterfaceAVKit> interface = adoptRef(*new WebVideoFullscreenInterfaceAVKit(playbackSessionInterface));
    [interface->m_playerViewControllerDelegate setFullscreenInterface:interface.ptr()];
    return interface;
}

WebVideoFullscreenInterfaceAVKit::WebVideoFullscreenInterfaceAVKit(WebPlaybackSessionInterfaceAVKit& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
    , m_playerViewControllerDelegate(adoptNS([[WebAVPlayerViewControllerDelegate alloc] init]))
    , m_watchdogTimer(*this, &WebVideoFullscreenInterfaceAVKit::watchdogTimerFired)
{
}

WebVideoFullscreenInterfaceAVKit::~WebVideoFullscreenInterfaceAVKit()
{
    WebAVPlayerController* playerController = this->playerController();
    if (playerController && playerController.externalPlaybackActive)
        externalPlaybackChanged(false, WebPlaybackSessionModel::TargetTypeNone, "");
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);
}

WebAVPlayerController *WebVideoFullscreenInterfaceAVKit::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);

    m_videoFullscreenModel = model;

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->addClient(*this);

    hasVideoChanged(m_videoFullscreenModel ? m_videoFullscreenModel->hasVideo() : false);
    videoDimensionsChanged(m_videoFullscreenModel ? m_videoFullscreenModel->videoDimensions() : FloatSize());
}

void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceAVKit::hasVideoChanged(bool hasVideo)
{
    [playerController() setHasEnabledVideo:hasVideo];
    [playerController() setHasVideo:hasVideo];
}

void WebVideoFullscreenInterfaceAVKit::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setVideoDimensions:videoDimensions];
    [playerController() setContentDimensions:videoDimensions];
    [m_playerLayerView setNeedsLayout];

    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[m_playerLayerView pictureInPicturePlayerLayerView];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipView setNeedsLayout];
}

void WebVideoFullscreenInterfaceAVKit::externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    [m_playerLayerView setHidden:enabled];
}

void WebVideoFullscreenInterfaceAVKit::applicationDidBecomeActive()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::applicationDidBecomeActive(%p)", this);
    if (m_shouldReturnToFullscreenAfterEnteringForeground && m_videoFullscreenModel && m_videoFullscreenModel->isVisible()) {
        [m_playerViewController stopPictureInPicture];
        return;
    }

    // If we are both in PiP and in Fullscreen (i.e., via auto-PiP), and we did not stop fullscreen upon returning, it must be
    // because the originating view is not visible, so hide the fullscreen window.
    if (isMode(HTMLMediaElementEnums::VideoFullscreenModeStandard | HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[protectedThis, this] (BOOL, NSError*) {
            [m_window setHidden:YES];
            [[m_playerViewController view] setHidden:YES];
        }];
    }
}

@interface UIWindow ()
- (BOOL)_isHostedInAnotherProcess;
@end

@interface UIViewController ()
@property (nonatomic, assign, setter=_setIgnoreAppSupportedOrientations:) BOOL _ignoreAppSupportedOrientations;
@end

void WebVideoFullscreenInterfaceAVKit::setupFullscreen(UIView& videoView, const WebCore::IntRect& initialRect, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback)
{
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::setupFullscreen(%p)", this);

    m_allowsPictureInPicturePlayback = allowsPictureInPicturePlayback;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    bool isInPictureInPictureMode = hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    m_mode = mode;
    m_parentView = parentView;
    m_parentWindow = parentView.window;

    if (![[parentView window] _isHostedInAnotherProcess]) {
        if (!m_window)
            m_window = adoptNS([allocUIWindowInstance() initWithFrame:[[getUIScreenClass() mainScreen] bounds]]);
        [m_window setBackgroundColor:clearUIColor()];
        if (!m_viewController)
            m_viewController = adoptNS([allocUIViewControllerInstance() init]);
        [[m_viewController view] setFrame:[m_window bounds]];
        [m_viewController _setIgnoreAppSupportedOrientations:YES];
        [m_window setRootViewController:m_viewController.get()];
        [m_window setWindowLevel:getUITextEffectsBeneathStatusBarWindowLevel() + 1];
        [m_window makeKeyAndVisible];
    }

    if (!m_playerLayerView)
        m_playerLayerView = adoptNS([[getWebAVPlayerLayerViewClass() alloc] init]);
    [m_playerLayerView setHidden:[playerController() isExternalPlaybackActive]];
    [m_playerLayerView setBackgroundColor:clearUIColor()];

    if (!isInPictureInPictureMode) {
        [m_playerLayerView setVideoView:&videoView];
        [m_playerLayerView addSubview:&videoView];
    }

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setModelVideoLayerFrame:CGRectMake(0, 0, initialRect.width(), initialRect.height())];
    [playerLayer setVideoDimensions:[playerController() contentDimensions]];
    playerLayer.fullscreenInterface = this;

    if (!m_playerViewController)
        m_playerViewController = adoptNS([allocAVPlayerViewControllerInstance() initWithPlayerLayerView:m_playerLayerView.get()]);

    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)playerController()];
    [m_playerViewController setDelegate:m_playerViewControllerDelegate.get()];
    [m_playerViewController setAllowsPictureInPicturePlayback:m_allowsPictureInPicturePlayback];

    [playerController() setPictureInPicturePossible:m_allowsPictureInPicturePlayback];

    if (m_viewController) {
        [m_viewController addChildViewController:m_playerViewController.get()];
        [[m_viewController view] addSubview:[m_playerViewController view]];
    } else
        [parentView addSubview:[m_playerViewController view]];

    [m_playerViewController view].frame = [parentView convertRect:initialRect toView:[m_playerViewController view].superview];

    [[m_playerViewController view] setBackgroundColor:clearUIColor()];
    [[m_playerViewController view] setAutoresizingMask:(UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin)];

    [[m_playerViewController view] setNeedsLayout];
    [[m_playerViewController view] layoutIfNeeded];

    [CATransaction commit];

    RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
    dispatch_async(dispatch_get_main_queue(), [protectedThis, this] {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didSetupFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreen(%p)", this);

    m_exitCompleted = false;
    m_exitRequested = false;
    m_enterRequested = true;

    if (mode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        enterPictureInPicture();
    else if (mode() == HTMLMediaElementEnums::VideoFullscreenModeStandard)
        enterFullscreenStandard();
    else
        ASSERT_NOT_REACHED();
}

void WebVideoFullscreenInterfaceAVKit::enterPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterPictureInPicture(%p)", this);
    
    if ([m_playerViewController isPictureInPicturePossible])
        [m_playerViewController startPictureInPicture];
    else
        failedToStartPictureInPicture();
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard(%p)", this);
    RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);

    if ([m_playerViewController isPictureInPictureActive]) {
        // NOTE: The fullscreen mode will be restored in prepareForPictureInPictureStopWithCompletionHandler().
        m_shouldReturnToFullscreenWhenStoppingPiP = true;
        [m_playerViewController stopPictureInPicture];
        return;
    }

    [m_playerViewController enterFullScreenAnimated:YES completionHandler:[this, protectedThis] (BOOL succeeded, NSError*) {
        UNUSED_PARAM(succeeded);
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard - lambda(%p) - succeeded(%s)", this, boolString(succeeded));
        [m_playerViewController setShowsPlaybackControls:YES];

        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didEnterFullscreen();
    }];
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen(const WebCore::IntRect& finalRect)
{
    m_watchdogTimer.stop();

    m_exitRequested = true;
    if (m_exitCompleted) {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didExitFullscreen();
        return;
    }
    
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::exitFullscreen(%p)", this);
    [m_playerViewController setShowsPlaybackControls:NO];
    
    [m_playerViewController view].frame = [m_parentView convertRect:finalRect toView:[m_playerViewController view].superview];

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];
    if ([playerLayer videoGravity] != getAVLayerVideoGravityResizeAspect())
        [playerLayer setVideoGravity:getAVLayerVideoGravityResizeAspect()];
    [[m_playerViewController view] layoutIfNeeded];

    if (isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        m_shouldReturnToFullscreenWhenStoppingPiP = false;
        [m_window setHidden:NO];
        [m_playerViewController stopPictureInPicture];
    } else if (isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture | HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[protectedThis, this] (BOOL, NSError*) {
            clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
            [m_window setHidden:NO];
            [m_playerViewController stopPictureInPicture];
        }];
    } else if (isMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[protectedThis, this] (BOOL, NSError*) {
            m_exitCompleted = true;

            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [m_playerLayerView setBackgroundColor:clearUIColor()];
            [[m_playerViewController view] setBackgroundColor:clearUIColor()];
            [CATransaction commit];

            dispatch_async(dispatch_get_main_queue(), [protectedThis, this]() {
                if (m_fullscreenChangeObserver)
                    m_fullscreenChangeObserver->didExitFullscreen();
            });
        }];
    };
}

@interface UIApplication ()
- (void)_setStatusBarOrientation:(UIInterfaceOrientation)o;
@end

@interface UIWindow ()
- (UIInterfaceOrientation)interfaceOrientation;
@end

void WebVideoFullscreenInterfaceAVKit::cleanupFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::cleanupFullscreen(%p)", this);
    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
        if (m_parentWindow)
            [[getUIApplicationClass() sharedApplication] _setStatusBarOrientation:[m_parentWindow interfaceOrientation]];
    }
    
    [m_playerViewController setDelegate:nil];
    [m_playerViewController setPlayerController:nil];
    
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        [m_playerViewController stopPictureInPicture];
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[] (BOOL, NSError *) { }];
    
    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];
    
    [m_playerLayerView removeFromSuperview];
    [[m_viewController view] removeFromSuperview];

    m_playerLayerView = nil;
    m_playerViewController = nil;
    m_window = nil;
    m_parentView = nil;
    m_parentWindow = nil;
    
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didCleanupFullscreen();

    m_enterRequested = false;
}

void WebVideoFullscreenInterfaceAVKit::invalidate()
{
    m_videoFullscreenModel = nil;
    m_fullscreenChangeObserver = nil;
    
    cleanupFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen()
{
    if (!m_enterRequested)
        return;
    
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        return;
    
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen(%p)", this);

    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];

    if (webPlaybackSessionModel() && m_videoFullscreenModel && !m_exitRequested) {
        webPlaybackSessionModel()->pause();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(bool visible, const IntRect& inlineRect)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    if (m_prepareToInlineCallback) {
        
        [m_playerViewController view].frame = [m_parentView convertRect:inlineRect toView:[m_playerViewController view].superview];

        std::function<void(bool)> callback = WTFMove(m_prepareToInlineCallback);
        callback(visible);
    }
}

bool WebVideoFullscreenInterfaceAVKit::mayAutomaticallyShowVideoPictureInPicture() const
{
    return [playerController() isPlaying] && m_mode == HTMLMediaElementEnums::VideoFullscreenModeStandard && supportsPictureInPicture();
}

void WebVideoFullscreenInterfaceAVKit::fullscreenMayReturnToInline(std::function<void(bool)> callback)
{
    m_prepareToInlineCallback = callback;
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->fullscreenMayReturnToInline();
}

void WebVideoFullscreenInterfaceAVKit::willStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStartPictureInPicture(%p)", this);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
}

void WebVideoFullscreenInterfaceAVKit::didStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStartPictureInPicture(%p)", this);
    m_shouldReturnToFullscreenAfterEnteringForeground = [m_playerViewController pictureInPictureWasStartedWhenEnteringBackground];
    [m_playerViewController setShowsPlaybackControls:YES];

    if (m_mode & HTMLMediaElementEnums::VideoFullscreenModeStandard) {
        if (![m_playerViewController pictureInPictureWasStartedWhenEnteringBackground]) {
            RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
            [m_playerViewController exitFullScreenAnimated:YES completionHandler:[protectedThis, this] (BOOL, NSError*) {
                [m_window setHidden:YES];
                [[m_playerViewController view] setHidden:YES];
            }];
        }
    } else {
        [m_window setHidden:YES];
        [[m_playerViewController view] setHidden:YES];
    }

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didEnterFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::failedToStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::failedToStartPictureInPicture(%p)", this);
    [m_playerViewController setShowsPlaybackControls:YES];

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return;

    m_exitCompleted = true;

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didEnterFullscreen();

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void WebVideoFullscreenInterfaceAVKit::willStopPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStopPictureInPicture(%p)", this);
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return;

    [m_window setHidden:NO];
    [[m_playerViewController view] setHidden:NO];

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void WebVideoFullscreenInterfaceAVKit::didStopPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStopPictureInPicture(%p)", this);
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        // ASSUMPTION: we are exiting pip because we are entering fullscreen
        clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        [m_playerViewController setShowsPlaybackControls:YES];

        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didEnterFullscreen();
        return;
    }

    m_exitCompleted = true;

    [m_playerLayerView setBackgroundColor:clearUIColor()];
    [[m_playerViewController view] setBackgroundColor:clearUIColor()];

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didExitFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(%p)", this);
    if (m_shouldReturnToFullscreenWhenStoppingPiP || m_shouldReturnToFullscreenAfterEnteringForeground) {
        m_shouldReturnToFullscreenWhenStoppingPiP = false;
        m_shouldReturnToFullscreenAfterEnteringForeground = false;

        // ASSUMPTION: we are exiting pip because we are entering fullscreen
        [m_window setHidden:NO];
        [[m_playerViewController view] setHidden:NO];

        [m_playerViewController enterFullScreenAnimated:YES completionHandler:^(BOOL success, NSError*) {
            setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
            completionHandler(success);
        }];
        return;
    }

    RefPtr<WebVideoFullscreenInterfaceAVKit> protectedThis(this);
    RetainPtr<id> strongCompletionHandler = adoptNS([completionHandler copy]);
    fullscreenMayReturnToInline([protectedThis, strongCompletionHandler](bool restored)  {
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler lambda(%p) - restored(%s)", protectedThis.get(), boolString(restored));
        void (^completionHandler)(BOOL restored) = strongCompletionHandler.get();
        completionHandler(restored);
    });
}

bool WebVideoFullscreenInterfaceAVKit::shouldExitFullscreenWithReason(WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason reason)
{
    if (!m_videoFullscreenModel)
        return true;

    if (reason == ExitFullScreenReason::PictureInPictureStarted) {
        if ([m_playerViewController pictureInPictureWasStartedWhenEnteringBackground])
            return false;

        m_shouldReturnToFullscreenWhenStoppingPiP = hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
        clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
        return true;
    }

    if (webPlaybackSessionModel() && (reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::RemoteControlStopEventReceived))
        webPlaybackSessionModel()->pause();
    
    m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, reason == ExitFullScreenReason::DoneButtonTapped);

    if (!m_watchdogTimer.isActive())
        m_watchdogTimer.startOneShot(defaultWatchdogTimerInterval);

    return false;
}

NO_RETURN_DUE_TO_ASSERT void WebVideoFullscreenInterfaceAVKit::watchdogTimerFired()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::watchdogTimerFired(%p) - no exit fullscreen response in %gs; forcing exit", this);
    ASSERT_NOT_REACHED();
    exitFullscreen(IntRect());
}

void WebVideoFullscreenInterfaceAVKit::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceAVKit::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

#endif // HAVE(AVKIT)

bool WebCore::supportsPictureInPicture()
{
#if PLATFORM(IOS) && HAVE(AVKIT)
    return [getAVPictureInPictureControllerClass() isPictureInPictureSupported];
#else
    return false;
#endif
}

#endif // PLATFORM(IOS)
