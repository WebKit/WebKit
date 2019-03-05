/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "IDBConnectionProxy.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"
#include "IDBDatabase.h"
#include "IDBGetRecordData.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>

namespace WebCore {
namespace IDBClient {

IDBConnectionProxy::IDBConnectionProxy(IDBConnectionToServer& connection)
    : m_connectionToServer(connection)
    , m_serverConnectionIdentifier(connection.identifier())
{
    ASSERT(isMainThread());
}

void IDBConnectionProxy::ref()
{
    m_connectionToServer.ref();
}

void IDBConnectionProxy::deref()
{
    m_connectionToServer.deref();
}

Ref<IDBOpenDBRequest> IDBConnectionProxy::openDatabase(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
{
    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);

        request = IDBOpenDBRequest::createOpenRequest(context, *this, databaseIdentifier, version);
        ASSERT(!m_openDBRequestMap.contains(request->resourceIdentifier()));
        m_openDBRequestMap.set(request->resourceIdentifier(), request.get());
    }

    callConnectionOnMainThread(&IDBConnectionToServer::openDatabase, IDBRequestData(*this, *request));

    return request.releaseNonNull();
}

Ref<IDBOpenDBRequest> IDBConnectionProxy::deleteDatabase(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier)
{
    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);

        request = IDBOpenDBRequest::createDeleteRequest(context, *this, databaseIdentifier);
        ASSERT(!m_openDBRequestMap.contains(request->resourceIdentifier()));
        m_openDBRequestMap.set(request->resourceIdentifier(), request.get());
    }

    callConnectionOnMainThread(&IDBConnectionToServer::deleteDatabase, IDBRequestData(*this, *request));

    return request.releaseNonNull();
}

void IDBConnectionProxy::didOpenDatabase(const IDBResultData& resultData)
{
    completeOpenDBRequest(resultData);
}

void IDBConnectionProxy::didDeleteDatabase(const IDBResultData& resultData)
{
    completeOpenDBRequest(resultData);
}

void IDBConnectionProxy::completeOpenDBRequest(const IDBResultData& resultData)
{
    ASSERT(isMainThread());

    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);
        request = m_openDBRequestMap.take(resultData.requestIdentifier());
    }

    if (!request)
        return;

    request->performCallbackOnOriginThread(*request, &IDBOpenDBRequest::requestCompleted, resultData);
}

void IDBConnectionProxy::createObjectStore(TransactionOperation& operation, const IDBObjectStoreInfo& info)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::createObjectStore, requestData, info);
}

void IDBConnectionProxy::renameObjectStore(TransactionOperation& operation, uint64_t objectStoreIdentifier, const String& newName)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::renameObjectStore, requestData, objectStoreIdentifier, newName);
}

void IDBConnectionProxy::renameIndex(TransactionOperation& operation, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::renameIndex, requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBConnectionProxy::deleteObjectStore(TransactionOperation& operation, const String& objectStoreName)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::deleteObjectStore, requestData, objectStoreName);
}

void IDBConnectionProxy::clearObjectStore(TransactionOperation& operation, uint64_t objectStoreIdentifier)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::clearObjectStore, requestData, objectStoreIdentifier);
}

void IDBConnectionProxy::createIndex(TransactionOperation& operation, const IDBIndexInfo& info)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::createIndex, requestData, info);
}

void IDBConnectionProxy::deleteIndex(TransactionOperation& operation, uint64_t objectStoreIdentifier, const String& indexName)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::deleteIndex, requestData, WTFMove(objectStoreIdentifier), indexName);
}

void IDBConnectionProxy::putOrAdd(TransactionOperation& operation, IDBKeyData&& keyData, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode mode)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::putOrAdd, requestData, keyData, value, mode);
}

void IDBConnectionProxy::getRecord(TransactionOperation& operation, const IDBGetRecordData& getRecordData)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::getRecord, requestData, getRecordData);
}

