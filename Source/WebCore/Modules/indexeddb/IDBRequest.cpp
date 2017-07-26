/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "Event.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "IDBKeyData.h"
#include "IDBObjectStore.h"
#include "IDBResultData.h"
#include "JSDOMConvertIndexedDB.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMConvertSequences.h"
#include "Logging.h"
#include "ScopeGuard.h"
#include "ScriptExecutionContext.h"
#include "ThreadSafeDataBuffer.h"
#include <heap/StrongInlines.h>
#include <wtf/Variant.h>

using namespace JSC;

namespace WebCore {

Ref<IDBRequest> IDBRequest::create(ScriptExecutionContext& context, IDBObjectStore& objectStore, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, objectStore, transaction));
}

Ref<IDBRequest> IDBRequest::create(ScriptExecutionContext& context, IDBCursor& cursor, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, cursor, transaction));
}

Ref<IDBRequest> IDBRequest::create(ScriptExecutionContext& context, IDBIndex& index, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, index, transaction));
}

Ref<IDBRequest> IDBRequest::createObjectStoreGet(ScriptExecutionContext& context, IDBObjectStore& objectStore, IndexedDB::ObjectStoreRecordType type, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, objectStore, type, transaction));
}

Ref<IDBRequest> IDBRequest::createIndexGet(ScriptExecutionContext& context, IDBIndex& index, IndexedDB::IndexRecordType requestedRecordType, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, index, requestedRecordType, transaction));
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy)
    : IDBActiveDOMObject(&context)
    , m_resourceIdentifier(connectionProxy)
    , m_connectionProxy(connectionProxy)
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBObjectStore& objectStore, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_source(&objectStore)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBCursor& cursor, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_pendingCursor(&cursor)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();

    WTF::switchOn(cursor.source(),
        [this] (const auto& value) { this->m_source = IDBRequest::Source { value }; }
    );

    cursor.setRequest(*this);
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBIndex& index, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_source(&index)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBObjectStore& objectStore, IndexedDB::ObjectStoreRecordType type, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_source(&objectStore)
    , m_requestedObjectStoreRecordType(type)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBIndex& index, IndexedDB::IndexRecordType requestedRecordType, IDBTransaction& transaction)
    : IDBRequest(context, index, transaction)
{
    m_requestedIndexRecordType = requestedRecordType;
}

IDBRequest::~IDBRequest()
{
    ASSERT(currentThread() == originThreadID());

    if (m_result) {
        WTF::switchOn(m_result.value(),
            [] (RefPtr<IDBCursor>& cursor) { cursor->clearRequest(); },
            [] (const auto&) { }
        );
    }
}

ExceptionOr<std::optional<IDBRequest::Result>> IDBRequest::result() const
{
    if (!isDone())
        return Exception { InvalidStateError, ASCIILiteral("Failed to read the 'result' property from 'IDBRequest': The request has not finished.") };

    return std::optional<IDBRequest::Result> { m_result };
}

ExceptionOr<DOMError*> IDBRequest::error() const
{
    ASSERT(currentThread() == originThreadID());

    if (!isDone())
        return Exception { InvalidStateError, ASCIILiteral("Failed to read the 'error' property from 'IDBRequest': The request has not finished.") };

    return m_domError.get();
}

void IDBRequest::setSource(IDBCursor& cursor)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_cursorRequestNotifier);

    m_source = Source { &cursor };
    m_cursorRequestNotifier = std::make_unique<ScopeGuard>([this]() {
        ASSERT(WTF::holds_alternative<RefPtr<IDBCursor>>(m_source.value()));
        WTF::get<RefPtr<IDBCursor>>(m_source.value())->decrementOutstandingRequestCount();
    });
}

void IDBRequest::setVersionChangeTransaction(IDBTransaction& transaction)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_transaction);
    ASSERT(transaction.isVersionChange());
    ASSERT(!transaction.isFinishedOrFinishing());

    m_transaction = &transaction;
}

RefPtr<WebCore::IDBTransaction> IDBRequest::transaction() const
{
    ASSERT(currentThread() == originThreadID());
    return m_shouldExposeTransactionToDOM ? m_transaction : nullptr;
}

uint64_t IDBRequest::sourceObjectStoreIdentifier() const
{
    ASSERT(currentThread() == originThreadID());

    if (!m_source)
        return 0;

    return WTF::switchOn(m_source.value(),
        [] (const RefPtr<IDBObjectStore>& objectStore) { return objectStore->info().identifier(); },
        [] (const RefPtr<IDBIndex>& index) { return index->info().objectStoreIdentifier(); },
        [] (const RefPtr<IDBCursor>&) { return 0; }
    );
}

