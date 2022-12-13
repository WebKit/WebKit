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
            Vector<uint8_t> auditTokenData;
            auditTokenData.resize(sizeof(*token));
            memcpy(auditTokenData.data(), &(*token), sizeof(*token));
            configuration.hostAppAuditTokenData = WTFMove(auditTokenData);
        }
#endif

        m_connection = makeUnique<WebPushD::Connection>(webPushMachServiceName.utf8(), *this, WTFMove(configuration));
    }
}

void NetworkNotificationManager::requestSystemNotificationPermission(const String& originString, CompletionHandler<void(bool)>&& completionHandler)
{
    LOG(Push, "Network process passing permission request to webpushd");
    sendMessageWithReply<WebPushD::MessageType::RequestSystemNotificationPermission>(WTFMove(completionHandler), originString);
}

void NetworkNotificationManager::setPushAndNotificationsEnabledForOrigin(const SecurityOriginData& origin, bool enabled, CompletionHandler<void()>&& completionHandler)
{
    if (!m_connection) {
        completionHandler();
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::SetPushAndNotificationsEnabledForOrigin>(WTFMove(completionHandler), origin.toString(), enabled);
}

void NetworkNotificationManager::deletePushAndNotificationRegistration(const SecurityOriginData& origin, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler("No connection to push daemon"_s);
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::DeletePushAndNotificationRegistration>(WTFMove(completionHandler), origin.toString());
}

void NetworkNotificationManager::getOriginsWithPushAndNotificationPermissions(CompletionHandler<void(const Vector<SecurityOriginData>&)>&& completionHandler)
{
    CompletionHandler<void(Vector<String>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (Vector<String> originStrings) mutable {
        auto origins = originStrings.map([](auto& originString) {
            return SecurityOriginData::fromURL({ { }, originString });
        });
        completionHandler(WTFMove(origins));
    };

    sendMessageWithReply<WebPushD::MessageType::GetOriginsWithPushAndNotificationPermissions>(WTFMove(replyHandler));
}

void NetworkNotificationManager::getPendingPushMessages(CompletionHandler<void(const Vector<WebPushMessage>&)>&& completionHandler)
{
    CompletionHandler<void(Vector<WebPushMessage>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (Vector<WebPushMessage>&& messages) mutable {
        LOG(Push, "Done getting %u push messages", (unsigned)messages.size());
        completionHandler(WTFMove(messages));
    };

    sendMessageWithReply<WebPushD::MessageType::GetPendingPushMessages>(WTFMove(replyHandler));
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

void NetworkNotificationManager::cancelNotification(const UUID&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::clearNotifications(const Vector<UUID>&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::didDestroyNotification(const UUID&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::subscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { AbortError, "No connection to push daemon"_s }));
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::SubscribeToPushService>(WTFMove(completionHandler), WTFMove(scopeURL), WTFMove(applicationServerKey));
}

void NetworkNotificationManager::unsubscribeFromPushService(URL&& scopeURL, std::optional<PushSubscriptionIdentifier> pushSubscriptionIdentifier, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { AbortError, "No connection to push daemon"_s }));
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::UnsubscribeFromPushService>(WTFMove(completionHandler), WTFMove(scopeURL), pushSubscriptionIdentifier);
}

void NetworkNotificationManager::getPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (m_networkSession.sessionID().isEphemeral()) {
        completionHandler(std::optional<WebCore::PushSubscriptionData> { });
        return;
    }

    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { AbortError, "No connection to push daemon"_s }));
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::GetPushSubscription>(WTFMove(completionHandler), WTFMove(scopeURL));
}

void NetworkNotificationManager::getPushPermissionState(URL&& scopeURL, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (m_networkSession.sessionID().isEphemeral()) {
        completionHandler(static_cast<uint8_t>(PushPermissionState::Denied));
        return;
    }

    if (!m_connection) {
        completionHandler(makeUnexpected(ExceptionData { AbortError, "No connection to push daemon"_s }));
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::GetPushPermissionState>(WTFMove(completionHandler), WTFMove(scopeURL));
}

void NetworkNotificationManager::incrementSilentPushCount(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::IncrementSilentPushCount>(WTFMove(completionHandler), WTFMove(origin));
}

void NetworkNotificationManager::removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::RemoveAllPushSubscriptions>(WTFMove(completionHandler));
}

