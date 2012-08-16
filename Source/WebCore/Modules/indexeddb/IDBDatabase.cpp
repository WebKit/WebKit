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

#include "EventQueue.h"
#include "ExceptionCode.h"
#include "IDBAny.h"
#include "IDBDatabaseCallbacksImpl.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBIndex.h"
#include "IDBKeyPath.h"
#include "IDBObjectStore.h"
#include "IDBTracing.h"
#include "IDBTransaction.h"
#include "IDBVersionChangeEvent.h"
#include "IDBVersionChangeRequest.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include <limits>

namespace WebCore {

PassRefPtr<IDBDatabase> IDBDatabase::create(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackendInterface> database)
{
    RefPtr<IDBDatabase> idbDatabase(adoptRef(new IDBDatabase(context, database)));
    idbDatabase->suspendIfNeeded();
    return idbDatabase.release();
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
    m_metadata = m_backend->metadata();
}

IDBDatabase::~IDBDatabase()
{
    close();
    m_databaseCallbacks->unregisterDatabase(this);
}

void IDBDatabase::transactionCreated(IDBTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(!m_transactions.contains(transaction));
    m_transactions.add(transaction);

    if (transaction->isVersionChange()) {
        ASSERT(!m_versionChangeTransaction);
        m_versionChangeTransaction = transaction;
        m_metadata = m_backend->metadata();
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
        m_metadata = m_backend->metadata();
    }

    if (m_closePending && m_transactions.isEmpty())
        closeConnection();
}

PassRefPtr<DOMStringList> IDBDatabase::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = m_metadata.objectStores.begin(); it != m_metadata.objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    objectStoreNames->sort();
    return objectStoreNames.release();
}

PassRefPtr<IDBAny> IDBDatabase::version() const
{
    int64_t intVersion = m_metadata.intVersion;
    if (intVersion == IDBDatabaseMetadata::NoIntVersion)
        return IDBAny::createString(m_metadata.version);
    return IDBAny::create(SerializedScriptValue::numberValue(intVersion));
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const Dictionary& options, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::IDB_INVALID_STATE_ERR;
        return 0;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
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

    if (m_metadata.objectStores.contains(name)) {
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        ec = IDBDatabaseException::IDB_SYNTAX_ERR;
        return 0;
    }

    bool autoIncrement = false;
    if (!options.isUndefinedOrNull())
        options.get("autoIncrement", autoIncrement);

    if (autoIncrement && ((keyPath.type() == IDBKeyPath::StringType && keyPath.string().isEmpty()) || keyPath.type() == IDBKeyPath::ArrayType)) {
        ec = IDBDatabaseException::IDB_INVALID_ACCESS_ERR;
        return 0;
    }

    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->createObjectStore(name, keyPath, autoIncrement, m_versionChangeTransaction->backend(), ec);
    if (!objectStoreBackend) {
        ASSERT(ec);
        return 0;
    }

    IDBObjectStoreMetadata metadata(name, keyPath, autoIncrement);
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(metadata, objectStoreBackend.release(), m_versionChangeTransaction.get());
    m_metadata.objectStores.set(name, metadata);

    m_versionChangeTransaction->objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void IDBDatabase::deleteObjectStore(const String& name, ExceptionCode& ec)
{
    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::IDB_INVALID_STATE_ERR;
        return;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }
    if (!m_metadata.objectStores.contains(name)) {
        ec = IDBDatabaseException::IDB_NOT_FOUND_ERR;
        return;
    }

    m_backend->deleteObjectStore(name, m_versionChangeTransaction->backend(), ec);
    if (!ec) {
        m_versionChangeTransaction->objectStoreDeleted(name);
        m_metadata.objectStores.remove(name);
    }
}

PassRefPtr<IDBVersionChangeRequest> IDBDatabase::setVersion(ScriptExecutionContext* context, const String& version, ExceptionCode& ec)
{
    if (version.isNull()) {
        ec = NATIVE_TYPE_ERR;
        return 0;
    }

    if (m_versionChangeTransaction) {
        ec = IDBDatabaseException::IDB_INVALID_STATE_ERR;
        return 0;
    }

    RefPtr<IDBVersionChangeRequest> request = IDBVersionChangeRequest::create(context, IDBAny::create(this), version);
    ASSERT(m_backend);
    m_backend->setVersion(version, request, m_databaseCallbacks, ec);
    return request;
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, PassRefPtr<DOMStringList> prpStoreNames, const String& modeString, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = prpStoreNames;
    ASSERT(storeNames.get());
    if (storeNames->isEmpty()) {
        ec = IDBDatabaseException::IDB_INVALID_ACCESS_ERR;
        return 0;
    }

    IDBTransaction::Mode mode = IDBTransaction::stringToMode(modeString, ec);
    if (ec)
        return 0;

    if (m_versionChangeTransaction || m_closePending) {
        ec = IDBDatabaseException::IDB_INVALID_STATE_ERR;
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
    RefPtr<IDBTransaction> transaction = IDBTransaction::create(context, transactionBackend, mode, this);
    transactionBackend->setCallbacks(transaction.get());
    return transaction.release();
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const String& storeName, const String& mode, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, ec);
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const String& storeName, unsigned short mode, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, ec);
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, PassRefPtr<DOMStringList> prpStoreNames, unsigned short mode, ExceptionCode& ec)
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Numeric transaction modes are deprecated in IDBDatabase.transaction. Use \"readonly\" or \"readwrite\"."));
    context->addConsoleMessage(JSMessageSource, LogMessageType, WarningMessageLevel, consoleMessage);
    AtomicString modeString = IDBTransaction::modeToString(IDBTransaction::Mode(mode), ec);
    if (ec)
        return 0;

    return transaction(context, prpStoreNames, modeString, ec);
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

void IDBDatabase::onVersionChange(const String& version)
{
    if (m_contextStopped || !scriptExecutionContext())
        return;

    if (m_closePending)
        return;

    enqueueEvent(IDBVersionChangeEvent::create(version, eventNames().versionchangeEvent));
}

void IDBDatabase::registerFrontendCallbacks()
{
    ASSERT(m_backend);
    m_backend->registerFrontendCallbacks(m_databaseCallbacks);
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
