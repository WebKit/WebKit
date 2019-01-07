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
#include "WebIDBConnectionToClient.h"

#if ENABLE(INDEXED_DATABASE)

#include "DataReference.h"
#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebIDBResult.h"
#include <WebCore/IDBError.h>
#include <WebCore/IDBGetAllRecordsData.h>
#include <WebCore/IDBGetRecordData.h>
#include <WebCore/IDBResultData.h>
#include <WebCore/IDBValue.h>
#include <WebCore/ThreadSafeDataBuffer.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>

namespace WebKit {
using namespace WebCore;

Ref<WebIDBConnectionToClient> WebIDBConnectionToClient::create(NetworkProcess& networkProcess, NetworkConnectionToWebProcess& connection, uint64_t serverConnectionIdentifier, PAL::SessionID sessionID)
{
    return adoptRef(*new WebIDBConnectionToClient(networkProcess, connection, serverConnectionIdentifier, sessionID));
}

WebIDBConnectionToClient::WebIDBConnectionToClient(NetworkProcess& networkProcess, NetworkConnectionToWebProcess& connection, uint64_t serverConnectionIdentifier, PAL::SessionID sessionID)
    : m_connection(connection)
    , m_networkProcess(networkProcess)
    , m_identifier(serverConnectionIdentifier)
    , m_sessionID(sessionID)
{
    relaxAdoptionRequirement();
    m_connectionToClient = IDBServer::IDBConnectionToClient::create(*this);
    networkProcess.idbServer(m_sessionID).registerConnection(*m_connectionToClient);
}

WebIDBConnectionToClient::~WebIDBConnectionToClient()
{
}

void WebIDBConnectionToClient::disconnectedFromWebProcess()
{
    m_networkProcess->idbServer(m_sessionID).unregisterConnection(*m_connectionToClient);
}

IPC::Connection* WebIDBConnectionToClient::messageSenderConnection()
{
    return &m_connection->connection();
}

WebCore::IDBServer::IDBConnectionToClient& WebIDBConnectionToClient::connectionToClient()
{
    return *m_connectionToClient;
}

void WebIDBConnectionToClient::didDeleteDatabase(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidDeleteDatabase(resultData));
}

void WebIDBConnectionToClient::didOpenDatabase(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidOpenDatabase(resultData));
}

void WebIDBConnectionToClient::didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    send(Messages::WebIDBConnectionToServer::DidAbortTransaction(transactionIdentifier, error));
}

void WebIDBConnectionToClient::didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    send(Messages::WebIDBConnectionToServer::DidCommitTransaction(transactionIdentifier, error));
}

void WebIDBConnectionToClient::didCreateObjectStore(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidCreateObjectStore(resultData));
}

void WebIDBConnectionToClient::didDeleteObjectStore(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidDeleteObjectStore(resultData));
}

void WebIDBConnectionToClient::didRenameObjectStore(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidRenameObjectStore(resultData));
}

void WebIDBConnectionToClient::didClearObjectStore(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidClearObjectStore(resultData));
}

void WebIDBConnectionToClient::didCreateIndex(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidCreateIndex(resultData));
}

void WebIDBConnectionToClient::didDeleteIndex(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidDeleteIndex(resultData));
}

void WebIDBConnectionToClient::didRenameIndex(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidRenameIndex(resultData));
}

void WebIDBConnectionToClient::didPutOrAdd(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidPutOrAdd(resultData));
}

template<class MessageType> void WebIDBConnectionToClient::handleGetResult(const WebCore::IDBResultData& resultData)
{
    if (resultData.type() == IDBResultType::Error) {
        send(MessageType(resultData));
        return;
    }

    if (resultData.type() == IDBResultType::GetAllRecordsSuccess && resultData.getAllResult().type() == IndexedDB::GetAllType::Keys) {
        send(MessageType(resultData));
        return;
    }

    auto blobFilePaths = resultData.type() == IDBResultType::GetAllRecordsSuccess ? resultData.getAllResult().allBlobFilePaths() : resultData.getResult().value().blobFilePaths();
    if (blobFilePaths.isEmpty()) {
        send(MessageType(resultData));
        return;
    }

    send(MessageType(resultData));
}

