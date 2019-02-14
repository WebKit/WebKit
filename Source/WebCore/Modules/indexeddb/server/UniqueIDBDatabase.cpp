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

#include "IDBBindingUtilities.h"
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
#include "SerializedScriptValue.h"
#include "UniqueIDBDatabaseConnection.h"
#include <JavaScriptCore/AuxiliaryBarrierInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>

namespace WebCore {
using namespace JSC;
namespace IDBServer {

UniqueIDBDatabase::UniqueIDBDatabase(IDBServer& server, const IDBDatabaseIdentifier& identifier)
    : m_server(server)
    , m_identifier(identifier)
    , m_operationAndTransactionTimer(*this, &UniqueIDBDatabase::operationAndTransactionTimerFired)
{
    LOG(IndexedDB, "UniqueIDBDatabase::UniqueIDBDatabase() (%p) %s", this, m_identifier.debugString().utf8().data());
}

UniqueIDBDatabase::~UniqueIDBDatabase()
{
    LOG(IndexedDB, "UniqueIDBDatabase::~UniqueIDBDatabase() (%p) %s", this, m_identifier.debugString().utf8().data());
    ASSERT(isMainThread());
    ASSERT(!hasAnyPendingCallbacks());
    ASSERT(!hasUnfinishedTransactions());
    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(m_openDatabaseConnections.isEmpty());
    ASSERT(m_clientClosePendingDatabaseConnections.isEmpty());
    ASSERT(m_serverClosePendingDatabaseConnections.isEmpty());

    RELEASE_ASSERT(m_databaseQueue.isKilled());
    RELEASE_ASSERT(m_databaseReplyQueue.isKilled());
    RELEASE_ASSERT(!m_backingStore);
}

const IDBDatabaseInfo& UniqueIDBDatabase::info() const
{
    RELEASE_ASSERT(m_databaseInfo);
    return *m_databaseInfo;
}

void UniqueIDBDatabase::openDatabaseConnection(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    LOG(IndexedDB, "UniqueIDBDatabase::openDatabaseConnection");
    ASSERT(!m_hardClosedForUserDelete);
    ASSERT(isMainThread());

    m_pendingOpenDBRequests.add(ServerOpenDBRequest::create(connection, requestData));

    // An open operation is already in progress, so we can't possibly handle this one yet.
    if (m_isOpeningBackingStore)
        return;

    handleDatabaseOperations();
}

bool UniqueIDBDatabase::hasAnyPendingCallbacks() const
{
    return !m_errorCallbacks.isEmpty()
        || !m_keyDataCallbacks.isEmpty()
        || !m_getResultCallbacks.isEmpty()
        || !m_getAllResultsCallbacks.isEmpty()
        || !m_countCallbacks.isEmpty();
}

bool UniqueIDBDatabase::isVersionChangeInProgress()
{
#if !LOG_DISABLED
    if (m_versionChangeTransaction)
        ASSERT(m_versionChangeDatabaseConnection);
#endif

    return m_versionChangeDatabaseConnection;
}

void UniqueIDBDatabase::performCurrentOpenOperation()
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::performCurrentOpenOperation (%p)", this);

    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isOpenRequest());

    if (!m_databaseInfo) {
        if (!m_isOpeningBackingStore) {
            m_isOpeningBackingStore = true;
            postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::openBackingStore, m_identifier));
        }

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

    if (!m_backingStoreOpenError.isNull()) {
        auto result = IDBResultData::error(m_currentOpenDBRequest->requestData().requestIdentifier(), m_backingStoreOpenError);
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
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::performCurrentDeleteOperation - %s", m_identifier.debugString().utf8().data());

    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isDeleteRequest());

    if (m_deleteBackingStoreInProgress)
        return;

    if (hasAnyOpenConnections()) {
        maybeNotifyConnectionsOfVersionChange();
        return;
    }

    if (hasUnfinishedTransactions())
        return;

    ASSERT(!hasAnyPendingCallbacks());
    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(m_openDatabaseConnections.isEmpty());

    // It's possible to have multiple delete requests queued up in a row.
    // In that scenario only the first request will actually have to delete the database.
    // Subsequent requests can immediately notify their completion.

    if (!m_deleteBackingStoreInProgress) {
        if (!m_databaseInfo && m_mostRecentDeletedDatabaseInfo)
            didDeleteBackingStore(0);
        else {
            m_deleteBackingStoreInProgress = true;
            postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::deleteBackingStore, m_identifier));
        }
    }
}

void UniqueIDBDatabase::deleteBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::deleteBackingStore");

    uint64_t deletedVersion = 0;

    if (m_backingStore) {
        m_backingStore->deleteBackingStore();
        m_backingStore = nullptr;
        m_backingStoreSupportsSimultaneousTransactions = false;
        m_backingStoreIsEphemeral = false;
    } else {
        auto backingStore = m_server.createBackingStore(identifier);

        IDBDatabaseInfo databaseInfo;
        auto error = backingStore->getOrEstablishDatabaseInfo(databaseInfo);
        if (!error.isNull())
            LOG_ERROR("Error getting database info from database %s that we are trying to delete", identifier.debugString().utf8().data());

        deletedVersion = databaseInfo.version();
        backingStore->deleteBackingStore();
    }

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didDeleteBackingStore, deletedVersion));
}

void UniqueIDBDatabase::performUnconditionalDeleteBackingStore()
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performUnconditionalDeleteBackingStore");

    if (m_backingStore)
        m_backingStore->deleteBackingStore();

    shutdownForClose();
}

void UniqueIDBDatabase::scheduleShutdownForClose()
{
    ASSERT(isMainThread());

    m_operationAndTransactionTimer.stop();

    RELEASE_ASSERT(!m_owningPointerForClose);
    m_owningPointerForClose = m_server.closeAndTakeUniqueIDBDatabase(*this);

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::shutdownForClose));
}

void UniqueIDBDatabase::shutdownForClose()
{
    ASSERT(!isMainThread());
    ASSERT(m_owningPointerForClose.get() == this);

    LOG(IndexedDB, "(db) UniqueIDBDatabase::shutdownForClose");

    m_backingStore = nullptr;
    m_backingStoreSupportsSimultaneousTransactions = false;
    m_backingStoreIsEphemeral = false;

    if (!m_databaseQueue.isEmpty()) {
        postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::shutdownForClose));
        return;
    }
    m_databaseQueue.kill();

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didShutdownForClose));
}

void UniqueIDBDatabase::didShutdownForClose()
{
    ASSERT(m_databaseReplyQueue.isEmpty());
    m_databaseReplyQueue.kill();
}

