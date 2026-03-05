/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "IDBBindingUtilities.h"
#include "IDBCursorInfo.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetAllResult.h"
#include "IDBGetRecordData.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenRequestData.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "IDBSerialization.h"
#include "IDBServer.h"
#include "IDBTransactionInfo.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "Logging.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseManager.h"
#include <algorithm>
#include <wtf/CompletionHandler.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

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

static inline uint64_t estimateSize(const IDBObjectStoreInfo& info, const IndexIDToIndexKeyMap& indexKeys, uint64_t primaryKeySize)
{
    // Each IndexRecord has 5 columns:
    //  - indexID, objectStoreID, recordID: these are varints, estimate 4 bytes each = 12 bytes
    //  - primary key: use primaryKeySize
    //  - secondary index key: use estimateSize(secondary key)
    static constexpr uint64_t baseIndexRowSize = 12;
    uint64_t size = 0;

    for (const auto& entry : indexKeys) {
        auto indexIterator = info.indexMap().find(entry.key);
        ASSERT(indexIterator != info.indexMap().end());

        if (indexIterator != info.indexMap().end() && indexIterator->value.multiEntry()) {
            for (const auto& secondaryKey : entry.value.multiEntry())
                size += (baseIndexRowSize + primaryKeySize + estimateSize(secondaryKey));
        } else
            size += (baseIndexRowSize + primaryKeySize + estimateSize(entry.value.asOneKey()));
    }

    return size;
}

static inline uint64_t NODELETE estimateSize(const IDBValue& value)
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(UniqueIDBDatabase);

UniqueIDBDatabase::UniqueIDBDatabase(UniqueIDBDatabaseManager& manager, const IDBDatabaseIdentifier& identifier)
    : m_manager(manager)
    , m_identifier(identifier)
{
    ASSERT(!isMainThread());

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
}

const IDBDatabaseInfo& UniqueIDBDatabase::info() const
{
    RELEASE_ASSERT(m_databaseInfo);
    return *m_databaseInfo;
}

UniqueIDBDatabaseManager* UniqueIDBDatabase::manager()
{
    return m_manager.get();
}

void UniqueIDBDatabase::openDatabaseConnection(IDBConnectionToClient& connection, const IDBOpenRequestData& requestData)
{
    LOG(IndexedDB, "UniqueIDBDatabase::openDatabaseConnection");
    ASSERT(!isMainThread());

    m_pendingOpenDBRequests.add(ServerOpenDBRequest::create(connection, requestData));

    handleDatabaseOperations();
}

static inline String quotaErrorMessageName(ASCIILiteral taskName)
{
    return makeString("Failed to "_s, taskName, " in database because not enough space for domain"_s);
}

void UniqueIDBDatabase::performCurrentOpenOperation()
{
    LOG(IndexedDB, "UniqueIDBDatabase::performCurrentOpenOperation (%p)", this);

    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isOpenRequest());

    // We don't need to create the file so no need to do space check.
    if (m_backingStore)
        return performCurrentOpenOperationAfterSpaceCheck(true);

    CheckedPtr manager = m_manager.get();
    if (!manager)
        return performCurrentOpenOperationAfterSpaceCheck(false);

    auto requestIdentifier = m_currentOpenDBRequest->requestData().requestIdentifier();
    if (m_openDBRequestsForSpaceCheck.contains(requestIdentifier))
        return;

    m_openDBRequestsForSpaceCheck.add(requestIdentifier);
    manager->requestSpace(m_identifier.origin(), defaultWriteOperationCost, [weakThis = WeakPtr { *this }, requestIdentifier](bool granted) mutable {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;

        checkedThis->m_openDBRequestsForSpaceCheck.remove(requestIdentifier);
        if (checkedThis->m_currentOpenDBRequest->requestData().requestIdentifier() != requestIdentifier)
            return;

        checkedThis->performCurrentOpenOperationAfterSpaceCheck(granted);
    });
}

