/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "IDBCursorInfo.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetAllResult.h"
#include "IDBGetRecordData.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyRangeData.h"
#include "IDBResultData.h"
#include "IDBServer.h"
#include "IDBTransactionInfo.h"
#include "IDBValue.h"
#include "Logging.h"
#include "StorageQuotaManager.h"
#include "UniqueIDBDatabaseConnection.h"
#include <wtf/Scope.h>

namespace WebCore {
using namespace JSC;
namespace IDBServer {

static const uint64_t defaultWriteOperationCost = 4;

static inline uint64_t estimateSize(const IDBKeyData& keyData)
{
    uint64_t size = 4;
    switch (keyData.type()) {
    case IndexedDB::KeyType::String:
        size += keyData.string().sizeInBytes();
        break;
    case IndexedDB::KeyType::Binary: {
        size += keyData.binary().size();
        break;
    }
    case IndexedDB::KeyType::Array:
        for (auto& data : keyData.array())
            size += estimateSize(data);
        break;
    default:
        break;
    }
    return size;
}

static inline uint64_t estimateSize(const IDBValue& value)
{
    uint64_t size = 4;
    size += value.data().size();
    for (auto& url : value.blobURLs())
        size += url.sizeInBytes();
    for (auto& path : value.blobFilePaths())
        size += path.sizeInBytes();
    return size;
}

static inline uint64_t estimateSize(const IDBKeyPath& keyPath)
{
    return WTF::switchOn(keyPath, [](const String& path) {
        return static_cast<uint64_t>(path.sizeInBytes());
    }, [](const Vector<String>& paths) {
        uint64_t size = 0;
        for (auto path : paths)
            size += path.sizeInBytes();
        return size;
    });
}

static inline uint64_t estimateSize(const IDBIndexInfo& info)
{
    uint64_t size = 4;
    size += info.name().sizeInBytes();
    size += estimateSize(info.keyPath());
    return size;
}

static inline uint64_t estimateSize(const IDBObjectStoreInfo& info)
{
    uint64_t size = 4;
    size += info.name().sizeInBytes();
    if (auto keyPath = info.keyPath())
        size += estimateSize(*keyPath);
    return size;
}

UniqueIDBDatabase::UniqueIDBDatabase(IDBServer& server, const IDBDatabaseIdentifier& identifier)
    : m_server(server)
    , m_identifier(identifier)
{
    ASSERT(!isMainThread());

    m_server.addDatabase(*this);
    LOG(IndexedDB, "UniqueIDBDatabase::UniqueIDBDatabase() (%p) %s", this, m_identifier.loggingString().utf8().data());
}

UniqueIDBDatabase::~UniqueIDBDatabase()
{
    LOG(IndexedDB, "UniqueIDBDatabase::~UniqueIDBDatabase() (%p) %s", this, m_identifier.loggingString().utf8().data());
    ASSERT(!isMainThread());
    ASSERT(m_pendingOpenDBRequests.isEmpty());
    ASSERT(!m_currentOpenDBRequest);
    ASSERT(m_inProgressTransactions.isEmpty());
    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(!hasAnyOpenConnections());
    ASSERT(!m_versionChangeTransaction);
    ASSERT(!m_versionChangeDatabaseConnection);
    RELEASE_ASSERT(!m_backingStore);

    m_server.removeDatabase(*this);
}

const IDBDatabaseInfo& UniqueIDBDatabase::info() const
{
    RELEASE_ASSERT(m_databaseInfo);
    return *m_databaseInfo;
}

void UniqueIDBDatabase::openDatabaseConnection(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    LOG(IndexedDB, "UniqueIDBDatabase::openDatabaseConnection");
    ASSERT(!isMainThread());

    m_pendingOpenDBRequests.add(ServerOpenDBRequest::create(connection, requestData));

    handleDatabaseOperations();
}

static inline String quotaErrorMessageName(const char* taskName)
{
    return makeString("Failed to ", taskName, " in database because not enough space for domain");
}

void UniqueIDBDatabase::performCurrentOpenOperation()
{
    LOG(IndexedDB, "UniqueIDBDatabase::performCurrentOpenOperation (%p)", this);

    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isOpenRequest());

    IDBError backingStoreOpenError;
    if (!m_backingStore) {
        // Quota check.
        auto decision = m_server.requestSpace(m_identifier.origin(), defaultWriteOperationCost);
        if (decision == StorageQuotaManager::Decision::Deny)
            backingStoreOpenError = IDBError { QuotaExceededError, quotaErrorMessageName("OpenBackingStore") };
        else {
            m_backingStore = m_server.createBackingStore(m_identifier);
            IDBDatabaseInfo databaseInfo;
            backingStoreOpenError = m_backingStore->getOrEstablishDatabaseInfo(databaseInfo);
            if (backingStoreOpenError.isNull())
                m_databaseInfo = makeUnique<IDBDatabaseInfo>(databaseInfo);
            else
                m_backingStore = nullptr;
        }
    }

    if (!backingStoreOpenError.isNull()) {
        auto result = IDBResultData::error(m_currentOpenDBRequest->requestData().requestIdentifier(), backingStoreOpenError);
        m_currentOpenDBRequest->connection().didOpenDatabase(result);
        m_currentOpenDBRequest = nullptr;

        return;
    }

