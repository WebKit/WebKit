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
#import "VideoFullscreenInterfaceAVKit.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(VIDEO_PRESENTATION_MODE)

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
#import <UIKit/UIKit.h>
#import <UIKit/UIWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if HAVE(PIP_CONTROLLER)
#import <AVKit/AVPictureInPictureController.h>
#endif

using namespace WebCore;

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

#if !PLATFORM(WATCHOS)
static const NSTimeInterval playbackControlsVisibleDurationAfterResettingVideoSource = 1.0;
#endif

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)

#if HAVE(PIP_CONTROLLER)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureControllerContentSource)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureContentViewController)
#endif

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

static const Seconds defaultWatchdogTimerInterval { 1_s };
static bool ignoreWatchdogForDebugging = false;

@interface AVPlayerViewController (Details)
@property (nonatomic) BOOL showsPlaybackControls;
@property (nonatomic) UIView* view;
@end

@class WebAVMediaSelectionOption;

@interface WebAVPlayerViewControllerDelegate : NSObject <AVPlayerViewControllerDelegate_WebKitOnly
#if HAVE(PIP_CONTROLLER)
    , AVPictureInPictureControllerDelegate
#endif
> {
    WeakPtr<VideoFullscreenInterfaceAVKit> _fullscreenInterface;
}
@property (assign) VideoFullscreenInterfaceAVKit* fullscreenInterface;
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@implementation WebAVPlayerViewControllerDelegate
- (VideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    ASSERT(isMainThread());
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(VideoFullscreenInterfaceAVKit*)fullscreenInterface
{
    ASSERT(isMainThread());
    _fullscreenInterface = *fullscreenInterface;
}

- (void)playerViewControllerWillStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStartPictureInPicture();
}

- (void)playerViewControllerDidStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStartPictureInPicture();
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController failedToStartPictureInPictureWithError:(NSError *)error
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(error);
    if (self.fullscreenInterface)
        self.fullscreenInterface->failedToStartPictureInPicture();
}

- (void)playerViewControllerWillStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStopPictureInPicture();
}

- (void)playerViewControllerDidStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStopPictureInPicture();
}

- (BOOL)playerViewControllerShouldAutomaticallyDismissAtPictureInPictureStart:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    return NO;
}

static VideoFullscreenInterfaceAVKit::ExitFullScreenReason convertToExitFullScreenReason(AVPlayerViewControllerExitFullScreenReason reason)
{
    switch (reason) {
    case AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped:
        return VideoFullscreenInterfaceAVKit::ExitFullScreenReason::DoneButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped:
        return VideoFullscreenInterfaceAVKit::ExitFullScreenReason::FullScreenButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted:
        return VideoFullscreenInterfaceAVKit::ExitFullScreenReason::PictureInPictureStarted;
    case AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled:
        return VideoFullscreenInterfaceAVKit::ExitFullScreenReason::PinchGestureHandled;
    case AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived:
        return VideoFullscreenInterfaceAVKit::ExitFullScreenReason::RemoteControlStopEventReceived;
    }
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    if (!self.fullscreenInterface)
        return YES;

    return self.fullscreenInterface->shouldExitFullscreenWithReason(convertToExitFullScreenReason(reason));
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(playerViewController);
    if (self.fullscreenInterface)
        self.fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}

- (BOOL)playerViewControllerShouldStartPictureInPictureFromInlineWhenEnteringBackground:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    return YES;
}

#if HAVE(PIP_CONTROLLER)

// AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStartPictureInPicture();
}

- (void)pictureInPictureControllerDidStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStartPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController failedToStartPictureInPictureWithError:(NSError *)error
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->failedToStartPictureInPicture();
}

- (void)pictureInPictureControllerWillStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->willStopPictureInPicture();
}

- (void)pictureInPictureControllerDidStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->didStopPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    if (self.fullscreenInterface)
        self.fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}

#endif

@end

@interface WebAVPictureInPicturePlayerLayerView : UIView
@end

static Class WebAVPictureInPicturePlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static WebAVPictureInPicturePlayerLayerView *allocWebAVPictureInPicturePlayerLayerViewInstance()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(PAL::getUIViewClass(), "WebAVPictureInPicturePlayerLayerView", 0);
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPictureInPicturePlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPictureInPicturePlayerLayerView_layerClass, "@@:");
    });

    return (WebAVPictureInPicturePlayerLayerView *)[theClass alloc];
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

static UIView *WebAVPlayerLayerView_videoView(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    CALayer* videoLayer = [webAVPlayerLayer videoSublayer];
    if (!videoLayer || !videoLayer.delegate)
        return nil;
    ASSERT([[videoLayer delegate] isKindOfClass:PAL::getUIViewClass()]);
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
    auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];

    auto *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setVideoGravity:AVLayerVideoGravityResizeAspect];
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
        pipView = [allocWebAVPictureInPicturePlayerLayerViewInstance() initWithFrame:CGRectZero];
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

