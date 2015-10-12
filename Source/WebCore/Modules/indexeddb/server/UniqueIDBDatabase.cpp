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
#include "UniqueIDBDatabaseConnection.h"
#include <wtf/MainThread.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
namespace IDBServer {
    
UniqueIDBDatabase::UniqueIDBDatabase(IDBServer& server, const IDBDatabaseIdentifier& identifier)
    : m_server(server)
    , m_identifier(identifier)
{
}

const IDBDatabaseInfo& UniqueIDBDatabase::info() const
{
    RELEASE_ASSERT(m_databaseInfo);
    return *m_databaseInfo;
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

    // 3.3.1 Opening a database
    // If requested version is undefined, then let requested version be 1 if db was created in the previous step,
    // or the current version of db otherwise.
    uint64_t requestedVersion = operation->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    // 3.3.1 Opening a database
    // If the database version higher than the requested version, abort these steps and return a VersionError.
    if (requestedVersion < m_databaseInfo->version()) {
        auto result = IDBResultData::error(operation->requestData().requestIdentifier(), IDBError(IDBExceptionCode::VersionError));
        operation->connection().didOpenDatabase(result);
        return;
    }

    Ref<UniqueIDBDatabaseConnection> connection = UniqueIDBDatabaseConnection::create(*this, operation->connection());
    UniqueIDBDatabaseConnection* rawConnection = &connection.get();
    m_server.registerDatabaseConnection(*rawConnection);

    if (requestedVersion == m_databaseInfo->version()) {
        addOpenDatabaseConnection(WTF::move(connection));

        auto result = IDBResultData::openDatabaseSuccess(operation->requestData().requestIdentifier(), *rawConnection);
        operation->connection().didOpenDatabase(result);
        return;
    }

    ASSERT(!m_versionChangeOperation);
    ASSERT(!m_versionChangeDatabaseConnection);

    m_versionChangeOperation = adoptRef(operation.leakRef());
    m_versionChangeDatabaseConnection = rawConnection;

    // 3.3.7 "versionchange" transaction steps
    // If there's no other open connections to this database, the version change process can begin immediately.
    if (!hasAnyOpenConnections()) {
        startVersionChangeTransaction();
        return;
    }

    // Otherwise we have to notify all those open connections and wait for them to close.
    notifyConnectionsOfVersionChange();
}

bool UniqueIDBDatabase::hasAnyOpenConnections() const
{
    return !m_openDatabaseConnections.isEmpty();
}

void UniqueIDBDatabase::startVersionChangeTransaction()
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::startVersionChangeTransaction");

    ASSERT(!m_versionChangeTransaction);
    ASSERT(m_versionChangeOperation);
    ASSERT(m_versionChangeDatabaseConnection);

    auto operation = m_versionChangeOperation;
    m_versionChangeOperation = nullptr;

    uint64_t requestedVersion = operation->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    addOpenDatabaseConnection(*m_versionChangeDatabaseConnection);

    m_versionChangeTransaction = m_versionChangeDatabaseConnection->createVersionChangeTransaction(requestedVersion);
    m_inProgressTransactions.set(m_versionChangeTransaction->info().identifier(), m_versionChangeTransaction);

    auto result = IDBResultData::openDatabaseUpgradeNeeded(operation->requestData().requestIdentifier(), *m_versionChangeTransaction);
    operation->connection().didOpenDatabase(result);
}

void UniqueIDBDatabase::notifyConnectionsOfVersionChange()
{
    ASSERT(m_versionChangeOperation);
    ASSERT(m_versionChangeDatabaseConnection);

    uint64_t requestedVersion = m_versionChangeOperation->requestData().requestedVersion();

    LOG(IndexedDB, "(main) UniqueIDBDatabase::notifyConnectionsOfVersionChange - %llu", requestedVersion);

    // 3.3.7 "versionchange" transaction steps
    // Fire a versionchange event at each connection in m_openDatabaseConnections that is open.
    // The event must not be fired on connections which has the closePending flag set.
    for (auto connection : m_openDatabaseConnections) {
        if (connection->closePending())
            continue;

        connection->fireVersionChangeEvent(requestedVersion);
    }
}

void UniqueIDBDatabase::addOpenDatabaseConnection(Ref<UniqueIDBDatabaseConnection>&& connection)
{
    ASSERT(!m_openDatabaseConnections.contains(&connection.get()));
    m_openDatabaseConnections.add(adoptRef(connection.leakRef()));
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
