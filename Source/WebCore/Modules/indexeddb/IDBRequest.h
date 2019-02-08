/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "ExceptionOr.h"
#include "IDBActiveDOMObject.h"
#include "IDBError.h"
#include "IDBKeyData.h"
#include "IDBResourceIdentifier.h"
#include "IDBValue.h"
#include "IndexedDB.h"
#include "JSValueInWrappedObject.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/Function.h>
#include <wtf/Scope.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMException;
class Event;
class IDBCursor;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;
class IDBResultData;
class IDBTransaction;
class ThreadSafeDataBuffer;

namespace IDBClient {
class IDBConnectionProxy;
class IDBConnectionToServer;
}

class IDBRequest : public EventTargetWithInlineData, public IDBActiveDOMObject, public RefCounted<IDBRequest>, public CanMakeWeakPtr<IDBRequest> {
public:
    enum class NullResultType {
        Empty,
        Undefined
    };

    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    static Ref<IDBRequest> createObjectStoreGet(ScriptExecutionContext&, IDBObjectStore&, IndexedDB::ObjectStoreRecordType, IDBTransaction&);
    static Ref<IDBRequest> createIndexGet(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    const IDBResourceIdentifier& resourceIdentifier() const { return m_resourceIdentifier; }

    virtual ~IDBRequest();

    using Result = Variant<RefPtr<IDBCursor>, RefPtr<IDBDatabase>, IDBKeyData, Vector<IDBKeyData>, IDBValue, Vector<IDBValue>, uint64_t, NullResultType>;
    ExceptionOr<Result> result() const;
    JSValueInWrappedObject& resultWrapper() { return m_resultWrapper; }

    using Source = Variant<RefPtr<IDBObjectStore>, RefPtr<IDBIndex>, RefPtr<IDBCursor>>;
    const Optional<Source>& source() const { return m_source; }

    ExceptionOr<DOMException*> error() const;

    RefPtr<IDBTransaction> transaction() const;
    
    enum class ReadyState { Pending, Done };
    ReadyState readyState() const { return m_readyState; }

    bool isDone() const { return m_readyState == ReadyState::Done; }

    uint64_t sourceObjectStoreIdentifier() const;
    uint64_t sourceIndexIdentifier() const;
    IndexedDB::ObjectStoreRecordType requestedObjectStoreRecordType() const;
    IndexedDB::IndexRecordType requestedIndexRecordType() const;

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

    void completeRequestAndDispatchEvent(const IDBResultData&);

    void setResult(const IDBKeyData&);
    void setResult(const Vector<IDBKeyData>&);
    void setResult(const Vector<IDBValue>&);
    void setResult(uint64_t);
    void setResultToStructuredClone(const IDBValue&);
    void setResultToUndefined();

    void willIterateCursor(IDBCursor&);
    void didOpenOrIterateCursor(const IDBResultData&);

    const IDBCursor* pendingCursor() const { return m_pendingCursor.get(); }

    void setSource(IDBCursor&);
    void setVersionChangeTransaction(IDBTransaction&);

    IndexedDB::RequestType requestType() const { return m_requestType; }

    bool hasPendingActivity() const final;

protected:
    IDBRequest(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&);

    void enqueueEvent(Ref<Event>&&);
    void dispatchEvent(Event&) override;

    void setResult(Ref<IDBDatabase>&&);

    IDBClient::IDBConnectionProxy& connectionProxy() { return m_connectionProxy.get(); }

    // FIXME: Protected data members aren't great for maintainability.
    // Consider adding protected helper functions and making these private.
    ReadyState m_readyState { ReadyState::Pending };
    RefPtr<IDBTransaction> m_transaction;
    bool m_shouldExposeTransactionToDOM { true };
    RefPtr<DOMException> m_domError;
    IndexedDB::RequestType m_requestType { IndexedDB::RequestType::Other };
    bool m_contextStopped { false };
    Event* m_openDatabaseSuccessEvent { nullptr };

private:
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IndexedDB::ObjectStoreRecordType, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    EventTargetInterface eventTargetInterface() const override;

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;
    virtual void cancelForStop();

    void refEventTarget() final { RefCounted::ref(); }
    void derefEventTarget() final { RefCounted::deref(); }
    void uncaughtExceptionInEventHandler() final;

    virtual bool isOpenDBRequest() const { return false; }

    void onError();
    void onSuccess();

    IDBCursor* resultCursor();

    IDBError m_idbError;
    IDBResourceIdentifier m_resourceIdentifier;

    JSValueInWrappedObject m_resultWrapper;
    Result m_result;
    Optional<Source> m_source;

    bool m_hasPendingActivity { true };
    IndexedDB::ObjectStoreRecordType m_requestedObjectStoreRecordType { IndexedDB::ObjectStoreRecordType::ValueOnly };
    IndexedDB::IndexRecordType m_requestedIndexRecordType { IndexedDB::IndexRecordType::Key };

    RefPtr<IDBCursor> m_pendingCursor;

    Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
