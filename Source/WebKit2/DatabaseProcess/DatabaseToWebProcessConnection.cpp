/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DatabaseToWebProcessConnection.h"

#include "DatabaseToWebProcessConnectionMessages.h"
#include "Logging.h"
#include "WebIDBConnectionToClient.h"
#include "WebIDBConnectionToClientMessages.h"
#include <wtf/RunLoop.h>

#if ENABLE(DATABASE_PROCESS)

namespace WebKit {

Ref<DatabaseToWebProcessConnection> DatabaseToWebProcessConnection::create(IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new DatabaseToWebProcessConnection(connectionIdentifier));
}

DatabaseToWebProcessConnection::DatabaseToWebProcessConnection(IPC::Connection::Identifier connectionIdentifier)
{
    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, *this);
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

DatabaseToWebProcessConnection::~DatabaseToWebProcessConnection()
{

}

void DatabaseToWebProcessConnection::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::DatabaseToWebProcessConnection::messageReceiverName()) {
        didReceiveDatabaseToWebProcessConnectionMessage(connection, decoder);
        return;
    }

#if ENABLE(INDEXED_DATABASE)
    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToClient::messageReceiverName()) {
        auto iterator = m_webIDBConnections.find(decoder.destinationID());
        if (iterator != m_webIDBConnections.end())
            iterator->value->didReceiveMessage(connection, decoder);
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

void DatabaseToWebProcessConnection::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (decoder.messageReceiverName() == Messages::DatabaseToWebProcessConnection::messageReceiverName()) {
        didReceiveSyncDatabaseToWebProcessConnectionMessage(connection, decoder, replyEncoder);
        return;
    }

    ASSERT_NOT_REACHED();
}

void DatabaseToWebProcessConnection::didClose(IPC::Connection&)
{
#if ENABLE(INDEXED_DATABASE)
    // FIXME: (Modern IDB) The WebProcess has disconnected, close all of the connections associated with it
#endif
}

void DatabaseToWebProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{

}

#if ENABLE(INDEXED_DATABASE)

static uint64_t generateConnectionToServerIdentifier()
{
    ASSERT(RunLoop::isMain());
    static uint64_t identifier = 0;
    return ++identifier;
}

void DatabaseToWebProcessConnection::establishIDBConnectionToServer(uint64_t& serverConnectionIdentifier)
{
    serverConnectionIdentifier = generateConnectionToServerIdentifier();
    LOG(IndexedDB, "DatabaseToWebProcessConnection::establishIDBConnectionToServer - %" PRIu64, serverConnectionIdentifier);
    ASSERT(!m_webIDBConnections.contains(serverConnectionIdentifier));

    m_webIDBConnections.set(serverConnectionIdentifier, WebIDBConnectionToClient::create(*this, serverConnectionIdentifier));
}

void DatabaseToWebProcessConnection::removeIDBConnectionToServer(uint64_t serverConnectionIdentifier)
{
    ASSERT(m_webIDBConnections.contains(serverConnectionIdentifier));

    auto connection = m_webIDBConnections.take(serverConnectionIdentifier);
    connection->disconnectedFromWebProcess();
}
#endif


} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
