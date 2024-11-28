/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebNotificationManagerMessageHandler.h"

#include "Logging.h"
#include "ServiceWorkerNotificationHandler.h"
#include "WebPageProxy.h"
#include <WebCore/NotificationData.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

WebNotificationManagerMessageHandler::WebNotificationManagerMessageHandler(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
{
}

void WebNotificationManagerMessageHandler::ref() const
{
    m_webPageProxy->ref();
}

void WebNotificationManagerMessageHandler::deref() const
{
    m_webPageProxy->deref();
}

Ref<WebPageProxy> WebNotificationManagerMessageHandler::protectedPage() const
{
    return m_webPageProxy.get();
}

void WebNotificationManagerMessageHandler::showNotification(IPC::Connection& connection, const WebCore::NotificationData& data, RefPtr<WebCore::NotificationResources>&& resources, CompletionHandler<void()>&& callback)
{
    RELEASE_LOG(Push, "WebNotificationManagerMessageHandler showNotification called");

    if (!data.serviceWorkerRegistrationURL.isEmpty()) {
        ServiceWorkerNotificationHandler::singleton().showNotification(connection, data, WTFMove(resources), WTFMove(callback));
        return;
    }
    protectedPage()->showNotification(connection, data, WTFMove(resources));
    callback();
}

void WebNotificationManagerMessageHandler::cancelNotification(WebCore::SecurityOriginData&& origin, const WTF::UUID& notificationID)
{
    Ref serviceWorkerNotificationHandler = ServiceWorkerNotificationHandler::singleton();
    if (serviceWorkerNotificationHandler->handlesNotification(notificationID)) {
        serviceWorkerNotificationHandler->cancelNotification(WTFMove(origin), notificationID);
        return;
    }
    protectedPage()->cancelNotification(notificationID);
}

void WebNotificationManagerMessageHandler::clearNotifications(const Vector<WTF::UUID>& notificationIDs)
{
    Ref serviceWorkerNotificationHandler = ServiceWorkerNotificationHandler::singleton();

    Vector<WTF::UUID> persistentNotifications;
    Vector<WTF::UUID> pageNotifications;
    persistentNotifications.reserveInitialCapacity(notificationIDs.size());
    pageNotifications.reserveInitialCapacity(notificationIDs.size());
    for (auto& notificationID : notificationIDs) {
        if (serviceWorkerNotificationHandler->handlesNotification(notificationID))
            persistentNotifications.append(notificationID);
        else
            pageNotifications.append(notificationID);
    }
    if (!persistentNotifications.isEmpty())
        serviceWorkerNotificationHandler->clearNotifications(persistentNotifications);
    if (!pageNotifications.isEmpty())
        protectedPage()->clearNotifications(pageNotifications);
}

void WebNotificationManagerMessageHandler::didDestroyNotification(const WTF::UUID& notificationID)
{
    Ref serviceWorkerNotificationHandler = ServiceWorkerNotificationHandler::singleton();
    if (serviceWorkerNotificationHandler->handlesNotification(notificationID)) {
        serviceWorkerNotificationHandler->didDestroyNotification(notificationID);
        return;
    }
    protectedPage()->didDestroyNotification(notificationID);
}

void WebNotificationManagerMessageHandler::pageWasNotifiedOfNotificationPermission()
{
    protectedPage()->pageWillLikelyUseNotifications();
}

void WebNotificationManagerMessageHandler::requestPermission(WebCore::SecurityOriginData&&, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler({ });
}

void WebNotificationManagerMessageHandler::getPermissionState(WebCore::SecurityOriginData&&, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler({ });
}

void WebNotificationManagerMessageHandler::getPermissionStateSync(WebCore::SecurityOriginData&&, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler({ });
}

} // namespace WebKit
