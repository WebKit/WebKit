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
#include "APINumber.h"
#include "APISecurityOrigin.h"
#include "Logging.h"
#include "WebNotification.h"
#include "WebNotificationManagerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/NotificationData.h>
#include <WebCore/SecurityOriginData.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {
using namespace WebCore;

ASCIILiteral WebNotificationManagerProxy::supplementName()
{
    return "WebNotificationManagerProxy"_s;
}

Ref<WebNotificationManagerProxy> WebNotificationManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebNotificationManagerProxy(processPool));
}

WebNotificationManagerProxy& WebNotificationManagerProxy::sharedServiceWorkerManager()
{
    ASSERT(isMainRunLoop());
    static NeverDestroyed<Ref<WebNotificationManagerProxy>> sharedManager = adoptRef(*new WebNotificationManagerProxy(nullptr));
    return sharedManager->get();
}

Ref<WebNotificationManagerProxy> WebNotificationManagerProxy::protectedSharedServiceWorkerManager()
{
    return sharedServiceWorkerManager();
}

WebNotificationManagerProxy::WebNotificationManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
    , m_provider(makeUnique<API::NotificationProvider>())
{
}

WebNotificationManagerProxy::~WebNotificationManagerProxy() = default;

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

static std::optional<WebPageProxyIdentifier> identifierForPagePointer(WebPageProxy* webPage)
{
    return webPage ? std::optional { webPage->identifier() } : std::nullopt;
}

void WebNotificationManagerProxy::show(WebPageProxy* webPage, IPC::Connection& connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    LOG(Notifications, "WebPageProxy (%p) asking to show notification (%s)", webPage, notificationData.notificationID.toString().utf8().data());

    auto notification = WebNotification::createNonPersistent(notificationData, identifierForPagePointer(webPage), connection);
    showImpl(webPage, WTFMove(notification), WTFMove(notificationResources));
}

bool WebNotificationManagerProxy::showPersistent(const WebsiteDataStore& dataStore, IPC::Connection* connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    LOG(Notifications, "WebsiteDataStore (%p) asking to show notification (%s)", &dataStore, notificationData.notificationID.toString().utf8().data());

    auto notification = WebNotification::createPersistent(notificationData, dataStore.configuration().identifier(), connection);
    return showImpl(nullptr, WTFMove(notification), WTFMove(notificationResources));
}

bool WebNotificationManagerProxy::showImpl(WebPageProxy* webPage, Ref<WebNotification>&& notification, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    m_globalNotificationMap.set(notification->identifier(), notification->coreNotificationID());
    m_notifications.set(notification->coreNotificationID(), notification);
    return m_provider->show(webPage, notification.get(), WTFMove(notificationResources));
}

void WebNotificationManagerProxy::cancel(WebPageProxy* page, const WTF::UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.get(pageNotificationID)) {
        m_provider->cancel(*webNotification);
        didDestroyNotification(page, pageNotificationID);
    }
}
    
void WebNotificationManagerProxy::didDestroyNotification(WebPageProxy*, const WTF::UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.take(pageNotificationID)) {
        m_globalNotificationMap.remove(webNotification->identifier());
        m_provider->didDestroyNotification(*webNotification);
    }
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage)
{
    ASSERT(webPage);
    clearNotifications(webPage, { });
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage, const Vector<WTF::UUID>& pageNotificationIDs)
{
    Vector<WebNotificationIdentifier> globalNotificationIDs;
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

        auto globalNotificationID = notification->identifier();
        globalNotificationIDs.append(globalNotificationID);
    }

    for (auto it = globalNotificationIDs.begin(), end = globalNotificationIDs.end(); it != end; ++it) {
        auto pageNotification = m_globalNotificationMap.take(*it);
        m_notifications.remove(pageNotification);
    }

    m_provider->clearNotifications(globalNotificationIDs);
}

void WebNotificationManagerProxy::providerDidShowNotification(WebNotificationIdentifier globalNotificationID)
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

    auto connection = notification->sourceConnection();
    if (!connection)
        return;

    connection->send(Messages::WebNotificationManager::DidShowNotification(it->value), 0);
}

static void dispatchDidClickNotification(WebNotification* notification)
{
    LOG(Notifications, "Provider did click notification (%s)", notification->coreNotificationID().toString().utf8().data());

    if (!notification)
        return;

    if (notification->isPersistentNotification()) {
        if (auto* dataStore = WebsiteDataStore::existingDataStoreForSessionID(notification->sessionID()))
            dataStore->networkProcess().processNotificationEvent(notification->data(), NotificationEventType::Click, [](bool) { });
        else
            RELEASE_LOG_ERROR(Notifications, "WebsiteDataStore not found from sessionID %" PRIu64 ", dropping notification click", notification->sessionID().toUInt64());
        return;
    }

    if (auto connection = notification->sourceConnection())
        connection->send(Messages::WebNotificationManager::DidClickNotification(notification->coreNotificationID()), 0);
}

void WebNotificationManagerProxy::providerDidClickNotification(WebNotificationIdentifier globalNotificationID)
{
    auto it = m_globalNotificationMap.find(globalNotificationID);
    if (it == m_globalNotificationMap.end())
        return;

    dispatchDidClickNotification(m_notifications.get(it->value));
}

