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
#include "IDBServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"
#include <wtf/Locker.h>
#include <wtf/MainThread.h>

namespace WebCore {
namespace IDBServer {

Ref<IDBServer> IDBServer::create()
{
    return adoptRef(*new IDBServer());
}

IDBServer::IDBServer()
{
    Locker<Lock> locker(m_databaseThreadCreationLock);
    m_threadID = createThread(IDBServer::databaseThreadEntry, this, "IndexedDatabase Server");
}

void IDBServer::registerConnection(IDBConnectionToClient& connection)
{
    ASSERT(!m_connectionMap.contains(connection.identifier()));
    m_connectionMap.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(IDBConnectionToClient& connection)
{
    ASSERT(m_connectionMap.contains(connection.identifier()));
    ASSERT(m_connectionMap.get(connection.identifier()) == &connection);

    m_connectionMap.remove(connection.identifier());
}

void IDBServer::registerTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!m_transactions.contains(transaction.info().identifier()));
    m_transactions.set(transaction.info().identifier(), &transaction);
}

void IDBServer::unregisterTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(m_transactions.contains(transaction.info().identifier()));
    ASSERT(m_transactions.get(transaction.info().identifier()) == &transaction);

    m_transactions.remove(transaction.info().identifier());
}

void IDBServer::registerDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.set(connection.identifier(), &connection);
}

void IDBServer::unregisterDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.remove(connection.identifier());
}

UniqueIDBDatabase& IDBServer::getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier& identifier)
{
    auto uniqueIDBDatabase = m_uniqueIDBDatabaseMap.add(identifier, nullptr);
    if (uniqueIDBDatabase.isNewEntry)
        uniqueIDBDatabase.iterator->value = UniqueIDBDatabase::create(*this, identifier);

    return *uniqueIDBDatabase.iterator->value;
}

std::unique_ptr<IDBBackingStore> IDBServer::createBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());

    // FIXME: For now we only have the in-memory backing store, which we'll continue to use for private browsing.
    // Once it's time for persistent backing stores this is where we'll calculate the correct path on disk
    // and create it.

    return MemoryIDBBackingStore::create(identifier);
}

void IDBServer::openDatabase(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDatabase");

    auto& uniqueIDBDatabase = getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());

    auto connection = m_connectionMap.get(requestData.requestIdentifier().connectionIdentifier());
    if (!connection) {
        // If the connection back to the client is gone, there's no way to open the database as
        // well as no way to message back failure.
        return;
    }

    uniqueIDBDatabase.openDatabaseConnection(*connection, requestData);
}

void IDBServer::deleteDatabase(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::deleteDatabase - %s", requestData.databaseIdentifier().debugString().utf8().data());
    
    auto connection = m_connectionMap.get(requestData.requestIdentifier().connectionIdentifier());
    if (!connection) {
        // If the connection back to the client is gone, there's no way to delete the database as
        // well as no way to message back failure.
        return;
    }

    auto* database = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
    if (!database) {
        connection->didDeleteDatabase(IDBResultData::deleteDatabaseSuccess(requestData.requestIdentifier(), IDBDatabaseInfo(requestData.databaseIdentifier().databaseName(), 0)));
        return;
    }

    database->handleDelete(*connection, requestData);
}

void IDBServer::deleteUniqueIDBDatabase(UniqueIDBDatabase& database)
{
    LOG(IndexedDB, "IDBServer::deleteUniqueIDBDatabase");

    auto deletedDatabase = m_uniqueIDBDatabaseMap.take(database.identifier());
    ASSERT_UNUSED(deletedDatabase, deletedDatabase.get() == &database);
}

void IDBServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortTransaction");

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to abort.
        // We also have no access to a connection over which to message failure-to-abort.
        return;
    }

    transaction->abort();
}

void IDBServer::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBServer::createObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createObjectStore(requestData, info);
}

void IDBServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBServer::deleteObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteObjectStore(requestData, objectStoreName);
}

void IDBServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBServer::clearObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBServer::createIndex");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createIndex(requestData, info);
}

void IDBServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBServer::deleteIndex");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const ThreadSafeDataBuffer& valueData, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBServer::putOrAdd");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->putOrAdd(requestData, keyData, valueData, overwriteMode);
}

void IDBServer::getRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::getRecord");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getRecord(requestData, keyRangeData);
}

void IDBServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::getCount");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getCount(requestData, keyRangeData);
}

void IDBServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::deleteRecord");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->deleteRecord(requestData, keyRangeData);
}

void IDBServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBServer::openCursor");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->openCursor(requestData, info);
}

void IDBServer::iterateCursor(const IDBRequestData& requestData, const IDBKeyData& key, unsigned long count)
{
    LOG(IndexedDB, "IDBServer::iterateCursor");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->iterateCursor(requestData, key, count);
}

void IDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBServer::establishTransaction");

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->establishTransaction(info);
}

void IDBServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::commitTransaction");

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to commit.
        // We also have no access to a connection over which to message failure-to-commit.
        return;
    }

    transaction->commit();
}

void IDBServer::didFinishHandlingVersionChangeTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFinishHandlingVersionChangeTransaction");

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction)
        return;

    transaction->didFinishHandlingVersionChange();
}

void IDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionClosed");

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionClosedFromClient();
}

void IDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFireVersionChangeEvent");

    if (auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier))
        databaseConnection->didFireVersionChangeEvent(requestIdentifier);
}

void IDBServer::postDatabaseTask(std::unique_ptr<CrossThreadTask>&& task)
{
    ASSERT(isMainThread());
    m_databaseQueue.append(WTFMove(task));
}

void IDBServer::postDatabaseTaskReply(std::unique_ptr<CrossThreadTask>&& task)
{
    ASSERT(!isMainThread());
    m_databaseReplyQueue.append(WTFMove(task));


    Locker<Lock> locker(m_mainThreadReplyLock);
    if (m_mainThreadReplyScheduled)
        return;

    m_mainThreadReplyScheduled = true;
    callOnMainThread([this] {
        handleTaskRepliesOnMainThread();
    });
}

void IDBServer::databaseThreadEntry(void* threadData)
{
    ASSERT(threadData);
    IDBServer* server = reinterpret_cast<IDBServer*>(threadData);
    server->databaseRunLoop();
}

void IDBServer::databaseRunLoop()
{
    ASSERT(!isMainThread());
    {
        Locker<Lock> locker(m_databaseThreadCreationLock);
    }

    while (auto task = m_databaseQueue.waitForMessage())
        task->performTask();
}

void IDBServer::handleTaskRepliesOnMainThread()
{
    {
        Locker<Lock> locker(m_mainThreadReplyLock);
        m_mainThreadReplyScheduled = false;
    }

    while (auto task = m_databaseReplyQueue.tryGetMessage())
        task->performTask();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
