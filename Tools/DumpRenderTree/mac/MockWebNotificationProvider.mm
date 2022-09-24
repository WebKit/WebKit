/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MockWebNotificationProvider.h"

#import <WebKit/WebSecurityOriginPrivate.h>
#import <wtf/UUID.h>
#import <wtf/text/StringHash.h>

@implementation MockWebNotificationProvider

+ (MockWebNotificationProvider *)shared
{
    static MockWebNotificationProvider *provider = [[MockWebNotificationProvider alloc] init];
    return provider;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;
    _permissions = adoptNS([[NSMutableDictionary alloc] init]);
    return self;
}

- (void)registerWebView:(WebView *)webView
{
    ASSERT(!_registeredWebViews.contains((__bridge CFTypeRef)webView));
    _registeredWebViews.add((__bridge CFTypeRef)webView);
}

- (void)unregisterWebView:(WebView *)webView
{
    ASSERT(_registeredWebViews.contains((__bridge CFTypeRef)webView));
    _registeredWebViews.remove((__bridge CFTypeRef)webView);
}

- (void)showNotification:(WebNotification *)notification fromWebView:(WebView *)webView
{
    ASSERT(_registeredWebViews.contains((__bridge CFTypeRef)webView));

    String notificationID([notification notificationID]);
    _notifications.add(notificationID, notification);
    _notificationViewMap.add(notificationID, (__bridge CFTypeRef)webView);

    [webView _notificationDidShow:[notification notificationID]];
}

- (void)cancelNotification:(WebNotification *)notification
{
    String notificationID([notification notificationID]);
    ASSERT(_notifications.contains(notificationID));

    [(__bridge WebView *)_notificationViewMap.get(notificationID) _notificationsDidClose:@[[notification notificationID]]];
}

- (void)notificationDestroyed:(WebNotification *)notification
{
    _notifications.remove(String([notification notificationID]));
    _notificationViewMap.remove(String([notification notificationID]));
}

- (void)clearNotifications:(NSArray *)notificationIDs
{
    for (NSString *nsNotificationID in notificationIDs) {
        String notificationID(nsNotificationID);
        RetainPtr<WebNotification> notification = _notifications.take(notificationID);
        _notificationViewMap.remove(notificationID);
    }
}

- (void)webView:(WebView *)webView didShowNotification:(NSString *)notificationID
{
    [_notifications.get(String(notificationID)).get() dispatchShowEvent];
}

- (void)webView:(WebView *)webView didClickNotification:(NSString *)notificationID
{
    [_notifications.get(String(notificationID)).get() dispatchClickEvent];
}

- (void)webView:(WebView *)webView didCloseNotifications:(NSArray *)notificationIDs
{
    for (NSString *nsNotificationID in notificationIDs) {
        String notificationID(nsNotificationID);
        NotificationIDMap::iterator it = _notifications.find(notificationID);
        ASSERT(it != _notifications.end());
        [it->value.get() dispatchCloseEvent];
        _notifications.remove(it);
        _notificationViewMap.remove(notificationID);
    }
}

- (void)simulateWebNotificationClick:(NSString *)nsNotificationID
{
    String notificationID(nsNotificationID);
    ASSERT(_notifications.contains(notificationID));
    [(__bridge WebView *)_notificationViewMap.get(notificationID) _notificationDidClick:nsNotificationID];
}

- (WebNotificationPermission)policyForOrigin:(WebSecurityOrigin *)origin
{
    NSNumber *permission = [_permissions.get() objectForKey:[origin stringValue]];
    if (!permission)
        return WebNotificationPermissionNotAllowed;
    if ([permission boolValue])
        return WebNotificationPermissionAllowed;
    return WebNotificationPermissionDenied;
}

- (void)setWebNotificationOrigin:(NSString *)origin permission:(BOOL)allowed
{
    [_permissions.get() setObject:[NSNumber numberWithBool:allowed] forKey:origin];
}

- (void)removeAllWebNotificationPermissions
{
    [_permissions.get() removeAllObjects];
}

- (void)reset
{
    auto notifications = WTFMove(_notifications);
    for (auto notification : notifications.values())
        [notification finalize];

    _notificationViewMap.clear();
    [self removeAllWebNotificationPermissions];
}

@end
