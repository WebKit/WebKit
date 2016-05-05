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

#include "IDBDatabase.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
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

// FIXME: Temporarily required during bringup of IDB-in-Workers.
// Once all IDB object reliance on the IDBConnectionToServer has been shifted to reliance on
// IDBConnectionProxy, remove this.
IDBConnectionToServer& IDBConnectionProxy::connectionToServer()
{
    ASSERT(isMainThread());
    return m_connectionToServer;
}

RefPtr<IDBOpenDBRequest> IDBConnectionProxy::openDatabase(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
{
    // FIXME: Handle Workers
    if (!isMainThread())
        return nullptr;

    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);

        request = IDBOpenDBRequest::createOpenRequest(context, *this, databaseIdentifier, version);
        ASSERT(!m_openDBRequestMap.contains(request->resourceIdentifier()));
        m_openDBRequestMap.set(request->resourceIdentifier(), request.get());
    }

    IDBRequestData requestData(*this, *request);

    // FIXME: For workers, marshall this to the main thread if necessary.
    m_connectionToServer.openDatabase(requestData);

    return request;
}

RefPtr<IDBOpenDBRequest> IDBConnectionProxy::deleteDatabase(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier)
{
    // FIXME: Handle Workers
    if (!isMainThread())
        return nullptr;

    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);

        request = IDBOpenDBRequest::createDeleteRequest(context, *this, databaseIdentifier);
        ASSERT(!m_openDBRequestMap.contains(request->resourceIdentifier()));
        m_openDBRequestMap.set(request->resourceIdentifier(), request.get());
    }

    IDBRequestData requestData(*this, *request);

    // FIXME: For workers, marshall this to the main thread if necessary.
    m_connectionToServer.deleteDatabase(requestData);

    return request;
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
        request = m_openDBRequestMap.get(resultData.requestIdentifier());
        ASSERT(request);
    }

    request->requestCompleted(resultData);
}

void IDBConnectionProxy::createObjectStore(TransactionOperation& operation, const IDBObjectStoreInfo& info)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.createObjectStore(requestData, info);
}

void IDBConnectionProxy::deleteObjectStore(TransactionOperation& operation, const String& objectStoreName)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.deleteObjectStore(requestData, objectStoreName);
}

void IDBConnectionProxy::clearObjectStore(TransactionOperation& operation, uint64_t objectStoreIdentifier)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBConnectionProxy::createIndex(TransactionOperation& operation, const IDBIndexInfo& info)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.createIndex(requestData, info);
}

void IDBConnectionProxy::deleteIndex(TransactionOperation& operation, uint64_t objectStoreIdentifier, const String& indexName)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBConnectionProxy::putOrAdd(TransactionOperation& operation, IDBKey* key, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode mode)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.putOrAdd(requestData, key, value, mode);
}

void IDBConnectionProxy::getRecord(TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.getRecord(requestData, keyRange);
}

void IDBConnectionProxy::getCount(TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.getCount(requestData, keyRange);
}

void IDBConnectionProxy::deleteRecord(TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.deleteRecord(requestData, keyRange);
}

void IDBConnectionProxy::openCursor(TransactionOperation& operation, const IDBCursorInfo& info)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.openCursor(requestData, info);
}

void IDBConnectionProxy::iterateCursor(TransactionOperation& operation, const IDBKeyData& key, unsigned long count)
{
    IDBRequestData requestData(operation);
    saveOperation(operation);

    // FIXME: Handle worker thread marshalling.

    m_connectionToServer.iterateCursor(requestData, key, count);
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

    ASSERT(operation);

    // FIXME: Handle getting operation->completed() onto the correct thread via the IDBTransaction.
    
    operation->completed(resultData);
}

void IDBConnectionProxy::fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    ASSERT(isMainThread());

    RefPtr<IDBDatabase> database;
    {
        Locker<Lock> locker(m_databaseConnectionMapLock);
        database = m_databaseConnectionMap.get(databaseConnectionIdentifier);
    }

    if (!database)
        return;

    database->fireVersionChangeEvent(requestIdentifier, requestedVersion);
}

void IDBConnectionProxy::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier);
}

void IDBConnectionProxy::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    ASSERT(isMainThread());

    RefPtr<IDBOpenDBRequest> request;
    {
        Locker<Lock> locker(m_openDBRequestMapLock);
        request = m_openDBRequestMap.get(requestIdentifier);
    }

    ASSERT(request);

    request->requestBlocked(oldVersion, newVersion);
}

void IDBConnectionProxy::establishTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!hasRecordOfTransaction(transaction));
        m_pendingTransactions.set(transaction.info().identifier(), &transaction);
    }

    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.establishTransaction(transaction.database().databaseConnectionIdentifier(), transaction.info());
}

void IDBConnectionProxy::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_pendingTransactions.take(transactionIdentifier);
    }

    ASSERT(transaction);

    // FIXME: Handle hopping to the Worker thread here if necessary.

    transaction->didStart(error);
}

void IDBConnectionProxy::commitTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!m_committingTransactions.contains(transaction.info().identifier()));
        m_committingTransactions.set(transaction.info().identifier(), &transaction);
    }

    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.commitTransaction(transaction.info().identifier());
}

void IDBConnectionProxy::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_committingTransactions.take(transactionIdentifier);
    }

    ASSERT(transaction);

    // FIXME: Handle hopping to the Worker thread here if necessary.

    transaction->didCommit(error);
}

void IDBConnectionProxy::abortTransaction(IDBTransaction& transaction)
{
    {
        Locker<Lock> locker(m_transactionMapLock);
        ASSERT(!m_abortingTransactions.contains(transaction.info().identifier()));
        m_abortingTransactions.set(transaction.info().identifier(), &transaction);
    }

    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.abortTransaction(transaction.info().identifier());
}

void IDBConnectionProxy::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<IDBTransaction> transaction;
    {
        Locker<Lock> locker(m_transactionMapLock);
        transaction = m_abortingTransactions.take(transactionIdentifier);
    }

    ASSERT(transaction);

    // FIXME: Handle hopping to the Worker thread here if necessary.

    transaction->didAbort(error);
}

bool IDBConnectionProxy::hasRecordOfTransaction(const IDBTransaction& transaction) const
{
    ASSERT(m_transactionMapLock.isLocked());

    auto identifier = transaction.info().identifier();
    return m_pendingTransactions.contains(identifier) || m_committingTransactions.contains(identifier) || m_abortingTransactions.contains(identifier);
}

void IDBConnectionProxy::didFinishHandlingVersionChangeTransaction(IDBTransaction& transaction)
{
    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.didFinishHandlingVersionChangeTransaction(transaction.info().identifier());
}

void IDBConnectionProxy::databaseConnectionClosed(IDBDatabase& database)
{
    // FIXME: Handle Workers
    if (!isMainThread())
        return;

    m_connectionToServer.databaseConnectionClosed(database.databaseConnectionIdentifier());
}

void IDBConnectionProxy::getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, std::function<void (const Vector<String>&)> callback)
{
    // This method is only meant to be called by the web inspector on the main thread.
    RELEASE_ASSERT(isMainThread());

    m_connectionToServer.getAllDatabaseNames(mainFrameOrigin, openingOrigin, callback);
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

    ASSERT(m_databaseConnectionMap.contains(database.databaseConnectionIdentifier()));
    ASSERT(m_databaseConnectionMap.get(database.databaseConnectionIdentifier()) == &database);
    m_databaseConnectionMap.remove(database.databaseConnectionIdentifier());
}

} // namesapce IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
