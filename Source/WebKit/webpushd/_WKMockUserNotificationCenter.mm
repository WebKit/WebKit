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
#import <wtf/RetainPtr.h>

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

static NSMutableArray *notificationsByBundleIdentifier(NSString *bundleIdentifier)
{
    static NeverDestroyed<RetainPtr<NSMutableDictionary>> notifications = adoptNS([NSMutableDictionary new]);

    if (!notifications->get()[bundleIdentifier])
        notifications->get()[bundleIdentifier] = [NSMutableArray arrayWithCapacity:1];

    return notifications->get()[bundleIdentifier];
}

static NSMutableSet *notificationPermissions()
{
    static NeverDestroyed<RetainPtr<NSMutableSet>> permissions = adoptNS([NSMutableSet new]);
    return permissions->get();
}

@implementation _WKMockUserNotificationCenter {
    RetainPtr<NSString> m_bundleIdentifier;
    RetainPtr<NSMutableArray> m_notifications;
}

- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier
{
    self = [super init];
    if (!self)
        return nil;

    m_bundleIdentifier = bundleIdentifier;
    m_notifications = notificationsByBundleIdentifier(bundleIdentifier);
    return self;
}

- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(nullable void(^)(NSError *error))completionHandler
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [m_notifications.get() addObject:[UNNotification notificationWithRequest:request date:[NSDate now]]];
#pragma clang diagnostic pop

    callOnMainRunLoop([completionHandler = makeBlockPtr(completionHandler)] mutable {
        completionHandler(nil);
    });
}

- (void)getDeliveredNotificationsWithCompletionHandler:(void(^)(NSArray<UNNotification *> *notifications))completionHandler
{
    callOnMainRunLoop([completionHandler = makeBlockPtr(completionHandler), notifications = [m_notifications.get() copy]] mutable {
        completionHandler(notifications);
    });
}


- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray<NSString *> *) identifiers
{
    RetainPtr toRemove = adoptNS([NSMutableArray new]);
    for (UNNotification *notification in m_notifications.get()) {
        if ([identifiers containsObject:notification.request.identifier])
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
    callOnMainRunLoop([completionHandler = makeBlockPtr(completionHandler), hasPermission = [notificationPermissions() containsObject:m_bundleIdentifier.get()]]() mutable {
        RetainPtr settings = [UNMutableNotificationSettings emptySettings];
        [settings setAuthorizationStatus:UNAuthorizationStatusAuthorized];
        completionHandler(settings.get());
    });
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options completionHandler:(void (^)(BOOL granted, NSError *))completionHandler
{
    [notificationPermissions() addObject:m_bundleIdentifier.get()];
    callOnMainRunLoop([completionHandler = makeBlockPtr(completionHandler)] mutable {
        completionHandler(YES, nil);
    });
}

@end

#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
