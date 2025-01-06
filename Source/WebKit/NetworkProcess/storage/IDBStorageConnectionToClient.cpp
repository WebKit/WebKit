/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "IDBStorageConnectionToClient.h"

#include "WebIDBConnectionToServerMessages.h"
#include "WebIDBResult.h"
#include <WebCore/IDBRequestData.h>
#include <WebCore/IDBResultData.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBStorageConnectionToClient);

IDBStorageConnectionToClient::IDBStorageConnectionToClient(IPC::Connection::UniqueID connection, WebCore::IDBConnectionIdentifier identifier)
    : m_connection(connection)
    , m_identifier(identifier)
    , m_connectionToClient(WebCore::IDBServer::IDBConnectionToClient::create(*this))
{
}

IDBStorageConnectionToClient::~IDBStorageConnectionToClient()
{
    m_connectionToClient->clearDelegate();
}

WebCore::IDBServer::IDBConnectionToClient& IDBStorageConnectionToClient::connectionToClient()
{
    return m_connectionToClient;
}

void IDBStorageConnectionToClient::didDeleteDatabase(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidDeleteDatabase(resultData), 0);
}

void IDBStorageConnectionToClient::didOpenDatabase(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidOpenDatabase(resultData), 0);
}

void IDBStorageConnectionToClient::didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidStartTransaction(transactionIdentifier, error), 0);
}

void IDBStorageConnectionToClient::didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidAbortTransaction(transactionIdentifier, error), 0);
}

void IDBStorageConnectionToClient::didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError& error)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidCommitTransaction(transactionIdentifier, error), 0);
}

void IDBStorageConnectionToClient::didCreateObjectStore(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidCreateObjectStore(resultData), 0);
}

void IDBStorageConnectionToClient::didDeleteObjectStore(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidDeleteObjectStore(resultData), 0);
}

void IDBStorageConnectionToClient::didRenameObjectStore(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidRenameObjectStore(resultData), 0);
}

void IDBStorageConnectionToClient::didClearObjectStore(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidClearObjectStore(resultData), 0);
}

void IDBStorageConnectionToClient::didCreateIndex(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidCreateIndex(resultData), 0);
}

void IDBStorageConnectionToClient::didDeleteIndex(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidDeleteIndex(resultData), 0);
}

void IDBStorageConnectionToClient::didRenameIndex(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidRenameIndex(resultData), 0);
}

void IDBStorageConnectionToClient::didPutOrAdd(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidPutOrAdd(resultData), 0);
}

void IDBStorageConnectionToClient::didGetRecord(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidGetRecord(resultData), 0);
}

void IDBStorageConnectionToClient::didGetAllRecords(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidGetAllRecords(resultData), 0);
}

void IDBStorageConnectionToClient::didGetCount(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidGetCount(resultData), 0);
}

void IDBStorageConnectionToClient::didDeleteRecord(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidDeleteRecord(resultData), 0);
}

void IDBStorageConnectionToClient::didOpenCursor(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidOpenCursor(resultData), 0);
}

void IDBStorageConnectionToClient::didIterateCursor(const WebCore::IDBResultData& resultData)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidIterateCursor(resultData), 0);
}

void IDBStorageConnectionToClient::didGetAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier& requestIdentifier, Vector<WebCore::IDBDatabaseNameAndVersion>&& databases)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidGetAllDatabaseNamesAndVersions(requestIdentifier, databases), 0);
}

void IDBStorageConnectionToClient::fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::FireVersionChangeEvent(connection.identifier(), requestIdentifier, requestedVersion), 0);
}

void IDBStorageConnectionToClient::didCloseFromServer(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection, const WebCore::IDBError& error)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::DidCloseFromServer(connection.identifier(), error), 0);
}

void IDBStorageConnectionToClient::notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    IPC::Connection::send(m_connection, Messages::WebIDBConnectionToServer::NotifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion), 0);
}

} // namespace WebKit
