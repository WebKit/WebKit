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

#include "SharedBuffer.h"

#include <gtest/gtest.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

class IDBBackingStoreTest : public testing::Test {
public:
    IDBBackingStoreTest() { }
    void SetUp()
    {
        SecurityOrigin* securityOrigin = 0;
        m_factory = 0;
        // Empty pathBase means an in-memory database.
        String pathBase;
        String fileIdentifier;
        m_backingStore = IDBBackingStore::open(securityOrigin, pathBase, fileIdentifier, m_factory);

        // useful keys and values during tests
        const char rawValue1[] = "value1";
        const char rawValue2[] = "value2";
        m_value1.append(rawValue1, sizeof(rawValue1));
        m_value2.append(rawValue2, sizeof(rawValue2));
        m_key1 = IDBKey::createNumber(99);
        m_key2 = IDBKey::createString("key2");
    }

protected:
    IDBFactoryBackendImpl* m_factory;
    RefPtr<IDBBackingStore> m_backingStore;

    // Sample keys and values that are consistent.
    RefPtr<IDBKey> m_key1;
    RefPtr<IDBKey> m_key2;
    Vector<char> m_value1;
    Vector<char> m_value2;
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
    {
        IDBBackingStore::Transaction transaction1(m_backingStore.get());
        transaction1.begin();
        IDBBackingStore::RecordIdentifier record;
        bool ok = m_backingStore->putRecord(&transaction1, highDatabaseId, highObjectStoreId, *m_key1.get(), SharedBuffer::create(m_value1.data(), m_value1.size()), &record);
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
        ok = transaction2.commit();
        EXPECT_TRUE(ok);
        EXPECT_EQ(m_value1, resultValue);
    }
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

        m_backingStore->getObjectStores(database.id, &database.objectStores);

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

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
