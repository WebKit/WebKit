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

#import <UserNotifications/UserNotifications.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UserNotifications/UNNotificationContent_Private.h>
#import <UserNotifications/UNNotificationSettings_Private.h>
#import <UserNotifications/UNUserNotificationCenter_Private.h>

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
#import <UserNotifications/UNNotificationIcon.h>
#import <UserNotifications/UNNotificationIcon_Private.h>
#import <UserNotifications/UNNotification_Private.h>
#endif

#else // USE(APPLE_INTERNAL_SDK)

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
@interface UNNotification ()
+ (instancetype)notificationWithRequest:(UNNotificationRequest *)request date:(NSDate *)date;

@property (readonly) NSString *sourceIdentifier;

@end

@interface UNNotificationIcon : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)iconForApplicationIdentifier:(NSString *)applicationIdentifier;
@end
#endif

@interface UNMutableNotificationContent ()
@property (NS_NONATOMIC_IOSONLY, copy) NSString *defaultActionBundleIdentifier;
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
@property (NS_NONATOMIC_IOSONLY, copy) UNNotificationIcon *icon;
#endif
@end

@interface UNMutableNotificationSettings : UNNotificationSettings
+ (instancetype)emptySettings;
@property (NS_NONATOMIC_IOSONLY, readwrite) UNAuthorizationStatus authorizationStatus;
@end

@interface UNUserNotificationCenter ()
- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier;
- (UNNotificationSettings *)notificationSettings;
@end

#endif // USE(APPLE_INTERNAL_SDK)

