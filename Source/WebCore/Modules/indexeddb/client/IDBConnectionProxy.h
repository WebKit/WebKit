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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBResourceIdentifier.h"
#include "TransactionOperation.h"
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBDatabase;
class IDBDatabaseIdentifier;
class IDBError;
class IDBOpenDBRequest;
class IDBResultData;
class IDBTransaction;
class ScriptExecutionContext;
class SecurityOrigin;

struct IDBGetRecordData;
struct IDBIterateCursorData;

namespace IDBClient {

class IDBConnectionToServer;

class IDBConnectionProxy {
    WTF_MAKE_NONCOPYABLE(IDBConnectionProxy);
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBConnectionProxy(IDBConnectionToServer&);

    Ref<IDBOpenDBRequest> openDatabase(ScriptExecutionContext&, const IDBDatabaseIdentifier&, uint64_t version);
    void didOpenDatabase(const IDBResultData&);

    Ref<IDBOpenDBRequest> deleteDatabase(ScriptExecutionContext&, const IDBDatabaseIdentifier&);
    void didDeleteDatabase(const IDBResultData&);

    void createObjectStore(TransactionOperation&, const IDBObjectStoreInfo&);
    void deleteObjectStore(TransactionOperation&, const String& objectStoreName);
    void clearObjectStore(TransactionOperation&, uint64_t objectStoreIdentifier);
    void createIndex(TransactionOperation&, const IDBIndexInfo&);
    void deleteIndex(TransactionOperation&, uint64_t objectStoreIdentifier, const String& indexName);
    void putOrAdd(TransactionOperation&, IDBKeyData&&, const IDBValue&, const IndexedDB::ObjectStoreOverwriteMode);
    void getRecord(TransactionOperation&, const IDBGetRecordData&);
    void getAllRecords(TransactionOperation&, const IDBGetAllRecordsData&);
    void getCount(TransactionOperation&, const IDBKeyRangeData&);
    void deleteRecord(TransactionOperation&, const IDBKeyRangeData&);
    void openCursor(TransactionOperation&, const IDBCursorInfo&);
    void iterateCursor(TransactionOperation&, const IDBIterateCursorData&);
    void renameObjectStore(TransactionOperation&, uint64_t objectStoreIdentifier, const String& newName);
    void renameIndex(TransactionOperation&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);

    void fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier);

    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion);
    void openDBRequestCancelled(const IDBRequestData&);

    void establishTransaction(IDBTransaction&);
    void commitTransaction(IDBTransaction&);
    void abortTransaction(IDBTransaction&);

    void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);
    void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);

    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, IDBTransaction&);
    void databaseConnectionPendingClose(IDBDatabase&);
    void databaseConnectionClosed(IDBDatabase&);

    void didCloseFromServer(uint64_t databaseConnectionIdentifier, const IDBError&);
    void confirmDidCloseFromServer(IDBDatabase&);

    void connectionToServerLost(const IDBError&);

    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier);

    void completeOperation(const IDBResultData&);

    uint64_t serverConnectionIdentifier() const { return m_serverConnectionIdentifier; }

    void ref();
    void deref();

    void getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, WTF::Function<void (const Vector<String>&)>&&);

    void registerDatabaseConnection(IDBDatabase&);
    void unregisterDatabaseConnection(IDBDatabase&);

    void forgetActiveOperations(const Vector<RefPtr<TransactionOperation>>&);
    void forgetTransaction(IDBTransaction&);
    void forgetActivityForCurrentThread();

private:
    void completeOpenDBRequest(const IDBResultData&);
    bool hasRecordOfTransaction(const IDBTransaction&) const;

    void saveOperation(TransactionOperation&);

    template<typename... Parameters, typename... Arguments>
    void callConnectionOnMainThread(void (IDBConnectionToServer::*method)(Parameters...), Arguments&&... arguments)
    {
        if (isMainThread())
            (m_connectionToServer.*method)(std::forward<Arguments>(arguments)...);
        else
            postMainThreadTask(m_connectionToServer, method, arguments...);
    }

    template<typename... Arguments>
    void postMainThreadTask(Arguments&&... arguments)
    {
        auto task = createCrossThreadTask(arguments...);
        m_mainThreadQueue.append(WTFMove(task));

        scheduleMainThreadTasks();
    }

    void scheduleMainThreadTasks();
    void handleMainThreadTasks();

    IDBConnectionToServer& m_connectionToServer;
    uint64_t m_serverConnectionIdentifier;

    HashMap<uint64_t, IDBDatabase*> m_databaseConnectionMap;
    Lock m_databaseConnectionMapLock;

    HashMap<IDBResourceIdentifier, RefPtr<IDBOpenDBRequest>> m_openDBRequestMap;
    Lock m_openDBRequestMapLock;

    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_pendingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_abortingTransactions;
    Lock m_transactionMapLock;

    HashMap<IDBResourceIdentifier, RefPtr<TransactionOperation>> m_activeOperations;
    Lock m_transactionOperationLock;

    CrossThreadQueue<CrossThreadTask> m_mainThreadQueue;
    Lock m_mainThreadTaskLock;
    RefPtr<IDBConnectionToServer> m_mainThreadProtector;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
