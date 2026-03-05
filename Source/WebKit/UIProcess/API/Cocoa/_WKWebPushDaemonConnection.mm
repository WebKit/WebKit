/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#import "_WKWebPushDaemonConnectionInternal.h"

#import "APIWebPushMessage.h"
#import "APIWebPushSubscriptionData.h"
#import "WKError.h"
#import "WKSecurityOriginInternal.h"
#import "WebPushDaemonConnectionConfiguration.h"
#import "_WKNotificationDataInternal.h"
#import "_WKWebPushMessageInternal.h"
#import "_WKWebPushSubscriptionDataInternal.h"
#import <WebCore/ExceptionData.h>
#import <WebCore/PushPermissionState.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKWebPushDaemonConnectionConfiguration

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

#if ENABLE(RELOCATABLE_WEBPUSHD)
    self.machServiceName = @"com.apple.webkit.webpushd.relocatable.service";
#else
    self.machServiceName = @"com.apple.webkit.webpushd.service";
#endif
    return self;
}

- (void)dealloc
{
IGNORE_NULL_CHECK_WARNINGS_BEGIN
    self.machServiceName = nil;
IGNORE_NULL_CHECK_WARNINGS_END
    self.partition = nil;
    self.bundleIdentifierOverrideForTesting = nil;

    [super dealloc];
}

@end

@implementation _WKWebPushDaemonConnection

- (instancetype)initWithConfiguration:(_WKWebPushDaemonConnectionConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    WebKit::WebPushD::WebPushDaemonConnectionConfiguration connectionConfiguration {
        { },
        configuration.bundleIdentifierOverrideForTesting,
        configuration.partition,
        { },
    };

#if !USE(EXTENSIONKIT)
    auto hostAppAuditToken = configuration.hostApplicationAuditToken;
    Vector<uint8_t> hostAppAuditTokenData(sizeof(hostAppAuditToken));
    memcpySpan(hostAppAuditTokenData.mutableSpan(), asByteSpan(hostAppAuditToken));
    connectionConfiguration.hostAppAuditTokenData = WTF::move(hostAppAuditTokenData);
#endif

    API::Object::constructInWrapper<API::WebPushDaemonConnection>(self, configuration.machServiceName, WTF::move(connectionConfiguration));

    return self;
}

static _WKWebPushPermissionState NODELETE toWKPermissionsState(WebCore::PushPermissionState state)
{
    switch (state) {
    case WebCore::PushPermissionState::Denied:
        return _WKWebPushPermissionStateDenied;
    case WebCore::PushPermissionState::Granted:
        return _WKWebPushPermissionStateGranted;
    case WebCore::PushPermissionState::Prompt:
        return _WKWebPushPermissionStatePrompt;
    }

    return _WKWebPushPermissionStateDenied;
}

- (void)getPushPermissionStateForOrigin:(NSURL *)originURL completionHandler:(void (^)(_WKWebPushPermissionState))completionHandler
{
    protect(*_connection)->getPushPermissionState(originURL, [completionHandlerCopy = makeBlockPtr(completionHandler)](auto result) {
        completionHandlerCopy(toWKPermissionsState(result));
    });
}

- (void)requestPushPermissionForOrigin:(NSURL *)originURL completionHandler:(void (^)(BOOL))completionHandler
{
    protect(*_connection)->requestPushPermission(originURL, [completionHandlerCopy = makeBlockPtr(completionHandler)] (bool result) {
        completionHandlerCopy(result);
    });
}

- (void)setAppBadge:(NSUInteger *)badge origin:(NSURL *)originURL
{
    std::optional<uint64_t> badgeValue = badge ? std::optional<uint64_t> { (uint64_t)badge } : std::nullopt;
    protect(*_connection)->setAppBadge(originURL, badgeValue);
}

