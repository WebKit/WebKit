/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKPortalVolumetricSceneController.h"

#if PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)

#import "Logging.h"
#import "MRUIKitSPI.h"
#import "UIKitSPI.h"
#import "WKPortalVolumetricGestureController.h"
#import <algorithm>
#import <cmath>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>

// A scene that never activates would otherwise leave the presentation completion pending forever.
static constexpr Seconds sceneActivationTimeout = 10_s;

static constexpr CGFloat defaultPointsPerMeter = 1360;

// UIKit instantiates the scene delegate itself, so a controller that already exists cannot be one, and the
// activation request hands back no reference to the scene it creates. Each request therefore carries a token in
// its user activity, and the connecting scene finds its own controller by that token.
static NSString * const volumetricSceneActivityType = @"com.apple.WebKit.ConnectedVolumetricScene";
static NSString * const volumetricSceneTokenKey = @"WKVolumetricSceneToken";

// Held strongly: a request in flight keeps its own controller alive until the scene arrives, the presentation is
// dismissed, or the activation timeout fires, so there is no window where a token maps to a dead controller.
static HashMap<uint64_t, RetainPtr<WKPortalVolumetricSceneController>>& pendingSceneControllers()
{
    static NeverDestroyed<HashMap<uint64_t, RetainPtr<WKPortalVolumetricSceneController>>> controllers;
    return controllers;
}

static uint64_t nextVolumetricSceneToken()
{
    static uint64_t lastToken;
    return ++lastToken;
}

static std::optional<uint64_t> volumetricSceneTokenFromActivities(NSSet<NSUserActivity *> *activities)
{
    for (NSUserActivity *activity in activities) {
        if (![activity.activityType isEqualToString:volumetricSceneActivityType])
            continue;
        if (RetainPtr token = dynamic_objc_cast<NSNumber>([activity userInfo][volumetricSceneTokenKey]))
            return [token unsignedLongLongValue];
    }
    return std::nullopt;
}

@interface WKPortalVolumetricSceneController (Internal)
- (void)_configureWithScene:(UIWindowScene *)scene;
- (BOOL)_handleCloseRequest;
@end

@interface WKPortalVolumetricSceneDelegate : NSObject <MRUIWindowSceneDelegate>
@end

@implementation WKPortalVolumetricSceneDelegate {
    WeakObjCPtr<WKPortalVolumetricSceneController> _sceneController;
}

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions
{
    RetainPtr<WKPortalVolumetricSceneController> controller;
    if (auto token = volumetricSceneTokenFromActivities([connectionOptions userActivities]))
        controller = pendingSceneControllers().take(*token);

    if (!controller) {
        // Nothing owns this scene, so destroy it rather than leave a window nobody can close.
        RELEASE_LOG_ERROR(ModelElement, "WKPortalVolumetricSceneDelegate: volumetric scene connected with no presentation waiting for it");
        [UIApplication.sharedApplication requestSceneSessionDestruction:session options:nil errorHandler:nil];
        return;
    }

    _sceneController = controller.get();
    [controller _configureWithScene:dynamic_objc_cast<UIWindowScene>(scene)];
}

// Forwarded because UIKit owns this object, not the controller.
- (BOOL)windowScene:(UIWindowScene *)windowScene shouldCloseForReason:(MRUICloseWindowSceneReason)reason
{
    return [_sceneController.get() _handleCloseRequest];
}

@end

@interface WKPortalVolumetricViewController : UIViewController
@property (nonatomic, weak) WKPortalVolumetricSceneController *sceneController;
@end

@implementation WKPortalVolumetricViewController

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

    RetainPtr<WKPortalVolumetricSceneController> sceneController = self.sceneController;
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>) {
        [sceneController updateLayoutForVolumeSize];
    } completion:nil];
}

// No resize transition is reported as the volume settles, so this is the only signal for the initial fit.
- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    [self.sceneController updateLayoutForVolumeSize];
}

@end

@implementation WKPortalVolumetricSceneController {
    RetainPtr<UIWindowScene> _volumetricScene;
    RetainPtr<UIWindow> _volumetricWindow;
    RetainPtr<_UIRemoteView> _hostedContentView;
    RetainPtr<WKPortalVolumetricGestureController> _inputGestureController;
    RetainPtr<UIViewController> _inputHostingController;
    BlockPtr<void(BOOL)> _pendingCompletion;
    BlockPtr<void()> _closeHandler;
    BlockPtr<void(WebCore::FloatSize)> _volumeSizeChangedHandler;
    WebCore::FloatSize _volumeSizeInMeters;
    uint64_t _sceneToken;
}

- (instancetype)initWithCloseHandler:(void (^)(void))closeHandler
{
    if (!(self = [super init]))
        return nil;

    _closeHandler = makeBlockPtr(closeHandler);

    return self;
}

- (void)dealloc
{
    [self _failPresentation];
    [super dealloc];
}

