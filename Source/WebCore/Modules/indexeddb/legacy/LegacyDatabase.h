/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LegacyDatabase_h
#define LegacyDatabase_h

#include "ActiveDOMObject.h"
#include "DOMStringList.h"
#include "Dictionary.h"
#include "Event.h"
#include "EventTarget.h"
#include "IDBDatabase.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseMetadata.h"
#include "IDBObjectStore.h"
#include "IndexedDB.h"
#include "LegacyTransaction.h"
#include "ScriptWrappable.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class LegacyTransaction;
class ScriptExecutionContext;

typedef int ExceptionCode;

class LegacyDatabase final : public IDBDatabase {
public:
    static Ref<LegacyDatabase> create(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackend>, PassRefPtr<IDBDatabaseCallbacks>);
    ~LegacyDatabase();

    void setMetadata(const IDBDatabaseMetadata& metadata) { m_metadata = metadata; }
    void transactionCreated(LegacyTransaction*);
    void transactionFinished(LegacyTransaction*);

    // Implement the IDL
    virtual const String name() const override final { return m_metadata.name; }
    virtual uint64_t version() const override final;
    virtual RefPtr<DOMStringList> objectStoreNames() const override final;

    virtual RefPtr<IDBObjectStore> createObjectStore(const String& name, const Dictionary&, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBObjectStore> createObjectStore(const String& name, const IDBKeyPath&, bool autoIncrement, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const Vector<String>&, const String& mode, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const String&, const String& mode, ExceptionCodeWithMessage&) override final;
    virtual void deleteObjectStore(const String& name, ExceptionCodeWithMessage&) override final;
    virtual void close() override final;

    // IDBDatabaseCallbacks
    virtual void onVersionChange(uint64_t oldVersion, uint64_t newVersion);
    virtual void onAbort(int64_t, PassRefPtr<IDBDatabaseError>);
    virtual void onComplete(int64_t);

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override final { return IDBDatabaseEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }

    bool isClosePending() const { return m_closePending; }
    void forceClose();
    const IDBDatabaseMetadata metadata() const { return m_metadata; }
    void enqueueEvent(Ref<Event>&&);

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(Event&) override;

    int64_t findObjectStoreId(const String& name) const;
    bool containsObjectStore(const String& name) const
    {
        return findObjectStoreId(name) != IDBObjectStoreMetadata::InvalidId;
    }

    IDBDatabaseBackend* backend() const { return m_backend.get(); }

    static int64_t nextTransactionId();

    // ActiveDOMObject API.
    bool hasPendingActivity() const override;

private:
    LegacyDatabase(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackend>, PassRefPtr<IDBDatabaseCallbacks>);

    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext* context, PassRefPtr<DOMStringList> scope, const String& mode, ExceptionCodeWithMessage& ec) { return transaction(context, *scope, mode, ec); }

    // ActiveDOMObject API.
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    // EventTarget
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    void closeConnection();

    IDBDatabaseMetadata m_metadata;
    RefPtr<IDBDatabaseBackend> m_backend;
    RefPtr<LegacyTransaction> m_versionChangeTransaction;
    typedef HashMap<int64_t, LegacyTransaction*> TransactionMap;
    TransactionMap m_transactions;

    bool m_closePending;
    bool m_isClosed;
    bool m_contextStopped;

    // Keep track of the versionchange events waiting to be fired on this
    // database so that we can cancel them if the database closes.
    Vector<Ref<Event>> m_enqueuedEvents;

    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

} // namespace WebCore

#endif

#endif // LegacyDatabase_h
