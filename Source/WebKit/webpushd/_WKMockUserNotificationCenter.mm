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
#import "_WKMockUserNotificationCenter.h"

#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

@interface _WKMockUserNotificationCenter ()
- (instancetype)initWithBundleIdentifierInternal:(NSString *)bundleIdentifier;
@end

static _WKMockUserNotificationCenter *centersByBundleIdentifier(NSString *bundleIdentifier)
{
    static NeverDestroyed<RetainPtr<NSMutableDictionary>> centers = adoptNS([NSMutableDictionary new]);

    if (!centers->get()[bundleIdentifier])
        centers->get()[bundleIdentifier] = adoptNS([[_WKMockUserNotificationCenter alloc] initWithBundleIdentifierInternal:bundleIdentifier]).get();

    return centers->get()[bundleIdentifier];
}

@implementation _WKMockUserNotificationCenter {
    OSObjectPtr<dispatch_queue_t> m_queue;
    BOOL m_hasPermission;
    RetainPtr<NSString> m_bundleIdentifier;
    RetainPtr<NSMutableArray> m_notifications;
    RetainPtr<NSNumber> m_appBadge;
}

- (instancetype)initWithBundleIdentifierInternal:(NSString *)bundleIdentifier
{
    self = [super init];
    if (!self)
        return nil;

    m_queue = adoptOSObject(dispatch_queue_create(nullptr, OSObjectPtr { DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL }.get()));
    m_bundleIdentifier = bundleIdentifier;
    m_notifications = adoptNS([[NSMutableArray alloc] init]);

    return self;
}

- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier
{
    self = centersByBundleIdentifier(bundleIdentifier);
    return [self retain];
}

- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(nullable void(^)(NSError *error))completionHandler
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [m_notifications.get() addObject:[UNNotification notificationWithRequest:request date:[NSDate now]]];
#pragma clang diagnostic pop

    // For testing purposes, we know that requests without a targetContentIdentifier are for badging only
    if (!request.content.targetContentIdentifier)
        m_appBadge = request.content.badge;

    dispatch_async(m_queue.get(), ^{
        completionHandler(nil);
    });
}

- (NSNumber *)getAppBadgeForTesting
{
    return m_appBadge.get();
}

- (void)getDeliveredNotificationsWithCompletionHandler:(void(^)(NSArray<UNNotification *> *notifications))completionHandler
{
    RetainPtr<NSArray> notifications = adoptNS([m_notifications copy]);
    dispatch_async(m_queue.get(), ^{
        completionHandler(notifications.get());
    });
}


- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray<NSString *> *) identifiers
{
    RetainPtr toRemove = adoptNS([NSMutableArray new]);
    for (UNNotification *notification in m_notifications.get()) {
        if ([identifiers containsObject:retainPtr(notification.request.identifier).get()])
            [toRemove addObject:notification];
    }

    [m_notifications removeObjectsInArray:toRemove.get()];
}

- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray<NSString *> *) identifiers
{
    // For now, the mock UNUserNotificationCenter doesn't distinguish between pending and delivered notifications.
    [self removePendingNotificationRequestsWithIdentifiers:identifiers];
}

- (void)getNotificationSettingsWithCompletionHandler:(void(^)(UNNotificationSettings *settings))completionHandler
{
    BOOL hasPermission = m_hasPermission;
    dispatch_async(m_queue.get(), ^{
        UNMutableNotificationSettings *settings = [UNMutableNotificationSettings emptySettings];
        settings.authorizationStatus = hasPermission ? UNAuthorizationStatusAuthorized : UNAuthorizationStatusNotDetermined;
        completionHandler(settings);
    });
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options completionHandler:(void (^)(BOOL granted, NSError *))completionHandler
{
    m_hasPermission = YES;
    dispatch_async(m_queue.get(), ^{
        completionHandler(YES, nil);
    });
}

- (void)setNotificationCategories:(NSSet<UNNotificationCategory *> *) categories
{
    // No-op. Stubbed out for compatibiltiy with UNUserNotificationCenter.
}

- (UNNotificationSettings *)notificationSettings
{
    RetainPtr settings = [UNMutableNotificationSettings emptySettings];
    [settings setAuthorizationStatus:m_hasPermission ? UNAuthorizationStatusAuthorized : UNAuthorizationStatusNotDetermined];
    return settings.autorelease();
}

@end

#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
