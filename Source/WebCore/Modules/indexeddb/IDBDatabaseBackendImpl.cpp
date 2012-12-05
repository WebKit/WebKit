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

#include "IDBBackingStore.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"
#include "IDBTransactionCoordinator.h"

namespace WebCore {

class IDBDatabaseBackendImpl::CreateObjectStoreOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
    {
        return adoptPtr(new CreateObjectStoreOperation(database, objectStore));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateObjectStoreOperation(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
        : m_database(database)
        , m_objectStore(objectStore)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
};

class IDBDatabaseBackendImpl::DeleteObjectStoreOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
    {
        return adoptPtr(new DeleteObjectStoreOperation(database, objectStore));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteObjectStoreOperation(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
        : m_database(database)
        , m_objectStore(objectStore)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
};

class IDBDatabaseBackendImpl::VersionChangeOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptPtr(new VersionChangeOperation(database, version, callbacks, databaseCallbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    VersionChangeOperation(PassRefPtr<IDBDatabaseBackendImpl> database, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_database(database)
        , m_version(version)
        , m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    int64_t m_version;
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

class IDBDatabaseBackendImpl::CreateObjectStoreAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
    {
        return adoptPtr(new CreateObjectStoreAbortOperation(database, objectStore));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateObjectStoreAbortOperation(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
        : m_database(database)
        , m_objectStore(objectStore)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
};

class IDBDatabaseBackendImpl::DeleteObjectStoreAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
    {
        return adoptPtr(new DeleteObjectStoreAbortOperation(database, objectStore));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteObjectStoreAbortOperation(PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
        : m_database(database)
        , m_objectStore(objectStore)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
};

class IDBDatabaseBackendImpl::VersionChangeAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBDatabaseBackendImpl> database, const String& previousVersion, int64_t previousIntVersion)
    {
        return adoptPtr(new VersionChangeAbortOperation(database, previousVersion, previousIntVersion));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    VersionChangeAbortOperation(PassRefPtr<IDBDatabaseBackendImpl> database, const String& previousVersion, int64_t previousIntVersion)
        : m_database(database)
        , m_previousVersion(previousVersion)
        , m_previousIntVersion(previousIntVersion)
    {
    }

    RefPtr<IDBDatabaseBackendImpl> m_database;
    String m_previousVersion;
    int64_t m_previousIntVersion;
};


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
    , m_metadata(name, InvalidId, NoStringVersion, IDBDatabaseMetadata::NoIntVersion, InvalidId)
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(IDBTransactionCoordinator::create())
    , m_closingConnection(false)
{
    ASSERT(!m_metadata.name.isNull());
}

bool IDBDatabaseBackendImpl::openInternal()
{
    bool success = m_backingStore->getIDBDatabaseMetaData(m_metadata.name, &m_metadata);
    ASSERT_WITH_MESSAGE(success == (m_metadata.id != InvalidId), "success = %s, m_id = %lld", success ? "true" : "false", static_cast<long long>(m_metadata.id));
    if (success) {
        loadObjectStores();
        return true;
    }
    return m_backingStore->createIDBDatabaseMetaData(m_metadata.name, m_metadata.version, m_metadata.intVersion, m_metadata.id);
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
    // FIXME: Figure out a way to keep m_metadata.objectStores.get(N).indexes up to date rather than regenerating this every time.
    IDBDatabaseMetadata metadata(m_metadata);
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        metadata.objectStores.set(it->value->id(), it->value->metadata());
    return metadata;
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::createObjectStore(int64_t id, const String& name, const IDBKeyPath& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(!m_objectStores.contains(id));

    RefPtr<IDBObjectStoreBackendImpl> objectStore = IDBObjectStoreBackendImpl::create(this, IDBObjectStoreMetadata(name, id, keyPath, autoIncrement, IDBObjectStoreBackendInterface::MinimumIndexId));
    ASSERT(objectStore->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    // FIXME: Fix edge cases around transaction aborts that prevent this from just being ASSERT(id == m_metadata.maxObjectStoreId + 1)
    ASSERT(id > m_metadata.maxObjectStoreId);
    m_metadata.maxObjectStoreId = id;

    if (!transaction->scheduleTask(CreateObjectStoreOperation::create(this, objectStore), CreateObjectStoreAbortOperation::create(this, objectStore))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }

    m_objectStores.set(id, objectStore);
    return objectStore.release();
}

void IDBDatabaseBackendImpl::CreateObjectStoreOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("CreateObjectStoreOperation");
    if (!m_database->m_backingStore->createObjectStore(transaction->backingStoreTransaction(), m_database->id(), m_objectStore->id(), m_objectStore->name(), m_objectStore->keyPath(), m_objectStore->autoIncrement())) {
        transaction->abort();
        return;
    }
}

PassRefPtr<IDBObjectStoreBackendImpl> IDBDatabaseBackendImpl::objectStore(int64_t id)
{
    return m_objectStores.get(id);
}

void IDBDatabaseBackendImpl::deleteObjectStore(int64_t id, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(m_objectStores.contains(id));

    RefPtr<IDBObjectStoreBackendImpl> objectStore = m_objectStores.get(id);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    if (!transaction->scheduleTask(DeleteObjectStoreOperation::create(this, objectStore), DeleteObjectStoreAbortOperation::create(this, objectStore))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }
    m_objectStores.remove(id);
}

void IDBDatabaseBackendImpl::DeleteObjectStoreOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("DeleteObjectStoreOperation");
    m_database->m_backingStore->deleteObjectStore(transaction->backingStoreTransaction(), m_database->id(), m_objectStore->id());
}

void IDBDatabaseBackendImpl::VersionChangeOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("VersionChangeOperation");
    int64_t databaseId = m_database->id();
    int64_t oldVersion = m_database->m_metadata.intVersion;
    ASSERT(m_version > oldVersion);
    m_database->m_metadata.intVersion = m_version;
    if (!m_database->m_backingStore->updateIDBDatabaseIntVersion(transaction->backingStoreTransaction(), databaseId, m_database->m_metadata.intVersion)) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error writing data to stable storage.");
        m_callbacks->onError(error);
        transaction->abort(error);
        return;
    }
    ASSERT(!m_database->m_pendingSecondHalfOpenWithVersion);
    m_database->m_pendingSecondHalfOpenWithVersion = PendingOpenWithVersionCall::create(m_callbacks, m_databaseCallbacks, m_version);
    m_callbacks->onUpgradeNeeded(oldVersion, transaction, m_database);
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
            m_pendingSecondHalfOpenWithVersion->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Version change transaction was aborted in upgradeneeded event handler."));
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
        ASSERT(m_pendingSecondHalfOpenWithVersion->version() == m_metadata.intVersion);
        ASSERT(m_metadata.id != InvalidId);
        m_pendingSecondHalfOpenWithVersion->callbacks()->onSuccess(this);
        m_pendingSecondHalfOpenWithVersion.release();
        // Fall through when complete, as pending deletes may be (partially) unblocked.
    }

    // Note that this check is only an optimization to reduce queue-churn and
    // not necessary for correctness; deleteDatabase and openConnection will
    // requeue their calls if this condition is true.
    if (m_runningVersionChangeTransaction)
        return;

    // Pending calls may be requeued.
    Deque<OwnPtr<PendingDeleteCall> > pendingDeleteCalls;
    m_pendingDeleteCalls.swap(pendingDeleteCalls);
    while (!pendingDeleteCalls.isEmpty()) {
        OwnPtr<PendingDeleteCall> pendingDeleteCall = pendingDeleteCalls.takeFirst();
        deleteDatabase(pendingDeleteCall->callbacks());
    }

    // This check is also not really needed, openConnection would just requeue its calls.
    if (m_runningVersionChangeTransaction || !m_pendingDeleteCalls.isEmpty())
        return;

    Deque<OwnPtr<PendingOpenWithVersionCall> > pendingOpenWithVersionCalls;
    m_pendingOpenWithVersionCalls.swap(pendingOpenWithVersionCalls);
    while (!pendingOpenWithVersionCalls.isEmpty()) {
        OwnPtr<PendingOpenWithVersionCall> pendingOpenWithVersionCall = pendingOpenWithVersionCalls.takeFirst();
        openConnectionWithVersion(pendingOpenWithVersionCall->callbacks(), pendingOpenWithVersionCall->databaseCallbacks(), pendingOpenWithVersionCall->version());
    }

    // Open calls can be requeued if an openWithVersion call started a version
    // change transaction.
    Deque<OwnPtr<PendingOpenCall> > pendingOpenCalls;
    m_pendingOpenCalls.swap(pendingOpenCalls);
    while (!pendingOpenCalls.isEmpty()) {
        OwnPtr<PendingOpenCall> pendingOpenCall = pendingOpenCalls.takeFirst();
        openConnection(pendingOpenCall->callbacks(), pendingOpenCall->databaseCallbacks());
    }
}

// FIXME: Remove this method in https://bugs.webkit.org/show_bug.cgi?id=103923.
PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::createTransaction(int64_t transactionId, const Vector<int64_t>& objectStoreIds, unsigned short mode)
{
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::create(transactionId, objectStoreIds, mode, this);
    m_transactions.add(transaction.get());
    return transaction.release();
}

void IDBDatabaseBackendImpl::createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, unsigned short mode)
{
    ASSERT_NOT_REACHED();
}

void IDBDatabaseBackendImpl::openConnection(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
{
    ASSERT(m_backingStore.get());
    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks));
        return;
    }
    if (m_metadata.id == InvalidId && !openInternal()) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error."));
        return;
    }
    if (m_metadata.version == NoStringVersion && m_metadata.intVersion == IDBDatabaseMetadata::NoIntVersion) {
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
            (*it)->onVersionChange(m_metadata.intVersion, requestedVersion);
    }
    // The spec dictates we wait until all the version change events are
    // delivered and then check m_databaseCallbacks.empty() before proceeding
    // or firing a blocked event, but instead we should be consistent with how
    // the old setVersion (incorrectly) did it.
    // FIXME: Remove the call to onBlocked and instead wait until the frontend
    // tells us that all the blocked events have been delivered. See
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount())
        callbacks->onBlocked(m_metadata.intVersion);
    // FIXME: Add test for m_runningVersionChangeTransaction.
    if (m_runningVersionChangeTransaction || connectionCount()) {
        m_pendingOpenWithVersionCalls.append(PendingOpenWithVersionCall::create(callbacks, databaseCallbacks, requestedVersion));
        return;
    }

    Vector<int64_t> objectStoreIds;
    // FIXME: The transactionId needs to be piped in through IDBDatabaseBackendInterface::open().
    RefPtr<IDBTransactionBackendInterface> transactionInterface = createTransaction(0, objectStoreIds, IDBTransaction::VERSION_CHANGE);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionInterface.get());

    if (!transaction->scheduleTask(VersionChangeOperation::create(this, requestedVersion, callbacks, databaseCallbacks), VersionChangeAbortOperation::create(this, m_metadata.version, m_metadata.intVersion))) {
        ASSERT_NOT_REACHED();
    }
    ASSERT(!m_pendingSecondHalfOpenWithVersion);
    m_databaseCallbacksSet.add(databaseCallbacks);
}

