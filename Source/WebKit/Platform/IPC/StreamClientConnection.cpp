/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "StreamClientConnection.h"
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace IPC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(StreamClientConnection);
WTF_MAKE_TZONE_ALLOCATED_IMPL(StreamClientConnection::DedicatedConnectionClient);

// FIXME(http://webkit.org/b/238986): Workaround for not being able to deliver messages from the dedicated connection to the work queue the client uses.

StreamClientConnection::DedicatedConnectionClient::DedicatedConnectionClient(StreamClientConnection& owner, Connection::Client& receiver)
    : m_owner(owner)
    , m_receiver(receiver)
{
}

void StreamClientConnection::DedicatedConnectionClient::didReceiveMessage(Connection& connection, Decoder& decoder)
{
    if (RefPtr receiver = m_receiver.get())
        receiver->didReceiveMessage(connection, decoder);
}

void StreamClientConnection::DedicatedConnectionClient::didReceiveSyncMessage(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder)
{
    if (RefPtr receiver = m_receiver.get())
        receiver->didReceiveSyncMessage(connection, decoder, replyEncoder);
}

void StreamClientConnection::DedicatedConnectionClient::didClose(Connection& connection)
{
    // Client is expected to listen to Connection::didClose() from the connection it sent to the dedicated connection to.
    if (RefPtr receiver = m_receiver.get())
        receiver->didClose(connection);
}

void StreamClientConnection::DedicatedConnectionClient::didReceiveInvalidMessage(Connection&, MessageName, const Vector<uint32_t>&)
{
    ASSERT_NOT_REACHED(); // The sender is expected to be trusted, so all invalid messages are programming errors.
}

std::optional<StreamClientConnection::StreamConnectionPair> StreamClientConnection::create(unsigned bufferSizeLog2, Seconds defaultTimeoutDuration)
{
    auto connectionIdentifiers = Connection::createConnectionIdentifierPair();
    if (!connectionIdentifiers)
        return std::nullopt;
    auto buffer = StreamClientConnectionBuffer::create(bufferSizeLog2);
    if (!buffer)
        return std::nullopt;
    // Create StreamClientConnection with "server" type Connection. The caller will send the "client" type connection identifier via
    // IPC to the other side, where StreamServerConnection will be created with "client" type Connection.
    // For Connection, "server" means the connection which was created first, the connection which is not sent through IPC to other party.
    // For Connection, "client" means the connection which was established by receiving it through IPC and creating IPC::Connection out from the identifier.
    // The "Client" in StreamClientConnection means the party that mostly does sending, e.g. untrusted party.
    // The "Server" in StreamServerConnection means the party that mostly does receiving, e.g. the trusted party which holds the destination object to communicate with.
    auto dedicatedConnection = Connection::createServerConnection(WTFMove(connectionIdentifiers->server));
    auto clientConnection = adoptRef(*new StreamClientConnection(WTFMove(dedicatedConnection), WTFMove(*buffer), defaultTimeoutDuration));
    StreamServerConnection::Handle serverHandle {
        WTFMove(connectionIdentifiers->client),
        clientConnection->m_buffer.createHandle()
    };
    return StreamClientConnection::StreamConnectionPair { WTFMove(clientConnection), WTFMove(serverHandle) };
}

StreamClientConnection::StreamClientConnection(Ref<Connection> connection, StreamClientConnectionBuffer&& buffer, Seconds defaultTimeoutDuration)
    : m_connection(WTFMove(connection))
    , m_buffer(WTFMove(buffer))
    , m_defaultTimeoutDuration(defaultTimeoutDuration)
{
}

StreamClientConnection::~StreamClientConnection()
{
    ASSERT(!m_connection->isValid());
}

void StreamClientConnection::setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait)
{
    m_buffer.setSemaphores(WTFMove(wakeUp), WTFMove(clientWait));
}

bool StreamClientConnection::hasSemaphores() const
{
    return m_buffer.hasSemaphores();
}

void StreamClientConnection::setMaxBatchSize(unsigned size)
{
    m_maxBatchSize = size;
    m_buffer.wakeUpServer();
}

void StreamClientConnection::open(Connection::Client& receiver, SerialFunctionDispatcher& dispatcher)
{
    lazyInitialize(m_dedicatedConnectionClient, makeUniqueWithoutRefCountedCheck<DedicatedConnectionClient>(*this, receiver));
    m_connection->open(Ref { *m_dedicatedConnectionClient }.get(), dispatcher);
}

Error StreamClientConnection::flushSentMessages()
{
    auto timeout = defaultTimeout();
    wakeUpServer(WakeUpServer::Yes);
    return m_connection->flushSentMessages(WTFMove(timeout));
}

void StreamClientConnection::invalidate()
{
    m_connection->invalidate();
}

void StreamClientConnection::wakeUpServer(WakeUpServer wakeUpResult)
{
    if (wakeUpResult == WakeUpServer::No && !m_batchSize)
        return;
    m_buffer.wakeUpServer();
    m_batchSize = 0;
}

void StreamClientConnection::wakeUpServerBatched(WakeUpServer wakeUpResult)
{
    if (wakeUpResult == WakeUpServer::Yes || m_batchSize) {
        m_batchSize++;
        if (m_batchSize >= m_maxBatchSize)
            wakeUpServer(WakeUpServer::Yes);
    }
}

StreamClientConnectionBuffer& StreamClientConnection::bufferForTesting()
{
    return m_buffer;
}

Connection& StreamClientConnection::connectionForTesting()
{
    return m_connection.get();
}

void StreamClientConnection::addWorkQueueMessageReceiver(ReceiverName name, WorkQueue& workQueue, WorkQueueMessageReceiverBase& receiver, uint64_t destinationID)
{
    m_connection->addWorkQueueMessageReceiver(name, workQueue, receiver, destinationID);
}

void StreamClientConnection::removeWorkQueueMessageReceiver(ReceiverName name, uint64_t destinationID)
{
    m_connection->removeWorkQueueMessageReceiver(name, destinationID);
}

#if ENABLE(CORE_IPC_SIGNPOSTS)

static bool streamingIPCSignpostsEnabled = false;

void StreamClientConnection::forceEnableSignposts()
{
    streamingIPCSignpostsEnabled = true;
}

bool StreamClientConnection::signpostsEnabled()
{
    static bool hasReadPreferences = false;
    if (!hasReadPreferences) [[unlikely]] {
        if (!isInAuxiliaryProcess() && CFPreferencesGetAppBooleanValue(CFSTR("WebKitDebugStreamingIPCSignposts"), kCFPreferencesCurrentApplication, nullptr))
            streamingIPCSignpostsEnabled = true;
        hasReadPreferences = true;
    }

    return streamingIPCSignpostsEnabled;
}

uintptr_t StreamClientConnection::generateSignpostIdentifier()
{
    static std::atomic<uintptr_t> identifier;
    return ++identifier;
}

void StreamClientConnection::emitSendSignpost(MessageName messageName)
{
    // Signposts can turn in to log message IPCs when emitted from WebContent. Don't emit a signpost
    // for log messages to avoid an infinite number of signposts.
    if (signpostsEnabled() && receiverName(messageName) != IPC::ReceiverName::LogStream) [[unlikely]]
        WTFEmitSignpost(generateSignpostIdentifier(), StreamClientConnection, "send: %" PUBLIC_LOG_STRING, description(messageName).characters());
}

#endif

}
