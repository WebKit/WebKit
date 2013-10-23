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

#ifndef IDBDatabaseBackendImpl_h
#define IDBDatabaseBackendImpl_h

#include "IDBCallbacks.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBMetadata.h"
#include <stdint.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBBackingStoreInterface;
class IDBDatabase;
class IDBFactoryBackendInterface;
class IDBTransactionBackendInterface;
class IDBTransactionCoordinator;

class IDBDatabaseBackendImpl FINAL : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<IDBDatabaseBackendImpl> create(const String& name, IDBBackingStoreInterface*, IDBFactoryBackendInterface*, const String& uniqueIdentifier);
    virtual ~IDBDatabaseBackendImpl();

    IDBBackingStoreInterface* backingStore() const;

    static const int64_t InvalidId = 0;
    int64_t id() const { return m_metadata.id; }
    void addObjectStore(const IDBObjectStoreMetadata&, int64_t newMaxObjectStoreId);
    void removeObjectStore(int64_t objectStoreId);
    void addIndex(int64_t objectStoreId, const IDBIndexMetadata&, int64_t newMaxIndexId);
    void removeIndex(int64_t objectStoreId, int64_t indexId);

    void openConnection(PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, int64_t version);
    void deleteDatabase(PassRefPtr<IDBCallbacks>);
    const IDBDatabaseMetadata& metadata() const { return m_metadata; }

    // IDBDatabaseBackendInterface
    virtual void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement);
    virtual void deleteObjectStore(int64_t transactionId, int64_t objectStoreId);
    virtual void createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, unsigned short mode);
    virtual void close(PassRefPtr<IDBDatabaseCallbacks>);

    virtual void commit(int64_t transactionId);
    virtual void abort(int64_t transactionId);
    virtual void abort(int64_t transactionId, PassRefPtr<IDBDatabaseError>);

    virtual void createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool unique, bool multiEntry);
    virtual void deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId);

    IDBTransactionCoordinator* transactionCoordinator() const { return m_transactionCoordinator.get(); }
    void transactionStarted(IDBTransactionBackendInterface*);
    void transactionFinished(IDBTransactionBackendInterface*);
    void transactionFinishedAndCompleteFired(IDBTransactionBackendInterface*);
    void transactionFinishedAndAbortFired(IDBTransactionBackendInterface*);

    virtual void get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, bool keyOnly, PassRefPtr<IDBCallbacks>) OVERRIDE;
    virtual void put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE;
    virtual void setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE;
    virtual void setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds) OVERRIDE;
    virtual void openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, bool keyOnly, TaskType, PassRefPtr<IDBCallbacks>) OVERRIDE;
    virtual void count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE;
    virtual void deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE;
    virtual void clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks>) OVERRIDE;

    virtual bool isIDBDatabaseBackendImpl() OVERRIDE { return true; }

    class VersionChangeOperation;
    class VersionChangeAbortOperation;

private:
    IDBDatabaseBackendImpl(const String& name, IDBBackingStoreInterface*, IDBFactoryBackendInterface*, const String& uniqueIdentifier);

    bool openInternal();
    void runIntVersionChangeTransaction(PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, int64_t requestedVersion);
    size_t connectionCount();
    void processPendingCalls();

    bool isDeleteDatabaseBlocked();
    void deleteDatabaseFinal(PassRefPtr<IDBCallbacks>);

    RefPtr<IDBBackingStoreInterface> m_backingStore;
    IDBDatabaseMetadata m_metadata;

    String m_identifier;
    // This might not need to be a RefPtr since the factory's lifetime is that of the page group, but it's better to be conservitive than sorry.
    RefPtr<IDBFactoryBackendInterface> m_factory;

    OwnPtr<IDBTransactionCoordinator> m_transactionCoordinator;
    RefPtr<IDBTransactionBackendInterface> m_runningVersionChangeTransaction;

    typedef HashMap<int64_t, IDBTransactionBackendInterface*> TransactionMap;
    TransactionMap m_transactions;

    class PendingOpenCall {
    public:
        static PassOwnPtr<PendingOpenCall> create(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t transactionId, int64_t version)
        {
            return adoptPtr(new PendingOpenCall(callbacks, databaseCallbacks, transactionId, version));
        }
        IDBCallbacks* callbacks() { return m_callbacks.get(); }
        IDBDatabaseCallbacks* databaseCallbacks() { return m_databaseCallbacks.get(); }
        int64_t version() { return m_version; }
        int64_t transactionId() const { return m_transactionId; }

    private:
        PendingOpenCall(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t transactionId, int64_t version)
            : m_callbacks(callbacks)
            , m_databaseCallbacks(databaseCallbacks)
            , m_version(version)
            , m_transactionId(transactionId)
        {
        }
        RefPtr<IDBCallbacks> m_callbacks;
        RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
        int64_t m_version;
        const int64_t m_transactionId;
    };

    Deque<OwnPtr<PendingOpenCall>> m_pendingOpenCalls;
    OwnPtr<PendingOpenCall> m_pendingSecondHalfOpen;

    class PendingDeleteCall {
    public:
        static PassOwnPtr<PendingDeleteCall> create(PassRefPtr<IDBCallbacks> callbacks)
        {
            return adoptPtr(new PendingDeleteCall(callbacks));
        }
        IDBCallbacks* callbacks() { return m_callbacks.get(); }

    private:
        PendingDeleteCall(PassRefPtr<IDBCallbacks> callbacks)
            : m_callbacks(callbacks)
        {
        }
        RefPtr<IDBCallbacks> m_callbacks;
    };

    Deque<OwnPtr<PendingDeleteCall>> m_pendingDeleteCalls;

    typedef ListHashSet<RefPtr<IDBDatabaseCallbacks>> DatabaseCallbacksSet;
    DatabaseCallbacksSet m_databaseCallbacksSet;

    bool m_closingConnection;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBDatabaseBackendImpl_h
