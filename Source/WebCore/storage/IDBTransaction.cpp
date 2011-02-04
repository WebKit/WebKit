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

#include "config.h"
#include "IDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "EventException.h"
#include "EventQueue.h"
#include "IDBAbortEvent.h"
#include "IDBCompleteEvent.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBPendingTransactionMonitor.h"

namespace WebCore {

PassRefPtr<IDBTransaction> IDBTransaction::create(ScriptExecutionContext* context, PassRefPtr<IDBTransactionBackendInterface> backend, IDBDatabase* db)
{ 
    return adoptRef(new IDBTransaction(context, backend, db));
}

IDBTransaction::IDBTransaction(ScriptExecutionContext* context, PassRefPtr<IDBTransactionBackendInterface> backend, IDBDatabase* db)
    : ActiveDOMObject(context, this)
    , m_backend(backend)
    , m_database(db)
    , m_mode(m_backend->mode())
{
    IDBPendingTransactionMonitor::addPendingTransaction(m_backend.get());
}

IDBTransaction::~IDBTransaction()
{
}

unsigned short IDBTransaction::mode() const
{
    return m_mode;
}

IDBDatabase* IDBTransaction::db()
{
    return m_database.get();
}

PassRefPtr<IDBObjectStore> IDBTransaction::objectStore(const String& name, ExceptionCode& ec)
{
    // FIXME: It'd be better (because it's more deterministic) if we didn't start throwing this until the complete or abort event fires.
    if (!m_backend) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }
    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->objectStore(name, ec);
    if (!objectStoreBackend) {
        ASSERT(ec);
        return 0;
    }
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(objectStoreBackend, m_backend.get());
    return objectStore.release();
}

void IDBTransaction::abort()
{
    RefPtr<IDBTransaction> selfRef = this;
    if (m_backend)
        m_backend->abort();
}

ScriptExecutionContext* IDBTransaction::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void IDBTransaction::onAbort()
{
    enqueueEvent(IDBAbortEvent::create());
}

void IDBTransaction::onComplete()
{
    enqueueEvent(IDBCompleteEvent::create());
}

bool IDBTransaction::canSuspend() const
{
    return !m_backend;
}

void IDBTransaction::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();

    RefPtr<IDBTransaction> selfRef = this;
    if (m_backend)
        m_backend->abort();
    m_backend.clear();
}

void IDBTransaction::enqueueEvent(PassRefPtr<Event> event)
{
    if (!scriptExecutionContext())
        return;

    ASSERT(m_backend);
    m_backend.clear();
    
    ASSERT(scriptExecutionContext()->isDocument());
    EventQueue* eventQueue = static_cast<Document*>(scriptExecutionContext())->eventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event);
}

EventTargetData* IDBTransaction::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBTransaction::ensureEventTargetData()
{
    return &m_eventTargetData;
}

}

#endif // ENABLE(INDEXED_DATABASE)
