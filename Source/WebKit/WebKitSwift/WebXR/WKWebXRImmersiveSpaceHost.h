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

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <wtf/Platform.h>

#if ENABLE(WEBXR) && PLATFORM(MAC)

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

// Presents the remote immersive space that a WebXR session on macOS draws into.
//
// This lives in WebKitSwift rather than WebKit because the SwiftUI immersive space API is in a
// framework that itself depends on WebKit, so linking it from WebKit.framework would create a cycle.
NS_SWIFT_UI_ACTOR
@interface WKWebXRImmersiveSpaceHost : NSObject

// Presents the immersive space and reports back the CompositorServices layer renderer and the ARKit
// device that back the session, or nils if the space cannot be presented.
+ (void)startSessionWithColorFormat:(MTLPixelFormat)colorFormat depthFormat:(MTLPixelFormat)depthFormat usesWebGPU:(BOOL)usesWebGPU completionHandler:(void (^)(id _Nullable layerRenderer, id _Nullable remoteDevice))completionHandler NS_SWIFT_NAME(startSession(colorFormat:depthFormat:usesWebGPU:completionHandler:)) NS_SWIFT_DISABLE_ASYNC;

// Tears the immersive space down. Safe to call when nothing is presented.
+ (void)endSession;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(WEBXR) && PLATFORM(MAC)
