/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import <WebKitAdditions/WebMockMediaDeviceRouteAdditions.h>

typedef NS_ENUM(NSInteger, WebMockMediaDeviceRouteErrorCode) {
    WebMockMediaDeviceRouteErrorCodeInvalidState,
    WebMockMediaDeviceRouteErrorCodeUnsupportedURL,
    WebMockMediaDeviceRouteErrorCodePlaybackError,
};

namespace WebCore {
class MockMediaDeviceRouteURLCallback;
}

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const WebMockMediaDeviceRouteErrorDomain;

@interface WebMockMediaDeviceRoute : NSObject <AVMediaSource, WebMediaDevicePlatformRoute>
@property (nonatomic, nullable, setter=setURLCallback:) WebCore::MockMediaDeviceRouteURLCallback* urlCallback;
@property (copy) NSString *routeDisplayName;
@property (nonatomic, getter=isReady) BOOL ready;
@property (nonatomic, strong, nullable) NSError *playbackError;
@property (nonatomic) CMTimeRange timeRange;
@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
