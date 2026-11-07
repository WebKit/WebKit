/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
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

#pragma once

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

DECLARE_SYSTEM_HEADER

#if HAVE(COMPOSITOR_SERVICES)

#import <CompositorServices/CompositorServices.h>

#if USE(APPLE_INTERNAL_SDK)

#import <CompositorServices/CompositorServices_Private.h>

#else // !USE(APPLE_INTERNAL_SDK)

#import <Spatial/SPRay3D.h>

CP_OBJECT_DECL(cp_swapchain);
CP_OBJECT_DECL(cp_swapchain_link);

@interface CP_OBJECT_NAME(cp_swapchain) ()
@property (readwrite, strong) NSArray<cp_swapchain_link_t> *swapchainLinks;
@end

@interface CP_OBJECT_NAME(cp_swapchain_link) ()
@property (nonatomic, readwrite, strong, nullable) NSArray<id<MTLTexture>> *depthTextures;
@end

@interface UITouch (Compositor)
- (SPPose3D)poseInLayer:(cp_layer_renderer_t)layer CF_SWIFT_NAME(pose(in:));
- (SPRay3D)selectionRayInLayer:(cp_layer_renderer_t)layer CF_SWIFT_NAME(selectionRay(in:));
@end

#endif // USE(APPLE_INTERNAL_SDK)
#endif // HAVE(COMPOSITOR_SERVICES)

#endif // !__has_feature(modules)
