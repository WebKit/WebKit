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
#include "IDBConnectionToServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionProxy.h"
#include "IDBDatabase.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "TransactionOperation.h"
#include <wtf/MainThread.h>

namespace WebCore {
namespace IDBClient {

Ref<IDBConnectionToServer> IDBConnectionToServer::create(IDBConnectionToServerDelegate& delegate)
{
    return adoptRef(*new IDBConnectionToServer(delegate));
}

IDBConnectionToServer::IDBConnectionToServer(IDBConnectionToServerDelegate& delegate)
    : m_delegate(delegate)
    , m_proxy(std::make_unique<IDBConnectionProxy>(*this))
{
}

uint64_t IDBConnectionToServer::identifier() const
{
    return m_delegate->identifier();
}

IDBConnectionProxy& IDBConnectionToServer::proxy()
{
    ASSERT(m_proxy);
    return *m_proxy;
}

void IDBConnectionToServer::deleteDatabase(const IDBRequestData& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteDatabase - %s", request.databaseIdentifier().debugString().utf8().data());
    m_delegate->deleteDatabase(request);
}

void IDBConnectionToServer::didDeleteDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteDatabase");
    m_proxy->didDeleteDatabase(resultData);
}

void IDBConnectionToServer::openDatabase(const IDBRequestData& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::openDatabase - %s (%" PRIu64 ")", request.databaseIdentifier().debugString().utf8().data(), request.requestedVersion());
    m_delegate->openDatabase(request);
}

void IDBConnectionToServer::didOpenDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenDatabase");
    m_proxy->didOpenDatabase(resultData);
}

void IDBConnectionToServer::createObjectStore(TransactionOperation& operation, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::createObjectStore");

    saveOperation(operation);

    m_delegate->createObjectStore(IDBRequestData(operation), info);
}

void IDBConnectionToServer::didCreateObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCreateObjectStore");
    completeOperation(resultData);
}

void IDBConnectionToServer::deleteObjectStore(TransactionOperation& operation, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteObjectStore");

    saveOperation(operation);

    m_delegate->deleteObjectStore(IDBRequestData(operation), objectStoreName);
}

void IDBConnectionToServer::didDeleteObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteObjectStore");
    completeOperation(resultData);
}

void IDBConnectionToServer::clearObjectStore(TransactionOperation& operation, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::clearObjectStore");

    saveOperation(operation);

    m_delegate->clearObjectStore(IDBRequestData(operation), objectStoreIdentifier);
}

void IDBConnectionToServer::didClearObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didClearObjectStore");
    completeOperation(resultData);
}

void IDBConnectionToServer::createIndex(TransactionOperation& operation, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::createIndex");

    saveOperation(operation);

    m_delegate->createIndex(IDBRequestData(operation), info);
}

void IDBConnectionToServer::didCreateIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCreateIndex");
    completeOperation(resultData);
}

void IDBConnectionToServer::deleteIndex(TransactionOperation& operation, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteIndex");

    saveOperation(operation);

    m_delegate->deleteIndex(IDBRequestData(operation), objectStoreIdentifier, indexName);
}

void IDBConnectionToServer::didDeleteIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteIndex");
    completeOperation(resultData);
}

void IDBConnectionToServer::putOrAdd(TransactionOperation& operation, IDBKey* key, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBConnectionToServer::putOrAdd");

    saveOperation(operation);
    m_delegate->putOrAdd(IDBRequestData(operation), key, value, overwriteMode);
}

void IDBConnectionToServer::didPutOrAdd(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didPutOrAdd");
    completeOperation(resultData);
}

void IDBConnectionToServer::getRecord(TransactionOperation& operation, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBConnectionToServer::getRecord");

    ASSERT(!keyRangeData.isNull);

    saveOperation(operation);
    m_delegate->getRecord(IDBRequestData(operation), keyRangeData);
}

void IDBConnectionToServer::didGetRecord(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetRecord");
    completeOperation(resultData);
}

void IDBConnectionToServer::getCount(TransactionOperation& operation, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBConnectionToServer::getCount");

    ASSERT(!keyRangeData.isNull);

    saveOperation(operation);
    m_delegate->getCount(IDBRequestData(operation), keyRangeData);
}

void IDBConnectionToServer::didGetCount(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetCount");
    completeOperation(resultData);
}

