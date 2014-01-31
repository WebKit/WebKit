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
#include "IDBDatabaseBackend.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBKeyRange.h"
#include "IDBRecordIdentifier.h"
#include "IDBServerConnection.h"
#include "IDBTransactionBackend.h"
#include "IDBTransactionCoordinator.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include <wtf/TemporaryChange.h>

namespace WebCore {

PassRefPtr<IDBDatabaseBackend> IDBDatabaseBackend::create(const String& name, const String& uniqueIdentifier, IDBFactoryBackendInterface* factory, IDBServerConnection& serverConnection)
{
    RefPtr<IDBDatabaseBackend> backend = adoptRef(new IDBDatabaseBackend(name, uniqueIdentifier, factory, serverConnection));
    backend->openInternalAsync();
    
    return backend.release();
}

IDBDatabaseBackend::IDBDatabaseBackend(const String& name, const String& uniqueIdentifier, IDBFactoryBackendInterface* factory, IDBServerConnection& serverConnection)
    : m_metadata(name, InvalidId, 0, InvalidId)
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_serverConnection(serverConnection)
    , m_transactionCoordinator(IDBTransactionCoordinator::create())
    , m_closingConnection(false)
    , m_didOpenInternal(false)
{
    ASSERT(!m_metadata.name.isNull());
}

void IDBDatabaseBackend::addObjectStore(const IDBObjectStoreMetadata& objectStore, int64_t newMaxObjectStoreId)
{
    ASSERT(!m_metadata.objectStores.contains(objectStore.id));
    if (newMaxObjectStoreId != IDBObjectStoreMetadata::InvalidId) {
        ASSERT(m_metadata.maxObjectStoreId < newMaxObjectStoreId);
        m_metadata.maxObjectStoreId = newMaxObjectStoreId;
    }
    m_metadata.objectStores.set(objectStore.id, objectStore);
}

void IDBDatabaseBackend::removeObjectStore(int64_t objectStoreId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    m_metadata.objectStores.remove(objectStoreId);
}

void IDBDatabaseBackend::addIndex(int64_t objectStoreId, const IDBIndexMetadata& index, int64_t newMaxIndexId)
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

void IDBDatabaseBackend::removeIndex(int64_t objectStoreId, int64_t indexId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    objectStore.indexes.remove(indexId);
    m_metadata.objectStores.set(objectStoreId, objectStore);
}

void IDBDatabaseBackend::openInternalAsync()
{
    RefPtr<IDBDatabaseBackend> self = this;
    m_serverConnection->getOrEstablishIDBDatabaseMetadata([self](const IDBDatabaseMetadata& metadata, bool success) {
        self->didOpenInternalAsync(metadata, success);
    });
}

void IDBDatabaseBackend::didOpenInternalAsync(const IDBDatabaseMetadata& metadata, bool success)
{
    m_didOpenInternal = true;

    if (!success) {
        processPendingOpenCalls(false);
        return;
    }

    m_metadata = metadata;

    processPendingCalls();
}

IDBDatabaseBackend::~IDBDatabaseBackend()
{
    m_factory->removeIDBDatabaseBackend(m_identifier);
}

void IDBDatabaseBackend::createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    LOG(StorageAPI, "IDBDatabaseBackend::createObjectStore");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionMode::VersionChange);

    ASSERT(!m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStoreMetadata(name, objectStoreId, keyPath, autoIncrement, IDBDatabaseBackend::MinimumIndexId);

    transaction->scheduleCreateObjectStoreOperation(objectStoreMetadata);
    addObjectStore(objectStoreMetadata, objectStoreId);
}

void IDBDatabaseBackend::deleteObjectStore(int64_t transactionId, int64_t objectStoreId)
{
    LOG(StorageAPI, "IDBDatabaseBackend::deleteObjectStore");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionMode::VersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    transaction->scheduleDeleteObjectStoreOperation(objectStoreMetadata);
    removeObjectStore(objectStoreId);
}

