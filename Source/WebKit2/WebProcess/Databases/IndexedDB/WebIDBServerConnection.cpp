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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "DatabaseProcessIDBConnectionMessages.h"
#include "DatabaseToWebProcessConnectionMessages.h"
#include "Logging.h"
#include "SecurityOriginData.h"
#include "WebProcess.h"
#include "WebToDatabaseProcessConnection.h"
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateServerConnectionIdentifier()
{
    ASSERT(isMainThread());
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

void WebIDBServerConnection::createIndex(IDBTransactionBackend&, const CreateIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::deleteIndex(IDBTransactionBackend&, const DeleteIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::get(IDBTransactionBackend&, const GetOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::put(IDBTransactionBackend&, const PutOperation&, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::openCursor(IDBTransactionBackend&, const OpenCursorOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::count(IDBTransactionBackend&, const CountOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::deleteRange(IDBTransactionBackend&, const DeleteRangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::clearObjectStore(IDBTransactionBackend&, const ClearObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
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

void WebIDBServerConnection::cursorAdvance(IDBCursorBackend&, const CursorAdvanceOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorIterate(IDBCursorBackend&, const CursorIterationOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorPrefetchIteration(IDBCursorBackend&, const CursorPrefetchIterationOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorPrefetchReset(IDBCursorBackend&, int usedPrefetches)
{
}

IPC::Connection* WebIDBServerConnection::messageSenderConnection()
{
    return WebProcess::shared().webToDatabaseProcessConnection()->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

