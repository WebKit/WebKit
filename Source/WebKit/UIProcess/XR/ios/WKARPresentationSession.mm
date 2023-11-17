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

#import <Metal/Metal.h>
#import <wtf/WeakObjCPtr.h>

NS_ASSUME_NONNULL_BEGIN

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

@interface _WKARPresentationSession : UIViewController <WKARPresentationSession>
- (nullable instancetype)initWithSession:(ARSession *)session descriptor:(WKARPresentationSessionDescriptor *)descriptor;
@end

#pragma mark - _WKARPresentationSession

@implementation _WKARPresentationSession {
    RetainPtr<ARSession> _session;
    RetainPtr<WKARPresentationSessionDescriptor> _sessionDescriptor;

    // View state
    RetainPtr<UIView> _view;
    WeakObjCPtr<CALayer> _cameraLayer;
    WeakObjCPtr<CAMetalLayer> _metalLayer;

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

- (nullable instancetype)initWithSession:(ARSession *)session descriptor:(WKARPresentationSessionDescriptor *)descriptor
{
    self = [super init];
    if (self) {
        _session = session;
        _sessionDescriptor = adoptNS([descriptor copy]);
        _device = adoptNS(MTLCreateSystemDefaultDevice());

        [self _loadMetal];
        [self _enterFullscreen];
    }

    return self;
}

#pragma mark - WKARPresentationSession

-(ARFrame *)currentFrame {
    return [_session currentFrame];
}

-(ARSession *)session {
    return (ARSession *) _session;
}

-(nonnull id<MTLSharedEvent>)completionEvent {
    return (id<MTLSharedEvent>) _completionEvent;
}

-(nullable id<MTLTexture>)colorTexture {
    return [_currentDrawable texture];
}

- (NSUInteger)startFrame
{
    ARFrame *currentFrame = [_session currentFrame];
    if (!currentFrame) {
        RELEASE_LOG(XR, "%s: no frame available", __FUNCTION__);
        return 0;
    }

    _capturedImage = currentFrame.capturedImage;
    _currentDrawable = [_metalLayer nextDrawable];
    _renderingFrameIndex += 1;

    return 1;
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

#pragma mark - UIViewController

- (void)loadView
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    RetainPtr<UIView> view = adoptNS([[UIView alloc] initWithFrame:UIScreen.mainScreen.bounds]);
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

    RetainPtr<ARWorldTrackingConfiguration> configuration = adoptNS([WebKit::allocARWorldTrackingConfigurationInstance() init]);
    [_session runWithConfiguration:configuration.get()];
}

- (void)viewWillDisappear:(BOOL)animated
{
    RELEASE_LOG(XR, "%s", __FUNCTION__);
    [_session pause];
    [super viewWillDisappear:animated];
}

#pragma mark - Private

- (void)_enterFullscreen
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(XR, "%s", __FUNCTION__);

    UIViewController* presentingViewController = [_sessionDescriptor presentingViewController];
    [presentingViewController presentViewController:self animated:NO completion:^(void) {
        RELEASE_LOG(XR, "%s: presentViewController complete", __FUNCTION__);
    }];
}

- (void)_loadMetal
{
    _completionEvent = adoptNS([_device newSharedEvent]);
    _renderingFrameIndex = 0;
}

@end

id<WKARPresentationSession> createPresesentationSession(ARSession *session, WKARPresentationSessionDescriptor *descriptor)
{
    return [[_WKARPresentationSession alloc] initWithSession:session descriptor:descriptor];
}

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEBXR) && USE(ARKITXR_IOS)