void UniqueIDBDatabase::performCurrentOpenOperationAfterSpaceCheck(bool isGranted)
{
    LOG(IndexedDB, "UniqueIDBDatabase::performCurrentOpenOperationAfterSpaceRequest (%p)", this);

    IDBError backingStoreOpenError;
    if (!m_backingStore) {
        if (!m_manager)
            backingStoreOpenError = IDBError { ExceptionCode::InvalidStateError };
        else if (!isGranted)
            backingStoreOpenError = IDBError { ExceptionCode::QuotaExceededError, quotaErrorMessageName("OpenBackingStore"_s) };
        else {
            m_backingStore = CheckedRef { *m_manager }->createBackingStore(m_identifier);
            IDBDatabaseInfo databaseInfo;
            backingStoreOpenError = protect(m_backingStore)->getOrEstablishDatabaseInfo(databaseInfo);
            if (!backingStoreOpenError)
                m_databaseInfo = makeUnique<IDBDatabaseInfo>(databaseInfo);
            else {
                LOG_ERROR("Failed to get database info '%s'", backingStoreOpenError.message().utf8().data());
                m_backingStore = nullptr;
            }
        }
    }

    RefPtr currentOpenDBRequest = m_currentOpenDBRequest;
    if (backingStoreOpenError) {
        auto result = IDBResultData::error(currentOpenDBRequest->requestData().requestIdentifier(), backingStoreOpenError);
        currentOpenDBRequest->didOpenDatabase(result);
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
    uint64_t requestedVersion = currentOpenDBRequest->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    // 3.3.1 Opening a database
    // If the database version higher than the requested version, abort these steps and return a VersionError.
    if (requestedVersion < m_databaseInfo->version()) {
        auto result = IDBResultData::error(currentOpenDBRequest->requestData().requestIdentifier(), IDBError(ExceptionCode::VersionError));
        currentOpenDBRequest->didOpenDatabase(result);
        m_currentOpenDBRequest = nullptr;
        return;
    }

    Ref connection = UniqueIDBDatabaseConnection::create(*this, *currentOpenDBRequest);

    if (requestedVersion == m_databaseInfo->version()) {
        auto* rawConnection = &connection.get();
        addOpenDatabaseConnection(WTF::move(connection));

        auto result = IDBResultData::openDatabaseSuccess(currentOpenDBRequest->requestData().requestIdentifier(), *rawConnection);
        currentOpenDBRequest->didOpenDatabase(result);
        m_currentOpenDBRequest = nullptr;
        return;
    }

    ASSERT(!m_versionChangeDatabaseConnection);
    m_versionChangeDatabaseConnection = WTF::move(connection);

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
    
    auto backingStore = m_backingStore ? std::exchange(m_backingStore, nullptr) : CheckedRef { *m_manager }->createBackingStore(m_identifier);
    uint64_t deletedVersion = backingStore->databaseVersion();
    backingStore->deleteBackingStore();
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
        m_mostRecentDeletedDatabaseInfo = std::exchange(m_databaseInfo, nullptr);

    // If this UniqueIDBDatabase was brought into existence for the purpose of deleting the file on disk,
    // we won't have a m_mostRecentDeletedDatabaseInfo. In that case, we'll manufacture one using the
    // passed in deletedVersion argument.
    if (!m_mostRecentDeletedDatabaseInfo)
        m_mostRecentDeletedDatabaseInfo = makeUnique<IDBDatabaseInfo>(m_identifier.databaseName(), deletedVersion, 0);

    if (RefPtr request = std::exchange(m_currentOpenDBRequest, nullptr))
        request->didDeleteDatabase(IDBResultData::deleteDatabaseSuccess(request->requestData().requestIdentifier(), *m_mostRecentDeletedDatabaseInfo));
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

    if (!m_currentOpenDBRequest && (m_versionChangeDatabaseConnection || m_versionChangeTransaction))
        return;

    if (!m_currentOpenDBRequest || m_currentOpenDBRequest->connection().isClosed())
        m_currentOpenDBRequest = takeNextRunnableRequest();

    while (m_currentOpenDBRequest) {
        handleCurrentOperation();
        if (m_versionChangeTransaction || m_currentOpenDBRequest)
            break;

        m_currentOpenDBRequest = takeNextRunnableRequest();
    }
    LOG(IndexedDB, "UniqueIDBDatabase::handleDatabaseOperations - There are %u pending after this round of handling", m_pendingOpenDBRequests.size());
}

void UniqueIDBDatabase::handleCurrentOperation()
{
    LOG(IndexedDB, "UniqueIDBDatabase::handleCurrentOperation");
    RefPtr currentOpenDBRequest = m_currentOpenDBRequest;
    RELEASE_ASSERT(currentOpenDBRequest);

    if (currentOpenDBRequest->isOpenRequest())
        performCurrentOpenOperation();
    else if (currentOpenDBRequest->isDeleteRequest())
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

void UniqueIDBDatabase::handleDelete(IDBConnectionToClient& connection, const IDBOpenRequestData& requestData)
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
    RefPtr versionChangeDatabaseConnection = m_versionChangeDatabaseConnection;
    ASSERT(versionChangeDatabaseConnection);

    uint64_t requestedVersion = m_currentOpenDBRequest->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    Ref versionChangeTransaction = versionChangeDatabaseConnection->createVersionChangeTransaction(requestedVersion);
    m_versionChangeTransaction = versionChangeTransaction.copyRef();
    auto versionChangeTransactionInfo = versionChangeTransaction->info();
    m_inProgressTransactions.set(versionChangeTransactionInfo.identifier(), versionChangeTransaction.copyRef());
    
    auto error = protect(m_backingStore)->beginTransaction(versionChangeTransactionInfo);
    RefPtr operation = std::exchange(m_currentOpenDBRequest, nullptr);
    operation->setVersionChangeTransaction(versionChangeTransaction.get());

    IDBResultData result;
    if (error.isNull()) {
        addOpenDatabaseConnection(*versionChangeDatabaseConnection);
        m_databaseInfo->setVersion(versionChangeTransactionInfo.newVersion());
        result = IDBResultData::openDatabaseUpgradeNeeded(operation->requestData().requestIdentifier(), versionChangeTransaction, *versionChangeDatabaseConnection);
        operation->didOpenDatabase(result);
    } else {
        versionChangeDatabaseConnection->abortTransactionWithoutCallback(versionChangeTransaction);
        m_versionChangeDatabaseConnection = nullptr;
        result = IDBResultData::error(operation->requestData().requestIdentifier(), error);
        operation->didOpenDatabase(result);
    }
}

void UniqueIDBDatabase::maybeNotifyConnectionsOfVersionChange()
{
    RefPtr currentOpenDBRequest = m_currentOpenDBRequest;
    ASSERT(currentOpenDBRequest);

    if (currentOpenDBRequest->hasNotifiedConnectionsOfVersionChange())
        return;

    uint64_t newVersion = currentOpenDBRequest->isOpenRequest() ? currentOpenDBRequest->requestData().requestedVersion() : 0;
    auto requestIdentifier = currentOpenDBRequest->requestData().requestIdentifier();

    LOG(IndexedDB, "UniqueIDBDatabase::notifyConnectionsOfVersionChange - %" PRIu64, newVersion);

    // 3.3.7 "versionchange" transaction steps
    // Fire a versionchange event at each connection in m_openDatabaseConnections that is open.
    // The event must not be fired on connections which has the closePending flag set.
    HashSet<IDBDatabaseConnectionIdentifier> connectionIdentifiers;
    for (const auto& connection : m_openDatabaseConnections) {
        if (connection->closePending())
            continue;

        connection->fireVersionChangeEvent(requestIdentifier, newVersion);
        connectionIdentifiers.add(connection->identifier());
    }

    if (!connectionIdentifiers.isEmpty())
        currentOpenDBRequest->notifiedConnectionsOfVersionChange(WTF::move(connectionIdentifiers));
    else
        currentOpenDBRequest->maybeNotifyRequestBlocked(m_databaseInfo->version());
}

void UniqueIDBDatabase::notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(IDBDatabaseConnectionIdentifier connectionIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabase::notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent - %" PRIu64, connectionIdentifier.toUInt64());

    RefPtr currentOpenDBRequest = m_currentOpenDBRequest;
    if (!currentOpenDBRequest)
        return;

    currentOpenDBRequest->connectionClosedOrFiredVersionChangeEvent(connectionIdentifier);

    if (currentOpenDBRequest->hasConnectionsPendingVersionChangeEvent())
        return;

    if (!hasAnyOpenConnections() || allConnectionsAreClosedOrClosing()) {
        handleDatabaseOperations();
        return;
    }

    // Since all open connections have fired their version change events but not all of them have closed,
    // this request is officially blocked.
    currentOpenDBRequest->maybeNotifyRequestBlocked(m_databaseInfo->version());
}

void UniqueIDBDatabase::clearTransactionsOnConnection(UniqueIDBDatabaseConnection& connection)
{
    Deque<Ref<UniqueIDBDatabaseTransaction>> pendingTransactions;
    while (!m_pendingTransactions.isEmpty()) {
        auto transaction = m_pendingTransactions.takeFirst();
        if (transaction->databaseConnection() != &connection)
            pendingTransactions.append(WTF::move(transaction));
        else
            connection.deleteTransaction(transaction);
    }
    if (!pendingTransactions.isEmpty())
        m_pendingTransactions.swap(pendingTransactions);

    Deque<Ref<UniqueIDBDatabaseTransaction>> transactionsToAbort;
    for (auto& transaction : m_inProgressTransactions.values()) {
        if (transaction->databaseConnection() == &connection)
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

    auto connectionIdentifier = connection.identifier();
    if (connectionClosedOnBehalfOfServer == IndexedDB::ConnectionClosedOnBehalfOfServer::Yes) {
        if (m_openDatabaseConnections.contains(&connection)) {
            clearTransactionsOnConnection(connection);
            m_openDatabaseConnections.remove(&connection);
        }
    }

    notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(connectionIdentifier);
}

void UniqueIDBDatabase::openDBRequestCancelled(const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabase::openDBRequestCancelled - %s", requestIdentifier.loggingString().utf8().data());

    if (m_currentOpenDBRequest && m_currentOpenDBRequest->requestData().requestIdentifier() == requestIdentifier)
        m_currentOpenDBRequest = nullptr;

    if (RefPtr versionChangeDatabaseConnection = m_versionChangeDatabaseConnection; versionChangeDatabaseConnection && versionChangeDatabaseConnection->openRequestIdentifier() == requestIdentifier) {
        RefPtr versionChangeTransaction = m_versionChangeTransaction;
        ASSERT(!m_versionChangeTransaction || versionChangeTransaction->databaseConnection() == versionChangeDatabaseConnection);

        connectionClosedFromClient(*versionChangeDatabaseConnection);
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

void UniqueIDBDatabase::createObjectStore(UniqueIDBDatabaseTransaction& transaction, const IDBObjectStoreInfo& info, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::createObjectStore");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        auto taskSize = defaultWriteOperationCost + estimateSize(info);
        manager->requestSpace(m_identifier.origin(), taskSize, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, info, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });
            checkedThis->createObjectStore(*weakTransaction, info, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    if (spaceCheckResult != SpaceCheckResult::Pass) {
        callback(IDBError { ExceptionCode::QuotaExceededError, quotaErrorMessageName("CreateObjectStore"_s) });
        return;
    }

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store has closed"_s });

    auto error = protect(m_backingStore)->createObjectStore(transaction.info().identifier(), info);
    if (error.isNull())
        m_databaseInfo->addExistingObjectStore(info);

    callback(error);
}

void UniqueIDBDatabase::deleteObjectStore(UniqueIDBDatabaseTransaction& transaction, const String& objectStoreName, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteObjectStore");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, objectStoreName, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->deleteObjectStore(*weakTransaction, objectStoreName, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreName);
    if (!info) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to delete non-existant object store"_s });
        return;
    }

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto error = protect(m_backingStore)->deleteObjectStore(transaction.info().identifier(), info->identifier());
    if (error.isNull())
        m_databaseInfo->deleteObjectStore(info->identifier());

    callback(error);
}

void UniqueIDBDatabase::renameObjectStore(UniqueIDBDatabaseTransaction& transaction, IDBObjectStoreIdentifier objectStoreIdentifier, const String& newName, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::renameObjectStore");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        auto taskSize = defaultWriteOperationCost + newName.sizeInBytes();
        manager->requestSpace(m_identifier.origin(), taskSize, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, objectStoreIdentifier, newName, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->renameObjectStore(*weakTransaction, objectStoreIdentifier, newName, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    if (spaceCheckResult != SpaceCheckResult::Pass) {
        callback(IDBError(ExceptionCode::QuotaExceededError, quotaErrorMessageName("RenameObjectStore"_s)));
        return;
    }

    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!info) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to rename non-existant object store"_s });
        return;
    }

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto error = protect(m_backingStore)->renameObjectStore(transaction.info().identifier(), objectStoreIdentifier, newName);
    if (error.isNull())
        m_databaseInfo->renameObjectStore(objectStoreIdentifier, newName);

    callback(error);
}

void UniqueIDBDatabase::clearObjectStore(UniqueIDBDatabaseTransaction& transaction, IDBObjectStoreIdentifier objectStoreIdentifier, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::clearObjectStore");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, objectStoreIdentifier, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->clearObjectStore(*weakTransaction, objectStoreIdentifier, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto error = protect(m_backingStore)->clearObjectStore(transaction.info().identifier(), objectStoreIdentifier);
    callback(error);
}

void UniqueIDBDatabase::createIndexAsync(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& indexInfo)
{
    ASSERT(!isMainThread());

    CheckedPtr manager = m_manager.get();
    if (!manager)
        transaction.didCreateIndexAsync(IDBError { ExceptionCode::InvalidStateError });

    auto taskSize = defaultWriteOperationCost + estimateSize(indexInfo);
    manager->requestSpace(m_identifier.origin(), taskSize, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, indexInfo](bool granted) mutable {
        RefPtr protectedTransaction = weakTransaction.get();
        if (!protectedTransaction)
            return;

        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return protectedTransaction->didCreateIndexAsync(IDBError { ExceptionCode::InvalidStateError, "Database is closed."_s });

        checkedThis->createIndexAsyncAfterQuotaCheck(*protectedTransaction, indexInfo, granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
    });
}

void UniqueIDBDatabase::createIndexAsyncAfterQuotaCheck(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& indexInfo, SpaceCheckResult spaceCheckResult)
{
    Ref protectedTransaction = transaction;
    if (spaceCheckResult != SpaceCheckResult::Pass)
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { ExceptionCode::QuotaExceededError, quotaErrorMessageName("CreateIndex"_s) }, DidCreateIndexInBackingStore::No);

    if (!m_backingStore)
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { ExceptionCode::InvalidStateError, "Backing store is closed."_s }, DidCreateIndexInBackingStore::No);

    if (!m_databaseInfo)
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { ExceptionCode::InvalidStateError, "Database info is invalid."_s }, DidCreateIndexInBackingStore::No);

    auto transactionIdentifier = transaction.info().identifier();
    auto createIndexError = protect(m_backingStore)->addIndex(transactionIdentifier, indexInfo);
    if (!createIndexError.isNull())
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, createIndexError, DidCreateIndexInBackingStore::No);

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(indexInfo.objectStoreIdentifier());
    if (!objectStoreInfo)
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { ExceptionCode::InvalidStateError, "Object store does not exist."_s });

    objectStoreInfo->addExistingIndex(indexInfo);
    m_databaseInfo->setMaxIndexID(indexInfo.identifier().toRawValue());

    bool needsToWaitGenerateIndexKey = false;
    protect(m_backingStore)->forEachObjectStoreRecord(transaction.info().identifier(), indexInfo.objectStoreIdentifier(), [&, protectedTransaction](auto&& recordOrError) mutable {
        if (!createIndexError.isNull())
            return;

        if (!recordOrError) {
            createIndexError = WTF::move(recordOrError.error());
            return;
        }

        auto record = recordOrError.value();
        if (!protectedTransaction->generateIndexKeyForRecord(indexInfo, objectStoreInfo->keyPath(), record.key, record.value, record.recordID)) {
            createIndexError = IDBError { ExceptionCode::UnknownError, "Failed to generate index key for record."_s };
            return;
        }
        needsToWaitGenerateIndexKey = true;
    });

    if (!createIndexError.isNull() || !needsToWaitGenerateIndexKey)
        transaction.didCreateIndexAsync(createIndexError);
}