uint64_t IDBRequest::sourceIndexIdentifier() const
{
    ASSERT(currentThread() == originThreadID());

    if (!m_source)
        return 0;

    return WTF::switchOn(m_source.value(),
        [] (const RefPtr<IDBObjectStore>&) -> uint64_t { return 0; },
        [] (const RefPtr<IDBIndex>& index) -> uint64_t { return index->info().identifier(); },
        [] (const RefPtr<IDBCursor>&) -> uint64_t { return 0; }
    );
}

IndexedDB::ObjectStoreRecordType IDBRequest::requestedObjectStoreRecordType() const
{
    ASSERT(currentThread() == originThreadID());

    return m_requestedObjectStoreRecordType;
}

IndexedDB::IndexRecordType IDBRequest::requestedIndexRecordType() const
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(m_source);
    ASSERT(WTF::holds_alternative<RefPtr<IDBIndex>>(m_source.value()));

    return m_requestedIndexRecordType;
}

EventTargetInterface IDBRequest::eventTargetInterface() const
{
    ASSERT(currentThread() == originThreadID());

    return IDBRequestEventTargetInterfaceType;
}

const char* IDBRequest::activeDOMObjectName() const
{
    ASSERT(currentThread() == originThreadID());

    return "IDBRequest";
}

bool IDBRequest::canSuspendForDocumentSuspension() const
{
    ASSERT(currentThread() == originThreadID());
    return false;
}

bool IDBRequest::hasPendingActivity() const
{
    ASSERT(currentThread() == originThreadID() || mayBeGCThread());
    return m_hasPendingActivity;
}

void IDBRequest::stop()
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_contextStopped);

    cancelForStop();

    removeAllEventListeners();

    m_contextStopped = true;
}

void IDBRequest::cancelForStop()
{
    // The base IDBRequest class has nothing additional to do here.
}

void IDBRequest::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(currentThread() == originThreadID());
    if (!scriptExecutionContext() || m_contextStopped)
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

bool IDBRequest::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBRequest::dispatchEvent - %s (%p)", event.type().string().utf8().data(), this);

    ASSERT(currentThread() == originThreadID());
    ASSERT(m_hasPendingActivity);
    ASSERT(!m_contextStopped);

    if (event.type() != eventNames().blockedEvent)
        m_readyState = ReadyState::Done;

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);

    if (&event == m_openDatabaseSuccessEvent)
        m_openDatabaseSuccessEvent = nullptr;
    else if (m_transaction && !m_transaction->isFinished()) {
        targets.append(m_transaction);
        targets.append(m_transaction->db());
    }

    m_hasPendingActivity = false;

    m_cursorRequestNotifier = nullptr;

    bool dontPreventDefault;
    {
        TransactionActivator activator(m_transaction.get());
        dontPreventDefault = IDBEventDispatcher::dispatch(event, targets);
    }

    // IDBEventDispatcher::dispatch() might have set the pending activity flag back to true, suggesting the request will be reused.
    // We might also re-use the request if this event was the upgradeneeded event for an IDBOpenDBRequest.
    if (!m_hasPendingActivity)
        m_hasPendingActivity = isOpenDBRequest() && (event.type() == eventNames().upgradeneededEvent || event.type() == eventNames().blockedEvent);

    // The request should only remain in the transaction's request list if it represents a pending cursor operation, or this is an open request that was blocked.
    if (m_transaction && !m_pendingCursor && event.type() != eventNames().blockedEvent)
        m_transaction->removeRequest(*this);

    if (dontPreventDefault && event.type() == eventNames().errorEvent && m_transaction && !m_transaction->isFinishedOrFinishing()) {
        ASSERT(m_domError);
        m_transaction->abortDueToFailedRequest(*m_domError);
    }

    if (m_transaction)
        m_transaction->finishedDispatchEventForRequest(*this);

    return dontPreventDefault;
}

void IDBRequest::uncaughtExceptionInEventHandler()
{
    LOG(IndexedDB, "IDBRequest::uncaughtExceptionInEventHandler");

    ASSERT(currentThread() == originThreadID());

    if (m_transaction && m_idbError.code() != AbortError)
        m_transaction->abortDueToFailedRequest(DOMError::create("AbortError", ASCIILiteral("IDBTransaction will abort due to uncaught exception in an event handler")));
}

void IDBRequest::setResult(const IDBKeyData& keyData)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* state = context->execState();
    if (!state)
        return;

    // FIXME: This conversion should be done lazily, when script needs the JSValues, so that global object
    // of the IDBRequest wrapper can be used, rather than the lexicalGlobalObject.
    VM& vm = context->vm();
    JSLockHolder lock(vm);
    m_result = Result { JSC::Strong<JSC::Unknown> { vm, toJS<IDLIDBKeyData>(*state, *jsCast<JSDOMGlobalObject*>(state->lexicalGlobalObject()), keyData) } };
}

