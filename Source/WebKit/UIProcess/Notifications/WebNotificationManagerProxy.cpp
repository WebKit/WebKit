/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "WebNotificationManagerProxy.h"

#include "APIArray.h"
#include "APINotificationProvider.h"
#include "APISecurityOrigin.h"
#include "WebNotification.h"
#include "WebNotificationManagerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/NotificationData.h>

namespace WebKit {
using namespace WebCore;

const char* WebNotificationManagerProxy::supplementName()
{
    return "WebNotificationManagerProxy";
}

Ref<WebNotificationManagerProxy> WebNotificationManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebNotificationManagerProxy(processPool));
}

WebNotificationManagerProxy& WebNotificationManagerProxy::sharedServiceWorkerManager()
{
    ASSERT(isMainRunLoop());
    static WebNotificationManagerProxy* sharedManager = new WebNotificationManagerProxy(nullptr);
    return *sharedManager;
}

WebNotificationManagerProxy::WebNotificationManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
    , m_provider(makeUnique<API::NotificationProvider>())
{
}

void WebNotificationManagerProxy::setProvider(std::unique_ptr<API::NotificationProvider>&& provider)
{
    if (!provider) {
        m_provider = makeUnique<API::NotificationProvider>();
        return;
    }

    m_provider = WTFMove(provider);
    m_provider->addNotificationManager(*this);
}

// WebContextSupplement

void WebNotificationManagerProxy::processPoolDestroyed()
{
    m_provider->removeNotificationManager(*this);
}

void WebNotificationManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebNotificationManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

HashMap<String, bool> WebNotificationManagerProxy::notificationPermissions()
{
    return m_provider->notificationPermissions();
}

static WebPageProxyIdentifier identifierForPagePointer(WebPageProxy* webPage)
{
    return webPage ? webPage->identifier() : WebPageProxyIdentifier();
}

void WebNotificationManagerProxy::show(WebPageProxy* webPage, IPC::Connection& connection, const WebCore::NotificationData& notificationData)
{
    LOG(Notifications, "WebPageProxy (%p) asking to show notification (%s)", webPage, notificationData.notificationID.toString().utf8().data());

    auto notification = WebNotification::create(notificationData, identifierForPagePointer(webPage), connection);
    m_globalNotificationMap.set(notification->notificationID(), notification->coreNotificationID());
    m_notifications.set(notification->coreNotificationID(), notification);
    m_provider->show(webPage, notification.get());
}

void WebNotificationManagerProxy::cancel(WebPageProxy*, const UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.get(pageNotificationID))
        m_provider->cancel(*webNotification);
}
    
void WebNotificationManagerProxy::didDestroyNotification(WebPageProxy*, const UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.take(pageNotificationID)) {
        m_globalNotificationMap.remove(webNotification->notificationID());
        m_provider->didDestroyNotification(*webNotification);
    }
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage)
{
    ASSERT(webPage);
    clearNotifications(webPage, { });
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage, const Vector<UUID>& pageNotificationIDs)
{
    Vector<uint64_t> globalNotificationIDs;
    globalNotificationIDs.reserveInitialCapacity(m_globalNotificationMap.size());

    // We always check page identity.
    // If the set of notification IDs is non-empty, we also check that.
    auto targetPageIdentifier = identifierForPagePointer(webPage);
    bool checkNotificationIDs = !pageNotificationIDs.isEmpty();

    for (auto notification : m_notifications.values()) {
        if (checkNotificationIDs && !pageNotificationIDs.contains(notification->coreNotificationID()))
            continue;
        if (targetPageIdentifier != notification->pageIdentifier())
            continue;

        uint64_t globalNotificationID = notification->notificationID();
        globalNotificationIDs.append(globalNotificationID);
    }

    for (auto it = globalNotificationIDs.begin(), end = globalNotificationIDs.end(); it != end; ++it) {
        auto pageNotification = m_globalNotificationMap.take(*it);
        m_notifications.remove(pageNotification);
    }

    m_provider->clearNotifications(globalNotificationIDs);
}