    // If we previously started a version change operation but were blocked by having open connections,
    // we might now be unblocked.
    if (m_versionChangeDatabaseConnection) {
        if (!m_versionChangeTransaction && !hasAnyOpenConnections())
            startVersionChangeTransaction();
        return;
    }

    // 3.3.1 Opening a database
    // If requested version is undefined, then let requested version be 1 if db was created in the previous step,
    // or the current version of db otherwise.
    uint64_t requestedVersion = m_currentOpenDBRequest->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    // 3.3.1 Opening a database
    // If the database version higher than the requested version, abort these steps and return a VersionError.
    if (requestedVersion < m_databaseInfo->version()) {
        auto result = IDBResultData::error(m_currentOpenDBRequest->requestData().requestIdentifier(), IDBError(VersionError));
        m_currentOpenDBRequest->connection().didOpenDatabase(result);
        m_currentOpenDBRequest = nullptr;

        return;
    }

    Ref<UniqueIDBDatabaseConnection> connection = UniqueIDBDatabaseConnection::create(*this, *m_currentOpenDBRequest);

    if (requestedVersion == m_databaseInfo->version()) {
        auto* rawConnection = &connection.get();
        addOpenDatabaseConnection(WTFMove(connection));

        auto result = IDBResultData::openDatabaseSuccess(m_currentOpenDBRequest->requestData().requestIdentifier(), *rawConnection);
        m_currentOpenDBRequest->connection().didOpenDatabase(result);
        m_currentOpenDBRequest = nullptr;

        return;
    }

    ASSERT(!m_versionChangeDatabaseConnection);
    m_versionChangeDatabaseConnection = WTFMove(connection);

    // 3.3.7 "versionchange" transaction steps
    // If there's no other open connections to this database, the version change process can begin immediately.
    if (!hasAnyOpenConnections()) {
        startVersionChangeTransaction();
        return;
    }

    // Otherwise we have to notify all those open connections and wait for them to close.
    maybeNotifyConnectionsOfVersionChange();
}

void UniqueIDBDatabase::performCurrentDeleteOperation()
{
    LOG(IndexedDB, "UniqueIDBDatabase::performCurrentDeleteOperation - %s", m_identifier.loggingString().utf8().data());

    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isDeleteRequest());

    if (hasAnyOpenConnections()) {
        maybeNotifyConnectionsOfVersionChange();
        return;
    }

    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(m_openDatabaseConnections.isEmpty());

    // It's possible to have multiple delete requests queued up in a row.
    // In that scenario only the first request will actually have to delete the database.
    // Subsequent requests can immediately notify their completion.

    if (!m_databaseInfo && m_mostRecentDeletedDatabaseInfo)
        didDeleteBackingStore(0);
    else
        deleteBackingStore();
}

void UniqueIDBDatabase::deleteBackingStore()
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteBackingStore");

    uint64_t deletedVersion = 0;

    if (m_backingStore) {
        m_backingStore->deleteBackingStore();
        m_backingStore = nullptr;
    } else {
        auto backingStore = m_server.createBackingStore(m_identifier);

        IDBDatabaseInfo databaseInfo;
        auto error = backingStore->getOrEstablishDatabaseInfo(databaseInfo);
        if (!error.isNull())
            LOG_ERROR("Error getting database info from database %s that we are trying to delete", m_identifier.loggingString().utf8().data());

        deletedVersion = databaseInfo.version();
        backingStore->deleteBackingStore();
    }

    didDeleteBackingStore(deletedVersion);
}

void UniqueIDBDatabase::didDeleteBackingStore(uint64_t deletedVersion)
{
    LOG(IndexedDB, "UniqueIDBDatabase::didDeleteBackingStore");

    ASSERT(m_inProgressTransactions.isEmpty());
    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(m_openDatabaseConnections.isEmpty());
    ASSERT(!m_backingStore);

    ASSERT(m_currentOpenDBRequest->isDeleteRequest());

    if (m_databaseInfo)
        m_mostRecentDeletedDatabaseInfo = WTFMove(m_databaseInfo);

    // If this UniqueIDBDatabase was brought into existence for the purpose of deleting the file on disk,
    // we won't have a m_mostRecentDeletedDatabaseInfo. In that case, we'll manufacture one using the
    // passed in deletedVersion argument.
    if (!m_mostRecentDeletedDatabaseInfo)
        m_mostRecentDeletedDatabaseInfo = makeUnique<IDBDatabaseInfo>(m_identifier.databaseName(), deletedVersion, 0);

    if (m_currentOpenDBRequest) {
        m_currentOpenDBRequest->notifyDidDeleteDatabase(*m_mostRecentDeletedDatabaseInfo);
        m_currentOpenDBRequest = nullptr;
    }
}

void UniqueIDBDatabase::clearStalePendingOpenDBRequests()
{
    while (!m_pendingOpenDBRequests.isEmpty() && m_pendingOpenDBRequests.first()->connection().isClosed())
        m_pendingOpenDBRequests.removeFirst();
}

