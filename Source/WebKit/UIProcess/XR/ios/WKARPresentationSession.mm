/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#import "WKARPresentationSession.h"

#if ENABLE(WEBXR) && USE(ARKITXR_IOS)

#import "Logging.h"
#import "WKExtrinsicButton.h"

#import <Metal/Metal.h>
#import <WebCore/PlatformXR.h>
#import <WebCore/PlatformXRPose.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>

#import "ARKitSoftLink.h"

NS_ASSUME_NONNULL_BEGIN

#pragma mark -

@interface UITouch (NormalizedLocation)
- (CGPoint)normalizedLocationInView:(UIView *)view;
@end

@implementation UITouch (NormalizedLocation)

- (CGPoint)normalizedLocationInView:(UIView *)view
{
    auto viewSize = view.bounds.size;
    auto point = [self locationInView:view];
    return CGPoint { point.x / viewSize.width, point.y / viewSize.height };
}
@end

#pragma mark - WKARPresentationSessionDescriptor

@implementation WKARPresentationSessionDescriptor {
    WeakObjCPtr<UIViewController> _presentingViewController;
}
@synthesize colorFormat = _colorFormat;
@synthesize colorUsage = _colorUsage;
@synthesize rasterSampleCount = _rasterSampleCount;

- (nonnull instancetype)init
{
    self = [super init];
    if (self) {
        _colorFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        _colorUsage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        _rasterSampleCount = 1;
    }

    return self;
}

- (instancetype)copyWithZone:(nullable NSZone *)zone
{
    WKARPresentationSessionDescriptor *descriptor = [[[self class] allocWithZone:zone] init];
    descriptor.colorFormat = self.colorFormat;
    descriptor.colorUsage = self.colorUsage;
    descriptor.rasterSampleCount = self.rasterSampleCount;
    descriptor.presentingViewController = self.presentingViewController;

    return descriptor;
}

- (nullable UIViewController *)presentingViewController
{
    return (UIViewController *) _presentingViewController;
}

- (void)setPresentingViewController:(nullable UIViewController*)presentingViewController
{
    _presentingViewController = presentingViewController;
}

@end

#pragma mark - _WKARPresentationSession

@interface _WKARPresentationSession : UIViewController <WKARPresentationSession, UIGestureRecognizerDelegate>
@property (atomic, readwrite, getter=isSessionEndRequested) BOOL sessionEndRequested;
- (nullable instancetype)initWithSession:(ARSession *)session descriptor:(WKARPresentationSessionDescriptor *)descriptor;
- (simd_float4x4)raycastQueryTransformFromPoint:(CGPoint) point;
@end

#pragma mark - _WKTransientGestureRecognizer

@interface _WKTransientGestureRecognizer : UIGestureRecognizer
- (nullable instancetype)initWithSession:(_WKARPresentationSession *)session;
- (Vector<PlatformXR::FrameData::InputSource>)collectInputSources;
@end

#pragma mark - _WKARPresentationSession implementation

@implementation _WKARPresentationSession {
    RetainPtr<ARSession> _session;
    RetainPtr<WKARPresentationSessionDescriptor> _sessionDescriptor;

    // View state
    WeakObjCPtr<CALayer> _cameraLayer;
    WeakObjCPtr<CAMetalLayer> _metalLayer;
    RetainPtr<WKExtrinsicButton> _cancelButton;
    RetainPtr<_WKTransientGestureRecognizer> _touchGestureRecognizer;

    // CAMetalLayer state
    RetainPtr<id<CAMetalDrawable>> _currentDrawable;

    // Metal state
    RetainPtr<id<MTLDevice>> _device;
    RetainPtr<id<MTLSharedEvent>> _completionEvent;
    NSUInteger _renderingFrameIndex;

    // Camera image
    RetainPtr<CVPixelBufferRef> _capturedImage;
}
@synthesize renderingFrameIndex = _renderingFrameIndex;
@synthesize sessionEndRequested = _sessionEndRequested;

