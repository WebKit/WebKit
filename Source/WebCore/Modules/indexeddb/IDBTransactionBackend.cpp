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
#include "IDBTransactionBackend.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackend.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBKeyRange.h"
#include "IDBServerConnection.h"
#include "IDBTransactionBackendOperations.h"
#include "IDBTransactionCoordinator.h"
#include "Logging.h"

namespace WebCore {

PassRefPtr<IDBTransactionBackend> IDBTransactionBackend::create(IDBDatabaseBackend* databaseBackend, int64_t id, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, IndexedDB::TransactionMode mode)
{
    HashSet<int64_t> objectStoreHashSet;
    for (size_t i = 0; i < objectStoreIds.size(); ++i)
        objectStoreHashSet.add(objectStoreIds[i]);

    return adoptRef(new IDBTransactionBackend(databaseBackend, id, callbacks, objectStoreHashSet, mode));
}

IDBTransactionBackend::IDBTransactionBackend(IDBDatabaseBackend* databaseBackend, int64_t id, PassRefPtr<IDBDatabaseCallbacks> callbacks, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode mode)
    : m_objectStoreIds(objectStoreIds)
    , m_mode(mode)
    , m_state(Unopened)
    , m_commitPending(false)
    , m_callbacks(callbacks)
    , m_database(databaseBackend)
    , m_taskTimer(this, &IDBTransactionBackend::taskTimerFired)
    , m_pendingPreemptiveEvents(0)
    , m_id(id)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();

    m_database->transactionCoordinator()->didCreateTransaction(this);

    RefPtr<IDBTransactionBackend> backend(this);
    m_database->serverConnection().openTransaction(id, objectStoreIds, mode, [backend](bool success) {
        if (!success) {
            callOnMainThread([backend]() {
                backend->abort();
            });
            return;
        }

        backend->m_state = Unused;
        if (backend->hasPendingTasks())
            backend->start();
    });
}

IDBTransactionBackend::~IDBTransactionBackend()
{
    // It shouldn't be possible for this object to get deleted unless it's unused, complete, or aborted.
    ASSERT(m_state == Finished || m_state == Unused);
}

void IDBTransactionBackend::scheduleTask(IDBDatabaseBackend::TaskType type, PassRefPtr<IDBOperation> task, PassRefPtr<IDBSynchronousOperation> abortTask)
{
    if (m_state == Finished)
        return;

    if (type == IDBDatabaseBackend::NormalTask)
        m_taskQueue.append(task);
    else
        m_preemptiveTaskQueue.append(task);

    if (abortTask)
        m_abortTaskQueue.prepend(abortTask);

    if (m_state == Unopened)
        return;

    if (m_state == Unused)
        start();
    else if (m_state == Running && !m_taskTimer.isActive())
        m_taskTimer.startOneShot(0);
}

void IDBTransactionBackend::abort()
{
    abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error (unknown cause)"));
}

void IDBTransactionBackend::abort(PassRefPtr<IDBDatabaseError> error)
{
#ifndef NDEBUG
    if (error)
        LOG(StorageAPI, "IDBTransactionBackend::abort - (%s) %s", error->name().utf8().data(), error->message().utf8().data());
    else
        LOG(StorageAPI, "IDBTransactionBackend::abort (no error)");
#endif

    if (m_state == Finished)
        return;

    bool wasRunning = m_state == Running;

    // The last reference to this object may be released while performing the
    // abort steps below. We therefore take a self reference to keep ourselves
    // alive while executing this method.
    Ref<IDBTransactionBackend> protect(*this);

    m_state = Finished;
    m_taskTimer.stop();

    if (wasRunning)
        m_database->serverConnection().rollbackTransaction(m_id, []() { });

    // Run the abort tasks, if any.
    while (!m_abortTaskQueue.isEmpty()) {
        RefPtr<IDBSynchronousOperation> task(m_abortTaskQueue.takeFirst());
        task->perform();
    }

    // Backing store resources (held via cursors) must be released before script callbacks
    // are fired, as the script callbacks may release references and allow the backing store
    // itself to be released, and order is critical.
    closeOpenCursors();

    m_database->serverConnection().resetTransaction(m_id, []() { });

    // Transactions must also be marked as completed before the front-end is notified, as
    // the transaction completion unblocks operations like closing connections.
    m_database->transactionCoordinator()->didFinishTransaction(this);
    ASSERT(!m_database->transactionCoordinator()->isActive(this));
    m_database->transactionFinished(this);

    RefPtr<IDBDatabaseBackend> database = m_database.release();

    if (m_callbacks)
        m_callbacks->onAbort(id(), error);

    database->transactionFinishedAndAbortFired(this);
}

