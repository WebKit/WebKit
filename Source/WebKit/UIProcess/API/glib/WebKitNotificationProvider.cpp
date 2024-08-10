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
#include "APINumber.h"
#include "NotificationService.h"
#include "WebKitNotificationPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>

using namespace WebKit;


class NotificationProvider final : public API::NotificationProvider {
public:
    explicit NotificationProvider(WebKitNotificationProvider& provider)
        : m_provider(provider)
    {
    }

private:
    bool show(WebPageProxy* page, WebNotification& notification, RefPtr<WebCore::NotificationResources>&& resources) override
    {
        m_provider.show(page, notification, WTFMove(resources));
        return true;
    }

    void cancel(WebNotification& notification) override
    {
        m_provider.cancel(notification);
    }

    void clearNotifications(const Vector<WebNotificationIdentifier>& notificationIDs) override
    {
        m_provider.clearNotifications(notificationIDs);
    }

    HashMap<String, bool> notificationPermissions() override
    {
        return m_provider.notificationPermissions();
    }

    WebKitNotificationProvider& m_provider;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebKitNotificationProvider);

WebKitNotificationProvider::WebKitNotificationProvider(WebNotificationManagerProxy* notificationManager, WebKitWebContext* webContext)
    : m_webContext(webContext)
    , m_notificationManager(notificationManager)
{
    ASSERT(m_notificationManager);
    m_notificationManager->setProvider(makeUnique<NotificationProvider>(*this));
}

WebKitNotificationProvider::~WebKitNotificationProvider()
{
    if (m_observerRegistered)
        NotificationService::singleton().removeObserver(*this);

    m_notificationManager->setProvider(nullptr);
}

void WebKitNotificationProvider::apiNotificationCloseCallback(WebKitNotification* notification, WebKitNotificationProvider* provider)
{
    uint64_t notificationID = webkit_notification_get_id(notification);
    Vector<RefPtr<API::Object>> arrayIDs;
    arrayIDs.append(API::UInt64::create(notificationID));
    provider->m_notificationManager->providerDidCloseNotifications(API::Array::create(WTFMove(arrayIDs)).ptr());
    provider->m_apiNotifications.remove(WebNotificationIdentifier { notificationID });
}

void WebKitNotificationProvider::apiNotificationClickedCallback(WebKitNotification* notification, WebKitNotificationProvider* provider)
{
    provider->m_notificationManager->providerDidClickNotification(WebNotificationIdentifier { webkit_notification_get_id(notification) });
}

void WebKitNotificationProvider::closeAPINotification(WebNotificationIdentifier notificationID)
{
    if (auto notification = m_apiNotifications.take(notificationID))
        webkit_notification_close(notification.get());
}

void WebKitNotificationProvider::withdrawAnyPreviousAPINotificationMatchingTag(const CString& tag)
{
    if (!tag.length())
        return;

    for (auto& notification : m_apiNotifications.values()) {
        if (tag == webkit_notification_get_tag(notification.get())) {
            closeAPINotification(WebNotificationIdentifier { webkit_notification_get_id(notification.get()) });
            break;
        }
    }

#if ASSERT_ENABLED
    for (auto& notification : m_apiNotifications.values())
        ASSERT(tag != webkit_notification_get_tag(notification.get()));
#endif
}

void WebKitNotificationProvider::show(WebPageProxy* page, WebNotification& webNotification, RefPtr<WebCore::NotificationResources>&& resources)
{
    if (!page || !m_webContext) {
        // FIXME: glib API needs to find their own solution to handling pageless notifications.
        show(webNotification, resources);
        return;
    }

    GRefPtr<WebKitNotification> notification = m_apiNotifications.get(webNotification.identifier());
    if (!notification) {
        withdrawAnyPreviousAPINotificationMatchingTag(webNotification.tag().utf8());
        notification = adoptGRef(webkitNotificationCreate(webNotification));
        g_signal_connect(notification.get(), "closed", G_CALLBACK(apiNotificationCloseCallback), this);
        g_signal_connect(notification.get(), "clicked", G_CALLBACK(apiNotificationClickedCallback), this);
        m_apiNotifications.set(webNotification.identifier(), notification);
    }

    auto* webView = webkitWebContextGetWebViewForPage(m_webContext, page);
    ASSERT(webView);

    if (webkitWebViewEmitShowNotification(webView, notification.get()))
        m_notificationManager->providerDidShowNotification(webNotification.identifier());
    else {
        g_signal_handlers_disconnect_by_data(notification.get(), this);
        show(webNotification, resources);
    }
}

void WebKitNotificationProvider::show(WebNotification& webNotification, const RefPtr<WebCore::NotificationResources>& resources)
{
    if (!m_observerRegistered) {
        NotificationService::singleton().addObserver(*this);
        m_observerRegistered = true;
    }

    if (NotificationService::singleton().showNotification(webNotification, resources))
        m_notificationManager->providerDidShowNotification(webNotification.identifier());
}

void WebKitNotificationProvider::cancelNotificationByID(WebNotificationIdentifier notificationID)
{
    closeAPINotification(notificationID);

    if (m_observerRegistered)
        NotificationService::singleton().cancelNotification(notificationID);
}

void WebKitNotificationProvider::cancel(const WebNotification& webNotification)
{
    cancelNotificationByID(webNotification.identifier());
}

void WebKitNotificationProvider::clearNotifications(const Vector<WebNotificationIdentifier>& notificationIDs)
{
    for (const auto& item : notificationIDs)
        cancelNotificationByID(item);
}

HashMap<WTF::String, bool> WebKitNotificationProvider::notificationPermissions()
{
    if (m_webContext)
        webkitWebContextInitializeNotificationPermissions(m_webContext);
    return m_notificationPermissions;
}

void WebKitNotificationProvider::setNotificationPermissions(HashMap<String, bool>&& permissionsMap)
{
    m_notificationPermissions = WTFMove(permissionsMap);
}

void WebKitNotificationProvider::didClickNotification(WebNotificationIdentifier notificationID)
{
    if (auto* notification = m_apiNotifications.get(notificationID))
        webkit_notification_clicked(notification);

    m_notificationManager->providerDidClickNotification(notificationID);
}

void WebKitNotificationProvider::didCloseNotification(WebNotificationIdentifier notificationID)
{
    closeAPINotification(notificationID);

    Vector<RefPtr<API::Object>> arrayIDs;
    arrayIDs.append(API::UInt64::create(notificationID.toUInt64()));
    m_notificationManager->providerDidCloseNotifications(API::Array::create(WTFMove(arrayIDs)).ptr());
}