- (nullable instancetype)initWithSession:(ARSession *)session descriptor:(WKARPresentationSessionDescriptor *)descriptor
{
    self = [super init];
    if (self) {
        _session = session;
        _sessionDescriptor = adoptNS([descriptor copy]);
        _device = adoptNS(MTLCreateSystemDefaultDevice());
        self.sessionEndRequested = NO;

        [self _loadMetal];
        [self _enterFullscreen];
    }

    return self;
}

- (simd_float4x4)raycastQueryTransformFromPoint:(CGPoint)point
{
    RetainPtr frame = [self currentFrame];
    ASSERT(frame, "Requires a valid frame");
    if (!frame)
        return matrix_identity_float4x4;

    // allowingTarget and alignment are dummy values. We only use origin and direction
    // from the raycast query object.
    auto allowingTarget = ARRaycastTargetEstimatedPlane;
    auto alignment = ARRaycastTargetAlignmentAny;
    RetainPtr query = [frame raycastQueryFromPoint:point allowingTarget:allowingTarget alignment:alignment];
    return ARMatrixMakeLookAt([query origin], [query direction]);
}

#pragma mark - WKARPresentationSession

- (ARFrame *)currentFrame {
    return [_session currentFrame];
}

- (ARSession *)session {
    return (ARSession *) _session;
}

- (nonnull id<MTLSharedEvent>)completionEvent {
    return (id<MTLSharedEvent>) _completionEvent;
}

- (nullable id<MTLTexture>)colorTexture {
    return [_currentDrawable texture];
}

- (NSUInteger)startFrame
{
    RetainPtr frame = [_session currentFrame];
    if (!frame) {
        RELEASE_LOG(XR, "%s: no frame available", __FUNCTION__);
        return 0;
    }

    _capturedImage = [frame capturedImage];
    _currentDrawable = [_metalLayer nextDrawable];
    _renderingFrameIndex += 1;

    return 1;
}

- (Vector<PlatformXR::FrameData::InputSource>)collectInputSources
{
    return [_touchGestureRecognizer collectInputSources];
}

- (void)present
{
    [CATransaction begin];
    {
        [_cameraLayer setContents:(id) _capturedImage.get()];
        if (_currentDrawable)
            [_currentDrawable present];
    }
    [CATransaction commit];
    [CATransaction flush];

    _capturedImage = nil;
    _currentDrawable = nil;
}

- (void)terminate
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    return touch.view == self.view;
}

#pragma mark - UIViewController

- (void)loadView
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    RetainPtr view = adoptNS([[UIView alloc] initWithFrame:UIScreen.mainScreen.bounds]);

    _touchGestureRecognizer = adoptNS([[_WKTransientGestureRecognizer alloc] initWithSession:self]);
    [_touchGestureRecognizer setDelegate:self];
    [view addGestureRecognizer:_touchGestureRecognizer.get()];
    self.view = view.get();
}

- (void)viewDidLoad
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    [super viewDidLoad];

    // FIXME(rdar://116570225): WebGL and Metal have inverted ideas of which
    // direction in Y is "up". The metal layer transform needs to be set
    // correctly for the WebGL to match the camera feed.
    _cameraLayer = [self.view layer];
    [_cameraLayer setBackgroundColor:UIColor.whiteColor.CGColor];
    [_cameraLayer setOpaque:YES];
    [_cameraLayer addSublayer:[CAMetalLayer layer]];
    _metalLayer = [_cameraLayer sublayers][0];
    [_metalLayer setDevice:_device.get()];
    [_metalLayer setOpaque:NO];
    [_metalLayer setPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB];
    [_metalLayer setPresentsWithTransaction:YES];
    [_metalLayer setFrame:[_cameraLayer bounds]];
    [_metalLayer setAffineTransform:CGAffineTransformMakeScale(1, -1)];

    // Controls
    NSBundle *bundle = [NSBundle bundleForClass:self.class];
    UIImage *doneImage = [UIImage imageNamed:@"Done" inBundle:bundle compatibleWithTraitCollection:nil];

    auto buttonSize = CGSizeMake(60.0, 47.0);
    _cancelButton = [WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
ALLOW_DEPRECATED_DECLARATIONS_END
    [_cancelButton setExtrinsicContentSize:buttonSize];
    [_cancelButton setTintColor:[UIColor whiteColor]];
    [_cancelButton setImage:[doneImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_cancelButton sizeToFit];
    [_cancelButton addTarget:self action:@selector(_cancelAction:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_cancelButton.get()];
}

#if !PLATFORM(VISION)
- (BOOL)preferStatusBarHidden
{
    return YES;
}
#endif

- (void)viewWillAppear:(BOOL)animated
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    [super viewWillAppear:animated];

    RetainPtr configuration = adoptNS([WebKit::allocARWorldTrackingConfigurationInstance() init]);
    [_session runWithConfiguration:configuration.get()];
}

- (void)viewWillDisappear:(BOOL)animated
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    [_session pause];
    [super viewWillDisappear:animated];
}