static WebAVPlayerLayerView *allocWebAVPlayerLayerViewInstance()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ASSERT(get__AVPlayerLayerViewClass());
        theClass = objc_allocateClassPair(get__AVPlayerLayerViewClass(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPlayerLayerView_dealloc, "v@:");
        class_addMethod(theClass, @selector(setPlayerController:), (IMP)WebAVPlayerLayerView_setPlayerController, "v@:@");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPlayerLayerView_playerController, "@@:");
        class_addMethod(theClass, @selector(setVideoView:), (IMP)WebAVPlayerLayerView_setVideoView, "v@:@");
        class_addMethod(theClass, @selector(videoView), (IMP)WebAVPlayerLayerView_videoView, "@@:");
        class_addMethod(theClass, @selector(playerLayer), (IMP)WebAVPlayerLayerView_playerLayer, "@@:");
        class_addMethod(theClass, @selector(startRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(stopRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(pictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_pictureInPicturePlayerLayerView, "@@:");

        class_addIvar(theClass, "_pictureInPicturePlayerLayerView", sizeof(WebAVPictureInPicturePlayerLayerView *), log2(sizeof(WebAVPictureInPicturePlayerLayerView *)), "@");

        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPlayerLayerView_layerClass, "@@:");
    });
    return (WebAVPlayerLayerView *)[theClass alloc];
}

#if HAVE(PIP_CONTROLLER)

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

#endif

NS_ASSUME_NONNULL_BEGIN
@interface WebAVPlayerViewController : NSObject<AVPlayerViewControllerDelegate>
- (instancetype)initWithFullscreenInterface:(VideoFullscreenInterfaceAVKit *)interface;
- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)startPictureInPicture;
- (void)stopPictureInPicture;

- (BOOL)playerViewControllerShouldHandleDoneButtonTap:(AVPlayerViewController *)playerViewController;
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end
NS_ASSUME_NONNULL_END

@implementation WebAVPlayerViewController {
    VideoFullscreenInterfaceAVKit *_fullscreenInterface;
#if PLATFORM(WATCHOS)
    RetainPtr<UIViewController> _presentingViewController;
#endif
    RetainPtr<AVPlayerViewController> _avPlayerViewController;
    RetainPtr<NSTimer> _startPictureInPictureTimer;
    WeakObjCPtr<WebAVPlayerViewControllerDelegate> _delegate;

#if HAVE(PIP_CONTROLLER)
    RetainPtr<AVPictureInPictureController> _pipController;
    RetainPtr<WebAVPictureInPictureContentViewController> _pipContentViewController;
#endif
}

- (instancetype)initWithFullscreenInterface:(VideoFullscreenInterfaceAVKit *)interface
{
    if (!(self = [super init]))
        return nil;

    _fullscreenInterface = interface;
    _avPlayerViewController = adoptNS([allocAVPlayerViewControllerInstance() initWithPlayerLayerView:interface->playerLayerView()]);
    _avPlayerViewController.get().modalPresentationStyle = UIModalPresentationOverFullScreen;
#if PLATFORM(WATCHOS)
    _avPlayerViewController.get().delegate = self;
#endif

#if HAVE(PIP_CONTROLLER)
    auto *playerController = static_cast<AVPlayerController *>(interface->playerController());
    _pipContentViewController = adoptNS([allocWebAVPictureInPictureContentViewControllerInstance() initWithController:playerController]);

    auto source = adoptNS([allocAVPictureInPictureControllerContentSourceInstance() initWithSourceView:static_cast<UIView *>(interface->playerLayerView()) contentViewController:_pipContentViewController.get() playerController:playerController]);

    _pipController = adoptNS([allocAVPictureInPictureControllerInstance() initWithContentSource:source.get()]);
#endif

    return self;
}

- (void)dealloc
{
    if (_startPictureInPictureTimer) {
        [self removeObserver];
        [_startPictureInPictureTimer invalidate];
        _startPictureInPictureTimer = nil;
    }
#if HAVE(PIP_CONTROLLER)
    _pipContentViewController = nil;
    _pipController = nil;
#endif
    [super dealloc];
}

- (BOOL)playerViewControllerShouldHandleDoneButtonTap:(AVPlayerViewController *)playerViewController
{
    ASSERT(playerViewController == _avPlayerViewController.get());
    if (!_delegate)
        return YES;

    return [_delegate playerViewController:playerViewController shouldExitFullScreenWithReason:AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped];
}

- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID
{
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([_avPlayerViewController respondsToSelector:@selector(setWebKitOverrideRouteSharingPolicy:routingContextUID:)])
        [_avPlayerViewController setWebKitOverrideRouteSharingPolicy:routeSharingPolicy routingContextUID:routingContextUID];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
}

- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler
{
#if PLATFORM(WATCHOS)
    _presentingViewController = _fullscreenInterface->presentingViewController();

    _avPlayerViewController.get().view.frame = _presentingViewController.get().view.frame;
    [_presentingViewController presentViewController:_fullscreenInterface->fullscreenViewController() animated:animated completion:^{
        if (completionHandler)
            completionHandler(YES, nil);
    }];
#else
    [_avPlayerViewController enterFullScreenAnimated:animated completionHandler:completionHandler];
#endif
}

- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler
{
#if PLATFORM(WATCHOS)
    if (!_presentingViewController)
        return;

    [_presentingViewController dismissViewControllerAnimated:animated completion:^{
        _presentingViewController = nil;
        if (completionHandler)
            completionHandler(YES, nil);
    }];
#else
    [_avPlayerViewController exitFullScreenAnimated:animated completionHandler:completionHandler];
#endif
}

#if PLATFORM(WATCHOS)
#define MY_NO_RETURN NO_RETURN_DUE_TO_ASSERT
#else
#define MY_NO_RETURN
#endif

static const NSTimeInterval startPictureInPictureTimeInterval = 5.0;

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
        [self startPictureInPicture];
    });
}

- (void)initObserver
{
#if HAVE(PIP_CONTROLLER)
    [_pipController addObserver:self forKeyPath:@"pictureInPicturePossible" options:NSKeyValueObservingOptionNew context:nil];
#else
    [_avPlayerViewController addObserver:self forKeyPath:@"pictureInPicturePossible" options:NSKeyValueObservingOptionNew context:nil];
#endif
}

- (void)removeObserver
{
#if HAVE(PIP_CONTROLLER)
    [_pipController removeObserver:self forKeyPath:@"pictureInPicturePossible" context:nil];
#else
    [_avPlayerViewController removeObserver:self forKeyPath:@"pictureInPicturePossible" context:nil];
#endif
}

- (void)tryToStartPictureInPicture MY_NO_RETURN
{
#if PLATFORM(WATCHOS)
    UNUSED_VARIABLE(startPictureInPictureTimeInterval);
    ASSERT_NOT_REACHED();
#else
    if (_startPictureInPictureTimer)
        return;

    if ([self isPictureInPicturePossible]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startPictureInPicture];
        });
        return;
    }

    _startPictureInPictureTimer = [NSTimer scheduledTimerWithTimeInterval:startPictureInPictureTimeInterval repeats:NO block:^(NSTimer *_Nonnull) {
        [self removeObserver];
        _startPictureInPictureTimer = nil;
        if (_fullscreenInterface)
            _fullscreenInterface->failedToStartPictureInPicture();
    }];

    [self initObserver];
#endif
}

- (void)startPictureInPicture MY_NO_RETURN
{
#if PLATFORM(WATCHOS)
    ASSERT_NOT_REACHED();
#elif HAVE(PIP_CONTROLLER)
    [_pipController startPictureInPicture];
#else
    [_avPlayerViewController startPictureInPicture];
#endif
}

- (void)stopPictureInPicture MY_NO_RETURN
{
#if PLATFORM(WATCHOS)
    ASSERT_NOT_REACHED();
#elif HAVE(PIP_CONTROLLER)
    [_pipController stopPictureInPicture];
#else
    [_avPlayerViewController stopPictureInPicture];
#endif
}

- (BOOL)isPictureInPicturePossible
{
#if PLATFORM(WATCHOS)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPicturePossible];
#else
    return [_avPlayerViewController isPictureInPicturePossible];
#endif
}

- (BOOL)isPictureInPictureActive
{
#if PLATFORM(WATCHOS)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPictureActive];
#else
    return [_avPlayerViewController isPictureInPictureActive];
#endif
}

- (BOOL)pictureInPictureActive
{
#if PLATFORM(WATCHOS)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPictureActive];
#else
    return [_avPlayerViewController isPictureInPictureActive];
#endif
}

- (BOOL)pictureInPictureWasStartedWhenEnteringBackground
{
#if PLATFORM(WATCHOS)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController pictureInPictureWasStartedWhenEnteringBackground];
#else
    return [_avPlayerViewController pictureInPictureWasStartedWhenEnteringBackground];
#endif
}

- (UIView *)view
{
    return [_avPlayerViewController view];
}

#if !PLATFORM(WATCHOS)
- (void)flashPlaybackControlsWithDuration:(NSTimeInterval)duration
{
    if ([_avPlayerViewController respondsToSelector:@selector(flashPlaybackControlsWithDuration:)])
        [_avPlayerViewController flashPlaybackControlsWithDuration:duration];
}
#endif

- (BOOL)showsPlaybackControls
{
#if PLATFORM(WATCHOS)
    return YES;
#else
    return [_avPlayerViewController showsPlaybackControls];
#endif
}