void WebNotificationManagerProxy::providerDidShowNotification(uint64_t globalNotificationID)
{
    auto it = m_globalNotificationMap.find(globalNotificationID);
    if (it == m_globalNotificationMap.end())
        return;

    auto notification = m_notifications.get(it->value);
    if (!notification) {
        ASSERT_NOT_REACHED();
        return;
    }

    LOG(Notifications, "Provider did show notification (%s)", notification->coreNotificationID().toString().utf8().data());

    auto* connection = notification->sourceConnection();
    if (!connection)
        return;

    connection->send(Messages::WebNotificationManager::DidShowNotification(it->value), 0);
}

static void dispatchDidClickNotification(WebNotification* notification)
{
    LOG(Notifications, "Provider did click notification (%s)", notification->coreNotificationID().toString().utf8().data());

    if (!notification)
        return;

    auto* connection = notification->sourceConnection();
    if (!connection)
        return;

    connection->send(Messages::WebNotificationManager::DidClickNotification(notification->coreNotificationID()), 0);
}

void WebNotificationManagerProxy::providerDidClickNotification(uint64_t globalNotificationID)
{
    auto it = m_globalNotificationMap.find(globalNotificationID);
    if (it == m_globalNotificationMap.end())
        return;

    dispatchDidClickNotification(m_notifications.get(it->value));
}

void WebNotificationManagerProxy::providerDidClickNotification(const UUID& coreNotificationID)
{
    dispatchDidClickNotification(m_notifications.get(coreNotificationID));
}

void WebNotificationManagerProxy::providerDidCloseNotifications(API::Array* globalNotificationIDs)
{
    Vector<RefPtr<WebNotification>> closedNotifications;

    size_t size = globalNotificationIDs->size();
    for (size_t i = 0; i < size; ++i) {
        // The passed array might have uint64_t identifiers or UUID data identifiers.
        // Handle both.

        std::optional<UUID> coreNotificationID;
        auto* intValue = globalNotificationIDs->at<API::UInt64>(i);
        if (intValue) {
            auto it = m_globalNotificationMap.find(intValue->value());
            if (it == m_globalNotificationMap.end())
                continue;

            coreNotificationID = it->value;
        } else {
            auto* dataValue = globalNotificationIDs->at<API::Data>(i);
            if (!dataValue)
                continue;

            auto span = dataValue->dataReference();
            if (span.size() != 16)
                continue;

            coreNotificationID = UUID { Span<const uint8_t, 16> { span.data(), 16 } };
        }

        ASSERT(coreNotificationID);

        auto notification = m_notifications.take(*coreNotificationID);
        if (!notification)
            continue;

        m_globalNotificationMap.remove(notification->notificationID());
        closedNotifications.append(WTFMove(notification));
    }

    for (auto& notification : closedNotifications) {
        if (auto* connection = notification->sourceConnection()) {
            LOG(Notifications, "Provider did close notification (%s)", notification->coreNotificationID().toString().utf8().data());
            Vector<UUID> notificationIDs = { notification->coreNotificationID() };
            connection->send(Messages::WebNotificationManager::DidCloseNotifications(notificationIDs), 0);
        }
    }
}

void WebNotificationManagerProxy::providerDidUpdateNotificationPolicy(const API::SecurityOrigin* origin, bool allowed)
{
    LOG(Notifications, "Provider did update notification policy for origin %s to %i", origin->securityOrigin().toString().utf8().data(), allowed);

    WebProcessPool::sendToAllRemoteWorkerProcesses(Messages::WebNotificationManager::DidUpdateNotificationDecision(origin->securityOrigin().toString(), allowed));

    if (!processPool())
        return;

    processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidUpdateNotificationDecision(origin->securityOrigin().toString(), allowed));
}

void WebNotificationManagerProxy::providerDidRemoveNotificationPolicies(API::Array* origins)
{
    if (!processPool())
        return;

    size_t size = origins->size();
    if (!size)
        return;
    
    Vector<String> originStrings;
    originStrings.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        originStrings.uncheckedAppend(origins->at<API::SecurityOrigin>(i)->securityOrigin().toString());

    WebProcessPool::sendToAllRemoteWorkerProcesses(Messages::WebNotificationManager::DidRemoveNotificationDecisions(originStrings));

    if (!processPool())
        return;
    processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidRemoveNotificationDecisions(originStrings));
}

} // namespace WebKit
