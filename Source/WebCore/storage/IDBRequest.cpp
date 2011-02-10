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

#include "Document.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBIndex.h"
#include "IDBErrorEvent.h"
#include "IDBObjectStore.h"
#include "IDBPendingTransactionMonitor.h"
#include "IDBSuccessEvent.h"

namespace WebCore {

PassRefPtr<IDBRequest> IDBRequest::create(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransaction* transaction)
{
    return adoptRef(new IDBRequest(context, source, transaction));
}

IDBRequest::IDBRequest(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransaction* transaction)
    : ActiveDOMObject(context, this)
    , m_source(source)
    , m_transaction(transaction)
    , m_readyState(LOADING)
    , m_finished(false)
{
    if (m_transaction)
        IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());
}

IDBRequest::~IDBRequest()
{
}

bool IDBRequest::resetReadyState(IDBTransaction* transaction)
{
    ASSERT(!m_finished);
    ASSERT(scriptExecutionContext());
    if (m_readyState != DONE)
        return false;

    m_transaction = transaction;
    m_readyState = LOADING;

    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());

    return true;
}

void IDBRequest::onError(PassRefPtr<IDBDatabaseError> error)
{
    enqueueEvent(IDBErrorEvent::create(m_source, *error));
}

void IDBRequest::onSuccess(PassRefPtr<IDBCursorBackendInterface> backend)
{
    enqueueEvent(IDBSuccessEvent::create(m_source, IDBAny::create(IDBCursor::create(backend, this, m_transaction.get()))));
}

void IDBRequest::onSuccess(PassRefPtr<IDBDatabaseBackendInterface> backend)
{
    enqueueEvent(IDBSuccessEvent::create(m_source, IDBAny::create(IDBDatabase::create(scriptExecutionContext(), backend))));
}

void IDBRequest::onSuccess(PassRefPtr<IDBIndexBackendInterface> backend)
{
    ASSERT_NOT_REACHED(); // FIXME: This method should go away.
}

void IDBRequest::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    enqueueEvent(IDBSuccessEvent::create(m_source, IDBAny::create(idbKey)));
}

void IDBRequest::onSuccess(PassRefPtr<IDBObjectStoreBackendInterface> backend)
{
    ASSERT_NOT_REACHED(); // FIXME: This method should go away.
}

void IDBRequest::onSuccess(PassRefPtr<IDBTransactionBackendInterface> prpBackend)
{
    if (!scriptExecutionContext())
        return;

    RefPtr<IDBTransactionBackendInterface> backend = prpBackend;
    RefPtr<IDBTransaction> frontend = IDBTransaction::create(scriptExecutionContext(), backend, m_source->idbDatabase().get());
    backend->setCallbacks(frontend.get());
    m_transaction = frontend;

    ASSERT(m_source->type() == IDBAny::IDBDatabaseType);
    m_source->idbDatabase()->setSetVersionTransaction(frontend.get());

    IDBPendingTransactionMonitor::removePendingTransaction(m_transaction->backend());
    enqueueEvent(IDBSuccessEvent::create(m_source, IDBAny::create(frontend.release())));
}

void IDBRequest::onSuccess(PassRefPtr<SerializedScriptValue> serializedScriptValue)
{
    enqueueEvent(IDBSuccessEvent::create(m_source, IDBAny::create(serializedScriptValue)));
}

bool IDBRequest::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us and we have event listeners. This is order to handle
    //        user generated events properly.
    return !m_finished || ActiveDOMObject::hasPendingActivity();
}

ScriptExecutionContext* IDBRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

bool IDBRequest::dispatchEvent(PassRefPtr<Event> event)
{
    ASSERT(!m_finished);
    ASSERT(scriptExecutionContext());
    ASSERT(event->target() == this);
    ASSERT(m_readyState < DONE);
    m_readyState = DONE;

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

    ASSERT(event->isIDBErrorEvent() || event->isIDBSuccessEvent());
    bool dontPreventDefault = static_cast<IDBEvent*>(event.get())->dispatch(targets);

    // If the event's result was of type IDBCursor, then it's possible for us to
    // fire again (unless the transaction completes).
    if (event->isIDBSuccessEvent()) {
        RefPtr<IDBAny> any = static_cast<IDBSuccessEvent*>(event.get())->result();
        if (any->type() != IDBAny::IDBCursorType)
            m_finished = true;
    } else
        m_finished = true;

    if (m_transaction) {
        if (dontPreventDefault && event->isIDBErrorEvent())
            m_transaction->backend()->abort();
        m_transaction->backend()->didCompleteTaskEvents();
    }
    return dontPreventDefault;
}

void IDBRequest::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(!m_finished);
    ASSERT(m_readyState < DONE);
    if (!scriptExecutionContext())
        return;

    ASSERT(scriptExecutionContext()->isDocument());
    EventQueue* eventQueue = static_cast<Document*>(scriptExecutionContext())->eventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event);
}

EventTargetData* IDBRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif
