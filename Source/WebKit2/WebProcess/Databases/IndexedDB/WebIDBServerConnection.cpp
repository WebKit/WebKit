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
    DEFINE_STATIC_LOCAL(uint64_t, identifier, (0));
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
}

bool WebIDBServerConnection::isClosed()
{
    return true;
}

void WebIDBServerConnection::deleteDatabase(const String& name, BoolCallbackFunction successCallback)
{
}

void WebIDBServerConnection::getOrEstablishIDBDatabaseMetadata(GetIDBDatabaseMetadataFunction completionCallback)
{
    // FIXME: Save the completionCallback to perform this request is complete.
    send(Messages::DatabaseProcessIDBConnection::GetOrEstablishIDBDatabaseMetadata());
}

void WebIDBServerConnection::didGetOrEstablishIDBDatabaseMetadata(bool, const IDBDatabaseMetadata&)
{
    // FIXME: Lookup the appropriate completionCallback to perform with these results.
}

void WebIDBServerConnection::close()
{
}

void WebIDBServerConnection::openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode, BoolCallbackFunction successCallback)
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

void WebIDBServerConnection::setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const IDBObjectStoreMetadata&, IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
}

void WebIDBServerConnection::createObjectStore(IDBTransactionBackend&, const CreateObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
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

void WebIDBServerConnection::put(IDBTransactionBackend&, const PutOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
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

void WebIDBServerConnection::deleteObjectStore(IDBTransactionBackend&, const DeleteObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{

}
void WebIDBServerConnection::changeDatabaseVersion(IDBTransactionBackend&, const IDBDatabaseBackend::VersionChangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback)
{
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

CoreIPC::Connection* WebIDBServerConnection::messageSenderConnection()
{
    return WebProcess::shared().webToDatabaseProcessConnection()->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

