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

class IDBDatabaseBackendImpl::PendingOpenCall {
public:
    static PassOwnPtr<PendingOpenCall> create(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptPtr(new PendingOpenCall(callbacks, databaseCallbacks));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }
    PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks() { return m_databaseCallbacks; }

private:
    PendingOpenCall(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }

    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

class IDBDatabaseBackendImpl::PendingOpenWithVersionCall {
public:
    static PassOwnPtr<PendingOpenWithVersionCall> create(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t version)
    {
        return adoptPtr(new PendingOpenWithVersionCall(callbacks, databaseCallbacks, version));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }
    PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks() { return m_databaseCallbacks; }
    int64_t version() { return m_version; }

private:
    PendingOpenWithVersionCall(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t version)
        : m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
        , m_version(version)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
    int64_t m_version;
};

class IDBDatabaseBackendImpl::PendingDeleteCall {
public:
    static PassOwnPtr<PendingDeleteCall> create(PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new PendingDeleteCall(callbacks));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }

private:
    PendingDeleteCall(PassRefPtr<IDBCallbacks> callbacks)
        : m_callbacks(callbacks)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBDatabaseBackendImpl::PendingSetVersionCall {
public:
    static PassOwnPtr<PendingSetVersionCall> create(const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptPtr(new PendingSetVersionCall(version, callbacks, databaseCallbacks));
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

PassRefPtr<IDBDatabaseBackendImpl> IDBDatabaseBackendImpl::create(const String& name, IDBBackingStore* database, IDBFactoryBackendImpl* factory, const String& uniqueIdentifier)
{
    RefPtr<IDBDatabaseBackendImpl> backend = adoptRef(new IDBDatabaseBackendImpl(name, database, factory, uniqueIdentifier));
    if (!backend->openInternal())
        return 0;
    return backend.release();
}

namespace {
const char* NoStringVersion = "";
}

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, IDBBackingStore* backingStore, IDBFactoryBackendImpl* factory, const String& uniqueIdentifier)
    : m_backingStore(backingStore)
    , m_id(InvalidId)
    , m_name(name)
    , m_version(NoStringVersion)
    , m_intVersion(IDBDatabaseMetadata::NoIntVersion)
    , m_maxObjectStoreId(InvalidId)
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(IDBTransactionCoordinator::create())
    , m_closingConnection(false)
{
    ASSERT(!m_name.isNull());
}

bool IDBDatabaseBackendImpl::openInternal()
{
    bool success = m_backingStore->getIDBDatabaseMetaData(m_name, m_version, m_intVersion, m_id, m_maxObjectStoreId);
    ASSERT_WITH_MESSAGE(success == (m_id != InvalidId), "success = %s, m_id = %lld", success ? "true" : "false", static_cast<long long>(m_id));
    if (success) {
        loadObjectStores();
        return true;
    }
    return m_backingStore->createIDBDatabaseMetaData(m_name, m_version, m_intVersion, m_id);
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
}

PassRefPtr<IDBBackingStore> IDBDatabaseBackendImpl::backingStore() const
{
    return m_backingStore;
}

IDBDatabaseMetadata IDBDatabaseBackendImpl::metadata() const
{
    IDBDatabaseMetadata metadata(m_name, m_id, m_version, m_intVersion, m_maxObjectStoreId);
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        metadata.objectStores.set(it->key, it->value->metadata());
    return metadata;
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::createObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    return createObjectStore(AutogenerateObjectStoreId, name, keyPath, autoIncrement, transactionPtr, ec);
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::createObjectStore(int64_t id, const String& name, const IDBKeyPath& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(!m_objectStores.contains(name));

    COMPILE_ASSERT(AutogenerateObjectStoreId == IDBBackingStore::AutogenerateObjectStoreId, AutogenerateObjectStoreIdMatches);
    RefPtr<IDBObjectStoreBackendImpl> objectStore = IDBObjectStoreBackendImpl::create(this, id, name, keyPath, autoIncrement, IDBObjectStoreBackendInterface::MinimumIndexId);
    ASSERT(objectStore->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    // FIXME: Fix edge cases around transaction aborts that prevent this from just being ASSERT(id == m_maxObjectStoreId + 1)
    ASSERT(id == AutogenerateObjectStoreId || id > m_maxObjectStoreId);
    m_maxObjectStoreId = id;

    RefPtr<IDBDatabaseBackendImpl> database = this;
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBDatabaseBackendImpl::createObjectStoreInternal, database, objectStore, transaction),
            createCallbackTask(&IDBDatabaseBackendImpl::removeObjectStoreFromMap, database, objectStore))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return 0;
    }

    m_objectStores.set(name, objectStore);
    return objectStore.release();
}

void IDBDatabaseBackendImpl::createObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    int64_t objectStoreId;

