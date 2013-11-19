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

#include "DatabaseProcessIDBConnectionMessages.h"
#include "DatabaseToWebProcessConnectionMessages.h"
#include "WebProcess.h"
#include "WebToDatabaseProcessConnection.h"

namespace WebKit {

static uint64_t generateBackendIdentifier()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(uint64_t, identifier, (0));
    return ++identifier;
}

WebIDBServerConnection::WebIDBServerConnection(const String& databaseName, const WebCore::SecurityOrigin& openingOrigin, const WebCore::SecurityOrigin& mainFrameOrigin)
    : m_backendIdentifier(generateBackendIdentifier())
{
    send(Messages::DatabaseToWebProcessConnection::EstablishIDBConnection(m_backendIdentifier));
    send(Messages::DatabaseProcessIDBConnection::EstablishConnection());
}

WebIDBServerConnection::~WebIDBServerConnection()
{
}

bool WebIDBServerConnection::isClosed()
{
    return true;
}

void WebIDBServerConnection::getOrEstablishIDBDatabaseMetadata(const String& name, GetIDBDatabaseMetadataFunction)
{
}

void WebIDBServerConnection::deleteDatabase(const String& name, BoolCallbackFunction successCallback)
{
}

void WebIDBServerConnection::close()
{
}

void WebIDBServerConnection::openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIds, WebCore::IndexedDB::TransactionMode, BoolCallbackFunction successCallback)
{
}

void WebIDBServerConnection::beginTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::commitTransaction(int64_t transactionID, BoolCallbackFunction successCallback)
{
}

void WebIDBServerConnection::resetTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::rollbackTransaction(int64_t transactionID, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const WebCore::IDBObjectStoreMetadata&, WebCore::IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<WebCore::IDBKey>>>& indexKeys, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::createObjectStore(WebCore::IDBTransactionBackend&, const WebCore::CreateObjectStoreOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::createIndex(WebCore::IDBTransactionBackend&, const WebCore::CreateIndexOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::deleteIndex(WebCore::IDBTransactionBackend&, const WebCore::DeleteIndexOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::get(WebCore::IDBTransactionBackend&, const WebCore::GetOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::put(WebCore::IDBTransactionBackend&, const WebCore::PutOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::openCursor(WebCore::IDBTransactionBackend&, const WebCore::OpenCursorOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::count(WebCore::IDBTransactionBackend&, const WebCore::CountOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::deleteRange(WebCore::IDBTransactionBackend&, const WebCore::DeleteRangeOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::clearObjectStore(WebCore::IDBTransactionBackend&, const WebCore::ClearObjectStoreOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::deleteObjectStore(WebCore::IDBTransactionBackend&, const WebCore::DeleteObjectStoreOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{

}
void WebIDBServerConnection::changeDatabaseVersion(WebCore::IDBTransactionBackend&, const WebCore::IDBDatabaseBackend::VersionChangeOperation&, std::function<void(PassRefPtr<WebCore::IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::cursorAdvance(WebCore::IDBCursorBackend&, const WebCore::CursorAdvanceOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorIterate(WebCore::IDBCursorBackend&, const WebCore::CursorIterationOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorPrefetchIteration(WebCore::IDBCursorBackend&, const WebCore::CursorPrefetchIterationOperation&, std::function<void()> completionCallback)
{
}

void WebIDBServerConnection::cursorPrefetchReset(WebCore::IDBCursorBackend&, int usedPrefetches)
{
}

CoreIPC::Connection* WebIDBServerConnection::messageSenderConnection()
{
    return WebProcess::shared().webToDatabaseProcessConnection()->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