void UniqueIDBDatabase::didDeleteBackingStore(uint64_t deletedVersion)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didDeleteBackingStore");

    ASSERT(!hasAnyPendingCallbacks());
    ASSERT(!hasUnfinishedTransactions());
    ASSERT(m_pendingTransactions.isEmpty());
    ASSERT(m_openDatabaseConnections.isEmpty());
    ASSERT(!m_backingStore);

    // It's possible that the openDBRequest was cancelled from client-side after the delete was already dispatched to the backingstore.
    // So it's okay if we don't have a currentOpenDBRequest, but if we do it has to be a deleteRequest.
    ASSERT(!m_currentOpenDBRequest || m_currentOpenDBRequest->isDeleteRequest());

    if (m_databaseInfo)
        m_mostRecentDeletedDatabaseInfo = WTFMove(m_databaseInfo);

    // If this UniqueIDBDatabase was brought into existence for the purpose of deleting the file on disk,
    // we won't have a m_mostRecentDeletedDatabaseInfo. In that case, we'll manufacture one using the
    // passed in deletedVersion argument.
    if (!m_mostRecentDeletedDatabaseInfo)
        m_mostRecentDeletedDatabaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), deletedVersion);

    if (m_currentOpenDBRequest) {
        m_currentOpenDBRequest->notifyDidDeleteDatabase(*m_mostRecentDeletedDatabaseInfo);
        m_currentOpenDBRequest = nullptr;
    }

    m_deleteBackingStoreInProgress = false;

    if (m_hardClosedForUserDelete)
        return;

    invokeOperationAndTransactionTimer();
}

void UniqueIDBDatabase::handleDatabaseOperations()
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::handleDatabaseOperations - There are %u pending", m_pendingOpenDBRequests.size());
    ASSERT(!m_hardClosedForUserDelete);

    if (m_deleteBackingStoreInProgress)
        return;

    if (m_versionChangeDatabaseConnection || m_versionChangeTransaction || m_currentOpenDBRequest) {
        // We can't start any new open-database operations right now, but we might be able to start handling a delete operation.
        if (!m_currentOpenDBRequest && !m_pendingOpenDBRequests.isEmpty() && m_pendingOpenDBRequests.first()->isDeleteRequest())
            m_currentOpenDBRequest = m_pendingOpenDBRequests.takeFirst();

        // Some operations (such as the first open operation after a delete) require multiple passes to completely handle
        if (m_currentOpenDBRequest)
            handleCurrentOperation();

        return;
    }

    if (m_pendingOpenDBRequests.isEmpty())
        return;

    m_currentOpenDBRequest = m_pendingOpenDBRequests.takeFirst();
    LOG(IndexedDB, "UniqueIDBDatabase::handleDatabaseOperations - Popped an operation, now there are %u pending", m_pendingOpenDBRequests.size());

    handleCurrentOperation();
}

void UniqueIDBDatabase::handleCurrentOperation()
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::handleCurrentOperation");
    ASSERT(!m_hardClosedForUserDelete);
    ASSERT(m_currentOpenDBRequest);

    if (m_currentOpenDBRequest->isOpenRequest())
        performCurrentOpenOperation();
    else if (m_currentOpenDBRequest->isDeleteRequest())
        performCurrentDeleteOperation();
    else
        ASSERT_NOT_REACHED();

    if (!m_currentOpenDBRequest)
        invokeOperationAndTransactionTimer();
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

static uint64_t generateUniqueCallbackIdentifier()
{
    ASSERT(isMainThread());
    static uint64_t currentID = 0;
    return ++currentID;
}

uint64_t UniqueIDBDatabase::storeCallbackOrFireError(ErrorCallback&& callback)
{
    if (m_hardClosedForUserDelete) {
        callback(IDBError::userDeleteError());
        return 0;
    }

    uint64_t identifier = generateUniqueCallbackIdentifier();
    ASSERT(!m_errorCallbacks.contains(identifier));
    m_errorCallbacks.add(identifier, WTFMove(callback));
    m_callbackQueue.append(identifier);
    return identifier;
}

uint64_t UniqueIDBDatabase::storeCallbackOrFireError(KeyDataCallback&& callback)
{
    if (m_hardClosedForUserDelete) {
        callback(IDBError::userDeleteError(), { });
        return 0;
    }

    uint64_t identifier = generateUniqueCallbackIdentifier();
    ASSERT(!m_keyDataCallbacks.contains(identifier));
    m_keyDataCallbacks.add(identifier, WTFMove(callback));
    m_callbackQueue.append(identifier);
    return identifier;
}

uint64_t UniqueIDBDatabase::storeCallbackOrFireError(GetResultCallback&& callback)
{
    if (m_hardClosedForUserDelete) {
        callback(IDBError::userDeleteError(), { });
        return 0;
    }

    uint64_t identifier = generateUniqueCallbackIdentifier();
    ASSERT(!m_getResultCallbacks.contains(identifier));
    m_getResultCallbacks.add(identifier, WTFMove(callback));
    m_callbackQueue.append(identifier);
    return identifier;
}

uint64_t UniqueIDBDatabase::storeCallbackOrFireError(GetAllResultsCallback&& callback)
{
    if (m_hardClosedForUserDelete) {
        callback(IDBError::userDeleteError(), { });
        return 0;
    }

    uint64_t identifier = generateUniqueCallbackIdentifier();
    ASSERT(!m_getAllResultsCallbacks.contains(identifier));
    m_getAllResultsCallbacks.add(identifier, WTFMove(callback));
    m_callbackQueue.append(identifier);
    return identifier;
}

uint64_t UniqueIDBDatabase::storeCallbackOrFireError(CountCallback&& callback)
{
    if (m_hardClosedForUserDelete) {
        callback(IDBError::userDeleteError(), 0);
        return 0;
    }

    uint64_t identifier = generateUniqueCallbackIdentifier();
    ASSERT(!m_countCallbacks.contains(identifier));
    m_countCallbacks.add(identifier, WTFMove(callback));
    m_callbackQueue.append(identifier);
    return identifier;
}

void UniqueIDBDatabase::handleDelete(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::handleDelete");
    ASSERT(!m_hardClosedForUserDelete);

    m_pendingOpenDBRequests.add(ServerOpenDBRequest::create(connection, requestData));
    handleDatabaseOperations();
}

