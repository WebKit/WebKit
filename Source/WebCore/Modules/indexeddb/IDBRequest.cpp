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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "IDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBEventDispatcher.h"
#include "IDBPendingTransactionMonitor.h"
#include "IDBTracing.h"
#include "IDBTransaction.h"

namespace WebCore {

PassRefPtr<IDBRequest> IDBRequest::create(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransaction* transaction)
{
    RefPtr<IDBRequest> request(adoptRef(new IDBRequest(context, source, transaction)));
    request->suspendIfNeeded();
    return request.release();
}

IDBRequest::IDBRequest(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransaction* transaction)
    : ActiveDOMObject(context, this)
    , m_errorCode(0)
    , m_source(source)
    , m_transaction(transaction)
    , m_readyState(LOADING)
    , m_requestFinished(false)
    , m_cursorFinished(false)
    , m_contextStopped(false)
    , m_cursorType(IDBCursorBackendInterface::InvalidCursorType)
    , m_cursor(0)
{
    if (m_transaction) {
        m_transaction->registerRequest(this);
        IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());
    }
}

IDBRequest::~IDBRequest()
{
    ASSERT(m_readyState == DONE || m_readyState == EarlyDeath || !scriptExecutionContext());
    if (m_transaction)
        m_transaction->unregisterRequest(this);
}

PassRefPtr<IDBAny> IDBRequest::result(ExceptionCode& ec) const
{
    if (m_readyState != DONE) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }
    return m_result;
}

unsigned short IDBRequest::errorCode(ExceptionCode& ec) const
{
    if (m_readyState != DONE) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }
    return m_errorCode;
}

String IDBRequest::webkitErrorMessage(ExceptionCode& ec) const
{
    if (m_readyState != DONE) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return String();
    }
    return m_errorMessage;
}

PassRefPtr<IDBAny> IDBRequest::source() const
{
    return m_source;
}

PassRefPtr<IDBTransaction> IDBRequest::transaction() const
{
    return m_transaction;
}

unsigned short IDBRequest::readyState() const
{
    ASSERT(m_readyState == LOADING || m_readyState == DONE);
    return m_readyState;
}

void IDBRequest::markEarlyDeath()
{
    ASSERT(m_readyState == LOADING);
    m_readyState = EarlyDeath;
}

bool IDBRequest::resetReadyState(IDBTransaction* transaction)
{
    ASSERT(!m_requestFinished);
    ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(transaction, transaction == m_transaction);
    if (m_readyState != DONE)
        return false;

    m_readyState = LOADING;
    m_result.clear();
    m_errorCode = 0;
    m_errorMessage = String();

    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());

    return true;
}

IDBAny* IDBRequest::source()
{
    return m_source.get();
}

void IDBRequest::abort()
{
    if (m_contextStopped || !scriptExecutionContext())
        return;

    if (m_readyState != LOADING) {
        ASSERT(m_readyState == DONE);
        return;
    }

    EventQueue* eventQueue = scriptExecutionContext()->eventQueue();
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        bool removed = eventQueue->cancelEvent(m_enqueuedEvents[i].get());
        ASSERT_UNUSED(removed, removed);
    }
    m_enqueuedEvents.clear();

    m_errorCode = 0;
    m_errorMessage = String();
    m_result.clear();
    onError(IDBDatabaseError::create(IDBDatabaseException::ABORT_ERR, "The transaction was aborted, so the request cannot be fulfilled."));
}

void IDBRequest::setCursorType(IDBCursorBackendInterface::CursorType cursorType)
{
    ASSERT(m_cursorType == IDBCursorBackendInterface::InvalidCursorType);
    m_cursorType = cursorType;
}

void IDBRequest::setCursor(PassRefPtr<IDBCursor> cursor)
{
    ASSERT(!m_cursor);
    m_cursor = cursor;
}

void IDBRequest::finishCursor()
{
    m_cursorFinished = true;
    if (m_readyState != LOADING)
        m_requestFinished = true;
}

void IDBRequest::onError(PassRefPtr<IDBDatabaseError> error)
{
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    m_errorCode = error->code();
    m_errorMessage = error->message();
    m_cursor.clear();
    enqueueEvent(Event::create(eventNames().errorEvent, true, true));
}

static PassRefPtr<Event> createSuccessEvent()
{
    return Event::create(eventNames().successEvent, false, false);
}

