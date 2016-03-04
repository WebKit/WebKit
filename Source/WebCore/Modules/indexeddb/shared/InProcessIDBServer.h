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

#ifndef InProcessIDBServer_h
#define InProcessIDBServer_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBConnectionToServer.h"
#include "IDBOpenDBRequestImpl.h"
#include "IDBServer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IDBServer {
class IDBServer;
}

class InProcessIDBServer final : public IDBClient::IDBConnectionToServerDelegate, public IDBServer::IDBConnectionToClientDelegate, public RefCounted<InProcessIDBServer> {
public:
    WEBCORE_EXPORT static Ref<InProcessIDBServer> create();
    WEBCORE_EXPORT static Ref<InProcessIDBServer> create(const String& databaseDirectoryPath);

    WEBCORE_EXPORT IDBClient::IDBConnectionToServer& connectionToServer() const;
    IDBServer::IDBConnectionToClient& connectionToClient() const;

    // IDBConnectionToServer
    void deleteDatabase(IDBRequestData&) override final;
    void openDatabase(IDBRequestData&) override final;
    void abortTransaction(IDBResourceIdentifier&) override final;
    void commitTransaction(IDBResourceIdentifier&) override final;
    void didFinishHandlingVersionChangeTransaction(IDBResourceIdentifier&) override final;
    void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) override final;
    void deleteObjectStore(const IDBRequestData&, const String& objectStoreName) override final;
    void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier) override final;
    void createIndex(const IDBRequestData&, const IDBIndexInfo&) override final;
    void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) override final;
    void putOrAdd(const IDBRequestData&, IDBKey*, SerializedScriptValue&, const IndexedDB::ObjectStoreOverwriteMode) override final;
    void getRecord(const IDBRequestData&, const IDBKeyRangeData&) override final;
    void getCount(const IDBRequestData&, const IDBKeyRangeData&) override final;
    void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) override final;
    void openCursor(const IDBRequestData&, const IDBCursorInfo&) override final;
    void iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count) override final;
    void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&) override final;
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) override final;
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier) override final;
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier) override final;

    // IDBConnectionToClient
    uint64_t identifier() const override;
    void didDeleteDatabase(const IDBResultData&) override final;
    void didOpenDatabase(const IDBResultData&) override final;
    void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    void didCreateObjectStore(const IDBResultData&) override final;
    void didDeleteObjectStore(const IDBResultData&) override final;
    void didClearObjectStore(const IDBResultData&) override final;
    void didCreateIndex(const IDBResultData&) override final;
    void didDeleteIndex(const IDBResultData&) override final;
    void didPutOrAdd(const IDBResultData&) override final;
    void didGetRecord(const IDBResultData&) override final;
    void didGetCount(const IDBResultData&) override final;
    void didDeleteRecord(const IDBResultData&) override final;
    void didOpenCursor(const IDBResultData&) override final;
    void didIterateCursor(const IDBResultData&) override final;
    void fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) override final;
    void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) override final;

    void ref() override { RefCounted<InProcessIDBServer>::ref(); }
    void deref() override { RefCounted<InProcessIDBServer>::deref(); }

private:
    InProcessIDBServer();
    InProcessIDBServer(const String& databaseDirectoryPath);

    Ref<IDBServer::IDBServer> m_server;
    RefPtr<IDBClient::IDBConnectionToServer> m_connectionToServer;
    RefPtr<IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // InProcessIDBServer_h