void UniqueIDBDatabase::didGenerateIndexKeyForRecord(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& indexInfo, const IDBKeyData& key, const IndexKey& indexKey, std::optional<int64_t> recordID)
{
    Ref protectedTransaction = transaction;
    if (!m_backingStore)
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { ExceptionCode::InvalidStateError, "Backing store is closed."_s });

    auto error = protect(m_backingStore)->updateIndexRecordsWithIndexKey(transaction.info().identifier(), indexInfo, key, indexKey, recordID);
    if (!error.isNull())
        return didCreateIndexAsyncForTransaction(transaction, indexInfo, error);

    if (!transaction.pendingGenerateIndexKeyRequests())
        didCreateIndexAsyncForTransaction(transaction, indexInfo, IDBError { });
}

void UniqueIDBDatabase::didCreateIndexAsyncForTransaction(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& indexInfo, const IDBError& error, DidCreateIndexInBackingStore didCreateIndexInBackingStore)
{
    CheckedPtr backingStore = m_backingStore.get();
    if (backingStore && !error.isNull() && didCreateIndexInBackingStore == DidCreateIndexInBackingStore::Yes)
        backingStore->revertAddIndex(transaction.info().identifier(), indexInfo.objectStoreIdentifier(), indexInfo.identifier());

    transaction.didCreateIndexAsync(error);
}

