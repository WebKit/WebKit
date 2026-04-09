/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebMessagePortChannelProvider.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <WebCore/MessagePort.h>
#include <WebCore/MessagePortIdentifier.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebMessagePortChannelProvider);

WebMessagePortChannelProvider& WebMessagePortChannelProvider::singleton()
{
    static WebMessagePortChannelProvider* provider = new WebMessagePortChannelProvider;
    return *provider;
}

WebMessagePortChannelProvider::WebMessagePortChannelProvider() = default;

WebMessagePortChannelProvider::~WebMessagePortChannelProvider()
{
    ASSERT_NOT_REACHED();
}

static inline IPC::Connection& networkProcessConnection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebMessagePortChannelProvider::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2, bool siteIsolationEnabled)
{
    if (!siteIsolationEnabled) {
        ASSERT(!m_inProcessPortMessages.contains(port1));
        ASSERT(!m_inProcessPortMessages.contains(port2));
        m_inProcessPortMessages.add(port1, Vector<MessageWithMessagePorts> { });
        m_inProcessPortMessages.add(port2, Vector<MessageWithMessagePorts> { });
    }

    protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::CreateNewMessagePortChannel { port1, port2 }, 0);
}

void WebMessagePortChannelProvider::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    m_inProcessPortMessages.add(local, Vector<MessageWithMessagePorts> { });
    m_portsPendingSync.add(local);

    protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::EntangleLocalPortInThisProcessToRemote { local, remote }, 0);
}

void WebMessagePortChannelProvider::messagePortDisentangled(const MessagePortIdentifier& port)
{
    protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::MessagePortDisentangled { port }, 0);
}

void WebMessagePortChannelProvider::messagePortSentToRemote(const WebCore::MessagePortIdentifier& port)
{
    auto inProcessPortMessages = m_inProcessPortMessages.take(port);
    for (auto& message : inProcessPortMessages)
        postMessageToRemote(WTF::move(message), port);
}

void WebMessagePortChannelProvider::messagePortClosed(const MessagePortIdentifier& port)
{
    m_inProcessPortMessages.remove(port);
    m_portsPendingSync.remove(port);
    protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::MessagePortClosed { port }, 0);
}

void WebMessagePortChannelProvider::takeAllMessagesForPort(const MessagePortIdentifier& port, CompletionHandler<void(Vector<MessageWithMessagePorts>&&, CompletionHandler<void()>&&)>&& completionHandler)
{
    // Fast path: port is local and not pending sync — zero IPC.
    if (!m_portsPendingSync.contains(port)) {
        auto iterator = m_inProcessPortMessages.find(port);
        if (iterator != m_inProcessPortMessages.end()) {
            auto messages = std::exchange(iterator->value, { });
            completionHandler(WTF::move(messages), [] { });
            return;
        }
    }

    // IPC path: pending sync (need to fetch pre-transfer messages) or port not in local map.
    protect(networkProcessConnection())->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::TakeAllMessagesForPort { port }, [completionHandler = WTF::move(completionHandler), port](Vector<WebCore::MessageWithMessagePorts>&& messages, std::optional<MessageBatchIdentifier> messageBatchIdentifier) mutable {
        if (!messageBatchIdentifier)
            return completionHandler({ }, [] { });

        auto& provider = WebMessagePortChannelProvider::singleton();
        provider.m_portsPendingSync.remove(port);

        auto iterator = provider.m_inProcessPortMessages.find(port);
        if (iterator != provider.m_inProcessPortMessages.end())
            messages.appendVector(std::exchange(iterator->value, { }));

        completionHandler(WTF::move(messages), [messageBatchIdentifier] {
            protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::DidDeliverMessagePortMessages { *messageBatchIdentifier }, 0);
        });
    }, 0);
}

void WebMessagePortChannelProvider::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget)
{
    auto iterator = m_inProcessPortMessages.find(remoteTarget);
    if (iterator != m_inProcessPortMessages.end()) {
        iterator->value.append(WTF::move(message));
        WebProcess::singleton().messagesAvailableForPort(remoteTarget);
        return;
    }

    for (auto& port : message.transferredPorts)
        messagePortSentToRemote(port.first);

    protect(networkProcessConnection())->send(Messages::NetworkConnectionToWebProcess::PostMessageToRemote { message, remoteTarget }, 0);
}

void WebMessagePortChannelProvider::returnUndeliveredMessages(const MessagePortIdentifier& port, Vector<MessageWithMessagePorts>&& messages)
{
    if (messages.isEmpty())
        return;

    auto& buffer = m_inProcessPortMessages.ensure(port, [] {
        return Vector<MessageWithMessagePorts> { };
    }).iterator->value;

    // Prepend: these are older messages that should be delivered before any new ones.
    messages.appendVector(WTF::move(buffer));
    buffer = WTF::move(messages);
}

} // namespace WebKit