void IDBConnectionProxy::getAllRecords(TransactionOperation& operation, const IDBGetAllRecordsData& getAllRecordsData)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::getAllRecords, requestData, getAllRecordsData);
}

void IDBConnectionProxy::getCount(TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::getCount, requestData, keyRange);
}

void IDBConnectionProxy::deleteRecord(TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::deleteRecord, requestData, keyRange);
}

void IDBConnectionProxy::openCursor(TransactionOperation& operation, const IDBCursorInfo& info)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::openCursor, requestData, info);
}

void IDBConnectionProxy::iterateCursor(TransactionOperation& operation, const IDBIterateCursorData& data)
{
    const IDBRequestData requestData { operation };
    saveOperation(operation);

    callConnectionOnMainThread(&IDBConnectionToServer::iterateCursor, requestData, data);
}

void IDBConnectionProxy::saveOperation(TransactionOperation& operation)
{
    Locker<Lock> locker(m_transactionOperationLock);

    ASSERT(!m_activeOperations.contains(operation.identifier()));
    m_activeOperations.set(operation.identifier(), &operation);
}

void IDBConnectionProxy::completeOperation(const IDBResultData& resultData)
{
    RefPtr<TransactionOperation> operation;
    {
        Locker<Lock> locker(m_transactionOperationLock);
        operation = m_activeOperations.take(resultData.requestIdentifier());
    }

    if (!operation)
        return;

    operation->transitionToComplete(resultData, WTFMove(operation));
}

void IDBConnectionProxy::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    callConnectionOnMainThread(&IDBConnectionToServer::abortOpenAndUpgradeNeeded, databaseConnectionIdentifier, transactionIdentifier);
}

void IDBConnectionProxy::fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    RefPtr<IDBDatabase> database;
    {
        Locker<Lock> locker(m_databaseConnectionMapLock);
        database = m_databaseConnectionMap.get(databaseConnectionIdentifier);
    }

    if (!database)
        return;

    database->performCallbackOnOriginThread(*database, &IDBDatabase::fireVersionChangeEvent, requestIdentifier, requestedVersion);
}

void IDBConnectionProxy::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    callConnectionOnMainThread(&IDBConnectionToServer::didFireVersionChangeEvent, databaseConnectionIdentifier, requestIdentifier);
}

void IDBConnectionProxy::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    ASSERT(isMainThread());

    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);
        request = m_openDBRequestMap.get(requestIdentifier);
    }

    if (!request)
        return;

    request->performCallbackOnOriginThread(*request, &IDBOpenDBRequest::requestBlocked, oldVersion, newVersion);
}

void IDBConnectionProxy::openDBRequestCancelled(const IDBRequestData& requestData)
{
    callConnectionOnMainThread(&IDBConnectionToServer::openDBRequestCancelled, requestData);
}

void IDBConnectionProxy::establishTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!hasRecordOfTransaction(transaction));
        m_pendingTransactions.set(transaction.info().identifier(), &transaction);
    }

    callConnectionOnMainThread(&IDBConnectionToServer::establishTransaction, transaction.database().databaseConnectionIdentifier(), transaction.info());
}

void IDBConnectionProxy::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_pendingTransactions.take(transactionIdentifier);
    }

    if (!transaction)
        return;

    transaction->performCallbackOnOriginThread(*transaction, &IDBTransaction::didStart, error);
}

void IDBConnectionProxy::commitTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!m_committingTransactions.contains(transaction.info().identifier()));
        m_committingTransactions.set(transaction.info().identifier(), &transaction);
    }

    callConnectionOnMainThread(&IDBConnectionToServer::commitTransaction, transaction.info().identifier());
}

void IDBConnectionProxy::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_committingTransactions.take(transactionIdentifier);
    }

    if (!transaction)
        return;

    transaction->performCallbackOnOriginThread(*transaction, &IDBTransaction::didCommit, error);
}