void UniqueIDBDatabase::startVersionChangeTransaction()
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::startVersionChangeTransaction");

    ASSERT(!m_versionChangeTransaction);
    ASSERT(m_currentOpenDBRequest);
    ASSERT(m_currentOpenDBRequest->isOpenRequest());
    ASSERT(m_versionChangeDatabaseConnection);

    auto operation = WTFMove(m_currentOpenDBRequest);

    uint64_t requestedVersion = operation->requestData().requestedVersion();
    if (!requestedVersion)
        requestedVersion = m_databaseInfo->version() ? m_databaseInfo->version() : 1;

    addOpenDatabaseConnection(*m_versionChangeDatabaseConnection);

    m_versionChangeTransaction = &m_versionChangeDatabaseConnection->createVersionChangeTransaction(requestedVersion);
    m_databaseInfo->setVersion(requestedVersion);

    m_inProgressTransactions.set(m_versionChangeTransaction->info().identifier(), m_versionChangeTransaction);
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::beginTransactionInBackingStore, m_versionChangeTransaction->info()));

    auto result = IDBResultData::openDatabaseUpgradeNeeded(operation->requestData().requestIdentifier(), *m_versionChangeTransaction);
    operation->connection().didOpenDatabase(result);
}

void UniqueIDBDatabase::beginTransactionInBackingStore(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "(db) UniqueIDBDatabase::beginTransactionInBackingStore");
    m_backingStore->beginTransaction(info);
}

void UniqueIDBDatabase::maybeNotifyConnectionsOfVersionChange()
{
    ASSERT(m_currentOpenDBRequest);

    if (m_currentOpenDBRequest->hasNotifiedConnectionsOfVersionChange())
        return;

    uint64_t newVersion = m_currentOpenDBRequest->isOpenRequest() ? m_currentOpenDBRequest->requestData().requestedVersion() : 0;
    auto requestIdentifier = m_currentOpenDBRequest->requestData().requestIdentifier();

    LOG(IndexedDB, "(main) UniqueIDBDatabase::notifyConnectionsOfVersionChange - %" PRIu64, newVersion);

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
        invokeOperationAndTransactionTimer();
        return;
    }

    // Since all open connections have fired their version change events but not all of them have closed,
    // this request is officially blocked.
    m_currentOpenDBRequest->maybeNotifyRequestBlocked(m_databaseInfo->version());
}

void UniqueIDBDatabase::didFireVersionChangeEvent(UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabase::didFireVersionChangeEvent");

    if (!m_currentOpenDBRequest)
        return;

    ASSERT_UNUSED(requestIdentifier, m_currentOpenDBRequest->requestData().requestIdentifier() == requestIdentifier);

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

void UniqueIDBDatabase::openBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::openBackingStore (%p)", this);

    ASSERT(!m_backingStore);
    m_backingStore = m_server.createBackingStore(identifier);
    m_backingStoreSupportsSimultaneousTransactions = m_backingStore->supportsSimultaneousTransactions();
    m_backingStoreIsEphemeral = m_backingStore->isEphemeral();

    IDBDatabaseInfo databaseInfo;
    auto error = m_backingStore->getOrEstablishDatabaseInfo(databaseInfo);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didOpenBackingStore, databaseInfo, error));
}

void UniqueIDBDatabase::didOpenBackingStore(const IDBDatabaseInfo& info, const IDBError& error)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didOpenBackingStore");
    
    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(info);
    m_backingStoreOpenError = error;

    ASSERT(m_isOpeningBackingStore);
    m_isOpeningBackingStore = false;

    if (m_hardClosedForUserDelete)
        return;

    handleDatabaseOperations();
}

void UniqueIDBDatabase::createObjectStore(UniqueIDBDatabaseTransaction& transaction, const IDBObjectStoreInfo& info, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::createObjectStore");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performCreateObjectStore, callbackID, transaction.info().identifier(), info));
}

void UniqueIDBDatabase::performCreateObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performCreateObjectStore");

    ASSERT(m_backingStore);
    m_backingStore->createObjectStore(transactionIdentifier, info);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformCreateObjectStore, callbackIdentifier, error, info));
}

void UniqueIDBDatabase::didPerformCreateObjectStore(uint64_t callbackIdentifier, const IDBError& error, const IDBObjectStoreInfo& info)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformCreateObjectStore");

    if (error.isNull())
        m_databaseInfo->addExistingObjectStore(info);

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::deleteObjectStore(UniqueIDBDatabaseTransaction& transaction, const String& objectStoreName, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::deleteObjectStore");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreName);
    if (!info) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to delete non-existant object store"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performDeleteObjectStore, callbackID, transaction.info().identifier(), info->identifier()));
}

void UniqueIDBDatabase::performDeleteObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performDeleteObjectStore");

    ASSERT(m_backingStore);
    m_backingStore->deleteObjectStore(transactionIdentifier, objectStoreIdentifier);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformDeleteObjectStore, callbackIdentifier, error, objectStoreIdentifier));
}

void UniqueIDBDatabase::didPerformDeleteObjectStore(uint64_t callbackIdentifier, const IDBError& error, uint64_t objectStoreIdentifier)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformDeleteObjectStore");

    if (error.isNull())
        m_databaseInfo->deleteObjectStore(objectStoreIdentifier);

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::renameObjectStore(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, const String& newName, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::renameObjectStore");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    auto* info = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!info) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to rename non-existant object store"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performRenameObjectStore, callbackID, transaction.info().identifier(), objectStoreIdentifier, newName));
}

void UniqueIDBDatabase::performRenameObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performRenameObjectStore");

    ASSERT(m_backingStore);
    m_backingStore->renameObjectStore(transactionIdentifier, objectStoreIdentifier, newName);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformRenameObjectStore, callbackIdentifier, error, objectStoreIdentifier, newName));
}

void UniqueIDBDatabase::didPerformRenameObjectStore(uint64_t callbackIdentifier, const IDBError& error, uint64_t objectStoreIdentifier, const String& newName)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformRenameObjectStore");

    if (error.isNull())
        m_databaseInfo->renameObjectStore(objectStoreIdentifier, newName);

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::clearObjectStore(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::clearObjectStore");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performClearObjectStore, callbackID, transaction.info().identifier(), objectStoreIdentifier));
}

void UniqueIDBDatabase::performClearObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performClearObjectStore");

    ASSERT(m_backingStore);
    m_backingStore->clearObjectStore(transactionIdentifier, objectStoreIdentifier);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformClearObjectStore, callbackIdentifier, error));
}

void UniqueIDBDatabase::didPerformClearObjectStore(uint64_t callbackIdentifier, const IDBError& error)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformClearObjectStore");

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::createIndex(UniqueIDBDatabaseTransaction& transaction, const IDBIndexInfo& info, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::createIndex");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performCreateIndex, callbackID, transaction.info().identifier(), info));
}

