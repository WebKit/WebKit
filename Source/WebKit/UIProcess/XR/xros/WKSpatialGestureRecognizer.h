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

#if ENABLE(WEBXR) && USE(COMPOSITORXR)

#import <CompositorServices/CompositorServices.h>
#import <UIKit/UIKit.h>
#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

@protocol WKSpatialGestureRecognizerDelegate;

@interface WKSpatialGestureRecognizer : UIGestureRecognizer

@property (weak, nonatomic) id <WKSpatialGestureRecognizerDelegate> spatialGestureRecognizerDelegate;

@end

@protocol WKSpatialGestureRecognizerDelegate <NSObject>

- (cp_layer_renderer_t)cpLayerForGestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer;

- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidStart:(NSNumber *)actionIdentifier targetRayTransform:(simd_float4x4)targetRayTransform poseTransform:(simd_float4x4)poseTransform;
- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidUpdate:(NSNumber *)actionIdentifier poseTransform:(simd_float4x4)poseTransform;
- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidEnd:(NSNumber *)actionIdentifier;
- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidCancel:(NSNumber *)actionIdentifier;

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