void UniqueIDBDatabase::deleteIndex(UniqueIDBDatabaseTransaction& transaction, IDBObjectStoreIdentifier objectStoreIdentifier, const String& indexName, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteIndex");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, objectStoreIdentifier, indexName, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->deleteIndex(*weakTransaction, objectStoreIdentifier, indexName, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to delete index from non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexName);
    if (!indexInfo) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to delete non-existant index"_s });
        return;
    }

    auto indexIdentifier = indexInfo->identifier();
    auto error = protect(m_backingStore)->deleteIndex(transaction.info().identifier(), objectStoreIdentifier, indexIdentifier);
    if (error.isNull())
        objectStoreInfo->deleteIndex(indexIdentifier);

    callback(error);
}

void UniqueIDBDatabase::renameIndex(UniqueIDBDatabaseTransaction& transaction, IDBObjectStoreIdentifier objectStoreIdentifier, IDBIndexIdentifier indexIdentifier, const String& newName, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::renameIndex");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        auto taskSize = defaultWriteOperationCost + newName.sizeInBytes();
        manager->requestSpace(m_identifier.origin(), taskSize, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, objectStoreIdentifier, indexIdentifier, newName, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->renameIndex(*weakTransaction, objectStoreIdentifier, indexIdentifier, newName, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    if (spaceCheckResult != SpaceCheckResult::Pass) {
        callback(IDBError { ExceptionCode::QuotaExceededError, quotaErrorMessageName("RenameIndex"_s) });
        return;
    }

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to rename index in non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo) {
        callback(IDBError { ExceptionCode::UnknownError, "Attempt to rename non-existant index"_s });
        return;
    }

    auto error = protect(m_backingStore)->renameIndex(transaction.info().identifier(), objectStoreIdentifier, indexIdentifier, newName);
    if (error.isNull())
        indexInfo->rename(newName);

    callback(error);
}

void UniqueIDBDatabase::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, const IndexIDToIndexKeyMap& indexKeys, IndexedDB::ObjectStoreOverwriteMode overwriteMode, KeyDataCallback&& callback)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::putOrAdd");

    ASSERT(m_databaseInfo);

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, keyData);

    auto objectStoreIdentifier = requestData.objectStoreIdentifier();
    auto* objectStoreInfo = protect(m_backingStore)->infoForObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Object store cannot be found in the backing store"_s }, keyData);

    CheckedPtr manager = m_manager.get();
    if (!manager)
        return callback(IDBError { ExceptionCode::InvalidStateError }, keyData);

    IDBKeyData usedKey;
    IDBError error;
    bool usedKeyIsGenerated = false;
    uint64_t keyNumber;
    auto transactionIdentifier = requestData.transactionIdentifier();
    auto generatedKeyResetter = makeScopeExit([checkedThis = CheckedRef { *this }, transactionIdentifier, objectStoreIdentifier, &keyNumber, &usedKeyIsGenerated]() {
        if (usedKeyIsGenerated)
            protect(checkedThis->m_backingStore)->revertGeneratedKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
    });

    if (objectStoreInfo->autoIncrement() && !keyData.isValid()) {
        error = protect(m_backingStore)->generateKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
        if (!error.isNull())
            return callback(error, usedKey);

        usedKey.setNumberValue(keyNumber);
        usedKeyIsGenerated = true;
    } else
        usedKey = keyData;

    if (overwriteMode == IndexedDB::ObjectStoreOverwriteMode::NoOverwrite) {
        bool keyExists;
        error = protect(m_backingStore)->keyExistsInObjectStore(transactionIdentifier, objectStoreIdentifier, usedKey, keyExists);
        if (!error && keyExists)
            error = IDBError { ExceptionCode::ConstraintError, "Key already exists in the object store"_s };

        if (!!error)
            return callback(error, usedKey);
    }

    auto usedIndexKeys = indexKeys;
    if (usedKeyIsGenerated) {
        // If key is generated on server, client does not know about that and uses placeholder in index keys.
        for (auto& indexKey : usedIndexKeys.values())
            indexKey.updatePlaceholderKeys(usedKey);
    }

    generatedKeyResetter.release();
    auto keySize = estimateSize(usedKey);
    auto valueSize = estimateSize(value);
    auto indexSize = estimateSize(*objectStoreInfo, usedIndexKeys, keySize);
    auto taskSize = defaultWriteOperationCost + keySize + valueSize + indexSize;

    LOG(IndexedDB, "UniqueIDBDatabase::putOrAdd quota check with task size: %" PRIu64 " key size: %" PRIu64 " value size: %" PRIu64 " index size: %" PRIu64, taskSize, keySize, valueSize, indexSize);
    manager->requestSpace(m_identifier.origin(), taskSize, [weakThis = WeakPtr { *this }, requestData, usedKey, value, overwriteMode, callback = WTF::move(callback), usedKeyIsGenerated, usedIndexKeys, objectStoreInfo = *objectStoreInfo](bool granted) mutable {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return callback(IDBError { ExceptionCode::InvalidStateError, "Database is closed"_s }, usedKey);

        checkedThis->putOrAddAfterSpaceCheck(requestData, usedKey, value, overwriteMode, WTF::move(callback), usedKeyIsGenerated, usedIndexKeys, objectStoreInfo, granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
    });
}