void IDBDatabaseBackend::createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
{
    LOG(StorageAPI, "IDBDatabaseBackend::createIndex");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionMode::VersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(!objectStore.indexes.contains(indexId));
    const IDBIndexMetadata indexMetadata(name, indexId, keyPath, unique, multiEntry);

    transaction->scheduleCreateIndexOperation(objectStoreId, indexMetadata);

    addIndex(objectStoreId, indexMetadata, indexId);
}

void IDBDatabaseBackend::deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId)
{
    LOG(StorageAPI, "IDBDatabaseBackend::deleteIndex");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionMode::VersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    const IDBIndexMetadata& indexMetadata = objectStore.indexes.get(indexId);

    transaction->scheduleDeleteIndexOperation(objectStoreId, indexMetadata);

    removeIndex(objectStoreId, indexId);
}

void IDBDatabaseBackend::commit(int64_t transactionId)
{
    // The frontend suggests that we commit, but we may have previously initiated an abort, and so have disposed of the transaction. onAbort has already been dispatched to the frontend, so it will find out about that asynchronously.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->commit();
}

void IDBDatabaseBackend::abort(int64_t transactionId)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort();
}

void IDBDatabaseBackend::abort(int64_t transactionId, PassRefPtr<IDBDatabaseError> error)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort(error);
}

void IDBDatabaseBackend::get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, bool keyOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackend::get");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleGetOperation(m_metadata, objectStoreId, indexId, keyRange, keyOnly ? IndexedDB::CursorType::KeyOnly : IndexedDB::CursorType::KeyAndValue, callbacks);
}

void IDBDatabaseBackend::put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackend::put");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionMode::ReadOnly);

    const IDBObjectStoreMetadata objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStoreMetadata.autoIncrement || key.get());

    transaction->schedulePutOperation(objectStoreMetadata, value, key, putMode, callbacks, indexIds, indexKeys);
}

void IDBDatabaseBackend::setIndexKeys(int64_t transactionID, int64_t objectStoreID, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIDs, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackend::setIndexKeys");
    ASSERT(prpPrimaryKey);
    ASSERT(m_metadata.objectStores.contains(objectStoreID));

    RefPtr<IDBTransactionBackend> transaction = m_transactions.get(transactionID);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionMode::VersionChange);

    RefPtr<IDBKey> primaryKey = prpPrimaryKey;
    m_serverConnection->setIndexKeys(transactionID, m_metadata.id, objectStoreID, m_metadata.objectStores.get(objectStoreID), *primaryKey, indexIDs, indexKeys, [transaction](PassRefPtr<IDBDatabaseError> error) {
        if (error)
            transaction->abort(error);
    });
}

void IDBDatabaseBackend::setIndexesReady(int64_t transactionId, int64_t, const Vector<int64_t>& indexIds)
{
    LOG(StorageAPI, "IDBDatabaseBackend::setIndexesReady");

    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleSetIndexesReadyOperation(indexIds.size());
}

void IDBDatabaseBackend::openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, bool keyOnly, TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackend::openCursor");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleOpenCursorOperation(objectStoreId, indexId, keyRange, direction, keyOnly ? IndexedDB::CursorType::KeyOnly : IndexedDB::CursorType::KeyAndValue, taskType, callbacks);
}

void IDBDatabaseBackend::count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackend::count");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    transaction->scheduleCountOperation(objectStoreId, indexId, keyRange, callbacks);
}


void IDBDatabaseBackend::deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackend::deleteRange");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionMode::ReadOnly);

    transaction->scheduleDeleteRangeOperation(objectStoreId, keyRange, callbacks);
}

void IDBDatabaseBackend::clearObjectStore(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackend::clear");
    IDBTransactionBackend* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionMode::ReadOnly);

    transaction->scheduleClearObjectStoreOperation(objectStoreId, callbacks);
}

void IDBDatabaseBackend::transactionStarted(IDBTransactionBackend* transaction)
{
    if (transaction->mode() == IndexedDB::TransactionMode::VersionChange) {
        ASSERT(!m_runningVersionChangeTransaction);
        m_runningVersionChangeTransaction = transaction;
    }
}