void UniqueIDBDatabase::handleDatabaseOperations()
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::handleDatabaseOperations - There are %u pending", m_pendingOpenDBRequests.size());

    if (m_versionChangeDatabaseConnection || m_versionChangeTransaction) {
        // We can't start any new open-database operations right now, but we might be able to start handling a delete operation.
        if (!m_currentOpenDBRequest)
            m_currentOpenDBRequest = takeNextRunnableRequest(RequestType::Delete);
    } else if (!m_currentOpenDBRequest || m_currentOpenDBRequest->connection().isClosed())
        m_currentOpenDBRequest = takeNextRunnableRequest();

    while (m_currentOpenDBRequest) {
        handleCurrentOperation();
        if (!m_currentOpenDBRequest) {
            if (m_versionChangeTransaction)
                m_currentOpenDBRequest = takeNextRunnableRequest(RequestType::Delete);
            else
                m_currentOpenDBRequest = takeNextRunnableRequest();
        } else // Request need multiple attempts to handle.
            break;
    }
    LOG(IndexedDB, "UniqueIDBDatabase::handleDatabaseOperations - There are %u pending after this round of handling", m_pendingOpenDBRequests.size());
}

void UniqueIDBDatabase::handleCurrentOperation()
{
    LOG(IndexedDB, "UniqueIDBDatabase::handleCurrentOperation");
    ASSERT(m_currentOpenDBRequest);

    if (m_currentOpenDBRequest->isOpenRequest())
        performCurrentOpenOperation();
    else if (m_currentOpenDBRequest->isDeleteRequest())
        performCurrentDeleteOperation();
    else
        ASSERT_NOT_REACHED();
}

bool UniqueIDBDatabase::hasAnyOpenConnections() const
{
    return !m_openDatabaseConnections.isEmpty();
}

bool UniqueIDBDatabase::allConnectionsAreClosedOrClosing() const
{
    for (auto& connection : m_openDatabaseConnections) {
        if (!connection->connectionIsClosing())
            return false;
    }

    return true;
}

void UniqueIDBDatabase::handleDelete(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    LOG(IndexedDB, "UniqueIDBDatabase::handleDelete");

    m_pendingOpenDBRequests.add(ServerOpenDBRequest::create(connection, requestData));
    handleDatabaseOperations();
}

void UniqueIDBDatabase::startVersionChangeTransaction()
{
    LOG(IndexedDB, "UniqueIDBDatabase::startVersionChangeTransaction");

    ASSERT(!m_versionChangeTransaction);
    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isOpenRequest());
    ASSERT(m_versionChangeDatabaseConnection);

    uint64_t requestedVersion = m_currentOpenDBRequest->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    m_versionChangeTransaction = &m_versionChangeDatabaseConnection->createVersionChangeTransaction(requestedVersion);
    auto versionChangeTransactionInfo = m_versionChangeTransaction->info();
    m_inProgressTransactions.set(versionChangeTransactionInfo.identifier(), m_versionChangeTransaction);
    
    auto error = m_backingStore->beginTransaction(versionChangeTransactionInfo);
    auto operation = WTFMove(m_currentOpenDBRequest);
    IDBResultData result;
    if (error.isNull()) {
        addOpenDatabaseConnection(*m_versionChangeDatabaseConnection);
        m_databaseInfo->setVersion(versionChangeTransactionInfo.newVersion());
        result = IDBResultData::openDatabaseUpgradeNeeded(operation->requestData().requestIdentifier(), *m_versionChangeTransaction);
        operation->connection().didOpenDatabase(result);
    } else {
        m_versionChangeDatabaseConnection->abortTransactionWithoutCallback(*m_versionChangeTransaction);
        m_versionChangeDatabaseConnection = nullptr;
        result = IDBResultData::error(operation->requestData().requestIdentifier(), error);
        operation->connection().didOpenDatabase(result);
    }
}

void UniqueIDBDatabase::maybeNotifyConnectionsOfVersionChange()
{
    ASSERT(m_currentOpenDBRequest);

    if (m_currentOpenDBRequest->hasNotifiedConnectionsOfVersionChange())
        return;

    uint64_t newVersion = m_currentOpenDBRequest->isOpenRequest() ? m_currentOpenDBRequest->requestData().requestedVersion() : 0;
    auto requestIdentifier = m_currentOpenDBRequest->requestData().requestIdentifier();

    LOG(IndexedDB, "UniqueIDBDatabase::notifyConnectionsOfVersionChange - %" PRIu64, newVersion);

    // 3.3.7 "versionchange" transaction steps
    // Fire a versionchange event at each connection in m_openDatabaseConnections that is open.
    // The event must not be fired on connections which has the closePending flag set.
    HashSet<uint64_t> connectionIdentifiers;
    for (const auto& connection : m_openDatabaseConnections) {
        if (connection->closePending())
            continue;

        connection->fireVersionChangeEvent(requestIdentifier, newVersion);
        connectionIdentifiers.add(connection->identifier());
    }

    if (!connectionIdentifiers.isEmpty())
        m_currentOpenDBRequest->notifiedConnectionsOfVersionChange(WTFMove(connectionIdentifiers));
    else
        m_currentOpenDBRequest->maybeNotifyRequestBlocked(m_databaseInfo->version());
}

