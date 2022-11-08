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

#include "IDBConnectionProxy.h"
#include "IDBDatabase.h"
#include "IDBDatabaseNameAndVersion.h"
#include "IDBGetRecordData.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "SecurityOrigin.h"
#include "TransactionOperation.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {
namespace IDBClient {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBConnectionToServer);

Ref<IDBConnectionToServer> IDBConnectionToServer::create(IDBConnectionToServerDelegate& delegate)
{
    return adoptRef(*new IDBConnectionToServer(delegate));
}

IDBConnectionToServer::IDBConnectionToServer(IDBConnectionToServerDelegate& delegate)
    : m_delegate(delegate)
    , m_proxy(makeUnique<IDBConnectionProxy>(*this))
{
}

IDBConnectionIdentifier IDBConnectionToServer::identifier() const
{
    return m_delegate->identifier();
}

IDBConnectionProxy& IDBConnectionToServer::proxy()
{
    ASSERT(m_proxy);
    return *m_proxy;
}

void IDBConnectionToServer::callResultFunctionWithErrorLater(ResultFunction function, const IDBResourceIdentifier& requestIdentifier)
{
    callOnMainThread([this, protectedThis = Ref { *this }, function, requestIdentifier]() {
        (this->*function)(IDBResultData::error(requestIdentifier, IDBError::serverConnectionLostError()));
    });
}

void IDBConnectionToServer::deleteDatabase(const IDBRequestData& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteDatabase - %s", request.databaseIdentifier().loggingString().utf8().data());
    
    if (m_serverConnectionIsValid)
        m_delegate->deleteDatabase(request);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didDeleteDatabase, request.requestIdentifier());
}

void IDBConnectionToServer::didDeleteDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteDatabase");
    m_proxy->didDeleteDatabase(resultData);
}

void IDBConnectionToServer::openDatabase(const IDBRequestData& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::openDatabase - %s (%s) (%" PRIu64 ")", request.databaseIdentifier().loggingString().utf8().data(), request.requestIdentifier().loggingString().utf8().data(), request.requestedVersion());

    if (m_serverConnectionIsValid)
        m_delegate->openDatabase(request);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didOpenDatabase, request.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->createObjectStore(requestData, info);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didCreateObjectStore, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->deleteObjectStore(requestData, objectStoreName);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didDeleteObjectStore, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->renameObjectStore(requestData, objectStoreIdentifier, newName);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didRenameObjectStore, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->clearObjectStore(requestData, objectStoreIdentifier);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didClearObjectStore, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->createIndex(requestData, info);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didCreateIndex, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->deleteIndex(requestData, objectStoreIdentifier, indexName);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didDeleteIndex, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didRenameIndex, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->putOrAdd(requestData, key, value, overwriteMode);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didPutOrAdd, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->getRecord(requestData, getRecordData);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didGetRecord, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->getAllRecords(requestData, getAllRecordsData);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didGetAllRecords, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->getCount(requestData, keyRangeData);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didGetCount, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->deleteRecord(requestData, keyRangeData);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didDeleteRecord, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->openCursor(requestData, info);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didOpenCursor, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->iterateCursor(requestData, data);
    else
        callResultFunctionWithErrorLater(&IDBConnectionToServer::didIterateCursor, requestData.requestIdentifier());
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

    if (m_serverConnectionIsValid)
        m_delegate->establishTransaction(databaseConnectionIdentifier, info);
}

void IDBConnectionToServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    LOG(IndexedDB, "IDBConnectionToServer::commitTransaction");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->commitTransaction(transactionIdentifier, pendingRequestCount);
    else {
        callOnMainThread([this, protectedThis = Ref { *this }, transactionIdentifier] {
            didCommitTransaction(transactionIdentifier, IDBError::serverConnectionLostError());
        });
    }
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

    if (m_serverConnectionIsValid)
        m_delegate->didFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
}

void IDBConnectionToServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::abortTransaction");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->abortTransaction(transactionIdentifier);
    else {
        callOnMainThread([this, protectedThis = Ref { *this }, transactionIdentifier] {
            didAbortTransaction(transactionIdentifier, IDBError::serverConnectionLostError());
        });
    }
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

void IDBConnectionToServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, const IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    LOG(IndexedDB, "IDBConnectionToServer::didFireVersionChangeEvent");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, connectionClosed);
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

void IDBConnectionToServer::connectionToServerLost(const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::connectionToServerLost");
    ASSERT(isMainThread());
    ASSERT(m_serverConnectionIsValid);
    
    m_serverConnectionIsValid = false;
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

    if (m_serverConnectionIsValid)
        m_delegate->openDBRequestCancelled(requestData);
}

void IDBConnectionToServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::databaseConnectionPendingClose");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->databaseConnectionPendingClose(databaseConnectionIdentifier);
}

void IDBConnectionToServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::databaseConnectionClosed");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->databaseConnectionClosed(databaseConnectionIdentifier);
}

void IDBConnectionToServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>& transactionIdentifier)
{
    LOG(IndexedDB, "IDBConnectionToServer::abortOpenAndUpgradeNeeded");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid)
        m_delegate->abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void IDBConnectionToServer::getAllDatabaseNamesAndVersions(const IDBResourceIdentifier& requestIdentifier, const ClientOrigin& origin)
{
    LOG(IndexedDB, "IDBConnectionToServer::getAllDatabaseNamesAndVersions");
    ASSERT(isMainThread());

    if (m_serverConnectionIsValid) {
        m_delegate->getAllDatabaseNamesAndVersions(requestIdentifier, origin);
        return;
    }

    callOnMainThread([this, protectedThis = Ref { *this }, requestIdentifier] {
        didGetAllDatabaseNamesAndVersions(requestIdentifier, { });
    });
}

void IDBConnectionToServer::didGetAllDatabaseNamesAndVersions(const IDBResourceIdentifier& requestIdentifier, Vector<IDBDatabaseNameAndVersion>&& databases)
{
    LOG(IndexedDB, "IDBConnectionToServer::didGetAllDatabaseNamesAndVersions");
    ASSERT(isMainThread());

    m_proxy->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(databases));
}

} // namespace IDBClient
} // namespace WebCore