void UniqueIDBDatabase::performCreateIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo& info)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performCreateIndex");

    ASSERT(m_backingStore);
    IDBError error = m_backingStore->createIndex(transactionIdentifier, info);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformCreateIndex, callbackIdentifier, error, info));
}

void UniqueIDBDatabase::didPerformCreateIndex(uint64_t callbackIdentifier, const IDBError& error, const IDBIndexInfo& info)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformCreateIndex");

    if (error.isNull()) {
        ASSERT(m_databaseInfo);
        auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(info.objectStoreIdentifier());
        ASSERT(objectStoreInfo);
        objectStoreInfo->addExistingIndex(info);
    }

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::deleteIndex(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, const String& indexName, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::deleteIndex");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to delete index from non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexName);
    if (!indexInfo) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to delete non-existant index"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performDeleteIndex, callbackID, transaction.info().identifier(), objectStoreIdentifier, indexInfo->identifier()));
}

void UniqueIDBDatabase::performDeleteIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const uint64_t indexIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performDeleteIndex");

    ASSERT(m_backingStore);
    m_backingStore->deleteIndex(transactionIdentifier, objectStoreIdentifier, indexIdentifier);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformDeleteIndex, callbackIdentifier, error, objectStoreIdentifier, indexIdentifier));
}

void UniqueIDBDatabase::didPerformDeleteIndex(uint64_t callbackIdentifier, const IDBError& error, uint64_t objectStoreIdentifier, uint64_t indexIdentifier)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformDeleteIndex");

    if (error.isNull()) {
        auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
        if (objectStoreInfo)
            objectStoreInfo->deleteIndex(indexIdentifier);
    }

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::renameIndex(UniqueIDBDatabaseTransaction& transaction, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::renameIndex");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to rename index in non-existant object store"_s });
        return;
    }

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo) {
        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to rename non-existant index"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performRenameIndex, callbackID, transaction.info().identifier(), objectStoreIdentifier, indexIdentifier, newName));
}

void UniqueIDBDatabase::performRenameIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performRenameIndex");

    ASSERT(m_backingStore);
    m_backingStore->renameIndex(transactionIdentifier, objectStoreIdentifier, indexIdentifier, newName);

    IDBError error;
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformRenameIndex, callbackIdentifier, error, objectStoreIdentifier, indexIdentifier, newName));
}

void UniqueIDBDatabase::didPerformRenameIndex(uint64_t callbackIdentifier, const IDBError& error, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformRenameIndex");

    if (error.isNull()) {
        auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
        ASSERT(objectStoreInfo);
        if (objectStoreInfo) {
            auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
            ASSERT(indexInfo);
            indexInfo->rename(newName);
        }
    }

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode, KeyDataCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::putOrAdd");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performPutOrAdd, callbackID, requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), keyData, value, overwriteMode));
}

VM& UniqueIDBDatabase::databaseThreadVM()
{
    ASSERT(!isMainThread());
    static VM* vm = &VM::create().leakRef();
    return *vm;
}

ExecState& UniqueIDBDatabase::databaseThreadExecState()
{
    ASSERT(!isMainThread());

    static NeverDestroyed<Strong<JSGlobalObject>> globalObject(databaseThreadVM(), JSGlobalObject::create(databaseThreadVM(), JSGlobalObject::createStructure(databaseThreadVM(), jsNull())));

    RELEASE_ASSERT(globalObject.get()->globalExec());
    return *globalObject.get()->globalExec();
}

void UniqueIDBDatabase::performPutOrAdd(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData& keyData, const IDBValue& originalRecordValue, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performPutOrAdd");

    ASSERT(m_backingStore);
    ASSERT(objectStoreIdentifier);

    IDBKeyData usedKey;
    IDBError error;

    auto* objectStoreInfo = m_backingStore->infoForObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo) {
        error = IDBError(InvalidStateError, "Object store cannot be found in the backing store"_s);
        postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
        return;
    }

    bool usedKeyIsGenerated = false;
    uint64_t keyNumber;
    auto generatedKeyResetter = WTF::makeScopeExit([this, transactionIdentifier, objectStoreIdentifier, &keyNumber, &usedKeyIsGenerated]() {
        if (usedKeyIsGenerated)
            m_backingStore->revertGeneratedKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
    });
    if (objectStoreInfo->autoIncrement() && !keyData.isValid()) {
        error = m_backingStore->generateKeyNumber(transactionIdentifier, objectStoreIdentifier, keyNumber);
        if (!error.isNull()) {
            postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
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
            error = IDBError(ConstraintError, "Key already exists in the object store"_s);

        if (!error.isNull()) {
            postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
            return;
        }
    }

    // 3.4.1.2 Object Store Storage Operation
    // If ObjectStore has a key path and the key is autogenerated, then inject the key into the value
    // using steps to assign a key to a value using a key path.
    ThreadSafeDataBuffer injectedRecordValue;
    if (usedKeyIsGenerated && objectStoreInfo->keyPath()) {
        VM& vm = databaseThreadVM();
        JSLockHolder locker(vm);
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto value = deserializeIDBValueToJSValue(databaseThreadExecState(), originalRecordValue.data());
        if (value.isUndefined()) {
            postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, IDBError(ConstraintError, "Unable to deserialize record value for record key injection"_s), usedKey));
            return;
        }

        if (!injectIDBKeyIntoScriptValue(databaseThreadExecState(), usedKey, value, objectStoreInfo->keyPath().value())) {
            postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, IDBError(ConstraintError, "Unable to inject record key into record value"_s), usedKey));
            return;
        }

        auto serializedValue = SerializedScriptValue::create(databaseThreadExecState(), value);
        if (UNLIKELY(scope.exception())) {
            postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, IDBError(ConstraintError, "Unable to serialize record value after injecting record key"_s), usedKey));
            return;
        }

        injectedRecordValue = ThreadSafeDataBuffer::copyVector(serializedValue->data());
    }

    // 3.4.1 Object Store Storage Operation
    // ...If a record already exists in store ...
    // then remove the record from store using the steps for deleting records from an object store...
    // This is important because formally deleting it from from the object store also removes it from the appropriate indexes.
    error = m_backingStore->deleteRange(transactionIdentifier, objectStoreIdentifier, usedKey);
    if (!error.isNull()) {
        postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
        return;
    }

    if (injectedRecordValue.data())
        error = m_backingStore->addRecord(transactionIdentifier, *objectStoreInfo, usedKey, { injectedRecordValue, originalRecordValue.blobURLs(), originalRecordValue.sessionID(), originalRecordValue.blobFilePaths() });
    else
        error = m_backingStore->addRecord(transactionIdentifier, *objectStoreInfo, usedKey, originalRecordValue);

    if (!error.isNull()) {
        postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
        return;
    }

    if (overwriteMode != IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor && objectStoreInfo->autoIncrement() && keyData.type() == IndexedDB::KeyType::Number)
        error = m_backingStore->maybeUpdateKeyGeneratorNumber(transactionIdentifier, objectStoreIdentifier, keyData.number());

    generatedKeyResetter.release();
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformPutOrAdd, callbackIdentifier, error, usedKey));
}

