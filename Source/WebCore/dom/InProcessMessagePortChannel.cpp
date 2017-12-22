/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "InProcessMessagePortChannel.h"

#include "MessagePort.h"
#include <wtf/Locker.h>

namespace WebCore {

void InProcessMessagePortChannel::createChannelBetweenPorts(MessagePort& port1, MessagePort& port2)
{
    auto queue1 = MessagePortQueue::create();
    auto queue2 = MessagePortQueue::create();

    auto channel1 = InProcessMessagePortChannel::create(queue1.get(), queue2.get());
    auto channel2 = InProcessMessagePortChannel::create(queue2.get(), queue1.get());

    channel1->m_entangledChannel = channel2.ptr();
    channel2->m_entangledChannel = channel1.ptr();

    port1.entangle(WTFMove(channel2));
    port2.entangle(WTFMove(channel1));
}

Ref<InProcessMessagePortChannel> InProcessMessagePortChannel::create(MessagePortQueue& incoming, MessagePortQueue& outgoing)
{
    return adoptRef(*new InProcessMessagePortChannel(incoming, outgoing));
}

InProcessMessagePortChannel::InProcessMessagePortChannel(MessagePortQueue& incoming, MessagePortQueue& outgoing)
    : m_incomingQueue(&incoming)
    , m_outgoingQueue(&outgoing)
{
}

InProcessMessagePortChannel::~InProcessMessagePortChannel()
{
    // Channels being destroyed should to have been closed.
    ASSERT(!m_outgoingQueue);
}

void InProcessMessagePortChannel::postMessageToRemote(Ref<SerializedScriptValue>&& message, std::unique_ptr<MessagePortChannelArray>&& channels)
{
    Locker<Lock> locker(m_lock);

    if (!m_outgoingQueue)
        return;

    bool wasEmpty = m_outgoingQueue->appendAndCheckEmpty(std::make_unique<EventData>(WTFMove(message), WTFMove(channels)));
    if (wasEmpty && m_remotePort)
        m_remotePort->messageAvailable();
}

Deque<std::unique_ptr<MessagePortChannel::EventData>> InProcessMessagePortChannel::takeAllMessagesFromRemote()
{
    Locker<Lock> locker(m_lock);
    return m_incomingQueue->takeAllMessages();
}

bool InProcessMessagePortChannel::isConnectedTo(MessagePort& port)
{
    // FIXME: What guarantees that the result remains the same after we release the lock?
    Locker<Lock> locker(m_lock);
    return m_remotePort == &port;
}

bool InProcessMessagePortChannel::entangleIfOpen(MessagePort& port)
{
    // We can't call member functions on our remote pair while holding our mutex or we'll deadlock,
    // but we need to guard against the remote port getting closed/freed, so create a standalone reference.
    RefPtr<InProcessMessagePortChannel> remote;
    {
        Locker<Lock> locker(m_lock);
        remote = m_entangledChannel;
    }

    if (!remote)
        return false;

    remote->setRemotePort(&port);

    return true;
}

void InProcessMessagePortChannel::disentangle()
{
    Locker<Lock> locker(m_lock);

    if (m_entangledChannel)
        m_entangledChannel->setRemotePort(nullptr);
}

bool InProcessMessagePortChannel::hasPendingActivity()
{
    // FIXME: What guarantees that the result remains the same after we release the lock?
    Locker<Lock> locker(m_lock);
    return !m_incomingQueue->isEmpty();
}

MessagePort* InProcessMessagePortChannel::locallyEntangledPort(const ScriptExecutionContext* context)
{
    Locker<Lock> locker(m_lock);

    // See if both contexts are run by the same thread (are the same context, or are both documents).
    if (!m_remotePort)
        return nullptr;

    // The remote port's ScriptExecutionContext is guaranteed not to change here - MessagePort::contextDestroyed()
    // will close the port before the context goes away, and close() will block because we are holding the mutex.
    ScriptExecutionContext* remoteContext = m_remotePort->scriptExecutionContext();
    if (remoteContext == context || (remoteContext && remoteContext->isDocument() && context->isDocument()))
        return m_remotePort;

    return nullptr;
}

RefPtr<InProcessMessagePortChannel> InProcessMessagePortChannel::takeEntangledChannel()
{
    RefPtr<InProcessMessagePortChannel> channel;

    {
        Locker<Lock> locker(m_lock);
        channel = WTFMove(m_entangledChannel);
    }

    return channel;
}

void InProcessMessagePortChannel::close()
{
    Locker<Lock> locker(m_lock);

    RefPtr<InProcessMessagePortChannel> channel;
    if (m_entangledChannel) {
        channel = m_entangledChannel->takeEntangledChannel();
        ASSERT(channel == this);
        m_entangledChannel->close();
    }

    // Disentangle ourselves from the other end. We still maintain a reference to our incoming queue, since previously-existing messages should still be delivered.
    m_remotePort = nullptr;
    m_outgoingQueue = nullptr;
    m_entangledChannel = nullptr;
}

void InProcessMessagePortChannel::setRemotePort(MessagePort* port)
{
    Locker<Lock> locker(m_lock);

    // Should never set port if it is already set.
    ASSERT(!port || !m_remotePort);

    m_remotePort = port;
}

} // namespace WebCore