void IDBRequest::onSuccess(PassRefPtr<DOMStringList> domStringList)
{
    IDB_TRACE("IDBRequest::onSuccess(DOMStringList)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    m_result = IDBAny::create(domStringList);
    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccess(PassRefPtr<IDBCursorBackendInterface> backend)
{
    IDB_TRACE("IDBRequest::onSuccess(IDBCursor)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    ASSERT(m_cursorType != IDBCursorBackendInterface::InvalidCursorType);
    RefPtr<IDBCursor> cursor;
    if (m_cursorType == IDBCursorBackendInterface::IndexKeyCursor)
        cursor = IDBCursor::create(backend, this, m_source.get(), m_transaction.get());
    else
        cursor = IDBCursorWithValue::create(backend, this, m_source.get(), m_transaction.get());
    setResultCursor(cursor, m_cursorType);

    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccess(PassRefPtr<IDBDatabaseBackendInterface> backend)
{
    IDB_TRACE("IDBRequest::onSuccess(IDBDatabase)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    if (m_contextStopped || !scriptExecutionContext())
        return;

    RefPtr<IDBDatabase> idbDatabase = IDBDatabase::create(scriptExecutionContext(), backend);
    idbDatabase->open();

    m_result = IDBAny::create(idbDatabase.release());
    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    IDB_TRACE("IDBRequest::onSuccess(IDBKey)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    if (idbKey && idbKey->valid())
        m_result = IDBAny::create(idbKey);
    else
        m_result = IDBAny::create(SerializedScriptValue::undefinedValue());
    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccess(PassRefPtr<IDBTransactionBackendInterface> prpBackend)
{
    IDB_TRACE("IDBRequest::onSuccess(IDBTransaction)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    RefPtr<IDBTransactionBackendInterface> backend = prpBackend;

    if (m_contextStopped || !scriptExecutionContext()) {
        backend->abort();
        return;
    }

    RefPtr<IDBTransaction> frontend = IDBTransaction::create(scriptExecutionContext(), backend, m_source->idbDatabase().get());
    backend->setCallbacks(frontend.get());
    m_transaction = frontend;

    ASSERT(m_source->type() == IDBAny::IDBDatabaseType);
    ASSERT(m_transaction->mode() == IDBTransaction::VERSION_CHANGE);
    m_source->idbDatabase()->setVersionChangeTransaction(frontend.get());

    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());

    m_result = IDBAny::create(frontend.release());
    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccess(PassRefPtr<SerializedScriptValue> serializedScriptValue)
{
    IDB_TRACE("IDBRequest::onSuccess(SerializedScriptValue)");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    m_result = IDBAny::create(serializedScriptValue);
    m_cursor.clear();
    enqueueEvent(createSuccessEvent());
}

void IDBRequest::onSuccessWithContinuation()
{
    IDB_TRACE("IDBRequest::onSuccessWithContinuation");
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_result);
    ASSERT(m_cursor);
    setResultCursor(m_cursor, m_cursorType);
    m_cursor.clear();
    enqueueEvent(createSuccessEvent());
}

bool IDBRequest::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us and we have event listeners. This is order to handle
    //        user generated events properly.
    return !m_requestFinished || ActiveDOMObject::hasPendingActivity();
}

void IDBRequest::stop()
{
    ActiveDOMObject::stop();
    if (m_contextStopped)
        return;

    m_contextStopped = true;
    if (m_readyState == LOADING)
        markEarlyDeath();
}

void IDBRequest::onBlocked()
{
    ASSERT_NOT_REACHED();
}

const AtomicString& IDBRequest::interfaceName() const
{
    return eventNames().interfaceForIDBRequest;
}

ScriptExecutionContext* IDBRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

bool IDBRequest::dispatchEvent(PassRefPtr<Event> event)
{
    IDB_TRACE("IDBRequest::dispatchEvent");
    ASSERT(!m_requestFinished);
    ASSERT(!m_contextStopped);
    ASSERT(m_enqueuedEvents.size());
    ASSERT(scriptExecutionContext());
    ASSERT(event->target() == this);
    ASSERT_WITH_MESSAGE(m_readyState < DONE, "m_readyState < DONE(%d), was %d", DONE, m_readyState);
    if (event->type() != eventNames().blockedEvent)
        m_readyState = DONE;

    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].get() == event.get())
            m_enqueuedEvents.remove(i);
    }

    Vector<RefPtr<EventTarget> > targets;
    targets.append(this);
    if (m_transaction) {
        targets.append(m_transaction);
        // If there ever are events that are associated with a database but
        // that do not have a transaction, then this will not work and we need
        // this object to actually hold a reference to the database (to ensure
        // it stays alive).
        targets.append(m_transaction->db());
    }

    RefPtr<IDBCursor> cursorToNotify;
    if (m_result) {
        if (m_result->type() == IDBAny::IDBCursorType)
            cursorToNotify = m_result->idbCursor();
        else if (m_result->type() == IDBAny::IDBCursorWithValueType)
            cursorToNotify = m_result->idbCursorWithValue();
        if (cursorToNotify)
            cursorToNotify->setValueReady();
    }

    // FIXME: When we allow custom event dispatching, this will probably need to change.
    ASSERT(event->type() == eventNames().successEvent || event->type() == eventNames().errorEvent || event->type() == eventNames().blockedEvent);
    bool dontPreventDefault = IDBEventDispatcher::dispatch(event.get(), targets);

    // If the result was of type IDBCursor, or a onBlocked event, then we'll fire again.
    if (event->type() != eventNames().blockedEvent && (!cursorToNotify || m_cursorFinished))
        m_requestFinished = true;

    if (cursorToNotify)
        cursorToNotify->postSuccessHandlerCallback();

    if (m_transaction && event->type() != eventNames().blockedEvent) {
        // If an error event and the default wasn't prevented...
        if (dontPreventDefault && event->type() == eventNames().errorEvent)
            m_transaction->backend()->abort();
        m_transaction->backend()->didCompleteTaskEvents();
    }
    return dontPreventDefault;
}

void IDBRequest::uncaughtExceptionInEventHandler()
{
    if (m_transaction)
        m_transaction->backend()->abort();
}

void IDBRequest::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(!m_requestFinished);

    if (m_contextStopped || !scriptExecutionContext())
        return;

    ASSERT(m_readyState < DONE);

    EventQueue* eventQueue = scriptExecutionContext()->eventQueue();
    event->setTarget(this);

    if (eventQueue->enqueueEvent(event.get()))
        m_enqueuedEvents.append(event);
}

EventTargetData* IDBRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void IDBRequest::setResultCursor(PassRefPtr<IDBCursor> prpCursor, IDBCursorBackendInterface::CursorType type)
{
    if (type == IDBCursorBackendInterface::IndexKeyCursor) {
        m_result = IDBAny::create(prpCursor);
        return;
    }

    m_result = IDBAny::create(IDBCursorWithValue::fromCursor(prpCursor));
}

} // namespace WebCore

#endif