void UniqueIDBDatabase::notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(uint64_t connectionIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabase::notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent - %" PRIu64, connectionIdentifier);

    ASSERT(m_currentOpenDBRequest);

    m_currentOpenDBRequest->connectionClosedOrFiredVersionChangeEvent(connectionIdentifier);

    if (m_currentOpenDBRequest->hasConnectionsPendingVersionChangeEvent())
        return;

    if (!hasAnyOpenConnections() || allConnectionsAreClosedOrClosing()) {
        handleDatabaseOperations();
        return;
    }

    // Since all open connections have fired their version change events but not all of them have closed,
    // this request is officially blocked.
    m_currentOpenDBRequest->maybeNotifyRequestBlocked(m_databaseInfo->version());
}

void UniqueIDBDatabase::clearTransactionsOnConnection(UniqueIDBDatabaseConnection& connection)
{
    Deque<RefPtr<UniqueIDBDatabaseTransaction>> pendingTransactions;
    while (!m_pendingTransactions.isEmpty()) {
        auto transaction = m_pendingTransactions.takeFirst();
        if (&transaction->databaseConnection() != &connection)
            pendingTransactions.append(WTFMove(transaction));
        else
            connection.deleteTransaction(*transaction);
    }
    if (!pendingTransactions.isEmpty())
        m_pendingTransactions.swap(pendingTransactions);

    Deque<RefPtr<UniqueIDBDatabaseTransaction>> transactionsToAbort;
    for (auto& transaction : m_inProgressTransactions.values()) {
        if (&transaction->databaseConnection() == &connection)
            transactionsToAbort.append(transaction);
    }
    for (auto& transaction : transactionsToAbort)
        transaction->abortWithoutCallback();
}

void UniqueIDBDatabase::didFireVersionChangeEvent(UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosedOnBehalfOfServer)
{
    LOG(IndexedDB, "UniqueIDBDatabase::didFireVersionChangeEvent");

    if (!m_currentOpenDBRequest)
        return;

    ASSERT_UNUSED(requestIdentifier, m_currentOpenDBRequest->requestData().requestIdentifier() == requestIdentifier);

    if (connectionClosedOnBehalfOfServer == IndexedDB::ConnectionClosedOnBehalfOfServer::Yes) {
        if (m_openDatabaseConnections.contains(&connection)) {
            clearTransactionsOnConnection(connection);
            m_openDatabaseConnections.remove(&connection);
        }
    }

    notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(connection.identifier());
}

void UniqueIDBDatabase::openDBRequestCancelled(const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabase::openDBRequestCancelled - %s", requestIdentifier.loggingString().utf8().data());

    if (m_currentOpenDBRequest && m_currentOpenDBRequest->requestData().requestIdentifier() == requestIdentifier)
        m_currentOpenDBRequest = nullptr;

    if (m_versionChangeDatabaseConnection && m_versionChangeDatabaseConnection->openRequestIdentifier() == requestIdentifier) {
        ASSERT(!m_versionChangeTransaction || m_versionChangeTransaction->databaseConnection().openRequestIdentifier() == requestIdentifier);
        ASSERT(!m_versionChangeTransaction || &m_versionChangeTransaction->databaseConnection() == m_versionChangeDatabaseConnection);

        connectionClosedFromClient(*m_versionChangeDatabaseConnection);
    }

    for (auto& request : m_pendingOpenDBRequests) {
        if (request->requestData().requestIdentifier() == requestIdentifier) {
            m_pendingOpenDBRequests.remove(request);
            return;
        }
    }
}

void UniqueIDBDatabase::addOpenDatabaseConnection(Ref<UniqueIDBDatabaseConnection>&& connection)
{
    ASSERT(!m_openDatabaseConnections.contains(&connection.get()));
    m_openDatabaseConnections.add(adoptRef(connection.leakRef()));
}

void UniqueIDBDatabase::createObjectStore(UniqueIDBDatabaseTransaction& transaction, const IDBObjectStoreInfo& info, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::createObjectStore");

    ASSERT(m_backingStore);

    // Quota check.
    auto taskSize = defaultWriteOperationCost + estimateSize(info);
    if (m_server.requestSpace(m_identifier.origin(), taskSize) == StorageQuotaManager::Decision::Deny) {
        callback(IDBError { QuotaExceededError, quotaErrorMessageName("CreateObjectStore") });
        return;
    }

    auto error = m_backingStore->createObjectStore(transaction.info().identifier(), info);
    if (error.isNull())
        m_databaseInfo->addExistingObjectStore(info);

    callback(error);
}

void UniqueIDBDatabase::deleteObjectStore(UniqueIDBDatabaseTransaction& transaction, const String& objectStoreName, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteObjectStore");

    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreName);
    if (!info) {
        callback(IDBError { UnknownError, "Attempt to delete non-existant object store"_s });
        return;
    }

    ASSERT(m_backingStore);
    auto error = m_backingStore->deleteObjectStore(transaction.info().identifier(), info->identifier());
    if (error.isNull())
        m_databaseInfo->deleteObjectStore(info->identifier());

    callback(error);
}