    if (!database->m_backingStore->createObjectStore(transaction->backingStoreTransaction(), database->id(), objectStore->id(), objectStore->name(), objectStore->keyPath(), objectStore->autoIncrement(), objectStoreId)) {
        transaction->abort();
        return;
    }

    // FIXME: Remove this when switch to front-end ID management is complete: https://bugs.webkit.org/show_bug.cgi?id=98085
    ASSERT(objectStore->id() == AutogenerateObjectStoreId || objectStore->id() == objectStoreId);
    objectStore->setId(objectStoreId);
    transaction->didCompleteTaskEvents();
}

PassRefPtr<IDBObjectStoreBackendImpl> IDBDatabaseBackendImpl::objectStore(const String& name)
{
    return m_objectStores.get(name);
}

void IDBDatabaseBackendImpl::deleteObjectStore(const String& name, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(m_objectStores.contains(name));

    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBObjectStoreBackendImpl> objectStore = m_objectStores.get(name);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    if (!transaction->scheduleTask(
            createCallbackTask(&IDBDatabaseBackendImpl::deleteObjectStoreInternal, database, objectStore, transaction),
            createCallbackTask(&IDBDatabaseBackendImpl::addObjectStoreToMap, database, objectStore))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }
    m_objectStores.remove(name);
}

void IDBDatabaseBackendImpl::deleteObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    database->m_backingStore->deleteObjectStore(transaction->backingStoreTransaction(), database->id(), objectStore->id());
    transaction->didCompleteTaskEvents();
}

void IDBDatabaseBackendImpl::setVersion(const String& version, PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, ExceptionCode& ec)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    if (!m_databaseCallbacksSet.contains(databaseCallbacks)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR, "Connection was closed before set version transaction was created"));
        return;
    }
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(version);
    }
    // FIXME: Only fire onBlocked if there are open connections after the
    // VersionChangeEvents are received, not just set up to fire.
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount() > 1) {
        callbacks->onBlocked();
        OwnPtr<PendingSetVersionCall> pendingSetVersionCall = PendingSetVersionCall::create(version, callbacks, databaseCallbacks);
        m_pendingSetVersionCalls.append(pendingSetVersionCall.release());
        return;
    }
    if (m_runningVersionChangeTransaction) {
        OwnPtr<PendingSetVersionCall> pendingSetVersionCall = PendingSetVersionCall::create(version, callbacks, databaseCallbacks);
        m_pendingSetVersionCalls.append(pendingSetVersionCall.release());
        return;
    }

    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    RefPtr<IDBTransactionBackendInterface> transactionInterface = this->transaction(objectStoreNames.get(), IDBTransaction::VERSION_CHANGE, ec);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionInterface.get());
    ASSERT(!ec);

    RefPtr<IDBDatabaseBackendImpl> database = this;
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBDatabaseBackendImpl::setVersionInternal, database, version, callbacks, transaction),
            createCallbackTask(&IDBDatabaseBackendImpl::resetVersion, database, m_version, m_intVersion))) {
        // FIXME: Remove one of the following lines.
        ASSERT_NOT_REACHED();
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
    }
}

void IDBDatabaseBackendImpl::setVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> prpTransaction)
{
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    int64_t databaseId = database->id();
    database->m_version = version;
    database->m_intVersion = IDBDatabaseMetadata::NoIntVersion;
    if (!database->m_backingStore->updateIDBDatabaseMetaData(transaction->backingStoreTransaction(), databaseId, database->m_version) || !database->m_backingStore->updateIDBDatabaseIntVersion(transaction->backingStoreTransaction(), databaseId, database->m_intVersion)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }
    callbacks->onSuccess(PassRefPtr<IDBTransactionBackendInterface>(transaction));
}

void IDBDatabaseBackendImpl::setIntVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    int64_t databaseId = database->id();
    int64_t oldVersion = database->m_intVersion;
    ASSERT(version > oldVersion);
    database->m_intVersion = version;
    if (!database->m_backingStore->updateIDBDatabaseIntVersion(transaction->backingStoreTransaction(), databaseId, database->m_intVersion)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }
    callbacks->onUpgradeNeeded(oldVersion, transaction, database);
}

