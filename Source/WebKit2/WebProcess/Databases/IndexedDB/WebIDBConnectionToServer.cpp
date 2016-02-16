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

#if ENABLE(INDEXED_DATABASE)

#include "DatabaseToWebProcessConnectionMessages.h"
#include "WebProcess.h"
#include "WebToDatabaseProcessConnection.h"
#include <WebCore/IDBConnectionToServer.h>
#include <WebCore/IDBOpenDBRequestImpl.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateConnectionToServerIdentifier()
{
    ASSERT(RunLoop::isMain());
    static uint64_t identifier = 0;
    return ++identifier;
}

Ref<WebIDBConnectionToServer> WebIDBConnectionToServer::create()
{
    return adoptRef(*new WebIDBConnectionToServer);
}

WebIDBConnectionToServer::WebIDBConnectionToServer()
    : m_identifier(generateConnectionToServerIdentifier())
{
    relaxAdoptionRequirement();
    m_connectionToServer = IDBClient::IDBConnectionToServer::create(*this);

    send(Messages::DatabaseToWebProcessConnection::EstablishIDBConnectionToServer(m_identifier));

    m_isOpenInServer = true;
}

WebIDBConnectionToServer::~WebIDBConnectionToServer()
{
    if (m_isOpenInServer)
        send(Messages::DatabaseToWebProcessConnection::RemoveIDBConnectionToServer(m_identifier));
}

IPC::Connection* WebIDBConnectionToServer::messageSenderConnection()
{
    return WebProcess::singleton().webToDatabaseProcessConnection()->connection();
}

IDBClient::IDBConnectionToServer& WebIDBConnectionToServer::coreConnectionToServer()
{
    return *m_connectionToServer;
}

void WebIDBConnectionToServer::deleteDatabase(IDBRequestData&)
{
}

void WebIDBConnectionToServer::openDatabase(IDBRequestData&)
{
}

void WebIDBConnectionToServer::abortTransaction(IDBResourceIdentifier&)
{
}

void WebIDBConnectionToServer::commitTransaction(IDBResourceIdentifier&)
{
}

void WebIDBConnectionToServer::didFinishHandlingVersionChangeTransaction(IDBResourceIdentifier&)
{
}

void WebIDBConnectionToServer::createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&)
{
}

void WebIDBConnectionToServer::deleteObjectStore(const IDBRequestData&, const String& objectStoreName)
{
}

void WebIDBConnectionToServer::clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier)
{
}

void WebIDBConnectionToServer::createIndex(const IDBRequestData&, const IDBIndexInfo&)
{
}

void WebIDBConnectionToServer::deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName)
{
}

void WebIDBConnectionToServer::putOrAdd(const IDBRequestData&, IDBKey*, SerializedScriptValue&, const IndexedDB::ObjectStoreOverwriteMode)
{
}

void WebIDBConnectionToServer::getRecord(const IDBRequestData&, const IDBKeyRangeData&)
{
}

void WebIDBConnectionToServer::getCount(const IDBRequestData&, const IDBKeyRangeData&)
{
}

void WebIDBConnectionToServer::deleteRecord(const IDBRequestData&, const IDBKeyRangeData&)
{
}

void WebIDBConnectionToServer::openCursor(const IDBRequestData&, const IDBCursorInfo&)
{
}

void WebIDBConnectionToServer::iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count)
{
}

void WebIDBConnectionToServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&)
{
}

void WebIDBConnectionToServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
}

void WebIDBConnectionToServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
}

void WebIDBConnectionToServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
