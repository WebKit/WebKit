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

#pragma once

#import <wtf/Platform.h>

#if PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)

#import <UIKit/UIKit.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

// Captures spatial drags over a volumetric scene, which has no glass for a UIPanGestureRecognizer to sit on. An
// invisible RealityKit entity fills the volume instead; nothing arrives until it is given a non-degenerate box.
NS_SWIFT_UI_ACTOR
@interface WKPortalVolumetricGestureController : NSObject

@property (nonatomic, copy, nullable) void (^onDragBegan)(CGPoint);
@property (nonatomic, copy, nullable) void (^onDragChanged)(CGPoint);
@property (nonatomic, copy, nullable) void (^onDragEnded)(void);

- (UIViewController *)makeHostingController;

// Extents in meters, not points.
- (void)updateProxyExtentsWithWidth:(float)width height:(float)height depth:(float)depth;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)
