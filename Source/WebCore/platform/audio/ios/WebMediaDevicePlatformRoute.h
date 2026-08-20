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

#import <AVKit/AVKit.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <wtf/Platform.h>

#if HAVE(AVSYSTEMROUTING_FRAMEWORK)
#import <AVSystemRouting/AVSystemRouting.h>
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@protocol WebMediaDevicePlatformRoute

@property (readonly, copy) NSString *routeDisplayName;
@property (readonly, copy) UTType *protocolType;

@optional
#if HAVE(AVSYSTEMROUTING_FRAMEWORK)
- (BOOL)addSession:(AVSystemRouteSession *)session;
- (BOOL)removeSession:(AVSystemRouteSession *)session;
#else
- (void)startWithURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable, NSObject<AVPlaybackUserInterfaceControllable> * _Nullable))completionHandler;
- (void)stop;
#endif

@end

#if HAVE(AVSYSTEMROUTING_FRAMEWORK)
@interface AVSystemRoute () <WebMediaDevicePlatformRoute>
@end
#endif

NS_HEADER_AUDIT_END(nullability, sendability)
