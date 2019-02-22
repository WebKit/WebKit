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
#include "Timer.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseTransaction.h"
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>

namespace JSC {
class ExecState;
class VM;
}

namespace WebCore {

class IDBError;
class IDBGetAllResult;
class IDBRequestData;
class IDBTransactionInfo;

enum class IDBGetRecordDataType;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBServer {

class IDBConnectionToClient;
class IDBServer;

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
    void transactionDestroyed(UniqueIDBDatabaseTransaction&);
    void connectionClosedFromClient(UniqueIDBDatabaseConnection&);
    void confirmConnectionClosedOnServer(UniqueIDBDatabaseConnection&);
    void didFireVersionChangeEvent(UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& requestIdentifier);
    void openDBRequestCancelled(const IDBResourceIdentifier& requestIdentifier);
    void confirmDidCloseFromServer(UniqueIDBDatabaseConnection&);

    void enqueueTransaction(Ref<UniqueIDBDatabaseTransaction>&&);

    void handleDelete(IDBConnectionToClient&, const IDBRequestData&);
    void immediateCloseForUserDelete();

    static JSC::VM& databaseThreadVM();
    static JSC::ExecState& databaseThreadExecState();

    bool hardClosedForUserDelete() const { return m_hardClosedForUserDelete; }

    void setQuota(uint64_t);
private:
    void handleDatabaseOperations();
    void handleCurrentOperation();
    void performCurrentOpenOperation();
    void performCurrentDeleteOperation();
    void addOpenDatabaseConnection(Ref<UniqueIDBDatabaseConnection>&&);
    bool hasAnyOpenConnections() const;
    bool allConnectionsAreClosedOrClosing() const;

    void startVersionChangeTransaction();
    void maybeNotifyConnectionsOfVersionChange();
    void notifyCurrentRequestConnectionClosedOrFiredVersionChangeEvent(uint64_t connectionIdentifier);
    bool isVersionChangeInProgress();

    void activateTransactionInBackingStore(UniqueIDBDatabaseTransaction&);
    void transactionCompleted(RefPtr<UniqueIDBDatabaseTransaction>&&);

    void connectionClosedFromServer(UniqueIDBDatabaseConnection&);

    void scheduleShutdownForClose();

