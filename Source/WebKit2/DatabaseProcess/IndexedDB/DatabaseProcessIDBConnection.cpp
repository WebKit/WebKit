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
#include "DatabaseProcessIDBConnection.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "DatabaseProcess.h"
#include "DatabaseToWebProcessConnection.h"
#include "IDBTransactionIdentifier.h"
#include "UniqueIDBDatabase.h"
#include "WebCoreArgumentCoders.h"
#include "WebIDBServerConnectionMessages.h"
#include <WebCore/IDBDatabaseMetadata.h>

using namespace WebCore;

namespace WebKit {

DatabaseProcessIDBConnection::DatabaseProcessIDBConnection(DatabaseToWebProcessConnection& connection, uint64_t serverConnectionIdentifier)
    : m_connection(connection)
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
{
}

DatabaseProcessIDBConnection::~DatabaseProcessIDBConnection()
{
    ASSERT(!m_uniqueIDBDatabase);
}

void DatabaseProcessIDBConnection::disconnectedFromWebProcess()
{
    m_uniqueIDBDatabase->unregisterConnection(*this);
    m_uniqueIDBDatabase.clear();
}

void DatabaseProcessIDBConnection::establishConnection(const String& databaseName, const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin)
{
    m_uniqueIDBDatabase = DatabaseProcess::shared().getOrCreateUniqueIDBDatabase(UniqueIDBDatabaseIdentifier(databaseName, openingOrigin, mainFrameOrigin));
    m_uniqueIDBDatabase->registerConnection(*this);
}

void DatabaseProcessIDBConnection::getOrEstablishIDBDatabaseMetadata(uint64_t requestID)
{
    ASSERT(m_uniqueIDBDatabase);

    RefPtr<DatabaseProcessIDBConnection> connection(this);
    m_uniqueIDBDatabase->getOrEstablishIDBDatabaseMetadata([connection, requestID](bool success, const IDBDatabaseMetadata& metadata) {
        connection->send(Messages::WebIDBServerConnection::DidGetOrEstablishIDBDatabaseMetadata(requestID, success, metadata));
    });
}

void DatabaseProcessIDBConnection::openTransaction(uint64_t requestID, int64_t transactionID, int64_t)
{
    ASSERT(m_uniqueIDBDatabase);

    RefPtr<DatabaseProcessIDBConnection> connection(this);
    m_uniqueIDBDatabase->openTransaction(IDBTransactionIdentifier(*this, transactionID), [connection, requestID](bool success) {
        connection->send(Messages::WebIDBServerConnection::DidOpenTransaction(requestID, success));
    });
}

CoreIPC::Connection* DatabaseProcessIDBConnection::messageSenderConnection()
{
    return m_connection->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