void IDBRequest::setResult(const Vector<IDBKeyData>& keyDatas)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* state = context->execState();
    if (!state)
        return;

    // FIXME: This conversion should be done lazily, when script needs the JSValues, so that global object
    // of the IDBRequest wrapper can be used, rather than the lexicalGlobalObject.
    VM& vm = context->vm();
    JSLockHolder lock(vm);
    m_result = Result { JSC::Strong<JSC::Unknown> { vm, toJS<IDLSequence<IDLIDBKeyData>>(*state, *jsCast<JSDOMGlobalObject*>(state->lexicalGlobalObject()), keyDatas) } };
}

void IDBRequest::setResult(const Vector<IDBValue>& values)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* state = context->execState();
    if (!state)
        return;

    // FIXME: This conversion should be done lazily, when script needs the JSValues, so that global object
    // of the IDBRequest wrapper can be used, rather than the lexicalGlobalObject.
    VM& vm = context->vm();
    JSLockHolder lock(vm);
    m_result = Result { JSC::Strong<JSC::Unknown> { vm, toJS<IDLSequence<IDLIDBValue>>(*state, *jsCast<JSDOMGlobalObject*>(state->lexicalGlobalObject()), values) } };
}

void IDBRequest::setResult(uint64_t number)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    m_result = Result { JSC::Strong<JSC::Unknown> { context->vm(), toJS<IDLUnrestrictedDouble>(number) } };
}

void IDBRequest::setResultToStructuredClone(const IDBValue& value)
{
    ASSERT(currentThread() == originThreadID());

    LOG(IndexedDB, "IDBRequest::setResultToStructuredClone");

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* state = context->execState();
    if (!state)
        return;

    // FIXME: This conversion should be done lazily, when script needs the JSValues, so that global object
    // of the IDBRequest wrapper can be used, rather than the lexicalGlobalObject.
    VM& vm = context->vm();
    JSLockHolder lock(vm);
    m_result = Result { JSC::Strong<JSC::Unknown> { vm, toJS<IDLIDBValue>(*state, *jsCast<JSDOMGlobalObject*>(state->lexicalGlobalObject()), value) } };
}

void IDBRequest::setResultToUndefined()
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    m_result = Result { JSC::Strong<JSC::Unknown> { context->vm(), JSC::jsUndefined() } };
}

IDBCursor* IDBRequest::resultCursor()
{
    ASSERT(currentThread() == originThreadID());

    if (!m_result)
        return nullptr;

    return WTF::switchOn(m_result.value(),
        [] (const RefPtr<IDBCursor>& cursor) -> IDBCursor* { return cursor.get(); },
        [] (const auto&) -> IDBCursor* { return nullptr; }
    );
}

void IDBRequest::willIterateCursor(IDBCursor& cursor)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(isDone());
    ASSERT(scriptExecutionContext());
    ASSERT(m_transaction);
    ASSERT(!m_pendingCursor);
    ASSERT(&cursor == resultCursor());
    ASSERT(!m_cursorRequestNotifier);

    m_pendingCursor = &cursor;
    m_hasPendingActivity = true;
    m_result = std::nullopt;
    m_readyState = ReadyState::Pending;
    m_domError = nullptr;
    m_idbError = IDBError { };

    m_cursorRequestNotifier = std::make_unique<ScopeGuard>([this]() {
        m_pendingCursor->decrementOutstandingRequestCount();
    });
}

void IDBRequest::didOpenOrIterateCursor(const IDBResultData& resultData)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(m_pendingCursor);

    m_result = std::nullopt;

    if (resultData.type() == IDBResultType::IterateCursorSuccess || resultData.type() == IDBResultType::OpenCursorSuccess) {
        m_pendingCursor->setGetResult(*this, resultData.getResult());
        if (resultData.getResult().isDefined())
            m_result = Result { m_pendingCursor };
    }

    m_cursorRequestNotifier = nullptr;
    m_pendingCursor = nullptr;

    completeRequestAndDispatchEvent(resultData);
}

void IDBRequest::completeRequestAndDispatchEvent(const IDBResultData& resultData)
{
    ASSERT(currentThread() == originThreadID());

    m_readyState = ReadyState::Done;

    m_idbError = resultData.error();
    if (!m_idbError.isNull())
        onError();
    else
        onSuccess();
}

void IDBRequest::onError()
{
    LOG(IndexedDB, "IDBRequest::onError");

    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_idbError.isNull());

    m_domError = DOMError::create(m_idbError.name(), m_idbError.message());
    enqueueEvent(Event::create(eventNames().errorEvent, true, true));
}

void IDBRequest::onSuccess()
{
    LOG(IndexedDB, "IDBRequest::onSuccess");
    ASSERT(currentThread() == originThreadID());

    enqueueEvent(Event::create(eventNames().successEvent, false, false));
}

void IDBRequest::setResult(Ref<IDBDatabase>&& database)
{
    ASSERT(currentThread() == originThreadID());

    m_result = Result { RefPtr<IDBDatabase> { WTFMove(database) } };
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
