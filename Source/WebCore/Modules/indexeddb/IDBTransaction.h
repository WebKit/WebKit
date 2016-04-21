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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "IDBError.h"
#include "IDBOpenDBRequest.h"
#include "IDBTransactionInfo.h"
#include "IndexedDB.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>

namespace WebCore {

class DOMError;
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

struct IDBKeyRangeData;

namespace IDBClient {
class TransactionOperation;
}

class IDBTransaction : public RefCounted<IDBTransaction>, public EventTargetWithInlineData, private ActiveDOMObject {
public:
    static const AtomicString& modeReadOnly();
    static const AtomicString& modeReadWrite();
    static const AtomicString& modeVersionChange();
    static const AtomicString& modeReadOnlyLegacy();
    static const AtomicString& modeReadWriteLegacy();

    static IndexedDB::TransactionMode stringToMode(const String&, ExceptionCode&);
    static const AtomicString& modeToString(IndexedDB::TransactionMode);

    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&);
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest&);

    ~IDBTransaction() final;

    // IDBTransaction IDL
    const String& mode() const;
    IDBDatabase* db();
    RefPtr<DOMError> error() const;
    RefPtr<IDBObjectStore> objectStore(const String& name, ExceptionCodeWithMessage&);
    void abort(ExceptionCodeWithMessage&);

    EventTargetInterface eventTargetInterface() const final { return IDBTransactionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { RefCounted::ref(); }
    void derefEventTarget() final { RefCounted::deref(); }
    using EventTarget::dispatchEvent;
    bool dispatchEvent(Event&) final;

    using RefCounted<IDBTransaction>::ref;
    using RefCounted<IDBTransaction>::deref;

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

    IDBClient::IDBConnectionToServer& serverConnection();

    void activate();
    void deactivate();

    void operationDidComplete(IDBClient::TransactionOperation&);

    bool isFinishedOrFinishing() const;
    bool isFinished() const { return m_state == IndexedDB::TransactionState::Finished; }

private:
    IDBTransaction(IDBDatabase&, const IDBTransactionInfo&, IDBOpenDBRequest*);

    void commit();

    void notifyDidAbort(const IDBError&);
    void finishAbortOrCommit();

    void scheduleOperation(RefPtr<IDBClient::TransactionOperation>&&);
    void operationTimerFired();

    void fireOnComplete();
    void fireOnAbort();
    void enqueueEvent(Ref<Event>&&);

    Ref<IDBRequest> requestIndexRecord(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, const IDBKeyRangeData&);

    void commitOnServer(IDBClient::TransactionOperation&);
    void abortOnServerAndCancelRequests(IDBClient::TransactionOperation&);

    void createObjectStoreOnServer(IDBClient::TransactionOperation&, const IDBObjectStoreInfo&);
    void didCreateObjectStoreOnServer(const IDBResultData&);

    void createIndexOnServer(IDBClient::TransactionOperation&, const IDBIndexInfo&);
    void didCreateIndexOnServer(const IDBResultData&);

    void clearObjectStoreOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier);
    void didClearObjectStoreOnServer(IDBRequest&, const IDBResultData&);

    void putOrAddOnServer(IDBClient::TransactionOperation&, RefPtr<IDBKey>, RefPtr<SerializedScriptValue>, const IndexedDB::ObjectStoreOverwriteMode&);
    void didPutOrAddOnServer(IDBRequest&, const IDBResultData&);

    void getRecordOnServer(IDBClient::TransactionOperation&, const IDBKeyRangeData&);
    void didGetRecordOnServer(IDBRequest&, const IDBResultData&);

    void getCountOnServer(IDBClient::TransactionOperation&, const IDBKeyRangeData&);
    void didGetCountOnServer(IDBRequest&, const IDBResultData&);

    void deleteRecordOnServer(IDBClient::TransactionOperation&, const IDBKeyRangeData&);
    void didDeleteRecordOnServer(IDBRequest&, const IDBResultData&);

    void deleteObjectStoreOnServer(IDBClient::TransactionOperation&, const String& objectStoreName);
    void didDeleteObjectStoreOnServer(const IDBResultData&);

    void deleteIndexOnServer(IDBClient::TransactionOperation&, const uint64_t& objectStoreIdentifier, const String& indexName);
    void didDeleteIndexOnServer(const IDBResultData&);

    Ref<IDBRequest> doRequestOpenCursor(ScriptExecutionContext&, Ref<IDBCursor>&&);
    void openCursorOnServer(IDBClient::TransactionOperation&, const IDBCursorInfo&);
    void didOpenCursorOnServer(IDBRequest&, const IDBResultData&);

    void iterateCursorOnServer(IDBClient::TransactionOperation&, const IDBKeyData&, const unsigned long& count);
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

    Deque<RefPtr<IDBClient::TransactionOperation>> m_transactionOperationQueue;
    Deque<RefPtr<IDBClient::TransactionOperation>> m_abortQueue;
    HashMap<IDBResourceIdentifier, RefPtr<IDBClient::TransactionOperation>> m_transactionOperationMap;

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

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