void WebIDBConnectionToClient::didGetRecord(const WebCore::IDBResultData& resultData)
{
    handleGetResult<Messages::WebIDBConnectionToServer::DidGetRecord>(resultData);
}

void WebIDBConnectionToClient::didGetAllRecords(const WebCore::IDBResultData& resultData)
{
    handleGetResult<Messages::WebIDBConnectionToServer::DidGetAllRecords>(resultData);
}

void WebIDBConnectionToClient::didGetCount(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidGetCount(resultData));
}

void WebIDBConnectionToClient::didDeleteRecord(const WebCore::IDBResultData& resultData)
{
    send(Messages::WebIDBConnectionToServer::DidDeleteRecord(resultData));
}

void WebIDBConnectionToClient::didOpenCursor(const WebCore::IDBResultData& resultData)
{
    handleGetResult<Messages::WebIDBConnectionToServer::DidOpenCursor>(resultData);
}

void WebIDBConnectionToClient::didIterateCursor(const WebCore::IDBResultData& resultData)
{
    handleGetResult<Messages::WebIDBConnectionToServer::DidIterateCursor>(resultData);
}

void WebIDBConnectionToClient::fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    send(Messages::WebIDBConnectionToServer::FireVersionChangeEvent(connection.identifier(), requestIdentifier, requestedVersion));
}

void WebIDBConnectionToClient::didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    send(Messages::WebIDBConnectionToServer::DidStartTransaction(transactionIdentifier, error));
}

void WebIDBConnectionToClient::didCloseFromServer(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection, const WebCore::IDBError& error)
{
    send(Messages::WebIDBConnectionToServer::DidCloseFromServer(connection.identifier(), error));
}

void WebIDBConnectionToClient::notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    send(Messages::WebIDBConnectionToServer::NotifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion));
}

void WebIDBConnectionToClient::didGetAllDatabaseNames(uint64_t callbackID, const Vector<String>& databaseNames)
{
    send(Messages::WebIDBConnectionToServer::DidGetAllDatabaseNames(callbackID, databaseNames));
}

void WebIDBConnectionToClient::deleteDatabase(const IDBRequestData& request)
{
    m_networkProcess->idbServer(m_sessionID).deleteDatabase(request);
}

void WebIDBConnectionToClient::openDatabase(const IDBRequestData& request)
{
    m_networkProcess->idbServer(m_sessionID).openDatabase(request);
}

void WebIDBConnectionToClient::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).abortTransaction(transactionIdentifier);
}

void WebIDBConnectionToClient::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).commitTransaction(transactionIdentifier);
}

void WebIDBConnectionToClient::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).didFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
}

void WebIDBConnectionToClient::createObjectStore(const IDBRequestData& request, const IDBObjectStoreInfo& info)
{
    m_networkProcess->idbServer(m_sessionID).createObjectStore(request, info);
}

void WebIDBConnectionToClient::deleteObjectStore(const IDBRequestData& request, const String& name)
{
    m_networkProcess->idbServer(m_sessionID).deleteObjectStore(request, name);
}

void WebIDBConnectionToClient::renameObjectStore(const IDBRequestData& request, uint64_t objectStoreIdentifier, const String& newName)
{
    m_networkProcess->idbServer(m_sessionID).renameObjectStore(request, objectStoreIdentifier, newName);
}

void WebIDBConnectionToClient::clearObjectStore(const IDBRequestData& request, uint64_t objectStoreIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).clearObjectStore(request, objectStoreIdentifier);
}

void WebIDBConnectionToClient::createIndex(const IDBRequestData& request, const IDBIndexInfo& info)
{
    m_networkProcess->idbServer(m_sessionID).createIndex(request, info);
}