- (void)setVolumeSizeChangedHandler:(void (^)(WebCore::FloatSize))handler
{
    _volumeSizeChangedHandler = makeBlockPtr(handler);
}

- (UIView *)_containerView
{
    return [_volumetricWindow rootViewController].view;
}

- (CGFloat)_pointsPerMeter
{
    UIView *container = [self _containerView];
    if (!container)
        return defaultPointsPerMeter;

    for (CALayer *layer = container.layer; layer; layer = layer.superlayer) {
        CGFloat pointsPerMeter = [[layer valueForKeyPath:@"separatedOptions.pointsPerMeter"] floatValue];
        if (pointsPerMeter > 0)
            return pointsPerMeter;
    }

    return defaultPointsPerMeter;
}

- (WebCore::FloatSize)_currentVolumeSizeInMeters
{
    UIView *container = [self _containerView];
    if (!container)
        return { };

    CGFloat pointsPerMeter = [self _pointsPerMeter];
    CGSize boundsSize = container.bounds.size;
    return WebCore::FloatSize(boundsSize.width / pointsPerMeter, boundsSize.height / pointsPerMeter);
}

// The autoresizing mask cannot recover this: the view is created at zero bounds, so its flexible margins are
// zero and stay zero.
- (void)_applyContentPlacement
{
    UIView *container = [self _containerView];
    if (!container || !_hostedContentView)
        return;

    [_hostedContentView setCenter:CGPointMake(CGRectGetMidX(container.bounds), CGRectGetMidY(container.bounds))];

    // FIXME: Read the volume's depth from the scene instead of inferring it from the in-plane extent.
    CGSize boundsSize = container.bounds.size;
    [_hostedContentView layer].zPosition = std::min(boundsSize.width, boundsSize.height) / 2;
}

- (void)_applyInputSurfaceExtents
{
    if (!_inputGestureController)
        return;

    float width = _volumeSizeInMeters.width();
    float height = _volumeSizeInMeters.height();
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        return;

    [_inputGestureController updateProxyExtentsWithWidth:width height:height depth:std::min(width, height)];
}

- (void)updateLayoutForVolumeSize
{
    if (![self _containerView])
        return;

    auto volumeSizeInMeters = [self _currentVolumeSizeInMeters];
    if (volumeSizeInMeters.isEmpty())
        return;

    // -viewDidLayoutSubviews runs on every layout pass, so gate on a real change or this sends an IPC per pass.
    if (volumeSizeInMeters == _volumeSizeInMeters)
        return;

    _volumeSizeInMeters = volumeSizeInMeters;

    [self _applyContentPlacement];
    [self _applyInputSurfaceExtents];

    if (_volumeSizeChangedHandler)
        _volumeSizeChangedHandler(_volumeSizeInMeters);
}

