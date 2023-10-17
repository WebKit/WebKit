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

- (instancetype)copyWithZone:(nullable NSZone *)zone
{
    WKARPresentationSessionDescriptor *descriptor = [[[self class] allocWithZone:zone] init];
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

    // Camera image
    RetainPtr<CVPixelBufferRef> _capturedImage;
}

- (nullable instancetype)initWithSession:(ARSession *)session descriptor:(WKARPresentationSessionDescriptor *)descriptor
{
    self = [super init];
    if (self) {
        _session = session;
        _sessionDescriptor = adoptNS([descriptor copy]);

        [self _enterFullscreen];
    }

    return self;
}

#pragma mark - WKARPresentationSession

-(ARSession *)session {
    return (ARSession *) _session;
}

- (NSUInteger)startFrame
{
    ARFrame *currentFrame = [_session currentFrame];
    if (!currentFrame) {
        RELEASE_LOG(XR, "%s: no frame available", __FUNCTION__);
        return 0;
    }

    _capturedImage = currentFrame.capturedImage;

    return 1;
}

- (void)present
{
    [CATransaction begin];
    {
        [_cameraLayer setContents:(id) _capturedImage.get()];
    }
    [CATransaction commit];
    [CATransaction flush];

    _capturedImage = nil;
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

    _cameraLayer = [self.view layer];
    [_cameraLayer setBackgroundColor:UIColor.whiteColor.CGColor];
    [_cameraLayer setOpaque:YES];
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

@end

id<WKARPresentationSession> createPresesentationSession(ARSession *session, WKARPresentationSessionDescriptor *descriptor)
{
    return [[_WKARPresentationSession alloc] initWithSession:session descriptor:descriptor];
}

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEBXR) && USE(ARKITXR_IOS)