- (void)setShowsPlaybackControls:(BOOL)showsPlaybackControls
{
#if PLATFORM(WATCHOS)
    UNUSED_PARAM(showsPlaybackControls);
#else
    [_avPlayerViewController setShowsPlaybackControls:showsPlaybackControls];
#endif
}

- (void)setAllowsPictureInPicturePlayback:(BOOL)allowsPictureInPicturePlayback
{
#if PLATFORM(WATCHOS)
    UNUSED_PARAM(allowsPictureInPicturePlayback);
#else
    [_avPlayerViewController setAllowsPictureInPicturePlayback:allowsPictureInPicturePlayback];
#endif
}

- (void)setDelegate:(WebAVPlayerViewControllerDelegate *)delegate
{
#if PLATFORM(WATCHOS)
    ASSERT(!delegate || [delegate respondsToSelector:@selector(playerViewController:shouldExitFullScreenWithReason:)]);
    _delegate = delegate;
#else
    [_avPlayerViewController setDelegate:delegate];
#endif

#if HAVE(PIP_CONTROLLER)
    [_pipController setDelegate:delegate];
#endif
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    [_avPlayerViewController setPlayerController:playerController];
}

- (AVPlayerViewController *)avPlayerViewController
{
    return _avPlayerViewController.get();
}

- (void)removeFromParentViewController
{
    [_avPlayerViewController removeFromParentViewController];
}

@end

Ref<VideoFullscreenInterfaceAVKit> VideoFullscreenInterfaceAVKit::create(PlaybackSessionInterfaceAVKit& playbackSessionInterface)
{
    Ref<VideoFullscreenInterfaceAVKit> interface = adoptRef(*new VideoFullscreenInterfaceAVKit(playbackSessionInterface));
    [interface->m_playerViewControllerDelegate setFullscreenInterface:interface.ptr()];
    return interface;
}

VideoFullscreenInterfaceAVKit::VideoFullscreenInterfaceAVKit(PlaybackSessionInterfaceAVKit& playbackSessionInterface)
    : m_playbackSessionInterface(playbackSessionInterface)
    , m_playerViewControllerDelegate(adoptNS([[WebAVPlayerViewControllerDelegate alloc] init]))
    , m_watchdogTimer(RunLoop::main(), this, &VideoFullscreenInterfaceAVKit::watchdogTimerFired)
{
}

VideoFullscreenInterfaceAVKit::~VideoFullscreenInterfaceAVKit()
{
    WebAVPlayerController* playerController = this->playerController();
    if (playerController && playerController.externalPlaybackActive)
        externalPlaybackChanged(false, PlaybackSessionModel::ExternalPlaybackTargetType::TargetTypeNone, emptyString());
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);
}

WebAVPlayerController *VideoFullscreenInterfaceAVKit::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

AVPlayerViewController *VideoFullscreenInterfaceAVKit::avPlayerViewController() const
{
    return [m_playerViewController avPlayerViewController];
}

void VideoFullscreenInterfaceAVKit::setVideoFullscreenModel(VideoFullscreenModel* model)
{
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->removeClient(*this);

    m_videoFullscreenModel = model;

    if (m_videoFullscreenModel) {
        m_videoFullscreenModel->addClient(*this);
        m_videoFullscreenModel->requestRouteSharingPolicyAndContextUID([this, protectedThis = Ref { *this }] (RouteSharingPolicy policy, String contextUID) {
            m_routeSharingPolicy = policy;
            m_routingContextUID = contextUID;

            if (m_playerViewController && !m_routingContextUID.isEmpty())
                [m_playerViewController setWebKitOverrideRouteSharingPolicy:(NSUInteger)m_routeSharingPolicy routingContextUID:m_routingContextUID];
        });
    }

    hasVideoChanged(m_videoFullscreenModel ? m_videoFullscreenModel->hasVideo() : false);
    videoDimensionsChanged(m_videoFullscreenModel ? m_videoFullscreenModel->videoDimensions() : FloatSize());
}

void VideoFullscreenInterfaceAVKit::setVideoFullscreenChangeObserver(VideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void VideoFullscreenInterfaceAVKit::hasVideoChanged(bool hasVideo)
{
    [playerController() setHasEnabledVideo:hasVideo];
    [playerController() setHasVideo:hasVideo];
}

void VideoFullscreenInterfaceAVKit::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (videoDimensions.isZero())
        return;

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setVideoDimensions:videoDimensions];
    [playerController() setContentDimensions:videoDimensions];
    [m_playerLayerView setNeedsLayout];

    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[m_playerLayerView pictureInPicturePlayerLayerView];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipView setNeedsLayout];
}

void VideoFullscreenInterfaceAVKit::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
    [m_playerLayerView setHidden:enabled];
}

bool VideoFullscreenInterfaceAVKit::pictureInPictureWasStartedWhenEnteringBackground() const
{
    return [m_playerViewController pictureInPictureWasStartedWhenEnteringBackground];
}

