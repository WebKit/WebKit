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

using namespace WebCore;

namespace WebKit {

Ref<WebIDBConnectionToClient> WebIDBConnectionToClient::create(DatabaseToWebProcessConnection& connection, uint64_t serverConnectionIdentifier)
{
    return adoptRef(*new WebIDBConnectionToClient(connection, serverConnectionIdentifier));
}

WebIDBConnectionToClient::WebIDBConnectionToClient(DatabaseToWebProcessConnection& connection, uint64_t serverConnectionIdentifier)
    : m_connection(connection)
{
    relaxAdoptionRequirement();
    m_connectionToClient = IDBServer::IDBConnectionToClient::create(*this);
}

WebIDBConnectionToClient::~WebIDBConnectionToClient()
{
}

void WebIDBConnectionToClient::disconnectedFromWebProcess()
{
}

IPC::Connection* WebIDBConnectionToClient::messageSenderConnection()
{
    return m_connection->connection();
}

WebCore::IDBServer::IDBConnectionToClient& WebIDBConnectionToClient::connectionToClient()
{
    return *m_connectionToClient;
}

void WebIDBConnectionToClient::didDeleteDatabase(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didOpenDatabase(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&)
{
}

void WebIDBConnectionToClient::didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&)
{
}

void WebIDBConnectionToClient::didCreateObjectStore(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didDeleteObjectStore(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didClearObjectStore(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didCreateIndex(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didDeleteIndex(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didPutOrAdd(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didGetRecord(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didGetCount(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didDeleteRecord(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didOpenCursor(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::didIterateCursor(const WebCore::IDBResultData&)
{
}

void WebIDBConnectionToClient::fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
}

void WebIDBConnectionToClient::didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&)
{
}

void WebIDBConnectionToClient::notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
