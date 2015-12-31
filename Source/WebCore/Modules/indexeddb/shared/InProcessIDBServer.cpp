/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "InProcessIDBServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBConnectionToServer.h"
#include "IDBCursorInfo.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenDBRequestImpl.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include <wtf/RunLoop.h>

namespace WebCore {

Ref<InProcessIDBServer> InProcessIDBServer::create()
{
    Ref<InProcessIDBServer> server = adoptRef(*new InProcessIDBServer);
    server->m_server->registerConnection(server->connectionToClient());
    return server;
}

InProcessIDBServer::InProcessIDBServer()
    : m_server(IDBServer::IDBServer::create())
{
    relaxAdoptionRequirement();
    m_connectionToServer = IDBClient::IDBConnectionToServer::create(*this);
    m_connectionToClient = IDBServer::IDBConnectionToClient::create(*this);
}

uint64_t InProcessIDBServer::identifier() const
{
    // An instance of InProcessIDBServer always has a 1:1 relationship with its instance of IDBServer.
    // Therefore the connection identifier between the two can always be "1".
    return 1;
}

IDBClient::IDBConnectionToServer& InProcessIDBServer::connectionToServer() const
{
    return *m_connectionToServer;
}

IDBServer::IDBConnectionToClient& InProcessIDBServer::connectionToClient() const
{
    return *m_connectionToClient;
}

void InProcessIDBServer::deleteDatabase(IDBRequestData& requestData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData] {
        m_server->deleteDatabase(requestData);
    });
}

void InProcessIDBServer::didDeleteDatabase(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didDeleteDatabase(resultData);
    });
}

void InProcessIDBServer::openDatabase(IDBRequestData& requestData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData] {
        m_server->openDatabase(requestData);
    });
}

void InProcessIDBServer::didOpenDatabase(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didOpenDatabase(resultData);
    });
}

void InProcessIDBServer::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, transactionIdentifier, error] {
        m_connectionToServer->didAbortTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, transactionIdentifier, error] {
        m_connectionToServer->didCommitTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::didCreateObjectStore(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didCreateObjectStore(resultData);
    });
}

void InProcessIDBServer::didDeleteObjectStore(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didDeleteObjectStore(resultData);
    });
}

void InProcessIDBServer::didClearObjectStore(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didClearObjectStore(resultData);
    });
}

void InProcessIDBServer::didCreateIndex(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didCreateIndex(resultData);
    });
}

void InProcessIDBServer::didDeleteIndex(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didDeleteIndex(resultData);
    });
}

void InProcessIDBServer::didPutOrAdd(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didPutOrAdd(resultData);
    });
}

void InProcessIDBServer::didGetRecord(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didGetRecord(resultData);
    });
}

void InProcessIDBServer::didGetCount(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didGetCount(resultData);
    });
}

void InProcessIDBServer::didDeleteRecord(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didDeleteRecord(resultData);
    });
}

void InProcessIDBServer::didOpenCursor(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didOpenCursor(resultData);
    });
}

void InProcessIDBServer::didIterateCursor(const IDBResultData& resultData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData] {
        m_connectionToServer->didIterateCursor(resultData);
    });
}

void InProcessIDBServer::abortTransaction(IDBResourceIdentifier& resourceIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resourceIdentifier] {
        m_server->abortTransaction(resourceIdentifier);
    });
}

void InProcessIDBServer::commitTransaction(IDBResourceIdentifier& resourceIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resourceIdentifier] {
        m_server->commitTransaction(resourceIdentifier);
    });
}

void InProcessIDBServer::didFinishHandlingVersionChangeTransaction(IDBResourceIdentifier& transactionIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, transactionIdentifier] {
        m_server->didFinishHandlingVersionChangeTransaction(transactionIdentifier);
    });
}

void InProcessIDBServer::createObjectStore(const IDBRequestData& resultData, const IDBObjectStoreInfo& info)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, resultData, info] {
        m_server->createObjectStore(resultData, info);
    });
}

void InProcessIDBServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData, objectStoreName] {
        m_server->deleteObjectStore(requestData, objectStoreName);
    });
}

void InProcessIDBServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData, objectStoreIdentifier] {
        m_server->clearObjectStore(requestData, objectStoreIdentifier);
    });
}

void InProcessIDBServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData, info] {
        m_server->createIndex(requestData, info);
    });
}

void InProcessIDBServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData, objectStoreIdentifier, indexName] {
        m_server->deleteIndex(requestData, objectStoreIdentifier, indexName);
    });
}

void InProcessIDBServer::putOrAdd(const IDBRequestData& requestData, IDBKey* key, SerializedScriptValue& value, const IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    RefPtr<InProcessIDBServer> self(this);
    IDBKeyData keyData(key);
    auto valueData = ThreadSafeDataBuffer::copyVector(value.data());

    RunLoop::current().dispatch([this, self, requestData, keyData, valueData, overwriteMode] {
        m_server->putOrAdd(requestData, keyData, valueData, overwriteMode);
    });
}

void InProcessIDBServer::getRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    RefPtr<InProcessIDBServer> self(this);

    RunLoop::current().dispatch([this, self, requestData, keyRangeData] {
        m_server->getRecord(requestData, keyRangeData);
    });
}

void InProcessIDBServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestData, keyRangeData] {
        m_server->getCount(requestData, keyRangeData);
    });
}

void InProcessIDBServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    RefPtr<InProcessIDBServer> self(this);

    RunLoop::current().dispatch([this, self, requestData, keyRangeData] {
        m_server->deleteRecord(requestData, keyRangeData);
    });
}

void InProcessIDBServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    RefPtr<InProcessIDBServer> self(this);

    RunLoop::current().dispatch([this, self, requestData, info] {
        m_server->openCursor(requestData, info);
    });
}

void InProcessIDBServer::iterateCursor(const IDBRequestData& requestData, const IDBKeyData& key, unsigned long count)
{
    RefPtr<InProcessIDBServer> self(this);

    RunLoop::current().dispatch([this, self, requestData, key, count] {
        m_server->iterateCursor(requestData, key, count);
    });
}

void InProcessIDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    RefPtr<InProcessIDBServer> self(this);

    RunLoop::current().dispatch([this, self, databaseConnectionIdentifier, info] {
        m_server->establishTransaction(databaseConnectionIdentifier, info);
    });
}

void InProcessIDBServer::fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    RefPtr<InProcessIDBServer> self(this);
    uint64_t databaseConnectionIdentifier = connection.identifier();
    RunLoop::current().dispatch([this, self, databaseConnectionIdentifier, requestIdentifier, requestedVersion] {
        m_connectionToServer->fireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, requestedVersion);
    });
}

void InProcessIDBServer::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, transactionIdentifier, error] {
        m_connectionToServer->didStartTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, requestIdentifier, oldVersion, newVersion] {
        m_connectionToServer->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
    });
}

void InProcessIDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, databaseConnectionIdentifier] {
        m_server->databaseConnectionClosed(databaseConnectionIdentifier);
    });
}

void InProcessIDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    RefPtr<InProcessIDBServer> self(this);
    RunLoop::current().dispatch([this, self, databaseConnectionIdentifier, requestIdentifier] {
        m_server->didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier);
    });
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
