/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "VideoPresentationInterfaceAVKit.h"

#if PLATFORM(IOS_FAMILY) && HAVE(AVKIT)

#import "Logging.h"
#import "PictureInPictureSupport.h"
#import "PlaybackSessionInterfaceAVKit.h"
#import "UIViewControllerUtilities.h"
#import "WebAVPlayerController.h"
#import "WebAVPlayerLayer.h"
#import "WebAVPlayerLayerView.h"
#import <AVFoundation/AVTime.h>
#import <UIKit/UIKit.h>
#import <UIKit/UIWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>

#if HAVE(PIP_CONTROLLER)
#import <AVKit/AVPictureInPictureController.h>
#endif

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
static const NSTimeInterval playbackControlsVisibleDurationAfterResettingVideoSource = 1.0;
#endif

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlayerViewController)

#if HAVE(PIP_CONTROLLER)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureControllerContentSource)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPictureInPictureContentViewController)
#endif

@interface AVPlayerViewController (Details)
@property (nonatomic) BOOL showsPlaybackControls;
@property (nonatomic) UIView* view;
@end

@class WebAVMediaSelectionOption;

@interface WebAVPlayerViewControllerDelegate : NSObject <
    AVPlayerViewControllerDelegate
#if HAVE(PIP_CONTROLLER)
    , AVPictureInPictureControllerDelegate
#endif
> {
    ThreadSafeWeakPtr<WebCore::VideoPresentationInterfaceAVKit> _fullscreenInterface;
}
@property (nonatomic, assign /* weak */) RefPtr<WebCore::VideoPresentationInterfaceAVKit> fullscreenInterface;
#if !PLATFORM(APPLETV)
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
#else
- (BOOL)playerViewControllerShouldDismiss:(AVPlayerViewController *)playerViewController;
#endif
@end

@implementation WebAVPlayerViewControllerDelegate
- (RefPtr<WebCore::VideoPresentationInterfaceAVKit>)fullscreenInterface
{
    ASSERT(isMainThread());
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(RefPtr<WebCore::VideoPresentationInterfaceAVKit>)fullscreenInterface
{
    ASSERT(isMainThread());
    if (!fullscreenInterface) {
        _fullscreenInterface = nullptr;
        return;
    }
    _fullscreenInterface = ThreadSafeWeakPtr { *fullscreenInterface };
}

#if PLATFORM(WATCHOS)
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
#endif

- (void)playerViewControllerWillStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->willStartPictureInPicture();
}

- (void)playerViewControllerDidStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->didStartPictureInPicture();
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController failedToStartPictureInPictureWithError:(NSError *)error
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(error);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->failedToStartPictureInPicture();
}

- (void)playerViewControllerWillStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->willStopPictureInPicture();
}

- (void)playerViewControllerDidStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->didStopPictureInPicture();
}

- (BOOL)playerViewControllerShouldAutomaticallyDismissAtPictureInPictureStart:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    return NO;
}

#if !PLATFORM(APPLETV)

static WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason convertToExitFullScreenReason(AVPlayerViewControllerExitFullScreenReason reason)
{
    switch (reason) {
    case AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped:
        return WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::DoneButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped:
        return WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::FullScreenButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted:
        return WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::PictureInPictureStarted;
    case AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled:
        return WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::PinchGestureHandled;
    case AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived:
        return WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::RemoteControlStopEventReceived;
    }
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        return fullscreenInterface->shouldExitFullscreenWithReason(convertToExitFullScreenReason(reason));

    return YES;
}

#else

- (BOOL)playerViewControllerShouldDismiss:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        return fullscreenInterface->shouldExitFullscreenWithReason(WebCore::VideoPresentationInterfaceAVKit::ExitFullScreenReason::PinchGestureHandled);

    return YES;
}

#endif // !PLATFORM(APPLETV)

- (void)playerViewController:(AVPlayerViewController *)playerViewController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(playerViewController);
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}

#if PLATFORM(WATCHOS)
IGNORE_WARNINGS_END
#endif

- (BOOL)playerViewControllerShouldStartPictureInPictureFromInlineWhenEnteringBackground:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    return YES;
}

#if HAVE(PIP_CONTROLLER)

// AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->willStartPictureInPicture();
}

- (void)pictureInPictureControllerDidStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->didStartPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController failedToStartPictureInPictureWithError:(NSError *)error
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->failedToStartPictureInPicture();
}

- (void)pictureInPictureControllerWillStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->willStopPictureInPicture();
}

- (void)pictureInPictureControllerDidStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->didStopPictureInPicture();
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    if (auto fullscreenInterface = self.fullscreenInterface)
        fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
}

#endif // HAVE(PIP_CONTROLLER)

@end

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

#endif // HAVE(PIP_CONTROLLER)

NS_ASSUME_NONNULL_BEGIN
@interface WebAVPlayerViewController : NSObject<AVPlayerViewControllerDelegate>
@property (readonly, nonatomic) WebAVPlayerLayerView *playerLayerView;
- (instancetype)initWithFullscreenInterface:(WebCore::VideoPresentationInterfaceAVKit *)interface;
- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)startPictureInPicture;
- (void)stopPictureInPicture;
#if !PLATFORM(APPLETV)
- (BOOL)playerViewControllerShouldHandleDoneButtonTap:(AVPlayerViewController *)playerViewController;
#endif
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
#if !RELEASE_LOG_DISABLED
@property (readonly, nonatomic) uint64_t logIdentifier;
@property (readonly, nonatomic) const Logger* loggerPtr;
@property (readonly, nonatomic) WTFLogChannel* logChannel;
#endif
@end
NS_ASSUME_NONNULL_END

@implementation WebAVPlayerViewController {
    ThreadSafeWeakPtr<WebCore::VideoPresentationInterfaceAVKit> _fullscreenInterface;
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    RetainPtr<UIViewController> _presentingViewController;
#endif
    RetainPtr<AVPlayerViewController> _avPlayerViewController;
    RetainPtr<WebAVPlayerLayerView> _playerLayerView;
    RetainPtr<NSTimer> _startPictureInPictureTimer;
    WeakObjCPtr<WebAVPlayerViewControllerDelegate> _delegate;

#if HAVE(PIP_CONTROLLER)
    RetainPtr<AVPictureInPictureController> _pipController;
    RetainPtr<WebAVPictureInPictureContentViewController> _pipContentViewController;
#endif
}

- (instancetype)initWithFullscreenInterface:(WebCore::VideoPresentationInterfaceAVKit *)interface
{
    if (!(self = [super init]))
        return nil;

    _fullscreenInterface = ThreadSafeWeakPtr { *interface };

    _playerLayerView = adoptNS([WebCore::allocWebAVPlayerLayerViewInstance() init]);
    RetainPtr playerLayer = (WebAVPlayerLayer *)[_playerLayerView playerLayer];
    if (interface)
        [playerLayer setPresentationModel:interface->videoPresentationModel().get()];

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

#if PLATFORM(APPLETV)
    _avPlayerViewController = adoptNS([allocAVPlayerViewControllerInstance() init]);
    [self configurePlayerViewControllerWithFullscreenInterface:interface];
#else
    _avPlayerViewController = adoptNS([allocAVPlayerViewControllerInstance() initWithPlayerLayerView:_playerLayerView.get()]);
#endif
    [_avPlayerViewController setModalPresentationStyle:UIModalPresentationOverFullScreen];
#if PLATFORM(WATCHOS)
    [_avPlayerViewController setDelegate:self];
#endif

#if PLATFORM(VISION)
    [_avPlayerViewController setPrefersRoomDimming:NO];
    [_avPlayerViewController setFullScreenBehaviors:AVPlayerViewControllerFullScreenBehaviorHostContentInline];
#endif

#if HAVE(PIP_CONTROLLER)
    auto *playerController = static_cast<AVPlayerController *>(interface->playerController());
    _pipContentViewController = adoptNS([allocWebAVPictureInPictureContentViewControllerInstance() initWithController:playerController]);

    auto source = adoptNS([allocAVPictureInPictureControllerContentSourceInstance() initWithSourceView:static_cast<UIView *>(interface->playerLayerView()) contentViewController:_pipContentViewController.get() playerController:playerController]);

    _pipController = adoptNS([allocAVPictureInPictureControllerInstance() initWithContentSource:source.get()]);
#endif

    return self;
}

