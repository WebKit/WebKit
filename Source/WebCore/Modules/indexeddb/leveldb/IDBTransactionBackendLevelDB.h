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

#ifndef IDBTransactionBackendLevelDB_h
#define IDBTransactionBackendLevelDB_h

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreLevelDB.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBDatabaseError.h"
#include "IDBTransactionBackendInterface.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBBackingStoreLevelDB;
class IDBCursorBackendLevelDB;
class IDBDatabaseCallbacks;

class IDBTransactionBackendLevelDB FINAL : public IDBTransactionBackendInterface {
public:
    static PassRefPtr<IDBTransactionBackendLevelDB> create(IDBDatabaseBackendImpl*, int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, IndexedDB::TransactionMode);
    virtual ~IDBTransactionBackendLevelDB();

    virtual void commit() OVERRIDE FINAL;
    virtual void abort() OVERRIDE FINAL;
    virtual void abort(PassRefPtr<IDBDatabaseError>) OVERRIDE FINAL;

    class Operation {
    public:
        virtual ~Operation() { }
        virtual void perform() = 0;
    };

    virtual void run() OVERRIDE;
    virtual IndexedDB::TransactionMode mode() const OVERRIDE FINAL { return m_mode; }
    const HashSet<int64_t>& scope() const OVERRIDE { return m_objectStoreIds; }
    void scheduleTask(PassOwnPtr<Operation> task, PassOwnPtr<Operation> abortTask = nullptr) { scheduleTask(IDBDatabaseBackendInterface::NormalTask, task, abortTask); }
    void scheduleTask(IDBDatabaseBackendInterface::TaskType, PassOwnPtr<Operation>, PassOwnPtr<Operation> abortTask = nullptr);
    void registerOpenCursor(IDBCursorBackendLevelDB*);
    void unregisterOpenCursor(IDBCursorBackendLevelDB*);
    void addPreemptiveEvent() { m_pendingPreemptiveEvents++; }
    void didCompletePreemptiveEvent() { m_pendingPreemptiveEvents--; ASSERT(m_pendingPreemptiveEvents >= 0); }
    virtual IDBBackingStoreInterface::Transaction* backingStoreTransaction() { return &m_transaction; }

    IDBDatabaseBackendImpl* database() const { return m_database.get(); }

    virtual void scheduleCreateObjectStoreOperation(const IDBObjectStoreMetadata&) OVERRIDE FINAL;
    virtual void scheduleDeleteObjectStoreOperation(const IDBObjectStoreMetadata&) OVERRIDE FINAL;
    virtual void scheduleVersionChangeOperation(int64_t transactionId, int64_t requestedVersion, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, const IDBDatabaseMetadata&) OVERRIDE FINAL;
    virtual void scheduleCreateIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&) OVERRIDE FINAL;
    virtual void scheduleDeleteIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&) OVERRIDE FINAL;
    virtual void scheduleGetOperation(const IDBDatabaseMetadata&, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorType, PassRefPtr<IDBCallbacks>) OVERRIDE FINAL;
    virtual void schedulePutOperation(const IDBObjectStoreMetadata&, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, IDBDatabaseBackendInterface::PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE FINAL;
    virtual void scheduleSetIndexesReadyOperation(size_t indexCount) OVERRIDE FINAL;
    virtual void scheduleOpenCursorOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, IndexedDB::CursorType, IDBDatabaseBackendInterface::TaskType, PassRefPtr<IDBCallbacks>) OVERRIDE FINAL;
    virtual void scheduleCountOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE FINAL;
    virtual void scheduleDeleteRangeOperation(int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE FINAL;
    virtual void scheduleClearOperation(int64_t objectStoreId, PassRefPtr<IDBCallbacks>) OVERRIDE FINAL;
    
private:
    IDBTransactionBackendLevelDB(IDBDatabaseBackendImpl*, int64_t id, PassRefPtr<IDBDatabaseCallbacks>, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode);

    enum State {
        Unused, // Created, but no tasks yet.
        StartPending, // Enqueued tasks, but backing store transaction not yet started.
        Running, // Backing store transaction started but not yet finished.
        Finished, // Either aborted or committed.
    };

    void start();

    bool isTaskQueueEmpty() const;
    bool hasPendingTasks() const;

    void taskTimerFired(Timer<IDBTransactionBackendLevelDB>*);
    void closeOpenCursors();

    const HashSet<int64_t> m_objectStoreIds;
    const IndexedDB::TransactionMode m_mode;

    State m_state;
    bool m_commitPending;
    RefPtr<IDBDatabaseCallbacks> m_callbacks;
    RefPtr<IDBDatabaseBackendImpl> m_database;

    typedef Deque<OwnPtr<Operation>> TaskQueue;
    TaskQueue m_taskQueue;
    TaskQueue m_preemptiveTaskQueue;
    TaskQueue m_abortTaskQueue;

    IDBBackingStoreLevelDB::Transaction m_transaction;

    // FIXME: delete the timer once we have threads instead.
    Timer<IDBTransactionBackendLevelDB> m_taskTimer;
    int m_pendingPreemptiveEvents;

    HashSet<IDBCursorBackendLevelDB*> m_openCursors;
    
    RefPtr<IDBBackingStoreInterface> m_backingStore;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#endif // IDBTransactionBackendLevelDB_h
