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

#import "config.h"

#if ENABLE(WEBXR) && USE(COMPOSITORXR)
#import "WKSpatialGestureRecognizer.h"

#import <Spatial/SPPose3D.h>
#import <UIKit/UIGestureRecognizerSubclass.h>
#import <WebCore/TransformationMatrix.h>
#import <pal/spi/cocoa/CompositorServicesSPI.h>

#import <pal/cocoa/ARKitSoftLink.h>

@interface UITouch (Spatial)

- (simd_float4x4)selectionRayTransformInLayer:(cp_layer_renderer_t)layer;
- (simd_float4x4)poseTransformInLayer:(cp_layer_renderer_t)layer;

@end

@implementation UITouch (Spatial)

- (simd_float4x4)selectionRayTransformInLayer:(cp_layer_renderer_t)layer
{
    SPRay3D selectionRay = [self selectionRayInLayer:layer];
    return WebCore::TransformationMatrix::makeLookAt(simd_float(selectionRay.origin.vector), simd_float(selectionRay.direction.vector));
}

- (simd_float4x4)poseTransformInLayer:(cp_layer_renderer_t)layer
{
    SPPose3D pose = [self poseInLayer:layer];
    return ARMatrix4x4DoubleToFloat(SPPose3DGet4x4Matrix(pose));
}

@end

@implementation WKSpatialGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    id<WKSpatialGestureRecognizerDelegate> spatialGestureRecognizerDelegate = _spatialGestureRecognizerDelegate;
    if (!spatialGestureRecognizerDelegate)
        return;

    cp_layer_renderer_t cpLayer = [spatialGestureRecognizerDelegate cpLayerForGestureRecognizer:self];
    if (!cpLayer)
        return;

    for (UITouch *touch in touches)
        [spatialGestureRecognizerDelegate gestureRecognizer:self transientActionDidStart:@(touch.hash) targetRayTransform:[touch selectionRayTransformInLayer:cpLayer] poseTransform:[touch poseTransformInLayer:cpLayer]];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    id<WKSpatialGestureRecognizerDelegate> spatialGestureRecognizerDelegate = _spatialGestureRecognizerDelegate;
    if (!spatialGestureRecognizerDelegate)
        return;

    cp_layer_renderer_t cpLayer = [spatialGestureRecognizerDelegate cpLayerForGestureRecognizer:self];
    if (!cpLayer)
        return;

    for (UITouch *touch in touches)
        [spatialGestureRecognizerDelegate gestureRecognizer:self transientActionDidUpdate:@(touch.hash) poseTransform:[touch poseTransformInLayer:cpLayer]];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    id<WKSpatialGestureRecognizerDelegate> spatialGestureRecognizerDelegate = _spatialGestureRecognizerDelegate;
    if (!spatialGestureRecognizerDelegate)
        return;

    cp_layer_renderer_t cpLayer = [spatialGestureRecognizerDelegate cpLayerForGestureRecognizer:self];
    if (!cpLayer)
        return;

    for (UITouch *touch in touches)
        [spatialGestureRecognizerDelegate gestureRecognizer:self transientActionDidEnd:@(touch.hash)];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    id<WKSpatialGestureRecognizerDelegate> spatialGestureRecognizerDelegate = _spatialGestureRecognizerDelegate;
    if (!spatialGestureRecognizerDelegate)
        return;

    cp_layer_renderer_t cpLayer = [spatialGestureRecognizerDelegate cpLayerForGestureRecognizer:self];
    if (!cpLayer)
        return;

    for (UITouch *touch in touches)
        [spatialGestureRecognizerDelegate gestureRecognizer:self transientActionDidCancel:@(touch.hash)];
}

@end

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
