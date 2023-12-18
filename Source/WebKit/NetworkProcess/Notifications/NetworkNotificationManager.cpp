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

#if ENABLE(BUILT_IN_NOTIFICATIONS)

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "Logging.h"
#include "NetworkSession.h"
#include "PushClientConnectionMessages.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushMessage.h"
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

NetworkNotificationManager::NetworkNotificationManager(NetworkSession& networkSession, const String& webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration&& configuration)
    : m_networkSession(networkSession)
{
    if (!m_networkSession.sessionID().isEphemeral() && !webPushMachServiceName.isEmpty()) {
#if PLATFORM(COCOA)
        auto token = m_networkSession.networkProcess().parentProcessConnection()->getAuditToken();
        if (token) {
            Vector<uint8_t> auditTokenData(sizeof(*token));
            memcpy(auditTokenData.data(), &(*token), sizeof(*token));
            configuration.hostAppAuditTokenData = WTFMove(auditTokenData);
        }
#endif

        m_connection = makeUnique<WebPushD::Connection>(webPushMachServiceName.utf8(), *this, WTFMove(configuration));
    }
}

void NetworkNotificationManager::setPushAndNotificationsEnabledForOrigin(const SecurityOriginData& origin, bool enabled, CompletionHandler<void()>&& completionHandler)
{
    if (!m_connection) {
        completionHandler();
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetPushAndNotificationsEnabledForOrigin(origin.toString(), enabled), WTFMove(completionHandler));
}

void NetworkNotificationManager::deletePushAndNotificationRegistration(const SecurityOriginData& origin, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler("No connection to push daemon"_s);
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::DeletePushAndNotificationRegistration(origin.toString()), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPendingPushMessages(CompletionHandler<void(const Vector<WebPushMessage>&)>&& completionHandler)
{
    CompletionHandler<void(Vector<WebPushMessage>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (Vector<WebPushMessage>&& messages) mutable {
        LOG(Push, "Done getting %u push messages", (unsigned)messages.size());
        completionHandler(WTFMove(messages));
    };

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPendingPushMessages(), WTFMove(replyHandler));
}

void NetworkNotificationManager::showNotification(IPC::Connection&, const WebCore::NotificationData&, RefPtr<NotificationResources>&&, CompletionHandler<void()>&& callback)
{
    callback();

//     FIXME: While we don't normally land commented-out code in the tree,
//     this is a nice bookmark for a development milestone; Roundtrip communication with webpushd
//     Will make next development steps obvious.
//
//    CompletionHandler<void(String)>&& completionHandler = [] (String reply) {
//        printf("Got reply: %s\n", reply.utf8().data());
//    };
//
//    sendMessageWithReply<WebPushD::MessageType::EchoTwice>(WTFMove(completionHandler), String("FIXME: Do useful work here"));
}

void NetworkNotificationManager::cancelNotification(const WTF::UUID&)
{
    if (!m_connection)
        return;
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

void NetworkNotificationManager::subscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SubscribeToPushService(WTFMove(scopeURL), WTFMove(applicationServerKey)), WTFMove(completionHandler));
}

void NetworkNotificationManager::unsubscribeFromPushService(URL&& scopeURL, std::optional<PushSubscriptionIdentifier> pushSubscriptionIdentifier, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::UnsubscribeFromPushService(WTFMove(scopeURL), pushSubscriptionIdentifier), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (m_networkSession.sessionID().isEphemeral()) {
        completionHandler(std::optional<WebCore::PushSubscriptionData> { });
        return;
    }

    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushSubscription(WTFMove(scopeURL)), WTFMove(completionHandler));
}

void NetworkNotificationManager::getPushPermissionState(URL&& scopeURL, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (m_networkSession.sessionID().isEphemeral()) {
        completionHandler(static_cast<uint8_t>(PushPermissionState::Denied));
        return;
    }

    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "No connection to push daemon"_s }));
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushPermissionState(WTFMove(scopeURL)), WTFMove(completionHandler));
}

void NetworkNotificationManager::incrementSilentPushCount(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::IncrementSilentPushCount(WTFMove(origin)), WTFMove(completionHandler));
}

void NetworkNotificationManager::removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RemoveAllPushSubscriptions(), WTFMove(completionHandler));
}

void NetworkNotificationManager::removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    m_connection->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RemovePushSubscriptionsForOrigin(WTFMove(origin)), WTFMove(completionHandler));
}

} // namespace WebKit
#endif // ENABLE(BUILT_IN_NOTIFICATIONS)
