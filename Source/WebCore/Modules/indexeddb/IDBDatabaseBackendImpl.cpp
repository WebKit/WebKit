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

#include "IDBBackingStoreInterface.h"
#include "IDBCursorBackendInterface.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBIndexWriter.h"
#include "IDBKeyRange.h"
#include "IDBRecordIdentifier.h"
#include "IDBTransactionBackendInterface.h"
#include "IDBTransactionCoordinator.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include <wtf/TemporaryChange.h>

namespace WebCore {

PassRefPtr<IDBDatabaseBackendImpl> IDBDatabaseBackendImpl::create(const String& name, IDBBackingStoreInterface* backingStore, IDBFactoryBackendInterface* factory, const String& uniqueIdentifier)
{
    RefPtr<IDBDatabaseBackendImpl> backend = adoptRef(new IDBDatabaseBackendImpl(name, backingStore, factory, uniqueIdentifier));
    if (!backend->openInternal())
        return 0;
    return backend.release();
}

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, IDBBackingStoreInterface* backingStore, IDBFactoryBackendInterface* factory, const String& uniqueIdentifier)
    : m_backingStore(backingStore)
    , m_metadata(name, InvalidId, 0, InvalidId)
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(IDBTransactionCoordinator::create())
    , m_closingConnection(false)
{
    ASSERT(!m_metadata.name.isNull());
}

void IDBDatabaseBackendImpl::addObjectStore(const IDBObjectStoreMetadata& objectStore, int64_t newMaxObjectStoreId)
{
    ASSERT(!m_metadata.objectStores.contains(objectStore.id));
    if (newMaxObjectStoreId != IDBObjectStoreMetadata::InvalidId) {
        ASSERT(m_metadata.maxObjectStoreId < newMaxObjectStoreId);
        m_metadata.maxObjectStoreId = newMaxObjectStoreId;
    }
    m_metadata.objectStores.set(objectStore.id, objectStore);
}

void IDBDatabaseBackendImpl::removeObjectStore(int64_t objectStoreId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    m_metadata.objectStores.remove(objectStoreId);
}

void IDBDatabaseBackendImpl::addIndex(int64_t objectStoreId, const IDBIndexMetadata& index, int64_t newMaxIndexId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(!objectStore.indexes.contains(index.id));
    objectStore.indexes.set(index.id, index);
    if (newMaxIndexId != IDBIndexMetadata::InvalidId) {
        ASSERT(objectStore.maxIndexId < newMaxIndexId);
        objectStore.maxIndexId = newMaxIndexId;
    }
    m_metadata.objectStores.set(objectStoreId, objectStore);
}

void IDBDatabaseBackendImpl::removeIndex(int64_t objectStoreId, int64_t indexId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    objectStore.indexes.remove(indexId);
    m_metadata.objectStores.set(objectStoreId, objectStore);
}

bool IDBDatabaseBackendImpl::openInternal()
{
    bool success = false;
    bool ok = m_backingStore->getIDBDatabaseMetaData(m_metadata.name, &m_metadata, success);
    ASSERT_WITH_MESSAGE(success == (m_metadata.id != InvalidId), "success = %s, m_id = %lld", success ? "true" : "false", static_cast<long long>(m_metadata.id));
    if (!ok)
        return false;
    if (success)
        return m_backingStore->getObjectStores(m_metadata.id, &m_metadata.objectStores);

    return m_backingStore->createIDBDatabaseMetaData(m_metadata.name, String::number(m_metadata.version), m_metadata.version, m_metadata.id);
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
}

IDBBackingStoreInterface* IDBDatabaseBackendImpl::backingStore() const
{
    return m_backingStore.get();
}

void IDBDatabaseBackendImpl::createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::createObjectStore");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(!m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStoreMetadata(name, objectStoreId, keyPath, autoIncrement, IDBDatabaseBackendInterface::MinimumIndexId);

    transaction->scheduleCreateObjectStoreOperation(objectStoreMetadata);
    addObjectStore(objectStoreMetadata, objectStoreId);
}

void IDBDatabaseBackendImpl::deleteObjectStore(int64_t transactionId, int64_t objectStoreId)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::deleteObjectStore");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    transaction->scheduleDeleteObjectStoreOperation(objectStoreMetadata);
    removeObjectStore(objectStoreId);
}

void IDBDatabaseBackendImpl::createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::createIndex");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(!objectStore.indexes.contains(indexId));
    const IDBIndexMetadata indexMetadata(name, indexId, keyPath, unique, multiEntry);

    transaction->scheduleCreateIndexOperation(objectStoreId, indexMetadata);

    addIndex(objectStoreId, indexMetadata, indexId);
}

