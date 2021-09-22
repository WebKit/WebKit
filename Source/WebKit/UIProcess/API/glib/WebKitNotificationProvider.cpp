/*
 * Copyright (C) 2013 Igalia S.L.
 * Copyright (C) 2014 Collabora Ltd.
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

#include "config.h"
#include "WebKitNotificationProvider.h"

#include "APIArray.h"
#include "APINotificationProvider.h"
#include "WebKitNotificationPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"
#include <wtf/text/CString.h>

using namespace WebKit;


class NotificationProvider final : public API::NotificationProvider {
public:
    explicit NotificationProvider(WebKitNotificationProvider& provider)
        : m_provider(provider)
    {
    }

private:
    void show(WebPageProxy& page, WebNotification& notification) override
    {
        m_provider.show(page, notification);
    }

    void cancel(WebNotification& notification) override
    {
        m_provider.cancel(notification);
    }

    void clearNotifications(const Vector<uint64_t>& notificationIDs) override
    {
        m_provider.clearNotifications(notificationIDs);
    }

    HashMap<String, bool> notificationPermissions() override
    {
        return m_provider.notificationPermissions();
    }

    WebKitNotificationProvider& m_provider;
};

WebKitNotificationProvider::WebKitNotificationProvider(WebNotificationManagerProxy* notificationManager, WebKitWebContext* webContext)
    : m_webContext(webContext)
    , m_notificationManager(notificationManager)
{
    ASSERT(m_notificationManager);
    m_notificationManager->setProvider(makeUnique<NotificationProvider>(*this));
}

WebKitNotificationProvider::~WebKitNotificationProvider()
{
    m_notificationManager->setProvider(nullptr);
}

void WebKitNotificationProvider::notificationCloseCallback(WebKitNotification* notification, WebKitNotificationProvider* provider)
{
    uint64_t notificationID = webkit_notification_get_id(notification);
    Vector<RefPtr<API::Object>> arrayIDs;
    arrayIDs.append(API::UInt64::create(notificationID));
    provider->m_notificationManager->providerDidCloseNotifications(API::Array::create(WTFMove(arrayIDs)).ptr());
    provider->m_notifications.remove(notificationID);
}

void WebKitNotificationProvider::notificationClickedCallback(WebKitNotification* notification, WebKitNotificationProvider* provider)
{
    provider->m_notificationManager->providerDidClickNotification(webkit_notification_get_id(notification));
}

void WebKitNotificationProvider::withdrawAnyPreviousNotificationMatchingTag(const CString& tag)
{
    if (!tag.length())
        return;

    for (auto& notification : m_notifications.values()) {
        if (tag == webkit_notification_get_tag(notification.get())) {
            webkit_notification_close(notification.get());
            break;
        }
    }

#ifndef NDEBUG
    for (auto& notification : m_notifications.values())
        ASSERT(tag != webkit_notification_get_tag(notification.get()));
#endif
}

void WebKitNotificationProvider::show(WebPageProxy& page, const WebNotification& webNotification)
{
    GRefPtr<WebKitNotification> notification = m_notifications.get(webNotification.notificationID());
    auto* webView = webkitWebContextGetWebViewForPage(m_webContext, &page);
    ASSERT(webView);

    if (!notification) {
        withdrawAnyPreviousNotificationMatchingTag(webNotification.tag().utf8());
        notification = adoptGRef(webkitNotificationCreate(webView, webNotification));
        g_signal_connect(notification.get(), "closed", G_CALLBACK(notificationCloseCallback), this);
        g_signal_connect(notification.get(), "clicked", G_CALLBACK(notificationClickedCallback), this);
        m_notifications.set(webNotification.notificationID(), notification);
    }

    if (webkitWebViewEmitShowNotification(webView, notification.get()))
        m_notificationManager->providerDidShowNotification(webNotification.notificationID());
}

void WebKitNotificationProvider::cancelNotificationByID(uint64_t notificationID)
{
    if (GRefPtr<WebKitNotification> notification = m_notifications.get(notificationID))
        webkit_notification_close(notification.get());
}

void WebKitNotificationProvider::cancel(const WebNotification& webNotification)
{
    cancelNotificationByID(webNotification.notificationID());
}

void WebKitNotificationProvider::clearNotifications(const Vector<uint64_t>& notificationIDs)
{
    for (const auto& item : notificationIDs)
        cancelNotificationByID(item);
}

HashMap<WTF::String, bool> WebKitNotificationProvider::notificationPermissions()
{
    webkitWebContextInitializeNotificationPermissions(m_webContext);
    return m_notificationPermissions;
}

void WebKitNotificationProvider::setNotificationPermissions(HashMap<String, bool>&& permissionsMap)
{
    m_notificationPermissions = WTFMove(permissionsMap);
}