void IDBConnectionToServer::deleteRecord(TransactionOperation& operation, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteRecord");

    ASSERT(!keyRangeData.isNull);

    saveOperation(operation);
    m_delegate->deleteRecord(IDBRequestData(operation), keyRangeData);
}

void IDBConnectionToServer::didDeleteRecord(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteRecord");
    completeOperation(resultData);
}

void IDBConnectionToServer::openCursor(TransactionOperation& operation, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::openCursor");

    saveOperation(operation);
    m_delegate->openCursor(IDBRequestData(operation), info);
}

void IDBConnectionToServer::didOpenCursor(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenCursor");
    completeOperation(resultData);
}

void IDBConnectionToServer::iterateCursor(TransactionOperation& operation, const IDBKeyData& key, unsigned long count)
{
    LOG(IndexedDB, "IDBConnectionToServer::iterateCursor");

    saveOperation(operation);
    m_delegate->iterateCursor(IDBRequestData(operation), key, count);
}

void IDBConnectionToServer::didIterateCursor(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didIterateCursor");
    completeOperation(resultData);
}

void IDBConnectionToServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::establishTransaction");
    ASSERT(isMainThread());

    m_delegate->establishTransaction(databaseConnectionIdentifier, info);
}

void IDBConnectionToServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::commitTransaction");
    ASSERT(isMainThread());

    m_delegate->commitTransaction(transactionIdentifier);
}

void IDBConnectionToServer::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCommitTransaction");
    ASSERT(isMainThread());

    m_proxy->didCommitTransaction(transactionIdentifier, error);
}

void IDBConnectionToServer::didFinishHandlingVersionChangeTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::didFinishHandlingVersionChangeTransaction");
    ASSERT(isMainThread());

    m_delegate->didFinishHandlingVersionChangeTransaction(transactionIdentifier);
}

void IDBConnectionToServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::abortTransaction");
    ASSERT(isMainThread());

    m_delegate->abortTransaction(transactionIdentifier);
}

void IDBConnectionToServer::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didAbortTransaction");
    ASSERT(isMainThread());

    m_proxy->didAbortTransaction(transactionIdentifier, error);
}

void IDBConnectionToServer::fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    LOG(IndexedDB, "IDBConnectionToServer::fireVersionChangeEvent");
    ASSERT(isMainThread());

    m_proxy->fireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, requestedVersion);
}

void IDBConnectionToServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::didFireVersionChangeEvent");
    ASSERT(isMainThread());

    m_delegate->didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier);
}

void IDBConnectionToServer::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didStartTransaction");
    ASSERT(isMainThread());

    m_proxy->didStartTransaction(transactionIdentifier, error);
}

void IDBConnectionToServer::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    LOG(IndexedDB, "IDBConnectionToServer::didStartTransaction");
    ASSERT(isMainThread());

    m_proxy->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
}

void IDBConnectionToServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::databaseConnectionClosed");
    ASSERT(isMainThread());

    m_delegate->databaseConnectionClosed(databaseConnectionIdentifier);
}

void IDBConnectionToServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::abortOpenAndUpgradeNeeded");

    m_delegate->abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void IDBConnectionToServer::saveOperation(TransactionOperation& operation)
{
    ASSERT(!m_activeOperations.contains(operation.identifier()));
    m_activeOperations.set(operation.identifier(), &operation);
}

void IDBConnectionToServer::completeOperation(const IDBResultData& resultData)
{
    auto operation = m_activeOperations.take(resultData.requestIdentifier());
    ASSERT(operation);

    operation->completed(resultData);
}

void IDBConnectionToServer::getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, std::function<void (const Vector<String>&)> callback)
{
    static uint64_t callbackID = 0;

    m_getAllDatabaseNamesCallbacks.add(++callbackID, WTFMove(callback));

    m_delegate->getAllDatabaseNames(SecurityOriginData::fromSecurityOrigin(mainFrameOrigin), SecurityOriginData::fromSecurityOrigin(openingOrigin), callbackID);
}

void IDBConnectionToServer::didGetAllDatabaseNames(uint64_t callbackID, const Vector<String>& databaseNames)
{
    auto callback = m_getAllDatabaseNamesCallbacks.take(callbackID);
    ASSERT(callback);

    callback(databaseNames);
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
