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

#include "IDBBackingStore.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBDatabaseInfo.h"
#include "IDBGetResult.h"
#include "ServerOpenDBRequest.h"
#include "UniqueIDBDatabaseTransaction.h"
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>

namespace WebCore {

struct ClientOrigin;
class IDBError;
class IDBGetAllResult;
struct IDBGetRecordData;
class IDBRequestData;
class IDBTransactionInfo;
class StorageQuotaManager;

enum class IDBGetRecordDataType;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBServer {

class IDBConnectionToClient;
class IDBServer;
class UniqueIDBDatabaseConnection;

typedef Function<void(const IDBError&)> ErrorCallback;
typedef Function<void(const IDBError&, const IDBKeyData&)> KeyDataCallback;
typedef Function<void(const IDBError&, const IDBGetResult&)> GetResultCallback;
typedef Function<void(const IDBError&, const IDBGetAllResult&)> GetAllResultsCallback;
typedef Function<void(const IDBError&, uint64_t)> CountCallback;

class UniqueIDBDatabase : public CanMakeWeakPtr<UniqueIDBDatabase> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    UniqueIDBDatabase(IDBServer&, const IDBDatabaseIdentifier&);
    UniqueIDBDatabase(UniqueIDBDatabase&) = delete;
    WEBCORE_EXPORT ~UniqueIDBDatabase();

    void openDatabaseConnection(IDBConnectionToClient&, const IDBRequestData&);

    const IDBDatabaseInfo& info() const;
    IDBServer& server() { return m_server; }
    const IDBDatabaseIdentifier& identifier() const { return m_identifier; }

    void createObjectStore(UniqueIDBDatabaseTransaction&, const IDBObjectStoreInfo&, ErrorCallback);
    void deleteObjectStore(UniqueIDBDatabaseTransaction&, const String& objectStoreName, ErrorCallback);
    void renameObjectStore(UniqueIDBDatabaseTransaction&, uint64_t objectStoreIdentifier, const String& newName, ErrorCallback);
    void clearObjectStore(UniqueIDBDatabaseTransaction&, uint64_t objectStoreIdentifier, ErrorCallback);
    void createIndex(UniqueIDBDatabaseTransaction&, const IDBIndexInfo&, ErrorCallback);
    void deleteIndex(UniqueIDBDatabaseTransaction&, uint64_t objectStoreIdentifier, const String& indexName, ErrorCallback);
    void renameIndex(UniqueIDBDatabaseTransaction&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName, ErrorCallback);
    void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, IndexedDB::ObjectStoreOverwriteMode, KeyDataCallback);
    void getRecord(const IDBRequestData&, const IDBGetRecordData&, GetResultCallback);
    void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&, GetAllResultsCallback);
    void getCount(const IDBRequestData&, const IDBKeyRangeData&, CountCallback);
    void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&, ErrorCallback);
    void openCursor(const IDBRequestData&, const IDBCursorInfo&, GetResultCallback);
    void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&, GetResultCallback);
    void commitTransaction(UniqueIDBDatabaseTransaction&, ErrorCallback);
    void abortTransaction(UniqueIDBDatabaseTransaction&, ErrorCallback);

    void didFinishHandlingVersionChange(UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& transactionIdentifier);
    void connectionClosedFromClient(UniqueIDBDatabaseConnection&);
    void didFireVersionChangeEvent(UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer);
    void openDBRequestCancelled(const IDBResourceIdentifier& requestIdentifier);

    void enqueueTransaction(Ref<UniqueIDBDatabaseTransaction>&&);

    void handleDelete(IDBConnectionToClient&, const IDBRequestData&);
    void immediateCloseForUserDelete();

    void abortActiveTransactions();

private:
    void handleDatabaseOperations();
    void handleCurrentOperation();
    void performCurrentOpenOperation();
    void performCurrentDeleteOperation();
    enum class RequestType { Delete, Any };
    RefPtr<ServerOpenDBRequest> takeNextRunnableRequest(RequestType = RequestType::Any);
    void addOpenDatabaseConnection(Ref<UniqueIDBDatabaseConnection>&&);
    bool hasAnyOpenConnections() const;
    bool allConnectionsAreClosedOrClosing() const;

    void startVersionChangeTransaction();
    void maybeNotifyConnectionsOfVersionChange();
    void notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(uint64_t connectionIdentifier);

    void handleTransactions();
    RefPtr<UniqueIDBDatabaseTransaction> takeNextRunnableTransaction(bool& hadDeferredTransactions);

    void activateTransactionInBackingStore(UniqueIDBDatabaseTransaction&);
    void transactionCompleted(RefPtr<UniqueIDBDatabaseTransaction>&&);

    void connectionClosedFromServer(UniqueIDBDatabaseConnection&);
    void deleteBackingStore();
    void didDeleteBackingStore(uint64_t deletedVersion);
    void close();

    bool isCurrentlyInUse() const;
    void clearStalePendingOpenDBRequests();

    void clearTransactionsOnConnection(UniqueIDBDatabaseConnection&);

    IDBServer& m_server;
    IDBDatabaseIdentifier m_identifier;

    ListHashSet<RefPtr<ServerOpenDBRequest>> m_pendingOpenDBRequests;
    RefPtr<ServerOpenDBRequest> m_currentOpenDBRequest;

    ListHashSet<RefPtr<UniqueIDBDatabaseConnection>> m_openDatabaseConnections;

    RefPtr<UniqueIDBDatabaseConnection> m_versionChangeDatabaseConnection;
    RefPtr<UniqueIDBDatabaseTransaction> m_versionChangeTransaction;

    std::unique_ptr<IDBBackingStore> m_backingStore;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBDatabaseInfo> m_mostRecentDeletedDatabaseInfo;

    Deque<RefPtr<UniqueIDBDatabaseTransaction>> m_pendingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<UniqueIDBDatabaseTransaction>> m_inProgressTransactions;

    // The keys into these sets are the object store ID.
    // These sets help to decide which transactions can be started and which must be deferred.
    HashCountedSet<uint64_t> m_objectStoreTransactionCounts;
    HashSet<uint64_t> m_objectStoreWriteTransactions;

    HashSet<IDBResourceIdentifier> m_cursorPrefetches;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
