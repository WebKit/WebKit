/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Connection.h"

#include "WebIDBConnectionToClient.h"
#include <WebCore/IDBServer.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/CrossThreadTaskHandler.h>
#include <wtf/RefCounter.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
class StorageQuotaManager;
namespace IDBServer {
class IDBServer;
}
}

namespace WebKit {

class WebIDBServer final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<WebIDBServer> create(PAL::SessionID, const String& directory, WebCore::IDBServer::IDBServer::StorageQuotaManagerSpaceRequester&&);

    void getOrigins(CompletionHandler<void(HashSet<WebCore::SecurityOriginData>&&)>&&);
    void closeAndDeleteDatabasesModifiedSince(WallTime, CompletionHandler<void()>&& callback);
    void closeAndDeleteDatabasesForOrigins(const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&& callback);
    void renameOrigin(const WebCore::SecurityOriginData&, const WebCore::SecurityOriginData&, CompletionHandler<void()>&&);

    enum class SuspensionCondition : bool { Always, IfIdle };
    bool suspend(SuspensionCondition = SuspensionCondition::Always);
    void resume();

    // Message handlers.
    void openDatabase(const WebCore::IDBRequestData&);
    void deleteDatabase(const WebCore::IDBRequestData&);
    void abortTransaction(const WebCore::IDBResourceIdentifier&);
    void commitTransaction(const WebCore::IDBResourceIdentifier&, uint64_t pendingRequestCount);
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&);
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&);
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName);
    void renameObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&);
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void renameIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void putOrAdd(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, WebCore::IndexedDB::ObjectStoreOverwriteMode);
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&);
    void getAllRecords(const WebCore::IDBRequestData&, const WebCore::IDBGetAllRecordsData&);
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&);
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBIterateCursorData&);
    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer);
    void openDBRequestCancelled(const WebCore::IDBRequestData&);
    void getAllDatabaseNamesAndVersions(IPC::Connection&, const WebCore::IDBResourceIdentifier&, const WebCore::ClientOrigin&);

    void addConnection(IPC::Connection&, WebCore::ProcessIdentifier);
    void removeConnection(IPC::Connection&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void close(CompletionHandler<void()>&& = { });

private:
    WebIDBServer(PAL::SessionID, const String& directory, WebCore::IDBServer::IDBServer::StorageQuotaManagerSpaceRequester&&);
    ~WebIDBServer();

    void postTask(WTF::Function<void()>&&);
    void postTaskReply(Function<void()>&&);

    Ref<WorkQueue> m_queue;

    Lock m_serverLock;
    std::unique_ptr<WebCore::IDBServer::IDBServer> m_server WTF_GUARDED_BY_LOCK(m_serverLock);
    bool m_isSuspended { false };

    HashMap<IPC::Connection::UniqueID, std::unique_ptr<WebIDBConnectionToClient>> m_connectionMap;
    WeakHashSet<IPC::Connection> m_connections; // Only used on the main thread.
};

} // namespace WebKit
