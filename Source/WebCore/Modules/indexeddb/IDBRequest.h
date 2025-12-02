/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Strong.h>
#include <WebCore/EventTarget.h>
#include <WebCore/EventTargetInterfaces.h>
#include <WebCore/IDBActiveDOMObject.h>
#include <WebCore/IDBError.h>
#include <WebCore/IDBGetAllResult.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBIndexIdentifier.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBObjectStoreIdentifier.h>
#include <WebCore/IDBResourceIdentifier.h>
#include <WebCore/IDBValue.h>
#include <WebCore/IndexedDB.h>
#include <WebCore/JSValueInWrappedObject.h>
#include <optional>
#include <wtf/Function.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
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
class WebCoreOpaqueRoot;
template<typename> class ExceptionOr;

namespace IDBClient {
class IDBConnectionProxy;
class IDBConnectionToServer;
}

class IDBRequest : public EventTarget, public IDBActiveDOMObject, public ThreadSafeRefCounted<IDBRequest> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(IDBRequest);
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

    USING_CAN_MAKE_WEAKPTR(EventTarget);

    const IDBResourceIdentifier& resourceIdentifier() const { return m_resourceIdentifier; }

    virtual ~IDBRequest();

    using Result = Variant<RefPtr<IDBCursor>, RefPtr<IDBDatabase>, IDBKeyData, Vector<IDBKeyData>, IDBGetResult, IDBGetAllResult, uint64_t, NullResultType>;
    ExceptionOr<Result> result() const;
    JSValueInWrappedObject& resultWrapper() { return m_resultWrapper; }

    using Source = Variant<RefPtr<IDBObjectStore>, RefPtr<IDBIndex>, RefPtr<IDBCursor>>;
    const std::optional<Source>& source() const { return m_source; }

    ExceptionOr<DOMException*> error() const;

    RefPtr<IDBTransaction> transaction() const;
    
    enum class ReadyState { Pending, Done };
    ReadyState readyState() const { return m_readyState; }

    bool isDone() const { return m_readyState == ReadyState::Done; }

    std::optional<IDBObjectStoreIdentifier> sourceObjectStoreIdentifier() const;
    std::optional<IDBIndexIdentifier> sourceIndexIdentifier() const;
    IndexedDB::ObjectStoreRecordType requestedObjectStoreRecordType() const;
    IndexedDB::IndexRecordType requestedIndexRecordType() const;

    ScriptExecutionContext* scriptExecutionContext() const final;
    using IDBActiveDOMObject::protectedScriptExecutionContext;

    // ActiveDOMObject.
    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void completeRequestAndDispatchEvent(const IDBResultData&);

    void setResult(const IDBKeyData&);
    void setResult(const Vector<IDBKeyData>&);
    void setResultToStructuredClone(const IDBGetResult&);
    void setResult(const IDBGetAllResult&);
    void setResult(uint64_t);
    void setResultToUndefined();

    void willIterateCursor(IDBCursor&);
    void didOpenOrIterateCursor(const IDBResultData&);

    IDBCursor* pendingCursor() const { return m_pendingCursor ? m_pendingCursor.get() : nullptr; }

    void setSource(IDBCursor&);
    void setVersionChangeTransaction(IDBTransaction&);

    IndexedDB::RequestType requestType() const { return m_requestType; }

    void setTransactionOperationID(uint64_t transactionOperationID) { m_currentTransactionOperationID = transactionOperationID; }
    bool willAbortTransactionAfterDispatchingEvent() const;
    void transactionTransitionedToFinishing();
    bool isEventBeingDispatched() const { return !!m_eventBeingDispatched; }

protected:
    IDBRequest(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&, IndexedDB::RequestType);

    void enqueueEvent(Ref<Event>&&);
    void dispatchEvent(Event&) override;

    void setResult(Ref<IDBDatabase>&&);
    void setReadyState(ReadyState state) { m_readyState = state; }
    
    void setShouldExposeTransactionToDOM(bool shouldExposeTransactionToDOM) { m_shouldExposeTransactionToDOM = shouldExposeTransactionToDOM; }

    IDBClient::IDBConnectionProxy& connectionProxy() { return m_connectionProxy.get(); }

private:
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IndexedDB::ObjectStoreRecordType, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    EventTargetInterfaceType eventTargetInterface() const override;

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;
    void stop() final;

    virtual void cancelForStop();

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void uncaughtExceptionInEventHandler() final;

    virtual bool isOpenDBRequest() const { return false; }

    void onError();
    void onSuccess();

    void clearWrappers();

protected:
    RefPtr<IDBTransaction> protectedTransaction() const;

    // FIXME: Protected data members aren't great for maintainability.
    // Consider adding protected helper functions and making these private.
    RefPtr<IDBTransaction> m_transaction;
    RefPtr<DOMException> m_domError;
    WeakPtr<Event> m_openDatabaseSuccessEvent;

private:
    IDBCursor* resultCursor();

    IDBError m_idbError;
    IDBResourceIdentifier m_resourceIdentifier;

    JSValueInWrappedObject m_resultWrapper;

    uint64_t m_currentTransactionOperationID { 0 };

    Result m_result;
    std::optional<Source> m_source;

    RefPtr<IDBCursor> m_pendingCursor;
    const Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;

    ReadyState m_readyState { ReadyState::Pending };
    IndexedDB::RequestType m_requestType { IndexedDB::RequestType::Other };
    IndexedDB::ObjectStoreRecordType m_requestedObjectStoreRecordType { IndexedDB::ObjectStoreRecordType::ValueOnly };
    IndexedDB::IndexRecordType m_requestedIndexRecordType { IndexedDB::IndexRecordType::Key };

    bool m_shouldExposeTransactionToDOM { true };
    enum class PendingActivityType : uint8_t { EndingEvent, CursorIteration, None };
    PendingActivityType m_pendingActivity { PendingActivityType::EndingEvent };
    bool m_hasUncaughtException { false };
    RefPtr<Event> m_eventBeingDispatched;
};

WebCoreOpaqueRoot root(IDBRequest*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IDBRequest)
    static bool isType(const WebCore::EventTarget& eventTarget) { return eventTarget.eventTargetInterface() == WebCore::EventTargetInterfaceType::IDBRequest; }
SPECIALIZE_TYPE_TRAITS_END()