void UniqueIDBDatabase::didPerformPutOrAdd(uint64_t callbackIdentifier, const IDBError& error, const IDBKeyData& resultKey)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformPutOrAdd");

    performKeyDataCallback(callbackIdentifier, error, resultKey);
}

void UniqueIDBDatabase::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData, GetResultCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::getRecord");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    if (uint64_t indexIdentifier = requestData.indexIdentifier())
        postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performGetIndexRecord, callbackID, requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), indexIdentifier, requestData.indexRecordType(), getRecordData.keyRangeData));
    else
        postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performGetRecord, callbackID, requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), getRecordData.keyRangeData, getRecordData.type));
}

void UniqueIDBDatabase::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData, GetAllResultsCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::getAllRecords");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performGetAllRecords, callbackID, requestData.transactionIdentifier(), getAllRecordsData));
}

void UniqueIDBDatabase::performGetRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& keyRangeData, IDBGetRecordDataType type)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performGetRecord");

    ASSERT(m_backingStore);

    IDBGetResult result;
    IDBError error = m_backingStore->getRecord(transactionIdentifier, objectStoreIdentifier, keyRangeData, type, result);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformGetRecord, callbackIdentifier, error, result));
}

void UniqueIDBDatabase::performGetIndexRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType recordType, const IDBKeyRangeData& range)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performGetIndexRecord");

    ASSERT(m_backingStore);

    IDBGetResult result;
    IDBError error = m_backingStore->getIndexRecord(transactionIdentifier, objectStoreIdentifier, indexIdentifier, recordType, range, result);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformGetRecord, callbackIdentifier, error, result));
}

void UniqueIDBDatabase::didPerformGetRecord(uint64_t callbackIdentifier, const IDBError& error, const IDBGetResult& result)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformGetRecord");

    performGetResultCallback(callbackIdentifier, error, result);
}

void UniqueIDBDatabase::performGetAllRecords(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performGetAllRecords");

    ASSERT(m_backingStore);

    IDBGetAllResult result;
    IDBError error = m_backingStore->getAllRecords(transactionIdentifier, getAllRecordsData, result);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformGetAllRecords, callbackIdentifier, error, WTFMove(result)));
}

void UniqueIDBDatabase::didPerformGetAllRecords(uint64_t callbackIdentifier, const IDBError& error, const IDBGetAllResult& result)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformGetAllRecords");

    performGetAllResultsCallback(callbackIdentifier, error, result);
}

void UniqueIDBDatabase::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& range, CountCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::getCount");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performGetCount, callbackID, requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), requestData.indexIdentifier(), range));
}

void UniqueIDBDatabase::performGetCount(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& keyRangeData)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performGetCount");

    ASSERT(m_backingStore);
    ASSERT(objectStoreIdentifier);

    uint64_t count;
    IDBError error = m_backingStore->getCount(transactionIdentifier, objectStoreIdentifier, indexIdentifier, keyRangeData, count);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformGetCount, callbackIdentifier, error, count));
}

void UniqueIDBDatabase::didPerformGetCount(uint64_t callbackIdentifier, const IDBError& error, uint64_t count)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformGetCount");

    performCountCallback(callbackIdentifier, error, count);
}

void UniqueIDBDatabase::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::deleteRecord");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performDeleteRecord, callbackID, requestData.transactionIdentifier(), requestData.objectStoreIdentifier(), keyRangeData));
}

void UniqueIDBDatabase::performDeleteRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performDeleteRecord");

    IDBError error = m_backingStore->deleteRange(transactionIdentifier, objectStoreIdentifier, range);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformDeleteRecord, callbackIdentifier, error));
}

void UniqueIDBDatabase::didPerformDeleteRecord(uint64_t callbackIdentifier, const IDBError& error)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformDeleteRecord");

    performErrorCallback(callbackIdentifier, error);
}

void UniqueIDBDatabase::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info, GetResultCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::openCursor");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performOpenCursor, callbackID, requestData.transactionIdentifier(), info));
}

void UniqueIDBDatabase::performOpenCursor(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo& info)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performOpenCursor");

    IDBGetResult result;
    IDBError error = m_backingStore->openCursor(transactionIdentifier, info, result);

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformOpenCursor, callbackIdentifier, error, result));
}

void UniqueIDBDatabase::didPerformOpenCursor(uint64_t callbackIdentifier, const IDBError& error, const IDBGetResult& result)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformOpenCursor");

    performGetResultCallback(callbackIdentifier, error, result);
}

void UniqueIDBDatabase::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data, GetResultCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::iterateCursor");

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performIterateCursor, callbackID, requestData.transactionIdentifier(), requestData.cursorIdentifier(), data));
}

void UniqueIDBDatabase::performIterateCursor(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData& data)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performIterateCursor");

    IDBGetResult result;
    IDBError error = m_backingStore->iterateCursor(transactionIdentifier, cursorIdentifier, data, result);

    if (error.isNull()) {
        auto addResult = m_cursorPrefetches.add(cursorIdentifier);
        if (addResult.isNewEntry)
            postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performPrefetchCursor, transactionIdentifier, cursorIdentifier));
    }

    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformIterateCursor, callbackIdentifier, error, result));
}

void UniqueIDBDatabase::performPrefetchCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_cursorPrefetches.contains(cursorIdentifier));
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performPrefetchCursor");

    if (m_hardClosedForUserDelete || !m_backingStore->prefetchCursor(transactionIdentifier, cursorIdentifier))
        m_cursorPrefetches.remove(cursorIdentifier);
    else
        postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performPrefetchCursor, transactionIdentifier, cursorIdentifier));
}

void UniqueIDBDatabase::didPerformIterateCursor(uint64_t callbackIdentifier, const IDBError& error, const IDBGetResult& result)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformIterateCursor");

    performGetResultCallback(callbackIdentifier, error, result);
}

bool UniqueIDBDatabase::prepareToFinishTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    auto takenTransaction = m_inProgressTransactions.take(transaction.info().identifier());
    if (!takenTransaction)
        return false;

    ASSERT(!m_finishingTransactions.contains(transaction.info().identifier()));
    m_finishingTransactions.set(transaction.info().identifier(), WTFMove(takenTransaction));

    return true;
}

