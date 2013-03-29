/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "IDBFactoryBackendImpl.h"
#include "IDBLevelDBCoding.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"

#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;
using IDBLevelDBCoding::KeyPrefix;

namespace {

class IDBBackingStoreTest : public testing::Test {
public:
    IDBBackingStoreTest() { }
    void SetUp()
    {
        SecurityOrigin* securityOrigin = 0;
        String fileIdentifier;
        m_backingStore = IDBBackingStore::openInMemory(securityOrigin, fileIdentifier);

        // useful keys and values during tests
        const char rawValue1[] = "value1";
        const char rawValue2[] = "value2";
        const char rawValue3[] = "value3";
        m_value1.append(rawValue1, sizeof(rawValue1));
        m_value2.append(rawValue2, sizeof(rawValue2));
        m_value3.append(rawValue3, sizeof(rawValue3));
        m_key1 = IDBKey::createNumber(99);
        m_key2 = IDBKey::createString("key2");
        m_key3 = IDBKey::createString("key3");
    }

protected:
    RefPtr<IDBBackingStore> m_backingStore;

    // Sample keys and values that are consistent.
    RefPtr<IDBKey> m_key1;
    RefPtr<IDBKey> m_key2;
    RefPtr<IDBKey> m_key3;
    Vector<char> m_value1;
    Vector<char> m_value2;
    Vector<char> m_value3;
};

TEST_F(IDBBackingStoreTest, PutGetConsistency)
{
    {
        IDBBackingStore::Transaction transaction1(m_backingStore.get());
        transaction1.begin();
        IDBBackingStore::RecordIdentifier record;
        bool ok = m_backingStore->putRecord(&transaction1, 1, 1, *m_key1.get(), SharedBuffer::create(m_value1.data(), m_value1.size()), &record);
        EXPECT_TRUE(ok);
        transaction1.commit();
    }

    {
        IDBBackingStore::Transaction transaction2(m_backingStore.get());
        transaction2.begin();
        Vector<char> resultValue;
        bool ok = m_backingStore->getRecord(&transaction2, 1, 1, *m_key1.get(), resultValue);
        transaction2.commit();
        EXPECT_TRUE(ok);
        EXPECT_EQ(m_value1, resultValue);
    }
}

// Make sure that using very high ( more than 32 bit ) values for databaseId and objectStoreId still work.
TEST_F(IDBBackingStoreTest, HighIds)
{
    const int64_t highDatabaseId = 1ULL << 35;
    const int64_t highObjectStoreId = 1ULL << 39;
    // indexIds are capped at 32 bits for storage purposes.
    const int64_t highIndexId = 1ULL << 29;

    const int64_t invalidHighIndexId = 1ULL << 37;

    const RefPtr<IDBKey> indexKey = m_key2;
    Vector<char> indexKeyRaw = IDBLevelDBCoding::encodeIDBKey(*indexKey);
    {
        IDBBackingStore::Transaction transaction1(m_backingStore.get());
        transaction1.begin();
        IDBBackingStore::RecordIdentifier record;
        bool ok = m_backingStore->putRecord(&transaction1, highDatabaseId, highObjectStoreId, *m_key1.get(), SharedBuffer::create(m_value1.data(), m_value1.size()), &record);
        EXPECT_TRUE(ok);

        ok = m_backingStore->putIndexDataForRecord(&transaction1, highDatabaseId, highObjectStoreId, invalidHighIndexId, *indexKey, record);
        EXPECT_FALSE(ok);

        ok = m_backingStore->putIndexDataForRecord(&transaction1, highDatabaseId, highObjectStoreId, highIndexId, *indexKey, record);
        EXPECT_TRUE(ok);

        ok = transaction1.commit();
        EXPECT_TRUE(ok);
    }

    {
        IDBBackingStore::Transaction transaction2(m_backingStore.get());
        transaction2.begin();
        Vector<char> resultValue;
        bool ok = m_backingStore->getRecord(&transaction2, highDatabaseId, highObjectStoreId, *m_key1.get(), resultValue);
        EXPECT_TRUE(ok);
        EXPECT_EQ(m_value1, resultValue);

        RefPtr<IDBKey> newPrimaryKey;
        ok = m_backingStore->getPrimaryKeyViaIndex(&transaction2, highDatabaseId, highObjectStoreId, invalidHighIndexId, *indexKey, newPrimaryKey);
        EXPECT_FALSE(ok);

        ok = m_backingStore->getPrimaryKeyViaIndex(&transaction2, highDatabaseId, highObjectStoreId, highIndexId, *indexKey, newPrimaryKey);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(newPrimaryKey->isEqual(m_key1.get()));

        ok = transaction2.commit();
        EXPECT_TRUE(ok);
    }
}

// Make sure that other invalid ids do not crash.
TEST_F(IDBBackingStoreTest, InvalidIds)
{
    // valid ids for use when testing invalid ids
    const int64_t databaseId = 1;
    const int64_t objectStoreId = 1;
    const int64_t indexId = IDBLevelDBCoding::MinimumIndexId;
    const int64_t invalidLowIndexId = 19; // indexIds must be > IDBLevelDBCoding::MinimumIndexId

    const RefPtr<SharedBuffer> value = SharedBuffer::create(m_value1.data(), m_value1.size());
    Vector<char> resultValue;

    IDBBackingStore::Transaction transaction1(m_backingStore.get());
    transaction1.begin();

    IDBBackingStore::RecordIdentifier record;
    bool ok = m_backingStore->putRecord(&transaction1, databaseId, KeyPrefix::InvalidId, *m_key1.get(), value, &record);
    EXPECT_FALSE(ok);
    ok = m_backingStore->putRecord(&transaction1, databaseId, 0, *m_key1.get(), value, &record);
    EXPECT_FALSE(ok);
    ok = m_backingStore->putRecord(&transaction1, KeyPrefix::InvalidId, objectStoreId, *m_key1.get(), value, &record);
    EXPECT_FALSE(ok);
    ok = m_backingStore->putRecord(&transaction1, 0, objectStoreId, *m_key1.get(), value, &record);
    EXPECT_FALSE(ok);

    ok = m_backingStore->getRecord(&transaction1, databaseId, KeyPrefix::InvalidId, *m_key1.get(), resultValue);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getRecord(&transaction1, databaseId, 0, *m_key1.get(), resultValue);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getRecord(&transaction1, KeyPrefix::InvalidId, objectStoreId, *m_key1.get(), resultValue);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getRecord(&transaction1, 0, objectStoreId, *m_key1.get(), resultValue);
    EXPECT_FALSE(ok);

    RefPtr<IDBKey> newPrimaryKey;
    ok = m_backingStore->getPrimaryKeyViaIndex(&transaction1, databaseId, objectStoreId, KeyPrefix::InvalidId, *m_key1, newPrimaryKey);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getPrimaryKeyViaIndex(&transaction1, databaseId, objectStoreId, invalidLowIndexId, *m_key1, newPrimaryKey);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getPrimaryKeyViaIndex(&transaction1, databaseId, objectStoreId, 0, *m_key1, newPrimaryKey);
    EXPECT_FALSE(ok);

    ok = m_backingStore->getPrimaryKeyViaIndex(&transaction1, KeyPrefix::InvalidId, objectStoreId, indexId, *m_key1, newPrimaryKey);
    EXPECT_FALSE(ok);
    ok = m_backingStore->getPrimaryKeyViaIndex(&transaction1, databaseId, KeyPrefix::InvalidId, indexId, *m_key1, newPrimaryKey);
    EXPECT_FALSE(ok);
}

TEST_F(IDBBackingStoreTest, CreateDatabase)
{
    const String databaseName("db1");
    int64_t databaseId;
    const String version("oldStringVersion");
    const int64_t intVersion = 9;

    const int64_t objectStoreId = 99;
    const String objectStoreName("objectStore1");
    const bool autoIncrement = true;
    const IDBKeyPath objectStoreKeyPath("objectStoreKey");

    const int64_t indexId = 999;
    const String indexName("index1");
    const bool unique = true;
    const bool multiEntry = true;
    const IDBKeyPath indexKeyPath("indexKey");

    {
        bool ok = m_backingStore->createIDBDatabaseMetaData(databaseName, version, intVersion, databaseId);
        EXPECT_TRUE(ok);
        EXPECT_GT(databaseId, 0);

        IDBBackingStore::Transaction transaction(m_backingStore.get());
        transaction.begin();

        ok = m_backingStore->createObjectStore(&transaction, databaseId, objectStoreId, objectStoreName, objectStoreKeyPath, autoIncrement);
        EXPECT_TRUE(ok);

        ok = m_backingStore->createIndex(&transaction, databaseId, objectStoreId, indexId, indexName, indexKeyPath, unique, multiEntry);
        EXPECT_TRUE(ok);

        ok = transaction.commit();
        EXPECT_TRUE(ok);
    }

    {
        IDBDatabaseMetadata database;
        bool found;
        bool ok = m_backingStore->getIDBDatabaseMetaData(databaseName, &database, found);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(found);

        // database.name is not filled in by the implementation.
        EXPECT_EQ(version, database.version);
        EXPECT_EQ(intVersion, database.intVersion);
        EXPECT_EQ(databaseId, database.id);

        ok = m_backingStore->getObjectStores(database.id, &database.objectStores);
        EXPECT_TRUE(ok);

        EXPECT_EQ(1, database.objectStores.size());
        IDBObjectStoreMetadata objectStore = database.objectStores.get(objectStoreId);
        EXPECT_EQ(objectStoreName, objectStore.name);
        EXPECT_EQ(objectStoreKeyPath, objectStore.keyPath);
        EXPECT_EQ(autoIncrement, objectStore.autoIncrement);

        EXPECT_EQ(1, objectStore.indexes.size());
        IDBIndexMetadata index = objectStore.indexes.get(indexId);
        EXPECT_EQ(indexName, index.name);
        EXPECT_EQ(indexKeyPath, index.keyPath);
        EXPECT_EQ(unique, index.unique);
        EXPECT_EQ(multiEntry, index.multiEntry);
    }
}

class MockIDBFactoryBackend : public IDBFactoryBackendImpl {
public:
    static PassRefPtr<MockIDBFactoryBackend> create()
    {
        return adoptRef(new MockIDBFactoryBackend());
    }

