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

#ifndef LegacyTransaction_h
#define LegacyTransaction_h

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "DOMError.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "IDBDatabaseMetadata.h"
#include "IDBTransaction.h"
#include "IndexedDB.h"
#include "LegacyDatabase.h"
#include "LegacyRequest.h"
#include "ScriptWrappable.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBDatabaseBackend;
class IDBDatabaseError;
class LegacyCursor;
class LegacyObjectStore;
class LegacyOpenDBRequest;
struct IDBObjectStoreMetadata;

class LegacyTransaction final : public IDBTransaction {
public:
    static Ref<LegacyTransaction> create(ScriptExecutionContext*, int64_t, const Vector<String>& objectStoreNames, IndexedDB::TransactionMode, LegacyDatabase*);
    static Ref<LegacyTransaction> create(ScriptExecutionContext*, int64_t, LegacyDatabase*, LegacyOpenDBRequest*, const IDBDatabaseMetadata& previousMetadata);
    virtual ~LegacyTransaction();

    IDBDatabaseBackend* backendDB() const;

    int64_t id() const { return m_id; }
    bool isActive() const { return m_state == Active; }
    bool isFinished() const { return m_state == Finished; }
    bool isReadOnly() const { return m_mode == IndexedDB::TransactionMode::ReadOnly; }
    bool isVersionChange() const { return m_mode == IndexedDB::TransactionMode::VersionChange; }

    virtual const String& mode() const override final;
    virtual IDBDatabase* db() override final;
    virtual RefPtr<DOMError> error() const override final { return m_error; }
    virtual RefPtr<IDBObjectStore> objectStore(const String& name, ExceptionCodeWithMessage&) override final;
    virtual void abort(ExceptionCodeWithMessage&) override final;

    class OpenCursorNotifier {
    public:
        OpenCursorNotifier(PassRefPtr<LegacyTransaction>, LegacyCursor*);
        ~OpenCursorNotifier();
        void cursorFinished();
    private:
        RefPtr<LegacyTransaction> m_transaction;
        LegacyCursor* m_cursor;
    };

    void registerRequest(LegacyRequest*);
    void unregisterRequest(LegacyRequest*);
    void objectStoreCreated(const String&, PassRefPtr<LegacyObjectStore>);
    void objectStoreDeleted(const String&);
    void setActive(bool);
    void setError(PassRefPtr<DOMError>, const String& errorMessage);

    void onAbort(PassRefPtr<IDBDatabaseError>);
    void onComplete();

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override final { return IDBTransactionEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(Event&) override final;

    // ActiveDOMObject
    virtual bool hasPendingActivity() const override final;

private:
    LegacyTransaction(ScriptExecutionContext*, int64_t, const Vector<String>&, IndexedDB::TransactionMode, LegacyDatabase*, LegacyOpenDBRequest*, const IDBDatabaseMetadata&);

    void enqueueEvent(Ref<Event>&&);
    void closeOpenCursors();

    void registerOpenCursor(LegacyCursor*);
    void unregisterOpenCursor(LegacyCursor*);

    // ActiveDOMObject API.
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;
    const char* activeDOMObjectName() const override;

    // EventTarget API.
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    enum State {
        Inactive, // Created or started, but not in an event callback
        Active, // Created or started, in creation scope or an event callback
        Finishing, // In the process of aborting or completing.
        Finished, // No more events will fire and no new requests may be filed.
    };

    int64_t m_id;
    RefPtr<LegacyDatabase> m_database;
    const Vector<String> m_objectStoreNames;
    LegacyOpenDBRequest* m_openDBRequest;
    const IndexedDB::TransactionMode m_mode;
    State m_state;
    bool m_hasPendingActivity;
    bool m_contextStopped;
    RefPtr<DOMError> m_error;
    String m_errorMessage;

    ListHashSet<RefPtr<LegacyRequest>> m_requestList;

    typedef HashMap<String, RefPtr<LegacyObjectStore>> IDBObjectStoreMap;
    IDBObjectStoreMap m_objectStoreMap;

    typedef HashSet<RefPtr<LegacyObjectStore>> IDBObjectStoreSet;
    IDBObjectStoreSet m_deletedObjectStores;

    typedef HashMap<RefPtr<LegacyObjectStore>, IDBObjectStoreMetadata> IDBObjectStoreMetadataMap;
    IDBObjectStoreMetadataMap m_objectStoreCleanupMap;
    IDBDatabaseMetadata m_previousMetadata;

    HashSet<LegacyCursor*> m_openCursors;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // LegacyTransaction_h