void UniqueIDBDatabase::commitTransaction(UniqueIDBDatabaseTransaction& transaction, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::commitTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(transaction.databaseConnection().database() == this);

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    if (!prepareToFinishTransaction(transaction)) {
        if (!m_openDatabaseConnections.contains(&transaction.databaseConnection())) {
            // This database connection is closing or has already closed, so there is no point in messaging back to it about the commit failing.
            forgetErrorCallback(callbackID);
            return;
        }

        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to commit transaction that is already finishing"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performCommitTransaction, callbackID, transaction.info().identifier()));
}

void UniqueIDBDatabase::performCommitTransaction(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performCommitTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    IDBError error = m_backingStore->commitTransaction(transactionIdentifier);
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformCommitTransaction, callbackIdentifier, error, transactionIdentifier));
}

void UniqueIDBDatabase::didPerformCommitTransaction(uint64_t callbackIdentifier, const IDBError& error, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformCommitTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    performErrorCallback(callbackIdentifier, error);

    transactionCompleted(m_finishingTransactions.take(transactionIdentifier));
}

void UniqueIDBDatabase::abortTransaction(UniqueIDBDatabaseTransaction& transaction, ErrorCallback callback)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::abortTransaction - %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(transaction.databaseConnection().database() == this);

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;

    if (!prepareToFinishTransaction(transaction)) {
        if (!m_openDatabaseConnections.contains(&transaction.databaseConnection())) {
            // This database connection is closing or has already closed, so there is no point in messaging back to it about the abort failing.
            forgetErrorCallback(callbackID);
            return;
        }

        performErrorCallback(callbackID, IDBError { UnknownError, "Attempt to abort transaction that is already finishing"_s });
        return;
    }

    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performAbortTransaction, callbackID, transaction.info().identifier()));
}

void UniqueIDBDatabase::didFinishHandlingVersionChange(UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didFinishHandlingVersionChange");

    ASSERT_UNUSED(transactionIdentifier, !m_versionChangeTransaction || m_versionChangeTransaction->info().identifier() == transactionIdentifier);
    ASSERT_UNUSED(connection, !m_versionChangeDatabaseConnection || m_versionChangeDatabaseConnection.get() == &connection);

    m_versionChangeTransaction = nullptr;
    m_versionChangeDatabaseConnection = nullptr;

    if (m_hardClosedForUserDelete) {
        maybeFinishHardClose();
        return;
    }

    invokeOperationAndTransactionTimer();
}

void UniqueIDBDatabase::performAbortTransaction(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performAbortTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    IDBError error = m_backingStore->abortTransaction(transactionIdentifier);
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformAbortTransaction, callbackIdentifier, error, transactionIdentifier));
}

void UniqueIDBDatabase::didPerformAbortTransaction(uint64_t callbackIdentifier, const IDBError& error, const IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformAbortTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto transaction = m_finishingTransactions.take(transactionIdentifier);
    ASSERT(transaction);

    if (m_versionChangeTransaction && m_versionChangeTransaction->info().identifier() == transactionIdentifier) {
        ASSERT(m_versionChangeTransaction == transaction);
        ASSERT(!m_versionChangeDatabaseConnection || &m_versionChangeTransaction->databaseConnection() == m_versionChangeDatabaseConnection);
        ASSERT(m_versionChangeTransaction->originalDatabaseInfo());
        m_databaseInfo = std::make_unique<IDBDatabaseInfo>(*m_versionChangeTransaction->originalDatabaseInfo());
    }

    performErrorCallback(callbackIdentifier, error);

    transactionCompleted(WTFMove(transaction));
}

void UniqueIDBDatabase::transactionDestroyed(UniqueIDBDatabaseTransaction& transaction)
{
    if (m_versionChangeTransaction == &transaction)
        m_versionChangeTransaction = nullptr;
}

void UniqueIDBDatabase::connectionClosedFromClient(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "(main) UniqueIDBDatabase::connectionClosedFromClient - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier());

    Ref<UniqueIDBDatabaseConnection> protectedConnection(connection);
    m_openDatabaseConnections.remove(&connection);

    if (m_versionChangeDatabaseConnection == &connection) {
        if (m_versionChangeTransaction) {
            m_clientClosePendingDatabaseConnections.add(WTFMove(m_versionChangeDatabaseConnection));

            auto transactionIdentifier = m_versionChangeTransaction->info().identifier();
            if (m_inProgressTransactions.contains(transactionIdentifier)) {
                ASSERT(!m_finishingTransactions.contains(transactionIdentifier));
                connection.abortTransactionWithoutCallback(*m_versionChangeTransaction);
            }

            return;
        }

        m_versionChangeDatabaseConnection = nullptr;
    }

    Deque<RefPtr<UniqueIDBDatabaseTransaction>> pendingTransactions;
    while (!m_pendingTransactions.isEmpty()) {
        auto transaction = m_pendingTransactions.takeFirst();
        if (&transaction->databaseConnection() != &connection)
            pendingTransactions.append(WTFMove(transaction));
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

    if (m_currentOpenDBRequest)
        notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(connection.identifier());

    if (connection.hasNonFinishedTransactions()) {
        m_clientClosePendingDatabaseConnections.add(WTFMove(protectedConnection));
        return;
    }

    if (m_hardClosedForUserDelete) {
        maybeFinishHardClose();
        return;
    }

    // Now that a database connection has closed, previously blocked operations might be runnable.
    invokeOperationAndTransactionTimer();
}

void UniqueIDBDatabase::connectionClosedFromServer(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::connectionClosedFromServer - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier());

    if (m_clientClosePendingDatabaseConnections.contains(&connection)) {
        ASSERT(!m_openDatabaseConnections.contains(&connection));
        ASSERT(!m_serverClosePendingDatabaseConnections.contains(&connection));
        return;
    }

    Ref<UniqueIDBDatabaseConnection> protectedConnection(connection);
    m_openDatabaseConnections.remove(&connection);

    connection.connectionToClient().didCloseFromServer(connection, IDBError::userDeleteError());

    m_serverClosePendingDatabaseConnections.add(WTFMove(protectedConnection));
}

void UniqueIDBDatabase::confirmDidCloseFromServer(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(isMainThread());
    LOG(IndexedDB, "UniqueIDBDatabase::confirmDidCloseFromServer - %s (%" PRIu64 ")", connection.openRequestIdentifier().loggingString().utf8().data(), connection.identifier());

    ASSERT(m_serverClosePendingDatabaseConnections.contains(&connection));
    m_serverClosePendingDatabaseConnections.remove(&connection);
}

