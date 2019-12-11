/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#import "WebNotificationClient.h"

#if ENABLE(NOTIFICATIONS)

#import "WebDelegateImplementationCaching.h"
#import "WebNotificationInternal.h"
#import "WebPreferencesPrivate.h"
#import "WebSecurityOriginInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/NotificationPermissionCallback.h>
#import <WebCore/ScriptExecutionContext.h>
#import <wtf/BlockObjCExceptions.h>

using namespace WebCore;

@interface WebNotificationPolicyListener : NSObject <WebAllowDenyPolicyListener>
{
    RefPtr<NotificationPermissionCallback> _callback;
}
- (id)initWithCallback:(RefPtr<NotificationPermissionCallback>&&)callback;
@end

static uint64_t generateNotificationID()
{
    static uint64_t uniqueNotificationID = 1;
    return uniqueNotificationID++;
}

WebNotificationClient::WebNotificationClient(WebView *webView)
    : m_webView(webView)
{
}

bool WebNotificationClient::show(Notification* notification)
{
    if (![m_webView _notificationProvider])
        return false;

    uint64_t notificationID = generateNotificationID();
    RetainPtr<WebNotification> webNotification = adoptNS([[WebNotification alloc] initWithCoreNotification:notification notificationID:notificationID]);
    m_notificationMap.set(notification, webNotification);

    auto it = m_notificationContextMap.add(notification->scriptExecutionContext(), Vector<RetainPtr<WebNotification>>()).iterator;
    it->value.append(webNotification);

    [[m_webView _notificationProvider] showNotification:webNotification.get() fromWebView:m_webView];
    return true;
}

void WebNotificationClient::cancel(Notification* notification)
{
    WebNotification *webNotification = m_notificationMap.get(notification).get();
    if (!webNotification)
        return;

    [[m_webView _notificationProvider] cancelNotification:webNotification];
}

void WebNotificationClient::clearNotifications(ScriptExecutionContext* context)
{
    auto it = m_notificationContextMap.find(context);
    if (it == m_notificationContextMap.end())
        return;
    
    Vector<RetainPtr<WebNotification>>& webNotifications = it->value;
    NSMutableArray *nsIDs = [NSMutableArray array];
    size_t count = webNotifications.size();
    for (size_t i = 0; i < count; ++i) {
        WebNotification *webNotification = webNotifications[i].get();
        [nsIDs addObject:[NSNumber numberWithUnsignedLongLong:[webNotification notificationID]]];
        core(webNotification)->finalize();
        m_notificationMap.remove(core(webNotification));
    }

    [[m_webView _notificationProvider] clearNotifications:nsIDs];
    m_notificationContextMap.remove(it);
}

void WebNotificationClient::notificationObjectDestroyed(Notification* notification)
{
    RetainPtr<WebNotification> webNotification = m_notificationMap.take(notification);
    if (!webNotification)
        return;

    auto it = m_notificationContextMap.find(notification->scriptExecutionContext());
    ASSERT(it != m_notificationContextMap.end());
    size_t index = it->value.find(webNotification);
    ASSERT(index != notFound);
    it->value.remove(index);
    if (it->value.isEmpty())
        m_notificationContextMap.remove(it);

    [[m_webView _notificationProvider] notificationDestroyed:webNotification.get()];
}

void WebNotificationClient::notificationControllerDestroyed()
{
    delete this;
}

void WebNotificationClient::requestPermission(ScriptExecutionContext* context, WebNotificationPolicyListener *listener)
{
    SEL selector = @selector(webView:decidePolicyForNotificationRequestFromOrigin:listener:);
    if (![[m_webView UIDelegate] respondsToSelector:selector])
        return;

    m_everRequestedPermission = true;

    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:context->securityOrigin()];
    
    CallUIDelegate(m_webView, selector, webOrigin, listener);
    
    [webOrigin release];
}

bool WebNotificationClient::hasPendingPermissionRequests(ScriptExecutionContext*) const
{
    // We know permission was requested but we don't know if the client responded. In this case, we play it
    // safe and presume there is one pending so that ActiveDOMObjects don't get suspended.
    return m_everRequestedPermission;
}

void WebNotificationClient::requestPermission(ScriptExecutionContext* context, RefPtr<NotificationPermissionCallback>&& callback)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebNotificationPolicyListener *listener = [[WebNotificationPolicyListener alloc] initWithCallback:WTFMove(callback)];
    requestPermission(context, listener);
    [listener release];
    END_BLOCK_OBJC_EXCEPTIONS;
}

NotificationClient::Permission WebNotificationClient::checkPermission(ScriptExecutionContext* context)
{
    if (!context || !context->isDocument())
        return NotificationClient::Permission::Denied;
    if (![[m_webView preferences] notificationsEnabled])
        return NotificationClient::Permission::Denied;
    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:context->securityOrigin()];
    WebNotificationPermission permission = [[m_webView _notificationProvider] policyForOrigin:webOrigin];
    [webOrigin release];
    switch (permission) {
        case WebNotificationPermissionAllowed:
            return NotificationClient::Permission::Granted;
        case WebNotificationPermissionDenied:
            return NotificationClient::Permission::Denied;
        case WebNotificationPermissionNotAllowed:
            return NotificationClient::Permission::Default;
        default:
            return NotificationClient::Permission::Default;
    }
}

uint64_t WebNotificationClient::notificationIDForTesting(WebCore::Notification* notification)
{
    return [m_notificationMap.get(notification).get() notificationID];
}

@implementation WebNotificationPolicyListener

- (id)initWithCallback:(RefPtr<NotificationPermissionCallback>&&)callback
{
    if (!(self = [super init]))
        return nil;

    _callback = WTFMove(callback);
    return self;
}

- (void)allow
{
    if (_callback)
        _callback->handleEvent(NotificationClient::Permission::Granted);
}

- (void)deny
{
    if (_callback)
        _callback->handleEvent(NotificationClient::Permission::Denied);
}

#if PLATFORM(IOS_FAMILY)
- (void)denyOnlyThisRequest
{
    ASSERT_NOT_REACHED();
}

- (BOOL)shouldClearCache
{
    ASSERT_NOT_REACHED();
    return NO;
}
#endif

@end

#endif