void IDBConnectionProxy::abortTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!m_abortingTransactions.contains(transaction.info().identifier()));
        m_abortingTransactions.set(transaction.info().identifier(), &transaction);
    }

    callConnectionOnMainThread(&IDBConnectionToServer::abortTransaction, transaction.info().identifier());
}

void IDBConnectionProxy::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_abortingTransactions.take(transactionIdentifier);
    }

    if (!transaction)
        return;

    transaction->performCallbackOnOriginThread(*transaction, &IDBTransaction::didAbort, error);
}

bool IDBConnectionProxy::hasRecordOfTransaction(const IDBTransaction& transaction) const
{
    ASSERT(m_transactionMapLock.isLocked());

    auto identifier = transaction.info().identifier();
    return m_pendingTransactions.contains(identifier) || m_committingTransactions.contains(identifier) || m_abortingTransactions.contains(identifier);
}

void IDBConnectionProxy::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, IDBTransaction& transaction)
{
    callConnectionOnMainThread(&IDBConnectionToServer::didFinishHandlingVersionChangeTransaction, databaseConnectionIdentifier, transaction.info().identifier());
}

void IDBConnectionProxy::databaseConnectionPendingClose(IDBDatabase& database)
{
    callConnectionOnMainThread(&IDBConnectionToServer::databaseConnectionPendingClose, database.databaseConnectionIdentifier());
}

void IDBConnectionProxy::databaseConnectionClosed(IDBDatabase& database)
{
    callConnectionOnMainThread(&IDBConnectionToServer::databaseConnectionClosed, database.databaseConnectionIdentifier());
}

void IDBConnectionProxy::didCloseFromServer(uint64_t databaseConnectionIdentifier, const IDBError& error)
{
    RefPtr<IDBDatabase> database;
    {
        Locker<Lock> locker(m_databaseConnectionMapLock);
        database = m_databaseConnectionMap.get(databaseConnectionIdentifier);
    }

    // If the IDBDatabase object is gone, message back to the server so it doesn't hang
    // waiting for a reply that will never come.
    if (!database) {
        m_connectionToServer.confirmDidCloseFromServer(databaseConnectionIdentifier);
        return;
    }

    database->performCallbackOnOriginThread(*database, &IDBDatabase::didCloseFromServer, error);
}

void IDBConnectionProxy::confirmDidCloseFromServer(IDBDatabase& database)
{
    callConnectionOnMainThread(&IDBConnectionToServer::confirmDidCloseFromServer, database.databaseConnectionIdentifier());
}

void IDBConnectionProxy::connectionToServerLost(const IDBError& error)
{
    Vector<uint64_t> databaseConnectionIdentifiers;
    {
        Locker<Lock> locker(m_databaseConnectionMapLock);
        databaseConnectionIdentifiers = copyToVector(m_databaseConnectionMap.keys());
    }

    for (auto connectionIdentifier : databaseConnectionIdentifiers) {
        RefPtr<IDBDatabase> database;
        {
            Locker<Lock> locker(m_databaseConnectionMapLock);
            database = m_databaseConnectionMap.get(connectionIdentifier);
        }

        if (!database)
            continue;

        database->performCallbackOnOriginThread(*database, &IDBDatabase::connectionToServerLost, error);
    }

    Vector<IDBResourceIdentifier> openDBRequestIdentifiers;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);
        openDBRequestIdentifiers = copyToVector(m_openDBRequestMap.keys());
    }

    for (auto& requestIdentifier : openDBRequestIdentifiers) {
        RefPtr<IDBOpenDBRequest> request;
        {
            Locker<Lock> locker(m_openDBRequestMapLock);
            request = m_openDBRequestMap.get(requestIdentifier);
        }

        if (!request)
            continue;

        auto result = IDBResultData::error(requestIdentifier, error);
        request->performCallbackOnOriginThread(*request, &IDBOpenDBRequest::requestCompleted, result);
    }
}