#if PLATFORM(WATCHOS)
static UIViewController *fallbackViewController(UIView *view)
{
    for (UIView *currentView = view; currentView; currentView = currentView.superview) {
        if (UIViewController *viewController = [PAL::getUIViewControllerClass() viewControllerForView:currentView]) {
            if (![viewController parentViewController])
                return viewController;
        }
    }

    LOG_ERROR("Failed to find a view controller suitable to present fullscreen video");
    return nil;
}

UIViewController *VideoFullscreenInterfaceAVKit::presentingViewController()
{
    auto *controller = videoFullscreenModel() ? videoFullscreenModel()->presentingViewController() : nil;
    if (!controller)
        controller = fallbackViewController(m_parentView.get());

    return controller;
}
#endif

void VideoFullscreenInterfaceAVKit::applicationDidBecomeActive()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::applicationDidBecomeActive(%p)", this);
}

void VideoFullscreenInterfaceAVKit::setupFullscreen(UIView& videoView, const FloatRect& initialRect, const FloatSize& videoDimensions, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::setupFullscreen(%p)", this);

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

void VideoFullscreenInterfaceAVKit::enterFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::enterFullscreen(%p) %d", this, mode());

    doEnterFullscreen();
}

bool VideoFullscreenInterfaceAVKit::exitFullscreen(const FloatRect& finalRect)
{
    m_watchdogTimer.stop();

    // VideoFullscreenManager may ask a video to exit standby while the video
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

void VideoFullscreenInterfaceAVKit::cleanupFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::cleanupFullscreen(%p)", this);
    m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason = false;

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

    [m_playerViewController setDelegate:nil];
    [m_playerViewController setPlayerController:nil];

    if (m_currentMode.hasPictureInPicture())
        [m_playerViewController stopPictureInPicture];

    if (m_currentMode.hasFullscreen()) {
        [[m_playerViewController view] layoutIfNeeded];
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[] (BOOL success, NSError* error) {
            if (!success)
                WTFLogAlways("-[AVPlayerViewController exitFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);
        }];
    }

    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];

    [m_playerLayerView removeFromSuperview];
    [[m_viewController view] removeFromSuperview];

    m_playerLayerView = nil;
    m_playerViewController = nil;
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

void VideoFullscreenInterfaceAVKit::invalidate()
{
    m_videoFullscreenModel = nullptr;
    m_fullscreenChangeObserver = nullptr;

    m_watchdogTimer.stop();
    m_enteringPictureInPicture = false;
    cleanupFullscreen();
}

void VideoFullscreenInterfaceAVKit::modelDestroyed()
{
    ASSERT(isUIThread());
    invalidate();
}

void VideoFullscreenInterfaceAVKit::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
#if !PLATFORM(WATCHOS)
    if (!identifier)
        [m_playerViewController flashPlaybackControlsWithDuration:playbackControlsVisibleDurationAfterResettingVideoSource];
#endif

    m_playerIdentifier = identifier;
}

void VideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen()
{
    if (m_currentMode.hasPictureInPicture())
        return;

    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen(%p)", this);

    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];

    if (playbackSessionModel() && m_videoFullscreenModel) {
        playbackSessionModel()->pause();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void VideoFullscreenInterfaceAVKit::preparedToReturnToInline(bool visible, const FloatRect& inlineRect)
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    setInlineRect(inlineRect, visible);
    [[m_playerViewController view] setNeedsLayout];
    [[m_playerViewController view] layoutIfNeeded];
    if (m_prepareToInlineCallback) {
        WTF::Function<void(bool)> callback = WTFMove(m_prepareToInlineCallback);
        callback(visible);
    }
}

void VideoFullscreenInterfaceAVKit::preparedToExitFullscreen()
{
#if PLATFORM(WATCHOS)
    if (!m_waitingForPreparedToExit)
        return;

    m_waitingForPreparedToExit = false;
    ASSERT(m_videoFullscreenModel);
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, true);
#endif
}

bool VideoFullscreenInterfaceAVKit::mayAutomaticallyShowVideoPictureInPicture() const
{
    return [playerController() isPlaying] && (m_standby || m_currentMode.isFullscreen()) && supportsPictureInPicture();
}

void VideoFullscreenInterfaceAVKit::prepareForPictureInPictureStop(WTF::Function<void(bool)>&& callback)
{
    m_prepareToInlineCallback = WTFMove(callback);
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->fullscreenMayReturnToInline();
}

void VideoFullscreenInterfaceAVKit::willStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::willStartPictureInPicture(%p)", this);
    m_enteringPictureInPicture = true;

    if (m_standby && !m_currentMode.hasVideo()) {
        [m_window setHidden:NO];
        [[m_playerViewController view] setHidden:NO];
    }

    if (!m_hasVideoContentLayer)
        m_fullscreenChangeObserver->requestVideoContentLayer();
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->willEnterPictureInPicture();
}