    // Database thread operations
    void deleteBackingStore(const IDBDatabaseIdentifier&);
    void openBackingStore(const IDBDatabaseIdentifier&);
    void performCommitTransaction(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier);
    void performAbortTransaction(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier);
    void beginTransactionInBackingStore(const IDBTransactionInfo&);
    void performCreateObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&);
    void performDeleteObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier);
    void performRenameObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName);
    void performClearObjectStore(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier);
    void performCreateIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&);
    void performDeleteIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier);
    void performRenameIndex(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void performPutOrAdd(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, const IDBValue&, IndexedDB::ObjectStoreOverwriteMode);
    void performGetRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IDBGetRecordDataType);
    void performGetAllRecords(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&);
    void performGetIndexRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&);
    void performGetCount(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&);
    void performDeleteRecord(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&);
    void performOpenCursor(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&);
    void performIterateCursor(uint64_t callbackIdentifier, const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData&);
    void performPrefetchCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier);

    void performActivateTransactionInBackingStore(uint64_t callbackIdentifier, const IDBTransactionInfo&);
    void performUnconditionalDeleteBackingStore();
    void shutdownForClose();

    // Main thread callbacks
    void didDeleteBackingStore(uint64_t deletedVersion);
    void didOpenBackingStore(const IDBDatabaseInfo&, const IDBError&);
    void didPerformCreateObjectStore(uint64_t callbackIdentifier, const IDBError&, const IDBObjectStoreInfo&);
    void didPerformDeleteObjectStore(uint64_t callbackIdentifier, const IDBError&, uint64_t objectStoreIdentifier);
    void didPerformRenameObjectStore(uint64_t callbackIdentifier, const IDBError&, uint64_t objectStoreIdentifier, const String& newName);
    void didPerformClearObjectStore(uint64_t callbackIdentifier, const IDBError&);
    void didPerformCreateIndex(uint64_t callbackIdentifier, const IDBError&, const IDBIndexInfo&);
    void didPerformDeleteIndex(uint64_t callbackIdentifier, const IDBError&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier);
    void didPerformRenameIndex(uint64_t callbackIdentifier, const IDBError&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void didPerformPutOrAdd(uint64_t callbackIdentifier, const IDBError&, const IDBKeyData&);
    void didPerformGetRecord(uint64_t callbackIdentifier, const IDBError&, const IDBGetResult&);
    void didPerformGetAllRecords(uint64_t callbackIdentifier, const IDBError&, const IDBGetAllResult&);
    void didPerformGetCount(uint64_t callbackIdentifier, const IDBError&, uint64_t);
    void didPerformDeleteRecord(uint64_t callbackIdentifier, const IDBError&);
    void didPerformOpenCursor(uint64_t callbackIdentifier, const IDBError&, const IDBGetResult&);
    void didPerformIterateCursor(uint64_t callbackIdentifier, const IDBError&, const IDBGetResult&);
    void didPerformCommitTransaction(uint64_t callbackIdentifier, const IDBError&, const IDBResourceIdentifier& transactionIdentifier);
    void didPerformAbortTransaction(uint64_t callbackIdentifier, const IDBError&, const IDBResourceIdentifier& transactionIdentifier);
    void didPerformActivateTransactionInBackingStore(uint64_t callbackIdentifier, const IDBError&);
    void didPerformUnconditionalDeleteBackingStore();
    void didShutdownForClose();

    uint64_t storeCallbackOrFireError(ErrorCallback&&);
    uint64_t storeCallbackOrFireError(KeyDataCallback&&);
    uint64_t storeCallbackOrFireError(GetAllResultsCallback&&);
    uint64_t storeCallbackOrFireError(GetResultCallback&&);
    uint64_t storeCallbackOrFireError(CountCallback&&);

    void performErrorCallback(uint64_t callbackIdentifier, const IDBError&);
    void performKeyDataCallback(uint64_t callbackIdentifier, const IDBError&, const IDBKeyData&);
    void performGetResultCallback(uint64_t callbackIdentifier, const IDBError&, const IDBGetResult&);
    void performGetAllResultsCallback(uint64_t callbackIdentifier, const IDBError&, const IDBGetAllResult&);
    void performCountCallback(uint64_t callbackIdentifier, const IDBError&, uint64_t);

    void forgetErrorCallback(uint64_t callbackIdentifier);

    bool hasAnyPendingCallbacks() const;
    bool isCurrentlyInUse() const;
    bool hasUnfinishedTransactions() const;

    void invokeOperationAndTransactionTimer();
    void operationAndTransactionTimerFired();
    RefPtr<UniqueIDBDatabaseTransaction> takeNextRunnableTransaction(bool& hadDeferredTransactions);

    bool prepareToFinishTransaction(UniqueIDBDatabaseTransaction&);
    
    void clearStalePendingOpenDBRequests();

    void postDatabaseTask(CrossThreadTask&&);
    void postDatabaseTaskReply(CrossThreadTask&&);
    void executeNextDatabaseTask();
    void executeNextDatabaseTaskReply();

    void maybeFinishHardClose();
    bool isDoneWithHardClose();

    IDBServer& m_server;
    IDBDatabaseIdentifier m_identifier;
    
    ListHashSet<RefPtr<ServerOpenDBRequest>> m_pendingOpenDBRequests;
    RefPtr<ServerOpenDBRequest> m_currentOpenDBRequest;

    ListHashSet<RefPtr<UniqueIDBDatabaseConnection>> m_openDatabaseConnections;
    HashSet<RefPtr<UniqueIDBDatabaseConnection>> m_clientClosePendingDatabaseConnections;
    HashSet<RefPtr<UniqueIDBDatabaseConnection>> m_serverClosePendingDatabaseConnections;

    RefPtr<UniqueIDBDatabaseConnection> m_versionChangeDatabaseConnection;
    RefPtr<UniqueIDBDatabaseTransaction> m_versionChangeTransaction;

    bool m_isOpeningBackingStore { false };
    IDBError m_backingStoreOpenError;
    std::unique_ptr<IDBBackingStore> m_backingStore;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBDatabaseInfo> m_mostRecentDeletedDatabaseInfo;

    bool m_backingStoreSupportsSimultaneousTransactions { false };
    bool m_backingStoreIsEphemeral { false };

    HashMap<uint64_t, ErrorCallback> m_errorCallbacks;
    HashMap<uint64_t, KeyDataCallback> m_keyDataCallbacks;
    HashMap<uint64_t, GetResultCallback> m_getResultCallbacks;
    HashMap<uint64_t, GetAllResultsCallback> m_getAllResultsCallbacks;
    HashMap<uint64_t, CountCallback> m_countCallbacks;
    Deque<uint64_t> m_callbackQueue;

    Timer m_operationAndTransactionTimer;

    Deque<RefPtr<UniqueIDBDatabaseTransaction>> m_pendingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<UniqueIDBDatabaseTransaction>> m_inProgressTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<UniqueIDBDatabaseTransaction>> m_finishingTransactions;

    // The keys into these sets are the object store ID.
    // These sets help to decide which transactions can be started and which must be deferred.
    HashCountedSet<uint64_t> m_objectStoreTransactionCounts;
    HashSet<uint64_t> m_objectStoreWriteTransactions;

    bool m_deleteBackingStoreInProgress { false };

    CrossThreadQueue<CrossThreadTask> m_databaseQueue;
    CrossThreadQueue<CrossThreadTask> m_databaseReplyQueue;

    bool m_hardClosedForUserDelete { false };
    bool m_owningPointerReleaseScheduled { false };
    std::unique_ptr<UniqueIDBDatabase> m_owningPointerForClose;

    HashSet<IDBResourceIdentifier> m_cursorPrefetches;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