void IDBDatabaseBackendImpl::openConnectionWithVersion(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t version)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction) {
        m_pendingOpenWithVersionCalls.append(PendingOpenWithVersionCall::create(callbacks, databaseCallbacks, version));
        return;
    }
    if (m_metadata.id == InvalidId) {
        if (openInternal())
            ASSERT(m_metadata.intVersion == IDBDatabaseMetadata::NoIntVersion);
        else {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error."));
            return;
        }
    }
    if (version > m_metadata.intVersion) {
        runIntVersionChangeTransaction(version, callbacks, databaseCallbacks);
        return;
    }
    if (version < m_metadata.intVersion) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::VersionError, String::format("The requested version (%lld) is less than the existing version (%lld).", static_cast<long long>(version), static_cast<long long>(m_metadata.intVersion))));
        return;
    }
    ASSERT(version == m_metadata.intVersion);
    m_databaseCallbacksSet.add(databaseCallbacks);
    callbacks->onSuccess(this);
}

void IDBDatabaseBackendImpl::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    if (m_runningVersionChangeTransaction) {
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
    if (!m_backingStore->deleteDatabase(m_metadata.name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error."));
        return;
    }
    m_metadata.version = NoStringVersion;
    m_metadata.id = InvalidId;
    m_metadata.intVersion = IDBDatabaseMetadata::NoIntVersion;
    m_objectStores.clear();
    callbacks->onSuccess();
}

void IDBDatabaseBackendImpl::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
{
    RefPtr<IDBDatabaseCallbacks> callbacks = prpCallbacks;
    ASSERT(m_databaseCallbacksSet.contains(callbacks));

    m_databaseCallbacksSet.remove(callbacks);
    if (m_pendingSecondHalfOpenWithVersion && m_pendingSecondHalfOpenWithVersion->databaseCallbacks() == callbacks) {
        m_pendingSecondHalfOpenWithVersion->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "The connection was closed."));
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
    Vector<IDBObjectStoreMetadata> objectStores = m_backingStore->getObjectStores(m_metadata.id);

    for (size_t i = 0; i < objectStores.size(); i++)
        m_objectStores.set(objectStores[i].id, IDBObjectStoreBackendImpl::create(this, objectStores[i]));
}

void IDBDatabaseBackendImpl::CreateObjectStoreAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(m_database->m_objectStores.contains(m_objectStore->id()));
    m_database->m_objectStores.remove(m_objectStore->id());
}

void IDBDatabaseBackendImpl::DeleteObjectStoreAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(!m_database->m_objectStores.contains(m_objectStore->id()));
    m_database->m_objectStores.set(m_objectStore->id(), m_objectStore);
}

void IDBDatabaseBackendImpl::VersionChangeAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    m_database->m_metadata.version = m_previousVersion;
    m_database->m_metadata.intVersion = m_previousIntVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