void UniqueIDBDatabase::renameObjectStore(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, const String& newName, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::renameObjectStore");

    // Quota check.
    auto taskSize = defaultWriteOperationCost + newName.sizeInBytes();
    if (m_server.requestSpace(m_identifier.origin(), taskSize) == StorageQuotaManager::Decision::Deny) {
        callback(IDBError(QuotaExceededError, quotaErrorMessageName("RenameObjectStore")));
        return;
    }

    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!info) {
        callback(IDBError { UnknownError, "Attempt to rename non-existant object store"_s });
        return;
    }

    ASSERT(m_backingStore);
    auto error = m_backingStore->renameObjectStore(transaction.info().identifier(), objectStoreIdentifier, newName);
    if (error.isNull())
        m_databaseInfo->renameObjectStore(objectStoreIdentifier, newName);

    callback(error);
}

void UniqueIDBDatabase::clearObjectStore(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::clearObjectStore");

    ASSERT(m_backingStore);
    auto error = m_backingStore->clearObjectStore(transaction.info().identifier(), objectStoreIdentifier);

    callback(error);
}

void UniqueIDBDatabase::createIndex(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& info, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::createIndex");

    // Quota check.
    auto taskSize = defaultWriteOperationCost + estimateSize(info);
    if (m_server.requestSpace(m_identifier.origin(), taskSize) == StorageQuotaManager::Decision::Deny) {
        callback(IDBError { QuotaExceededError, quotaErrorMessageName("CreateIndex") });
        return;
    }

    ASSERT(m_backingStore);
    auto error = m_backingStore->createIndex(transaction.info().identifier(), info);
    if (error.isNull()) {
        ASSERT(m_databaseInfo);
        auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(info.objectStoreIdentifier());
        ASSERT(objectStoreInfo);
        objectStoreInfo->addExistingIndex(info);
        m_databaseInfo->setMaxIndexID(info.identifier());
    }

    callback(error);
}

void UniqueIDBDatabase::deleteIndex(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, const String& indexName, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteIndex");

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        callback(IDBError { UnknownError, "Attempt to delete index from non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexName);
    if (!indexInfo) {
        callback(IDBError { UnknownError, "Attempt to delete non-existant index"_s });
        return;
    }
    auto indexIdentifier = indexInfo->identifier();

    ASSERT(m_backingStore);
    auto error = m_backingStore->deleteIndex(transaction.info().identifier(), objectStoreIdentifier, indexIdentifier);
    if (error.isNull())
        objectStoreInfo->deleteIndex(indexIdentifier);

    callback(error);
}

void UniqueIDBDatabase::renameIndex(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::renameIndex");

    // Quota check.
    auto taskSize = defaultWriteOperationCost + newName.sizeInBytes();
    if (m_server.requestSpace(m_identifier.origin(), taskSize) == StorageQuotaManager::Decision::Deny) {
        callback(IDBError { QuotaExceededError, quotaErrorMessageName("RenameIndex") });
        return;
    }

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        callback(IDBError { UnknownError, "Attempt to rename index in non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo) {
        callback(IDBError { UnknownError, "Attempt to rename non-existant index"_s });
        return;
    }

    ASSERT(m_backingStore);
    auto error = m_backingStore->renameIndex(transaction.info().identifier(), objectStoreIdentifier, indexIdentifier, newName);
    if (error.isNull())
        indexInfo->rename(newName);

    callback(error);
}

void UniqueIDBDatabase::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode, KeyDataCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::putOrAdd");

    ASSERT(m_databaseInfo);

    IDBKeyData usedKey;
    IDBError error;

    ASSERT(m_backingStore);
    auto objectStoreIdentifier = requestData.objectStoreIdentifier();
    auto* objectStoreInfo = m_backingStore->infoForObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        callback(IDBError { InvalidStateError, "Object store cannot be found in the backing store"_s }, usedKey);
        return;
    }

    // Quota check.
    auto taskSize = defaultWriteOperationCost + estimateSize(keyData) + estimateSize(value);
    auto* objectStore = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (objectStore)
        taskSize += objectStore->indexNames().size() * taskSize;
    if (m_server.requestSpace(m_identifier.origin(), taskSize) == StorageQuotaManager::Decision::Deny) {
        callback(IDBError { QuotaExceededError, quotaErrorMessageName("PutOrAdd") }, usedKey);
        return;
    }

    bool usedKeyIsGenerated = false;
    uint64_t keyNumber;
    auto transactionIdentifier = requestData.transactionIdentifier();
    auto generatedKeyResetter = WTF::makeScopeExit([this, transactionIdentifier, objectStoreIdentifier, &keyNumber, &usedKeyIsGenerated]() {
        if (usedKeyIsGenerated)
            m_backingStore->revertGeneratedKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
    });

    if (objectStoreInfo->autoIncrement() && !keyData.isValid()) {
        error = m_backingStore->generateKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
        if (!error.isNull()) {
            callback(error, usedKey);
            return;
        }
        usedKey.setNumberValue(keyNumber);
        usedKeyIsGenerated = true;
    } else
        usedKey = keyData;

    if (overwriteMode == IndexedDB::ObjectStoreOverwriteMode::NoOverwrite) {
        bool keyExists;
        error = m_backingStore->keyExistsInObjectStore(transactionIdentifier, objectStoreIdentifier, usedKey, keyExists);
        if (error.isNull() && keyExists)
            error = IDBError { ConstraintError, "Key already exists in the object store"_s };

        if (!error.isNull()) {
            callback(error, usedKey);
            return;
        }
    }
    // If a record already exists in store, then remove the record from store using the steps for deleting records from an object store.
    // This is important because formally deleting it from from the object store also removes it from the appropriate indexes.
    error = m_backingStore->deleteRange(transactionIdentifier, objectStoreIdentifier, usedKey);
    if (!error.isNull()) {
        callback(error, usedKey);
        return;
    }

    error = m_backingStore->addRecord(transactionIdentifier, *objectStoreInfo, usedKey, value);
    if (!error.isNull()) {
        callback(error, usedKey);
        return;
    }

    if (overwriteMode != IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor && objectStoreInfo->autoIncrement() && keyData.type() == IndexedDB::KeyType::Number)
        error = m_backingStore->maybeUpdateKeyGeneratorNumber(transactionIdentifier, objectStoreIdentifier, keyData.number());

    generatedKeyResetter.release();
    callback(error, usedKey);
}

void UniqueIDBDatabase::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData, GetResultCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getRecord");

    IDBGetResult result;
    IDBError error;

    ASSERT(m_backingStore);
    if (uint64_t indexIdentifier = requestData.indexIdentifier())
        error = m_backingStore->getIndexRecord(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), indexIdentifier, requestData.indexRecordType(), getRecordData.keyRangeData, result);
    else
        error = m_backingStore->getRecord(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), getRecordData.keyRangeData, getRecordData.type, result);

    callback(error, result);
}

