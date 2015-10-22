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
#include "IDBObjectStoreImpl.h"
#include "IDBTransaction.h"
#include "IDBTransactionInfo.h"
#include "IndexedDB.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>

namespace WebCore {

class IDBObjectStoreInfo;
class IDBResultData;

namespace IDBClient {

class IDBDatabase;
class TransactionOperation;

class IDBTransaction : public WebCore::IDBTransaction {
public:
    static Ref<IDBTransaction> create(IDBDatabase&, const IDBTransactionInfo&);

    virtual ~IDBTransaction() override final;

    // IDBTransaction IDL
    virtual const String& mode() const override final;
    virtual WebCore::IDBDatabase* db() override final;
    virtual RefPtr<DOMError> error() const override final;
    virtual RefPtr<WebCore::IDBObjectStore> objectStore(const String& name, ExceptionCode&) override final;
    virtual void abort(ExceptionCode&) override final;

    virtual EventTargetInterface eventTargetInterface() const override final { return IDBTransactionEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }
    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) override final;

    virtual const char* activeDOMObjectName() const override final;
    virtual bool canSuspendForPageCache() const override final;
    virtual bool hasPendingActivity() const override final;

    const IDBTransactionInfo info() const { return m_info; }
    IDBDatabase& database() { return m_database.get(); }
    const IDBDatabase& database() const { return m_database.get(); }
    IDBDatabaseInfo* originalDatabaseInfo() const { return m_originalDatabaseInfo.get(); }

    void didAbort(const IDBError&);
    void didCommit(const IDBError&);

    bool isVersionChange() const { return m_info.mode() == IndexedDB::TransactionMode::VersionChange; }
    bool isActive() const;

    Ref<IDBObjectStore> createObjectStore(const IDBObjectStoreInfo&);

    IDBConnectionToServer& serverConnection();

private:
    IDBTransaction(IDBDatabase&, const IDBTransactionInfo&);

    bool isFinishedOrFinishing() const;

    void commit();

    void finishAbortOrCommit();

    void scheduleOperation(RefPtr<TransactionOperation>&&);
    void scheduleOperationTimer();
    void operationTimerFired();
    void activationTimerFired();

    void fireOnComplete();
    void fireOnAbort();
    void enqueueEvent(Ref<Event>);

    void createObjectStoreOnServer(TransactionOperation&, const IDBObjectStoreInfo&);
    void didCreateObjectStoreOnServer(const IDBResultData&);

    Ref<IDBDatabase> m_database;
    IDBTransactionInfo m_info;
    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfo;

    IndexedDB::TransactionState m_state { IndexedDB::TransactionState::Unstarted };
    IDBError m_idbError;

    Timer m_operationTimer;
    std::unique_ptr<Timer> m_activationTimer;

    Deque<RefPtr<TransactionOperation>> m_transactionOperationQueue;
    HashMap<IDBResourceIdentifier, RefPtr<TransactionOperation>> m_transactionOperationMap;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionImpl_h
