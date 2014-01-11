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

#ifndef IDBTransactionBackend_h
#define IDBTransactionBackend_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseBackend.h"
#include "IDBDatabaseError.h"
#include "IDBOperation.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBCursorBackend;
class IDBDatabaseCallbacks;

class IDBTransactionBackend : public RefCounted<IDBTransactionBackend> {
public:
    static PassRefPtr<IDBTransactionBackend> create(IDBDatabaseBackend*, int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, IndexedDB::TransactionMode);
    ~IDBTransactionBackend();

    void commit();
    void abort();
    void abort(PassRefPtr<IDBDatabaseError>);

    void run();
    IndexedDB::TransactionMode mode() const  { return m_mode; }
    const HashSet<int64_t>& scope() const  { return m_objectStoreIds; }

    void scheduleTask(PassRefPtr<IDBOperation> task, PassRefPtr<IDBSynchronousOperation> abortTask = nullptr) { scheduleTask(IDBDatabaseBackend::NormalTask, task, abortTask); }
    void scheduleTask(IDBDatabaseBackend::TaskType, PassRefPtr<IDBOperation>, PassRefPtr<IDBSynchronousOperation> abortTask = nullptr);

    void registerOpenCursor(IDBCursorBackend*);
    void unregisterOpenCursor(IDBCursorBackend*);

    void addPreemptiveEvent()  { m_pendingPreemptiveEvents++; }
    void didCompletePreemptiveEvent()  { m_pendingPreemptiveEvents--; ASSERT(m_pendingPreemptiveEvents >= 0); }

    IDBDatabaseBackend& database() const  { return *m_database; }

    void scheduleCreateObjectStoreOperation(const IDBObjectStoreMetadata&);
    void scheduleDeleteObjectStoreOperation(const IDBObjectStoreMetadata&);
    void scheduleVersionChangeOperation(int64_t requestedVersion, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, const IDBDatabaseMetadata&);
    void scheduleCreateIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&);
    void scheduleDeleteIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&);
    void scheduleGetOperation(const IDBDatabaseMetadata&, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorType, PassRefPtr<IDBCallbacks>);
    void schedulePutOperation(const IDBObjectStoreMetadata&, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, IDBDatabaseBackend::PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&);
    void scheduleSetIndexesReadyOperation(size_t indexCount);
    void scheduleOpenCursorOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, IndexedDB::CursorType, IDBDatabaseBackend::TaskType, PassRefPtr<IDBCallbacks>);
    void scheduleCountOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>);
    void scheduleDeleteRangeOperation(int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>);
    void scheduleClearObjectStoreOperation(int64_t objectStoreId, PassRefPtr<IDBCallbacks>);

    int64_t id() const { return m_id; }

private:
    IDBTransactionBackend(IDBDatabaseBackend*, int64_t id, PassRefPtr<IDBDatabaseCallbacks>, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode);

    enum State {
        Unopened, // Backing store transaction not yet created.
        Unused, // Backing store transaction created, but no tasks yet.
        StartPending, // Enqueued tasks, but backing store transaction not yet started.
        Running, // Backing store transaction started but not yet finished.
        Finished, // Either aborted or committed.
    };

    void start();

    bool isTaskQueueEmpty() const;
    bool hasPendingTasks() const;

    void taskTimerFired(Timer<IDBTransactionBackend>&);
    void closeOpenCursors();

    const HashSet<int64_t> m_objectStoreIds;
    const IndexedDB::TransactionMode m_mode;

    State m_state;
    bool m_commitPending;
    RefPtr<IDBDatabaseCallbacks> m_callbacks;
    RefPtr<IDBDatabaseBackend> m_database;

    typedef Deque<RefPtr<IDBOperation>> TaskQueue;
    TaskQueue m_taskQueue;
    TaskQueue m_preemptiveTaskQueue;
    Deque<RefPtr<IDBSynchronousOperation>> m_abortTaskQueue;

    // FIXME: delete the timer once we have threads instead.
    Timer<IDBTransactionBackend> m_taskTimer;
    int m_pendingPreemptiveEvents;

    HashSet<IDBCursorBackend*> m_openCursors;

    int64_t m_id;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBTransactionBackend_h