void IDBDatabaseBackendImpl::deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::deleteIndex");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    const IDBIndexMetadata& indexMetadata = objectStore.indexes.get(indexId);

    transaction->scheduleDeleteIndexOperation(objectStoreId, indexMetadata);

    removeIndex(objectStoreId, indexId);
}

void IDBDatabaseBackendImpl::commit(int64_t transactionId)
{
    // The frontend suggests that we commit, but we may have previously initiated an abort, and so have disposed of the transaction. onAbort has already been dispatched to the frontend, so it will find out about that asynchronously.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->commit();
}

void IDBDatabaseBackendImpl::abort(int64_t transactionId)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort();
}

void IDBDatabaseBackendImpl::abort(int64_t transactionId, PassRefPtr<IDBDatabaseError> error)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort(error);
}

void IDBDatabaseBackendImpl::get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, bool keyOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::get");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleGetOperation(m_metadata, objectStoreId, indexId, keyRange, keyOnly ? IndexedDB::CursorKeyOnly : IndexedDB::CursorKeyAndValue, callbacks);
}

void IDBDatabaseBackendImpl::put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::put");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    const IDBObjectStoreMetadata objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStoreMetadata.autoIncrement || key.get());

    transaction->schedulePutOperation(objectStoreMetadata, value, key, putMode, callbacks, indexIds, indexKeys);
}

void IDBDatabaseBackendImpl::setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::setIndexKeys");
    ASSERT(prpPrimaryKey);

    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    RefPtr<IDBKey> primaryKey = prpPrimaryKey;
    RefPtr<IDBBackingStoreInterface> store = backingStore();
    // FIXME: This method could be asynchronous, but we need to evaluate if it's worth the extra complexity.
    RefPtr<IDBRecordIdentifier> recordIdentifier;
    bool ok = store->keyExistsInObjectStore(transaction->backingStoreTransaction(), m_metadata.id, objectStoreId, *primaryKey, recordIdentifier);
    if (!ok) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error setting index keys."));
        return;
    }
    if (!recordIdentifier) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error setting index keys for object store."));
        transaction->abort(error.release());
        return;
    }

    Vector<RefPtr<IDBIndexWriter>> indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);
    bool backingStoreSuccess = store->makeIndexWriters(*transaction, id(), objectStoreMetadata, *primaryKey, false, indexIds, indexKeys, indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IDBIndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier.get(), *store.get(), transaction->backingStoreTransaction(), id(), objectStoreId);
    }
}

void IDBDatabaseBackendImpl::setIndexesReady(int64_t transactionId, int64_t, const Vector<int64_t>& indexIds)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::setIndexesReady");

    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleSetIndexesReadyOperation(indexIds.size());
}

void IDBDatabaseBackendImpl::openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, bool keyOnly, TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::openCursor");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleOpenCursorOperation(objectStoreId, indexId, keyRange, direction, keyOnly ? IndexedDB::CursorKeyOnly : IndexedDB::CursorKeyAndValue, taskType, callbacks);
}

void IDBDatabaseBackendImpl::count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::count");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    transaction->scheduleCountOperation(objectStoreId, indexId, keyRange, callbacks);
}


void IDBDatabaseBackendImpl::deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::deleteRange");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    transaction->scheduleDeleteRangeOperation(objectStoreId, keyRange, callbacks);
}

void IDBDatabaseBackendImpl::clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendImpl::clear");
    IDBTransactionBackendInterface* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    transaction->scheduleClearOperation(objectStoreId, callbacks);
}

void IDBDatabaseBackendImpl::transactionStarted(IDBTransactionBackendInterface* transaction)
{
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        ASSERT(!m_runningVersionChangeTransaction);
        m_runningVersionChangeTransaction = transaction;
    }
}

void IDBDatabaseBackendImpl::transactionFinished(IDBTransactionBackendInterface* rawTransaction)
{
    RefPtr<IDBTransactionBackendInterface> transaction = rawTransaction;
    ASSERT(m_transactions.contains(transaction->id()));
    ASSERT(m_transactions.get(transaction->id()) == transaction.get());
    m_transactions.remove(transaction->id());
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        ASSERT(transaction.get() == m_runningVersionChangeTransaction.get());
        m_runningVersionChangeTransaction.clear();
    }
}

void IDBDatabaseBackendImpl::transactionFinishedAndAbortFired(IDBTransactionBackendInterface* rawTransaction)
{
    RefPtr<IDBTransactionBackendInterface> transaction = rawTransaction;
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        // If this was an open-with-version call, there will be a "second
        // half" open call waiting for us in processPendingCalls.
        // FIXME: When we no longer support setVersion, assert such a thing.
        if (m_pendingSecondHalfOpen) {
            m_pendingSecondHalfOpen->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Version change transaction was aborted in upgradeneeded event handler."));
            m_pendingSecondHalfOpen.release();
        }
        processPendingCalls();
    }
}