    PassRefPtr<IDBBackingStore> testOpenBackingStore(PassRefPtr<SecurityOrigin> origin, const String& dataDirectory)
    {
        return openBackingStore(origin, dataDirectory);
    }
};

TEST(IDBFactoryBackendTest, BackingStoreLifetime)
{
    RefPtr<SecurityOrigin> origin1 = SecurityOrigin::create("http", "localhost", 81);
    RefPtr<SecurityOrigin> origin2 = SecurityOrigin::create("http", "localhost", 82);

    RefPtr<MockIDBFactoryBackend> factory = MockIDBFactoryBackend::create();

    OwnPtr<webkit_support::ScopedTempDirectory> tempDirectory = adoptPtr(webkit_support::CreateScopedTempDirectory());
    tempDirectory->CreateUniqueTempDir();
    const String path = String::fromUTF8(tempDirectory->path().c_str());

    RefPtr<IDBBackingStore> diskStore1 = factory->testOpenBackingStore(origin1, path);
    EXPECT_TRUE(diskStore1->hasOneRef());

    RefPtr<IDBBackingStore> diskStore2 = factory->testOpenBackingStore(origin1, path);
    EXPECT_EQ(diskStore1.get(), diskStore2.get());
    EXPECT_EQ(2, diskStore2->refCount());

    RefPtr<IDBBackingStore> diskStore3 = factory->testOpenBackingStore(origin2, path);
    EXPECT_TRUE(diskStore3->hasOneRef());
    EXPECT_EQ(2, diskStore1->refCount());
}

TEST(IDBFactoryBackendTest, MemoryBackingStoreLifetime)
{
    RefPtr<SecurityOrigin> origin1 = SecurityOrigin::create("http", "localhost", 81);
    RefPtr<SecurityOrigin> origin2 = SecurityOrigin::create("http", "localhost", 82);

    RefPtr<MockIDBFactoryBackend> factory = MockIDBFactoryBackend::create();
    RefPtr<IDBBackingStore> memStore1 = factory->testOpenBackingStore(origin1, String());
    EXPECT_EQ(2, memStore1->refCount());
    RefPtr<IDBBackingStore> memStore2 = factory->testOpenBackingStore(origin1, String());
    EXPECT_EQ(memStore1.get(), memStore2.get());
    EXPECT_EQ(3, memStore2->refCount());

    RefPtr<IDBBackingStore> memStore3 = factory->testOpenBackingStore(origin2, String());
    EXPECT_EQ(2, memStore3->refCount());
    EXPECT_EQ(3, memStore1->refCount());

    factory.clear();
    EXPECT_EQ(2, memStore1->refCount());
    EXPECT_EQ(1, memStore3->refCount());
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