- (void)subscribeToPushServiceForScope:(NSURL *)scopeURL applicationServerKey:(NSData *)applicationServerKey completionHandler:(void (^)(_WKWebPushSubscriptionData *, NSError *))completionHandler
{
    auto key = makeVector(applicationServerKey);
    protect(*_connection)->subscribeToPushService(scopeURL, WTF::move(key), [completionHandlerCopy = makeBlockPtr(completionHandler)] (auto result) {
        if (result)
            return completionHandlerCopy(wrapper(API::WebPushSubscriptionData::create(WTF::move(result.value()))).get(), nil);

        // FIXME: This error can be used to create DOMException; we may consider adding a new value to WKErrorCode for it.
        RetainPtr error = adoptNS([[NSError alloc] initWithDomain:@"WKErrorDomain" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:result.error().message.createNSString().get() }]);
        completionHandlerCopy(nil, error.get());
    });
}

- (void)unsubscribeFromPushServiceForScope:(NSURL *)scopeURL completionHandler:(void (^)(BOOL unsubscribed, NSError *))completionHandler
{
    protect(*_connection)->unsubscribeFromPushService(scopeURL, [completionHandlerCopy = makeBlockPtr(completionHandler)] (auto result) {
        if (result)
            return completionHandlerCopy(result.value(), nil);

        RetainPtr error = adoptNS([[NSError alloc] initWithDomain:@"WKErrorDomain" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:result.error().message.createNSString().get() }]);
        completionHandlerCopy(false, error.get());
    });
}

- (void)getSubscriptionForScope:(NSURL *)scopeURL completionHandler:(void (^)(_WKWebPushSubscriptionData *, NSError *))completionHandler
{
    protect(*_connection)->getPushSubscription(scopeURL, [completionHandlerCopy = makeBlockPtr(completionHandler)] (auto result) {
        if (result) {
            if (auto data = result.value())
                return completionHandlerCopy(wrapper(API::WebPushSubscriptionData::create(WTF::move(*data))).get(), nil);

            return completionHandlerCopy(nil, nil);
        }

        RetainPtr error = adoptNS([[NSError alloc] initWithDomain:@"WKErrorDomain" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:result.error().message.createNSString().get() }]);
        completionHandlerCopy(nil, error.get());
    });
}

- (void)getNextPendingPushMessage:(void (^)(_WKWebPushMessage *))completionHandler
{
    protect(*_connection)->getNextPendingPushMessage([completionHandlerCopy = makeBlockPtr(completionHandler)] (auto result) {
        if (!result)
            return completionHandlerCopy(nil);

        return completionHandlerCopy(wrapper(API::WebPushMessage::create(WTF::move(result.value()))).get());
    });
}


- (void)showNotification:(_WKNotificationData *)notificationData completionHandler:(void (^)(void))completionHandler
{
    protect(*_connection)->showNotification([notificationData _getCoreData], [completionHandlerCopy = makeBlockPtr(completionHandler)] () {
        completionHandlerCopy();
    });
}

- (void)getNotifications:(NSURL *)scopeURL tag:(NSString *)tag completionHandler:(void (^)(NSArray<_WKNotificationData *> *, NSError *))completionHandler
{
    protect(*_connection)->getNotifications(scopeURL, tag, [completionHandlerCopy = makeBlockPtr(completionHandler)] (auto result) {
        if (result) {
            NSMutableArray<_WKNotificationData *> *nsResult = [NSMutableArray arrayWithCapacity:result.value().size()];
            for (auto& data : result.value())
                [nsResult addObject:[[[_WKNotificationData alloc] _initWithCoreData:data] autorelease]];

            return completionHandlerCopy(nsResult, nil);
        }

        RetainPtr error = adoptNS([[NSError alloc] initWithDomain:@"WKErrorDomain" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:result.error().message.createNSString().get() }]);
        completionHandlerCopy(nil, error.get());
    });
}

- (void)cancelNotification:(NSURL *)securityOriginURL uuid:(NSUUID *)notificationIdentifier
{
    // UUID::fromNSUUID only fails if the passed in NSUUID is nil, which would be a crash-worthy misuse of this API
    protect(*_connection)->cancelNotification(securityOriginURL, *WTF::UUID::fromNSUUID(notificationIdentifier));
}

- (API::Object&)_apiObject
{
    return *_connection;
}

@end