#pragma mark - Private

- (void)_cancelAction:(id)sender
{
    self.sessionEndRequested = YES;
    [self _exitFullscreen];
}

- (void)_enterFullscreen
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(XR, "%s", __FUNCTION__);

    UIViewController* presentingViewController = [_sessionDescriptor presentingViewController];
    self.modalPresentationStyle = UIModalPresentationFullScreen;
    [presentingViewController presentViewController:self animated:NO completion:^(void) {
        RELEASE_LOG(XR, "%s: presentViewController complete", __FUNCTION__);
    }];
}

- (void)_exitFullscreen
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(XR, "%s", __FUNCTION__);

    UIViewController* presentingViewController = [_sessionDescriptor presentingViewController];
    [presentingViewController dismissViewControllerAnimated:NO completion:^(void) {
        RELEASE_LOG(XR, "%s: dismissViewController complete", __FUNCTION__);
    }];
}

- (void)_loadMetal
{
    _completionEvent = adoptNS([_device newSharedEvent]);
    _renderingFrameIndex = 0;
}

@end

id<WKARPresentationSession> createPresentationSession(ARSession *session, WKARPresentationSessionDescriptor *descriptor)
{
    return [[_WKARPresentationSession alloc] initWithSession:session descriptor:descriptor];
}

#pragma mark - _WKTransientAction

@interface _WKTransientAction : NSObject
@property (nonatomic, readonly) simd_float4x4 targetRay;
@property (nonatomic, readonly) simd_float4x4 startingPoseInverse;
@property (nonatomic) simd_float4x4 pose;
- (nullable instancetype)initWithTargetRay:(simd_float4x4)targetRay pose:(simd_float4x4)pose;
@end

@implementation _WKTransientAction

- (nullable instancetype)initWithTargetRay:(simd_float4x4)targetRay pose:(simd_float4x4)pose
{
    self = [super init];
    if (self) {
        _targetRay = targetRay;
        _startingPoseInverse = simd_inverse(pose);
        _pose = pose;
    }
    return self;
}

@end

#pragma mark - _WKTransientGestureRecognizer implementation

@implementation _WKTransientGestureRecognizer {
    OSObjectPtr<dispatch_queue_t> _accessQueue;
    WeakObjCPtr<_WKARPresentationSession> _session;
    RetainPtr<NSMutableDictionary<NSNumber *, _WKTransientAction *>> _transientActions;
}

