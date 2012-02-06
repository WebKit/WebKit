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
#include "IDBDatabase.h"

#include "EventQueue.h"
#include "ExceptionCode.h"
#include "EventQueue.h"
#include "IDBAny.h"
#include "IDBDatabaseCallbacksImpl.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBIndex.h"
#include "IDBKeyPath.h"
#include "IDBObjectStore.h"
#include "IDBVersionChangeEvent.h"
#include "IDBVersionChangeRequest.h"
#include "IDBTransaction.h"
#include "ScriptExecutionContext.h"
#include <limits>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBDatabase> IDBDatabase::create(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackendInterface> database)
{
    return adoptRef(new IDBDatabase(context, database));
}

IDBDatabase::IDBDatabase(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackendInterface> backend)
    : ActiveDOMObject(context, this)
    , m_backend(backend)
    , m_closePending(false)
    , m_contextStopped(false)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
    m_databaseCallbacks = IDBDatabaseCallbacksImpl::create(this);
}

IDBDatabase::~IDBDatabase()
{
    close();
    m_databaseCallbacks->unregisterDatabase(this);
}

void IDBDatabase::setVersionChangeTransaction(IDBTransaction* transaction)
{
    ASSERT(!m_versionChangeTransaction);
    m_versionChangeTransaction = transaction;
}

void IDBDatabase::clearVersionChangeTransaction(IDBTransaction* transaction)
{
    ASSERT_UNUSED(transaction, m_versionChangeTransaction == transaction);
    m_versionChangeTransaction = 0;
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const OptionsObject& options, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    String keyPath;
    bool keyPathExists = options.getWithUndefinedOrNullCheck("keyPath", keyPath);
    if (keyPathExists && !IDBIsValidKeyPath(keyPath)) {
        ec = IDBDatabaseException::NON_TRANSIENT_ERR;
        return 0;
    }

    bool autoIncrement = false;
    options.get("autoIncrement", autoIncrement);
    // FIXME: Look up evictable and pass that on as well.

    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->createObjectStore(name, keyPath, autoIncrement, m_versionChangeTransaction->backend(), ec);
    if (!objectStoreBackend) {
        ASSERT(ec);
        return 0;
    }

    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(objectStoreBackend.release(), m_versionChangeTransaction.get());
    m_versionChangeTransaction->objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void IDBDatabase::deleteObjectStore(const String& name, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }

    m_backend->deleteObjectStore(name, m_versionChangeTransaction->backend(), ec);
    m_versionChangeTransaction->objectStoreDeleted(name);
}

PassRefPtr<IDBVersionChangeRequest> IDBDatabase::setVersion(ScriptExecutionContext* context, const String& version, ExceptionCode& ec)
{
    if (version.isNull()) {
        ec = IDBDatabaseException::NON_TRANSIENT_ERR;
        return 0;
    }

    RefPtr<IDBVersionChangeRequest> request = IDBVersionChangeRequest::create(context, IDBAny::create(this), version);
    m_backend->setVersion(version, request, m_databaseCallbacks, ec);
    return request;
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const String& storeName, unsigned short mode, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, ec);
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, PassRefPtr<DOMStringList> prpStoreNames, unsigned short mode, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = prpStoreNames;
    if (!storeNames || storeNames->isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    if (mode != IDBTransaction::READ_WRITE && mode != IDBTransaction::READ_ONLY) {
        ec = IDBDatabaseException::NON_TRANSIENT_ERR;
        return 0;
    }
    if (m_closePending) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    // We need to create a new transaction synchronously. Locks are acquired asynchronously. Operations
    // can be queued against the transaction at any point. They will start executing as soon as the
    // appropriate locks have been acquired.
    // Also note that each backend object corresponds to exactly one IDBTransaction object.
    RefPtr<IDBTransactionBackendInterface> transactionBackend = m_backend->transaction(storeNames.get(), mode, ec);
    if (!transactionBackend) {
        ASSERT(ec);
        return 0;
    }
    RefPtr<IDBTransaction> transaction = IDBTransaction::create(context, transactionBackend, this);
    transactionBackend->setCallbacks(transaction.get());
    return transaction.release();
}

void IDBDatabase::close()
{
    if (m_closePending)
        return;

    m_closePending = true;
    m_backend->close(m_databaseCallbacks);

    if (m_contextStopped || !scriptExecutionContext())
        return;

    EventQueue* eventQueue = scriptExecutionContext()->eventQueue();
    // Remove any pending versionchange events scheduled to fire on this
    // connection. They would have been scheduled by the backend when another
    // connection called setVersion, but the frontend connection is being
    // closed before they could fire.
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        bool removed = eventQueue->cancelEvent(m_enqueuedEvents[i].get());
        ASSERT_UNUSED(removed, removed);
    }
}

void IDBDatabase::onVersionChange(const String& version)
{
    if (m_contextStopped || !scriptExecutionContext())
        return;

    enqueueEvent(IDBVersionChangeEvent::create(version, eventNames().versionchangeEvent));
}

void IDBDatabase::open()
{
    m_backend->open(m_databaseCallbacks);
}

void IDBDatabase::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(!m_contextStopped);
    ASSERT(scriptExecutionContext());
    EventQueue* eventQueue = scriptExecutionContext()->eventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event.get());
    m_enqueuedEvents.append(event);
}

bool IDBDatabase::dispatchEvent(PassRefPtr<Event> event)
{
    ASSERT(event->type() == eventNames().versionchangeEvent);
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].get() == event.get())
            m_enqueuedEvents.remove(i);
    }
    return EventTarget::dispatchEvent(event.get());
}

void IDBDatabase::stop()
{
    ActiveDOMObject::stop();
    // Stop fires at a deterministic time, so we need to call close in it.
    close();

    m_contextStopped = true;
}

const AtomicString& IDBDatabase::interfaceName() const
{
    return eventNames().interfaceForIDBDatabase;
}

ScriptExecutionContext* IDBDatabase::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetData* IDBDatabase::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBDatabase::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
