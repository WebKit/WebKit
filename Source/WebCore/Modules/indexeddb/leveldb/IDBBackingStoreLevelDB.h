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

#ifndef IDBBackingStoreLevelDB_h
#define IDBBackingStoreLevelDB_h

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreInterface.h"
#include "IDBKey.h"
#include "IDBMetadata.h"
#include "IDBRecordIdentifier.h"
#include "IndexedDB.h"
#include "LevelDBIterator.h"
#include "LevelDBTransaction.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LevelDBComparator;
class LevelDBDatabase;
class LevelDBTransaction;
class IDBBackingStoreTransactionLevelDB;
class IDBKey;
class IDBKeyRange;
class SecurityOrigin;
class SharedBuffer;

class LevelDBFactory {
public:
    virtual ~LevelDBFactory() { }
    virtual PassOwnPtr<LevelDBDatabase> openLevelDB(const String& fileName, const LevelDBComparator*) = 0;
    virtual bool destroyLevelDB(const String& fileName) = 0;
};

class IDBBackingStoreLevelDB FINAL : public IDBBackingStoreInterface {
public:
    class Transaction;

    virtual ~IDBBackingStoreLevelDB();
    static PassRefPtr<IDBBackingStoreLevelDB> open(const SecurityOrigin&, const String& pathBase, const String& fileIdentifier);
    static PassRefPtr<IDBBackingStoreLevelDB> open(const SecurityOrigin&, const String& pathBase, const String& fileIdentifier, LevelDBFactory*);
    static PassRefPtr<IDBBackingStoreLevelDB> openInMemory(const String& identifier);
    static PassRefPtr<IDBBackingStoreLevelDB> openInMemory(const String& identifier, LevelDBFactory*);
    WeakPtr<IDBBackingStoreLevelDB> createWeakPtr() { return m_weakFactory.createWeakPtr(); }

    virtual void establishBackingStoreTransaction(int64_t transactionID);

    virtual Vector<String> getDatabaseNames();

    // New-style asynchronous callbacks
    virtual void getOrEstablishIDBDatabaseMetadata(const String& name, GetIDBDatabaseMetadataFunction) OVERRIDE;
    virtual void deleteDatabase(const String& name, BoolCallbackFunction) OVERRIDE;

    // Old-style synchronous callbacks
    virtual bool updateIDBDatabaseVersion(IDBBackingStoreTransactionInterface&, int64_t rowId, uint64_t version) OVERRIDE;

    virtual bool createObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) OVERRIDE;
    virtual bool deleteObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId) OVERRIDE WARN_UNUSED_RETURN;

    virtual bool getRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, Vector<char>& record) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool putRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, PassRefPtr<SharedBuffer> value, IDBRecordIdentifier*) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool clearObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool deleteRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBRecordIdentifier&) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool getKeyGeneratorCurrentNumber(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t& currentNumber) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t newState, bool checkCurrent) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool keyExistsInObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RefPtr<IDBRecordIdentifier>& foundIDBRecordIdentifier) OVERRIDE WARN_UNUSED_RETURN;

    virtual bool createIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool deleteIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool putIndexDataForRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const IDBRecordIdentifier*) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool getPrimaryKeyViaIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& primaryKey) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool keyExistsInIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey, bool& exists) OVERRIDE WARN_UNUSED_RETURN;

    virtual PassRefPtr<IDBBackingStoreCursorInterface> openObjectStoreKeyCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) OVERRIDE;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openObjectStoreCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) OVERRIDE;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openIndexKeyCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) OVERRIDE;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openIndexCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) OVERRIDE;

    virtual bool makeIndexWriters(int64_t transactionID, int64_t databaseId, const IDBObjectStoreMetadata&, IDBKey& primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&, Vector<RefPtr<IDBIndexWriterLevelDB>>& indexWriters, String* errorMessage, bool& completed) OVERRIDE WARN_UNUSED_RETURN;

    virtual PassRefPtr<IDBKey> generateKey(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId) OVERRIDE;
    virtual bool updateKeyGenerator(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, bool checkCurrent) OVERRIDE;

    LevelDBDatabase* levelDBDatabase() { return m_db.get(); }

    static int compareIndexKeys(const LevelDBSlice&, const LevelDBSlice&);

    void removeBackingStoreTransaction(IDBBackingStoreTransactionLevelDB*);

protected:
    IDBBackingStoreLevelDB(const String& identifier, PassOwnPtr<LevelDBDatabase>, PassOwnPtr<LevelDBComparator>);

private:
    static PassRefPtr<IDBBackingStoreLevelDB> create(const String& identifier, PassOwnPtr<LevelDBDatabase>, PassOwnPtr<LevelDBComparator>);

    // FIXME: LevelDB needs to support uint64_t as the version type.
    bool createIDBDatabaseMetaData(IDBDatabaseMetadata&);
    bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata*, bool& success) WARN_UNUSED_RETURN;
    bool getObjectStores(int64_t databaseId, IDBDatabaseMetadata::ObjectStoreMap* objectStores) WARN_UNUSED_RETURN;

    bool findKeyInIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, Vector<char>& foundEncodedPrimaryKey, bool& found);
    bool getIndexes(int64_t databaseId, int64_t objectStoreId, IDBObjectStoreMetadata::IndexMap*) WARN_UNUSED_RETURN;

    String m_identifier;

    OwnPtr<LevelDBDatabase> m_db;
    OwnPtr<LevelDBComparator> m_comparator;
    WeakPtrFactory<IDBBackingStoreLevelDB> m_weakFactory;

    HashMap<int64_t, RefPtr<IDBBackingStoreTransactionLevelDB>> m_backingStoreTransactions;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#endif // IDBBackingStoreLevelDB_h
