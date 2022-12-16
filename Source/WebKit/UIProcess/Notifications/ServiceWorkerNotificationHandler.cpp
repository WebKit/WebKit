/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ServiceWorkerNotificationHandler.h"

#include "Logging.h"
#include "WebsiteDataStore.h"
#include <WebCore/NotificationData.h>
#include <wtf/Scope.h>

namespace WebKit {

ServiceWorkerNotificationHandler& ServiceWorkerNotificationHandler::singleton()
{
    ASSERT(isMainRunLoop());
    static ServiceWorkerNotificationHandler& handler = *new ServiceWorkerNotificationHandler;
    return handler;
}

void ServiceWorkerNotificationHandler::requestSystemNotificationPermission(const String&, CompletionHandler<void(bool)>&&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

WebsiteDataStore* ServiceWorkerNotificationHandler::dataStoreForNotificationID(const UUID& notificationID)
{
    auto iterator = m_notificationToSessionMap.find(notificationID);
    if (iterator == m_notificationToSessionMap.end())
        return nullptr;

    return WebsiteDataStore::existingDataStoreForSessionID(iterator->value);
}

void ServiceWorkerNotificationHandler::showNotification(IPC::Connection& connection, const WebCore::NotificationData& data, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&& callback)
{
    RELEASE_LOG(Push, "ServiceWorkerNotificationHandler showNotification called");

    auto scope = makeScopeExit([&callback] { callback(); });

    auto* dataStore = WebsiteDataStore::existingDataStoreForSessionID(data.sourceSession);
    if (!dataStore)
        return;

    m_notificationToSessionMap.add(data.notificationID, data.sourceSession);
    dataStore->showServiceWorkerNotification(connection, data);
}

void ServiceWorkerNotificationHandler::cancelNotification(const UUID& notificationID)
{
    if (auto* dataStore = dataStoreForNotificationID(notificationID))
        dataStore->cancelServiceWorkerNotification(notificationID);
}

void ServiceWorkerNotificationHandler::clearNotifications(const Vector<UUID>& notificationIDs)
{
    for (auto& notificationID : notificationIDs) {
        if (auto* dataStore = dataStoreForNotificationID(notificationID))
            dataStore->clearServiceWorkerNotification(notificationID);
    }
}

void ServiceWorkerNotificationHandler::didDestroyNotification(const UUID& notificationID)
{
    if (auto* dataStore = dataStoreForNotificationID(notificationID))
        dataStore->didDestroyServiceWorkerNotification(notificationID);
}

} // namespace WebKit
