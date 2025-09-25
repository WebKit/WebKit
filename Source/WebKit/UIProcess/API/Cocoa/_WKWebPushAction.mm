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
@property (nonatomic, readwrite, strong) NSNumber *version;
@property (nonatomic, readwrite, strong) NSUUID *webClipIdentifier;
@property (nonatomic, readwrite, strong) NSString *type;
@property (nonatomic, readwrite, assign) std::optional<WebCore::NotificationData> coreNotificationData;
@end

@implementation _WKWebPushAction

static RetainPtr<NSUUID> uuidFromPushPartition(NSString *pushPartition)
{
    if (pushPartition.length != 32)
        return nil;

    RetainPtr uuidString = adoptNS([[NSString alloc] initWithFormat:@"%@-%@-%@-%@-%@",
        [pushPartition substringWithRange:NSMakeRange(0, 8)],
        [pushPartition substringWithRange:NSMakeRange(8, 4)],
        [pushPartition substringWithRange:NSMakeRange(12, 4)],
        [pushPartition substringWithRange:NSMakeRange(16, 4)],
        [pushPartition substringWithRange:NSMakeRange(20, 12)]]);
    return adoptNS([[NSUUID alloc] initWithUUIDString:uuidString.get()]);
}

- (void)dealloc
{
    [_version release];
    [_webClipIdentifier release];
    [_type release];
    [super dealloc];
}

+ (_WKWebPushAction *)webPushActionWithDictionary:(NSDictionary *)dictionary
{
    auto versionValue = retainPtr(dictionary[WebKit::WebPushD::pushActionVersionKey()]);
    auto pushPartitionValue = retainPtr(dictionary[WebKit::WebPushD::pushActionPartitionKey()]);
    RetainPtr uuid = uuidFromPushPartition(pushPartition);
    auto typeValue = retainPtr(dictionary[WebKit::WebPushD::pushActionTypeKey()]);
    {
        auto* version = dynamic_objc_cast<NSNumber>(versionValue.get());
        auto* pushPartition = dynamic_objc_cast<NSString>(pushPartitionValue.get());
        auto* type = dynamic_objc_cast<NSString>(typeValue.get());
        if (!version || !pushPartition || !uuid || !type)
            return nil;

        RetainPtr result = adoptNS([[_WKWebPushAction alloc] init]);
        result.get().version = version;
        result.get().webClipIdentifier = uuid.get();
        result.get().type = type;

        return result.autorelease();
    }
}

+ (_WKWebPushAction *)_webPushActionWithNotificationResponse:(UNNotificationResponse *)response
{
#if PLATFORM(IOS)
    if (![response.notification.sourceIdentifier hasPrefix:@"com.apple.WebKit.PushBundle."])
        return nil;

    NSString *pushPartition = [response.notification.sourceIdentifier substringFromIndex:28];
    RetainPtr webClipIdentifier = uuidFromPushPartition(pushPartition);
    if (!webClipIdentifier)
        return nil;

    auto notificationData = WebCore::NotificationData::fromDictionary(response.notification.request.content.userInfo);
    if (!notificationData)
        return nil;

    _WKWebPushAction *result = [[[_WKWebPushAction alloc] init] autorelease];
    result.webClipIdentifier = webClipIdentifier.get();

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
    RetainPtr<NSString> taskName;
    if (_webClipIdentifier)
        taskName = adoptNS([[NSString alloc] initWithFormat:@"%@ for %@", self._nameForBackgroundTaskAndLogging, _webClipIdentifier]);
    else
        taskName = adoptNS([[NSString alloc] initWithFormat:@"%@", self._nameForBackgroundTaskAndLogging]);

    return [UIApplication.sharedApplication beginBackgroundTaskWithName:taskName.get() expirationHandler:^{
        RELEASE_LOG_ERROR(Push, "Took too long to handle Web Push action: '%@'", taskName.get());
    }];
#else
    return 0;
#endif // PLATFORM(IOS)
}

@end