void NetworkNotificationManager::removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    if (!m_connection) {
        completionHandler(0);
        return;
    }

    sendMessageWithReply<WebPushD::MessageType::RemovePushSubscriptionsForOrigin>(WTFMove(completionHandler), WTFMove(origin));
}

template<WebPushD::MessageType messageType, typename... Args>
void NetworkNotificationManager::sendMessage(Args&&... args) const
{
    RELEASE_ASSERT(m_connection);

    Daemon::Encoder encoder;
    encoder.encode(std::forward<Args>(args)...);
    m_connection->send(messageType, encoder.takeBuffer());
}

template<typename... Args> struct ReplyCaller;
template<> struct ReplyCaller<> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void()>&& completionHandler)
    {
        completionHandler();
    }
};

template<> struct ReplyCaller<String> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(String&&)>&& completionHandler)
    {
        std::optional<String> string;
        decoder >> string;
        if (!string)
            return completionHandler({ });
        completionHandler(WTFMove(*string));
    }
};

template<> struct ReplyCaller<const String&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(const String&)>&& completionHandler)
    {
        std::optional<String> string;
        decoder >> string;
        if (!string)
            return completionHandler({ });
        completionHandler(WTFMove(*string));
    }
};

template<> struct ReplyCaller<bool> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(bool)>&& completionHandler)
    {
        std::optional<bool> boolean;
        decoder >> boolean;
        if (!boolean)
            return completionHandler(false);
        completionHandler(*boolean);
    }
};

template<> struct ReplyCaller<unsigned> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(bool)>&& completionHandler)
    {
        std::optional<int> value;
        decoder >> value;
        if (!value)
            return completionHandler(0);
        completionHandler(*value);
    }
};

template<> struct ReplyCaller<Vector<String>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
    {
        std::optional<Vector<String>> strings;
        decoder >> strings;
        if (!strings)
            return completionHandler({ });
        completionHandler(WTFMove(*strings));
    }
};

template<> struct ReplyCaller<Vector<WebPushMessage>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Vector<WebPushMessage>&&)>&& completionHandler)
    {
        std::optional<Vector<WebPushMessage>> messages;
        decoder >> messages;
        if (!messages)
            return completionHandler({ });
        completionHandler(WTFMove(*messages));
    }
};

template<> struct ReplyCaller<Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&& completionHandler)
    {
        std::optional<Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>> data;
        decoder >> data;

        if (!data)
            completionHandler(makeUnexpected(ExceptionData { AbortError, "Couldn't decode message"_s }));
        else
            completionHandler(WTFMove(*data));
    }
};

template<> struct ReplyCaller<Expected<bool, WebCore::ExceptionData>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
    {
        std::optional<Expected<bool, WebCore::ExceptionData>> data;
        decoder >> data;

        if (!data)
            completionHandler(makeUnexpected(ExceptionData { AbortError, "Couldn't decode message"_s }));
        else
            completionHandler(WTFMove(*data));
    }
};

template<> struct ReplyCaller<Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&& completionHandler)
    {
        std::optional<Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>> data;
        decoder >> data;

        if (!data)
            completionHandler(makeUnexpected(ExceptionData { AbortError, "Couldn't decode message"_s }));
        else
            completionHandler(WTFMove(*data));
    }
};

template<> struct ReplyCaller<Expected<uint8_t, WebCore::ExceptionData>&&> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&& completionHandler)
    {
        std::optional<Expected<uint8_t, WebCore::ExceptionData>> data;
        decoder >> data;

        if (!data)
            completionHandler(makeUnexpected(ExceptionData { AbortError, "Couldn't decode message"_s }));
        else
            completionHandler(WTFMove(*data));
    }
};

template<WebPushD::MessageType messageType, typename... Args, typename... ReplyArgs>
void NetworkNotificationManager::sendMessageWithReply(CompletionHandler<void(ReplyArgs...)>&& completionHandler, Args&&... args) const
{
    RELEASE_ASSERT(m_connection);

    Daemon::Encoder encoder;
    encoder.encode(std::forward<Args>(args)...);
    m_connection->sendWithReply(messageType, encoder.takeBuffer(), [completionHandler = WTFMove(completionHandler)] (auto replyBuffer) mutable {
        Daemon::Decoder decoder(WTFMove(replyBuffer));
        ReplyCaller<ReplyArgs...>::callReply(WTFMove(decoder), WTFMove(completionHandler));
    });
}

} // namespace WebKit
#endif // ENABLE(BUILT_IN_NOTIFICATIONS)
