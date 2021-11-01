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
#include "NetworkSession.h"
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

NetworkNotificationManager::NetworkNotificationManager(NetworkSession& networkSession, const String& webPushMachServiceName)
    : m_networkSession(networkSession)
{
    if (!m_networkSession.sessionID().isEphemeral() && !webPushMachServiceName.isEmpty())
        m_connection = makeUnique<WebPushD::Connection>(webPushMachServiceName.utf8(), *this);
}

void NetworkNotificationManager::requestSystemNotificationPermission(const String& originString, CompletionHandler<void(bool)>&& completionHandler)
{
    sendMessageWithReply<WebPushD::MessageType::RequestSystemNotificationPermission>(WTFMove(completionHandler), originString);
}

void NetworkNotificationManager::deletePushAndNotificationRegistration(const SecurityOriginData& origin, CompletionHandler<void(const String&)>&& completionHandler)
{
    sendMessageWithReply<WebPushD::MessageType::DeletePushAndNotificationRegistration>(WTFMove(completionHandler), origin.toString());
}

void NetworkNotificationManager::getOriginsWithPushAndNotificationPermissions(CompletionHandler<void(const Vector<SecurityOriginData>&)>&& completionHandler)
{
    CompletionHandler<void(Vector<String>&&)> replyHandler = [completionHandler = WTFMove(completionHandler)] (Vector<String> originStrings) mutable {
        Vector<SecurityOriginData> origins;
        for (auto& originString : originStrings)
            origins.append(SecurityOriginData::fromURL({ { }, originString }));
        completionHandler(WTFMove(origins));
    };

    sendMessageWithReply<WebPushD::MessageType::GetOriginsWithPushAndNotificationPermissions>(WTFMove(replyHandler));
}

void NetworkNotificationManager::showNotification(const String&, const String&, const String&, const String&, const String&, WebCore::NotificationDirection, const String&, uint64_t)
{
    if (!m_connection)
        return;

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

void NetworkNotificationManager::cancelNotification(uint64_t)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::clearNotifications(const Vector<uint64_t>&)
{
    if (!m_connection)
        return;
}

void NetworkNotificationManager::didDestroyNotification(uint64_t)
{
    if (!m_connection)
        return;
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
