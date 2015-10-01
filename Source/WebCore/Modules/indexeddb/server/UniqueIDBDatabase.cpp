/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "UniqueIDBDatabase.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBResultData.h"
#include "IDBServer.h"
#include "Logging.h"
#include <wtf/MainThread.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
namespace IDBServer {
    
UniqueIDBDatabase::UniqueIDBDatabase(IDBServer& server, const IDBDatabaseIdentifier& identifier)
    : m_server(server)
    , m_identifier(identifier)
{
}

void UniqueIDBDatabase::openDatabaseConnection(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    auto operation = IDBServerOperation::create(connection, requestData);
    m_pendingOpenDatabaseOperations.append(WTF::move(operation));

    if (m_databaseInfo) {
        handleOpenDatabaseOperations();
        return;
    }
    
    m_server.postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::openBackingStore, m_identifier));
}

void UniqueIDBDatabase::handleOpenDatabaseOperations()
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::handleOpenDatabaseOperations");

    auto operation = m_pendingOpenDatabaseOperations.takeFirst();

    // FIXME: Reporting open operations as failures for now.
    // Creating database connections for success will be the next step.

    IDBResultData result(operation->requestData().requestIdentifier(), IDBError(IDBExceptionCode::Unknown));
    operation->connection().didOpenDatabase(result);
}

void UniqueIDBDatabase::openBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::openBackingStore");

    ASSERT(!m_backingStore);
    m_backingStore = m_server.createBackingStore(identifier);
    auto databaseInfo = m_backingStore->getOrEstablishDatabaseInfo();

    m_server.postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didOpenBackingStore, databaseInfo));
}

void UniqueIDBDatabase::didOpenBackingStore(const IDBDatabaseInfo& info)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didOpenBackingStore");
    
    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(info);

    handleOpenDatabaseOperations();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
