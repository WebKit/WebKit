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

#include "EventTarget.h"
#include "IDBActiveDOMObject.h"
#include "IDBError.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetRecordData.h"
#include "IDBKeyRangeData.h"
#include "IDBOpenDBRequest.h"
#include "IDBTransactionInfo.h"
#include "IDBTransactionMode.h"
#include "IndexedDB.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>

namespace WebCore {

class DOMException;
class DOMStringList;
class IDBCursor;
class IDBCursorInfo;
class IDBDatabase;
class IDBIndex;
class IDBIndexInfo;
class IDBKey;
class IDBKeyData;
class IDBObjectStore;
class IDBObjectStoreInfo;
class IDBResultData;
class SerializedScriptValue;

struct IDBIterateCursorData;
struct IDBKeyRangeData;

namespace IDBClient {
class IDBConnectionProxy;
class TransactionOperation;
}

class IDBTransaction : public ThreadSafeRefCounted<IDBTransaction>, public EventTargetWithInlineData, public IDBActiveDOMObject {
public:
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&);
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest&);

    ~IDBTransaction() final;

    // IDBTransaction IDL
    Ref<DOMStringList> objectStoreNames() const;
    IDBTransactionMode mode() const { return m_info.mode(); }
    IDBDatabase* db();
    DOMException* error() const;
    ExceptionOr<Ref<IDBObjectStore>> objectStore(const String& name);
    ExceptionOr<void> abort();

    EventTargetInterface eventTargetInterface() const final { return IDBTransactionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ThreadSafeRefCounted::ref(); }
    void derefEventTarget() final { ThreadSafeRefCounted::deref(); }
    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) final;

    using ThreadSafeRefCounted<IDBTransaction>::ref;
    using ThreadSafeRefCounted<IDBTransaction>::deref;

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    bool hasPendingActivity() const final;
    void stop() final;

    const IDBTransactionInfo& info() const { return m_info; }
    IDBDatabase& database() { return m_database.get(); }
    const IDBDatabase& database() const { return m_database.get(); }
    IDBDatabaseInfo* originalDatabaseInfo() const { return m_info.originalDatabaseInfo(); }

    void didStart(const IDBError&);
    void didAbort(const IDBError&);
    void didCommit(const IDBError&);

    bool isVersionChange() const { return mode() == IDBTransactionMode::Versionchange; }
    bool isReadOnly() const { return mode() == IDBTransactionMode::Readonly; }
    bool isActive() const;

    Ref<IDBObjectStore> createObjectStore(const IDBObjectStoreInfo&);
    void renameObjectStore(IDBObjectStore&, const String& newName);
    std::unique_ptr<IDBIndex> createIndex(IDBObjectStore&, const IDBIndexInfo&);
    void renameIndex(IDBIndex&, const String& newName);

    Ref<IDBRequest> requestPutOrAdd(JSC::ExecState&, IDBObjectStore&, IDBKey*, SerializedScriptValue&, IndexedDB::ObjectStoreOverwriteMode);
    Ref<IDBRequest> requestGetRecord(JSC::ExecState&, IDBObjectStore&, const IDBGetRecordData&);
    Ref<IDBRequest> requestGetAllObjectStoreRecords(JSC::ExecState&, IDBObjectStore&, const IDBKeyRangeData&, IndexedDB::GetAllType, Optional<uint32_t> count);
    Ref<IDBRequest> requestGetAllIndexRecords(JSC::ExecState&, IDBIndex&, const IDBKeyRangeData&, IndexedDB::GetAllType, Optional<uint32_t> count);
    Ref<IDBRequest> requestDeleteRecord(JSC::ExecState&, IDBObjectStore&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestClearObjectStore(JSC::ExecState&, IDBObjectStore&);
    Ref<IDBRequest> requestCount(JSC::ExecState&, IDBObjectStore&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestCount(JSC::ExecState&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestGetValue(JSC::ExecState&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestGetKey(JSC::ExecState&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestOpenCursor(JSC::ExecState&, IDBObjectStore&, const IDBCursorInfo&);
    Ref<IDBRequest> requestOpenCursor(JSC::ExecState&, IDBIndex&, const IDBCursorInfo&);
    void iterateCursor(IDBCursor&, const IDBIterateCursorData&);

    void deleteObjectStore(const String& objectStoreName);
    void deleteIndex(uint64_t objectStoreIdentifier, const String& indexName);

    void addRequest(IDBRequest&);
    void removeRequest(IDBRequest&);

    void abortDueToFailedRequest(DOMException&);

    void activate();
    void deactivate();

    void operationCompletedOnServer(const IDBResultData&, IDBClient::TransactionOperation&);
    void operationCompletedOnClient(IDBClient::TransactionOperation&);

    void finishedDispatchEventForRequest(IDBRequest&);

    bool isFinishedOrFinishing() const;
    bool isFinished() const { return m_state == IndexedDB::TransactionState::Finished; }

    IDBClient::IDBConnectionProxy& connectionProxy();

    void connectionClosedFromServer(const IDBError&);

    void visitReferencedObjectStores(JSC::SlotVisitor&) const;

private:
    IDBTransaction(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest*);

    void commit();

    void internalAbort();
    void notifyDidAbort(const IDBError&);
    void finishAbortOrCommit();
    void abortInProgressOperations(const IDBError&);

    void scheduleOperation(RefPtr<IDBClient::TransactionOperation>&&);
    void pendingOperationTimerFired();
    void completedOperationTimerFired();

    void fireOnComplete();
    void fireOnAbort();
    void enqueueEvent(Ref<Event>&&);

    Ref<IDBRequest> requestIndexRecord(JSC::ExecState&, IDBIndex&, IndexedDB::IndexRecordType, const IDBKeyRangeData&);

    void commitOnServer(IDBClient::TransactionOperation&);
    void abortOnServerAndCancelRequests(IDBClient::TransactionOperation&);

    void createObjectStoreOnServer(IDBClient::TransactionOperation&, const IDBObjectStoreInfo&);
    void didCreateObjectStoreOnServer(const IDBResultData&);

    void renameObjectStoreOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier, const String& newName);
    void didRenameObjectStoreOnServer(const IDBResultData&);

    void createIndexOnServer(IDBClient::TransactionOperation&, const IDBIndexInfo&);
    void didCreateIndexOnServer(const IDBResultData&);

    void renameIndexOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier, const uint64_t& indexIdentifier, const String& newName);
    void didRenameIndexOnServer(const IDBResultData&);

    void clearObjectStoreOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier);
    void didClearObjectStoreOnServer(IDBRequest&, const IDBResultData&);

    void putOrAddOnServer(IDBClient::TransactionOperation&, RefPtr<IDBKey>, RefPtr<SerializedScriptValue>, const IndexedDB::ObjectStoreOverwriteMode&);
    void didPutOrAddOnServer(IDBRequest&, const IDBResultData&);

    void getRecordOnServer(IDBClient::TransactionOperation&, const IDBGetRecordData&);
    void didGetRecordOnServer(IDBRequest&, const IDBResultData&);

    void getAllRecordsOnServer(IDBClient::TransactionOperation&, const IDBGetAllRecordsData&);
    void didGetAllRecordsOnServer(IDBRequest&, const IDBResultData&);

    void getCountOnServer(IDBClient::TransactionOperation&, const IDBKeyRangeData&);
    void didGetCountOnServer(IDBRequest&, const IDBResultData&);

    void deleteRecordOnServer(IDBClient::TransactionOperation&, const IDBKeyRangeData&);
    void didDeleteRecordOnServer(IDBRequest&, const IDBResultData&);

    void deleteObjectStoreOnServer(IDBClient::TransactionOperation&, const String& objectStoreName);
    void didDeleteObjectStoreOnServer(const IDBResultData&);

    void deleteIndexOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier, const String& indexName);
    void didDeleteIndexOnServer(const IDBResultData&);

    Ref<IDBRequest> doRequestOpenCursor(JSC::ExecState&, Ref<IDBCursor>&&);
    void openCursorOnServer(IDBClient::TransactionOperation&, const IDBCursorInfo&);
    void didOpenCursorOnServer(IDBRequest&, const IDBResultData&);

    void iterateCursorOnServer(IDBClient::TransactionOperation&, const IDBIterateCursorData&);
    void didIterateCursorOnServer(IDBRequest&, const IDBResultData&);

    void transitionedToFinishing(IndexedDB::TransactionState);

    void establishOnServer();

    void completeNoncursorRequest(IDBRequest&, const IDBResultData&);
    void completeCursorRequest(IDBRequest&, const IDBResultData&);

    void schedulePendingOperationTimer();
    void scheduleCompletedOperationTimer();

    Ref<IDBDatabase> m_database;
    IDBTransactionInfo m_info;

    IndexedDB::TransactionState m_state { IndexedDB::TransactionState::Inactive };
    bool m_startedOnServer { false };

    IDBError m_idbError;
    RefPtr<DOMException> m_domError;

    Timer m_pendingOperationTimer;
    Timer m_completedOperationTimer;
    std::unique_ptr<Timer> m_activationTimer;

    RefPtr<IDBOpenDBRequest> m_openDBRequest;

    Deque<RefPtr<IDBClient::TransactionOperation>> m_pendingTransactionOperationQueue;
    Deque<IDBClient::TransactionOperation*> m_transactionOperationsInProgressQueue;
    Deque<std::pair<RefPtr<IDBClient::TransactionOperation>, IDBResultData>> m_completedOnServerQueue;
    Deque<RefPtr<IDBClient::TransactionOperation>> m_abortQueue;

    HashMap<IDBResourceIdentifier, RefPtr<IDBClient::TransactionOperation>> m_transactionOperationMap;

    mutable Lock m_referencedObjectStoreLock;
    HashMap<String, std::unique_ptr<IDBObjectStore>> m_referencedObjectStores;
    HashMap<uint64_t, std::unique_ptr<IDBObjectStore>> m_deletedObjectStores;

    HashSet<RefPtr<IDBRequest>> m_openRequests;
    RefPtr<IDBRequest> m_currentlyCompletingRequest;

    bool m_contextStopped { false };
};

class TransactionActivator {
    WTF_MAKE_NONCOPYABLE(TransactionActivator);
public:
    TransactionActivator(IDBTransaction* transaction)
        : m_transaction(transaction)
    {
        if (m_transaction)
            m_transaction->activate();
    }

    ~TransactionActivator()
    {
        if (m_transaction)
            m_transaction->deactivate();
    }

private:
    IDBTransaction* m_transaction;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
