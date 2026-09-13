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
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<MessagePortChannel> MessagePortChannel::create(MessagePortChannelRegistry& registry, const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    return adoptRef(*new MessagePortChannel(registry, port1, port2));
}

MessagePortChannel::MessagePortChannel(MessagePortChannelRegistry& registry, const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
    : m_ports { port1, port2 }
    , m_registry(registry)
{
    ASSERT(isMainThread());

    m_processes[0] = port1.processIdentifier;
    m_entangledToProcessProtectors[0] = this;
    m_processes[1] = port2.processIdentifier;
    m_entangledToProcessProtectors[1] = this;

    protect(m_registry)->messagePortChannelCreated(*this);
}

MessagePortChannel::~MessagePortChannel()
{
    protect(m_registry)->messagePortChannelDestroyed(*this);
}

std::optional<ProcessIdentifier> MessagePortChannel::processForPort(const MessagePortIdentifier& port)
{
    ASSERT(isMainThread());
    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;
    return m_processes[i];
}

bool MessagePortChannel::includesPort(const MessagePortIdentifier& port)
{
    ASSERT(isMainThread());

    return m_ports[0] == port || m_ports[1] == port;
}

void MessagePortChannel::entanglePortWithProcess(const MessagePortIdentifier& port, ProcessIdentifier process)
{
    ASSERT(isMainThread());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    LOG_WITH_STREAM(MessagePorts, stream << "MessagePortChannel "_s << logString() << " ("_s << this << ") entangling port "_s << port.logString() << " (that port has "_s << m_pendingMessages[i].size() << " messages available)"_s);

    ASSERT(!m_processes[i] || *m_processes[i] == process);
    m_processes[i] = process;

    if (m_status[i] == MessagePortStatus::Unclaimed)
        m_status[i] = MessagePortStatus::Open;

    m_entangledToProcessProtectors[i] = this;
    m_pendingMessagePortTransfers[i].remove(*this);
}

void MessagePortChannel::disentanglePort(const MessagePortIdentifier& port)
{
    ASSERT(isMainThread());

    LOG_WITH_STREAM(MessagePorts, stream << "MessagePortChannel "_s << logString() << " ("_s << this << ") disentangling port "_s << port.logString());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    ASSERT(m_processes[i] || m_status[i] != MessagePortStatus::Open);
    m_processes[i] = std::nullopt;
    m_pendingMessagePortTransfers[i].add(*this);

    // This set of steps is to guarantee that the lock is unlocked before the
    // last ref to this object is released.
    auto protectedThis = WTF::move(m_entangledToProcessProtectors[i]);
}

void MessagePortChannel::closePort(const MessagePortIdentifier& port, MessagePortStatus status)
{
    ASSERT(isMainThread());
    ASSERT(status != MessagePortStatus::Open);

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    m_processes[i] = std::nullopt;
    if (m_status[i] != MessagePortStatus::Closed)
        m_status[i] = status;

    m_pendingMessages[i].clear();
    m_pendingMessagePortTransfers[i].clear();
    m_pendingMessageProtectors[i] = nullptr;
    m_entangledToProcessProtectors[i] = nullptr;
}

bool MessagePortChannel::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget, CompletionHandlerCallingScope&& blobURLsInFlight)
{
    ASSERT(isMainThread());

    ASSERT(remoteTarget == m_ports[0] || remoteTarget == m_ports[1]);
    size_t i = remoteTarget == m_ports[0] ? 0 : 1;

    if (m_status[i] != MessagePortStatus::Open)
        return false;

    m_pendingMessages[i].append({ WTF::move(message), WTF::move(blobURLsInFlight) });
    LOG_WITH_STREAM(MessagePorts, stream << "MessagePortChannel "_s << logString() << " ("_s << this << ") now has "_s << m_pendingMessages[i].size() << " messages pending on port "_s << remoteTarget.logString());

    if (m_pendingMessages[i].size() == 1) {
        m_pendingMessageProtectors[i] = this;
        return true;
    }

    ASSERT(m_pendingMessageProtectors[i] == this);
    return false;
}

void MessagePortChannel::takeAllMessagesForPort(const MessagePortIdentifier& port, CompletionHandler<void(Vector<MessageWithMessagePorts>&&, CompletionHandler<void()>&&)>&& callback)
{
    ASSERT(isMainThread());

    LOG_WITH_STREAM(MessagePorts, stream << "MessagePortChannel "_s << this << " taking all messages for port "_s << port.logString());

    ASSERT(port == m_ports[0] || port == m_ports[1]);
    size_t i = port == m_ports[0] ? 0 : 1;

    if (m_pendingMessages[i].isEmpty()) {
        callback({ }, [] { });
        return;
    }

    ASSERT(m_pendingMessageProtectors[i] == this);

    auto pendingMessages = std::exchange(m_pendingMessages[i], { });

    ++m_messageBatchesInFlight;

    LOG_WITH_STREAM(MessagePorts, stream << "There are "_s << pendingMessages.size() << " messages to take for port "_s << port.logString() << ". Taking them now, messages in flight is now "_s << m_messageBatchesInFlight);

    auto size = pendingMessages.size();
    auto messages = WTF::map(pendingMessages, [](auto& pendingMessage) {
        return WTF::move(pendingMessage.first);
    });
    auto blobURLsInFlight = WTF::map(WTF::move(pendingMessages), [](auto&& pendingMessage) {
        return WTF::move(pendingMessage.second);
    });

    callback(WTF::move(messages), [size, port, blobURLsInFlight = WTF::move(blobURLsInFlight), protectedThis = WTF::move(m_pendingMessageProtectors[i])] {
        UNUSED_PARAM(port);
#if LOG_DISABLED
        UNUSED_PARAM(size);
#endif
        --(protectedThis->m_messageBatchesInFlight);
        LOG_WITH_STREAM(MessagePorts, stream << "Message port channel "_s << protectedThis->logString() << " was notified that a batch of "_s << size << " message port messages targeted for port "_s << port.logString() << " just completed dispatch, in flight is now "_s << protectedThis->m_messageBatchesInFlight);

    });
}

bool MessagePortChannel::hasAnyMessagesPendingOrInFlight() const
{
    ASSERT(isMainThread());
    return m_messageBatchesInFlight || !m_pendingMessages[0].isEmpty() || !m_pendingMessages[1].isEmpty();
}

} // namespace WebCore
