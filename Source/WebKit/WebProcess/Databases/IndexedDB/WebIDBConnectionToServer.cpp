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
#include "WebIDBConnectionToServer.h"

#include "DataReference.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkStorageManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebIDBResult.h"
#include "WebProcess.h"
#include <WebCore/IDBConnectionToServer.h>
#include <WebCore/IDBCursorInfo.h>
#include <WebCore/IDBError.h>
#include <WebCore/IDBIndexInfo.h>
#include <WebCore/IDBIterateCursorData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/IDBObjectStoreInfo.h>
#include <WebCore/IDBOpenDBRequest.h>
#include <WebCore/IDBRequestData.h>
#include <WebCore/IDBResourceIdentifier.h>
#include <WebCore/IDBResultData.h>
#include <WebCore/IDBTransactionInfo.h>
#include <WebCore/IDBValue.h>
#include <WebCore/ProcessIdentifier.h>

namespace WebKit {
using namespace WebCore;

Ref<WebIDBConnectionToServer> WebIDBConnectionToServer::create()
{
    return adoptRef(*new WebIDBConnectionToServer());
}

WebIDBConnectionToServer::WebIDBConnectionToServer()
    : m_connectionToServer(IDBClient::IDBConnectionToServer::create(*this))
{
}

WebIDBConnectionToServer::~WebIDBConnectionToServer()
{
}

IDBConnectionIdentifier WebIDBConnectionToServer::identifier() const
{
    return Process::identifier();
}

IPC::Connection* WebIDBConnectionToServer::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

IDBClient::IDBConnectionToServer& WebIDBConnectionToServer::coreConnectionToServer()
{
    return m_connectionToServer;
}

void WebIDBConnectionToServer::deleteDatabase(const IDBRequestData& requestData)
{
    send(Messages::NetworkStorageManager::DeleteDatabase(requestData));
}

void WebIDBConnectionToServer::openDatabase(const IDBRequestData& requestData)
{
    m_origins.add(requestData.requestIdentifier(), requestData.databaseIdentifier().origin());
    send(Messages::NetworkStorageManager::OpenDatabase(requestData));
}

void WebIDBConnectionToServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    auto iterator = m_origins.find(transactionIdentifier);
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::AbortTransaction(iterator->value, transactionIdentifier));
}

void WebIDBConnectionToServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    auto iterator = m_origins.find(transactionIdentifier);
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::CommitTransaction(iterator->value, transactionIdentifier, pendingRequestCount));
}

void WebIDBConnectionToServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    auto iterator = m_originsByConnection.find(databaseConnectionIdentifier);
    RELEASE_ASSERT(iterator != m_originsByConnection.end());
    send(Messages::NetworkStorageManager::DidFinishHandlingVersionChangeTransaction(iterator->value, databaseConnectionIdentifier, transactionIdentifier));
}

void WebIDBConnectionToServer::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::CreateObjectStore(iterator->value, requestData, info));
}

void WebIDBConnectionToServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::DeleteObjectStore(iterator->value, requestData, objectStoreName));
}

void WebIDBConnectionToServer::renameObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::RenameObjectStore(iterator->value, requestData, objectStoreIdentifier, newName));
}

void WebIDBConnectionToServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::ClearObjectStore(iterator->value, requestData, objectStoreIdentifier));
}

void WebIDBConnectionToServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::CreateIndex(iterator->value, requestData, info));
}

void WebIDBConnectionToServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::DeleteIndex(iterator->value, requestData, objectStoreIdentifier, indexName));
}

void WebIDBConnectionToServer::renameIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::RenameIndex(iterator->value, requestData, objectStoreIdentifier, indexIdentifier, newName));
}

void WebIDBConnectionToServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode mode)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::PutOrAdd(iterator->value, requestData, keyData, value, mode));
}

void WebIDBConnectionToServer::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::GetRecord(iterator->value, requestData, getRecordData));
}

void WebIDBConnectionToServer::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::GetAllRecords(iterator->value, requestData, getAllRecordsData));
}

void WebIDBConnectionToServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& range)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::GetCount(iterator->value, requestData, range));
}

void WebIDBConnectionToServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& range)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::DeleteRecord(iterator->value, requestData, range));
}

void WebIDBConnectionToServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::OpenCursor(iterator->value, requestData, info));
}

void WebIDBConnectionToServer::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    auto iterator = m_origins.find(requestData.transactionIdentifier());
    RELEASE_ASSERT(iterator != m_origins.end());
    send(Messages::NetworkStorageManager::IterateCursor(iterator->value, requestData, data));
}

void WebIDBConnectionToServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    auto iterator = m_originsByConnection.find(databaseConnectionIdentifier);
    RELEASE_ASSERT(iterator != m_originsByConnection.end());
    m_origins.add(info.identifier(), iterator->value);
    send(Messages::NetworkStorageManager::EstablishTransaction(iterator->value, databaseConnectionIdentifier, info));
}

void WebIDBConnectionToServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    auto iterator = m_originsByConnection.find(databaseConnectionIdentifier);
    RELEASE_ASSERT(iterator != m_originsByConnection.end());
    send(Messages::NetworkStorageManager::DatabaseConnectionPendingClose(iterator->value, databaseConnectionIdentifier));
}

void WebIDBConnectionToServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    auto origin = m_originsByConnection.take(databaseConnectionIdentifier);
    send(Messages::NetworkStorageManager::DatabaseConnectionClosed(origin, databaseConnectionIdentifier));
}

void WebIDBConnectionToServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>& transactionIdentifier)
{
    auto iterator = m_originsByConnection.find(databaseConnectionIdentifier);
    RELEASE_ASSERT(iterator != m_originsByConnection.end());
    send(Messages::NetworkStorageManager::AbortOpenAndUpgradeNeeded(iterator->value, databaseConnectionIdentifier, transactionIdentifier));
}

void WebIDBConnectionToServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, const IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    auto iterator = m_originsByConnection.find(databaseConnectionIdentifier);
    RELEASE_ASSERT(iterator != m_originsByConnection.end());
    send(Messages::NetworkStorageManager::DidFireVersionChangeEvent(iterator->value, databaseConnectionIdentifier, requestIdentifier, connectionClosed));
}

void WebIDBConnectionToServer::openDBRequestCancelled(const IDBRequestData& requestData)
{
    send(Messages::NetworkStorageManager::OpenDBRequestCancelled(requestData));
}

void WebIDBConnectionToServer::getAllDatabaseNamesAndVersions(const IDBResourceIdentifier& requestIdentifier, const ClientOrigin& origin)
{
    send(Messages::NetworkStorageManager::GetAllDatabaseNamesAndVersions(requestIdentifier, origin));
}

void WebIDBConnectionToServer::didDeleteDatabase(const IDBResultData& result)
{
    m_connectionToServer->didDeleteDatabase(result);
}

void WebIDBConnectionToServer::didOpenDatabase(const IDBResultData& result)
{
    auto origin =  m_origins.take(result.requestIdentifier());
    if (result.type() == IDBResultType::OpenDatabaseSuccess || result.type() == IDBResultType::OpenDatabaseUpgradeNeeded) {
        ASSERT(result.databaseConnectionIdentifier());
        m_originsByConnection.add(result.databaseConnectionIdentifier(), origin);
    }

    m_connectionToServer->didOpenDatabase(result);
}

void WebIDBConnectionToServer::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    m_origins.remove(transactionIdentifier);
    m_connectionToServer->didAbortTransaction(transactionIdentifier, error);
}

void WebIDBConnectionToServer::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    m_origins.remove(transactionIdentifier);
    m_connectionToServer->didCommitTransaction(transactionIdentifier, error);
}

void WebIDBConnectionToServer::didCreateObjectStore(const IDBResultData& result)
{
    m_connectionToServer->didCreateObjectStore(result);
}

void WebIDBConnectionToServer::didDeleteObjectStore(const IDBResultData& result)
{
    m_connectionToServer->didDeleteObjectStore(result);
}

void WebIDBConnectionToServer::didRenameObjectStore(const IDBResultData& result)
{
    m_connectionToServer->didRenameObjectStore(result);
}

void WebIDBConnectionToServer::didClearObjectStore(const IDBResultData& result)
{
    m_connectionToServer->didClearObjectStore(result);
}

void WebIDBConnectionToServer::didCreateIndex(const IDBResultData& result)
{
    m_connectionToServer->didCreateIndex(result);
}

void WebIDBConnectionToServer::didDeleteIndex(const IDBResultData& result)
{
    m_connectionToServer->didDeleteIndex(result);
}

void WebIDBConnectionToServer::didRenameIndex(const IDBResultData& result)
{
    m_connectionToServer->didRenameIndex(result);
}

void WebIDBConnectionToServer::didPutOrAdd(const IDBResultData& result)
{
    m_connectionToServer->didPutOrAdd(result);
}

void WebIDBConnectionToServer::didGetRecord(const WebIDBResult& result)
{
    m_connectionToServer->didGetRecord(result.resultData());
}

void WebIDBConnectionToServer::didGetAllRecords(const WebIDBResult& result)
{
    m_connectionToServer->didGetAllRecords(result.resultData());
}

void WebIDBConnectionToServer::didGetCount(const IDBResultData& result)
{
    m_connectionToServer->didGetCount(result);
}

void WebIDBConnectionToServer::didDeleteRecord(const IDBResultData& result)
{
    m_connectionToServer->didDeleteRecord(result);
}

void WebIDBConnectionToServer::didOpenCursor(const WebIDBResult& result)
{
    m_connectionToServer->didOpenCursor(result.resultData());
}

void WebIDBConnectionToServer::didIterateCursor(const WebIDBResult& result)
{
    m_connectionToServer->didIterateCursor(result.resultData());
}

void WebIDBConnectionToServer::fireVersionChangeEvent(uint64_t uniqueDatabaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    m_connectionToServer->fireVersionChangeEvent(uniqueDatabaseConnectionIdentifier, requestIdentifier, requestedVersion);
}

void WebIDBConnectionToServer::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    m_connectionToServer->didStartTransaction(transactionIdentifier, error);
}

void WebIDBConnectionToServer::didCloseFromServer(uint64_t databaseConnectionIdentifier, const IDBError& error)
{
    m_connectionToServer->didCloseFromServer(databaseConnectionIdentifier, error);
}

void WebIDBConnectionToServer::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    m_connectionToServer->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
}

void WebIDBConnectionToServer::didGetAllDatabaseNamesAndVersions(const IDBResourceIdentifier& requestIdentifier, Vector<IDBDatabaseNameAndVersion>&& databases)
{
    m_connectionToServer->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(databases));
}

void WebIDBConnectionToServer::connectionToServerLost()
{
    m_connectionToServer->connectionToServerLost(IDBError { WebCore::UnknownError, "An internal error was encountered in the Indexed Database server"_s });
}

} // namespace WebKit