void UniqueIDBDatabase::putOrAddAfterSpaceCheck(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode, KeyDataCallback&& callback, bool isKeyGenerated, const IndexIDToIndexKeyMap& indexKeys, const IDBObjectStoreInfo& objectStoreInfo, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    ASSERT(m_databaseInfo);

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, keyData);

    uint64_t keyNumber = isKeyGenerated ? keyData.number() : 0;
    auto objectStoreIdentifier = objectStoreInfo.identifier();
    auto transactionIdentifier = requestData.transactionIdentifier();
    auto generatedKeyResetter = makeScopeExit([checkedThis = CheckedRef { *this }, transactionIdentifier, objectStoreIdentifier, &keyNumber, &isKeyGenerated]() {
        if (isKeyGenerated)
            protect(checkedThis->m_backingStore)->revertGeneratedKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
    });

    if (spaceCheckResult != SpaceCheckResult::Pass)
        return callback(IDBError { ExceptionCode::QuotaExceededError, quotaErrorMessageName("PutOrAdd"_s) }, keyData);
    // If a record already exists in store, then remove the record from store using the steps for deleting records from an object store.
    // This is important because formally deleting it from the object store also removes it from the appropriate indexes.
    IDBError error = protect(m_backingStore)->deleteRange(transactionIdentifier, objectStoreIdentifier, keyData);
    if (!error.isNull())
        return callback(error, keyData);

    error = protect(m_backingStore)->addRecord(transactionIdentifier, objectStoreInfo, keyData, indexKeys, value);
    if (!error.isNull())
        return callback(error, keyData);

    if (overwriteMode != IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor && objectStoreInfo.autoIncrement() && keyData.type() == IndexedDB::KeyType::Number)
        error = protect(m_backingStore)->maybeUpdateKeyGeneratorNumber(transactionIdentifier, objectStoreIdentifier, keyData.number());

    generatedKeyResetter.release();
    callback(error, keyData);
}

