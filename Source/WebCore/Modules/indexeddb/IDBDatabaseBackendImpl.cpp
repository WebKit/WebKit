/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "IDBDatabaseBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "CrossThreadTask.h"
#include "DOMStringList.h"
#include "IDBBackingStore.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTransactionBackendImpl.h"
#include "IDBTransactionCoordinator.h"

namespace WebCore {

class IDBDatabaseBackendImpl::PendingOpenCall : public RefCounted<PendingOpenCall> {
public:
    static PassRefPtr<PendingOpenCall> create(PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new PendingOpenCall(callbacks));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }

private:
    PendingOpenCall(PassRefPtr<IDBCallbacks> callbacks)
        : m_callbacks(callbacks)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBDatabaseBackendImpl::PendingDeleteCall : public RefCounted<PendingDeleteCall> {
public:
    static PassRefPtr<PendingDeleteCall> create(PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new PendingDeleteCall(callbacks));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }

private:
    PendingDeleteCall(PassRefPtr<IDBCallbacks> callbacks)
        : m_callbacks(callbacks)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBDatabaseBackendImpl::PendingSetVersionCall : public RefCounted<PendingSetVersionCall> {
public:
    static PassRefPtr<PendingSetVersionCall> create(const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptRef(new PendingSetVersionCall(version, callbacks, databaseCallbacks));
    }
    String version() { return m_version; }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }
    PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks() { return m_databaseCallbacks; }

private:
    PendingSetVersionCall(const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_version(version)
        , m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }
    String m_version;
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, IDBBackingStore* backingStore, IDBTransactionCoordinator* coordinator, IDBFactoryBackendImpl* factory, const String& uniqueIdentifier)
    : m_backingStore(backingStore)
    , m_id(InvalidId)
    , m_name(name)
    , m_version("")
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(coordinator)
{
    ASSERT(!m_name.isNull());
    openInternal();
}

void IDBDatabaseBackendImpl::openInternal()
{
    bool success = m_backingStore->getIDBDatabaseMetaData(m_name, m_version, m_id);
    ASSERT(success == (m_id != InvalidId));
    if (success) {
        loadObjectStores();
        return;
    }
    if (!m_backingStore->createIDBDatabaseMetaData(m_name, m_version, m_id))
        ASSERT_NOT_REACHED(); // FIXME: Need better error handling.
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
    m_factory->removeIDBDatabaseBackend(m_identifier);
}

PassRefPtr<IDBBackingStore> IDBDatabaseBackendImpl::backingStore() const
{
    return m_backingStore;
}

PassRefPtr<DOMStringList> IDBDatabaseBackendImpl::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    objectStoreNames->sort();
    return objectStoreNames.release();
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(transactionPtr->mode() == IDBTransaction::VERSION_CHANGE);

    if (m_objectStores.contains(name)) {
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = IDBObjectStoreBackendImpl::create(m_backingStore.get(), m_id, name, keyPath, autoIncrement);
    ASSERT(objectStore->name() == name);

    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::createObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::removeObjectStoreFromMap, database, objectStore))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    m_objectStores.set(name, objectStore);
    return objectStore.release();
}

void IDBDatabaseBackendImpl::createObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore,  PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    int64_t objectStoreId;

    if (!database->m_backingStore->createObjectStore(database->id(), objectStore->name(), objectStore->keyPath(), objectStore->autoIncrement(), objectStoreId)) {
        transaction->abort();
        return;
    }

    objectStore->setId(objectStoreId);
    transaction->didCompleteTaskEvents();
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::objectStore(const String& name)
{
    return m_objectStores.get(name);
}

void IDBDatabaseBackendImpl::deleteObjectStore(const String& name, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = m_objectStores.get(name);
    if (!objectStore) {
        ec = IDBDatabaseException::NOT_FOUND_ERR;
        return;
    }
    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::deleteObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::addObjectStoreToMap, database, objectStore))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }
    m_objectStores.remove(name);
}

void IDBDatabaseBackendImpl::deleteObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    database->m_backingStore->deleteObjectStore(database->id(), objectStore->id());
    transaction->didCompleteTaskEvents();
}

void IDBDatabaseBackendImpl::setVersion(const String& version, PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, ExceptionCode& ec)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    if (!m_databaseCallbacksSet.contains(databaseCallbacks)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ABORT_ERR, "Connection was closed before set version transaction was created"));
        return;
    }
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(version);
    }
    // FIXME: Only fire onBlocked if there are open connections after the
    // VersionChangeEvents are received, not just set up to fire.
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (m_databaseCallbacksSet.size() > 1) {
        callbacks->onBlocked();
        RefPtr<PendingSetVersionCall> pendingSetVersionCall = PendingSetVersionCall::create(version, callbacks, databaseCallbacks);
        m_pendingSetVersionCalls.append(pendingSetVersionCall);
        return;
    }
    if (m_runningVersionChangeTransaction) {
        RefPtr<PendingSetVersionCall> pendingSetVersionCall = PendingSetVersionCall::create(version, callbacks, databaseCallbacks);
        m_pendingSetVersionCalls.append(pendingSetVersionCall);
        return;
    }

    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = IDBTransactionBackendImpl::create(objectStoreNames.get(), IDBTransaction::VERSION_CHANGE, this);
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::setVersionInternal, database, version, callbacks, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::resetVersion, database, m_version))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
    }
}

void IDBDatabaseBackendImpl::setVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    int64_t databaseId = database->id();
    database->m_version = version;
    if (!database->m_backingStore->updateIDBDatabaseMetaData(databaseId, database->m_version)) {
        // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }
    callbacks->onSuccess(transaction);
}

