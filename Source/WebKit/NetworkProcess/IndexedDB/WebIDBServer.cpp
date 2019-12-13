/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#include "config.h"
#include "WebIDBServer.h"

#include "WebIDBConnectionToClient.h"
#include <WebCore/SQLiteDatabaseTracker.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/threads/BinarySemaphore.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit {

Ref<WebIDBServer> WebIDBServer::create(PAL::SessionID sessionID, const String& directory, IDBServer::IDBServer::StorageQuotaManagerSpaceRequester&& spaceRequester)
{
    return adoptRef(*new WebIDBServer(sessionID, directory, WTFMove(spaceRequester)));
}

WebIDBServer::WebIDBServer(PAL::SessionID sessionID, const String& directory, IDBServer::IDBServer::StorageQuotaManagerSpaceRequester&& spaceRequester)
    : m_server(IDBServer::IDBServer::create(sessionID, directory, WTFMove(spaceRequester)))
{
    ASSERT(RunLoop::isMain());
}

void WebIDBServer::registerConnection(WebCore::IDBServer::IDBConnectionToClient& connection)
{
    ASSERT(RunLoop::isMain());

    m_server->registerConnection(connection);
}

void WebIDBServer::unregisterConnection(WebCore::IDBServer::IDBConnectionToClient& connection)
{
    ASSERT(RunLoop::isMain());

    m_server->unregisterConnection(connection);
}
    
void WebIDBServer::closeAndDeleteDatabasesModifiedSince(WallTime modificationTime, CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    m_server->closeAndDeleteDatabasesModifiedSince(modificationTime, WTFMove(callback));
}

void WebIDBServer::closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>& originDatas, CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    m_server->closeAndDeleteDatabasesForOrigins(originDatas, WTFMove(callback));
}

void WebIDBServer::suspend(IDBServer::ShouldForceStop shouldForceStop)
{
    ASSERT(RunLoop::isMain());

    if (m_isSuspended)
        return;

    m_isSuspended = true;
    m_server->tryStop(shouldForceStop);
}

void WebIDBServer::resume()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_isSuspended);

    m_isSuspended = false;
    m_server->resume();
}

void WebIDBServer::openDatabase(const WebCore::IDBRequestData& requestData)
{
    ASSERT(RunLoop::isMain());

    m_server->openDatabase(requestData);
}

void WebIDBServer::deleteDatabase(const WebCore::IDBRequestData& requestData)
{
    ASSERT(RunLoop::isMain());

    m_server->deleteDatabase(requestData);
}

void WebIDBServer::abortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->abortTransaction(transactionIdentifier);
}

void WebIDBServer::commitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->commitTransaction(transactionIdentifier);
}

void WebIDBServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->didFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
}

void WebIDBServer::createObjectStore(const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    ASSERT(RunLoop::isMain());

    m_server->createObjectStore(requestData, objectStoreInfo);
}

void WebIDBServer::deleteObjectStore(const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    ASSERT(RunLoop::isMain());

    m_server->deleteObjectStore(requestData, objectStoreName);
}

void WebIDBServer::renameObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    ASSERT(RunLoop::isMain());

    m_server->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void WebIDBServer::clearObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->clearObjectStore(requestData, objectStoreIdentifier);
}

void WebIDBServer::createIndex(const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    ASSERT(RunLoop::isMain());

    m_server->createIndex(requestData, indexInfo);
}

void WebIDBServer::deleteIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    ASSERT(RunLoop::isMain());

    m_server->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void WebIDBServer::renameIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    ASSERT(RunLoop::isMain());

    m_server->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void WebIDBServer::putOrAdd(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, WebCore::IndexedDB::ObjectStoreOverwriteMode overWriteMode)
{
    ASSERT(RunLoop::isMain());

    m_server->putOrAdd(requestData, keyData, value, overWriteMode);
}

void WebIDBServer::getRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    ASSERT(RunLoop::isMain());

    m_server->getRecord(requestData, getRecordData);
}

void WebIDBServer::getAllRecords(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    ASSERT(RunLoop::isMain());

    m_server->getAllRecords(requestData, getAllRecordsData);
}

void WebIDBServer::getCount(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    ASSERT(RunLoop::isMain());

    m_server->getCount(requestData, keyRangeData);
}

void WebIDBServer::deleteRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    ASSERT(RunLoop::isMain());

    m_server->deleteRecord(requestData, keyRangeData);
}

void WebIDBServer::openCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    ASSERT(RunLoop::isMain());

    m_server->openCursor(requestData, cursorInfo);
}

void WebIDBServer::iterateCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& iterateCursorData)
{
    ASSERT(RunLoop::isMain());

    m_server->iterateCursor(requestData, iterateCursorData);
}

void WebIDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    ASSERT(RunLoop::isMain());

    m_server->establishTransaction(databaseConnectionIdentifier, transactionInfo);
}

void WebIDBServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->databaseConnectionPendingClose(databaseConnectionIdentifier);
}

void WebIDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->databaseConnectionClosed(databaseConnectionIdentifier);
}

void WebIDBServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void WebIDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    ASSERT(RunLoop::isMain());

    m_server->didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, connectionClosed);
}

void WebIDBServer::openDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    ASSERT(RunLoop::isMain());

    m_server->openDBRequestCancelled(requestData);
}

void WebIDBServer::getAllDatabaseNames(IPC::Connection& connection, const WebCore::SecurityOriginData& mainFrameOrigin, const WebCore::SecurityOriginData& openingOrigin, uint64_t callbackID)
{
    ASSERT(RunLoop::isMain());

    auto* webIDBConnection = m_connectionMap.get(connection.uniqueID());
    ASSERT(webIDBConnection);

    m_server->getAllDatabaseNames(webIDBConnection->identifier(), mainFrameOrigin, openingOrigin, callbackID);
}
    
void WebIDBServer::confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier)
{
    ASSERT(RunLoop::isMain());

    m_server->confirmDidCloseFromServer(databaseConnectionIdentifier);
}

void WebIDBServer::addConnection(IPC::Connection& connection, WebCore::ProcessIdentifier processIdentifier)
{
    ASSERT(RunLoop::isMain());

    auto[iter, isNewEntry] = m_connectionMap.ensure(connection.uniqueID(), [&connection, processIdentifier] {
        return makeUnique<WebIDBConnectionToClient>(connection, processIdentifier);
    });
    
    ASSERT(isNewEntry);

    m_server->registerConnection(iter->value->connectionToClient());
}

void WebIDBServer::removeConnection(IPC::Connection& connection)
{
    auto takenConnection = m_connectionMap.take(connection.uniqueID());

    ASSERT(takenConnection);

    m_server->unregisterConnection(takenConnection->connectionToClient());
}

} // namespace WebKit
#endif
