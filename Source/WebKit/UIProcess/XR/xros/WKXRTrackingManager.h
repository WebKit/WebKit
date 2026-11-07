/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEBXR) && USE(COMPOSITORXR)

#import "WKSpatialGestureRecognizer.h"

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#import <ARKit/ARKit.h>
ALLOW_DEPRECATED_DECLARATIONS_END

#import <WebCore/PlatformXR.h>
#import <WebCore/PlatformXRPose.h>

#if HAVE(SPATIAL_CONTROLLERS)
#import "WKXRControllerManager.h"
#endif

NS_ASSUME_NONNULL_BEGIN

@interface WKXRTrackingManager : NSObject <WKSpatialGestureRecognizerDelegate>

@property (nonatomic, readonly, getter=isWorldTrackingSupported) BOOL worldTrackingSupported;
@property (nonatomic, readonly, getter=isValid) BOOL valid;
@property (nonatomic, readonly) std::optional<PlatformXRPose> latestFloorPose;

- (instancetype)initWithHandTrackingEnabled:(BOOL)handTrackingEnabled layerRenderer:(cp_layer_renderer_t)layerRenderer
#if HAVE(SPATIAL_CONTROLLERS)
    controllerManager:(RetainPtr<WKXRControllerManager>&)controllerManager
#endif
    ; // NOLINT
- (OSObjectPtr<ar_device_anchor_t>)deviceAnchorAtTime:(NSTimeInterval)time;
- (Vector<PlatformXR::FrameData::InputSource>)collectInputSources;

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
