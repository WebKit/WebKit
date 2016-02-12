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

#ifndef IDBTransactionImpl_h
#define IDBTransactionImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseInfo.h"
#include "IDBError.h"
#include "IDBIndexImpl.h"
#include "IDBObjectStoreImpl.h"
#include "IDBTransaction.h"
#include "IDBTransactionInfo.h"
#include "IndexedDB.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>

namespace WebCore {

class IDBCursorInfo;
class IDBIndexInfo;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBResultData;

struct IDBKeyRangeData;

namespace IDBClient {

class IDBCursor;
class IDBDatabase;
class IDBIndex;
class IDBOpenDBRequest;
class TransactionOperation;

class IDBTransaction : public WebCore::IDBTransaction {
public:
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&);
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest&);

    virtual ~IDBTransaction() override final;

    // IDBTransaction IDL
    virtual const String& mode() const override final;
    virtual WebCore::IDBDatabase* db() override final;
    virtual RefPtr<DOMError> error() const override final;
    virtual RefPtr<WebCore::IDBObjectStore> objectStore(const String& name, ExceptionCodeWithMessage&) override final;
    virtual void abort(ExceptionCodeWithMessage&) override final;

    virtual EventTargetInterface eventTargetInterface() const override final { return IDBTransactionEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }
    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(Event&) override final;

    virtual const char* activeDOMObjectName() const override final;
    virtual bool canSuspendForDocumentSuspension() const override final;
    virtual bool hasPendingActivity() const override final;
    virtual void stop() override final;

    const IDBTransactionInfo& info() const { return m_info; }
    IDBDatabase& database() { return m_database.get(); }
    const IDBDatabase& database() const { return m_database.get(); }
    IDBDatabaseInfo* originalDatabaseInfo() const { return m_info.originalDatabaseInfo(); }

    void didStart(const IDBError&);
    void didAbort(const IDBError&);
    void didCommit(const IDBError&);

    bool isVersionChange() const { return m_info.mode() == IndexedDB::TransactionMode::VersionChange; }
    bool isReadOnly() const { return m_info.mode() == IndexedDB::TransactionMode::ReadOnly; }
    bool isActive() const;

    Ref<IDBObjectStore> createObjectStore(const IDBObjectStoreInfo&);
    std::unique_ptr<IDBIndex> createIndex(IDBObjectStore&, const IDBIndexInfo&);

    Ref<IDBRequest> requestPutOrAdd(ScriptExecutionContext&, IDBObjectStore&, IDBKey*, SerializedScriptValue&, IndexedDB::ObjectStoreOverwriteMode);
    Ref<IDBRequest> requestGetRecord(ScriptExecutionContext&, IDBObjectStore&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestDeleteRecord(ScriptExecutionContext&, IDBObjectStore&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestClearObjectStore(ScriptExecutionContext&, IDBObjectStore&);
    Ref<IDBRequest> requestCount(ScriptExecutionContext&, IDBObjectStore&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestCount(ScriptExecutionContext&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestGetValue(ScriptExecutionContext&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestGetKey(ScriptExecutionContext&, IDBIndex&, const IDBKeyRangeData&);
    Ref<IDBRequest> requestOpenCursor(ScriptExecutionContext&, IDBObjectStore&, const IDBCursorInfo&);
    Ref<IDBRequest> requestOpenCursor(ScriptExecutionContext&, IDBIndex&, const IDBCursorInfo&);
    void iterateCursor(IDBCursor&, const IDBKeyData&, unsigned long count);

    void deleteObjectStore(const String& objectStoreName);
    void deleteIndex(uint64_t objectStoreIdentifier, const String& indexName);

    void addRequest(IDBRequest&);
    void removeRequest(IDBRequest&);

    void abortDueToFailedRequest(DOMError&);

    IDBConnectionToServer& serverConnection();

    void activate();
    void deactivate();

    void operationDidComplete(TransactionOperation&);

    bool isFinishedOrFinishing() const;
    bool isFinished() const { return m_state == IndexedDB::TransactionState::Finished; }

private:
    IDBTransaction(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest*);

    void commit();

    void notifyDidAbort(const IDBError&);
    void finishAbortOrCommit();

    void scheduleOperation(RefPtr<TransactionOperation>&&);
    void operationTimerFired();

    void fireOnComplete();
    void fireOnAbort();
    void enqueueEvent(Ref<Event>&&);

    Ref<IDBRequest> requestIndexRecord(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, const IDBKeyRangeData&);

    void commitOnServer(TransactionOperation&);
    void abortOnServerAndCancelRequests(TransactionOperation&);

    void createObjectStoreOnServer(TransactionOperation&, const IDBObjectStoreInfo&);
    void didCreateObjectStoreOnServer(const IDBResultData&);

    void createIndexOnServer(TransactionOperation&, const IDBIndexInfo&);
    void didCreateIndexOnServer(const IDBResultData&);

    void clearObjectStoreOnServer(TransactionOperation&, const uint64_t& objectStoreIdentifier);
    void didClearObjectStoreOnServer(IDBRequest&, const IDBResultData&);

    void putOrAddOnServer(TransactionOperation&, RefPtr<IDBKey>, RefPtr<SerializedScriptValue>, const IndexedDB::ObjectStoreOverwriteMode&);
    void didPutOrAddOnServer(IDBRequest&, const IDBResultData&);

    void getRecordOnServer(TransactionOperation&, const IDBKeyRangeData&);
    void didGetRecordOnServer(IDBRequest&, const IDBResultData&);

    void getCountOnServer(TransactionOperation&, const IDBKeyRangeData&);
    void didGetCountOnServer(IDBRequest&, const IDBResultData&);

    void deleteRecordOnServer(TransactionOperation&, const IDBKeyRangeData&);
    void didDeleteRecordOnServer(IDBRequest&, const IDBResultData&);

    void deleteObjectStoreOnServer(TransactionOperation&, const String& objectStoreName);
    void didDeleteObjectStoreOnServer(const IDBResultData&);

    void deleteIndexOnServer(TransactionOperation&, const uint64_t& objectStoreIdentifier, const String& indexName);
    void didDeleteIndexOnServer(const IDBResultData&);

    Ref<IDBRequest> doRequestOpenCursor(ScriptExecutionContext&, Ref<IDBCursor>&&);
    void openCursorOnServer(TransactionOperation&, const IDBCursorInfo&);
    void didOpenCursorOnServer(IDBRequest&, const IDBResultData&);

    void iterateCursorOnServer(TransactionOperation&, const IDBKeyData&, const unsigned long& count);
    void didIterateCursorOnServer(IDBRequest&, const IDBResultData&);

    void transitionedToFinishing(IndexedDB::TransactionState);

    void establishOnServer();

    void scheduleOperationTimer();

    Ref<IDBDatabase> m_database;
    IDBTransactionInfo m_info;

    IndexedDB::TransactionState m_state { IndexedDB::TransactionState::Inactive };
    bool m_startedOnServer { false };

    IDBError m_idbError;
    RefPtr<DOMError> m_domError;

    Timer m_operationTimer;
    std::unique_ptr<Timer> m_activationTimer;

    RefPtr<IDBOpenDBRequest> m_openDBRequest;

    Deque<RefPtr<TransactionOperation>> m_transactionOperationQueue;
    Deque<RefPtr<TransactionOperation>> m_abortQueue;
    HashMap<IDBResourceIdentifier, RefPtr<TransactionOperation>> m_transactionOperationMap;

    HashMap<String, RefPtr<IDBObjectStore>> m_referencedObjectStores;

    HashSet<RefPtr<IDBRequest>> m_openRequests;

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

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionImpl_h