void VideoFullscreenInterfaceAVKit::didStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::didStartPictureInPicture(%p)", this);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, !m_enterFullscreenNeedsEnterPictureInPicture);
    [m_playerViewController setShowsPlaybackControls:YES];
    [m_viewController _setIgnoreAppSupportedOrientations:NO];

    if (m_currentMode.hasFullscreen()) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;
        [[m_playerViewController view] layoutIfNeeded];
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[protectedThis = Ref { *this }, this] (BOOL success, NSError *error) {
            exitFullscreenHandler(success, error);
        }];
    } else {
        if (m_standby && !m_blocksReturnToFullscreenFromPictureInPicture)
            m_shouldReturnToFullscreenWhenStoppingPictureInPicture = true;

        [m_window setHidden:YES];
        [[m_playerViewController view] setHidden:YES];
    }

    if (m_enterFullscreenNeedsEnterPictureInPicture)
        doEnterFullscreen();
}

void VideoFullscreenInterfaceAVKit::failedToStartPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::failedToStartPictureInPicture(%p)", this);
    [m_playerViewController setShowsPlaybackControls:YES];

    m_targetMode.setPictureInPicture(false);
    if (m_currentMode.hasFullscreen())
        return;

    if (m_videoFullscreenModel) {
        m_videoFullscreenModel->failedToEnterPictureInPicture();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
        m_videoFullscreenModel->fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
    if (m_fullscreenChangeObserver) {
        m_fullscreenChangeObserver->failedToEnterFullscreen();
        m_changingStandbyOnly = false;
    }

    m_enterFullscreenNeedsExitPictureInPicture = false;
    m_exitFullscreenNeedsExitPictureInPicture = false;
}

void VideoFullscreenInterfaceAVKit::willStopPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::willStopPictureInPicture(%p)", this);

    m_exitingPictureInPicture = true;
    m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

    if (m_currentMode.hasFullscreen())
        return;

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->willExitPictureInPicture();
}

void VideoFullscreenInterfaceAVKit::didStopPictureInPicture()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::didStopPictureInPicture(%p)", this);
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
        [m_playerViewController setShowsPlaybackControls:YES];

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
    [[m_playerViewController view] setBackgroundColor:clearUIColor()];

    if (m_enterFullscreenNeedsExitPictureInPicture)
        doEnterFullscreen();

    if (m_exitFullscreenNeedsExitPictureInPicture)
        doExitFullscreen();
}

void VideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(%p)", this);

    if (m_shouldReturnToFullscreenWhenStoppingPictureInPicture) {
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

        [m_window setHidden:NO];
        [[m_playerViewController view] setHidden:NO];

        [[m_playerViewController view] layoutIfNeeded];
        [m_playerViewController enterFullScreenAnimated:YES completionHandler:^(BOOL success, NSError *error) {
            enterFullscreenHandler(success, error);
            completionHandler(success);
        }];

        if (m_standby) {
            m_returningToStandby = true;
            [m_playerViewController setAllowsPictureInPicturePlayback:NO];
        }

        return;
    }

    prepareForPictureInPictureStop([protectedThis = Ref { *this }, strongCompletionHandler = adoptNS([completionHandler copy])](bool restored)  {
        LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler lambda(%p) - restored(%s)", protectedThis.ptr(), boolString(restored));
        ((void (^)(BOOL))strongCompletionHandler.get())(restored);
    });
}

bool VideoFullscreenInterfaceAVKit::shouldExitFullscreenWithReason(VideoFullscreenInterfaceAVKit::ExitFullScreenReason reason)
{
    // AVKit calls playerViewController:shouldExitFullScreenWithReason in the scenario that the exit fullscreen request
    // is from the web process (e.g., through Javascript API videoElement.webkitExitFullscreen()).
    // We have to ignore the callback in that case.
    if (m_shouldIgnoreAVKitCallbackAboutExitFullscreenReason)
        return true;

    if (!m_videoFullscreenModel)
        return true;

    if (reason == ExitFullScreenReason::PictureInPictureStarted)
        return false;

    if (playbackSessionModel() && (reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::RemoteControlStopEventReceived))
        playbackSessionModel()->pause();

    if (!m_watchdogTimer.isActive() && !ignoreWatchdogForDebugging)
        m_watchdogTimer.startOneShot(defaultWatchdogTimerInterval);

#if PLATFORM(WATCHOS)
    if (m_fullscreenChangeObserver) {
        m_waitingForPreparedToExit = true;
        m_fullscreenChangeObserver->willExitFullscreen();
        return false;
    }
#endif

    BOOL finished = reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::PinchGestureHandled;
    ASSERT(m_videoFullscreenModel);
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, finished);

    return false;
}

void VideoFullscreenInterfaceAVKit::setHasVideoContentLayer(bool value)
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

