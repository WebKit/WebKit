/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "_WKWebPushActionInternal.h"

#import "Logging.h"
#import "UserNotificationsSPI.h"
#import "WebPushDaemonConstants.h"

#if PLATFORM(IOS)
#import "UIKitSPI.h"
#endif

NSString * const _WKWebPushActionTypePushEvent = @"_WKWebPushActionTypePushEvent";
NSString * const _WKWebPushActionTypeNotificationClick = @"_WKWebPushActionTypeNotificationClick";
NSString * const _WKWebPushActionTypeNotificationClose = @"_WKWebPushActionTypeNotificationClose";

@interface _WKWebPushAction ()
@property (nonatomic, readwrite) NSNumber *version;
@property (nonatomic, readwrite) NSString *pushPartition;
@property (nonatomic, readwrite) NSString *type;
@property (nonatomic, readwrite) std::optional<WebCore::NotificationData> coreNotificationData;
@end

@implementation _WKWebPushAction

+ (_WKWebPushAction *)webPushActionWithDictionary:(NSDictionary *)dictionary
{
    NSNumber *version = dictionary[WebKit::WebPushD::pushActionVersionKey()];
    if (!version || ![version isKindOfClass:[NSNumber class]])
        return nil;

    NSString *pushPartition = dictionary[WebKit::WebPushD::pushActionPartitionKey()];
    if (!pushPartition || ![pushPartition isKindOfClass:[NSString class]])
        return nil;

    NSString *type = dictionary[WebKit::WebPushD::pushActionTypeKey()];
    if (!type || ![type isKindOfClass:[NSString class]])
        return nil;

    _WKWebPushAction *result = [[_WKWebPushAction alloc] init];
    result.version = version;
    result.pushPartition = pushPartition;
    result.type = type;

    return [result autorelease];
}

+ (_WKWebPushAction *)_webPushActionWithNotificationResponse:(UNNotificationResponse *)response
{
#if PLATFORM(IOS)
    if (![response.notification.sourceIdentifier hasPrefix:@"com.apple.WebKit.PushBundle."])
        return nil;

    auto notificationData = WebCore::NotificationData::fromDictionary(response.notification.request.content.userInfo);
    if (!notificationData)
        return nil;

    _WKWebPushAction *result = [[[_WKWebPushAction alloc] init] autorelease];

    if ([response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier])
        result.type = _WKWebPushActionTypeNotificationClick;
    else if ([response.actionIdentifier isEqualToString:UNNotificationDismissActionIdentifier])
        result.type = _WKWebPushActionTypeNotificationClose;
    else {
        RELEASE_LOG_ERROR(Push, "Unknown notification response action identifier encountered: %@", response.actionIdentifier);
        return nil;
    }

    result.coreNotificationData = WTFMove(notificationData);

    return result;
#else
    return nil;
#endif // PLATFORM(IOS)
}

- (NSString *)_nameForBackgroundTaskAndLogging
{
    if ([_type isEqualToString:_WKWebPushActionTypePushEvent])
        return @"Web Push Event";
    if ([_type isEqualToString:_WKWebPushActionTypeNotificationClick])
        return @"Web Notification Click";
    if ([_type isEqualToString:_WKWebPushActionTypeNotificationClose])
        return @"Web Notification Close";

    return @"Unknown Web Push event";
}

- (UIBackgroundTaskIdentifier)beginBackgroundTaskForHandling
{
#if PLATFORM(IOS)
    NSString *taskName;
    if (_pushPartition)
        taskName = [NSString stringWithFormat:@"%@ for %@", self._nameForBackgroundTaskAndLogging, _pushPartition];
    else
        taskName = [NSString stringWithFormat:@"%@", self._nameForBackgroundTaskAndLogging];

    return [UIApplication.sharedApplication beginBackgroundTaskWithName:taskName expirationHandler:^{
        RELEASE_LOG_ERROR(Push, "Took too long to handle Web Push action: '%@'", taskName);
    }];
#else
    return 0;
#endif // PLATFORM(IOS)
}

@end
