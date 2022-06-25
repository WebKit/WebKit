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
#include "NetworkBroadcastChannelRegistry.h"

#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebBroadcastChannelRegistryMessages.h"
#include <WebCore/MessageWithMessagePorts.h>
#include <wtf/CallbackAggregator.h>

namespace WebKit {

#define REGISTRY_MESSAGE_CHECK(assertion) REGISTRY_MESSAGE_CHECK_COMPLETION(assertion, (void)0)
#define REGISTRY_MESSAGE_CHECK_COMPLETION(assertion, completion) do { \
    ASSERT(assertion); \
    if (UNLIKELY(!(assertion))) { \
        if (auto webProcessIdentifier = m_networkProcess->webProcessIdentifierForConnection(connection)) \
            m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(webProcessIdentifier), 0); \
        { completion; } \
        return; \
    } \
} while (0)

static bool isValidClientOrigin(const WebCore::ClientOrigin& clientOrigin)
{
    return !clientOrigin.topOrigin.isEmpty() && !clientOrigin.clientOrigin.isEmpty();
}

NetworkBroadcastChannelRegistry::NetworkBroadcastChannelRegistry(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
}

void NetworkBroadcastChannelRegistry::registerChannel(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const String& name)
{
    REGISTRY_MESSAGE_CHECK(isValidClientOrigin(origin));

    auto& channelsForOrigin = m_broadcastChannels.ensure(origin, [] { return NameToConnectionIdentifiersMap { }; }).iterator->value;
    auto& connectionIdentifiersForName = channelsForOrigin.ensure(name, [] { return Vector<IPC::Connection::UniqueID> { }; }).iterator->value;
    ASSERT(!connectionIdentifiersForName.contains(connection.uniqueID()));
    connectionIdentifiersForName.append(connection.uniqueID());
}

void NetworkBroadcastChannelRegistry::unregisterChannel(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const String& name)
{
    REGISTRY_MESSAGE_CHECK(isValidClientOrigin(origin));

    auto channelsForOriginIterator = m_broadcastChannels.find(origin);
    ASSERT(channelsForOriginIterator != m_broadcastChannels.end());
    if (channelsForOriginIterator == m_broadcastChannels.end())
        return;
    auto connectionIdentifiersForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(connectionIdentifiersForNameIterator != channelsForOriginIterator->value.end());
    if (connectionIdentifiersForNameIterator == channelsForOriginIterator->value.end())
        return;

    ASSERT(connectionIdentifiersForNameIterator->value.contains(connection.uniqueID()));
    connectionIdentifiersForNameIterator->value.removeFirst(connection.uniqueID());
}

void NetworkBroadcastChannelRegistry::postMessage(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const String& name, WebCore::MessageWithMessagePorts&& message, CompletionHandler<void()>&& completionHandler)
{
    REGISTRY_MESSAGE_CHECK_COMPLETION(isValidClientOrigin(origin), completionHandler());

    auto channelsForOriginIterator = m_broadcastChannels.find(origin);
    ASSERT(channelsForOriginIterator != m_broadcastChannels.end());
    if (channelsForOriginIterator == m_broadcastChannels.end())
        return completionHandler();
    auto connectionIdentifiersForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(connectionIdentifiersForNameIterator != channelsForOriginIterator->value.end());
    if (connectionIdentifiersForNameIterator == channelsForOriginIterator->value.end())
        return completionHandler();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& connectionID : connectionIdentifiersForNameIterator->value) {
        // Only dispatch the post the messages to BroadcastChannels outside the source process.
        if (connectionID == connection.uniqueID())
            continue;

        RefPtr connection = IPC::Connection::connection(connectionID);
        if (!connection)
            continue;

        connection->sendWithAsyncReply(Messages::WebBroadcastChannelRegistry::PostMessageToRemote(origin, name, message), [callbackAggregator] { }, 0);
    }
}

void NetworkBroadcastChannelRegistry::removeConnection(IPC::Connection& connection)
{
    Vector<WebCore::ClientOrigin> originsToRemove;
    for (auto& entry : m_broadcastChannels) {
        Vector<String> namesToRemove;
        for (auto& innerEntry : entry.value) {
            innerEntry.value.removeFirst(connection.uniqueID());
            if (innerEntry.value.isEmpty())
                namesToRemove.append(innerEntry.key);
        }
        for (auto& nameToRemove : namesToRemove)
            entry.value.remove(nameToRemove);
        if (entry.value.isEmpty())
            originsToRemove.append(entry.key);
    }
    for (auto& originToRemove : originsToRemove)
        m_broadcastChannels.remove(originToRemove);
}

#undef REGISTRY_MESSAGE_CHECK
#undef REGISTRY_MESSAGE_CHECK_COMPLETION

} // namespace WebKit
