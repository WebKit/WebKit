/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformMessagePortChannel.h"

#include "MessagePort.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

void MessagePortChannel::createChannel(MessagePort* port1, MessagePort* port2)
{
    Ref<PlatformMessagePortChannel::MessagePortQueue> queue1 = PlatformMessagePortChannel::MessagePortQueue::create();
    Ref<PlatformMessagePortChannel::MessagePortQueue> queue2 = PlatformMessagePortChannel::MessagePortQueue::create();

    auto channel1 = std::make_unique<MessagePortChannel>(PlatformMessagePortChannel::create(queue1.ptr(), queue2.ptr()));
    auto channel2 = std::make_unique<MessagePortChannel>(PlatformMessagePortChannel::create(queue2.ptr(), queue1.ptr()));

    channel1->m_channel->m_entangledChannel = channel2->m_channel;
    channel2->m_channel->m_entangledChannel = channel1->m_channel;

    port1->entangle(WTFMove(channel2));
    port2->entangle(WTFMove(channel1));
}

MessagePortChannel::MessagePortChannel(RefPtr<PlatformMessagePortChannel>&& channel)
    : m_channel(WTFMove(channel))
{
}

MessagePortChannel::~MessagePortChannel()
{
    close();
}

bool MessagePortChannel::entangleIfOpen(MessagePort* port)
{
    // We can't call member functions on our remote pair while holding our mutex or we'll deadlock,
    // but we need to guard against the remote port getting closed/freed, so create a standalone reference.
    RefPtr<PlatformMessagePortChannel> remote = m_channel->entangledChannel();
    if (!remote)
        return false;
    remote->setRemotePort(port);
    return true;
}

void MessagePortChannel::disentangle()
{
    RefPtr<PlatformMessagePortChannel> remote = m_channel->entangledChannel();
    if (remote)
        remote->setRemotePort(nullptr);
}

void MessagePortChannel::postMessageToRemote(Ref<SerializedScriptValue>&& message, std::unique_ptr<MessagePortChannelArray> channels)
{
    LockHolder lock(m_channel->m_mutex);
    if (!m_channel->m_outgoingQueue)
        return;
    bool wasEmpty = m_channel->m_outgoingQueue->appendAndCheckEmpty(std::make_unique<EventData>(WTFMove(message), WTFMove(channels)));
    if (wasEmpty && m_channel->m_remotePort)
        m_channel->m_remotePort->messageAvailable();
}

auto MessagePortChannel::takeMessageFromRemote() -> std::unique_ptr<EventData>
{
    LockHolder lock(m_channel->m_mutex);
    return m_channel->m_incomingQueue->takeMessage();
}

auto MessagePortChannel::takeAllMessagesFromRemote() -> Deque<std::unique_ptr<EventData>>
{
    LockHolder lock(m_channel->m_mutex);
    return m_channel->m_incomingQueue->takeAllMessages();
}

void MessagePortChannel::close()
{
    RefPtr<PlatformMessagePortChannel> remote = m_channel->entangledChannel();
    if (!remote)
        return;
    m_channel->closeInternal();
    remote->closeInternal();
}

bool MessagePortChannel::isConnectedTo(MessagePort* port)
{
    // FIXME: What guarantees that the result remains the same after we release the lock?
    LockHolder lock(m_channel->m_mutex);
    return m_channel->m_remotePort == port;
}

bool MessagePortChannel::hasPendingActivity()
{
    // FIXME: What guarantees that the result remains the same after we release the lock?
    LockHolder lock(m_channel->m_mutex);
    return !m_channel->m_incomingQueue->isEmpty();
}

MessagePort* MessagePortChannel::locallyEntangledPort(const ScriptExecutionContext* context)
{
    LockHolder lock(m_channel->m_mutex);
    // See if both contexts are run by the same thread (are the same context, or are both documents).
    if (m_channel->m_remotePort) {
        // The remote port's ScriptExecutionContext is guaranteed not to change here - MessagePort::contextDestroyed()
        // will close the port before the context goes away, and close() will block because we are holding the mutex.
        ScriptExecutionContext* remoteContext = m_channel->m_remotePort->scriptExecutionContext();
        if (remoteContext == context || (remoteContext && remoteContext->isDocument() && context->isDocument()))
            return m_channel->m_remotePort;
    }
    return 0;
}

Ref<PlatformMessagePortChannel> PlatformMessagePortChannel::create(MessagePortQueue* incoming, MessagePortQueue* outgoing)
{
    return adoptRef(*new PlatformMessagePortChannel(incoming, outgoing));
}

PlatformMessagePortChannel::PlatformMessagePortChannel(MessagePortQueue* incoming, MessagePortQueue* outgoing)
    : m_incomingQueue(incoming)
    , m_outgoingQueue(outgoing)
{
}

PlatformMessagePortChannel::~PlatformMessagePortChannel()
{
}

void PlatformMessagePortChannel::setRemotePort(MessagePort* port)
{
    LockHolder lock(m_mutex);
    // Should never set port if it is already set.
    ASSERT(!port || !m_remotePort);
    m_remotePort = port;
}

RefPtr<PlatformMessagePortChannel> PlatformMessagePortChannel::entangledChannel()
{
    // FIXME: What guarantees that the result remains the same after we release the lock?
    // This lock only guarantees that the returned pointer will not be pointing to released memory,
    // but not that it will still be pointing to this object's entangled port channel.
    LockHolder lock(m_mutex);
    return m_entangledChannel;
}

void PlatformMessagePortChannel::closeInternal()
{
    LockHolder lock(m_mutex);
    // Disentangle ourselves from the other end. We still maintain a reference to our incoming queue, since previously-existing messages should still be delivered.
    m_remotePort = nullptr;
    m_entangledChannel = nullptr;
    m_outgoingQueue = nullptr;
}

} // namespace WebCore
