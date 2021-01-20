/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "WebCoreArgumentCoders.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebIDBResult.h"
#include "WebIDBServer.h"
#include <WebCore/IDBGetAllRecordsData.h>
#include <WebCore/IDBGetRecordData.h>
#include <WebCore/IDBResultData.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>

namespace WebKit {
using namespace WebCore;

WebIDBConnectionToClient::WebIDBConnectionToClient(IPC::Connection& connection, WebCore::IDBConnectionIdentifier serverConnectionIdentifier)
    : m_connection(makeRef(connection))
    , m_identifier(serverConnectionIdentifier)
    , m_connectionToClient(IDBServer::IDBConnectionToClient::create(*this))
{
}

WebIDBConnectionToClient::~WebIDBConnectionToClient()
{
    m_connectionToClient->clearDelegate();
}

IPC::Connection* WebIDBConnectionToClient::messageSenderConnection() const
{
    return m_connection.ptr();
}

WebCore::IDBServer::IDBConnectionToClient& WebIDBConnectionToClient::connectionToClient()
{
    return m_connectionToClient;
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

void WebIDBConnectionToClient::didGetAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier& requestIdentifier, const Vector<WebCore::IDBDatabaseNameAndVersion>& databases)
{
    send(Messages::WebIDBConnectionToServer::DidGetAllDatabaseNamesAndVersions(requestIdentifier, databases));
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
