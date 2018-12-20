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

#include "DOMException.h"
#include "Event.h"
#include "EventDispatcher.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBKeyData.h"
#include "IDBObjectStore.h"
#include "IDBResultData.h"
#include "JSDOMConvertIndexedDB.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMConvertSequences.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "ThreadSafeDataBuffer.h"
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/Scope.h>
#include <wtf/Variant.h>


namespace WebCore {
using namespace JSC;

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
    ASSERT(&originThread() == &Thread::current());

    if (m_result) {
        WTF::switchOn(m_result.value(),
            [] (RefPtr<IDBCursor>& cursor) { cursor->clearRequest(); },
            [] (const auto&) { }
        );
    }
}

ExceptionOr<Optional<IDBRequest::Result>> IDBRequest::result() const
{
    if (!isDone())
        return Exception { InvalidStateError, "Failed to read the 'result' property from 'IDBRequest': The request has not finished."_s };

    return Optional<IDBRequest::Result> { m_result };
}

ExceptionOr<DOMException*> IDBRequest::error() const
{
    ASSERT(&originThread() == &Thread::current());

    if (!isDone())
        return Exception { InvalidStateError, "Failed to read the 'error' property from 'IDBRequest': The request has not finished."_s };

    return m_domError.get();
}

void IDBRequest::setSource(IDBCursor& cursor)
{
    ASSERT(&originThread() == &Thread::current());

    m_source = Source { &cursor };
}

void IDBRequest::setVersionChangeTransaction(IDBTransaction& transaction)
{
    ASSERT(&originThread() == &Thread::current());
    ASSERT(!m_transaction);
    ASSERT(transaction.isVersionChange());
    ASSERT(!transaction.isFinishedOrFinishing());

    m_transaction = &transaction;
}

RefPtr<WebCore::IDBTransaction> IDBRequest::transaction() const
{
    ASSERT(&originThread() == &Thread::current());
    return m_shouldExposeTransactionToDOM ? m_transaction : nullptr;
}

uint64_t IDBRequest::sourceObjectStoreIdentifier() const
{
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

    return m_requestedObjectStoreRecordType;
}

IndexedDB::IndexRecordType IDBRequest::requestedIndexRecordType() const
{
    ASSERT(&originThread() == &Thread::current());
    ASSERT(m_source);
    ASSERT(WTF::holds_alternative<RefPtr<IDBIndex>>(m_source.value()));

    return m_requestedIndexRecordType;
}

EventTargetInterface IDBRequest::eventTargetInterface() const
{
    ASSERT(&originThread() == &Thread::current());

    return IDBRequestEventTargetInterfaceType;
}

const char* IDBRequest::activeDOMObjectName() const
{
    ASSERT(&originThread() == &Thread::current());

    return "IDBRequest";
}

bool IDBRequest::canSuspendForDocumentSuspension() const
{
    ASSERT(&originThread() == &Thread::current());
    return false;
}

bool IDBRequest::hasPendingActivity() const
{
    ASSERT(&originThread() == &Thread::current() || mayBeGCThread());
    return !m_contextStopped && m_hasPendingActivity;
}

void IDBRequest::stop()
{
    ASSERT(&originThread() == &Thread::current());
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
    ASSERT(&originThread() == &Thread::current());
    if (m_contextStopped)
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

void IDBRequest::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBRequest::dispatchEvent - %s (%p)", event.type().string().utf8().data(), this);

    ASSERT(&originThread() == &Thread::current());
    ASSERT(m_hasPendingActivity);
    ASSERT(!m_contextStopped);

    auto protectedThis = makeRef(*this);

    if (event.type() != eventNames().blockedEvent)
        m_readyState = ReadyState::Done;

    Vector<EventTarget*> targets { this };

    if (&event == m_openDatabaseSuccessEvent)
        m_openDatabaseSuccessEvent = nullptr;
    else if (m_transaction && !m_transaction->isFinished())
        targets = { this, m_transaction.get(), &m_transaction->database() };

    m_hasPendingActivity = false;

    {
        TransactionActivator activator(m_transaction.get());
        EventDispatcher::dispatchEvent(targets, event);
    }

    // Dispatching the event might have set the pending activity flag back to true, suggesting the request will be reused.
    // We might also re-use the request if this event was the upgradeneeded event for an IDBOpenDBRequest.
    if (!m_hasPendingActivity)
        m_hasPendingActivity = isOpenDBRequest() && (event.type() == eventNames().upgradeneededEvent || event.type() == eventNames().blockedEvent);

    // The request should only remain in the transaction's request list if it represents a pending cursor operation, or this is an open request that was blocked.
    if (m_transaction && !m_pendingCursor && event.type() != eventNames().blockedEvent)
        m_transaction->removeRequest(*this);

    if (!event.defaultPrevented() && event.type() == eventNames().errorEvent && m_transaction && !m_transaction->isFinishedOrFinishing()) {
        ASSERT(m_domError);
        m_transaction->abortDueToFailedRequest(*m_domError);
    }

    if (m_transaction)
        m_transaction->finishedDispatchEventForRequest(*this);
}