- (nullable instancetype)initWithSession:(_WKARPresentationSession *)session
{
    self = [super init];
    if (self) {
        _accessQueue = adoptOSObject(dispatch_queue_create("com.apple.WebContent._WKTransientGestureRecognizer.AccessQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL));
        _session = session;
        _transientActions = adoptNS([NSMutableDictionary new]);
    }
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (!_session)
        return;

    RetainPtr view = [_session view];
    RetainPtr frame = [_session currentFrame];
    if (!frame)
        return;

    // We differ from visionOS here and use the ARCamera as the pose reference
    // space.
    simd_float4x4 poseTransform = [frame camera].transform;

    Vector<RetainPtr<UITouch>> touchList;
    for (UITouch *touch in touches)
        touchList.append(touch);

    dispatch_async(_accessQueue.get(), ^{
        for (RetainPtr touch : touchList) {
            auto normalizedPoint = [touch normalizedLocationInView:view.get()];
            auto targetRayTransform = [_session raycastQueryTransformFromPoint:normalizedPoint];

            RetainPtr id = @([touch hash]);
            RetainPtr transientAction = adoptNS([[_WKTransientAction alloc] initWithTargetRay:targetRayTransform pose:poseTransform]);
            [_transientActions setObject:transientAction.get() forKey:id.get()];
            RELEASE_LOG_DEBUG(XR, "Transient action started: %@", id.get());
        }
    });
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (!_session)
        return;

    RetainPtr frame = [_session currentFrame];
    if (!frame)
        return;

    // We differ from visionOS here and use the ARCamera as the pose reference
    // space.
    simd_float4x4 poseTransform = [frame camera].transform;

    Vector<RetainPtr<UITouch>> touchList;
    for (UITouch *touch in touches)
        touchList.append(touch);

    dispatch_async(_accessQueue.get(), ^{
        for (RetainPtr touch : touchList) {
            RetainPtr id = @([touch hash]);
            RetainPtr transientAction = [_transientActions objectForKey:id.get()];
            ASSERT(transientAction);
            if (!transientAction)
                RELEASE_LOG_ERROR(XR, "ERROR: Transient action updated without begin: %@", id.get());
            [transientAction setPose:poseTransform];
        }
    });
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _doneWithTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _doneWithTouches:touches];
}

- (void)_doneWithTouches:(NSSet<UITouch *> *)touches
{
    if (!_session)
        return;

    Vector<RetainPtr<UITouch>> touchList;
    for (UITouch *touch in touches)
        touchList.append(touch);

    dispatch_async(_accessQueue.get(), ^{
        for (RetainPtr touch : touchList) {
            RetainPtr id = @([touch hash]);
            RetainPtr transientAction = [_transientActions objectForKey:id.get()];
            ASSERT(transientAction);
            if (!transientAction)
                RELEASE_LOG_ERROR(XR, "ERROR: Transient action ended without begin: %@", id.get());
            [_transientActions removeObjectForKey:id.get()];
            RELEASE_LOG_DEBUG(XR, "Transient action ended: %@", id.get());
        }
    });
}

#pragma mark - Input sources collection

- (std::optional<PlatformXR::FrameData::InputSource>)_platformXRInputSourceFromTransientAction:(_WKTransientAction *)transientAction actionIdentifier:(int)actionIdentifier
{
    dispatch_assert_queue(_accessQueue.get());

    simd_float4x4 multiplier = simd_mul(transientAction.pose, transientAction.startingPoseInverse);
    PlatformXRPose targetRay(simd_mul(multiplier, transientAction.targetRay));
    PlatformXRPose pose(transientAction.pose);

    PlatformXR::FrameData::InputSource data;
    data.handedness = PlatformXR::XRHandedness::None;
    data.handle = actionIdentifier;
    data.profiles = Vector<String> { "generic-button-invisible"_s, "generic-button"_s };
    data.targetRayMode = PlatformXR::XRTargetRayMode::TransientPointer;

    data.pointerOrigin.pose = targetRay.pose();
    data.pointerOrigin.isPositionEmulated = false;

    PlatformXR::FrameData::InputSourcePose gripPose;
    gripPose.pose = pose.pose();
    gripPose.isPositionEmulated = false;
    data.gripOrigin = gripPose;

    PlatformXR::FrameData::InputSourceButton button;
    button.pressed = true;
    button.touched = true;
    button.pressedValue = 1.0;
    data.buttons.append(button);

    return data;
}

- (Vector<PlatformXR::FrameData::InputSource>)collectInputSources
{
    __block Vector<PlatformXR::FrameData::InputSource> result;

    dispatch_sync(_accessQueue.get(), ^{
        [_transientActions enumerateKeysAndObjectsUsingBlock:^(NSNumber *id, _WKTransientAction *transientAction, BOOL*) {
            if (auto transientInputData = [self _platformXRInputSourceFromTransientAction:transientAction actionIdentifier:id.intValue])
                result.append(transientInputData.value());
        }];
    });

    return result;
}

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEBXR) && USE(ARKITXR_IOS)
