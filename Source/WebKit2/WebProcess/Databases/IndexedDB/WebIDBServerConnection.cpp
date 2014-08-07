/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "WebIDBServerConnection.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "AsyncRequest.h"
#include "DataReference.h"
#include "DatabaseProcessIDBConnectionMessages.h"
#include "DatabaseToWebProcessConnectionMessages.h"
#include "Logging.h"
#include "SecurityOriginData.h"
#include "WebProcess.h"
#include "WebToDatabaseProcessConnection.h"
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateServerConnectionIdentifier()
{
    ASSERT(RunLoop::isMain());
    static uint64_t identifier = 0;
    return ++identifier;
}

PassRefPtr<WebIDBServerConnection> WebIDBServerConnection::create(const String& databaseName, const SecurityOrigin& openingOrigin, const SecurityOrigin& mainFrameOrigin)
{
    RefPtr<WebIDBServerConnection> result = adoptRef(new WebIDBServerConnection(databaseName, openingOrigin, mainFrameOrigin));
    WebProcess::shared().webToDatabaseProcessConnection()->registerWebIDBServerConnection(*result);
    return result.release();
}

WebIDBServerConnection::WebIDBServerConnection(const String& databaseName, const SecurityOrigin& openingOrigin, const SecurityOrigin& mainFrameOrigin)
    : m_serverConnectionIdentifier(generateServerConnectionIdentifier())
    , m_databaseName(databaseName)
    , m_openingOrigin(*openingOrigin.isolatedCopy())
    , m_mainFrameOrigin(*mainFrameOrigin.isolatedCopy())
{
    send(Messages::DatabaseToWebProcessConnection::EstablishIDBConnection(m_serverConnectionIdentifier));
    send(Messages::DatabaseProcessIDBConnection::EstablishConnection(databaseName, SecurityOriginData::fromSecurityOrigin(&openingOrigin), SecurityOriginData::fromSecurityOrigin(&mainFrameOrigin)));
}

WebIDBServerConnection::~WebIDBServerConnection()
{
    WebProcess::shared().webToDatabaseProcessConnection()->removeWebIDBServerConnection(*this);

    for (const auto& it : m_serverRequests)
        it.value->requestAborted();
}

bool WebIDBServerConnection::isClosed()
{
    // FIXME: Return real value here.
    return true;
}

void WebIDBServerConnection::deleteDatabase(const String& name, BoolCallbackFunction successCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<bool>::create(successCallback);

    serverRequest->setAbortHandler([successCallback]() {
        successCallback(false);
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess deleteDatabase request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::DeleteDatabase(requestID, name));
}

void WebIDBServerConnection::didDeleteDatabase(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didDeleteDatabase request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success);
}

void WebIDBServerConnection::getOrEstablishIDBDatabaseMetadata(GetIDBDatabaseMetadataFunction completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<const IDBDatabaseMetadata&, bool>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        IDBDatabaseMetadata metadata;
        completionCallback(metadata, false);
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess getOrEstablishIDBDatabaseMetadata request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::GetOrEstablishIDBDatabaseMetadata(requestID));
}

void WebIDBServerConnection::didGetOrEstablishIDBDatabaseMetadata(uint64_t requestID, bool success, const IDBDatabaseMetadata& metadata)
{
    LOG(IDB, "WebProcess didGetOrEstablishIDBDatabaseMetadata request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(metadata, success);
}

void WebIDBServerConnection::close()
{
    LOG(IDB, "WebProcess close");

    RefPtr<WebIDBServerConnection> protector(this);

    for (auto& request : m_serverRequests)
        request.value->requestAborted();

    m_serverRequests.clear();

    send(Messages::DatabaseProcessIDBConnection::Close());
}

void WebIDBServerConnection::openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode, BoolCallbackFunction successCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<bool>::create(successCallback);

    serverRequest->setAbortHandler([successCallback]() {
        successCallback(false);
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess openTransaction ID %lli (request ID %llu)", transactionID, requestID);

    Vector<int64_t> objectStoreIDVector;
    copyToVector(objectStoreIDs, objectStoreIDVector);
    send(Messages::DatabaseProcessIDBConnection::OpenTransaction(requestID, transactionID, objectStoreIDVector, static_cast<uint64_t>(mode)));
}

void WebIDBServerConnection::didOpenTransaction(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didOpenTransaction request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success);
}

void WebIDBServerConnection::beginTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<>::create(completionCallback, completionCallback);

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess beginTransaction ID %lli (request ID %llu)", transactionID, requestID);

    send(Messages::DatabaseProcessIDBConnection::BeginTransaction(requestID, transactionID));
}

