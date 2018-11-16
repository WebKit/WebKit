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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBConnectionToServer.h"
#include "IDBServer.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IDBServer {
class IDBServer;
}

class InProcessIDBServer final : public IDBClient::IDBConnectionToServerDelegate, public IDBServer::IDBConnectionToClientDelegate, public RefCounted<InProcessIDBServer>, public IDBServer::IDBBackingStoreTemporaryFileHandler {
public:
    WEBCORE_EXPORT static Ref<InProcessIDBServer> create();
    WEBCORE_EXPORT static Ref<InProcessIDBServer> create(const String& databaseDirectoryPath);

    WEBCORE_EXPORT IDBClient::IDBConnectionToServer& connectionToServer() const;
    IDBServer::IDBConnectionToClient& connectionToClient() const;
    IDBServer::IDBServer& server() { return m_server.get(); }

    IDBServer::IDBServer& idbServer() { return m_server.get(); }

    // IDBConnectionToServer
    void deleteDatabase(const IDBRequestData&) final;
    void openDatabase(const IDBRequestData&) final;
    void abortTransaction(const IDBResourceIdentifier&) final;
    void commitTransaction(const IDBResourceIdentifier&) final;
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier&) final;
    void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) final;
    void deleteObjectStore(const IDBRequestData&, const String& objectStoreName) final;
    void renameObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName) final;
    void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier) final;
    void createIndex(const IDBRequestData&, const IDBIndexInfo&) final;
    void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) final;
    void renameIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName) final;
    void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, const IndexedDB::ObjectStoreOverwriteMode) final;
    void getRecord(const IDBRequestData&, const IDBGetRecordData&) final;
    void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&) final;
    void getCount(const IDBRequestData&, const IDBKeyRangeData&) final;
    void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) final;
    void openCursor(const IDBRequestData&, const IDBCursorInfo&) final;
    void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&) final;
    void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&) final;
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier) final;
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) final;
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier) final;
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier) final;
    void openDBRequestCancelled(const IDBRequestData&) final;
    void confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier) final;
    void getAllDatabaseNames(const SecurityOriginData& mainFrameOrigin, const SecurityOriginData& openingOrigin, uint64_t callbackID) final;

    // IDBConnectionToClient
    uint64_t identifier() const override;
    void didDeleteDatabase(const IDBResultData&) final;
    void didOpenDatabase(const IDBResultData&) final;
    void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) final;
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) final;
    void didCreateObjectStore(const IDBResultData&) final;
    void didDeleteObjectStore(const IDBResultData&) final;
    void didRenameObjectStore(const IDBResultData&) final;
    void didClearObjectStore(const IDBResultData&) final;
    void didCreateIndex(const IDBResultData&) final;
    void didDeleteIndex(const IDBResultData&) final;
    void didRenameIndex(const IDBResultData&) final;
    void didPutOrAdd(const IDBResultData&) final;
    void didGetRecord(const IDBResultData&) final;
    void didGetAllRecords(const IDBResultData&) final;
    void didGetCount(const IDBResultData&) final;
    void didDeleteRecord(const IDBResultData&) final;
    void didOpenCursor(const IDBResultData&) final;
    void didIterateCursor(const IDBResultData&) final;
    void fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) final;
    void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) final;
    void didCloseFromServer(IDBServer::UniqueIDBDatabaseConnection&, const IDBError&) final;
    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) final;
    void didGetAllDatabaseNames(uint64_t callbackID, const Vector<String>& databaseNames) final;

    void ref() override { RefCounted<InProcessIDBServer>::ref(); }
    void deref() override { RefCounted<InProcessIDBServer>::deref(); }

    void accessToTemporaryFileComplete(const String& path) override;

private:
    InProcessIDBServer();
    InProcessIDBServer(const String& databaseDirectoryPath);

    Ref<IDBServer::IDBServer> m_server;
    RefPtr<IDBClient::IDBConnectionToServer> m_connectionToServer;
    RefPtr<IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
