/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBServerConnectionLevelDB_h
#define IDBServerConnectionLevelDB_h

#include "IDBServerConnection.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

namespace WebCore {

class IDBBackingStoreLevelDB;
class IDBBackingStoreTransactionLevelDB;

class IDBServerConnectionLevelDB FINAL : public IDBServerConnection {
public:
    static PassRefPtr<IDBServerConnection> create(IDBBackingStoreLevelDB* backingStore)
    {
        return adoptRef(new IDBServerConnectionLevelDB(backingStore));
    }

    virtual ~IDBServerConnectionLevelDB();

    // FIXME: For now, server connection provides a synchronous accessor to the in-process backing store objects.
    // This is temporary and will be removed soon.
    virtual IDBBackingStoreInterface* deprecatedBackingStore() OVERRIDE;

    virtual bool isClosed() OVERRIDE;

    // Database-level operations
    virtual void getOrEstablishIDBDatabaseMetadata(const String& name, GetIDBDatabaseMetadataFunction) OVERRIDE;
    virtual void deleteDatabase(const String& name, BoolCallbackFunction successCallback) OVERRIDE;
    virtual void close() OVERRIDE;

    // Transaction-level operations
    virtual void openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode, BoolCallbackFunction successCallback) OVERRIDE;
    virtual void beginTransaction(int64_t transactionID, std::function<void()> completionCallback) OVERRIDE;
    virtual void commitTransaction(int64_t transactionID, BoolCallbackFunction successCallback) OVERRIDE;
    virtual void resetTransaction(int64_t transactionID, std::function<void()> completionCallback) OVERRIDE;
    virtual void rollbackTransaction(int64_t transactionID, std::function<void()> completionCallback) OVERRIDE;
    virtual void setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const IDBObjectStoreMetadata&, IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback);

    virtual void createObjectStore(IDBTransactionBackend&, const CreateObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void createIndex(IDBTransactionBackend&, const CreateIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void deleteIndex(IDBTransactionBackend&, const DeleteIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void get(IDBTransactionBackend&, const GetOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void put(IDBTransactionBackend&, const PutOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void openCursor(IDBTransactionBackend&, const OpenCursorOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void count(IDBTransactionBackend&, const CountOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void deleteRange(IDBTransactionBackend&, const DeleteRangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void clearObjectStore(IDBTransactionBackend&, const ClearObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void deleteObjectStore(IDBTransactionBackend&, const DeleteObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
    virtual void changeDatabaseVersion(IDBTransactionBackend&, const IDBDatabaseBackend::VersionChangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) OVERRIDE;
private:
    IDBServerConnectionLevelDB(IDBBackingStoreLevelDB*);

    RefPtr<IDBBackingStoreLevelDB> m_backingStore;
    HashMap<int64_t, RefPtr<IDBBackingStoreTransactionLevelDB>> m_backingStoreTransactions;

    bool m_closed;
};

} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBServerConnectionLevelDB_h