void IDBDatabaseBackendImpl::transactionStarted(PassRefPtr<IDBTransactionBackendImpl> prpTransaction)
{
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE) {
        ASSERT(!m_runningVersionChangeTransaction);
        m_runningVersionChangeTransaction = transaction;
    }
}

void IDBDatabaseBackendImpl::transactionFinished(PassRefPtr<IDBTransactionBackendImpl> prpTransaction)
{
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    ASSERT(m_transactions.contains(transaction.get()));
    m_transactions.remove(transaction.get());
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE) {
        ASSERT(transaction.get() == m_runningVersionChangeTransaction.get());
        m_runningVersionChangeTransaction.clear();
    }
}

void IDBDatabaseBackendImpl::transactionFinishedAndAbortFired(PassRefPtr<IDBTransactionBackendImpl> prpTransaction)
{
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE) {
        // If this was an open-with-version call, there will be a "second
        // half" open call waiting for us in processPendingCalls.
        // FIXME: When we no longer support setVersion, assert such a thing.
        if (m_pendingSecondHalfOpenWithVersion) {
            m_pendingSecondHalfOpenWithVersion->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR, "Version change transaction was aborted in upgradeneeded event handler."));
            m_pendingSecondHalfOpenWithVersion.release();
        }
        processPendingCalls();
    }
}

void IDBDatabaseBackendImpl::transactionFinishedAndCompleteFired(PassRefPtr<IDBTransactionBackendImpl> prpTransaction)
{
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    if (transaction->mode() == IDBTransaction::VERSION_CHANGE)
        processPendingCalls();
}

size_t IDBDatabaseBackendImpl::connectionCount()
{
    // This does not include pending open calls, as those should not block version changes and deletes.
    return m_databaseCallbacksSet.size();
}

void IDBDatabaseBackendImpl::processPendingCalls()
{
    if (m_pendingSecondHalfOpenWithVersion) {
        ASSERT(m_pendingSecondHalfOpenWithVersion->version() == m_intVersion);
        ASSERT(m_id != InvalidId);
        m_pendingSecondHalfOpenWithVersion->callbacks()->onSuccess(this);
        m_pendingSecondHalfOpenWithVersion.release();
        // Fall through when complete, as pending deletes may be (partially) unblocked.
    }

    // Pending calls may be requeued or aborted
    Deque<OwnPtr<PendingSetVersionCall> > pendingSetVersionCalls;
    m_pendingSetVersionCalls.swap(pendingSetVersionCalls);
    while (!pendingSetVersionCalls.isEmpty()) {
        ExceptionCode ec = 0;
        OwnPtr<PendingSetVersionCall> pendingSetVersionCall = pendingSetVersionCalls.takeFirst();
        setVersion(pendingSetVersionCall->version(), pendingSetVersionCall->callbacks(), pendingSetVersionCall->databaseCallbacks(), ec);
        ASSERT(!ec);
    }

    // If there were any pending set version calls, we better have started one.
    ASSERT(m_pendingSetVersionCalls.isEmpty() || m_runningVersionChangeTransaction);

    // m_pendingSetVersionCalls is non-empty in two cases:
    // 1) When two versionchange transactions are requested while another
    //    version change transaction is running.
    // 2) When three versionchange transactions are requested in a row, before
    //    any of their event handlers are run.
    // Note that this check is only an optimization to reduce queue-churn and
    // not necessary for correctness; deleteDatabase and openConnection will
    // requeue their calls if this condition is true.
    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty())
        return;

    // Pending calls may be requeued.
    Deque<OwnPtr<PendingDeleteCall> > pendingDeleteCalls;
    m_pendingDeleteCalls.swap(pendingDeleteCalls);
    while (!pendingDeleteCalls.isEmpty()) {
        OwnPtr<PendingDeleteCall> pendingDeleteCall = pendingDeleteCalls.takeFirst();
        deleteDatabase(pendingDeleteCall->callbacks());
    }

    // This check is also not really needed, openConnection would just requeue its calls.
    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty() || !m_pendingDeleteCalls.isEmpty())
        return;

    Deque<OwnPtr<PendingOpenWithVersionCall> > pendingOpenWithVersionCalls;
    m_pendingOpenWithVersionCalls.swap(pendingOpenWithVersionCalls);
    while (!pendingOpenWithVersionCalls.isEmpty()) {
        OwnPtr<PendingOpenWithVersionCall> pendingOpenWithVersionCall = pendingOpenWithVersionCalls.takeFirst();
        openConnectionWithVersion(pendingOpenWithVersionCall->callbacks(), pendingOpenWithVersionCall->databaseCallbacks(), pendingOpenWithVersionCall->version());
    }

    // Given the check above, it appears that calls cannot be requeued by
    // openConnection, but use a different queue for iteration to be safe.
    Deque<OwnPtr<PendingOpenCall> > pendingOpenCalls;
    m_pendingOpenCalls.swap(pendingOpenCalls);
    while (!pendingOpenCalls.isEmpty()) {
        OwnPtr<PendingOpenCall> pendingOpenCall = pendingOpenCalls.takeFirst();
        openConnection(pendingOpenCall->callbacks(), pendingOpenCall->databaseCallbacks());
    }
    ASSERT(m_pendingOpenCalls.isEmpty());
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::transaction(DOMStringList* objectStoreNames, unsigned short mode, ExceptionCode& ec)
{
    for (size_t i = 0; i < objectStoreNames->length(); ++i) {
        if (!m_objectStores.contains(objectStoreNames->item(i))) {
            ec = IDBDatabaseException::IDB_NOT_FOUND_ERR;
            return 0;
        }
    }

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::create(objectStoreNames, mode, this);
    m_transactions.add(transaction.get());
    return transaction.release();
}