void WebIDBServerConnection::didBeginTransaction(uint64_t requestID, bool)
{
    LOG(IDB, "WebProcess didBeginTransaction request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest();
}

void WebIDBServerConnection::commitTransaction(int64_t transactionID, BoolCallbackFunction successCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<bool>::create(successCallback);

    serverRequest->setAbortHandler([successCallback]() {
        successCallback(false);
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess commitTransaction ID %lli (request ID %llu)", transactionID, requestID);

    send(Messages::DatabaseProcessIDBConnection::CommitTransaction(requestID, transactionID));
}

void WebIDBServerConnection::didCommitTransaction(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didCommitTransaction request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success);
}

void WebIDBServerConnection::resetTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<>::create(completionCallback, completionCallback);

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess resetTransaction ID %lli (request ID %llu)", transactionID, requestID);

    send(Messages::DatabaseProcessIDBConnection::ResetTransaction(requestID, transactionID));
}

void WebIDBServerConnection::didResetTransaction(uint64_t requestID, bool)
{
    LOG(IDB, "WebProcess didResetTransaction request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest();
}

bool WebIDBServerConnection::resetTransactionSync(int64_t transactionID)
{
    bool success;
    sendSync(Messages::DatabaseProcessIDBConnection::ResetTransactionSync(transactionID), Messages::DatabaseProcessIDBConnection::ResetTransactionSync::Reply(success));
    return success;
}

void WebIDBServerConnection::rollbackTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<>::create(completionCallback, completionCallback);

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess rollbackTransaction ID %lli (request ID %llu)", transactionID, requestID);

    send(Messages::DatabaseProcessIDBConnection::RollbackTransaction(requestID, transactionID));
}

void WebIDBServerConnection::didRollbackTransaction(uint64_t requestID, bool)
{
    LOG(IDB, "WebProcess didRollbackTransaction request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest();
}

bool WebIDBServerConnection::rollbackTransactionSync(int64_t transactionID)
{
    bool success;
    sendSync(Messages::DatabaseProcessIDBConnection::RollbackTransactionSync(transactionID), Messages::DatabaseProcessIDBConnection::RollbackTransactionSync::Reply(success));
    return success;
}

void WebIDBServerConnection::setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const IDBObjectStoreMetadata&, IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::createObjectStore(IDBTransactionBackend& transaction, const CreateObjectStoreOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    String objectStoreName = operation.objectStoreMetadata().name;
    serverRequest->setAbortHandler([objectStoreName, completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Unknown error occured creating object store '%s'", objectStoreName.utf8().data())));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess createObjectStore '%s' in transaction ID %llu (request ID %llu)", operation.objectStoreMetadata().name.utf8().data(), transaction.id(), requestID);

    send(Messages::DatabaseProcessIDBConnection::CreateObjectStore(requestID, transaction.id(), operation.objectStoreMetadata()));
}

void WebIDBServerConnection::didCreateObjectStore(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didCreateObjectStore request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured creating object store"));
}

void WebIDBServerConnection::createIndex(IDBTransactionBackend&transaction, const CreateIndexOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured creating index"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess create index request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::CreateIndex(requestID, transaction.id(), operation.objectStoreID(), operation.idbIndexMetadata()));
}

void WebIDBServerConnection::didCreateIndex(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didCreateIndex request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured creating index"));
}

void WebIDBServerConnection::deleteIndex(IDBTransactionBackend&transaction, const DeleteIndexOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured deleting index"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess delete index request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::DeleteIndex(requestID, transaction.id(), operation.objectStoreID(), operation.idbIndexMetadata().id));
}

void WebIDBServerConnection::didDeleteIndex(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didDeleteIndex request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured deleting index"));
}

void WebIDBServerConnection::get(IDBTransactionBackend& transaction, const GetOperation& operation, std::function<void(const IDBGetResult&, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<const IDBGetResult&, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBGetResult(), IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured getting record"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess get request ID %llu", requestID);

    ASSERT(operation.keyRange());

    send(Messages::DatabaseProcessIDBConnection::GetRecord(requestID, transaction.id(), operation.objectStoreID(), operation.indexID(), operation.keyRange(), static_cast<int64_t>(operation.cursorType())));
}

void WebIDBServerConnection::put(IDBTransactionBackend& transaction, const PutOperation& operation, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBKey>, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(nullptr, IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured putting record"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess put request ID %llu", requestID);

    ASSERT(operation.value());

    IPC::DataReference value(reinterpret_cast<const uint8_t*>(operation.value()->data()), operation.value()->size());

    Vector<Vector<IDBKeyData>> indexKeys;
    for (const auto& keys : operation.indexKeys()) {
        indexKeys.append(Vector<IDBKeyData>());
        for (const auto& key : keys)
            indexKeys.last().append(IDBKeyData(key.get()));
    }

    send(Messages::DatabaseProcessIDBConnection::PutRecord(requestID, transaction.id(), operation.objectStore().id, IDBKeyData(operation.key()), value, operation.putMode(), operation.indexIDs(), indexKeys));
}

void WebIDBServerConnection::didPutRecord(uint64_t requestID, const WebCore::IDBKeyData& resultKey, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didPutRecord request ID %llu (error - %s)", requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(resultKey.isNull ? nullptr : resultKey.maybeCreateIDBKey(), errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::didGetRecord(uint64_t requestID, const WebCore::IDBGetResult& getResult, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didGetRecord request ID %llu (error - %s)", requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(getResult, errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::didOpenCursor(uint64_t requestID, int64_t cursorID, const IDBKeyData& key, const IDBKeyData& primaryKey, const IPC::DataReference& valueData, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didOpenCursor request ID %llu (error - %s)", requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    RefPtr<SharedBuffer> value = SharedBuffer::create(valueData.data(), valueData.size());
    serverRequest->completeRequest(cursorID, key.maybeCreateIDBKey(), primaryKey.maybeCreateIDBKey(), value.release(), errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::didAdvanceCursor(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const IPC::DataReference& valueData, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didAdvanceCursor request ID %llu (error - %s)", requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    RefPtr<SharedBuffer> value = SharedBuffer::create(valueData.data(), valueData.size());
    serverRequest->completeRequest(key.maybeCreateIDBKey(), primaryKey.maybeCreateIDBKey(), value.release(), errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::didIterateCursor(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const IPC::DataReference& valueData, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didIterateCursor request ID %llu (error - %s)", requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    RefPtr<SharedBuffer> value = SharedBuffer::create(valueData.data(), valueData.size());
    serverRequest->completeRequest(key.maybeCreateIDBKey(), primaryKey.maybeCreateIDBKey(), value.release(), errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::count(IDBTransactionBackend& transaction, const CountOperation& operation, std::function<void(int64_t, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<int64_t, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(0, IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured getting count"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess count request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::Count(requestID, transaction.id(), operation.objectStoreID(), operation.indexID(), IDBKeyRangeData(operation.keyRange())));
}

void WebIDBServerConnection::didCount(uint64_t requestID, int64_t count, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didCount %lli request ID %llu (error - %s)", count, requestID, errorMessage.utf8().data());

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(count, errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::deleteRange(IDBTransactionBackend& transaction, const DeleteRangeOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured getting count"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess deleteRange request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::DeleteRange(requestID, transaction.id(), operation.objectStoreID(), IDBKeyRangeData(operation.keyRange())));
}

void WebIDBServerConnection::didDeleteRange(uint64_t requestID, uint32_t errorCode, const String& errorMessage)
{
    LOG(IDB, "WebProcess didDeleteRange request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(errorCode ? IDBDatabaseError::create(errorCode, errorMessage) : nullptr);
}

void WebIDBServerConnection::clearObjectStore(IDBTransactionBackend&, const ClearObjectStoreOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured clearing object store"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess clearObjectStore request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::ClearObjectStore(requestID, operation.transaction()->id(), operation.objectStoreID()));
}

void WebIDBServerConnection::didClearObjectStore(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didClearObjectStore request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured clearing object store"));
}

void WebIDBServerConnection::deleteObjectStore(IDBTransactionBackend&, const DeleteObjectStoreOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured deleting object store"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess deleteObjectStore request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::DeleteObjectStore(requestID, operation.transaction()->id(), operation.objectStoreMetadata().id));
}

void WebIDBServerConnection::didDeleteObjectStore(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didDeleteObjectStore request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured deleting object store"));
}

void WebIDBServerConnection::changeDatabaseVersion(IDBTransactionBackend&, const IDBDatabaseBackend::VersionChangeOperation& operation, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured changing database version"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess changeDatabaseVersion request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::ChangeDatabaseVersion(requestID, operation.transaction()->id(), static_cast<uint64_t>(operation.version())));
}

void WebIDBServerConnection::didChangeDatabaseVersion(uint64_t requestID, bool success)
{
    LOG(IDB, "WebProcess didChangeDatabaseVersion request ID %llu", requestID);

    RefPtr<AsyncRequest> serverRequest = m_serverRequests.take(requestID);

    if (!serverRequest)
        return;

    serverRequest->completeRequest(success ? nullptr : IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured changing database version"));
}

void WebIDBServerConnection::openCursor(IDBTransactionBackend&, const OpenCursorOperation& operation, std::function<void(int64_t, PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<int64_t, PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(0, nullptr, nullptr, nullptr, IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured opening database cursor"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess openCursor request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::OpenCursor(requestID, operation.transactionID(), operation.objectStoreID(), operation.indexID(), static_cast<int64_t>(operation.direction()), static_cast<int64_t>(operation.cursorType()), static_cast<int64_t>(operation.taskType()), operation.keyRange()));
}

void WebIDBServerConnection::cursorAdvance(IDBCursorBackend&, const CursorAdvanceOperation& operation, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(nullptr, nullptr, nullptr, IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured advancing database cursor"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess cursorAdvance request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::CursorAdvance(requestID, operation.cursorID(), operation.count()));
}

void WebIDBServerConnection::cursorIterate(IDBCursorBackend&, const CursorIterationOperation& operation, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
    RefPtr<AsyncRequest> serverRequest = AsyncRequestImpl<PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>>::create(completionCallback);

    serverRequest->setAbortHandler([completionCallback]() {
        completionCallback(nullptr, nullptr, nullptr, IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Unknown error occured iterating database cursor"));
    });

    uint64_t requestID = serverRequest->requestID();
    ASSERT(!m_serverRequests.contains(requestID));
    m_serverRequests.add(requestID, serverRequest.release());

    LOG(IDB, "WebProcess cursorIterate request ID %llu", requestID);

    send(Messages::DatabaseProcessIDBConnection::CursorIterate(requestID, operation.cursorID(), operation.key()));
}

IPC::Connection* WebIDBServerConnection::messageSenderConnection()
{
    return WebProcess::shared().webToDatabaseProcessConnection()->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

