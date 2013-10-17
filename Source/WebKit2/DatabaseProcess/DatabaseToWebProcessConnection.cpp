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

#include "DatabaseProcessIDBDatabaseBackend.h"
#include "DatabaseProcessIDBDatabaseBackendMessages.h"
#include "DatabaseToWebProcessConnectionMessages.h"
#include <WebCore/RunLoop.h>

#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

PassRefPtr<DatabaseToWebProcessConnection> DatabaseToWebProcessConnection::create(CoreIPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(new DatabaseToWebProcessConnection(connectionIdentifier));
}

DatabaseToWebProcessConnection::DatabaseToWebProcessConnection(CoreIPC::Connection::Identifier connectionIdentifier)
{
    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

DatabaseToWebProcessConnection::~DatabaseToWebProcessConnection()
{

}

void DatabaseToWebProcessConnection::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::DatabaseToWebProcessConnection::messageReceiverName()) {
        didReceiveDatabaseToWebProcessConnectionMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::DatabaseProcessIDBDatabaseBackend::messageReceiverName()) {
        IDBDatabaseBackendMap::iterator backendIterator = m_idbDatabaseBackends.find(decoder.destinationID());
        if (backendIterator != m_idbDatabaseBackends.end())
            backendIterator->value->didReceiveDatabaseProcessIDBDatabaseBackendMessage(connection, decoder);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

void DatabaseToWebProcessConnection::didClose(CoreIPC::Connection*)
{

}

void DatabaseToWebProcessConnection::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName)
{

}

void DatabaseToWebProcessConnection::establishIDBDatabaseBackend(uint64_t backendIdentifier)
{
    RefPtr<DatabaseProcessIDBDatabaseBackend> backend = DatabaseProcessIDBDatabaseBackend::create(backendIdentifier);
    m_idbDatabaseBackends.set(backendIdentifier, backend.release());
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
