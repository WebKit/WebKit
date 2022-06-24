/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

namespace IPC {

// FIXME(http://webkit.org/b/238986): Workaround for not being able to deliver messages from the dedicated connection to the work queue the client uses.
class StreamClientConnection::DedicatedConnectionClient final : public Connection::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DedicatedConnectionClient);
public:
    DedicatedConnectionClient(MessageReceiver& receiver)
        : m_receiver(receiver)
    {
    }
    // Connection::Client.
    void didReceiveMessage(Connection& connection, Decoder& decoder) final { m_receiver.didReceiveMessage(connection, decoder); }
    bool didReceiveSyncMessage(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder) final { return m_receiver.didReceiveSyncMessage(connection, decoder, replyEncoder); }
    void didClose(Connection&) final { } // Client is expected to listen to Connection::didClose() from the connection it sent to the dedicated connection to.
    void didReceiveInvalidMessage(Connection&, MessageName) final { ASSERT_NOT_REACHED(); } // The sender is expected to be trusted, so all invalid messages are programming errors.
private:
    MessageReceiver& m_receiver;
};

StreamClientConnection::StreamConnectionWithDedicatedConnection StreamClientConnection::createWithDedicatedConnection(MessageReceiver& receiver, size_t bufferSize)
{
    auto connectionIdentifiers = Connection::createConnectionIdentifierPair();
    // Create StreamClientConnection with "server" type Connection. The caller will send the "client" type connection identifier via
    // IPC to the other side, where StreamServerConnection will be created with "client" type Connection.
    // For Connection, "server" means the connection which was created first, the connection which is not sent through IPC to other party.
    // For Connection, "client" means the connection which was established by receiving it through IPC and creating IPC::Connection out from the identifier.
    // The "Client" in StreamClientConnection means the party that mostly does sending, e.g. untrusted party.
    // The "Server" in StreamServerConnection means the party that mostly does receiving, e.g. the trusted party which holds the destination object to communicate with.
    auto dedicatedConnectionClient = makeUnique<DedicatedConnectionClient>(receiver);
    auto dedicatedConnection = Connection::createServerConnection(connectionIdentifiers->server, *dedicatedConnectionClient);
    std::unique_ptr<StreamClientConnection> streamConnection { new StreamClientConnection(WTFMove(dedicatedConnection), bufferSize, WTFMove(dedicatedConnectionClient)) };
    // FIXME(http://webkit.org/b238944): Make IPC::Attachment really movable on OS(DARWIN).
    return { WTFMove(streamConnection), WTFMove(connectionIdentifiers->client) };
#
}

StreamClientConnection::StreamClientConnection(Ref<Connection>&& connection, size_t bufferSize, std::unique_ptr<DedicatedConnectionClient>&& dedicatedConnectionClient)
    : m_connection(WTFMove(connection))
    , m_dedicatedConnectionClient(WTFMove(dedicatedConnectionClient))
    , m_buffer(bufferSize)
{
    // Read starts from 0 with limit of 0 and reader sleeping.
    sharedClientOffset().store(ClientOffset::serverIsSleepingTag, std::memory_order_relaxed);
    // Write starts from 0 with a limit of the whole buffer.
    sharedClientLimit().store(static_cast<ClientLimit>(0), std::memory_order_relaxed);
}

StreamClientConnection::StreamClientConnection(Connection& connection, size_t bufferSize)
    : StreamClientConnection(Ref { connection }, bufferSize, { })
{
}

StreamClientConnection::~StreamClientConnection()
{
    ASSERT(!m_dedicatedConnectionClient || !m_connection->isValid());
}

void StreamClientConnection::open()
{
    if (m_dedicatedConnectionClient)
        m_connection->open();
}

void StreamClientConnection::invalidate()
{
    if (m_dedicatedConnectionClient)
        m_connection->invalidate();
}

void StreamClientConnection::setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait)
{
    m_semaphores = { WTFMove(wakeUp), WTFMove(clientWait) };
    wakeUpServer(WakeUpServer::Yes);
}

void StreamClientConnection::wakeUpServer(WakeUpServer wakeUpResult)
{
    if (wakeUpResult == WakeUpServer::No && !m_batchSize)
        return;
    if (!m_semaphores)
        return;
    m_semaphores->wakeUp.signal();
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

StreamConnectionBuffer& StreamClientConnection::bufferForTesting()
{
    return m_buffer;
}

Connection& StreamClientConnection::connectionForTesting()
{
    return m_connection.get();
}

}