void UniqueIDBDatabase::enqueueTransaction(Ref<UniqueIDBDatabaseTransaction>&& transaction)
{
    LOG(IndexedDB, "UniqueIDBDatabase::enqueueTransaction - %s", transaction->info().loggingString().utf8().data());
    ASSERT(!m_hardClosedForUserDelete);

    ASSERT(transaction->info().mode() != IDBTransactionMode::Versionchange);

    m_pendingTransactions.append(WTFMove(transaction));

    invokeOperationAndTransactionTimer();
}

bool UniqueIDBDatabase::isCurrentlyInUse() const
{
    return !m_openDatabaseConnections.isEmpty() || !m_clientClosePendingDatabaseConnections.isEmpty() || !m_pendingOpenDBRequests.isEmpty() || m_currentOpenDBRequest || m_versionChangeDatabaseConnection || m_versionChangeTransaction || m_isOpeningBackingStore || m_deleteBackingStoreInProgress;
}

bool UniqueIDBDatabase::hasUnfinishedTransactions() const
{
    return !m_inProgressTransactions.isEmpty() || !m_finishingTransactions.isEmpty();
}

void UniqueIDBDatabase::invokeOperationAndTransactionTimer()
{
    LOG(IndexedDB, "UniqueIDBDatabase::invokeOperationAndTransactionTimer()");
    RELEASE_ASSERT(!m_hardClosedForUserDelete);
    RELEASE_ASSERT(!m_owningPointerForClose);

    if (!m_operationAndTransactionTimer.isActive())
        m_operationAndTransactionTimer.startOneShot(0_s);
}

void UniqueIDBDatabase::operationAndTransactionTimerFired()
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::operationAndTransactionTimerFired");
    ASSERT(!m_hardClosedForUserDelete);
    ASSERT(isMainThread());

    // This UniqueIDBDatabase might be no longer in use by any web page.
    // Assuming it is not ephemeral, the server should now close it to free up resources.
    if (!m_backingStoreIsEphemeral && !isCurrentlyInUse()) {
        ASSERT(m_pendingTransactions.isEmpty());
        ASSERT(!hasUnfinishedTransactions());

        scheduleShutdownForClose();
        return;
    }

    // The current operation might require multiple attempts to handle, so try to
    // make further progress on it now.
    if (m_currentOpenDBRequest)
        handleCurrentOperation();

    if (!m_currentOpenDBRequest)
        handleDatabaseOperations();

    bool hadDeferredTransactions = false;
    auto transaction = takeNextRunnableTransaction(hadDeferredTransactions);

    if (transaction) {
        m_inProgressTransactions.set(transaction->info().identifier(), transaction);
        for (auto objectStore : transaction->objectStoreIdentifiers()) {
            m_objectStoreTransactionCounts.add(objectStore);
            if (!transaction->isReadOnly()) {
                m_objectStoreWriteTransactions.add(objectStore);
                ASSERT(m_objectStoreTransactionCounts.count(objectStore) == 1 || m_hardClosedForUserDelete);
            }
        }

        activateTransactionInBackingStore(*transaction);

        // If no transactions were deferred, it's possible we can start another transaction right now.
        if (!hadDeferredTransactions)
            invokeOperationAndTransactionTimer();
    }
}

void UniqueIDBDatabase::activateTransactionInBackingStore(UniqueIDBDatabaseTransaction& transaction)
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::activateTransactionInBackingStore");
    ASSERT(isMainThread());

    RefPtr<UniqueIDBDatabaseTransaction> refTransaction(&transaction);

    ErrorCallback callback = [refTransaction](const IDBError& error) {
        refTransaction->didActivateInBackingStore(error);
    };

    uint64_t callbackID = storeCallbackOrFireError(WTFMove(callback));
    if (!callbackID)
        return;
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performActivateTransactionInBackingStore, callbackID, transaction.info()));
}

void UniqueIDBDatabase::performActivateTransactionInBackingStore(uint64_t callbackIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "(db) UniqueIDBDatabase::performActivateTransactionInBackingStore");

    IDBError error = m_backingStore->beginTransaction(info);
    postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::didPerformActivateTransactionInBackingStore, callbackIdentifier, error));
}

void UniqueIDBDatabase::didPerformActivateTransactionInBackingStore(uint64_t callbackIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "(main) UniqueIDBDatabase::didPerformActivateTransactionInBackingStore");

    if (m_hardClosedForUserDelete)
        return;

    invokeOperationAndTransactionTimer();

    performErrorCallback(callbackIdentifier, error);
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

    if (!m_backingStoreSupportsSimultaneousTransactions && hasUnfinishedTransactions()) {
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
    ASSERT(!m_finishingTransactions.contains(transaction->info().identifier()));
    ASSERT(isMainThread());

    for (auto objectStore : transaction->objectStoreIdentifiers()) {
        if (!transaction->isReadOnly()) {
            m_objectStoreWriteTransactions.remove(objectStore);
            ASSERT(m_objectStoreTransactionCounts.count(objectStore) == 1 || m_hardClosedForUserDelete);
        }
        m_objectStoreTransactionCounts.remove(objectStore);
    }

    if (!transaction->databaseConnection().hasNonFinishedTransactions())
        m_clientClosePendingDatabaseConnections.remove(&transaction->databaseConnection());

    if (m_versionChangeTransaction == transaction)
        m_versionChangeTransaction = nullptr;

    // It's possible that this database had its backing store deleted but there were a few outstanding asynchronous operations.
    // If this transaction completing was the last of those operations, we can finally delete this UniqueIDBDatabase.
    if (m_clientClosePendingDatabaseConnections.isEmpty() && m_pendingOpenDBRequests.isEmpty() && !m_databaseInfo) {
        scheduleShutdownForClose();
        return;
    }

    // Previously blocked operations might be runnable.
    if (!m_hardClosedForUserDelete)
        invokeOperationAndTransactionTimer();
    else
        maybeFinishHardClose();
}

void UniqueIDBDatabase::postDatabaseTask(CrossThreadTask&& task)
{
    m_databaseQueue.append(WTFMove(task));
    m_server.postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::executeNextDatabaseTask));
}

void UniqueIDBDatabase::postDatabaseTaskReply(CrossThreadTask&& task)
{
    m_databaseReplyQueue.append(WTFMove(task));
    m_server.postDatabaseTaskReply(createCrossThreadTask(*this, &UniqueIDBDatabase::executeNextDatabaseTaskReply));
}