bool IDBTransactionBackend::isTaskQueueEmpty() const
{
    return m_preemptiveTaskQueue.isEmpty() && m_taskQueue.isEmpty();
}

bool IDBTransactionBackend::hasPendingTasks() const
{
    return m_pendingPreemptiveEvents || !isTaskQueueEmpty();
}

void IDBTransactionBackend::registerOpenCursor(IDBCursorBackend* cursor)
{
    m_openCursors.add(cursor);
}

void IDBTransactionBackend::unregisterOpenCursor(IDBCursorBackend* cursor)
{
    m_openCursors.remove(cursor);
}

void IDBTransactionBackend::run()
{
    // TransactionCoordinator has started this transaction. Schedule a timer
    // to process the first task.
    ASSERT(m_state == StartPending || m_state == Running);
    ASSERT(!m_taskTimer.isActive());

    m_taskTimer.startOneShot(0);
}

void IDBTransactionBackend::start()
{
    ASSERT(m_state == Unused);

    m_state = StartPending;
    m_database->transactionCoordinator()->didStartTransaction(this);
    m_database->transactionStarted(this);
}

void IDBTransactionBackend::commit()
{
    LOG(StorageAPI, "IDBTransactionBackend::commit transaction %lli in state %u", static_cast<long long>(m_id), m_state);

    // In multiprocess ports, front-end may have requested a commit but an abort has already
    // been initiated asynchronously by the back-end.
    if (m_state == Finished)
        return;

    ASSERT(m_state == Unopened || m_state == Unused || m_state == Running);
    m_commitPending = true;

    // Front-end has requested a commit, but there may be tasks like createIndex which
    // are considered synchronous by the front-end but are processed asynchronously.
    if (hasPendingTasks()) {
        LOG(StorageAPI, "IDBTransactionBackend::commit - Not committing now, transaction still has pending tasks (Transaction %lli)", static_cast<long long>(m_id));
        return;
    }

    // The last reference to this object may be released while performing the
    // commit steps below. We therefore take a self reference to keep ourselves
    // alive while executing this method.
    RefPtr<IDBTransactionBackend> backend(this);

    bool unused = m_state == Unused || m_state == Unopened;
    m_state = Finished;

    bool committed = unused;

    m_database->serverConnection().commitTransaction(m_id, [backend, this, committed, unused](bool success) mutable {
        // This might be commitTransaction request aborting during or after synchronous IDBTransactionBackend::abort() call.
        // This can easily happen if the page is navigated before all transactions finish.
        // In this case we have no further cleanup and don't need to make any callbacks.
        if (!m_database) {
            ASSERT(!success);
            return;
        }

        committed |= success;

        // Backing store resources (held via cursors) must be released before script callbacks
        // are fired, as the script callbacks may release references and allow the backing store
        // itself to be released, and order is critical.
        closeOpenCursors();

        m_database->serverConnection().resetTransaction(m_id, []() { });

        // Transactions must also be marked as completed before the front-end is notified, as
        // the transaction completion unblocks operations like closing connections.
        if (!unused)
            m_database->transactionCoordinator()->didFinishTransaction(this);
        m_database->transactionFinished(this);

        if (committed) {
            m_callbacks->onComplete(id());
            m_database->transactionFinishedAndCompleteFired(this);
        } else {
            m_callbacks->onAbort(id(), IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error committing transaction."));
            m_database->transactionFinishedAndAbortFired(this);
        }

        m_database = 0;
    });
}

void IDBTransactionBackend::taskTimerFired(Timer<IDBTransactionBackend>&)
{
    LOG(StorageAPI, "IDBTransactionBackend::taskTimerFired");

    if (m_state == StartPending) {
        m_database->serverConnection().beginTransaction(m_id, []() { });
        m_state = Running;
    }

    // The last reference to this object may be released while performing a task.
    // Take a self reference to keep this object alive so that tasks can
    // successfully make their completion callbacks.
    RefPtr<IDBTransactionBackend> self(this);

    TaskQueue* taskQueue = m_pendingPreemptiveEvents ? &m_preemptiveTaskQueue : &m_taskQueue;
    if (!taskQueue->isEmpty() && m_state != Finished) {
        ASSERT(m_state == Running);
        RefPtr<IDBOperation> task(taskQueue->takeFirst());
        task->perform([self, this, task]() {
            m_taskTimer.startOneShot(0);
        });

        return;
    }

    // If there are no pending tasks, we haven't already committed/aborted,
    // and the front-end requested a commit, it is now safe to do so.
    if (!hasPendingTasks() && m_state != Finished && m_commitPending)
        commit();
}

void IDBTransactionBackend::closeOpenCursors()
{
    for (HashSet<IDBCursorBackend*>::iterator i = m_openCursors.begin(); i != m_openCursors.end(); ++i)
        (*i)->close();
    m_openCursors.clear();
}

void IDBTransactionBackend::scheduleCreateObjectStoreOperation(const IDBObjectStoreMetadata& objectStoreMetadata)
{
    scheduleTask(CreateObjectStoreOperation::create(this, objectStoreMetadata), CreateObjectStoreAbortOperation::create(this, objectStoreMetadata.id));
}

void IDBTransactionBackend::scheduleDeleteObjectStoreOperation(const IDBObjectStoreMetadata& objectStoreMetadata)
{
    scheduleTask(DeleteObjectStoreOperation::create(this, objectStoreMetadata), DeleteObjectStoreAbortOperation::create(this, objectStoreMetadata));
}

void IDBTransactionBackend::scheduleVersionChangeOperation(int64_t requestedVersion, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, const IDBDatabaseMetadata& metadata)
{
    scheduleTask(IDBDatabaseBackend::VersionChangeOperation::create(this, requestedVersion, callbacks, databaseCallbacks), IDBDatabaseBackend::VersionChangeAbortOperation::create(this, String::number(metadata.version), metadata.version));
}

void IDBTransactionBackend::scheduleCreateIndexOperation(int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
{
    scheduleTask(CreateIndexOperation::create(this, objectStoreId, indexMetadata), CreateIndexAbortOperation::create(this, objectStoreId, indexMetadata.id));
}

void IDBTransactionBackend::scheduleDeleteIndexOperation(int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
{
    scheduleTask(DeleteIndexOperation::create(this, objectStoreId, indexMetadata), DeleteIndexAbortOperation::create(this, objectStoreId, indexMetadata));
}

void IDBTransactionBackend::scheduleGetOperation(const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(GetOperation::create(this, metadata, objectStoreId, indexId, keyRange, cursorType, callbacks));
}

void IDBTransactionBackend::schedulePutOperation(const IDBObjectStoreMetadata& objectStoreMetadata, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, IDBDatabaseBackend::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    scheduleTask(PutOperation::create(this, objectStoreMetadata, value, key, putMode, callbacks, indexIds, indexKeys));
}

void IDBTransactionBackend::scheduleSetIndexesReadyOperation(size_t indexCount)
{
    scheduleTask(IDBDatabaseBackend::PreemptiveTask, SetIndexesReadyOperation::create(this, indexCount));
}

void IDBTransactionBackend::scheduleOpenCursorOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(OpenCursorOperation::create(this, objectStoreId, indexId, keyRange, direction, cursorType, taskType, callbacks));
}

void IDBTransactionBackend::scheduleCountOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(CountOperation::create(this, objectStoreId, indexId, keyRange, callbacks));
}

void IDBTransactionBackend::scheduleDeleteRangeOperation(int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(DeleteRangeOperation::create(this, objectStoreId, keyRange, callbacks));
}

void IDBTransactionBackend::scheduleClearObjectStoreOperation(int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(ClearObjectStoreOperation::create(this, objectStoreId, callbacks));
}

};

#endif // ENABLE(INDEXED_DATABASE)