void IDBDatabaseBackendImpl::transactionFinishedAndCompleteFired(IDBTransactionBackendInterface* rawTransaction)
{
    RefPtr<IDBTransactionBackendInterface> transaction = rawTransaction;
    if (transaction->mode() == IndexedDB::TransactionVersionChange)
        processPendingCalls();
}

size_t IDBDatabaseBackendImpl::connectionCount()
{
    // This does not include pending open calls, as those should not block version changes and deletes.
    return m_databaseCallbacksSet.size();
}

void IDBDatabaseBackendImpl::processPendingCalls()
{
    if (m_pendingSecondHalfOpen) {
        // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
        ASSERT(m_pendingSecondHalfOpen->version() == (int64_t)m_metadata.version);
        ASSERT(m_metadata.id != InvalidId);
        m_pendingSecondHalfOpen->callbacks()->onSuccess(this, this->metadata());
        m_pendingSecondHalfOpen.release();
        // Fall through when complete, as pending deletes may be (partially) unblocked.
    }

    // Note that this check is only an optimization to reduce queue-churn and
    // not necessary for correctness; deleteDatabase and openConnection will
    // requeue their calls if this condition is true.
    if (m_runningVersionChangeTransaction)
        return;

    if (!m_pendingDeleteCalls.isEmpty() && isDeleteDatabaseBlocked())
        return;
    while (!m_pendingDeleteCalls.isEmpty()) {
        OwnPtr<PendingDeleteCall> pendingDeleteCall = m_pendingDeleteCalls.takeFirst();
        deleteDatabaseFinal(pendingDeleteCall->callbacks());
    }
    // deleteDatabaseFinal should never re-queue calls.
    ASSERT(m_pendingDeleteCalls.isEmpty());

    // This check is also not really needed, openConnection would just requeue its calls.
    if (m_runningVersionChangeTransaction)
        return;

    // Open calls can be requeued if an open call started a version change transaction.
    Deque<OwnPtr<PendingOpenCall>> pendingOpenCalls;
    m_pendingOpenCalls.swap(pendingOpenCalls);
    while (!pendingOpenCalls.isEmpty()) {
        OwnPtr<PendingOpenCall> pendingOpenCall = pendingOpenCalls.takeFirst();
        openConnection(pendingOpenCall->callbacks(), pendingOpenCall->databaseCallbacks(), pendingOpenCall->transactionId(), pendingOpenCall->version());
    }
}

void IDBDatabaseBackendImpl::createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, unsigned short mode)
{
    RefPtr<IDBTransactionBackendInterface> transaction = m_factory->maybeCreateTransactionBackend(this, transactionId, callbacks, objectStoreIds, static_cast<IndexedDB::TransactionMode>(mode));

    if (!transaction)
        return;

    ASSERT(!m_transactions.contains(transactionId));
    m_transactions.add(transactionId, transaction.get());
}

void IDBDatabaseBackendImpl::openConnection(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, int64_t version)
{
    ASSERT(m_backingStore.get());
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;

    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks, transactionId, version));
        return;
    }

    if (m_metadata.id == InvalidId) {
        // The database was deleted then immediately re-opened; openInternal() recreates it in the backing store.
        if (openInternal())
            ASSERT(m_metadata.version == IDBDatabaseMetadata::NoIntVersion);
        else {
            String message;
            RefPtr<IDBDatabaseError> error;
            if (version == IDBDatabaseMetadata::NoIntVersion)
                message = "Internal error opening database with no version specified.";
            else
                message = String::format("Internal error opening database with version %lld", static_cast<long long>(version));
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, message));
            return;
        }
    }

    // We infer that the database didn't exist from its lack of either type of version.
    bool isNewDatabase = m_metadata.version == IDBDatabaseMetadata::NoIntVersion;

    if (version == IDBDatabaseMetadata::DefaultIntVersion) {
        // FIXME: this comments was related to Chromium code. It may be incorrect
        // For unit tests only - skip upgrade steps. Calling from script with DefaultIntVersion throws exception.
        ASSERT(isNewDatabase);
        m_databaseCallbacksSet.add(databaseCallbacks);
        callbacks->onSuccess(this, this->metadata());
        return;
    }

    if (version == IDBDatabaseMetadata::NoIntVersion) {
        if (!isNewDatabase) {
            m_databaseCallbacksSet.add(RefPtr<IDBDatabaseCallbacks>(databaseCallbacks));
            callbacks->onSuccess(this, this->metadata());
            return;
        }
        // Spec says: If no version is specified and no database exists, set database version to 1.
        version = 1;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    if (version > (int64_t)m_metadata.version) {
        runIntVersionChangeTransaction(callbacks, databaseCallbacks, transactionId, version);
        return;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    if (version < (int64_t)m_metadata.version) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::VersionError, String::format("The requested version (%lld) is less than the existing version (%lld).", static_cast<long long>(version), static_cast<long long>(m_metadata.version))));
        return;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    ASSERT(version == (int64_t)m_metadata.version);
    m_databaseCallbacksSet.add(databaseCallbacks);
    callbacks->onSuccess(this, this->metadata());
}

