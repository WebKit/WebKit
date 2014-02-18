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

#include "IDBBackingStoreCursorLevelDB.h"
#include "IDBBackingStoreTransactionLevelDB.h"
#include "IDBDatabaseMetadata.h"
#include "IDBKey.h"
#include "IDBRecordIdentifier.h"
#include "IndexedDB.h"
#include "LevelDBIterator.h"
#include "LevelDBTransaction.h"
#include <functional>
#include <memory>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LevelDBComparator;
class LevelDBDatabase;
class LevelDBTransaction;
class IDBIndexWriterLevelDB;
class IDBKey;
class IDBKeyRange;
class IDBTransactionBackend;
class SecurityOrigin;
class SharedBuffer;

class LevelDBFactory {
public:
    virtual ~LevelDBFactory() { }
    virtual std::unique_ptr<LevelDBDatabase> openLevelDB(const String& fileName, const LevelDBComparator*) = 0;
    virtual bool destroyLevelDB(const String& fileName) = 0;
};

class IDBBackingStoreLevelDB : public RefCounted<IDBBackingStoreLevelDB> {
public:
    class Transaction;

    virtual ~IDBBackingStoreLevelDB();
    static PassRefPtr<IDBBackingStoreLevelDB> open(const SecurityOrigin&, const String& pathBase, const String& fileIdentifier);
    static PassRefPtr<IDBBackingStoreLevelDB> open(const SecurityOrigin&, const String& pathBase, const String& fileIdentifier, LevelDBFactory*);
    static PassRefPtr<IDBBackingStoreLevelDB> openInMemory(const String& identifier);
    static PassRefPtr<IDBBackingStoreLevelDB> openInMemory(const String& identifier, LevelDBFactory*);
    WeakPtr<IDBBackingStoreLevelDB> createWeakPtr() { return m_weakFactory.createWeakPtr(); }

    void establishBackingStoreTransaction(int64_t transactionID);

    Vector<String> getDatabaseNames();

    // New-style asynchronous callbacks
    void getOrEstablishIDBDatabaseMetadata(const String& name, std::function<void (const IDBDatabaseMetadata&, bool success)> callbackFunction);
    void deleteDatabase(const String& name, std::function<void (bool success)> successCallback);

    // Old-style synchronous callbacks
    bool updateIDBDatabaseVersion(IDBBackingStoreTransactionLevelDB&, int64_t rowId, uint64_t version);

    bool createObjectStore(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement);
    bool deleteObjectStore(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId) WARN_UNUSED_RETURN;

    bool getRecord(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, Vector<char>& record) WARN_UNUSED_RETURN;
    bool putRecord(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, PassRefPtr<SharedBuffer> value, IDBRecordIdentifier*) WARN_UNUSED_RETURN;
    bool clearObjectStore(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId) WARN_UNUSED_RETURN;
    bool deleteRecord(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, const IDBRecordIdentifier&) WARN_UNUSED_RETURN;
    bool getKeyGeneratorCurrentNumber(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t& currentNumber) WARN_UNUSED_RETURN;
    bool maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t newState, bool checkCurrent) WARN_UNUSED_RETURN;
    bool keyExistsInObjectStore(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RefPtr<IDBRecordIdentifier>& foundIDBRecordIdentifier) WARN_UNUSED_RETURN;

    bool createIndex(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) WARN_UNUSED_RETURN;
    bool deleteIndex(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId) WARN_UNUSED_RETURN;
    bool putIndexDataForRecord(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const IDBRecordIdentifier*) WARN_UNUSED_RETURN;
    bool getPrimaryKeyViaIndex(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& primaryKey) WARN_UNUSED_RETURN;
    bool keyExistsInIndex(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey, bool& exists) WARN_UNUSED_RETURN;

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> openObjectStoreKeyCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB&, int64_t databaseID, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection);
    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> openObjectStoreCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB&, int64_t databaseID, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection);
    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> openIndexKeyCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB&, int64_t databaseID, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection);
    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> openIndexCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB&, int64_t databaseID, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection);

    bool makeIndexWriters(int64_t transactionID, int64_t databaseId, const IDBObjectStoreMetadata&, IDBKey& primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<Vector<RefPtr<IDBKey>>>&, Vector<RefPtr<IDBIndexWriterLevelDB>>& indexWriters, String* errorMessage, bool& completed) WARN_UNUSED_RETURN;

    virtual PassRefPtr<IDBKey> generateKey(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId);
    bool updateKeyGenerator(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, bool checkCurrent);

    LevelDBDatabase* levelDBDatabase() { return m_db.get(); }

    static int compareIndexKeys(const LevelDBSlice&, const LevelDBSlice&);

    void removeBackingStoreTransaction(IDBBackingStoreTransactionLevelDB*);

private:
    IDBBackingStoreLevelDB(const String& identifier, std::unique_ptr<LevelDBDatabase>, std::unique_ptr<LevelDBComparator>);
    static PassRefPtr<IDBBackingStoreLevelDB> create(const String& identifier, std::unique_ptr<LevelDBDatabase>, std::unique_ptr<LevelDBComparator>);

    // FIXME: LevelDB needs to support uint64_t as the version type.
    bool createIDBDatabaseMetaData(IDBDatabaseMetadata&);
    bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata*, bool& success) WARN_UNUSED_RETURN;
    bool getObjectStores(int64_t databaseId, IDBDatabaseMetadata::ObjectStoreMap* objectStores) WARN_UNUSED_RETURN;

    bool findKeyInIndex(IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, Vector<char>& foundEncodedPrimaryKey, bool& found);
    bool getIndexes(int64_t databaseId, int64_t objectStoreId, IDBObjectStoreMetadata::IndexMap*) WARN_UNUSED_RETURN;

    String m_identifier;

    std::unique_ptr<LevelDBDatabase> m_db;
    std::unique_ptr<LevelDBComparator> m_comparator;
    WeakPtrFactory<IDBBackingStoreLevelDB> m_weakFactory;

    HashMap<int64_t, RefPtr<IDBBackingStoreTransactionLevelDB>> m_backingStoreTransactions;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#endif // IDBBackingStoreLevelDB_h
