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

#include "DatabaseProcessIDBConnection.h"
#include "DatabaseProcessIDBConnectionMessages.h"
#include "DatabaseToWebProcessConnectionMessages.h"
#include <wtf/RunLoop.h>

#if ENABLE(DATABASE_PROCESS)

namespace WebKit {

PassRefPtr<DatabaseToWebProcessConnection> DatabaseToWebProcessConnection::create(IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(new DatabaseToWebProcessConnection(connectionIdentifier));
}

DatabaseToWebProcessConnection::DatabaseToWebProcessConnection(IPC::Connection::Identifier connectionIdentifier)
{
    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

DatabaseToWebProcessConnection::~DatabaseToWebProcessConnection()
{

}

void DatabaseToWebProcessConnection::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::DatabaseToWebProcessConnection::messageReceiverName()) {
        didReceiveDatabaseToWebProcessConnectionMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::DatabaseProcessIDBConnection::messageReceiverName()) {
        IDBConnectionMap::iterator backendIterator = m_idbConnections.find(decoder.destinationID());
        if (backendIterator != m_idbConnections.end())
            backendIterator->value->didReceiveDatabaseProcessIDBConnectionMessage(connection, decoder);
        return;
    }

    ASSERT_NOT_REACHED();
}

void DatabaseToWebProcessConnection::didReceiveSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::DatabaseProcessIDBConnection::messageReceiverName()) {
        IDBConnectionMap::iterator backendIterator = m_idbConnections.find(decoder.destinationID());
        if (backendIterator != m_idbConnections.end())
            backendIterator->value->didReceiveSyncDatabaseProcessIDBConnectionMessage(connection, decoder, reply);
        return;
    }

    ASSERT_NOT_REACHED();
}

void DatabaseToWebProcessConnection::didClose(IPC::Connection*)
{
    // The WebProcess has disconnected, close all of the connections associated with it
    while (!m_idbConnections.isEmpty())
        removeDatabaseProcessIDBConnection(m_idbConnections.begin()->key);
}

void DatabaseToWebProcessConnection::didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{

}

void DatabaseToWebProcessConnection::establishIDBConnection(uint64_t serverConnectionIdentifier)
{
    RefPtr<DatabaseProcessIDBConnection> idbConnection = DatabaseProcessIDBConnection::create(*this, serverConnectionIdentifier);
    m_idbConnections.set(serverConnectionIdentifier, idbConnection.release());
}

void DatabaseToWebProcessConnection::removeDatabaseProcessIDBConnection(uint64_t serverConnectionIdentifier)
{
    ASSERT(m_idbConnections.contains(serverConnectionIdentifier));

    RefPtr<DatabaseProcessIDBConnection> idbConnection = m_idbConnections.take(serverConnectionIdentifier);
    idbConnection->disconnectedFromWebProcess();
}


} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