void UniqueIDBDatabase::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData, GetResultCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getRecord");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError }, IDBGetResult { });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, getRecordData, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database is closed"_s }, IDBGetResult { });

            checkedThis->getRecord(requestData, getRecordData, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, IDBGetResult { });

    IDBGetResult result;
    IDBError error;

    auto transactionIdentifier = requestData.transactionIdentifier();
    if (auto indexIdentifier = requestData.indexIdentifier())
        error = protect(m_backingStore)->getIndexRecord(transactionIdentifier, requestData.objectStoreIdentifier(), *indexIdentifier, requestData.indexRecordType(), getRecordData.keyRangeData, result);
    else
        error = protect(m_backingStore)->getRecord(transactionIdentifier, requestData.objectStoreIdentifier(), getRecordData.keyRangeData, getRecordData.type, result);

    callback(error, result);
}

void UniqueIDBDatabase::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData, GetAllResultsCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getAllRecords");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError }, IDBGetAllResult { });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, getAllRecordsData, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database is closed"_s }, IDBGetAllResult { });

            checkedThis->getAllRecords(requestData, getAllRecordsData, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, IDBGetAllResult { });

    IDBGetAllResult result;
    auto error = protect(m_backingStore)->getAllRecords(requestData.transactionIdentifier(), getAllRecordsData, result);

    callback(error, result);
}

void UniqueIDBDatabase::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& range, CountCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::getCount");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError }, 0);

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, range, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!weakThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database is closed"_s }, 0);

            checkedThis->getCount(requestData, range, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, 0);

    uint64_t count = 0;
    auto error = protect(m_backingStore)->getCount(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), requestData.indexIdentifier(), range, count);

    callback(error, count);
}

void UniqueIDBDatabase::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::deleteRecord");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, keyRangeData, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database is closed"_s });

            checkedThis->deleteRecord(requestData, keyRangeData, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto error = protect(m_backingStore)->deleteRange(requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), keyRangeData);

    callback(error);
}

void UniqueIDBDatabase::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info, GetResultCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::openCursor");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError }, IDBGetResult { });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, info, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s }, IDBGetResult { });

            checkedThis->openCursor(requestData, info, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, IDBGetResult { });

    IDBGetResult result;
    auto error = protect(m_backingStore)->openCursor(requestData.transactionIdentifier(), info, result);

    callback(error, result);
}

void UniqueIDBDatabase::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data, GetResultCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::iterateCursor");

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError }, IDBGetResult { });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, requestData, data, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s }, IDBGetResult { });

            checkedThis->iterateCursor(requestData, data, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s }, IDBGetResult { });

    IDBGetResult result;
    auto cursorIdentifier = requestData.cursorIdentifier();
    auto error = protect(m_backingStore)->iterateCursor(requestData.transactionIdentifier(), cursorIdentifier, data, result);

    callback(error, result);
}

