/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "IDBBackingStore.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBIndexBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTransactionCoordinator.h"

#include <gtest/gtest.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

class FakeIDBBackingStore : public IDBBackingStore {
public:
    virtual void getDatabaseNames(Vector<String>& foundNames) OVERRIDE { }
    virtual bool getIDBDatabaseMetaData(const String& name, String& foundVersion, int64_t& foundId) OVERRIDE { return false; }
    virtual bool createIDBDatabaseMetaData(const String& name, const String& version, int64_t& rowId) OVERRIDE { return true; }
    virtual bool updateIDBDatabaseMetaData(int64_t rowId, const String& version) OVERRIDE { return false; }
    virtual bool deleteDatabase(const String& name) OVERRIDE { return false; }

    virtual void getObjectStores(int64_t databaseId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundAutoIncrementFlags) OVERRIDE { }
    virtual bool createObjectStore(int64_t databaseId, const String& name, const String& keyPath, bool autoIncrement, int64_t& assignedObjectStoreId) OVERRIDE { return false; }
    virtual void deleteObjectStore(int64_t databaseId, int64_t objectStoreId) OVERRIDE { }

    virtual PassRefPtr<ObjectStoreRecordIdentifier> createInvalidRecordIdentifier() OVERRIDE { return PassRefPtr<ObjectStoreRecordIdentifier>(); }

    virtual String getObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey&) OVERRIDE { return String(); }
    virtual bool putObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey&, const String& value, ObjectStoreRecordIdentifier*) OVERRIDE { return false; }
    virtual void clearObjectStore(int64_t databaseId, int64_t objectStoreId) OVERRIDE { }
    virtual void deleteObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const ObjectStoreRecordIdentifier*) OVERRIDE { }
    virtual double nextAutoIncrementNumber(int64_t databaseId, int64_t objectStoreId) OVERRIDE { return 0.0; }
    virtual bool keyExistsInObjectStore(int64_t databaseId, int64_t objectStoreId, const IDBKey&, ObjectStoreRecordIdentifier* foundRecordIdentifier) OVERRIDE { return false; }

    virtual bool forEachObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, ObjectStoreRecordCallback&) OVERRIDE { return false; }

    virtual void getIndexes(int64_t databaseId, int64_t objectStoreId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundUniqueFlags, Vector<bool>& foundMultiEntryFlags) OVERRIDE { }
    virtual bool createIndex(int64_t databaseId, int64_t objectStoreId, const String& name, const String& keyPath, bool isUnique, bool isMultiEntry, int64_t& indexId) OVERRIDE { return false; }
    virtual void deleteIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId) OVERRIDE { }
    virtual bool putIndexDataForRecord(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const ObjectStoreRecordIdentifier*) OVERRIDE { return false; }
    virtual bool deleteIndexDataForRecord(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const ObjectStoreRecordIdentifier*) OVERRIDE { return false; }
    virtual String getObjectViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&) OVERRIDE { return String(); }
    virtual PassRefPtr<IDBKey> getPrimaryKeyViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&) OVERRIDE { return PassRefPtr<IDBKey>(); }
    virtual bool keyExistsInIndex(int64_t databaseid, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey) OVERRIDE { return false; }

    virtual PassRefPtr<Cursor> openObjectStoreCursor(int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
    virtual PassRefPtr<Cursor> openIndexKeyCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
    virtual PassRefPtr<Cursor> openIndexCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }

    virtual PassRefPtr<Transaction> createTransaction() OVERRIDE { return PassRefPtr<Transaction>(); }
};

TEST(IDBDatabaseBackendTest, BackingStoreRetention)
{
    RefPtr<FakeIDBBackingStore> backingStore = adoptRef(new FakeIDBBackingStore());
    EXPECT_TRUE(backingStore->hasOneRef());

    IDBTransactionCoordinator* coordinator = 0;
    IDBFactoryBackendImpl* factory = 0;
    RefPtr<IDBDatabaseBackendImpl> db = IDBDatabaseBackendImpl::create("db", backingStore.get(), coordinator, factory, "uniqueid");
    EXPECT_GT(backingStore->refCount(), 1);

    const String keyPath;
    const bool autoIncrement = false;
    RefPtr<IDBObjectStoreBackendImpl> store = IDBObjectStoreBackendImpl::create(db.get(), "store", keyPath, autoIncrement);
    EXPECT_GT(backingStore->refCount(), 1);

    const bool unique = false;
    const bool multiEntry = false;
    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(db.get(), store.get(), "index", "store", "keyPath", unique, multiEntry);
    EXPECT_GT(backingStore->refCount(), 1);

    db.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
    store.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
    index.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