void UniqueIDBDatabase::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData, GetAllResultsCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getAllRecords");

    ASSERT(m_backingStore);
    IDBGetAllResult result;
    auto error = m_backingStore->getAllRecords(requestData.transactionIdentifier(), getAllRecordsData, result);

    callback(error, result);
}

void UniqueIDBDatabase::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& range, CountCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getCount");

    ASSERT(m_backingStore);
    uint64_t count = 0;
    auto error = m_backingStore->getCount(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), requestData.indexIdentifier(), range, count);

    callback(error, count);
}

void UniqueIDBDatabase::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteRecord");

    ASSERT(m_backingStore);
    auto error = m_backingStore->deleteRange(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), keyRangeData);

    callback(error);
}

void UniqueIDBDatabase::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info, GetResultCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::openCursor");

    ASSERT(m_backingStore);

    IDBGetResult result;
    auto error = m_backingStore->openCursor(requestData.transactionIdentifier(), info, result);

    callback(error, result);
}

void UniqueIDBDatabase::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data, GetResultCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::iterateCursor");

    ASSERT(m_backingStore);

    IDBGetResult result;
    auto transactionIdentifier = requestData.transactionIdentifier();
    auto cursorIdentifier = requestData.cursorIdentifier();
    auto error = m_backingStore->iterateCursor(transactionIdentifier, cursorIdentifier, data, result);

    callback(error, result);
}

void UniqueIDBDatabase::commitTransaction(UniqueIDBDatabaseTransaction& transaction, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::commitTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(transaction.databaseConnection().database() == this);
    ASSERT(m_backingStore);
    auto takenTransaction = m_inProgressTransactions.take(transaction.info().identifier());
    if (!takenTransaction) {
        if (!m_openDatabaseConnections.contains(&transaction.databaseConnection()))
            return;

        callback(IDBError { UnknownError, "Attempt to commit transaction that is not running"_s });
        return;
    }

    auto error = m_backingStore->commitTransaction(transaction.info().identifier());

    callback(error);
    transactionCompleted(WTFMove(takenTransaction));
}

void UniqueIDBDatabase::abortTransaction(UniqueIDBDatabaseTransaction& transaction, ErrorCallback callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::abortTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(transaction.databaseConnection().database() == this);

    auto takenTransaction = m_inProgressTransactions.take(transaction.info().identifier());
    if (!takenTransaction) {
        if (!m_openDatabaseConnections.contains(&transaction.databaseConnection()))
            return;

        callback(IDBError { UnknownError, "Attempt to abort transaction that is not running"_s });
        return;
    }

    // If transaction is already aborted on the main thread for suspension,
    // return the result of that abort.
    if (auto existingAbortResult = takenTransaction->mainThreadAbortResult()) {
        callback(*existingAbortResult);
        transactionCompleted(WTFMove(takenTransaction));
        return;
    }

    auto transactionIdentifier = transaction.info().identifier();
    if (m_versionChangeTransaction && m_versionChangeTransaction->info().identifier() == transactionIdentifier) {
        ASSERT(m_versionChangeTransaction == &transaction);
        ASSERT(!m_versionChangeDatabaseConnection || &m_versionChangeTransaction->databaseConnection() == m_versionChangeDatabaseConnection);
        ASSERT(m_versionChangeTransaction->originalDatabaseInfo());
        m_databaseInfo = makeUnique<IDBDatabaseInfo>(*m_versionChangeTransaction->originalDatabaseInfo());
    }

    auto error = m_backingStore->abortTransaction(transactionIdentifier);

    callback(error);
    transactionCompleted(WTFMove(takenTransaction));
}

