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
#include "NetworkNotificationManager.h"

#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "PushClientConnectionMessages.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushMessage.h"
#include <WebCore/NotificationData.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkNotificationManager);

Ref<NetworkNotificationManager> NetworkNotificationManager::create(const String& webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration&& configuration, NetworkProcess& networkProcess)
{
    return adoptRef(*new NetworkNotificationManager(webPushMachServiceName, WTFMove(configuration), networkProcess));
}

NetworkNotificationManager::NetworkNotificationManager(const String& webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration&& configuration, NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
    if (!webPushMachServiceName.isEmpty())
        m_connection = WebPushD::Connection::create(webPushMachServiceName.utf8(), WTFMove(configuration));
}

void NetworkNotificationManager::setPushAndNotificationsEnabledForOrigin(const SecurityOriginData& origin, bool enabled, CompletionHandler<void()>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler();
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetPushAndNotificationsEnabledForOrigin(origin.toString(), enabled), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPendingPushMessage(CompletionHandler<void(const std::optional<WebPushMessage>&)>&& completionHandler)
{
    CompletionHandler<void(std::optional<WebPushMessage>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (auto&& message) mutable {
        RELEASE_LOG(Push, "Done getting %u push messages", message ? 1 : 0);
        completionHandler(WTFMove(message));
    };

    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPendingPushMessage(), WTFMove(replyHandler));
}

void NetworkNotificationManager::getPendingPushMessages(CompletionHandler<void(const Vector<WebPushMessage>&)>&& completionHandler)
{
    CompletionHandler<void(Vector<WebPushMessage>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (Vector<WebPushMessage>&& messages) mutable {
        LOG(Push, "Done getting %u push messages", (unsigned)messages.size());
        completionHandler(WTFMove(messages));
    };

    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPendingPushMessages(), WTFMove(replyHandler));
}

void NetworkNotificationManager::showNotification(const WebCore::NotificationData& notification, RefPtr<NotificationResources>&& notificationResources, CompletionHandler<void()>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler();
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::ShowNotification { notification, notificationResources }, WTFMove(completionHandler));
}

void NetworkNotificationManager::showNotification(IPC::Connection&, const WebCore::NotificationData& notification, RefPtr<NotificationResources>&& notificationResources, CompletionHandler<void()>&& completionHandler)
{
    showNotification(notification, WTFMove(notificationResources), WTFMove(completionHandler));
}

void NetworkNotificationManager::getNotifications(const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No active connection to webpushd"_s }));
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetNotifications { registrationURL, tag }, WTFMove(completionHandler));
}

void NetworkNotificationManager::cancelNotification(WebCore::SecurityOriginData&& origin, const WTF::UUID& notificationID)
{
    RefPtr connection = m_connection;
    if (!connection)
        return;

    connection->sendWithoutUsingIPCConnection(Messages::PushClientConnection::CancelNotification { WTFMove(origin), notificationID });
}

void NetworkNotificationManager::clearNotifications(const Vector<WTF::UUID>&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::didDestroyNotification(const WTF::UUID&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::requestPermission(WebCore::SecurityOriginData&& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        RELEASE_LOG_ERROR(Push, "requestPermission failed: no active connection to webpushd");
        return completionHandler(false);
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RequestPushPermission { WTFMove(origin) }, WTFMove(completionHandler));
}

void NetworkNotificationManager::setAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    RefPtr connection = m_connection;
    if (!connection)
        return;

    connection->sendWithoutUsingIPCConnection(Messages::PushClientConnection::SetAppBadge { origin, badge });
}

void NetworkNotificationManager::subscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SubscribeToPushService(WTFMove(scopeURL), WTFMove(applicationServerKey)), WTFMove(completionHandler));
}

void NetworkNotificationManager::unsubscribeFromPushService(URL&& scopeURL, std::optional<PushSubscriptionIdentifier> pushSubscriptionIdentifier, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::UnsubscribeFromPushService(WTFMove(scopeURL), pushSubscriptionIdentifier), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(std::optional<WebCore::PushSubscriptionData> { });
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushSubscription(WTFMove(scopeURL)), WTFMove(completionHandler));
}

void NetworkNotificationManager::incrementSilentPushCount(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(0);
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::IncrementSilentPushCount(WTFMove(origin)), WTFMove(completionHandler));
}

void NetworkNotificationManager::removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(0);
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RemoveAllPushSubscriptions(), WTFMove(completionHandler));
}

void NetworkNotificationManager::removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(0);
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RemovePushSubscriptionsForOrigin(WTFMove(origin)), WTFMove(completionHandler));
}

void NetworkNotificationManager::getAppBadgeForTesting(CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection) {
        completionHandler(std::nullopt);
        return;
    }

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetAppBadgeForTesting(), WTFMove(completionHandler));
}

void NetworkNotificationManager::setServiceWorkerIsBeingInspected(const URL& scopeURL, bool isInspected)
{
    RefPtr connection = m_connection;
    if (!connection)
        return;

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetServiceWorkerIsBeingInspected { scopeURL, isInspected }, []() { });
}

static void getPushPermissionStateImpl(WebPushD::Connection* connection, WebCore::SecurityOriginData&& origin, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    if (!connection)
        return completionHandler(WebCore::PushPermissionState::Denied);

    connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushPermissionState(WTFMove(origin)), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPermissionState(WebCore::SecurityOriginData&& origin, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    getPushPermissionStateImpl(protectedConnection().get(), WTFMove(origin), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPermissionStateSync(WebCore::SecurityOriginData&& origin, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    getPushPermissionStateImpl(protectedConnection().get(), WTFMove(origin), WTFMove(completionHandler));
}

std::optional<SharedPreferencesForWebProcess> NetworkNotificationManager::sharedPreferencesForWebProcess(const IPC::Connection& connection) const
{
    RefPtr webProcessConnection = m_networkProcess->protectedWebProcessConnection(connection);
    if (!webProcessConnection)
        return std::nullopt;
    return webProcessConnection->sharedPreferencesForWebProcess();
}

RefPtr<WebPushD::Connection> NetworkNotificationManager::protectedConnection() const
{
    return m_connection;
}

} // namespace WebKit
#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