void UniqueIDBDatabase::commitTransaction(UniqueIDBDatabaseTransaction& transaction, uint64_t handledRequestResultsCount, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::commitTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [handledRequestResultsCount, weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->commitTransaction(*weakTransaction, handledRequestResultsCount, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    ASSERT(transaction.database() == this);

    if (transaction.shouldAbortDueToUnhandledRequestError(handledRequestResultsCount)) {
        abortTransaction(transaction, [callback = WTF::move(callback)](auto error) {
            if (!error.isNull())
                return callback(error);

            callback(IDBError { ExceptionCode::UnknownError, "Transaction is aborted due to unhandled failed request"_s });
        }, spaceCheckResult);
        return;
    }

    if (!m_backingStore)
        return callback(IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s });

    auto takenTransaction = m_inProgressTransactions.take(transaction.info().identifier());
    if (!takenTransaction) {
        if (transaction.databaseConnection() && !m_openDatabaseConnections.contains(transaction.databaseConnection()))
            return;

        callback(IDBError { ExceptionCode::UnknownError, "Attempt to commit transaction that is not running"_s });
        return;
    }

    auto error = protect(m_backingStore)->commitTransaction(transaction.info().identifier());

    callback(error);
    transactionCompleted(WTF::move(takenTransaction));
}

void UniqueIDBDatabase::abortTransaction(UniqueIDBDatabaseTransaction& transaction, ErrorCallback&& callback, SpaceCheckResult spaceCheckResult)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::abortTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    if (spaceCheckResult == SpaceCheckResult::Unknown) {
        CheckedPtr manager = m_manager.get();
        if (!manager)
            return callback(IDBError { ExceptionCode::InvalidStateError });

        manager->requestSpace(m_identifier.origin(), 0, [weakThis = WeakPtr { *this }, weakTransaction = WeakPtr { transaction }, callback = WTF::move(callback)](bool granted) mutable {
            CheckedPtr checkedThis = weakThis.get();
            if (!checkedThis || !weakTransaction)
                return callback(IDBError { ExceptionCode::InvalidStateError, "Database or transaction is closed"_s });

            checkedThis->abortTransaction(*weakTransaction, WTF::move(callback), granted ? SpaceCheckResult::Pass : SpaceCheckResult::Fail);
        });
        return;
    }

    ASSERT(spaceCheckResult == SpaceCheckResult::Pass);
    ASSERT(transaction.database() == this);

    auto takenTransaction = m_inProgressTransactions.take(transaction.info().identifier());
    if (!takenTransaction) {
        if (!m_openDatabaseConnections.contains(transaction.databaseConnection()))
            return;

        callback(IDBError { ExceptionCode::UnknownError, "Attempt to abort transaction that is not running"_s });
        return;
    }

    // If transaction is already aborted for suspension, return the result of that abort.
    if (auto existingAbortResult = takenTransaction->suspensionAbortResult()) {
        callback(*existingAbortResult);
        transactionCompleted(WTF::move(takenTransaction));
        return;
    }

    auto transactionIdentifier = transaction.info().identifier();
    if (m_versionChangeTransaction && m_versionChangeTransaction->info().identifier() == transactionIdentifier) {
        ASSERT(m_versionChangeTransaction == &transaction);
        ASSERT(!m_versionChangeDatabaseConnection || m_versionChangeTransaction->databaseConnection() == m_versionChangeDatabaseConnection);
        ASSERT(m_versionChangeTransaction->originalDatabaseInfo());
        RefPtr versionChangeTransaction = m_versionChangeTransaction;
        m_databaseInfo = makeUnique<IDBDatabaseInfo>(*versionChangeTransaction->originalDatabaseInfo());
    }

    IDBError error;
    if (CheckedPtr backingStore = m_backingStore.get())
        error = backingStore->abortTransaction(transactionIdentifier);
    else
        error = IDBError { ExceptionCode::InvalidStateError, "Backing store is closed"_s };

    callback(error);
    transactionCompleted(WTF::move(takenTransaction));
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
    LOG(IndexedDB, "UniqueIDBDatabase::connectionClosedFromClient - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier().toUInt64());

    Ref<UniqueIDBDatabaseConnection> protectedConnection(connection);
    m_openDatabaseConnections.remove(&connection);

    if (m_versionChangeDatabaseConnection == &connection) {
        m_versionChangeDatabaseConnection = nullptr;
        if (RefPtr transaction = m_versionChangeTransaction) {
            connection.abortTransactionWithoutCallback(*transaction);
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
    LOG(IndexedDB, "UniqueIDBDatabase::connectionClosedFromServer - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier().toUInt64());

    protect(connection.connectionToClient())->didCloseFromServer(connection, IDBError::userDeleteError());

    m_openDatabaseConnections.remove(&connection);
}

void UniqueIDBDatabase::enqueueTransaction(Ref<UniqueIDBDatabaseTransaction>&& transaction)
{
    LOG(IndexedDB, "UniqueIDBDatabase::enqueueTransaction - %s", transaction->info().loggingString().utf8().data());

    ASSERT(transaction->info().mode() != IDBTransactionMode::Versionchange);

    m_pendingTransactions.append(WTF::move(transaction));

    handleTransactions();
}

void UniqueIDBDatabase::handleTransactions()
{
    LOG(IndexedDB, "UniqueIDBDatabase::handleTransactions - There are %zu pending", m_pendingTransactions.size());

    bool hadDeferredTransactions = false;
    RefPtr transaction = takeNextRunnableTransaction(hadDeferredTransactions);

    while (transaction) {
        m_inProgressTransactions.set(transaction->info().identifier(), *transaction);
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

    auto error = protect(m_backingStore)->beginTransaction(transaction.info());

    transaction.didActivateInBackingStore(error);
}

template<typename T> bool scopesOverlap(const T& aScopes, const Vector<IDBObjectStoreIdentifier>& bScopes)
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

    // Version change transaction should have exclusive access to database.
    if (m_versionChangeTransaction)
        return nullptr;

    if (m_pendingTransactions.isEmpty())
        return nullptr;

    bool hasReadWriteTransactionInProgress = std::ranges::any_of(m_inProgressTransactions, [&](auto& entry) {
        return !entry.value->isReadOnly();
    });
    Deque<Ref<UniqueIDBDatabaseTransaction>> deferredTransactions;
    RefPtr<UniqueIDBDatabaseTransaction> currentTransaction;

    HashSet<IDBObjectStoreIdentifier> deferredReadWriteScopes;

    while (!m_pendingTransactions.isEmpty()) {
        currentTransaction = m_pendingTransactions.takeFirst();

        switch (currentTransaction->info().mode()) {
        case IDBTransactionMode::Readonly: {
            bool hasOverlappingScopes = scopesOverlap(deferredReadWriteScopes, currentTransaction->objectStoreIdentifiers());
            hasOverlappingScopes |= scopesOverlap(m_objectStoreWriteTransactions, currentTransaction->objectStoreIdentifiers());

            if (hasOverlappingScopes)
                deferredTransactions.append(currentTransaction.releaseNonNull());

            break;
        }
        case IDBTransactionMode::Readwrite: {
            bool hasOverlappingScopes = scopesOverlap(m_objectStoreTransactionCounts, currentTransaction->objectStoreIdentifiers());
            hasOverlappingScopes |= scopesOverlap(deferredReadWriteScopes, currentTransaction->objectStoreIdentifiers());
            bool hasBackingStoreSupport = protect(m_backingStore)->supportsSimultaneousReadWriteTransactions() || !hasReadWriteTransactionInProgress;
            if (hasOverlappingScopes || !hasBackingStoreSupport) {
                for (auto objectStore : currentTransaction->objectStoreIdentifiers())
                    deferredReadWriteScopes.add(objectStore);
                deferredTransactions.append(currentTransaction.releaseNonNull());
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
        request.didOpenDatabase(result);
    else
        request.didDeleteDatabase(result);
}

void UniqueIDBDatabase::immediateClose()
{
    LOG(IndexedDB, "UniqueIDBDatabase::immediateClose");

    // Error out all transactions.
    // Pending transactions must be cleared before in-progress transactions,
    // or they may get started right away after aborting in-progress transactions.
    for (auto& transaction : m_pendingTransactions) {
        if (RefPtr databaseConnection = transaction->databaseConnection())
            databaseConnection->deleteTransaction(transaction);
    }
    m_pendingTransactions.clear();

    for (Ref transaction : copyToVector(m_inProgressTransactions.values()))
        transaction->abortWithoutCallback();

    ASSERT(m_inProgressTransactions.isEmpty());

    m_objectStoreTransactionCounts.clear();
    m_objectStoreWriteTransactions.clear();

    // Error out all IDBOpenDBRequests
    if (RefPtr request = std::exchange(m_currentOpenDBRequest, nullptr))
        errorOpenDBRequestForUserDelete(*request);

    for (auto& request : m_pendingOpenDBRequests)
        errorOpenDBRequestForUserDelete(request);

    m_pendingOpenDBRequests.clear();

    // Close all open connections
    auto openDatabaseConnections = m_openDatabaseConnections;
    for (auto& connection : openDatabaseConnections)
        connectionClosedFromServer(connection);

    if (RefPtr versionChangeDatabaseConnection = m_versionChangeDatabaseConnection) {
        if (!openDatabaseConnections.contains(versionChangeDatabaseConnection.get()))
            connectionClosedFromServer(*versionChangeDatabaseConnection);
        m_versionChangeDatabaseConnection = nullptr;
    }

    ASSERT(!hasAnyOpenConnections());

    close();
}

bool UniqueIDBDatabase::hasActiveTransactions() const
{
    ASSERT(isMainThread());

    return !m_inProgressTransactions.isEmpty();
}

void UniqueIDBDatabase::abortActiveTransactions()
{
    for (auto& identifier : copyToVector(m_inProgressTransactions.keys())) {
        RefPtr transaction = m_inProgressTransactions.get(identifier);
        transaction->setSuspensionAbortResult(protect(m_backingStore)->abortTransaction(transaction->info().identifier()));
    }
}

void UniqueIDBDatabase::close()
{
    LOG(IndexedDB, "UniqueIDBDatabase::close");

    if (m_backingStore) {
        protect(m_backingStore)->close();
        m_backingStore = nullptr;
    }
}

bool UniqueIDBDatabase::tryClose()
{
    if (hasDataInMemory())
        return false;

    if (hasAnyOpenConnections() || m_versionChangeDatabaseConnection)
        return false;

    close();
    return true;
}
    
bool UniqueIDBDatabase::hasDataInMemory() const
{
    return m_backingStore && protect(m_backingStore)->isEphemeral();
}

RefPtr<ServerOpenDBRequest> UniqueIDBDatabase::takeNextRunnableRequest()
{
    // Connection of request may be closed or lost.
    clearStalePendingOpenDBRequests();

    if (!m_pendingOpenDBRequests.isEmpty())
        return m_pendingOpenDBRequests.takeFirst();

    return nullptr;
}

String UniqueIDBDatabase::filePath() const
{
    return m_backingStore ? protect(m_backingStore)->fullDatabasePath() : nullString();
}

std::optional<IDBDatabaseNameAndVersion> UniqueIDBDatabase::nameAndVersion() const
{
    if (!m_backingStore)
        return std::nullopt;

    RefPtr versionChangeTransaction = m_versionChangeTransaction;
    if (versionChangeTransaction) {
        if (auto databaseInfo = versionChangeTransaction->originalDatabaseInfo()) {
            // The database is newly created.
            if (!databaseInfo->version())
                return std::nullopt;

            return IDBDatabaseNameAndVersion { databaseInfo->name(), databaseInfo->version() };
        }

        return std::nullopt;
    }

    return IDBDatabaseNameAndVersion { m_databaseInfo->name(), m_databaseInfo->version() };
}

void UniqueIDBDatabase::handleLowMemoryWarning()
{
    if (CheckedPtr backingStore = m_backingStore.get())
        backingStore->handleLowMemoryWarning();
}

} // namespace IDBServer
} // namespace WebCore