void WebNotificationManagerProxy::providerDidClickNotification(const WTF::UUID& coreNotificationID)
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

        std::optional<WTF::UUID> coreNotificationID;
        RefPtr intValue = globalNotificationIDs->at<API::UInt64>(i);
        if (intValue) {
            auto it = m_globalNotificationMap.find(WebNotificationIdentifier { intValue->value() });
            if (it == m_globalNotificationMap.end())
                continue;

            coreNotificationID = it->value;
        } else {
            RefPtr dataValue = globalNotificationIDs->at<API::Data>(i);
            if (!dataValue)
                continue;

            auto span = dataValue->span();
            if (span.size() != 16)
                continue;

            coreNotificationID = WTF::UUID { std::span<const uint8_t, 16> { span } };
        }

        ASSERT(coreNotificationID);

        auto notification = m_notifications.take(*coreNotificationID);
        if (!notification)
            continue;

        if (notification->isPersistentNotification()) {
            if (auto* dataStore = WebsiteDataStore::existingDataStoreForSessionID(notification->sessionID()))
                dataStore->networkProcess().processNotificationEvent(notification->data(), NotificationEventType::Close, [](bool) { });
            else
                RELEASE_LOG_ERROR(Notifications, "WebsiteDataStore not found from sessionID %" PRIu64 ", dropping notification close", notification->sessionID().toUInt64());
            return;
        }

        m_globalNotificationMap.remove(notification->identifier());
        closedNotifications.append(WTFMove(notification));
    }

    for (auto& notification : closedNotifications) {
        if (auto connection = notification->sourceConnection()) {
            LOG(Notifications, "Provider did close notification (%s)", notification->coreNotificationID().toString().utf8().data());
            Vector<WTF::UUID> notificationIDs = { notification->coreNotificationID() };
            connection->send(Messages::WebNotificationManager::DidCloseNotifications(notificationIDs), 0);
        }
    }
}

static void setPushesAndNotificationsEnabledForOrigin(const WebCore::SecurityOriginData& origin, bool enabled)
{
    WebsiteDataStore::forEachWebsiteDataStore([&origin, enabled](WebsiteDataStore& dataStore) {
        if (dataStore.isPersistent())
            dataStore.networkProcess().setPushAndNotificationsEnabledForOrigin(dataStore.sessionID(), origin, enabled, []() { });
    });
}

static void removePushSubscriptionsForOrigins(const Vector<WebCore::SecurityOriginData>& origins)
{
    WebsiteDataStore::forEachWebsiteDataStore([&origins](WebsiteDataStore& dataStore) {
        if (dataStore.isPersistent()) {
            for (auto& origin : origins)
                dataStore.networkProcess().removePushSubscriptionsForOrigin(dataStore.sessionID(), origin, [originString = origin.toString()](auto&&) { });
        }
    });
}

static Vector<WebCore::SecurityOriginData> apiArrayToSecurityOrigins(API::Array* origins)
{
    if (!origins)
        return { };

    return Vector<WebCore::SecurityOriginData>(origins->size(), [origins](size_t i) {
        return origins->at<API::SecurityOrigin>(i)->securityOrigin();
    });
}

static Vector<String> apiArrayToSecurityOriginStrings(API::Array* origins)
{
    if (!origins)
        return { };

    return Vector<String>(origins->size(), [origins](size_t i) {
        return origins->at<API::SecurityOrigin>(i)->securityOrigin().toString();
    });
}

void WebNotificationManagerProxy::providerDidUpdateNotificationPolicy(const API::SecurityOrigin* origin, bool enabled)
{
    RELEASE_LOG(Notifications, "Provider did update notification policy for origin %" SENSITIVE_LOG_STRING " to %d", origin->securityOrigin().toString().utf8().data(), enabled);

    auto originString = origin->securityOrigin().toString();
    if (originString.isEmpty())
        return;

    if (this == &sharedServiceWorkerManager()) {
        setPushesAndNotificationsEnabledForOrigin(origin->securityOrigin(), enabled);
        WebProcessPool::sendToAllRemoteWorkerProcesses(Messages::WebNotificationManager::DidUpdateNotificationDecision(originString, enabled));
        return;
    }

    if (processPool())
        processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidUpdateNotificationDecision(originString, enabled));
}

void WebNotificationManagerProxy::providerDidRemoveNotificationPolicies(API::Array* origins)
{
    size_t size = origins->size();
    if (!size)
        return;

    if (this == &sharedServiceWorkerManager()) {
        removePushSubscriptionsForOrigins(apiArrayToSecurityOrigins(origins));
        WebProcessPool::sendToAllRemoteWorkerProcesses(Messages::WebNotificationManager::DidRemoveNotificationDecisions(apiArrayToSecurityOriginStrings(origins)));
        return;
    }

    if (processPool())
        processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidRemoveNotificationDecisions(apiArrayToSecurityOriginStrings(origins)));
}

void WebNotificationManagerProxy::getNotifications(const URL& url, const String& tag, PAL::SessionID sessionID, CompletionHandler<void(Vector<NotificationData>&&)>&& callback)
{
    Vector<WebNotification*> notifications;
    for (auto& notification : m_notifications.values()) {
        auto& data = notification->data();
        if (data.serviceWorkerRegistrationURL != url || data.sourceSession != sessionID)
            continue;
        if (!tag.isEmpty() && data.tag != tag)
            continue;
        notifications.append(notification.ptr());
    }
    // Let's sort as per https://notifications.spec.whatwg.org/#dom-serviceworkerregistration-getnotifications.
    std::sort(notifications.begin(), notifications.end(), [](auto& a, auto& b) {
        if (a->data().creationTime < b->data().creationTime)
            return true;
        return a->data().creationTime == b->data().creationTime && a->identifier() < b->identifier();
    });
    callback(map(notifications, [](auto& notification) { return notification->data(); }));
}

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
