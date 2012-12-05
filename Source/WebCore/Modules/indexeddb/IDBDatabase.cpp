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

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "EventQueue.h"
#include "ExceptionCode.h"
#include "IDBAny.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "IDBKeyPath.h"
#include "IDBObjectStore.h"
#include "IDBTracing.h"
#include "IDBTransaction.h"
#include "IDBUpgradeNeededEvent.h"
#include "IDBVersionChangeEvent.h"
#include "IDBVersionChangeRequest.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include <limits>
#include <wtf/Atomics.h>

namespace WebCore {

PassRefPtr<IDBDatabase> IDBDatabase::create(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackendInterface> database, PassRefPtr<IDBDatabaseCallbacks> callbacks)
{
    RefPtr<IDBDatabase> idbDatabase(adoptRef(new IDBDatabase(context, database, callbacks)));
    idbDatabase->suspendIfNeeded();
    return idbDatabase.release();
}

IDBDatabase::IDBDatabase(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackendInterface> backend, PassRefPtr<IDBDatabaseCallbacks> callbacks)
    : ActiveDOMObject(context, this)
    , m_backend(backend)
    , m_closePending(false)
    , m_contextStopped(false)
    , m_databaseCallbacks(callbacks)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
}

IDBDatabase::~IDBDatabase()
{
    close();
}

int64_t IDBDatabase::nextTransactionId()
{
    // Only keep a 32-bit counter to allow ports to use the other 32
    // bits of the id.
    AtomicallyInitializedStatic(int, currentTransactionId = 0);
    return atomicIncrement(&currentTransactionId);
}

void IDBDatabase::transactionCreated(IDBTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(!m_transactions.contains(transaction));
    m_transactions.add(transaction);

    if (transaction->isVersionChange()) {
        ASSERT(!m_versionChangeTransaction);
        m_versionChangeTransaction = transaction;
    }
}

void IDBDatabase::transactionFinished(IDBTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(m_transactions.contains(transaction));
    m_transactions.remove(transaction);

    if (transaction->isVersionChange()) {
        ASSERT(m_versionChangeTransaction == transaction);
        m_versionChangeTransaction = 0;
    }

    if (m_closePending && m_transactions.isEmpty())
        closeConnection();
}

PassRefPtr<DOMStringList> IDBDatabase::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = m_metadata.objectStores.begin(); it != m_metadata.objectStores.end(); ++it)
        objectStoreNames->append(it->value.name);
    objectStoreNames->sort();
    return objectStoreNames.release();
}