void IDBDatabaseBackendImpl::openConnection(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
{
    ASSERT(m_backingStore.get());
    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty()) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks));
        return;
    }
    if (m_id == InvalidId && !openInternal()) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }
    if (m_version == NoStringVersion && m_intVersion == IDBDatabaseMetadata::NoIntVersion) {
        // Spec says: If no version is specified and no database exists, set
        // database version to 1. We infer that the database didn't exist from
        // its lack of either type of version.
        openConnectionWithVersion(callbacks, databaseCallbacks, 1);
        return;
    }
    m_databaseCallbacksSet.add(RefPtr<IDBDatabaseCallbacks>(databaseCallbacks));
    callbacks->onSuccess(this);
}

void IDBDatabaseBackendImpl::runIntVersionChangeTransaction(int64_t requestedVersion, PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    ASSERT(callbacks);
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(m_intVersion, requestedVersion);
    }
    // The spec dictates we wait until all the version change events are
    // delivered and then check m_databaseCallbacks.empty() before proceeding
    // or firing a blocked event, but instead we should be consistent with how
    // the old setVersion (incorrectly) did it.
    // FIXME: Remove the call to onBlocked and instead wait until the frontend
    // tells us that all the blocked events have been delivered. See
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount())
        callbacks->onBlocked(m_intVersion);
    // FIXME: Add test for m_runningVersionChangeTransaction.
    if (m_runningVersionChangeTransaction || connectionCount()) {
        m_pendingOpenWithVersionCalls.append(PendingOpenWithVersionCall::create(callbacks, databaseCallbacks, requestedVersion));
        return;
    }

    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    ExceptionCode ec = 0;
    RefPtr<IDBTransactionBackendInterface> transactionInterface = transaction(objectStoreNames.get(), IDBTransaction::VERSION_CHANGE, ec);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionInterface.get());
    ASSERT(!ec);

    RefPtr<IDBDatabaseBackendImpl> database = this;
    OwnPtr<ScriptExecutionContext::Task> intVersionTask = createCallbackTask(&IDBDatabaseBackendImpl::setIntVersionInternal, database, requestedVersion, callbacks, transaction);
    OwnPtr<ScriptExecutionContext::Task> resetVersionOnAbortTask = createCallbackTask(&IDBDatabaseBackendImpl::resetVersion, database, m_version, m_intVersion);
    if (!transaction->scheduleTask(intVersionTask.release(), resetVersionOnAbortTask.release())) {
        // FIXME: Remove one of the following lines.
        ASSERT_NOT_REACHED();
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
    }
    ASSERT(!m_pendingSecondHalfOpenWithVersion);
    m_pendingSecondHalfOpenWithVersion = PendingOpenWithVersionCall::create(callbacks, databaseCallbacks, requestedVersion);
    m_databaseCallbacksSet.add(databaseCallbacks);
}

void IDBDatabaseBackendImpl::openConnectionWithVersion(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t version)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty()) {
        m_pendingOpenWithVersionCalls.append(PendingOpenWithVersionCall::create(callbacks, databaseCallbacks, version));
        return;
    }
    if (m_id == InvalidId) {
        if (openInternal())
            ASSERT(m_intVersion == IDBDatabaseMetadata::NoIntVersion);
        else {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
            return;
        }
    }
    if (version > m_intVersion) {
        runIntVersionChangeTransaction(version, callbacks, databaseCallbacks);
        return;
    }
    if (version < m_intVersion) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::VER_ERR, String::format("The requested version (%lld) is less than the existing version (%lld).", static_cast<long long>(version), static_cast<long long>(m_intVersion))));
        return;
    }
    ASSERT(version == m_intVersion);
    m_databaseCallbacksSet.add(databaseCallbacks);
    callbacks->onSuccess(this);
}