void IDBDatabaseBackend::transactionFinished(IDBTransactionBackend* rawTransaction)
{
    RefPtr<IDBTransactionBackend> transaction = rawTransaction;
    ASSERT(m_transactions.contains(transaction->id()));
    ASSERT(m_transactions.get(transaction->id()) == transaction.get());
    m_transactions.remove(transaction->id());
    if (transaction->mode() == IndexedDB::TransactionMode::VersionChange) {
        ASSERT(transaction.get() == m_runningVersionChangeTransaction.get());
        m_runningVersionChangeTransaction.clear();
    }
}

void IDBDatabaseBackend::transactionFinishedAndAbortFired(IDBTransactionBackend* rawTransaction)
{
    RefPtr<IDBTransactionBackend> transaction = rawTransaction;
    if (transaction->mode() == IndexedDB::TransactionMode::VersionChange) {
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

void IDBDatabaseBackend::transactionFinishedAndCompleteFired(IDBTransactionBackend* rawTransaction)
{
    RefPtr<IDBTransactionBackend> transaction = rawTransaction;
    if (transaction->mode() == IndexedDB::TransactionMode::VersionChange)
        processPendingCalls();
}

size_t IDBDatabaseBackend::connectionCount()
{
    // This does not include pending open calls, as those should not block version changes and deletes.
    return m_databaseCallbacksSet.size();
}

void IDBDatabaseBackend::processPendingCalls()
{
    // processPendingCalls() will be called again after openInternalAsync() completes.
    if (!m_didOpenInternal)
        return;

    if (m_pendingSecondHalfOpen) {
        ASSERT(m_pendingSecondHalfOpen->version() == m_metadata.version);
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
        OwnPtr<IDBPendingDeleteCall> pendingDeleteCall = m_pendingDeleteCalls.takeFirst();
        m_deleteCallbacksWaitingCompletion.add(pendingDeleteCall->callbacks());
        deleteDatabaseAsync(pendingDeleteCall->callbacks());
    }

    // deleteDatabaseAsync should never re-queue calls.
    ASSERT(m_pendingDeleteCalls.isEmpty());

    // If there are any database deletions waiting for completion, we're done for now.
    // Further callbacks will be handled in a future call to processPendingCalls().
    if (!m_deleteCallbacksWaitingCompletion.isEmpty())
        return;

    if (m_runningVersionChangeTransaction)
        return;

    processPendingOpenCalls(true);
}

void IDBDatabaseBackend::processPendingOpenCalls(bool success)
{
    // Open calls can be requeued if an open call started a version change transaction or deletes the database.
    Deque<OwnPtr<IDBPendingOpenCall>> pendingOpenCalls;
    m_pendingOpenCalls.swap(pendingOpenCalls);

    while (!pendingOpenCalls.isEmpty()) {
        OwnPtr<IDBPendingOpenCall> pendingOpenCall = pendingOpenCalls.takeFirst();
        if (success) {
            if (m_metadata.id == InvalidId) {
                // This database was deleted then quickly re-opened.
                // openInternalAsync() will recreate it in the backing store and then resume processing pending callbacks.
                pendingOpenCalls.prepend(pendingOpenCall.release());
                pendingOpenCalls.swap(m_pendingOpenCalls);

                openInternalAsync();
                return;
            }
            openConnectionInternal(pendingOpenCall->callbacks(), pendingOpenCall->databaseCallbacks(), pendingOpenCall->transactionId(), pendingOpenCall->version());
        } else {
            String message;
            if (pendingOpenCall->version() == IDBDatabaseMetadata::NoIntVersion)
                message = "Internal error opening database with no version specified.";
            else
                message = String::format("Internal error opening database with version %llu", static_cast<unsigned long long>(pendingOpenCall->version()));
            pendingOpenCall->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, message));
        }
    }
}

void IDBDatabaseBackend::createTransaction(int64_t transactionID, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode)
{
    RefPtr<IDBTransactionBackend> transaction = IDBTransactionBackend::create(this, transactionID, callbacks, objectStoreIDs, mode);

    ASSERT(!m_transactions.contains(transactionID));
    m_transactions.add(transactionID, transaction.get());
}

void IDBDatabaseBackend::openConnection(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, uint64_t version)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;

    m_pendingOpenCalls.append(IDBPendingOpenCall::create(*callbacks, *databaseCallbacks, transactionId, version));

    processPendingCalls();
}