void IDBRequest::uncaughtExceptionInEventHandler()
{
    LOG(IndexedDB, "IDBRequest::uncaughtExceptionInEventHandler");

    ASSERT(&originThread() == &Thread::current());

    if (m_transaction && m_idbError.code() != AbortError)
        m_transaction->abortDueToFailedRequest(DOMException::create(AbortError, "IDBTransaction will abort due to uncaught exception in an event handler"_s));
}

void IDBRequest::setResult(const IDBKeyData& keyData)
{
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    m_result = Result { JSC::Strong<JSC::Unknown> { context->vm(), toJS<IDLUnrestrictedDouble>(number) } };
}

void IDBRequest::setResultToStructuredClone(const IDBValue& value)
{
    ASSERT(&originThread() == &Thread::current());

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
    ASSERT(&originThread() == &Thread::current());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    m_result = Result { JSC::Strong<JSC::Unknown> { context->vm(), JSC::jsUndefined() } };
}

IDBCursor* IDBRequest::resultCursor()
{
    ASSERT(&originThread() == &Thread::current());

    if (!m_result)
        return nullptr;

    return WTF::switchOn(m_result.value(),
        [] (const RefPtr<IDBCursor>& cursor) -> IDBCursor* { return cursor.get(); },
        [] (const auto&) -> IDBCursor* { return nullptr; }
    );
}

void IDBRequest::willIterateCursor(IDBCursor& cursor)
{
    ASSERT(&originThread() == &Thread::current());
    ASSERT(isDone());
    ASSERT(scriptExecutionContext());
    ASSERT(m_transaction);
    ASSERT(!m_pendingCursor);
    ASSERT(&cursor == resultCursor());

    m_pendingCursor = &cursor;
    m_hasPendingActivity = true;
    m_result = WTF::nullopt;
    m_readyState = ReadyState::Pending;
    m_domError = nullptr;
    m_idbError = IDBError { };
}

void IDBRequest::didOpenOrIterateCursor(const IDBResultData& resultData)
{
    ASSERT(&originThread() == &Thread::current());
    ASSERT(m_pendingCursor);

    m_result = WTF::nullopt;

    if (resultData.type() == IDBResultType::IterateCursorSuccess || resultData.type() == IDBResultType::OpenCursorSuccess) {
        m_pendingCursor->setGetResult(*this, resultData.getResult());
        if (resultData.getResult().isDefined())
            m_result = Result { m_pendingCursor };
    }

    m_pendingCursor = nullptr;

    completeRequestAndDispatchEvent(resultData);
}

void IDBRequest::completeRequestAndDispatchEvent(const IDBResultData& resultData)
{
    ASSERT(&originThread() == &Thread::current());

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

    ASSERT(&originThread() == &Thread::current());
    ASSERT(!m_idbError.isNull());

    m_domError = m_idbError.toDOMException();
    enqueueEvent(Event::create(eventNames().errorEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
}

void IDBRequest::onSuccess()
{
    LOG(IndexedDB, "IDBRequest::onSuccess");
    ASSERT(&originThread() == &Thread::current());
    enqueueEvent(Event::create(eventNames().successEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void IDBRequest::setResult(Ref<IDBDatabase>&& database)
{
    ASSERT(&originThread() == &Thread::current());

    m_result = Result { RefPtr<IDBDatabase> { WTFMove(database) } };
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
