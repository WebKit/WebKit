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

#import "config.h"
#import "WebMockMediaDeviceRoute.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import "MockMediaDeviceRouteURLCallback.h"
#import <WebCore/JSDOMPromise.h>
#import <WebKitAdditions/WebMockMediaDeviceRouteAdditions.mm>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

NS_ASSUME_NONNULL_BEGIN

static NSErrorDomain const WebMockMediaDeviceRouteErrorDomain = @"WebMockMediaDeviceRouteErrorDomain";

typedef NS_ENUM(NSInteger, WebMockMediaDeviceRouteErrorCode) {
    WebMockMediaDeviceRouteErrorCodeInvalidState,
    WebMockMediaDeviceRouteErrorCodeURLPromiseRejected,
};

@implementation WebMockMediaDeviceRoute {
    RefPtr<WebCore::MockMediaDeviceRouteURLCallback> _urlCallback;
    RefPtr<WebCore::DOMPromise> _urlPromise;
}

@synthesize currentAudioOption;
@synthesize currentSegment;
@synthesize currentSubtitleOption;
@synthesize currentValue;
@synthesize hasAudio;
@synthesize isAudioOnly;
@synthesize isPlaying;
@synthesize maxValue;
@synthesize minValue;
@synthesize muted;
@synthesize options;
@synthesize playbackError;
@synthesize playbackSpeed;
@synthesize playbackType;
@synthesize scanSpeed;
@synthesize segments;
@synthesize state;
@synthesize supportedModes;
@synthesize volume;

- (WebCore::MockMediaDeviceRouteURLCallback* _Nullable)urlCallback
{
    return _urlCallback.get();
}

- (void)setURLCallback:(WebCore::MockMediaDeviceRouteURLCallback* _Nullable)urlCallback
{
    _urlCallback = urlCallback;
}

- (void)startApplicationWithURL:(NSURL *)url launchType:(WebMediaDevicePlatformRouteLaunchType)launchType withCompletionHandler:(void (^)(NSError * _Nullable, WebMediaDevicePlatformRouteLaunchResult * _Nullable))completionHandler
{
    if (!_urlCallback)
        return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeInvalidState userInfo:nil], nil);

    _urlPromise = _urlCallback->invoke(url.absoluteString).releaseReturnValue();
    _urlPromise->whenSettled([weakSelf = WeakObjCPtr { self }, completionHandler = makeBlockPtr(completionHandler)]() {
        RetainPtr protectedSelf = weakSelf.get();
        if (!protectedSelf)
            return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeInvalidState userInfo:nil], nil);

        switch (std::exchange(protectedSelf->_urlPromise, nullptr)->status()) {
        case WebCore::DOMPromise::Status::Fulfilled:
            return completionHandler(nil, nil);
        case WebCore::DOMPromise::Status::Rejected:
            return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeURLPromiseRejected userInfo:nil], nil);
        case WebCore::DOMPromise::Status::Pending:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    });
}

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