void IDBDatabaseBackendImpl::runIntVersionChangeTransaction(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, int64_t requestedVersion)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    ASSERT(callbacks);
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(m_metadata.version, requestedVersion, IndexedDB::NullVersion);
    }
    // The spec dictates we wait until all the version change events are
    // delivered and then check m_databaseCallbacks.empty() before proceeding
    // or firing a blocked event, but instead we should be consistent with how
    // the old setVersion (incorrectly) did it.
    // FIXME: Remove the call to onBlocked and instead wait until the frontend
    // tells us that all the blocked events have been delivered. See
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount())
        callbacks->onBlocked(m_metadata.version);
    // FIXME: Add test for m_runningVersionChangeTransaction.
    if (m_runningVersionChangeTransaction || connectionCount()) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks, transactionId, requestedVersion));
        return;
    }

    Vector<int64_t> objectStoreIds;
    createTransaction(transactionId, databaseCallbacks, objectStoreIds, IndexedDB::TransactionVersionChange);
    RefPtr<IDBTransactionBackendInterface> transaction = m_transactions.get(transactionId);

    transaction->scheduleVersionChangeOperation(transactionId, requestedVersion, callbacks, databaseCallbacks, m_metadata);

    ASSERT(!m_pendingSecondHalfOpen);
    m_databaseCallbacksSet.add(databaseCallbacks);
}

void IDBDatabaseBackendImpl::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (isDeleteDatabaseBlocked()) {
        for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
            // Front end ensures the event is not fired at connections that have closePending set.
            (*it)->onVersionChange(m_metadata.version, 0, IndexedDB::NullVersion);
        }
        // FIXME: Only fire onBlocked if there are open connections after the
        // VersionChangeEvents are received, not just set up to fire.
        // https://bugs.webkit.org/show_bug.cgi?id=71130
        callbacks->onBlocked(m_metadata.version);
        m_pendingDeleteCalls.append(PendingDeleteCall::create(callbacks.release()));
        return;
    }
    deleteDatabaseFinal(callbacks.release());
}

bool IDBDatabaseBackendImpl::isDeleteDatabaseBlocked()
{
    return connectionCount();
}

void IDBDatabaseBackendImpl::deleteDatabaseFinal(PassRefPtr<IDBCallbacks> callbacks)
{
    ASSERT(!isDeleteDatabaseBlocked());
    ASSERT(m_backingStore);
    if (!m_backingStore->deleteDatabase(m_metadata.name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error deleting database."));
        return;
    }
    m_metadata.id = InvalidId;
    m_metadata.version = IDBDatabaseMetadata::NoIntVersion;
    m_metadata.objectStores.clear();
    callbacks->onSuccess();
}

void IDBDatabaseBackendImpl::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
{
    RefPtr<IDBDatabaseCallbacks> callbacks = prpCallbacks;
    ASSERT(m_databaseCallbacksSet.contains(callbacks));

    m_databaseCallbacksSet.remove(callbacks);
    if (m_pendingSecondHalfOpen && m_pendingSecondHalfOpen->databaseCallbacks() == callbacks) {
        m_pendingSecondHalfOpen->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "The connection was closed."));
        m_pendingSecondHalfOpen.release();
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
    TemporaryChange<bool> closingConnection(m_closingConnection, true);
    processPendingCalls();

    // FIXME: Add a test for the m_pendingOpenCalls cases below.
    if (!connectionCount() && !m_pendingOpenCalls.size() && !m_pendingDeleteCalls.size()) {
        TransactionMap transactions(m_transactions);
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Connection is closing.");
        for (TransactionMap::const_iterator::Values it = transactions.values().begin(), end = transactions.values().end(); it != end; ++it)
            (*it)->abort(error);

        ASSERT(m_transactions.isEmpty());

        m_backingStore.clear();

        // This check should only be false in unit tests.
        ASSERT(m_factory);
        if (m_factory)
            m_factory->removeIDBDatabaseBackend(m_identifier);
    }
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