PassRefPtr<IDBAny> IDBDatabase::version() const
{
    int64_t intVersion = m_metadata.intVersion;
    if (intVersion == IDBDatabaseMetadata::NoIntVersion)
        return IDBAny::createString(m_metadata.version);
    return IDBAny::create(intVersion);
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const Dictionary& options, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }

    IDBKeyPath keyPath;
    if (!options.isUndefinedOrNull()) {
        String keyPathString;
        Vector<String> keyPathArray;
        if (options.get("keyPath", keyPathArray))
            keyPath = IDBKeyPath(keyPathArray);
        else if (options.getWithUndefinedOrNullCheck("keyPath", keyPathString))
            keyPath = IDBKeyPath(keyPathString);
    }

    if (containsObjectStore(name)) {
        ec = IDBDatabaseException::ConstraintError;
        return 0;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        ec = IDBDatabaseException::SyntaxError;
        return 0;
    }

    bool autoIncrement = false;
    if (!options.isUndefinedOrNull())
        options.get("autoIncrement", autoIncrement);

    if (autoIncrement && ((keyPath.type() == IDBKeyPath::StringType && keyPath.string().isEmpty()) || keyPath.type() == IDBKeyPath::ArrayType)) {
        ec = IDBDatabaseException::InvalidAccessError;
        return 0;
    }

    int64_t objectStoreId = m_metadata.maxObjectStoreId + 1;
    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->createObjectStore(objectStoreId, name, keyPath, autoIncrement, m_versionChangeTransaction->backend(), ec);
    if (!objectStoreBackend) {
        ASSERT(ec);
        return 0;
    }

    IDBObjectStoreMetadata metadata(name, objectStoreId, keyPath, autoIncrement, IDBObjectStoreBackendInterface::MinimumIndexId);
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(metadata, objectStoreBackend.release(), m_versionChangeTransaction.get());
    m_metadata.objectStores.set(metadata.id, metadata);
    ++m_metadata.maxObjectStoreId;

    m_versionChangeTransaction->objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void IDBDatabase::deleteObjectStore(const String& name, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    int64_t objectStoreId = findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        ec = IDBDatabaseException::NotFoundError;
        return;
    }

    m_backend->deleteObjectStore(objectStoreId, m_versionChangeTransaction->backend(), ec);
    if (!ec) {
        m_versionChangeTransaction->objectStoreDeleted(name);
        m_metadata.objectStores.remove(objectStoreId);
    }
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const Vector<String>& scope, const String& modeString, ExceptionCode& ec)
{
    if (!scope.size()) {
        ec = IDBDatabaseException::InvalidAccessError;
        return 0;
    }

    IDBTransaction::Mode mode = IDBTransaction::stringToMode(modeString, context, ec);
    if (ec)
        return 0;

    if (m_versionChangeTransaction || m_closePending) {
        ec = IDBDatabaseException::InvalidStateError;
        return 0;
    }

    Vector<int64_t> objectStoreIds;
    for (size_t i = 0; i < scope.size(); ++i) {
        int64_t objectStoreId = findObjectStoreId(scope[i]);
        if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
            ec = IDBDatabaseException::NotFoundError;
            return 0;
        }
        objectStoreIds.append(objectStoreId);
    }

    // We need to create a new transaction synchronously. Locks are acquired asynchronously. Operations
    // can be queued against the transaction at any point. They will start executing as soon as the
    // appropriate locks have been acquired.
    // Also note that each backend object corresponds to exactly one IDBTransaction object.
    int64_t transactionId = nextTransactionId();
    RefPtr<IDBTransactionBackendInterface> transactionBackend = m_backend->createTransaction(transactionId, objectStoreIds, mode);
    if (!transactionBackend) {
        ASSERT(ec);
        return 0;
    }

    RefPtr<IDBTransaction> transaction = IDBTransaction::create(context, transactionId, transactionBackend, scope, mode, this);
    transactionBackend->setCallbacks(transaction.get());
    return transaction.release();
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const String& storeName, const String& mode, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, ec);
}

void IDBDatabase::forceClose()
{
    ExceptionCode ec = 0;
    for (HashSet<IDBTransaction*>::iterator it = m_transactions.begin(); it != m_transactions.end(); ++it)
        (*it)->abort(ec);
    this->close();
}

void IDBDatabase::close()
{
    if (m_closePending)
        return;

    m_closePending = true;

    if (m_transactions.isEmpty())
        closeConnection();
}

void IDBDatabase::closeConnection()
{
    ASSERT(m_closePending);
    ASSERT(m_transactions.isEmpty());

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

void IDBDatabase::onVersionChange(int64_t oldVersion, int64_t newVersion)
{
    if (m_contextStopped || !scriptExecutionContext())
        return;

    if (m_closePending)
        return;

    enqueueEvent(IDBUpgradeNeededEvent::create(oldVersion, newVersion, eventNames().versionchangeEvent));
}

void IDBDatabase::onVersionChange(const String& version)
{
    if (m_contextStopped || !scriptExecutionContext())
        return;

    if (m_closePending)
        return;

    enqueueEvent(IDBVersionChangeEvent::create(version, eventNames().versionchangeEvent));
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
    IDB_TRACE("IDBDatabase::dispatchEvent");
    ASSERT(event->type() == eventNames().versionchangeEvent);
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].get() == event.get())
            m_enqueuedEvents.remove(i);
    }
    return EventTarget::dispatchEvent(event.get());
}

int64_t IDBDatabase::findObjectStoreId(const String& name) const
{
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = m_metadata.objectStores.begin(); it != m_metadata.objectStores.end(); ++it) {
        if (it->value.name == name) {
            ASSERT(it->key != IDBObjectStoreMetadata::InvalidId);
            return it->key;
        }
    }
    return IDBObjectStoreMetadata::InvalidId;
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
