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

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebBroadcastChannelRegistryMessages.h"
#include <WebCore/MessageWithMessagePorts.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

static bool isValidClientOrigin(const WebCore::ClientOrigin& clientOrigin)
{
    return !clientOrigin.topOrigin.isNull() && !clientOrigin.clientOrigin.isNull();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkBroadcastChannelRegistry);

Ref<NetworkBroadcastChannelRegistry> NetworkBroadcastChannelRegistry::create(NetworkProcess& networkProcess)
{
    return adoptRef(*new NetworkBroadcastChannelRegistry(networkProcess));
}

NetworkBroadcastChannelRegistry::NetworkBroadcastChannelRegistry(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
}

NetworkBroadcastChannelRegistry::~NetworkBroadcastChannelRegistry() = default;

void NetworkBroadcastChannelRegistry::registerChannel(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const String& name)
{
    MESSAGE_CHECK(isValidClientOrigin(origin), connection);

    auto& channelsForOrigin = m_broadcastChannels.ensure(origin, [] { return NameToConnectionIdentifiersMap { }; }).iterator->value;
    auto& connectionIdentifiersForName = channelsForOrigin.ensure(name, [] { return Vector<IPC::Connection::UniqueID> { }; }).iterator->value;
    ASSERT(!connectionIdentifiersForName.contains(connection.uniqueID()));
    connectionIdentifiersForName.append(connection.uniqueID());
}

void NetworkBroadcastChannelRegistry::unregisterChannel(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const String& name)
{
    MESSAGE_CHECK(isValidClientOrigin(origin), connection);

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
    MESSAGE_CHECK_COMPLETION(isValidClientOrigin(origin), connection, completionHandler());

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

std::optional<SharedPreferencesForWebProcess> NetworkBroadcastChannelRegistry::sharedPreferencesForWebProcess(const IPC::Connection& connection) const
{
    RefPtr webProcessConnection = m_networkProcess->protectedWebProcessConnection(connection);
    if (!webProcessConnection)
        return std::nullopt;
    return webProcessConnection->sharedPreferencesForWebProcess();
}

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION

} // namespace WebKit