void UniqueIDBDatabase::didFinishHandlingVersionChange(UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::didFinishHandlingVersionChange");

    ASSERT_UNUSED(transactionIdentifier, !m_versionChangeTransaction || m_versionChangeTransaction->info().identifier() == transactionIdentifier);
    ASSERT_UNUSED(connection, !m_versionChangeDatabaseConnection || m_versionChangeDatabaseConnection.get() == &connection);

    m_versionChangeTransaction = nullptr;
    m_versionChangeDatabaseConnection = nullptr;

    handleDatabaseOperations();
    handleTransactions();
}

void UniqueIDBDatabase::connectionClosedFromClient(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::connectionClosedFromClient - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier());

    Ref<UniqueIDBDatabaseConnection> protectedConnection(connection);
    m_openDatabaseConnections.remove(&connection);

    if (m_versionChangeDatabaseConnection == &connection) {
        m_versionChangeDatabaseConnection = nullptr;
        if (m_versionChangeTransaction) {
            connection.abortTransactionWithoutCallback(*m_versionChangeTransaction);
            ASSERT(!connection.hasNonFinishedTransactions());

            // Previous blocked operations or transactions may be runnable.
            handleDatabaseOperations();
            handleTransactions();

            return;
        }
    }

    // Remove all pending transactions on the connection.
    clearTransactionsOnConnection(connection);

    if (m_currentOpenDBRequest)
        notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(connection.identifier());

    ASSERT(!connection.hasNonFinishedTransactions());

    // Now that a database connection has closed, previously blocked operations might be runnable.
    handleDatabaseOperations();
    handleTransactions();
}

void UniqueIDBDatabase::connectionClosedFromServer(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::connectionClosedFromServer - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier());

    connection.connectionToClient().didCloseFromServer(connection, IDBError::userDeleteError());

    m_openDatabaseConnections.remove(&connection);
}

void UniqueIDBDatabase::enqueueTransaction(Ref<UniqueIDBDatabaseTransaction>&& transaction)
{
    LOG(IndexedDB, "UniqueIDBDatabase::enqueueTransaction - %s", transaction->info().loggingString().utf8().data());

    ASSERT(transaction->info().mode() != IDBTransactionMode::Versionchange);

    m_pendingTransactions.append(WTFMove(transaction));

    handleTransactions();
}

void UniqueIDBDatabase::handleTransactions()
{
    LOG(IndexedDB, "UniqueIDBDatabase::handleTransactions - There are %zu pending", m_pendingTransactions.size());

    bool hadDeferredTransactions = false;
    auto transaction = takeNextRunnableTransaction(hadDeferredTransactions);

    while (transaction) {
        m_inProgressTransactions.set(transaction->info().identifier(), transaction);
        for (auto objectStore : transaction->objectStoreIdentifiers()) {
            m_objectStoreTransactionCounts.add(objectStore);
            if (!transaction->isReadOnly()) {
                m_objectStoreWriteTransactions.add(objectStore);
                ASSERT(m_objectStoreTransactionCounts.count(objectStore) == 1);
            }
        }

        activateTransactionInBackingStore(*transaction);
        if (hadDeferredTransactions)
            break;
        transaction = takeNextRunnableTransaction(hadDeferredTransactions);
    }
    LOG(IndexedDB, "UniqueIDBDatabase::handleTransactions - There are %zu pending after this round of handling", m_pendingTransactions.size());
}

void UniqueIDBDatabase::activateTransactionInBackingStore(UniqueIDBDatabaseTransaction& transaction)
{
    LOG(IndexedDB, "UniqueIDBDatabase::activateTransactionInBackingStore");

    ASSERT(m_backingStore);

    auto error = m_backingStore->beginTransaction(transaction.info());

    transaction.didActivateInBackingStore(error);
}

template<typename T> bool scopesOverlap(const T& aScopes, const Vector<uint64_t>& bScopes)
{
    for (auto scope : bScopes) {
        if (aScopes.contains(scope))
            return true;
    }

    return false;
}

RefPtr<UniqueIDBDatabaseTransaction> UniqueIDBDatabase::takeNextRunnableTransaction(bool& hadDeferredTransactions)
{
    hadDeferredTransactions = false;

    if (m_pendingTransactions.isEmpty())
        return nullptr;

    if (!m_backingStore->supportsSimultaneousTransactions() && !m_inProgressTransactions.isEmpty()) {
        LOG(IndexedDB, "UniqueIDBDatabase::takeNextRunnableTransaction - Backing store only supports 1 transaction, and we already have 1");
        return nullptr;
    }

    Deque<RefPtr<UniqueIDBDatabaseTransaction>> deferredTransactions;
    RefPtr<UniqueIDBDatabaseTransaction> currentTransaction;

    HashSet<uint64_t> deferredReadWriteScopes;

    while (!m_pendingTransactions.isEmpty()) {
        currentTransaction = m_pendingTransactions.takeFirst();

        switch (currentTransaction->info().mode()) {
        case IDBTransactionMode::Readonly: {
            bool hasOverlappingScopes = scopesOverlap(deferredReadWriteScopes, currentTransaction->objectStoreIdentifiers());
            hasOverlappingScopes |= scopesOverlap(m_objectStoreWriteTransactions, currentTransaction->objectStoreIdentifiers());

            if (hasOverlappingScopes)
                deferredTransactions.append(WTFMove(currentTransaction));

            break;
        }
        case IDBTransactionMode::Readwrite: {
            bool hasOverlappingScopes = scopesOverlap(m_objectStoreTransactionCounts, currentTransaction->objectStoreIdentifiers());
            hasOverlappingScopes |= scopesOverlap(deferredReadWriteScopes, currentTransaction->objectStoreIdentifiers());

            if (hasOverlappingScopes) {
                for (auto objectStore : currentTransaction->objectStoreIdentifiers())
                    deferredReadWriteScopes.add(objectStore);
                deferredTransactions.append(WTFMove(currentTransaction));
            }

            break;
        }
        case IDBTransactionMode::Versionchange:
            // Version change transactions should never be scheduled in the traditional manner.
            RELEASE_ASSERT_NOT_REACHED();
        }

        // If we didn't defer the currentTransaction above, it can be run now.
        if (currentTransaction)
            break;
    }

    hadDeferredTransactions = !deferredTransactions.isEmpty();
    if (!hadDeferredTransactions)
        return currentTransaction;

    // Prepend the deferred transactions back on the beginning of the deque for future scheduling passes.
    while (!deferredTransactions.isEmpty())
        m_pendingTransactions.prepend(deferredTransactions.takeLast());

    return currentTransaction;
}

