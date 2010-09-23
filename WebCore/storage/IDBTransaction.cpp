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

#include "Event.h"
#include "EventException.h"
#include "IDBAbortEvent.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBObjectStore.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBPendingTransactionMonitor.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBTransaction::IDBTransaction(ScriptExecutionContext* context, PassRefPtr<IDBTransactionBackendInterface> backend, IDBDatabase* db)
    : ActiveDOMObject(context, this)
    , m_backend(backend)
    , m_database(db)
    , m_stopped(false)
    , m_timer(this, &IDBTransaction::timerFired)
{
    IDBPendingTransactionMonitor::addPendingTransaction(m_backend.get());
}

IDBTransaction::~IDBTransaction()
{
}

unsigned short IDBTransaction::mode() const
{
    return m_backend->mode();
}

IDBDatabase* IDBTransaction::db()
{
    return m_database.get();
}

PassRefPtr<IDBObjectStore> IDBTransaction::objectStore(const String& name, const ExceptionCode&)
{
    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->objectStore(name);
    if (!objectStoreBackend) {
        // FIXME: throw IDBDatabaseException::NOT_ALLOWED_ERR.
        return 0;
    }
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(objectStoreBackend, m_backend.get());
    return objectStore.release();
}

void IDBTransaction::abort()
{
    m_backend->abort();
}

ScriptExecutionContext* IDBTransaction::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void IDBTransaction::onAbort()
{
    if (!m_stopped) {
        m_selfRef = this;
        m_stopped = true;
        m_timer.startOneShot(0);
    }
    // Release the backend as it holds a (circular) reference back to us.
    m_backend.clear();
}

int IDBTransaction::id() const
{
    return m_backend->id();
}

bool IDBTransaction::canSuspend() const
{
    // We may be in the middle of a transaction so we cannot suspend our object.
    // Instead, we simply don't allow the owner page to go into the back/forward cache.
    return false;
}

void IDBTransaction::stop()
{
    if (!m_stopped) {
        // The document is getting detached. Abort!
        m_stopped = true;
        m_backend->abort();
    }
}

EventTargetData* IDBTransaction::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBTransaction::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void IDBTransaction::timerFired(Timer<IDBTransaction>* transaction)
{
    ASSERT(m_selfRef);

    RefPtr<IDBTransaction> selfRef = m_selfRef.release();
    dispatchEvent(IDBAbortEvent::create());
}

}

#endif // ENABLE(INDEXED_DATABASE)