- (WebCore::FloatSize)hostContentWithContext:(WebCore::LayerHostingContextIdentifier)contentContext pid:(int)pid
{
    UIView *container = [self _containerView];
    if (!container)
        return { };

    // The hosting claim lives as long as the view is in a hierarchy, not as long as the reference.
    [_hostedContentView removeFromSuperview];

    RetainPtr remoteView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:pid contextID:contentContext.toUInt64()]);
    [remoteView setAutoresizingMask:(UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
    [container addSubview:remoteView.get()];
    _hostedContentView = remoteView;

    [self _applyContentPlacement];

    // A rebind re-adds the content above the input surface.
    if (_inputHostingController)
        [container bringSubviewToFront:[_inputHostingController view]];

    _volumeSizeInMeters = [self _currentVolumeSizeInMeters];
    [self _applyInputSurfaceExtents];
    return _volumeSizeInMeters;
}

- (void)installInputSurfaceWithBegan:(void (^)(CGPoint))began changed:(void (^)(CGPoint))changed ended:(void (^)(void))ended
{
    UIView *container = [self _containerView];
    if (!container || _inputGestureController)
        return;

    _inputGestureController = adoptNS([[WKPortalVolumetricGestureController alloc] init]);
    [_inputGestureController setOnDragBegan:began];
    [_inputGestureController setOnDragChanged:changed];
    [_inputGestureController setOnDragEnded:ended];

    _inputHostingController = [_inputGestureController makeHostingController];

    RetainPtr rootViewController = [_volumetricWindow rootViewController];
    [rootViewController addChildViewController:_inputHostingController.get()];
    [[_inputHostingController view] setFrame:container.bounds];
    [[_inputHostingController view] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [container addSubview:[_inputHostingController view]];
    [_inputHostingController didMoveToParentViewController:rootViewController.get()];

    [self _applyInputSurfaceExtents];
}

- (void)_removeInputSurface
{
    if (_inputHostingController) {
        [_inputHostingController willMoveToParentViewController:nil];
        [[_inputHostingController view] removeFromSuperview];
        [_inputHostingController removeFromParentViewController];
        _inputHostingController = nil;
    }

    _inputGestureController = nil;
}

// Supplying a configuration binds our delegate class to the new scene, which is both how the scene reaches
// -scene:willConnectToSession:options: and how we get to answer -windowScene:shouldCloseForReason:.
- (void)presentWithCompletion:(void (^)(BOOL success))completion
{
    _pendingCompletion = makeBlockPtr(completion);
    _sceneToken = nextVolumetricSceneToken();
    pendingSceneControllers().set(_sceneToken, self);

    RetainPtr configuration = [UISceneConfiguration _internalConfigurationWithRole:UIWindowSceneSessionRoleVolumetricApplication sceneClass:nil delegateClass:WKPortalVolumetricSceneDelegate.class storyboard:nil];
    RetainPtr request = [UISceneSessionActivationRequest _requestWithConfiguration:configuration.get()];

    // Carries this request's identity through to -scene:willConnectToSession:options:.
    RetainPtr activity = adoptNS([[NSUserActivity alloc] initWithActivityType:volumetricSceneActivityType]);
    [activity setUserInfo:@{ volumetricSceneTokenKey: @(_sceneToken) }];
    [request setUserActivity:activity.get()];

    RetainPtr options = adoptNS([[_UIVolumetricWindowSceneActivationRequestOptions alloc] init]);
    [options _setInternal:YES];
    [request setOptions:options.get()];

    __weak WKPortalVolumetricSceneController *weakSelf = self;
    [UIApplication.sharedApplication activateSceneSessionForRequest:request.get() errorHandler:^(NSError *error) {
        RELEASE_LOG_ERROR(ModelElement, "WKPortalVolumetricSceneController: failed to activate volumetric scene: %@", error);
        [weakSelf _failPresentation];
    }];

    RunLoop::mainSingleton().dispatchAfter(sceneActivationTimeout, [weakSelf = WeakObjCPtr<WKPortalVolumetricSceneController> { self }] {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf || !strongSelf->_pendingCompletion)
            return;
        RELEASE_LOG_ERROR(ModelElement, "WKPortalVolumetricSceneController: volumetric scene did not activate in time");
        [strongSelf _failPresentation];
    });
}

- (void)_failPresentation
{
    [self _stopWaitingForScene];

    if (!_pendingCompletion)
        return;

    auto completion = std::exchange(_pendingCompletion, nullptr);
    completion(NO);
}

- (void)_configureWithScene:(UIWindowScene *)scene
{
    if (!scene)
        return;

    [self _stopWaitingForScene];
    _volumetricScene = scene;

    _volumetricWindow = adoptNS([[UIWindow alloc] initWithWindowScene:scene]);
    [_volumetricWindow setBackgroundColor:[UIColor clearColor]];

    RetainPtr rootViewController = adoptNS([[WKPortalVolumetricViewController alloc] init]);
    [rootViewController setSceneController:self];

    RetainPtr rootView = adoptNS([[UIView alloc] initWithFrame:[_volumetricWindow bounds]]);
    [rootView setBackgroundColor:[UIColor clearColor]];
    [rootView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [rootViewController setView:rootView.get()];

    [_volumetricWindow setRootViewController:rootViewController.get()];
    [_volumetricWindow makeKeyAndVisible];

    if (!_pendingCompletion)
        return;

    auto completion = std::exchange(_pendingCompletion, nullptr);
    completion(YES);
}

- (void)_stopWaitingForScene
{
    if (!_sceneToken)
        return;

    // Cleared first: the map may hold the last reference, so the remove below can reach -dealloc and re-enter.
    auto token = std::exchange(_sceneToken, 0);

    // Exact matching means a scene that arrives after this point finds no controller and destroys itself,
    // rather than being handed to whichever request happened to be waiting next.
    pendingSceneControllers().remove(token);
}

- (void)dismissWithCompletion:(void (^)(void))completion
{
    [self _failPresentation];

    _volumeSizeChangedHandler = nil;

    [self _removeInputSurface];

    [_hostedContentView removeFromSuperview];
    _hostedContentView = nil;

    if (!_volumetricScene) {
        if (completion)
            completion();
        return;
    }

    RetainPtr session = [_volumetricScene session];
    _volumetricScene = nil;
    _volumetricWindow = nil;

    [UIApplication.sharedApplication requestSceneSessionDestruction:session.get() options:nil errorHandler:^(NSError *error) {
        RELEASE_LOG_ERROR(ModelElement, "WKPortalVolumetricSceneController: failed to destroy volumetric scene: %@", error);
    }];

    if (completion)
        completion();
}

- (BOOL)_handleCloseRequest
{
    // Refused so the teardown runs in WebKit's order: the page has to be told before the scene disappears.
    if (_closeHandler)
        _closeHandler();
    return NO;
}

@end

#endif // PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)