void IDBDatabaseBackendImpl::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    if (m_runningVersionChangeTransaction || !m_pendingSetVersionCalls.isEmpty()) {
        m_pendingDeleteCalls.append(PendingDeleteCall::create(prpCallbacks));
        return;
    }
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        (*it)->onVersionChange(NoStringVersion);
    }
    // FIXME: Only fire onBlocked if there are open connections after the
    // VersionChangeEvents are received, not just set up to fire.
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount()) {
        m_pendingDeleteCalls.append(PendingDeleteCall::create(callbacks));
        callbacks->onBlocked();
        return;
    }
    ASSERT(m_backingStore);
    if (!m_backingStore->deleteDatabase(m_name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }
    m_version = NoStringVersion;
    m_id = InvalidId;
    m_intVersion = IDBDatabaseMetadata::NoIntVersion;
    m_objectStores.clear();
    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

void IDBDatabaseBackendImpl::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
{
    RefPtr<IDBDatabaseCallbacks> callbacks = prpCallbacks;
    ASSERT(m_databaseCallbacksSet.contains(callbacks));

    m_databaseCallbacksSet.remove(callbacks);
    if (m_pendingSecondHalfOpenWithVersion && m_pendingSecondHalfOpenWithVersion->databaseCallbacks() == callbacks) {
        m_pendingSecondHalfOpenWithVersion->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR, "The connection was closed."));
        m_pendingSecondHalfOpenWithVersion.release();
    }

    if (connectionCount() > 1)
        return;

    // processPendingCalls allows the inspector to process a pending open call
    // and call close, reentering IDBDatabaseBackendImpl::close. Then the
    // backend would be removed both by the inspector closing its connection, and
    // by the connection that first called close.
    // To avoid that situation, don't proceed in case of reentrancy.
    if (m_closingConnection)
        return;
    m_closingConnection = true;
    processPendingCalls();

    // FIXME: Add a test for the m_pendingOpenCalls and m_pendingOpenWithVersionCalls cases below.
    if (!connectionCount() && !m_pendingOpenCalls.size() && !m_pendingOpenWithVersionCalls.size() && !m_pendingDeleteCalls.size()) {
        TransactionSet transactions(m_transactions);
        for (TransactionSet::const_iterator it = transactions.begin(); it != transactions.end(); ++it)
            (*it)->abort();

        ASSERT(m_transactions.isEmpty());

        m_backingStore.clear();
        // This check should only be false in tests.
        if (m_factory)
            m_factory->removeIDBDatabaseBackend(m_identifier);
    }
    m_closingConnection = false;
}

void IDBDatabaseBackendImpl::loadObjectStores()
{
    Vector<int64_t> ids;
    Vector<String> names;
    Vector<IDBKeyPath> keyPaths;
    Vector<bool> autoIncrementFlags;
    Vector<int64_t> maxIndexIds;
    m_backingStore->getObjectStores(m_id, ids, names, keyPaths, autoIncrementFlags, maxIndexIds);

    ASSERT(names.size() == ids.size());
    ASSERT(keyPaths.size() == ids.size());
    ASSERT(autoIncrementFlags.size() == ids.size());
    ASSERT(maxIndexIds.size() == ids.size());

    for (size_t i = 0; i < ids.size(); i++)
        m_objectStores.set(names[i], IDBObjectStoreBackendImpl::create(this, ids[i], names[i], keyPaths[i], autoIncrementFlags[i], maxIndexIds[i]));
}

void IDBDatabaseBackendImpl::removeObjectStoreFromMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> prpObjectStore)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = prpObjectStore;
    ASSERT(database->m_objectStores.contains(objectStore->name()));
    database->m_objectStores.remove(objectStore->name());
}

void IDBDatabaseBackendImpl::addObjectStoreToMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStorePtr = objectStore;
    ASSERT(!database->m_objectStores.contains(objectStorePtr->name()));
    database->m_objectStores.set(objectStorePtr->name(), objectStorePtr);
}

void IDBDatabaseBackendImpl::resetVersion(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& previousVersion, int64_t previousIntVersion)
{
    database->m_version = previousVersion;
    database->m_intVersion = previousIntVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
