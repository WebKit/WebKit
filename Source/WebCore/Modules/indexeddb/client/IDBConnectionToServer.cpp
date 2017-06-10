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
#include "IDBGetRecordData.h"
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
    LOG(IndexedDB, "IDBConnectionToServer::openDatabase - %s (%s) (%" PRIu64 ")", request.databaseIdentifier().debugString().utf8().data(), request.requestIdentifier().loggingString().utf8().data(), request.requestedVersion());
    m_delegate->openDatabase(request);
}

void IDBConnectionToServer::didOpenDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenDatabase");
    m_proxy->didOpenDatabase(resultData);
}

void IDBConnectionToServer::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::createObjectStore");
    ASSERT(isMainThread());

    m_delegate->createObjectStore(requestData, info);
}

void IDBConnectionToServer::didCreateObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCreateObjectStore");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteObjectStore");
    ASSERT(isMainThread());

    m_delegate->deleteObjectStore(requestData, objectStoreName);
}

void IDBConnectionToServer::didDeleteObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteObjectStore");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::renameObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBConnectionToServer::renameObjectStore");
    ASSERT(isMainThread());

    m_delegate->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void IDBConnectionToServer::didRenameObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didRenameObjectStore");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::clearObjectStore");
    ASSERT(isMainThread());

    m_delegate->clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBConnectionToServer::didClearObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didClearObjectStore");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::createIndex");
    ASSERT(isMainThread());

    m_delegate->createIndex(requestData, info);
}

void IDBConnectionToServer::didCreateIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCreateIndex");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteIndex");
    ASSERT(isMainThread());

    m_delegate->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBConnectionToServer::didDeleteIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteIndex");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::renameIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBConnectionToServer::renameIndex");
    ASSERT(isMainThread());

    m_delegate->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBConnectionToServer::didRenameIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didRenameIndex");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& key, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBConnectionToServer::putOrAdd");
    ASSERT(isMainThread());

    m_delegate->putOrAdd(requestData, key, value, overwriteMode);
}

void IDBConnectionToServer::didPutOrAdd(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didPutOrAdd");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBConnectionToServer::getRecord");
    ASSERT(isMainThread());
    ASSERT(!getRecordData.keyRangeData.isNull);

    m_delegate->getRecord(requestData, getRecordData);
}

void IDBConnectionToServer::didGetRecord(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetRecord");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBConnectionToServer::getAllRecords");
    ASSERT(isMainThread());

    m_delegate->getAllRecords(requestData, getAllRecordsData);
}

void IDBConnectionToServer::didGetAllRecords(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetAllRecords");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBConnectionToServer::getCount");
    ASSERT(isMainThread());
    ASSERT(!keyRangeData.isNull);

    m_delegate->getCount(requestData, keyRangeData);
}

void IDBConnectionToServer::didGetCount(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetCount");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteRecord");
    ASSERT(isMainThread());
    ASSERT(!keyRangeData.isNull);

    m_delegate->deleteRecord(requestData, keyRangeData);
}

void IDBConnectionToServer::didDeleteRecord(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteRecord");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBConnectionToServer::openCursor");
    ASSERT(isMainThread());

    m_delegate->openCursor(requestData, info);
}

void IDBConnectionToServer::didOpenCursor(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenCursor");
    m_proxy->completeOperation(resultData);
}

void IDBConnectionToServer::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBConnectionToServer::iterateCursor");
    ASSERT(isMainThread());

    m_delegate->iterateCursor(requestData, data);
}

void IDBConnectionToServer::didIterateCursor(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didIterateCursor");
    m_proxy->completeOperation(resultData);
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

void IDBConnectionToServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::didFinishHandlingVersionChangeTransaction");
    ASSERT(isMainThread());

    m_delegate->didFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
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

void IDBConnectionToServer::didCloseFromServer(uint64_t databaseConnectionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCloseFromServer");
    ASSERT(isMainThread());

    m_proxy->didCloseFromServer(databaseConnectionIdentifier, error);
}

void IDBConnectionToServer::confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::confirmDidCloseFromServer");
    ASSERT(isMainThread());

    m_delegate->confirmDidCloseFromServer(databaseConnectionIdentifier);
}

void IDBConnectionToServer::connectionToServerLost(const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::connectionToServerLost");
    ASSERT(isMainThread());

    m_proxy->connectionToServerLost(error);
}

void IDBConnectionToServer::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    LOG(IndexedDB, "IDBConnectionToServer::didStartTransaction");
    ASSERT(isMainThread());

    m_proxy->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
}

void IDBConnectionToServer::openDBRequestCancelled(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBConnectionToServer::openDBRequestCancelled");
    ASSERT(isMainThread());

    m_delegate->openDBRequestCancelled(requestData);
}

void IDBConnectionToServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::databaseConnectionPendingClose");
    ASSERT(isMainThread());

    m_delegate->databaseConnectionPendingClose(databaseConnectionIdentifier);
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
    ASSERT(isMainThread());

    m_delegate->abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void IDBConnectionToServer::getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, Function<void (const Vector<String>&)>&& callback)
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
