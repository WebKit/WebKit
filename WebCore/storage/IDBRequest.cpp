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

#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBErrorEvent.h"
#include "IDBObjectStore.h"
#include "IDBPendingTransactionMonitor.h"
#include "IDBSuccessEvent.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBRequest::IDBRequest(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransactionBackendInterface* transaction)
    : ActiveDOMObject(context, this)
    , m_source(source)
    , m_transaction(transaction)
    , m_timer(this, &IDBRequest::timerFired)
    , m_readyState(LOADING)
{
    if (m_transaction)
        IDBPendingTransactionMonitor::removePendingTransaction(m_transaction.get());
}

IDBRequest::~IDBRequest()
{
}

bool IDBRequest::resetReadyState(IDBTransactionBackendInterface* transaction)
{
    ASSERT(m_readyState == DONE);
    m_readyState = LOADING;
    ASSERT(!m_transaction);
    m_transaction = transaction;
    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction.get());
    return true;
}

void IDBRequest::onError(PassRefPtr<IDBDatabaseError> error)
{
    scheduleEvent(0, error);
}

void IDBRequest::onSuccess()
{
    scheduleEvent(IDBAny::createNull(), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBCursorBackendInterface> backend)
{
    scheduleEvent(IDBAny::create(IDBCursor::create(backend, this, m_transaction.get())), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBDatabaseBackendInterface> backend)
{
    scheduleEvent(IDBAny::create(IDBDatabase::create(backend)), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBIndexBackendInterface> backend)
{
    scheduleEvent(IDBAny::create(IDBIndex::create(backend, m_transaction.get())), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    scheduleEvent(IDBAny::create(idbKey), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBObjectStoreBackendInterface> backend)
{
    // FIXME: This function should go away once createObjectStore is sync.
    scheduleEvent(IDBAny::create(IDBObjectStore::create(backend, m_transaction.get())), 0);
}

void IDBRequest::onSuccess(PassRefPtr<IDBTransactionBackendInterface> prpBackend)
{
    RefPtr<IDBTransactionBackendInterface> backend = prpBackend;
    // This is only used by setVersion which will always have a source that's an IDBDatabase.
    m_source->idbDatabase()->setSetVersionTransaction(backend.get());
    RefPtr<IDBTransaction> frontend = IDBTransaction::create(scriptExecutionContext(), backend, m_source->idbDatabase().get());
    backend->setCallbacks(frontend.get());
    m_transaction = backend;
    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction.get());
    scheduleEvent(IDBAny::create(frontend.release()), 0);
}

void IDBRequest::onSuccess(PassRefPtr<SerializedScriptValue> serializedScriptValue)
{
    scheduleEvent(IDBAny::create(serializedScriptValue), 0);
}

ScriptExecutionContext* IDBRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

bool IDBRequest::canSuspend() const
{
    // IDBTransactions cannot be suspended at the moment. We therefore
    // disallow the back/forward cache for pages that use IndexedDatabase.
    return false;
}

EventTargetData* IDBRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void IDBRequest::timerFired(Timer<IDBRequest>*)
{
    ASSERT(m_selfRef);
    ASSERT(m_pendingEvents.size());
    // FIXME: We should handle the stop event and stop any timers when we see it. We can then assert here that scriptExecutionContext is non-null.

    // We need to keep self-referencing ourself, otherwise it's possible we'll be deleted.
    // But in some cases, suspend() could be called while we're dispatching an event, so we
    // need to make sure that resume() doesn't re-start the timer based on m_selfRef being set.
    RefPtr<IDBRequest> selfRef = m_selfRef.release();

    // readyStateReset can be called synchronously while we're dispatching the event.
    RefPtr<IDBTransactionBackendInterface> transaction = m_transaction;
    m_transaction.clear();

    Vector<PendingEvent> pendingEvents;
    pendingEvents.swap(m_pendingEvents);
    for (size_t i = 0; i < pendingEvents.size(); ++i) {
        // It's possible we've navigated in which case we'll crash.
        if (!scriptExecutionContext())
            return;

        if (pendingEvents[i].m_error) {
            ASSERT(!pendingEvents[i].m_result);
            dispatchEvent(IDBErrorEvent::create(m_source, *pendingEvents[i].m_error));
        } else {
            ASSERT(pendingEvents[i].m_result->type() != IDBAny::UndefinedType);
            dispatchEvent(IDBSuccessEvent::create(m_source, pendingEvents[i].m_result));
        }
    }
    if (transaction) {
        // Now that we processed all pending events, let the transaction monitor check if
        // it can commit the current transaction or if there's anything new pending.
        // FIXME: Handle the workers case.
        transaction->didCompleteTaskEvents();
    }
}

void IDBRequest::scheduleEvent(PassRefPtr<IDBAny> result, PassRefPtr<IDBDatabaseError> error)
{
    ASSERT(m_readyState < DONE);
    ASSERT(!!m_selfRef == m_timer.isActive());

    PendingEvent pendingEvent;
    pendingEvent.m_result = result;
    pendingEvent.m_error = error;
    m_pendingEvents.append(pendingEvent);

    m_readyState = DONE;
    if (!m_timer.isActive()) {
        m_selfRef = this;
        m_timer.startOneShot(0);
    }
}

} // namespace WebCore

#endif