void VideoFullscreenInterfaceAVKit::setInlineRect(const FloatRect& inlineRect, bool visible)
{
    m_inlineRect = inlineRect;
    m_inlineIsVisible = visible;
    m_hasUpdatedInlineRect = true;

    if (m_playerViewController && m_parentView) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [m_playerViewController view].frame = [m_parentView convertRect:inlineRect toView:[m_playerViewController view].superview];
        [CATransaction commit];
    }

    if (m_setupNeedsInlineRect)
        doSetup();

    if (m_exitFullscreenNeedInlineRect)
        doExitFullscreen();
}

void VideoFullscreenInterfaceAVKit::doSetup()
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

#if !PLATFORM(WATCHOS)
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
#endif // !PLATFORM(WATCHOS)

    if (!m_playerLayerView)
        m_playerLayerView = adoptNS([allocWebAVPlayerLayerViewInstance() init]);
    [m_playerLayerView setHidden:[playerController() isExternalPlaybackActive]];
    [m_playerLayerView setBackgroundColor:clearUIColor()];

    if (!m_currentMode.hasPictureInPicture()) {
        [m_playerLayerView setVideoView:m_videoView.get()];
        [m_playerLayerView addSubview:m_videoView.get()];
    }

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    auto modelVideoLayerFrame = CGRectMake(0, 0, m_inlineRect.width(), m_inlineRect.height());
    [playerLayer setModelVideoLayerFrame:modelVideoLayerFrame];
    [playerLayer setVideoDimensions:[playerController() contentDimensions]];
    playerLayer.fullscreenInterface = this;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->setVideoLayerFrame(modelVideoLayerFrame);

    if (!m_playerViewController)
        m_playerViewController = adoptNS([[WebAVPlayerViewController alloc] initWithFullscreenInterface:this]);

    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)playerController()];
    [m_playerViewController setDelegate:m_playerViewControllerDelegate.get()];
    [m_playerViewController setAllowsPictureInPicturePlayback:m_allowsPictureInPicturePlayback];
    [playerController() setAllowsPictureInPicture:m_allowsPictureInPicturePlayback];
    if (!m_routingContextUID.isEmpty())
        [m_playerViewController setWebKitOverrideRouteSharingPolicy:(NSUInteger)m_routeSharingPolicy routingContextUID:m_routingContextUID];

#if PLATFORM(WATCHOS)
    m_viewController = videoFullscreenModel() ? videoFullscreenModel()->createVideoFullscreenViewController(m_playerViewController.get().avPlayerViewController) : nil;
#endif

    if (m_viewController) {
        [m_viewController addChildViewController:m_playerViewController.get().avPlayerViewController];
        [[m_viewController view] addSubview:[m_playerViewController view]];
    } else
        [m_parentView addSubview:[m_playerViewController view]];

    [m_playerViewController view].frame = [m_parentView convertRect:m_inlineRect toView:[m_playerViewController view].superview];
    [[m_playerViewController view] setBackgroundColor:clearUIColor()];
    [[m_playerViewController view] setAutoresizingMask:(UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin)];

    [[m_playerViewController view] setNeedsLayout];
    [[m_playerViewController view] layoutIfNeeded];

    if (m_targetStandby && !m_currentMode.hasVideo()) {
        [m_window setHidden:YES];
        [[m_playerViewController view] setHidden:YES];
    }

    [CATransaction commit];

    finalizeSetup();
}

void VideoFullscreenInterfaceAVKit::preparedToReturnToStandby()
{
    if (!m_returningToStandby)
        return;

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture, true);
}

void VideoFullscreenInterfaceAVKit::finalizeSetup()
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

void VideoFullscreenInterfaceAVKit::doEnterFullscreen()
{
    m_standby = m_targetStandby;

    [[m_playerViewController view] layoutIfNeeded];
    if (m_targetMode.hasFullscreen() && !m_currentMode.hasFullscreen()) {
        [m_window setHidden:NO];
        [m_playerViewController enterFullScreenAnimated:YES completionHandler:[this, protectedThis = Ref { *this }] (BOOL success, NSError *error) {
            enterFullscreenHandler(success, error, NextAction::NeedsEnterFullScreen);
        }];
        return;
    }

    if (m_targetMode.hasPictureInPicture() && !m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsEnterPictureInPicture = true;
        [m_playerViewController tryToStartPictureInPicture];
        return;
    }
    m_enterFullscreenNeedsEnterPictureInPicture = false;

    if (!m_targetMode.hasFullscreen() && m_currentMode.hasFullscreen()) {
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[protectedThis = Ref { *this }, this] (BOOL success, NSError *error) {
            exitFullscreenHandler(success, error, NextAction::NeedsEnterFullScreen);
        }];
        return;
    }

    if (!m_targetMode.hasPictureInPicture() && m_currentMode.hasPictureInPicture()) {
        m_enterFullscreenNeedsExitPictureInPicture = true;
        [m_playerViewController stopPictureInPicture];
        return;
    }
    m_enterFullscreenNeedsExitPictureInPicture = false;

    if (m_fullscreenChangeObserver) {
        FloatSize size;
        if (m_currentMode.hasPictureInPicture()) {
            auto *pipView = (WebAVPictureInPicturePlayerLayerView *)[m_playerLayerView pictureInPicturePlayerLayerView];
            auto *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
            auto videoFrame = [pipPlayerLayer calculateTargetVideoFrame];
            size = FloatSize(videoFrame.size());
        }
        m_fullscreenChangeObserver->didEnterFullscreen(size);
        m_enteringPictureInPicture = false;
        m_changingStandbyOnly = false;
    }

    if (m_currentMode.hasPictureInPicture() && m_videoFullscreenModel)
        m_videoFullscreenModel->didEnterPictureInPicture();
}