#if PLATFORM(APPLETV)
- (void)configurePlayerViewControllerWithFullscreenInterface:(WebCore::VideoPresentationInterfaceAVKit *)interface
{
    // FIXME (116592344): This is a proof-of-concept hack to work around lack support for a custom
    // AVPlayerLayerView in tvOS's version of AVPlayerViewController. This will be replaced once
    // proper API is available.

    RELEASE_ASSERT([_avPlayerViewController view]);

    [[_avPlayerViewController playerLayerView] removeFromSuperview];

    WebAVPlayerLayerView *playerLayerView = interface->playerLayerView();
    [_avPlayerViewController setPlayerLayerView:playerLayerView];

    playerLayerView.pixelBufferAttributes = [_avPlayerViewController pixelBufferAttributes];
    playerLayerView.playerController = (AVPlayerController *)interface->playerController();
    playerLayerView.translatesAutoresizingMaskIntoConstraints = NO;
    playerLayerView.playerLayer.videoGravity = [_avPlayerViewController videoGravity];

    UIView *contentContainerView = [_avPlayerViewController view].subviews.firstObject;
    [contentContainerView addSubview:playerLayerView];
    [NSLayoutConstraint activateConstraints:@[
        [playerLayerView.widthAnchor constraintEqualToAnchor:contentContainerView.widthAnchor],
        [playerLayerView.heightAnchor constraintEqualToAnchor:contentContainerView.heightAnchor],
        [playerLayerView.centerXAnchor constraintEqualToAnchor:contentContainerView.centerXAnchor],
        [playerLayerView.centerYAnchor constraintEqualToAnchor:contentContainerView.centerYAnchor],
    ]];
}
#endif // PLATFORM(APPLETV)

- (void)dealloc
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

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

- (WebAVPlayerLayerView *)playerLayerView
{
    return _playerLayerView.get();
}

#if !PLATFORM(APPLETV)
- (BOOL)playerViewControllerShouldHandleDoneButtonTap:(AVPlayerViewController *)playerViewController
{
    ASSERT(playerViewController == _avPlayerViewController.get());
    if (!_delegate)
        return YES;

    return [_delegate playerViewController:playerViewController shouldExitFullScreenWithReason:AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped];
}
#endif

- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID
{
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([_avPlayerViewController respondsToSelector:@selector(setWebKitOverrideRouteSharingPolicy:routingContextUID:)])
        [_avPlayerViewController setWebKitOverrideRouteSharingPolicy:routeSharingPolicy routingContextUID:routingContextUID];
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, !!animated);
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    auto fullscreenInterface = _fullscreenInterface.get();
    if (!fullscreenInterface) {
        if (completionHandler)
            completionHandler(NO, nil);
        return;
    }

    _presentingViewController = fullscreenInterface->presentingViewController();

    _avPlayerViewController.get().view.frame = _presentingViewController.get().view.frame;
    [_presentingViewController presentViewController:fullscreenInterface->fullscreenViewController() animated:animated completion:^{
        if (completionHandler)
            completionHandler(YES, nil);
    }];
#else
    [_avPlayerViewController enterFullScreenAnimated:animated completionHandler:completionHandler];
#endif
}

- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, !!animated);
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
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

#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
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
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
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
        if (auto fullscreenInterface = _fullscreenInterface.get())
            fullscreenInterface->failedToStartPictureInPicture();
    }];

    [self initObserver];
#endif
}

- (void)startPictureInPicture MY_NO_RETURN
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    ASSERT_NOT_REACHED();
#elif HAVE(PIP_CONTROLLER)
    [_pipController startPictureInPicture];
#else
    [_avPlayerViewController startPictureInPicture];
#endif
}

- (void)stopPictureInPicture MY_NO_RETURN
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    ASSERT_NOT_REACHED();
#elif HAVE(PIP_CONTROLLER)
    [_pipController stopPictureInPicture];
#else
    [_avPlayerViewController stopPictureInPicture];
#endif
}

- (BOOL)isPictureInPicturePossible
{
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPicturePossible];
#else
    return [_avPlayerViewController isPictureInPicturePossible];
#endif
}

- (BOOL)isPictureInPictureActive
{
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPictureActive];
#else
    return [_avPlayerViewController isPictureInPictureActive];