void IDBDatabaseBackend::openConnectionInternal(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, uint64_t version)
{
    ASSERT(m_pendingDeleteCalls.isEmpty());
    ASSERT(!m_runningVersionChangeTransaction);

    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;

    // We infer that the database didn't exist from its lack of either type of version.
    bool isNewDatabase = m_metadata.version == IDBDatabaseMetadata::NoIntVersion;

    if (version == IDBDatabaseMetadata::DefaultIntVersion) {
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

    if (version > m_metadata.version) {
        runIntVersionChangeTransaction(callbacks, databaseCallbacks, transactionId, version);
        return;
    }

    if (version < m_metadata.version) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::VersionError, String::format("The requested version (%llu) is less than the existing version (%llu).", static_cast<unsigned long long>(version), static_cast<unsigned long long>(m_metadata.version))));
        return;
    }

    ASSERT(version == m_metadata.version);
    m_databaseCallbacksSet.add(databaseCallbacks);
    callbacks->onSuccess(this, this->metadata());
}

void IDBDatabaseBackend::runIntVersionChangeTransaction(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, int64_t requestedVersion)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    ASSERT(callbacks);
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(m_metadata.version, requestedVersion, IndexedDB::VersionNullness::Null);
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
        m_pendingOpenCalls.append(IDBPendingOpenCall::create(*callbacks, *databaseCallbacks, transactionId, requestedVersion));
        return;
    }

    Vector<int64_t> objectStoreIds;
    createTransaction(transactionId, databaseCallbacks, objectStoreIds, IndexedDB::TransactionMode::VersionChange);
    RefPtr<IDBTransactionBackend> transaction = m_transactions.get(transactionId);

    transaction->scheduleVersionChangeOperation(requestedVersion, callbacks, databaseCallbacks, m_metadata);

    ASSERT(!m_pendingSecondHalfOpen);
    m_databaseCallbacksSet.add(databaseCallbacks);
}

void IDBDatabaseBackend::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (isDeleteDatabaseBlocked()) {
        for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
            // Front end ensures the event is not fired at connections that have closePending set.
            (*it)->onVersionChange(m_metadata.version, 0, IndexedDB::VersionNullness::Null);
        }
        // FIXME: Only fire onBlocked if there are open connections after the
        // VersionChangeEvents are received, not just set up to fire.
        // https://bugs.webkit.org/show_bug.cgi?id=71130
        callbacks->onBlocked(m_metadata.version);
        m_pendingDeleteCalls.append(IDBPendingDeleteCall::create(callbacks.release()));
        return;
    }
    deleteDatabaseAsync(callbacks.release());
}

bool IDBDatabaseBackend::isDeleteDatabaseBlocked()
{
    return connectionCount();
}

void IDBDatabaseBackend::deleteDatabaseAsync(PassRefPtr<IDBCallbacks> callbacks)
{
    ASSERT(!isDeleteDatabaseBlocked());

    RefPtr<IDBDatabaseBackend> self(this);
    m_serverConnection->deleteDatabase(m_metadata.name, [self, callbacks](bool success) {
        ASSERT(self->m_deleteCallbacksWaitingCompletion.contains(callbacks));
        self->m_deleteCallbacksWaitingCompletion.remove(callbacks);

        // If this IDBDatabaseBackend was closed while waiting for deleteDatabase to complete, no point in performing any callbacks.
        if (!self->m_serverConnection->isClosed())
            return;

        if (success) {
            self->m_metadata.id = InvalidId;
            self->m_metadata.version = IDBDatabaseMetadata::NoIntVersion;
            self->m_metadata.objectStores.clear();
            callbacks->onSuccess();
        } else
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error deleting database."));

        self->processPendingCalls();
    });
}

void IDBDatabaseBackend::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
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
    // and call close, reentering IDBDatabaseBackend::close. Then the
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

        m_serverConnection->close();

        // This check should only be false in unit tests.
        ASSERT(m_factory);
        if (m_factory)
            m_factory->removeIDBDatabaseBackend(m_identifier);
    }
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
