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

#include <WebCore/IDBConnectionToClient.h>
#include <WebCore/IDBConnectionToServer.h>
#include <WebCore/IDBServer.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace PAL {
class SessionID;
}

namespace WebCore {
struct ClientOrigin;
class StorageQuotaManager;

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IDBServer {
class IDBServer;
}
} // namespace WebCore

class InProcessIDBServer final : public WebCore::IDBClient::IDBConnectionToServerDelegate, public WebCore::IDBServer::IDBConnectionToClientDelegate, public ThreadSafeRefCounted<InProcessIDBServer> {
public:

    static Ref<InProcessIDBServer> create(PAL::SessionID);
    static Ref<InProcessIDBServer> create(PAL::SessionID, const String& databaseDirectoryPath);

    virtual ~InProcessIDBServer();

    WebCore::IDBClient::IDBConnectionToServer& connectionToServer() const;
    WebCore::IDBServer::IDBConnectionToClient& connectionToClient() const;
    WebCore::IDBServer::IDBServer& server() { return *m_server; }

    // IDBConnectionToServer
    void deleteDatabase(const WebCore::IDBRequestData&) final;
    void openDatabase(const WebCore::IDBRequestData&) final;
    void abortTransaction(const WebCore::IDBResourceIdentifier&) final;
    void commitTransaction(const WebCore::IDBResourceIdentifier&, uint64_t pendingCountRequest) final;
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&) final;
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&) final;
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName) final;
    void renameObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName) final;
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier) final;
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&) final;
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) final;
    void renameIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName) final;
    void putOrAdd(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, const WebCore::IndexedDB::ObjectStoreOverwriteMode) final;
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&) final;
    void getAllRecords(const WebCore::IDBRequestData&, const WebCore::IDBGetAllRecordsData&) final;
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) final;
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) final;
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&) final;
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBIterateCursorData&) final;
    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&) final;
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier) final;
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) final;
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier) final;
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer) final;
    void openDBRequestCancelled(const WebCore::IDBRequestData&) final;
    void getAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier&, const WebCore::ClientOrigin&) final;

    // IDBConnectionToClient
    WebCore::IDBConnectionIdentifier identifier() const final;
    void didDeleteDatabase(const WebCore::IDBResultData&) final;
    void didOpenDatabase(const WebCore::IDBResultData&) final;
    void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCreateObjectStore(const WebCore::IDBResultData&) final;
    void didDeleteObjectStore(const WebCore::IDBResultData&) final;
    void didRenameObjectStore(const WebCore::IDBResultData&) final;
    void didClearObjectStore(const WebCore::IDBResultData&) final;
    void didCreateIndex(const WebCore::IDBResultData&) final;
    void didDeleteIndex(const WebCore::IDBResultData&) final;
    void didRenameIndex(const WebCore::IDBResultData&) final;
    void didPutOrAdd(const WebCore::IDBResultData&) final;
    void didGetRecord(const WebCore::IDBResultData&) final;
    void didGetAllRecords(const WebCore::IDBResultData&) final;
    void didGetCount(const WebCore::IDBResultData&) final;
    void didDeleteRecord(const WebCore::IDBResultData&) final;
    void didOpenCursor(const WebCore::IDBResultData&) final;
    void didIterateCursor(const WebCore::IDBResultData&) final;
    void fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) final;
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCloseFromServer(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBError&) final;
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) final;
    void didGetAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier&, Vector<WebCore::IDBDatabaseNameAndVersion>&&) final;

    void closeAndDeleteDatabasesModifiedSince(WallTime);

    void dispatchTask(Function<void()>&&);
    void dispatchTaskReply(Function<void()>&&);

    WebCore::StorageQuotaManager* quotaManager(const WebCore::ClientOrigin&);

private:
    InProcessIDBServer(PAL::SessionID, const String& databaseDirectoryPath = nullString());

    Lock m_serverLock;
    std::unique_ptr<WebCore::IDBServer::IDBServer> m_server;
    RefPtr<WebCore::IDBClient::IDBConnectionToServer> m_connectionToServer;
    RefPtr<WebCore::IDBServer::IDBConnectionToClient> m_connectionToClient;
    Ref<WorkQueue> m_queue;

    HashMap<WebCore::ClientOrigin, RefPtr<WebCore::StorageQuotaManager>> m_quotaManagers;
};
