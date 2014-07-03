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

#ifndef IDBDatabaseBackend_h
#define IDBDatabaseBackend_h

#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseMetadata.h"
#include "IDBKeyRange.h"
#include "IDBPendingDeleteCall.h"
#include "IDBPendingOpenCall.h"

#include <stdint.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabase;
class IDBFactoryBackendInterface;
class IDBKey;
class IDBKeyPath;
class IDBServerConnection;
class IDBTransactionBackend;
class IDBTransactionCoordinator;
class SharedBuffer;

struct IDBDatabaseMetadata;
struct IDBIndexMetadata;
struct IDBObjectStoreMetadata;

typedef Vector<RefPtr<IDBKey>> IndexKeys;
typedef int ExceptionCode;

class IDBDatabaseBackend : public RefCounted<IDBDatabaseBackend> {
public:
    static PassRefPtr<IDBDatabaseBackend> create(const String& name, const String& uniqueIdentifier, IDBFactoryBackendInterface*, IDBServerConnection&);
    ~IDBDatabaseBackend();

    IDBServerConnection& serverConnection() { return m_serverConnection.get(); }

    static const int64_t InvalidId = 0;
    int64_t id() const { return m_metadata.id; }
    void addObjectStore(const IDBObjectStoreMetadata&, int64_t newMaxObjectStoreId);
    void removeObjectStore(int64_t objectStoreId);
    void addIndex(int64_t objectStoreId, const IDBIndexMetadata&, int64_t newMaxIndexId);
    void removeIndex(int64_t objectStoreId, int64_t indexId);

    void openConnection(PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, uint64_t version);
    void deleteDatabase(PassRefPtr<IDBCallbacks>);

    // IDBDatabaseBackend
    void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement);
    void deleteObjectStore(int64_t transactionId, int64_t objectStoreId);
    void createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, IndexedDB::TransactionMode);
    void close(PassRefPtr<IDBDatabaseCallbacks>);

    void commit(int64_t transactionId);
    void abort(int64_t transactionId);
    void abort(int64_t transactionId, PassRefPtr<IDBDatabaseError>);

    void createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool unique, bool multiEntry);
    void deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId);

    IDBTransactionCoordinator* transactionCoordinator() const { return m_transactionCoordinator.get(); }
    void transactionStarted(IDBTransactionBackend*);
    void transactionFinished(IDBTransactionBackend*);
    void transactionFinishedAndCompleteFired(IDBTransactionBackend*);
    void transactionFinishedAndAbortFired(IDBTransactionBackend*);

    enum TaskType {
        NormalTask = 0,
        PreemptiveTask
    };

    enum PutMode {
        AddOrUpdate,
        AddOnly,
        CursorUpdate
    };

    static const int64_t MinimumIndexId = 30;

    void get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, bool keyOnly, PassRefPtr<IDBCallbacks>);
    void put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&);
    void setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&);
    void setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds);
    void openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, bool keyOnly, TaskType, PassRefPtr<IDBCallbacks>);
    void count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>);
    void deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>);
    void clearObjectStore(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks>);

    const IDBDatabaseMetadata& metadata() const { return m_metadata; }
    void setCurrentVersion(uint64_t version) { m_metadata.version = version; }

    bool hasPendingSecondHalfOpen() { return m_pendingSecondHalfOpen.get(); }
    void setPendingSecondHalfOpen(std::unique_ptr<IDBPendingOpenCall> pendingOpenCall) { m_pendingSecondHalfOpen = WTF::move(pendingOpenCall); }

    IDBFactoryBackendInterface& factoryBackend() { return *m_factory; }

    class VersionChangeOperation;
    class VersionChangeAbortOperation;

private:
    IDBDatabaseBackend(const String& name, const String& uniqueIdentifier, IDBFactoryBackendInterface*, IDBServerConnection&);

    void openConnectionInternal(PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, uint64_t version);

    void openInternalAsync();
    void didOpenInternalAsync(const IDBDatabaseMetadata&, bool success);

    void runIntVersionChangeTransaction(PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, int64_t requestedVersion);
    size_t connectionCount();
    void processPendingCalls();
    void processPendingOpenCalls(bool success);

    bool isDeleteDatabaseBlocked();
    void deleteDatabaseAsync(PassRefPtr<IDBCallbacks>);

    IDBDatabaseMetadata m_metadata;

    String m_identifier;

    RefPtr<IDBFactoryBackendInterface> m_factory;
    Ref<IDBServerConnection> m_serverConnection;

    std::unique_ptr<IDBTransactionCoordinator> m_transactionCoordinator;
    RefPtr<IDBTransactionBackend> m_runningVersionChangeTransaction;

    typedef HashMap<int64_t, IDBTransactionBackend*> TransactionMap;
    TransactionMap m_transactions;

    Deque<std::unique_ptr<IDBPendingOpenCall>> m_pendingOpenCalls;
    std::unique_ptr<IDBPendingOpenCall> m_pendingSecondHalfOpen;

    Deque<std::unique_ptr<IDBPendingDeleteCall>> m_pendingDeleteCalls;
    HashSet<RefPtr<IDBCallbacks>> m_deleteCallbacksWaitingCompletion;

    typedef ListHashSet<RefPtr<IDBDatabaseCallbacks>> DatabaseCallbacksSet;
    DatabaseCallbacksSet m_databaseCallbacksSet;

    bool m_closingConnection;
    bool m_didOpenInternal;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBDatabaseBackend_h