void UniqueIDBDatabase::transactionCompleted(RefPtr<UniqueIDBDatabaseTransaction>&& transaction)
{
    ASSERT(transaction);
    ASSERT(!m_inProgressTransactions.contains(transaction->info().identifier()));
    ASSERT(!isMainThread());

    for (auto objectStore : transaction->objectStoreIdentifiers()) {
        if (!transaction->isReadOnly()) {
            m_objectStoreWriteTransactions.remove(objectStore);
            ASSERT(m_objectStoreTransactionCounts.count(objectStore) == 1);
        }
        m_objectStoreTransactionCounts.remove(objectStore);
    }

    if (m_versionChangeTransaction == transaction)
        m_versionChangeTransaction = nullptr;

    // Previously blocked operations might be runnable.
    handleDatabaseOperations();
    handleTransactions();
}

static void errorOpenDBRequestForUserDelete(ServerOpenDBRequest& request)
{
    auto result = IDBResultData::error(request.requestData().requestIdentifier(), IDBError::userDeleteError());
    if (request.isOpenRequest())
        request.connection().didOpenDatabase(result);
    else
        request.connection().didDeleteDatabase(result);
}

void UniqueIDBDatabase::immediateCloseForUserDelete()
{
    LOG(IndexedDB, "UniqueIDBDatabase::immediateCloseForUserDelete");

    // Error out all transactions.
    // Pending transactions must be cleared before in-progress transactions,
    // or they may get started right away after aborting in-progress transactions.
    for (auto& transaction : m_pendingTransactions)
        transaction->databaseConnection().deleteTransaction(*transaction);
    m_pendingTransactions.clear();

    for (auto& identifier : copyToVector(m_inProgressTransactions.keys()))
        m_inProgressTransactions.get(identifier)->abortWithoutCallback();

    ASSERT(m_inProgressTransactions.isEmpty());

    m_objectStoreTransactionCounts.clear();
    m_objectStoreWriteTransactions.clear();

    // Error out all IDBOpenDBRequests
    if (m_currentOpenDBRequest) {
        errorOpenDBRequestForUserDelete(*m_currentOpenDBRequest);
        m_currentOpenDBRequest = nullptr;
    }

    for (auto& request : m_pendingOpenDBRequests)
        errorOpenDBRequestForUserDelete(*request);

    m_pendingOpenDBRequests.clear();

    // Close all open connections
    auto openDatabaseConnections = m_openDatabaseConnections;
    for (auto& connection : openDatabaseConnections)
        connectionClosedFromServer(*connection);

    if (m_versionChangeDatabaseConnection) {
        connectionClosedFromServer(*m_versionChangeDatabaseConnection);
        m_versionChangeDatabaseConnection = nullptr;
    }

    ASSERT(!hasAnyOpenConnections());

    close();
}

void UniqueIDBDatabase::abortActiveTransactions()
{
    ASSERT(isMainThread());
    ASSERT(m_server.lock().isHeld());

    for (auto& identifier : copyToVector(m_inProgressTransactions.keys())) {
        auto transaction = m_inProgressTransactions.get(identifier);
        transaction->setMainThreadAbortResult(m_backingStore->abortTransaction(transaction->info().identifier()));
    }
}

void UniqueIDBDatabase::close()
{
    LOG(IndexedDB, "UniqueIDBDatabase::close");

    if (m_backingStore) {
        m_backingStore->close();
        m_backingStore = nullptr;
    }
}

RefPtr<ServerOpenDBRequest> UniqueIDBDatabase::takeNextRunnableRequest(RequestType requestType)
{
    // Connection of request may be closed or lost.
    clearStalePendingOpenDBRequests();

    if (!m_pendingOpenDBRequests.isEmpty()) {
        if (requestType == RequestType::Delete && !m_pendingOpenDBRequests.first()->isDeleteRequest())
            return nullptr;
        return m_pendingOpenDBRequests.takeFirst();
    }

    return nullptr;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
