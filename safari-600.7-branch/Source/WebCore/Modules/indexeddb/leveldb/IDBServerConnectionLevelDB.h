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

class IDBBackingStoreCursorLevelDB;
class IDBBackingStoreLevelDB;
class IDBBackingStoreTransactionLevelDB;

class IDBServerConnectionLevelDB final : public IDBServerConnection {
public:
    static PassRefPtr<IDBServerConnection> create(const String& databaseName, IDBBackingStoreLevelDB* backingStore)
    {
        return adoptRef(new IDBServerConnectionLevelDB(databaseName, backingStore));
    }

    virtual ~IDBServerConnectionLevelDB();

    virtual bool isClosed() override;

    // Database-level operations
    virtual void getOrEstablishIDBDatabaseMetadata(GetIDBDatabaseMetadataFunction) override;
    virtual void close() override;
    virtual void deleteDatabase(const String& name, BoolCallbackFunction successCallback) override;

    // Transaction-level operations
    virtual void openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode, BoolCallbackFunction successCallback) override;
    virtual void beginTransaction(int64_t transactionID, std::function<void()> completionCallback) override;
    virtual void commitTransaction(int64_t transactionID, BoolCallbackFunction successCallback) override;
    virtual void resetTransaction(int64_t transactionID, std::function<void()> completionCallback) override;
    virtual void rollbackTransaction(int64_t transactionID, std::function<void()> completionCallback) override;
    virtual void setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const IDBObjectStoreMetadata&, IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback);

    virtual void createObjectStore(IDBTransactionBackend&, const CreateObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void createIndex(IDBTransactionBackend&, const CreateIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void deleteIndex(IDBTransactionBackend&, const DeleteIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void get(IDBTransactionBackend&, const GetOperation&, std::function<void(const IDBGetResult&, PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void put(IDBTransactionBackend&, const PutOperation&, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBDatabaseError>)> completionCallback)  override;
    virtual void openCursor(IDBTransactionBackend&, const OpenCursorOperation&, std::function<void(int64_t, PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void count(IDBTransactionBackend&, const CountOperation&, std::function<void(int64_t, PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void deleteRange(IDBTransactionBackend&, const DeleteRangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void clearObjectStore(IDBTransactionBackend&, const ClearObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void deleteObjectStore(IDBTransactionBackend&, const DeleteObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void changeDatabaseVersion(IDBTransactionBackend&, const IDBDatabaseBackend::VersionChangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) override;

    // Cursor-level operations
    virtual void cursorAdvance(IDBCursorBackend&, const CursorAdvanceOperation&, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback) override;
    virtual void cursorIterate(IDBCursorBackend&, const CursorIterationOperation&, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SharedBuffer>, PassRefPtr<IDBDatabaseError>)> completionCallback) override;

private:
    IDBServerConnectionLevelDB(const String& databaseName, IDBBackingStoreLevelDB*);

    RefPtr<IDBBackingStoreLevelDB> m_backingStore;
    HashMap<int64_t, RefPtr<IDBBackingStoreTransactionLevelDB>> m_backingStoreTransactions;
    HashMap<int64_t, RefPtr<IDBBackingStoreCursorLevelDB>> m_backingStoreCursors;

    int64_t m_nextCursorID;

    bool m_closed;

    String m_databaseName;
};

} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBServerConnectionLevelDB_h
