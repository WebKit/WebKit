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
#include "MessagePortChannel.h"

#include "Logging.h"
#include "MessagePortChannelRegistry.h"

namespace WebCore {

Ref<MessagePortChannel> MessagePortChannel::create(MessagePortChannelRegistry& registry, const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    return adoptRef(*new MessagePortChannel(registry, port1, port2));
}

MessagePortChannel::MessagePortChannel(MessagePortChannelRegistry& registry, const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
    : m_registry(registry)
{
    relaxAdoptionRequirement();

    m_ports[0] = port1;
    m_processes[0] = port1.processIdentifier;
    m_entangledToProcessProtectors[0] = this;
    m_ports[1] = port2;
    m_processes[1] = port2.processIdentifier;
    m_entangledToProcessProtectors[1] = this;

    m_registry.messagePortChannelCreated(*this);
}

MessagePortChannel::~MessagePortChannel()
{
    m_registry.messagePortChannelDestroyed(*this);
}

bool MessagePortChannel::includesPort(const MessagePortIdentifier& port)
{
    Locker<Lock> locker(m_lock);

    return m_ports[0] == port || m_ports[1] == port;
}

void MessagePortChannel::entanglePortWithProcess(const MessagePortIdentifier& port, ProcessIdentifier process)
{
    Locker<Lock> locker(m_lock);

    LOG(MessagePorts, "MessagePortChannel %s (%p) entangling port %s", logString().utf8().data(), this, port.logString().utf8().data());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    ASSERT(!m_processes[i] || *m_processes[i] == process);
    m_processes[i] = process;
    m_entangledToProcessProtectors[i] = this;
}

void MessagePortChannel::disentanglePort(const MessagePortIdentifier& port)
{
    Locker<Lock> locker(m_lock);

    LOG(MessagePorts, "MessagePortChannel %s (%p) disentangling port %s", logString().utf8().data(), this, port.logString().utf8().data());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    ASSERT(m_processes[i] || m_isClosed[i]);
    m_processes[i] = std::nullopt;

    // This set of steps is to guarantee that the lock is unlocked before the
    // last ref to this object is released.
    auto protectedThis = WTFMove(m_entangledToProcessProtectors[i]);
    locker.unlockEarly();
}

void MessagePortChannel::closePort(const MessagePortIdentifier& port)
{
    Locker<Lock> locker(m_lock);

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    m_processes[i] = std::nullopt;
    m_isClosed[i] = true;

    // This set of steps is to guarantee that the lock is unlocked before the
    // last ref to this object is released.
    auto protectedThis = makeRef(*this);

    m_pendingMessages[i].clear();
    m_pendingMessagePortTransfers[i].clear();
    m_pendingMessageProtectors[i] = nullptr;
    m_entangledToProcessProtectors[i] = nullptr;

    locker.unlockEarly();
}

bool MessagePortChannel::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget)
{
    Locker<Lock> locker(m_lock);

    ASSERT(remoteTarget == m_ports[0] || remoteTarget == m_ports[1]);
    size_t i = remoteTarget == m_ports[0] ? 0 : 1;

    for (auto& channelPair : message.transferredPorts) {
        auto* channel = m_registry.existingChannelContainingPort(channelPair.first);
        // One of the ports in the channel might have been closed, therefore removing record of the channel.
        // That's okay; such ports can still be transferred. We just don't have to protect the channel.
        if (!channel)
            continue;

        ASSERT(channel->includesPort(channelPair.second));

#ifndef NDEBUG
        if (auto* otherChannel = m_registry.existingChannelContainingPort(channelPair.second))
            ASSERT(channel == otherChannel);
#endif
        // Having a pending message should keep a port alive with a ref.
        // The ref will be cleared after the batch of pending messages has been delivered.
        m_pendingMessagePortTransfers[i].add(channel);
    }

    m_pendingMessages[i].append(WTFMove(message));
    LOG(MessagePorts, "MessagePortChannel %s (%p) now has %zu messages pending on port %s", logString().utf8().data(), this, m_pendingMessages[i].size(), remoteTarget.logString().utf8().data());

    if (m_pendingMessages[i].size() == 1) {
        m_pendingMessageProtectors[i] = this;
        return true;
    }

    ASSERT(m_pendingMessageProtectors[i] == this);
    return false;
}

void MessagePortChannel::takeAllMessagesForPort(const MessagePortIdentifier& port, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&& callback)
{
    Locker<Lock> locker(m_lock);

    LOG(MessagePorts, "MessagePortChannel %p taking all messages for port %s", this, port.logString().utf8().data());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    if (m_pendingMessages[i].isEmpty()) {
        callback({ }, [] { });
        return;
    }

    ASSERT(m_pendingMessageProtectors[i]);

    Vector<MessageWithMessagePorts> result;
    result.swap(m_pendingMessages[i]);

    ++m_messageBatchesInFlight;

    LOG(MessagePorts, "There are %zu messages to take for port %s. Taking them now, messages in flight is now %llu", result.size(), port.logString().utf8().data(), m_messageBatchesInFlight);

    auto size = result.size();
    HashSet<RefPtr<MessagePortChannel>> transferredPortProtectors;
    transferredPortProtectors.swap(m_pendingMessagePortTransfers[i]);

    locker.unlockEarly();
    callback(WTFMove(result), [size, this, port, protectedThis = WTFMove(m_pendingMessageProtectors[i]), transferredPortProtectors = WTFMove(transferredPortProtectors)] {
        UNUSED_PARAM(port);
        --m_messageBatchesInFlight;
        LOG(MessagePorts, "Message port channel %s was notified that a batch of %zu message port messages targeted for port %s just completed dispatch, in flight is now %llu", logString().utf8().data(), size, port.logString().utf8().data(), m_messageBatchesInFlight);

    });
}

bool MessagePortChannel::hasAnyMessagesPendingOrInFlight() const
{
    Locker<Lock> locker(m_lock);

    return m_messageBatchesInFlight || !m_pendingMessages[0].isEmpty() || !m_pendingMessages[1].isEmpty();
}

} // namespace WebCore