#endif
}

- (BOOL)pictureInPictureActive
{
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    return NO;
#elif HAVE(PIP_CONTROLLER)
    return [_pipController isPictureInPictureActive];
#else
    return [_avPlayerViewController isPictureInPictureActive];
#endif
}

- (BOOL)pictureInPictureWasStartedWhenEnteringBackground
{
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
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

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
- (void)flashPlaybackControlsWithDuration:(NSTimeInterval)duration
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
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
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, !!showsPlaybackControls);
#if PLATFORM(WATCHOS)
    UNUSED_PARAM(showsPlaybackControls);
#else
    [_avPlayerViewController setShowsPlaybackControls:showsPlaybackControls];
#endif
}

- (void)setAllowsPictureInPicturePlayback:(BOOL)allowsPictureInPicturePlayback
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, !!allowsPictureInPicturePlayback);
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

#if !RELEASE_LOG_DISABLED
- (uint64_t)logIdentifier
{
    if (auto fullscreenInterface = _fullscreenInterface.get())
        return fullscreenInterface->logIdentifier();
    return 0;
}

- (const Logger*)loggerPtr
{
    if (auto fullscreenInterface = _fullscreenInterface.get())
        return fullscreenInterface->loggerPtr();
    return nullptr;
}

- (WTFLogChannel*)logChannel
{
    return &WebCore::LogFullscreen;
}
#endif
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceAVKit);

Ref<VideoPresentationInterfaceAVKit> VideoPresentationInterfaceAVKit::create(PlaybackSessionInterfaceIOS& playbackSessionInterface)
{
    return adoptRef(*new VideoPresentationInterfaceAVKit(playbackSessionInterface));
}

VideoPresentationInterfaceAVKit::VideoPresentationInterfaceAVKit(PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : VideoPresentationInterfaceIOS(playbackSessionInterface)
    , m_playerViewControllerDelegate(adoptNS([[WebAVPlayerViewControllerDelegate alloc] init]))
{
    [m_playerViewControllerDelegate setFullscreenInterface:this];
}

VideoPresentationInterfaceAVKit::~VideoPresentationInterfaceAVKit()
{
    WebAVPlayerController* playerController = this->playerController();
    if (playerController && playerController.externalPlaybackActive)
        externalPlaybackChanged(false, PlaybackSessionModel::ExternalPlaybackTargetType::TargetTypeNone, emptyString());
}

UIViewController *VideoPresentationInterfaceAVKit::playerViewController() const
{
    return avPlayerViewController();
}

AVPlayerViewController *VideoPresentationInterfaceAVKit::avPlayerViewController() const
{
    return [m_playerViewController avPlayerViewController];
}

void VideoPresentationInterfaceAVKit::setupFullscreen(const FloatRect& initialRect, const FloatSize& videoDimensions, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    [playerController() setContentDimensions:videoDimensions];
    VideoPresentationInterfaceIOS::setupFullscreen(initialRect, videoDimensions, parentView, mode, allowsPictureInPicturePlayback, standby, blocksReturnToFullscreenFromPictureInPicture);
    if (playerLayer().captionsLayer != captionsLayer())
        playerLayer().captionsLayer = captionsLayer();
}

void VideoPresentationInterfaceAVKit::updateRouteSharingPolicy()
{
    if (m_playerViewController && !m_routingContextUID.isEmpty())
        [m_playerViewController setWebKitOverrideRouteSharingPolicy:(NSUInteger)m_routeSharingPolicy routingContextUID:m_routingContextUID];
}

void VideoPresentationInterfaceAVKit::hasVideoChanged(bool hasVideo)
{
    [playerController() setHasEnabledVideo:hasVideo];
    [playerController() setHasVideo:hasVideo];
}

bool VideoPresentationInterfaceAVKit::pictureInPictureWasStartedWhenEnteringBackground() const
{
    return [m_playerViewController pictureInPictureWasStartedWhenEnteringBackground];
}

void VideoPresentationInterfaceAVKit::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (!identifier)
        [m_playerViewController flashPlaybackControlsWithDuration:playbackControlsVisibleDurationAfterResettingVideoSource];
#endif
    VideoPresentationInterfaceIOS::setPlayerIdentifier(identifier);
}