void UniqueIDBDatabase::executeNextDatabaseTask()
{
    ASSERT(!isMainThread());
    ASSERT(!m_databaseQueue.isKilled());

    auto task = m_databaseQueue.tryGetMessage();
    ASSERT(task);

    task->performTask();
}

void UniqueIDBDatabase::executeNextDatabaseTaskReply()
{
    ASSERT(isMainThread());
    ASSERT(!m_databaseReplyQueue.isKilled());

    auto task = m_databaseReplyQueue.tryGetMessage();
    ASSERT(task);

    task->performTask();

    // If this database was force closed (e.g. for a user delete) and there are no more
    // cleanup tasks left, delete this.
    maybeFinishHardClose();
}

void UniqueIDBDatabase::maybeFinishHardClose()
{
    if (m_owningPointerForClose && isDoneWithHardClose()) {
        if (m_owningPointerReleaseScheduled)
            return;
        m_owningPointerReleaseScheduled = true;

        callOnMainThread([this] {
            ASSERT(isDoneWithHardClose());
            m_owningPointerForClose = nullptr;
        });
    }
}

bool UniqueIDBDatabase::isDoneWithHardClose()
{
    return m_databaseReplyQueue.isKilled() && m_clientClosePendingDatabaseConnections.isEmpty() && m_serverClosePendingDatabaseConnections.isEmpty();
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
    LOG(IndexedDB, "UniqueIDBDatabase::immediateCloseForUserDelete - Cancelling (%i, %i, %i, %i) callbacks", m_errorCallbacks.size(), m_keyDataCallbacks.size(), m_getResultCallbacks.size(), m_countCallbacks.size());

    ASSERT(isMainThread());

    // Error out all transactions
    for (auto& identifier : copyToVector(m_inProgressTransactions.keys()))
        m_inProgressTransactions.get(identifier)->abortWithoutCallback();

    ASSERT(m_inProgressTransactions.isEmpty());

    for (auto& transaction : m_pendingTransactions)
        transaction->databaseConnection().deleteTransaction(*transaction);
    m_pendingTransactions.clear();
    m_objectStoreTransactionCounts.clear();
    m_objectStoreWriteTransactions.clear();

    // Error out all pending callbacks
    IDBError error = IDBError::userDeleteError();
    IDBKeyData keyData;
    IDBGetResult getResult;
    IDBGetAllResult getAllResult;

    while (!m_callbackQueue.isEmpty()) {
        auto identifier = m_callbackQueue.first();
        if (m_errorCallbacks.contains(identifier))
            performErrorCallback(identifier, error);
        else if (m_keyDataCallbacks.contains(identifier))
            performKeyDataCallback(identifier, error, keyData);
        else if (m_getResultCallbacks.contains(identifier))
            performGetResultCallback(identifier, error, getResult);
        else if (m_countCallbacks.contains(identifier))
            performCountCallback(identifier, error, 0);
        else if (m_getAllResultsCallbacks.contains(identifier))
            performGetAllResultsCallback(identifier, error, getAllResult);
        else
            ASSERT_NOT_REACHED();
    }

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

    // Cancel the operation timer
    m_operationAndTransactionTimer.stop();

    // Set up the database to remain alive-but-inert until all of its background activity finishes and all
    // database connections confirm that they have closed.
    m_hardClosedForUserDelete = true;

    // If this database already owns itself, it is already closing on the background thread.
    // After that close completes, the next database thread task will be "delete all currently closed databases"
    // which will also cover this database.
    if (m_owningPointerForClose)
        return;

    // Otherwise, this database is still potentially active.
    // So we'll have it own itself and then perform a clean unconditional delete on the background thread.
    m_owningPointerForClose = m_server.closeAndTakeUniqueIDBDatabase(*this);
    postDatabaseTask(createCrossThreadTask(*this, &UniqueIDBDatabase::performUnconditionalDeleteBackingStore));
}

void UniqueIDBDatabase::performErrorCallback(uint64_t callbackIdentifier, const IDBError& error)
{
    auto callback = m_errorCallbacks.take(callbackIdentifier);
    ASSERT(callback || m_hardClosedForUserDelete);
    if (callback) {
        callback(error);
        ASSERT(m_callbackQueue.first() == callbackIdentifier);
        m_callbackQueue.removeFirst();
    }
}

void UniqueIDBDatabase::performKeyDataCallback(uint64_t callbackIdentifier, const IDBError& error, const IDBKeyData& resultKey)
{
    auto callback = m_keyDataCallbacks.take(callbackIdentifier);
    ASSERT(callback || m_hardClosedForUserDelete);
    if (callback) {
        callback(error, resultKey);
        ASSERT(m_callbackQueue.first() == callbackIdentifier);
        m_callbackQueue.removeFirst();
    }
}

void UniqueIDBDatabase::performGetResultCallback(uint64_t callbackIdentifier, const IDBError& error, const IDBGetResult& resultData)
{
    auto callback = m_getResultCallbacks.take(callbackIdentifier);
    ASSERT(callback || m_hardClosedForUserDelete);
    if (callback) {
        callback(error, resultData);
        ASSERT(m_callbackQueue.first() == callbackIdentifier);
        m_callbackQueue.removeFirst();
    }
}

void UniqueIDBDatabase::performGetAllResultsCallback(uint64_t callbackIdentifier, const IDBError& error, const IDBGetAllResult& resultData)
{
    auto callback = m_getAllResultsCallbacks.take(callbackIdentifier);
    ASSERT(callback || m_hardClosedForUserDelete);
    if (callback) {
        callback(error, resultData);
        ASSERT(m_callbackQueue.first() == callbackIdentifier);
        m_callbackQueue.removeFirst();
    }
}

void UniqueIDBDatabase::performCountCallback(uint64_t callbackIdentifier, const IDBError& error, uint64_t count)
{
    auto callback = m_countCallbacks.take(callbackIdentifier);
    ASSERT(callback || m_hardClosedForUserDelete);
    if (callback) {
        callback(error, count);
        ASSERT(m_callbackQueue.first() == callbackIdentifier);
        m_callbackQueue.removeFirst();
    }
}

void UniqueIDBDatabase::forgetErrorCallback(uint64_t callbackIdentifier)
{
    ASSERT(m_errorCallbacks.contains(callbackIdentifier));
    ASSERT(m_callbackQueue.last() == callbackIdentifier);
    m_callbackQueue.removeLast();
    m_errorCallbacks.remove(callbackIdentifier);
}

void UniqueIDBDatabase::setQuota(uint64_t quota)
{
    if (m_backingStore)
        m_backingStore->setQuota(quota);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
