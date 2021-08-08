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

#include "WebBroadcastChannelRegistryMessages.h"
#include <WebCore/MessageWithMessagePorts.h>
#include <wtf/CallbackAggregator.h>

namespace WebKit {

NetworkBroadcastChannelRegistry::NetworkBroadcastChannelRegistry() = default;

void NetworkBroadcastChannelRegistry::registerChannel(IPC::Connection& connection, const WebCore::SecurityOriginData& origin, const String& name, WebCore::BroadcastChannelIdentifier channelIdentifier)
{
    auto& channelsForOrigin = m_broadcastChannels.ensure(origin, [] { return NameToChannelIdentifiersMap { }; }).iterator->value;
    auto& channelsForName = channelsForOrigin.ensure(name, [] { return Vector<GlobalBroadcastChannelIdentifier> { }; }).iterator->value;
    GlobalBroadcastChannelIdentifier globalChannelIdentifier { connection.uniqueID(), channelIdentifier };
    ASSERT(!channelsForName.contains(globalChannelIdentifier));
    channelsForName.append(WTFMove(globalChannelIdentifier));
}

void NetworkBroadcastChannelRegistry::unregisterChannel(IPC::Connection& connection, const WebCore::SecurityOriginData& origin, const String& name, WebCore::BroadcastChannelIdentifier channelIdentifier)
{
    auto channelsForOriginIterator = m_broadcastChannels.find(origin);
    ASSERT(channelsForOriginIterator != m_broadcastChannels.end());
    if (channelsForOriginIterator == m_broadcastChannels.end())
        return;
    auto channelsForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(channelsForNameIterator != channelsForOriginIterator->value.end());
    if (channelsForNameIterator == channelsForOriginIterator->value.end())
        return;

    GlobalBroadcastChannelIdentifier globalChannelIdentifier { connection.uniqueID(), channelIdentifier };
    ASSERT(channelsForNameIterator->value.contains(globalChannelIdentifier));
    channelsForNameIterator->value.removeFirst(globalChannelIdentifier);
}

void NetworkBroadcastChannelRegistry::postMessage(IPC::Connection& connection, const WebCore::SecurityOriginData& origin, const String& name, WebCore::BroadcastChannelIdentifier source, WebCore::MessageWithMessagePorts&& message, CompletionHandler<void()>&& completionHandler)
{
    auto channelsForOriginIterator = m_broadcastChannels.find(origin);
    ASSERT(channelsForOriginIterator != m_broadcastChannels.end());
    if (channelsForOriginIterator == m_broadcastChannels.end())
        return completionHandler();
    auto channelsForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(channelsForNameIterator != channelsForOriginIterator->value.end());
    if (channelsForNameIterator == channelsForOriginIterator->value.end())
        return completionHandler();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    GlobalBroadcastChannelIdentifier sourceGlobalChannelIdentifier { connection.uniqueID(), source };
    for (auto& globalIdentifier : channelsForNameIterator->value) {
        if (globalIdentifier == sourceGlobalChannelIdentifier)
            continue;

        RefPtr connection = IPC::Connection::connection(globalIdentifier.connectionIdentifier);
        if (!connection)
            continue;

        connection->sendWithAsyncReply(Messages::WebBroadcastChannelRegistry::PostMessageToRemote(globalIdentifier.channelIndentifierInProcess, message), [callbackAggregator] { }, 0);
    }
}

void NetworkBroadcastChannelRegistry::removeConnection(IPC::Connection& connection)
{
    Vector<WebCore::SecurityOriginData> originsToRemove;
    for (auto& entry : m_broadcastChannels) {
        Vector<String> namesToRemove;
        for (auto& innerEntry : entry.value) {
            innerEntry.value.removeAllMatching([connectionIdentifier = connection.uniqueID()](auto& globalIdentifier) {
                return globalIdentifier.connectionIdentifier == connectionIdentifier;
            });
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

} // namespace WebKit
