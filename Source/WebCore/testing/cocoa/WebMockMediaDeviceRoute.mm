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
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/WeakObjCPtr.h>

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlaybackUserInterfacePlaybackPosition)

NS_ASSUME_NONNULL_BEGIN

NSErrorDomain const WebMockMediaDeviceRouteErrorDomain = @"WebMockMediaDeviceRouteErrorDomain";

@interface WebMockMediaDeviceRoute ()
@property (nonatomic, strong, nullable) AVPlaybackUserInterfacePlaybackPosition *playbackPosition;
@end

@implementation WebMockMediaDeviceRoute {
    RefPtr<WebCore::MockMediaDeviceRouteURLCallback> _urlCallback;
    RefPtr<WebCore::DOMPromise> _urlPromise;
}

@synthesize timeRange;
@synthesize segments;
@synthesize currentSegment;
@synthesize seekableTimeRanges;
@synthesize playbackPosition;
@synthesize ready;
@synthesize playing;
@synthesize buffering;
@synthesize playbackSpeed;
@synthesize scanSpeed;
@synthesize state;
@synthesize supportedSeekCapabilities;
@synthesize containsLiveStreamingContent;
@synthesize error;
@synthesize currentAudioOption;
@synthesize currentAudioDescriptionOption;
@synthesize currentLegibleOption;
@synthesize audioOptions;
@synthesize audioDescriptionOptions;
@synthesize legibleOptions;
@synthesize hasAudio;
@synthesize muted;
@synthesize volume;
@synthesize metadata;
@synthesize routeDisplayName;

- (void)seekToPosition:(CMTime)position tolerance:(CMTime)tolerance
{
    RetainPtr playbackPosition = adoptNS([allocAVPlaybackUserInterfacePlaybackPositionInstance() initWithPosition:position hostTime:CMClockGetTime(CMClockGetHostTimeClock()) rate:0]);
    self.playbackPosition = playbackPosition.get();
}

- (WebCore::MockMediaDeviceRouteURLCallback* _Nullable)urlCallback
{
    return _urlCallback.get();
}

- (void)setURLCallback:(WebCore::MockMediaDeviceRouteURLCallback* _Nullable)urlCallback
{
    _urlCallback = urlCallback;
}

- (void)startWithURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable, NSObject<AVPlaybackControl> * _Nullable))completionHandler
{
    if (!_urlCallback)
        return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeInvalidState userInfo:nil], nil);

    auto result = _urlCallback->invoke(url.absoluteString);
    if (result.type() != WebCore::CallbackResultType::Success)
        return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeInvalidState userInfo:nil], nil);

    _urlPromise = result.releaseReturnValue();
    _urlPromise->whenSettled([weakSelf = WeakObjCPtr { self }, completionHandler = makeBlockPtr(completionHandler)]() {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeInvalidState userInfo:nil], nil);

        switch (std::exchange(strongSelf->_urlPromise, nullptr)->status()) {
        case WebCore::DOMPromise::Status::Fulfilled:
            return completionHandler(nil, strongSelf.get());
        case WebCore::DOMPromise::Status::Rejected:
            return completionHandler([NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodeUnsupportedURL userInfo:nil], nil);
        case WebCore::DOMPromise::Status::Pending:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    });
}

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