void IDBConnectionProxy::scheduleMainThreadTasks()
{
    Locker<Lock> locker(m_mainThreadTaskLock);
    if (m_mainThreadProtector)
        return;

    m_mainThreadProtector = &m_connectionToServer;
    callOnMainThread([this] {
        handleMainThreadTasks();
    });
}

void IDBConnectionProxy::handleMainThreadTasks()
{
    RefPtr<IDBConnectionToServer> protector;
    {
        Locker<Lock> locker(m_mainThreadTaskLock);
        ASSERT(m_mainThreadProtector);
        protector = WTFMove(m_mainThreadProtector);
    }

    while (auto task = m_mainThreadQueue.tryGetMessage())
        task->performTask();
}

void IDBConnectionProxy::getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, Function<void (const Vector<String>&)>&& callback)
{
    // This method is only meant to be called by the web inspector on the main thread.
    RELEASE_ASSERT(isMainThread());

    m_connectionToServer.getAllDatabaseNames(mainFrameOrigin, openingOrigin, WTFMove(callback));
}

void IDBConnectionProxy::registerDatabaseConnection(IDBDatabase& database)
{
    Locker<Lock> locker(m_databaseConnectionMapLock);

    ASSERT(!m_databaseConnectionMap.contains(database.databaseConnectionIdentifier()));
    m_databaseConnectionMap.set(database.databaseConnectionIdentifier(), &database);
}

void IDBConnectionProxy::unregisterDatabaseConnection(IDBDatabase& database)
{
    Locker<Lock> locker(m_databaseConnectionMapLock);

    ASSERT(!m_databaseConnectionMap.contains(database.databaseConnectionIdentifier()) || m_databaseConnectionMap.get(database.databaseConnectionIdentifier()) == &database);
    m_databaseConnectionMap.remove(database.databaseConnectionIdentifier());
}

void IDBConnectionProxy::forgetActiveOperations(const Vector<RefPtr<TransactionOperation>>& operations)
{
    Locker<Lock> locker(m_transactionOperationLock);

    for (auto& operation : operations)
        m_activeOperations.remove(operation->identifier());
}

void IDBConnectionProxy::forgetTransaction(IDBTransaction& transaction)
{
    Locker<Lock> locker(m_transactionMapLock);

    m_pendingTransactions.remove(transaction.info().identifier());
    m_committingTransactions.remove(transaction.info().identifier());
    m_abortingTransactions.remove(transaction.info().identifier());
}

template<typename KeyType, typename ValueType>
void removeItemsMatchingCurrentThread(HashMap<KeyType, ValueType>& map)
{
    auto& currentThread = Thread::current();

    Vector<KeyType> keys;
    keys.reserveInitialCapacity(map.size());
    for (auto& iterator : map) {
        if (&iterator.value->originThread() == &currentThread)
            keys.uncheckedAppend(iterator.key);
    }

    for (auto& key : keys)
        map.remove(key);
}

void IDBConnectionProxy::forgetActivityForCurrentThread()
{
    ASSERT(!isMainThread());

    {
        Locker<Lock> lock(m_databaseConnectionMapLock);
        removeItemsMatchingCurrentThread(m_databaseConnectionMap);
    }
    {
        Locker<Lock> lock(m_openDBRequestMapLock);
        removeItemsMatchingCurrentThread(m_openDBRequestMap);
    }
    {
        Locker<Lock> lock(m_transactionMapLock);
        removeItemsMatchingCurrentThread(m_pendingTransactions);
        removeItemsMatchingCurrentThread(m_committingTransactions);
        removeItemsMatchingCurrentThread(m_abortingTransactions);
    }
    {
        Locker<Lock> lock(m_transactionOperationLock);
        removeItemsMatchingCurrentThread(m_activeOperations);
    }
}

} // namesapce IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