void IDBDatabaseBackendImpl::transactionStarted(PassRefPtr<IDBTransactionBackendInterface> prpTransaction)
{
    RefPtr<IDBTransactionBackendInterface> transaction = prpTransaction;
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE) {
        ASSERT(!m_runningVersionChangeTransaction);
        m_runningVersionChangeTransaction = transaction;
    }
}

void IDBDatabaseBackendImpl::transactionFinished(PassRefPtr<IDBTransactionBackendInterface> prpTransaction)
{
    RefPtr<IDBTransactionBackendInterface> transaction = prpTransaction;
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE) {
        ASSERT(transaction.get() == m_runningVersionChangeTransaction.get());
        m_runningVersionChangeTransaction.clear();
        processPendingCalls();
    }
}

void IDBDatabaseBackendImpl::processPendingCalls()
{
    // Pending calls may be requeued or aborted
    Deque<RefPtr<PendingSetVersionCall> > pendingSetVersionCalls;
    m_pendingSetVersionCalls.swap(pendingSetVersionCalls);
    while (!pendingSetVersionCalls.isEmpty()) {
        ExceptionCode ec = 0;
        RefPtr<PendingSetVersionCall> pendingSetVersionCall = pendingSetVersionCalls.takeFirst();
        setVersion(pendingSetVersionCall->version(), pendingSetVersionCall->callbacks(), pendingSetVersionCall->databaseCallbacks(), ec);
        ASSERT(!ec);
    }

    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty())
        return;

    // Pending calls may be requeued.
    Deque<RefPtr<PendingDeleteCall> > pendingDeleteCalls;
    m_pendingDeleteCalls.swap(pendingDeleteCalls);
    while (!pendingDeleteCalls.isEmpty()) {
        RefPtr<PendingDeleteCall> pendingDeleteCall = pendingDeleteCalls.takeFirst();
        deleteDatabase(pendingDeleteCall->callbacks());
    }

    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty() || !m_pendingDeleteCalls.isEmpty())
        return;

    while (!m_pendingOpenCalls.isEmpty()) {
        RefPtr<PendingOpenCall> pendingOpenCall = m_pendingOpenCalls.takeFirst();
        openConnection(pendingOpenCall->callbacks());
    }
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::transaction(DOMStringList* objectStoreNames, unsigned short mode, ExceptionCode& ec)
{
    for (size_t i = 0; i < objectStoreNames->length(); ++i) {
        if (!m_objectStores.contains(objectStoreNames->item(i))) {
            ec = IDBDatabaseException::NOT_FOUND_ERR;
            return 0;
        }
    }

    // FIXME: Return not allowed err if close has been called.
    return IDBTransactionBackendImpl::create(objectStoreNames, mode, this);
}

void IDBDatabaseBackendImpl::open(PassRefPtr<IDBDatabaseCallbacks> callbacks)
{
    m_databaseCallbacksSet.add(RefPtr<IDBDatabaseCallbacks>(callbacks));
}

void IDBDatabaseBackendImpl::openConnection(PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty())
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks));
    else {
        if (m_id == InvalidId)
            openInternal();
        callbacks->onSuccess(this);
    }
}

void IDBDatabaseBackendImpl::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty()) {
        m_pendingDeleteCalls.append(PendingDeleteCall::create(prpCallbacks));
        return;
    }
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    // FIXME: Only fire onVersionChange if there the connection isn't in
    // the process of closing.
    // https://bugs.webkit.org/show_bug.cgi?id=71129
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it)
        (*it)->onVersionChange("");
    // FIXME: Only fire onBlocked if there are open connections after the
    // VersionChangeEvents are received, not just set up to fire.
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (!m_databaseCallbacksSet.isEmpty()) {
        m_pendingDeleteCalls.append(PendingDeleteCall::create(callbacks));
        callbacks->onBlocked();
        return;
    }
    if (!m_backingStore->deleteDatabase(m_name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }
    m_version = "";
    m_id = InvalidId;
    m_objectStores.clear();
    callbacks->onSuccess(SerializedScriptValue::nullValue());
}

void IDBDatabaseBackendImpl::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
{
    RefPtr<IDBDatabaseCallbacks> callbacks = prpCallbacks;
    ASSERT(m_databaseCallbacksSet.contains(callbacks));
    m_databaseCallbacksSet.remove(callbacks);
    if (m_databaseCallbacksSet.size() > 1)
        return;

    processPendingCalls();
}

void IDBDatabaseBackendImpl::loadObjectStores()
{
    Vector<int64_t> ids;
    Vector<String> names;
    Vector<String> keyPaths;
    Vector<bool> autoIncrementFlags;
    m_backingStore->getObjectStores(m_id, ids, names, keyPaths, autoIncrementFlags);

    ASSERT(names.size() == ids.size());
    ASSERT(keyPaths.size() == ids.size());
    ASSERT(autoIncrementFlags.size() == ids.size());

    for (size_t i = 0; i < ids.size(); i++)
        m_objectStores.set(names[i], IDBObjectStoreBackendImpl::create(m_backingStore.get(), m_id, ids[i], names[i], keyPaths[i], autoIncrementFlags[i]));
}

void IDBDatabaseBackendImpl::removeObjectStoreFromMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    ASSERT(database->m_objectStores.contains(objectStore->name()));
    database->m_objectStores.remove(objectStore->name());
}

void IDBDatabaseBackendImpl::addObjectStoreToMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStorePtr = objectStore;
    ASSERT(!database->m_objectStores.contains(objectStorePtr->name()));
    database->m_objectStores.set(objectStorePtr->name(), objectStorePtr);
}

void IDBDatabaseBackendImpl::resetVersion(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version)
{
    database->m_version = version;
}


} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