bool VideoPresentationInterfaceAVKit::mayAutomaticallyShowVideoPictureInPicture() const
{
    return [playerController() isPlaying] && (m_standby || m_currentMode.isFullscreen()) && supportsPictureInPicture();
}

void VideoPresentationInterfaceAVKit::setupPlayerViewController()
{
    if (!m_playerViewController)
        m_playerViewController = adoptNS([[WebAVPlayerViewController alloc] initWithFullscreenInterface:this]);

    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)playerController()];
    [m_playerViewController setDelegate:m_playerViewControllerDelegate.get()];
    [m_playerViewController setAllowsPictureInPicturePlayback:m_allowsPictureInPicturePlayback];
    [playerController() setAllowsPictureInPicture:m_allowsPictureInPicturePlayback];
    if (!m_routingContextUID.isEmpty())
        [m_playerViewController setWebKitOverrideRouteSharingPolicy:(NSUInteger)m_routeSharingPolicy routingContextUID:m_routingContextUID];

    if (!m_currentMode.hasPictureInPicture() && !m_changingStandbyOnly) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Moving videoView to fullscreen WebAVPlayerLayerView");
        [playerLayerView() transferVideoViewTo:[m_playerViewController playerLayerView]];
    }

#if PLATFORM(WATCHOS)
    m_viewController = videoPresentationModel() ? videoPresentationModel()->createVideoFullscreenViewController(avPlayerViewController()) : nil;
#endif
}

void VideoPresentationInterfaceAVKit::invalidatePlayerViewController()
{
    returnVideoView();

    [m_playerViewController setDelegate:nil];
    [m_playerViewController setPlayerController:nil];
    m_playerViewController = nil;
}

void VideoPresentationInterfaceAVKit::presentFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    [m_playerViewController enterFullScreenAnimated:animated completionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}

void VideoPresentationInterfaceAVKit::dismissFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    [m_playerViewController exitFullScreenAnimated:animated completionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}

void VideoPresentationInterfaceAVKit::tryToStartPictureInPicture()
{
    [m_playerViewController tryToStartPictureInPicture];
}

void VideoPresentationInterfaceAVKit::stopPictureInPicture()
{
    [m_playerViewController stopPictureInPicture];
}

bool VideoPresentationInterfaceAVKit::isPlayingVideoInEnhancedFullscreen() const
{
    return hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) && [playerController() isPlaying];
}

void VideoPresentationInterfaceAVKit::setAllowsPictureInPicturePlayback(bool allowsPictureInPicturePlayback)
{
    [m_playerViewController setAllowsPictureInPicturePlayback:allowsPictureInPicturePlayback];
}

void VideoPresentationInterfaceAVKit::setShowsPlaybackControls(bool showsPlaybackControls)
{
    [m_playerViewController setShowsPlaybackControls:showsPlaybackControls];
}

void VideoPresentationInterfaceAVKit::setContentDimensions(const FloatSize& contentDimensions)
{
    [playerController() setContentDimensions:contentDimensions];
}

bool VideoPresentationInterfaceAVKit::isExternalPlaybackActive() const
{
    return [playerController() isExternalPlaybackActive];
}

bool VideoPresentationInterfaceAVKit::willRenderToLayer() const
{
    return true;
}

void VideoPresentationInterfaceAVKit::returnVideoView()
{
    [[m_playerViewController playerLayerView] transferVideoViewTo:playerLayerView()];
}

static std::optional<bool> isPictureInPictureSupported;

void setSupportsPictureInPicture(bool isSupported)
{
    isPictureInPictureSupported = isSupported;
}

bool supportsPictureInPicture()
{
#if ENABLE(VIDEO_PRESENTATION_MODE) && !PLATFORM(WATCHOS)
    if (isPictureInPictureSupported.has_value())
        return *isPictureInPictureSupported;
    return [getAVPictureInPictureControllerClass() isPictureInPictureSupported];
#else
    return false;
#endif
}

void VideoPresentationInterfaceAVKit::setupCaptionsLayer(CALayer *, const WebCore::FloatSize&)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [captionsLayer() removeFromSuperlayer];
    playerLayer().captionsLayer = captionsLayer();
    [playerLayer() layoutSublayers];
    [CATransaction commit];
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && HAVE(AVKIT)