void WebIDBConnectionToClient::deleteIndex(const IDBRequestData& request, uint64_t objectStoreIdentifier, const String& name)
{
    m_networkProcess->idbServer(m_sessionID).deleteIndex(request, objectStoreIdentifier, name);
}

void WebIDBConnectionToClient::renameIndex(const IDBRequestData& request, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    m_networkProcess->idbServer(m_sessionID).renameIndex(request, objectStoreIdentifier, indexIdentifier, newName);
}

void WebIDBConnectionToClient::putOrAdd(const IDBRequestData& request, const IDBKeyData& key, const IDBValue& value, unsigned overwriteMode)
{
    if (overwriteMode != static_cast<unsigned>(IndexedDB::ObjectStoreOverwriteMode::NoOverwrite)
        && overwriteMode != static_cast<unsigned>(IndexedDB::ObjectStoreOverwriteMode::Overwrite)
        && overwriteMode != static_cast<unsigned>(IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor)) {
        // FIXME: This message from the WebProcess is corrupt.
        // The Network Process should return early at this point, but can we also kill the bad WebProcess?
        return;
    }

    IndexedDB::ObjectStoreOverwriteMode mode = static_cast<IndexedDB::ObjectStoreOverwriteMode>(overwriteMode);

    m_networkProcess->idbServer(m_sessionID).putOrAdd(request, key, value, mode);
}

void WebIDBConnectionToClient::getRecord(const IDBRequestData& request, const IDBGetRecordData& getRecordData)
{
    m_networkProcess->idbServer(m_sessionID).getRecord(request, getRecordData);
}

void WebIDBConnectionToClient::getAllRecords(const IDBRequestData& request, const IDBGetAllRecordsData& getAllRecordsData)
{
    m_networkProcess->idbServer(m_sessionID).getAllRecords(request, getAllRecordsData);
}

void WebIDBConnectionToClient::getCount(const IDBRequestData& request, const IDBKeyRangeData& range)
{
    m_networkProcess->idbServer(m_sessionID).getCount(request, range);
}

void WebIDBConnectionToClient::deleteRecord(const IDBRequestData& request, const IDBKeyRangeData& range)
{
    m_networkProcess->idbServer(m_sessionID).deleteRecord(request, range);
}

void WebIDBConnectionToClient::openCursor(const IDBRequestData& request, const IDBCursorInfo& info)
{
    m_networkProcess->idbServer(m_sessionID).openCursor(request, info);
}

void WebIDBConnectionToClient::iterateCursor(const IDBRequestData& request, const IDBIterateCursorData& data)
{
    m_networkProcess->idbServer(m_sessionID).iterateCursor(request, data);
}

void WebIDBConnectionToClient::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    m_networkProcess->idbServer(m_sessionID).establishTransaction(databaseConnectionIdentifier, info);
}

void WebIDBConnectionToClient::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).databaseConnectionPendingClose(databaseConnectionIdentifier);
}

void WebIDBConnectionToClient::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).databaseConnectionClosed(databaseConnectionIdentifier);
}

void WebIDBConnectionToClient::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void WebIDBConnectionToClient::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).didFireVersionChangeEvent(databaseConnectionIdentifier, transactionIdentifier);
}

void WebIDBConnectionToClient::openDBRequestCancelled(const IDBRequestData& requestData)
{
    m_networkProcess->idbServer(m_sessionID).openDBRequestCancelled(requestData);
}

void WebIDBConnectionToClient::confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier)
{
    m_networkProcess->idbServer(m_sessionID).confirmDidCloseFromServer(databaseConnectionIdentifier);
}

void WebIDBConnectionToClient::getAllDatabaseNames(uint64_t serverConnectionIdentifier, const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& openingOrigin, uint64_t callbackID)
{
    m_networkProcess->idbServer(m_sessionID).getAllDatabaseNames(serverConnectionIdentifier, topOrigin, openingOrigin, callbackID);
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