void VideoFullscreenInterfaceAVKit::doExitFullscreen()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::doExitFullscreen(%p)", this);

    if (m_currentMode.hasVideo() && !m_hasUpdatedInlineRect && m_fullscreenChangeObserver) {
        m_exitFullscreenNeedInlineRect = true;
        m_fullscreenChangeObserver->requestUpdateInlineRect();
        return;
    }
    m_exitFullscreenNeedInlineRect = false;

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[protectedThis = Ref { *this }, this] (BOOL success, NSError *error) {
            exitFullscreenHandler(success, error, NextAction::NeedsExitFullScreen);
        }];
        return;
    }

    if (m_currentMode.hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        m_exitFullscreenNeedsExitPictureInPicture = true;
        m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;
        [m_window setHidden:NO];
        [m_playerViewController stopPictureInPicture];
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

void VideoFullscreenInterfaceAVKit::exitFullscreenHandler(BOOL success, NSError* error, NextActions nextActions)
{
    if (!success)
        WTFLogAlways("-[AVPlayerViewController exitFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);

    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::didExitFullscreen(%p) - %d", this, success);

    clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, false);

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        [m_window setHidden:YES];
        [[m_playerViewController view] setHidden:YES];
    } else {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [m_playerLayerView setBackgroundColor:clearUIColor()];
        [[m_playerViewController view] setBackgroundColor:clearUIColor()];
        [CATransaction commit];
    }

    if (nextActions.contains(NextAction::NeedsEnterFullScreen))
        doEnterFullscreen();

    if (nextActions.contains(NextAction::NeedsExitFullScreen))
        doExitFullscreen();
}

void VideoFullscreenInterfaceAVKit::enterFullscreenHandler(BOOL success, NSError* error, NextActions nextActions)
{
    if (!success) {
        WTFLogAlways("-[AVPlayerViewController enterFullScreenAnimated:completionHandler:] failed with error %s", [[error localizedDescription] UTF8String]);
        ASSERT_NOT_REACHED();
        return;
    }

    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::enterFullscreenStandard - lambda(%p)", this);
    if (!m_standby) {
        setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard, !nextActions.contains(NextAction::NeedsEnterFullScreen));
        [m_playerViewController setShowsPlaybackControls:YES];
    } else
        [m_playerViewController setShowsPlaybackControls:NO];

    if (nextActions.contains(NextAction::NeedsEnterFullScreen))
        doEnterFullscreen();
}

void VideoFullscreenInterfaceAVKit::returnToStandby()
{
    m_returningToStandby = false;
    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didSetupFullscreen();
}

NO_RETURN_DUE_TO_ASSERT void VideoFullscreenInterfaceAVKit::watchdogTimerFired()
{
    LOG(Fullscreen, "VideoFullscreenInterfaceAVKit::watchdogTimerFired(%p) - no exit fullscreen response in %gs; forcing fullscreen hidden.", this, defaultWatchdogTimerInterval.value());
    ASSERT_NOT_REACHED();
    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];
}

void VideoFullscreenInterfaceAVKit::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.setMode(mode);
    // Mode::mode() can be 3 (VideoFullscreenModeStandard | VideoFullscreenModePictureInPicture).
    // HTMLVideoElement does not expect such a value in the fullscreenModeChanged() callback.
    if (m_videoFullscreenModel && shouldNotifyModel)
        m_videoFullscreenModel->fullscreenModeChanged(mode);
}

void VideoFullscreenInterfaceAVKit::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((~m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.clearMode(mode);
    if (m_videoFullscreenModel && shouldNotifyModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_currentMode.mode());
}

bool VideoFullscreenInterfaceAVKit::isPlayingVideoInEnhancedFullscreen() const
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
#if ENABLE(VIDEO_PRESENTATION_MODE) && !PLATFORM(WATCHOS)
    if (isPictureInPictureSupported.has_value())
        return *isPictureInPictureSupported;
    return [getAVPictureInPictureControllerClass() isPictureInPictureSupported];
#else
    return false;
#endif
}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(VIDEO_PRESENTATION_MODE)
