/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

namespace WebCore {

// MessagePortChannel implementations - just delegate to the PlatformMessagePortChannel.
bool MessagePortChannel::entangleIfOpen(MessagePort* port)
{
    return m_channel->entangleIfOpen(port);
}

void MessagePortChannel::disentangle()
{
    m_channel->disentangle();
}

void MessagePortChannel::postMessageToRemote(PassOwnPtr<MessagePortChannel::EventData> message)
{
    m_channel->postMessageToRemote(message);
}

bool MessagePortChannel::tryGetMessageFromRemote(OwnPtr<MessagePortChannel::EventData>& result)
{
    return m_channel->tryGetMessageFromRemote(result);
}

void MessagePortChannel::close()
{
    m_channel->close();
}

bool MessagePortChannel::isConnectedTo(MessagePort* port)
{
    return m_channel->isConnectedTo(port);
}

bool MessagePortChannel::hasPendingActivity()
{
    return m_channel->hasPendingActivity();
}


PassRefPtr<PlatformMessagePortChannel> PlatformMessagePortChannel::create(PassRefPtr<MessagePortQueue> incoming, PassRefPtr<MessagePortQueue> outgoing)
{
    return adoptRef(new PlatformMessagePortChannel(incoming, outgoing));
}

PlatformMessagePortChannel::PlatformMessagePortChannel(PassRefPtr<MessagePortQueue> incoming, PassRefPtr<MessagePortQueue> outgoing)
    : m_entangledChannel(0)
    , m_incomingQueue(incoming)
    , m_outgoingQueue(outgoing)
    , m_remotePort(0)
{
}

PlatformMessagePortChannel::~PlatformMessagePortChannel()
{
}

void PlatformMessagePortChannel::createChannel(PassRefPtr<MessagePort> port1, PassRefPtr<MessagePort> port2)
{
    // Create incoming/outgoing queues.
    RefPtr<PlatformMessagePortChannel::MessagePortQueue> queue1 = PlatformMessagePortChannel::MessagePortQueue::create();
    RefPtr<PlatformMessagePortChannel::MessagePortQueue> queue2 = PlatformMessagePortChannel::MessagePortQueue::create();

    // Create proxies for each endpoint.
    RefPtr<PlatformMessagePortChannel> channel1 = PlatformMessagePortChannel::create(queue1, queue2);
    RefPtr<PlatformMessagePortChannel> channel2 = PlatformMessagePortChannel::create(queue2, queue1);

    // Entangle the two endpoints.
    channel1->setEntangledChannel(channel2);
    channel2->setEntangledChannel(channel1);

    // Now entangle the proxies with the appropriate local ports.
    port1->entangle(MessagePortChannel::create(channel2));
    port2->entangle(MessagePortChannel::create(channel1));
}

bool PlatformMessagePortChannel::entangleIfOpen(MessagePort* port)
{
    // We can't call member functions on our remote pair while holding our mutex or we'll deadlock, but we need to guard against the remote port getting closed/freed, so create a standalone reference.
    RefPtr<PlatformMessagePortChannel> remote = entangledChannel();
    if (!remote)
        return false;
    remote->setRemotePort(port);
    return true;
}

void PlatformMessagePortChannel::disentangle()
{
    RefPtr<PlatformMessagePortChannel> remote = entangledChannel();
    if (remote)
        remote->setRemotePort(0);
}

void PlatformMessagePortChannel::setRemotePort(MessagePort* port)
{
    MutexLocker lock(m_mutex);
    // Should never set port if it is already set.
    ASSERT(!port || !m_remotePort);
    m_remotePort = port;
}

MessagePort* PlatformMessagePortChannel::remotePort()
{
    MutexLocker lock(m_mutex);
    return m_remotePort;
}

PassRefPtr<PlatformMessagePortChannel> PlatformMessagePortChannel::entangledChannel()
{
    MutexLocker lock(m_mutex);
    return m_entangledChannel;
}

void PlatformMessagePortChannel::setEntangledChannel(PassRefPtr<PlatformMessagePortChannel> remote)
{
    MutexLocker lock(m_mutex);
    // Should only be set as part of initial creation/entanglement.
    if (remote)
        ASSERT(!m_entangledChannel.get());
    m_entangledChannel = remote;
}

void PlatformMessagePortChannel::postMessageToRemote(PassOwnPtr<MessagePortChannel::EventData> message)
{
    MutexLocker lock(m_mutex);
    if (!m_outgoingQueue)
        return;
    bool wasEmpty = m_outgoingQueue->appendAndCheckEmpty(message);
    if (wasEmpty && m_remotePort)
        m_remotePort->messageAvailable();
}

bool PlatformMessagePortChannel::tryGetMessageFromRemote(OwnPtr<MessagePortChannel::EventData>& result)
{
    MutexLocker lock(m_mutex);
    return m_incomingQueue->tryGetMessage(result);
}

bool PlatformMessagePortChannel::isConnectedTo(MessagePort* port)
{
    MutexLocker lock(m_mutex);
    return m_remotePort == port;
}

// Closes the port so no further messages can be sent from either end.
void PlatformMessagePortChannel::close()
{
    RefPtr<PlatformMessagePortChannel> remote = entangledChannel();
    if (!remote)
        return;
    closeInternal();
    remote->closeInternal();
}

void PlatformMessagePortChannel::closeInternal()
{
    MutexLocker lock(m_mutex);
    // Disentangle ourselves from the other end. We still maintain a reference to our incoming queue, since previously-existing messages should still be delivered.
    m_remotePort = 0;
    m_entangledChannel = 0;
    m_outgoingQueue = 0;
}

bool PlatformMessagePortChannel::hasPendingActivity()
{
    MutexLocker lock(m_mutex);
    return m_entangledChannel || !m_incomingQueue->isEmpty();
}

} // namespace WebCore
