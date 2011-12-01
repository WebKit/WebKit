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

#include "config.h"
#include "IDBLevelDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

#include "Assertions.h"
#include "FileSystem.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBKeyRange.h"
#include "IDBLevelDBCoding.h"
#include "LevelDBComparator.h"
#include "LevelDBDatabase.h"
#include "LevelDBIterator.h"
#include "LevelDBSlice.h"
#include "LevelDBTransaction.h"
#include "SecurityOrigin.h"

namespace WebCore {

using namespace IDBLevelDBCoding;

template <typename DBOrTransaction>
static bool getInt(DBOrTransaction* db, const Vector<char>& key, int64_t& foundInt)
{
    Vector<char> result;
    if (!db->get(key, result))
        return false;

    foundInt = decodeInt(result.begin(), result.end());
    return true;
}

template <typename DBOrTransaction>
static bool putInt(DBOrTransaction* db, const Vector<char>& key, int64_t value)
{
    return db->put(key, encodeInt(value));
}

template <typename DBOrTransaction>
static bool getString(DBOrTransaction* db, const Vector<char>& key, String& foundString)
{
    Vector<char> result;
    if (!db->get(key, result))
        return false;

    foundString = decodeString(result.begin(), result.end());
    return true;
}

template <typename DBOrTransaction>
static bool putString(DBOrTransaction* db, const Vector<char> key, const String& value)
{
    if (!db->put(key, encodeString(value)))
        return false;
    return true;
}

static int compareKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return compare(a, b);
}

static int compareIndexKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return compare(a, b, true);
}

class Comparator : public LevelDBComparator {
public:
    virtual int compare(const LevelDBSlice& a, const LevelDBSlice& b) const { return IDBLevelDBCoding::compare(a, b); }
    virtual const char* name() const { return "idb_cmp1"; }
};

static bool setUpMetadata(LevelDBDatabase* db)
{
    const Vector<char> metaDataKey = SchemaVersionKey::encode();

    int64_t schemaVersion = 0;
    if (!getInt(db, metaDataKey, schemaVersion)) {
        schemaVersion = 0;
        if (!putInt(db, metaDataKey, schemaVersion))
            return false;
    }

    // FIXME: Eventually, we'll need to be able to transition between schemas.
    if (schemaVersion)
        return false; // Don't know what to do with this version.

    return true;
}

IDBLevelDBBackingStore::IDBLevelDBBackingStore(const String& identifier, IDBFactoryBackendImpl* factory, PassOwnPtr<LevelDBDatabase> db)
    : m_identifier(identifier)
    , m_factory(factory)
    , m_db(db)
{
    m_factory->addIDBBackingStore(identifier, this);
}

IDBLevelDBBackingStore::~IDBLevelDBBackingStore()
{
    m_factory->removeIDBBackingStore(m_identifier);

    // m_db's destructor uses m_comparator. The order of destruction is important.
    m_db.clear();
    m_comparator.clear();
}

PassRefPtr<IDBBackingStore> IDBLevelDBBackingStore::open(SecurityOrigin* securityOrigin, const String& pathBaseArg, const String& fileIdentifier, IDBFactoryBackendImpl* factory)
{
    String pathBase = pathBaseArg;

    OwnPtr<LevelDBComparator> comparator = adoptPtr(new Comparator());
    OwnPtr<LevelDBDatabase> db;

    if (pathBase.isEmpty())
        db = LevelDBDatabase::openInMemory(comparator.get());
    else {
        if (!makeAllDirectories(pathBase)) {
            LOG_ERROR("Unable to create IndexedDB database path %s", pathBase.utf8().data());
            return PassRefPtr<IDBBackingStore>();
        }
        // FIXME: We should eventually use the same LevelDB database for all origins.
        String path = pathByAppendingComponent(pathBase, securityOrigin->databaseIdentifier() + ".indexeddb.leveldb");

        db = LevelDBDatabase::open(path, comparator.get());
    }

    if (!db)
        return PassRefPtr<IDBBackingStore>();

    // FIXME: Handle comparator name changes.

    RefPtr<IDBLevelDBBackingStore> backingStore(adoptRef(new IDBLevelDBBackingStore(fileIdentifier, factory, db.release())));
    backingStore->m_comparator = comparator.release();

    if (!setUpMetadata(backingStore->m_db.get()))
        return PassRefPtr<IDBBackingStore>();

    return backingStore.release();
}

void IDBLevelDBBackingStore::getDatabaseNames(Vector<String>& foundNames)
{
    const Vector<char> startKey = DatabaseNameKey::encodeMinKeyForOrigin(m_identifier);
    const Vector<char> stopKey = DatabaseNameKey::encodeStopKeyForOrigin(m_identifier);

    ASSERT(foundNames.isEmpty());

    OwnPtr<LevelDBIterator> it = m_db->createIterator();
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        DatabaseNameKey databaseNameKey;
        p = DatabaseNameKey::decode(p, limit, &databaseNameKey);
        ASSERT(p);

        foundNames.append(databaseNameKey.databaseName());
    }
}

bool IDBLevelDBBackingStore::getIDBDatabaseMetaData(const String& name, String& foundVersion, int64_t& foundId)
{
    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);

    bool ok = getInt(m_db.get(), key, foundId);
    if (!ok)
        return false;

    ok = getString(m_db.get(), DatabaseMetaDataKey::encode(foundId, DatabaseMetaDataKey::kUserVersion), foundVersion);
    if (!ok)
        return false;

    return true;
}

static int64_t getNewDatabaseId(LevelDBDatabase* db)
{
    int64_t maxDatabaseId = -1;
    if (!getInt(db, MaxDatabaseIdKey::encode(), maxDatabaseId))
        maxDatabaseId = 0;

    ASSERT(maxDatabaseId >= 0);

    int64_t databaseId = maxDatabaseId + 1;
    if (!putInt(db, MaxDatabaseIdKey::encode(), databaseId))
        return -1;

    return databaseId;
}

bool IDBLevelDBBackingStore::createIDBDatabaseMetaData(const String& name, const String& version, int64_t& rowId)
{
    rowId = getNewDatabaseId(m_db.get());
    if (rowId < 0)
        return false;

    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    if (!putInt(m_db.get(), key, rowId))
        return false;
    if (!putString(m_db.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::kUserVersion), version))
        return false;
    return true;
}

bool IDBLevelDBBackingStore::updateIDBDatabaseMetaData(int64_t rowId, const String& version)
{
    ASSERT(m_currentTransaction);
    if (!putString(m_currentTransaction.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::kUserVersion), version))
        return false;

    return true;
}

static bool deleteRange(LevelDBTransaction* transaction, const Vector<char>& begin, const Vector<char>& end)
{
    OwnPtr<LevelDBIterator> it = transaction->createIterator();
    for (it->seek(begin); it->isValid() && compareKeys(it->key(), end) < 0; it->next()) {
        if (!transaction->remove(it->key()))
            return false;
    }

    return true;
}


bool IDBLevelDBBackingStore::deleteDatabase(const String& name)
{
    if (m_currentTransaction)
        return false;

    RefPtr<IDBLevelDBBackingStore::Transaction> transaction = IDBLevelDBBackingStore::Transaction::create(this);
    transaction->begin();

    int64_t databaseId;
    String version;
    if (!getIDBDatabaseMetaData(name, version, databaseId)) {
        transaction->rollback();
        return true;
    }

    const Vector<char> startKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::kOriginName);
    const Vector<char> stopKey = DatabaseMetaDataKey::encode(databaseId + 1, DatabaseMetaDataKey::kOriginName);
    if (!deleteRange(m_currentTransaction.get(), startKey, stopKey)) {
        transaction->rollback();
        return false;
    }

    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    m_currentTransaction->remove(key);

    transaction->commit();
    return true;
}

static bool checkObjectStoreAndMetaDataType(const LevelDBIterator* it, const Vector<char>& stopKey, int64_t objectStoreId, int64_t metaDataType)
{
    if (!it->isValid() || compareKeys(it->key(), stopKey) >= 0)
        return false;

    ObjectStoreMetaDataKey metaDataKey;
    const char* p = ObjectStoreMetaDataKey::decode(it->key().begin(), it->key().end(), &metaDataKey);
    ASSERT_UNUSED(p, p);
    if (metaDataKey.objectStoreId() != objectStoreId)
        return false;
    if (metaDataKey.metaDataType() != metaDataType)
        return false;
    return true;
}

void IDBLevelDBBackingStore::getObjectStores(int64_t databaseId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundAutoIncrementFlags)
{
    const Vector<char> startKey = ObjectStoreMetaDataKey::encode(databaseId, 1, 0);
    const Vector<char> stopKey = ObjectStoreMetaDataKey::encodeMaxKey(databaseId);

    ASSERT(foundIds.isEmpty());
    ASSERT(foundNames.isEmpty());
    ASSERT(foundKeyPaths.isEmpty());
    ASSERT(foundAutoIncrementFlags.isEmpty());

    OwnPtr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        ObjectStoreMetaDataKey metaDataKey;
        p = ObjectStoreMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);
        if (metaDataKey.metaDataType()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        int64_t objectStoreId = metaDataKey.objectStoreId();
        String objectStoreName = decodeString(it->value().begin(), it->value().end());

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 1)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        String keyPath = decodeString(it->value().begin(), it->value().end());
        bool hasKeyPath = true;

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 2)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        // FIXME: Add encode/decode functions for bools.
        bool autoIncrement = *it->value().begin();

        it->next(); // Is evicatble.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 3)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Last version.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 4)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Maxium index id allocated.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 5)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // [optional] has key path (is not null)
        if (checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, 6)) {
            // FIXME: Add encode/decode functions for bools.
            hasKeyPath = *it->value().begin();
            if (!hasKeyPath && !keyPath.isEmpty()) {
                LOG_ERROR("Internal Indexed DB error.");
                return;
            }
            it->next();
        }

        foundIds.append(objectStoreId);
        foundNames.append(objectStoreName);
        foundKeyPaths.append(hasKeyPath ? keyPath : String());
        foundAutoIncrementFlags.append(autoIncrement);
    }
}

static int64_t getNewObjectStoreId(LevelDBTransaction* transaction, int64_t databaseId)
{
    int64_t maxObjectStoreId = -1;
    const Vector<char> maxObjectStoreIdKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::kMaxObjectStoreId);
    if (!getInt(transaction, maxObjectStoreIdKey, maxObjectStoreId))
        maxObjectStoreId = 0;

    ASSERT(maxObjectStoreId >= 0);

    int64_t objectStoreId = maxObjectStoreId + 1;
    if (!putInt(transaction, maxObjectStoreIdKey, objectStoreId))
        return -1;

    return objectStoreId;
}

bool IDBLevelDBBackingStore::createObjectStore(int64_t databaseId, const String& name, const String& keyPath, bool autoIncrement, int64_t& assignedObjectStoreId)
{
    ASSERT(m_currentTransaction);
    int64_t objectStoreId = getNewObjectStoreId(m_currentTransaction.get(), databaseId);
    if (objectStoreId < 0)
        return false;

    const Vector<char> nameKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0);
    const Vector<char> keyPathKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 1);
    const Vector<char> autoIncrementKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 2);
    const Vector<char> evictableKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 3);
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 4);
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 5);
    const Vector<char> hasKeyPathKey  = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 6);
    const Vector<char> namesKey = ObjectStoreNamesKey::encode(databaseId, name);

    bool ok = putString(m_currentTransaction.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putString(m_currentTransaction.get(), keyPathKey, keyPath);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), autoIncrementKey, autoIncrement);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), evictableKey, false);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), lastVersionKey, 1);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), maxIndexIdKey, kMinimumIndexId);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), hasKeyPathKey, !keyPath.isNull());
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), namesKey, objectStoreId);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    assignedObjectStoreId = objectStoreId;

    return true;
}

void IDBLevelDBBackingStore::deleteObjectStore(int64_t databaseId, int64_t objectStoreId)
{
    ASSERT(m_currentTransaction);

    String objectStoreName;
    getString(m_currentTransaction.get(), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0), objectStoreName);

    if (!deleteRange(m_currentTransaction.get(), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0), ObjectStoreMetaDataKey::encodeMaxKey(databaseId, objectStoreId)))
        return; // FIXME: Report error.

    m_currentTransaction->remove(ObjectStoreNamesKey::encode(databaseId, objectStoreName));

    if (!deleteRange(m_currentTransaction.get(), IndexFreeListKey::encode(databaseId, objectStoreId, 0), IndexFreeListKey::encodeMaxKey(databaseId, objectStoreId)))
        return; // FIXME: Report error.
    if (!deleteRange(m_currentTransaction.get(), IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0), IndexMetaDataKey::encodeMaxKey(databaseId, objectStoreId)))
        return; // FIXME: Report error.

    clearObjectStore(databaseId, objectStoreId);
}

String IDBLevelDBBackingStore::getObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey& key)
{
    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    ASSERT(m_currentTransaction);
    if (!m_currentTransaction->get(leveldbKey, data))
        return String();

    int64_t version;
    const char* p = decodeVarInt(data.begin(), data.end(), version);
    if (!p)
        return String();
    (void) version;

    return decodeString(p, data.end());
}

namespace {
class LevelDBRecordIdentifier : public IDBBackingStore::ObjectStoreRecordIdentifier {
public:
    static PassRefPtr<LevelDBRecordIdentifier> create(const Vector<char>& primaryKey, int64_t version) { return adoptRef(new LevelDBRecordIdentifier(primaryKey, version)); }
    static PassRefPtr<LevelDBRecordIdentifier> create() { return adoptRef(new LevelDBRecordIdentifier()); }

    virtual bool isValid() const { return m_primaryKey.isEmpty(); }
    Vector<char> primaryKey() const { return m_primaryKey; }
    void setPrimaryKey(const Vector<char>& primaryKey) { m_primaryKey = primaryKey; }
    int64_t version() const { return m_version; }
    void setVersion(int64_t version) { m_version = version; }

private:
    LevelDBRecordIdentifier(const Vector<char>& primaryKey, int64_t version) : m_primaryKey(primaryKey), m_version(version) { ASSERT(!primaryKey.isEmpty()); }
    LevelDBRecordIdentifier() : m_primaryKey(), m_version(-1) {}

    Vector<char> m_primaryKey; // FIXME: Make it more clear that this is the *encoded* version of the key.
    int64_t m_version;
};
}

static int64_t getNewVersionNumber(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 4);

    int64_t lastVersion = -1;
    if (!getInt(transaction, lastVersionKey, lastVersion))
        lastVersion = 0;

    ASSERT(lastVersion >= 0);

    int64_t version = lastVersion + 1;
    bool ok = putInt(transaction, lastVersionKey, version);
    ASSERT_UNUSED(ok, ok);

    ASSERT(version > lastVersion); // FIXME: Think about how we want to handle the overflow scenario.

    return version;
}

bool IDBLevelDBBackingStore::putObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey& key, const String& value, ObjectStoreRecordIdentifier* recordIdentifier)
{
    ASSERT(m_currentTransaction);
    int64_t version = getNewVersionNumber(m_currentTransaction.get(), databaseId, objectStoreId);
    const Vector<char> objectStoredataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);

    Vector<char> v;
    v.append(encodeVarInt(version));
    v.append(encodeString(value));

    if (!m_currentTransaction->put(objectStoredataKey, v))
        return false;

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, key);
    if (!m_currentTransaction->put(existsEntryKey, encodeInt(version)))
        return false;

    LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<LevelDBRecordIdentifier*>(recordIdentifier);
    levelDBRecordIdentifier->setPrimaryKey(encodeIDBKey(key));
    levelDBRecordIdentifier->setVersion(version);
    return true;
}

void IDBLevelDBBackingStore::clearObjectStore(int64_t databaseId, int64_t objectStoreId)
{
    ASSERT(m_currentTransaction);
    const Vector<char> startKey = KeyPrefix(databaseId, objectStoreId, 0).encode();
    const Vector<char> stopKey = KeyPrefix(databaseId, objectStoreId + 1, 0).encode();

    deleteRange(m_currentTransaction.get(), startKey, stopKey);
}

PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> IDBLevelDBBackingStore::createInvalidRecordIdentifier()
{
    return LevelDBRecordIdentifier::create();
}

void IDBLevelDBBackingStore::deleteObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const ObjectStoreRecordIdentifier* recordIdentifier)
{
    ASSERT(m_currentTransaction);
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);

    const Vector<char> objectStoreDataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, levelDBRecordIdentifier->primaryKey());
    m_currentTransaction->remove(objectStoreDataKey);

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, levelDBRecordIdentifier->primaryKey());
    m_currentTransaction->remove(existsEntryKey);
}

double IDBLevelDBBackingStore::nextAutoIncrementNumber(int64_t databaseId, int64_t objectStoreId)
{
    ASSERT(m_currentTransaction);
    const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
    const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

    OwnPtr<LevelDBIterator> it = m_currentTransaction->createIterator();

    int maxNumericKey = 0;

    // FIXME: Be more efficient: seek to something after the object store data, then reverse.

    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        ObjectStoreDataKey dataKey;
        p = ObjectStoreDataKey::decode(p, limit, &dataKey);
        ASSERT(p);

        if (dataKey.userKey()->type() == IDBKey::NumberType) {
            int64_t n = static_cast<int64_t>(dataKey.userKey()->number());
            if (n > maxNumericKey)
                maxNumericKey = n;
        }
    }

    return maxNumericKey + 1;
}

bool IDBLevelDBBackingStore::keyExistsInObjectStore(int64_t databaseId, int64_t objectStoreId, const IDBKey& key, ObjectStoreRecordIdentifier* foundRecordIdentifier)
{
    ASSERT(m_currentTransaction);
    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    if (!m_currentTransaction->get(leveldbKey, data))
        return false;

    int64_t version;
    if (!decodeVarInt(data.begin(), data.end(), version))
        return false;

    LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<LevelDBRecordIdentifier*>(foundRecordIdentifier);
    levelDBRecordIdentifier->setPrimaryKey(encodeIDBKey(key));
    levelDBRecordIdentifier->setVersion(version);
    return true;
}

bool IDBLevelDBBackingStore::forEachObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, ObjectStoreRecordCallback& callback)
{
    ASSERT(m_currentTransaction);
    const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
    const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

    OwnPtr<LevelDBIterator> it = m_currentTransaction->createIterator();
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();
        ObjectStoreDataKey dataKey;
        p = ObjectStoreDataKey::decode(p, limit, &dataKey);
        ASSERT(p);

        RefPtr<IDBKey> primaryKey = dataKey.userKey();

        int64_t version;
        const char* q = decodeVarInt(it->value().begin(), it->value().end(), version);
        if (!q)
            return false;

        RefPtr<LevelDBRecordIdentifier> ri = LevelDBRecordIdentifier::create(encodeIDBKey(*primaryKey), version);
        String idbValue = decodeString(q, it->value().end());

        callback.callback(ri.get(), idbValue);
    }

    return true;
}

static bool checkIndexAndMetaDataKey(const LevelDBIterator* it, const Vector<char>& stopKey, int64_t indexId, unsigned char metaDataType)
{
    if (!it->isValid() || compareKeys(it->key(), stopKey) >= 0)
        return false;

    IndexMetaDataKey metaDataKey;
    const char* p = IndexMetaDataKey::decode(it->key().begin(), it->key().end(), &metaDataKey);
    ASSERT_UNUSED(p, p);
    if (metaDataKey.indexId() != indexId)
        return false;
    if (metaDataKey.metaDataType() != metaDataType)
        return false;
    return true;
}


void IDBLevelDBBackingStore::getIndexes(int64_t databaseId, int64_t objectStoreId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundUniqueFlags, Vector<bool>& foundMultientryFlags)
{
    const Vector<char> startKey = IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0);
    const Vector<char> stopKey = IndexMetaDataKey::encode(databaseId, objectStoreId + 1, 0, 0);

    ASSERT(foundIds.isEmpty());
    ASSERT(foundNames.isEmpty());
    ASSERT(foundKeyPaths.isEmpty());
    ASSERT(foundUniqueFlags.isEmpty());
    ASSERT(foundMultientryFlags.isEmpty());

    OwnPtr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        IndexMetaDataKey metaDataKey;
        p = IndexMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);

        int64_t indexId = metaDataKey.indexId();
        ASSERT(!metaDataKey.metaDataType());

        String indexName = decodeString(it->value().begin(), it->value().end());

        it->next(); // unique flag
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, 1)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        // FIXME: Add encode/decode functions for bools.
        bool indexUnique = *it->value().begin();

        it->next(); // keyPath
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, 2)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        String keyPath = decodeString(it->value().begin(), it->value().end());

        it->next(); // [optional] multientry flag
        bool indexMultientry = false;
        if (checkIndexAndMetaDataKey(it.get(), stopKey, indexId, 3)) {
            // FIXME: Add encode/decode functions for bools.
            indexMultientry = *it->value().begin();
            it->next();
        }

        foundIds.append(indexId);
        foundNames.append(indexName);
        foundKeyPaths.append(keyPath);
        foundUniqueFlags.append(indexUnique);
        foundMultientryFlags.append(indexMultientry);
    }
}

static int64_t getNewIndexId(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId)
{
    int64_t maxIndexId = -1;
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 5);
    if (!getInt(transaction, maxIndexIdKey, maxIndexId))
        maxIndexId = kMinimumIndexId;

    ASSERT(maxIndexId >= 0);

    int64_t indexId = maxIndexId + 1;
    if (!putInt(transaction, maxIndexIdKey, indexId))
        return -1;

    return indexId;
}

bool IDBLevelDBBackingStore::createIndex(int64_t databaseId, int64_t objectStoreId, const String& name, const String& keyPath, bool isUnique, bool isMultientry, int64_t& indexId)
{
    ASSERT(m_currentTransaction);
    indexId = getNewIndexId(m_currentTransaction.get(), databaseId, objectStoreId);
    if (indexId < 0)
        return false;

    const Vector<char> nameKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 0);
    const Vector<char> uniqueKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 1);
    const Vector<char> keyPathKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 2);
    const Vector<char> multientryKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 3);

    bool ok = putString(m_currentTransaction.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), uniqueKey, isUnique);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putString(m_currentTransaction.get(), keyPathKey, keyPath);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), multientryKey, isMultientry);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    return true;
}

void IDBLevelDBBackingStore::deleteIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    ASSERT(m_currentTransaction);

    const Vector<char> nameKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 0);
    const Vector<char> uniqueKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 1);
    const Vector<char> keyPathKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 2);

    if (!m_currentTransaction->remove(nameKey)) {
        LOG_ERROR("Internal Indexed DB error.");
        return;
    }
    if (!m_currentTransaction->remove(uniqueKey)) {
        LOG_ERROR("Internal Indexed DB error.");
        return;
    }
    if (!m_currentTransaction->remove(keyPathKey)) {
        LOG_ERROR("Internal Indexed DB error.");
        return;
    }

    const Vector<char> indexDataStart = IndexDataKey::encodeMinKey(databaseId, objectStoreId, indexId);
    const Vector<char> indexDataEnd = IndexDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);

    if (!deleteRange(m_currentTransaction.get(), indexDataStart, indexDataEnd)) {
        LOG_ERROR("Internal Indexed DB error.");
        return;
    }
}

bool IDBLevelDBBackingStore::putIndexDataForRecord(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, const ObjectStoreRecordIdentifier* recordIdentifier)
{
    ASSERT(indexId >= kMinimumIndexId);
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);

    ASSERT(m_currentTransaction);
    const Vector<char> indexDataKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, encodeIDBKey(key), levelDBRecordIdentifier->primaryKey());

    Vector<char> data;
    data.append(encodeVarInt(levelDBRecordIdentifier->version()));
    data.append(levelDBRecordIdentifier->primaryKey());

    return m_currentTransaction->put(indexDataKey, data);
}

static bool findGreatestKeyLessThan(LevelDBTransaction* transaction, const Vector<char>& target, Vector<char>& foundKey)
{
    OwnPtr<LevelDBIterator> it = transaction->createIterator();
    it->seek(target);

    if (!it->isValid()) {
        it->seekToLast();
        if (!it->isValid())
            return false;
    }

    while (compareIndexKeys(it->key(), target) >= 0) {
        it->prev();
        if (!it->isValid())
            return false;
    }

    foundKey.clear();
    foundKey.append(it->key().begin(), it->key().end() - it->key().begin());
    return true;
}

bool IDBLevelDBBackingStore::deleteIndexDataForRecord(int64_t, int64_t, int64_t, const ObjectStoreRecordIdentifier*)
{
    // FIXME: This isn't needed since we invalidate index data via the version number mechanism.
    return true;
}

String IDBLevelDBBackingStore::getObjectViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    RefPtr<IDBKey> primaryKey = getPrimaryKeyViaIndex(databaseId, objectStoreId, indexId, key);
    if (!primaryKey)
        return String();

    return getObjectStoreRecord(databaseId, objectStoreId, *primaryKey);
}

static bool versionExists(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t version, const Vector<char>& encodedPrimaryKey)
{
    const Vector<char> key = ExistsEntryKey::encode(databaseId, objectStoreId, encodedPrimaryKey);
    Vector<char> data;

    if (!transaction->get(key, data))
        return false;

    return decodeInt(data.begin(), data.end()) == version;
}

static bool findKeyInIndex(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, Vector<char>& foundEncodedPrimaryKey)
{
    ASSERT(foundEncodedPrimaryKey.isEmpty());

    const Vector<char> leveldbKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key);
    OwnPtr<LevelDBIterator> it = transaction->createIterator();
    it->seek(leveldbKey);

    for (;;) {
        if (!it->isValid())
            return false;
        if (compareIndexKeys(it->key(), leveldbKey) > 0)
            return false;

        int64_t version;
        const char* p = decodeVarInt(it->value().begin(), it->value().end(), version);
        if (!p)
            return false;
        foundEncodedPrimaryKey.append(p, it->value().end() - p);

        if (!versionExists(transaction, databaseId, objectStoreId, version, foundEncodedPrimaryKey)) {
            // Delete stale index data entry and continue.
            transaction->remove(it->key());
            it->next();
            continue;
        }

        return true;
    }
}

PassRefPtr<IDBKey> IDBLevelDBBackingStore::getPrimaryKeyViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    ASSERT(m_currentTransaction);

    Vector<char> foundEncodedPrimaryKey;
    if (findKeyInIndex(m_currentTransaction.get(), databaseId, objectStoreId, indexId, key, foundEncodedPrimaryKey)) {
        RefPtr<IDBKey> primaryKey;
        decodeIDBKey(foundEncodedPrimaryKey.begin(), foundEncodedPrimaryKey.end(), primaryKey);
        return primaryKey.release();
    }

    return 0;
}

bool IDBLevelDBBackingStore::keyExistsInIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey)
{
    ASSERT(m_currentTransaction);

    Vector<char> foundEncodedPrimaryKey;
    if (!findKeyInIndex(m_currentTransaction.get(), databaseId, objectStoreId, indexId, indexKey, foundEncodedPrimaryKey))
        return false;

    decodeIDBKey(foundEncodedPrimaryKey.begin(), foundEncodedPrimaryKey.end(), foundPrimaryKey);
    return true;
}

namespace {

struct CursorOptions {
    Vector<char> lowKey;
    bool lowOpen;
    Vector<char> highKey;
    bool highOpen;
    bool forward;
    bool unique;
};

class CursorImplCommon : public IDBBackingStore::Cursor {
public:
    // IDBBackingStore::Cursor
    virtual bool continueFunction(const IDBKey*);
    virtual PassRefPtr<IDBKey> key() { return m_currentKey; }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_currentKey; }
    virtual String value() = 0;
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() = 0; // FIXME: I don't think this is actually used, so drop it.
    virtual int64_t indexDataId() = 0;
    virtual void close() { }

    virtual bool loadCurrentRow() = 0;
    bool firstSeek();

protected:
    CursorImplCommon(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
        : m_transaction(transaction)
        , m_cursorOptions(cursorOptions)
    {
    }

    CursorImplCommon(const CursorImplCommon* other)
        : m_transaction(other->m_transaction)
        , m_cursorOptions(other->m_cursorOptions)
        , m_currentKey(other->m_currentKey)
    {
        if (other->m_iterator) {
            m_iterator = m_transaction->createIterator();

            if (other->m_iterator->isValid()) {
                m_iterator->seek(other->m_iterator->key());
                ASSERT(m_iterator->isValid());
            }
        }
    }

    virtual ~CursorImplCommon() {}

    LevelDBTransaction* m_transaction;
    CursorOptions m_cursorOptions;
    OwnPtr<LevelDBIterator> m_iterator;
    RefPtr<IDBKey> m_currentKey;
};

bool CursorImplCommon::firstSeek()
{
    m_iterator = m_transaction->createIterator();

    if (m_cursorOptions.forward)
        m_iterator->seek(m_cursorOptions.lowKey);
    else
        m_iterator->seek(m_cursorOptions.highKey);

    for (;;) {
        if (!m_iterator->isValid())
            return false;

        if (m_cursorOptions.forward && m_cursorOptions.highOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) >= 0)
            return false;
        if (m_cursorOptions.forward && !m_cursorOptions.highOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) > 0)
            return false;
        if (!m_cursorOptions.forward && m_cursorOptions.lowOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) <= 0)
            return false;
        if (!m_cursorOptions.forward && !m_cursorOptions.lowOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) < 0)
            return false;

        if (m_cursorOptions.forward && m_cursorOptions.lowOpen) {
            // lowKey not included in the range.
            if (compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) <= 0) {
                m_iterator->next();
                continue;
            }
        }
        if (!m_cursorOptions.forward && m_cursorOptions.highOpen) {
            // highKey not included in the range.
            if (compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) >= 0) {
                m_iterator->prev();
                continue;
            }
        }

        if (!loadCurrentRow()) {
            if (m_cursorOptions.forward)
                m_iterator->next();
            else
                m_iterator->prev();
            continue;
        }

        return true;
    }
}

bool CursorImplCommon::continueFunction(const IDBKey* key)
{
    // FIXME: This shares a lot of code with firstSeek.
    RefPtr<IDBKey> previousKey = m_currentKey;

    for (;;) {
        if (m_cursorOptions.forward)
            m_iterator->next();
        else
            m_iterator->prev();

        if (!m_iterator->isValid())
            return false;

        Vector<char> trash;
        if (!m_transaction->get(m_iterator->key(), trash))
             continue;

        if (m_cursorOptions.forward && m_cursorOptions.highOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) >= 0) // high key not included in range
            return false;
        if (m_cursorOptions.forward && !m_cursorOptions.highOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) > 0)
            return false;
        if (!m_cursorOptions.forward && m_cursorOptions.lowOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) <= 0) // low key not included in range
            return false;
        if (!m_cursorOptions.forward && !m_cursorOptions.lowOpen && compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) < 0)
            return false;

        if (!loadCurrentRow())
            continue;

        if (key) {
            if (m_cursorOptions.forward) {
                if (m_currentKey->isLessThan(key))
                    continue;
            } else {
                if (key->isLessThan(m_currentKey.get()))
                    continue;
            }
        }

        if (m_cursorOptions.unique && m_currentKey->isEqual(previousKey.get()))
            continue;

        break;
    }

    return true;
}

class ObjectStoreCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<ObjectStoreCursorImpl> create(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
    {
        return adoptRef(new ObjectStoreCursorImpl(transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStore::Cursor> clone()
    {
        return adoptRef(new ObjectStoreCursorImpl(this));
    }

    // CursorImplCommon
    virtual String value() { return m_currentValue; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    virtual bool loadCurrentRow();

private:
    ObjectStoreCursorImpl(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
        : CursorImplCommon(transaction, cursorOptions)
    {
    }

    ObjectStoreCursorImpl(const ObjectStoreCursorImpl* other)
        : CursorImplCommon(other)
        , m_currentValue(other->m_currentValue)
    {
    }

    String m_currentValue;
};

bool ObjectStoreCursorImpl::loadCurrentRow()
{
    const char* p = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    ObjectStoreDataKey objectStoreDataKey;
    p = ObjectStoreDataKey::decode(p, keyLimit, &objectStoreDataKey);
    ASSERT(p);
    if (!p)
        return false;

    m_currentKey = objectStoreDataKey.userKey();

    int64_t version;
    const char* q = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), version);
    ASSERT(q);
    if (!q)
        return false;
    (void) version;

    m_currentValue = decodeString(q, m_iterator->value().end());

    return true;
}

class IndexKeyCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<IndexKeyCursorImpl> create(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
    {
        return adoptRef(new IndexKeyCursorImpl(transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStore::Cursor> clone()
    {
        return adoptRef(new IndexKeyCursorImpl(this));
    }

    // CursorImplCommon
    virtual String value() { ASSERT_NOT_REACHED(); return String(); }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_primaryKey; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    virtual bool loadCurrentRow();

private:
    IndexKeyCursorImpl(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
        : CursorImplCommon(transaction, cursorOptions)
    {
    }

    IndexKeyCursorImpl(const IndexKeyCursorImpl* other)
        : CursorImplCommon(other)
        , m_primaryKey(other->m_primaryKey)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
};

bool IndexKeyCursorImpl::loadCurrentRow()
{
    const char* p = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();
    IndexDataKey indexDataKey;
    p = IndexDataKey::decode(p, keyLimit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    int64_t indexDataVersion;
    const char* q = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), indexDataVersion);
    ASSERT(q);
    if (!q)
        return false;

    q = decodeIDBKey(q, m_iterator->value().end(), m_primaryKey);
    ASSERT(q);
    if (!q)
        return false;

    Vector<char> primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    if (!m_transaction->get(primaryLevelDBKey, result))
        return false;

    int64_t objectStoreDataVersion;
    const char* t = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    ASSERT(t);
    if (!t)
        return false;

    if (objectStoreDataVersion != indexDataVersion) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    return true;
}

class IndexCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<IndexCursorImpl> create(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
    {
        return adoptRef(new IndexCursorImpl(transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStore::Cursor> clone()
    {
        return adoptRef(new IndexCursorImpl(this));
    }

    // CursorImplCommon
    virtual String value() { return m_value; }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_primaryKey; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    bool loadCurrentRow();

private:
    IndexCursorImpl(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
        : CursorImplCommon(transaction, cursorOptions)
    {
    }

    IndexCursorImpl(const IndexCursorImpl* other)
        : CursorImplCommon(other)
        , m_primaryKey(other->m_primaryKey)
        , m_value(other->m_value)
        , m_primaryLevelDBKey(other->m_primaryLevelDBKey)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
    String m_value;
    Vector<char> m_primaryLevelDBKey;
};

bool IndexCursorImpl::loadCurrentRow()
{
    const char *p = m_iterator->key().begin();
    const char *limit = m_iterator->key().end();

    IndexDataKey indexDataKey;
    p = IndexDataKey::decode(p, limit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    const char *q = m_iterator->value().begin();
    const char *valueLimit = m_iterator->value().end();

    int64_t indexDataVersion;
    q = decodeVarInt(q, valueLimit, indexDataVersion);
    ASSERT(q);
    if (!q)
        return false;
    q = decodeIDBKey(q, valueLimit, m_primaryKey);
    ASSERT(q);
    if (!q)
        return false;

    m_primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    if (!m_transaction->get(m_primaryLevelDBKey, result))
        return false;

    int64_t objectStoreDataVersion;
    const char* t = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    ASSERT(t);
    if (!t)
        return false;

    if (objectStoreDataVersion != indexDataVersion) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    m_value = decodeString(t, result.end());
    return true;
}

}

static bool findLastIndexKeyEqualTo(LevelDBTransaction* transaction, const Vector<char>& target, Vector<char>& foundKey)
{
    OwnPtr<LevelDBIterator> it = transaction->createIterator();
    it->seek(target);

    if (!it->isValid())
        return false;

    while (it->isValid() && !compareIndexKeys(it->key(), target)) {
        foundKey.clear();
        foundKey.append(it->key().begin(), it->key().end() - it->key().begin());
        it->next();
    }

    return true;
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openObjectStoreCursor(int64_t databaseId, int64_t objectStoreId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    ASSERT(m_currentTransaction);
    CursorOptions cursorOptions;

    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    cursorOptions.forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);
    cursorOptions.unique = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::PREV_NO_DUPLICATE);

    if (!lowerBound) {
        cursorOptions.lowKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
        cursorOptions.lowOpen = true; // Not included.
    } else {
        cursorOptions.lowKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->lower());
        cursorOptions.lowOpen = range->lowerOpen();
    }

    if (!upperBound) {
        cursorOptions.highKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());
        cursorOptions.highOpen = true; // Not included.

        if (!cursorOptions.forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();
    }

    RefPtr<ObjectStoreCursorImpl> cursor = ObjectStoreCursorImpl::create(m_currentTransaction.get(), cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexKeyCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    ASSERT(m_currentTransaction);
    CursorOptions cursorOptions;
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    cursorOptions.forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);
    cursorOptions.unique = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::PREV_NO_DUPLICATE);

    if (!lowerBound) {
        cursorOptions.lowKey = IndexDataKey::encodeMinKey(databaseId, objectStoreId, indexId);
        cursorOptions.lowOpen = false; // Included.
    } else {
        cursorOptions.lowKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->lower());
        cursorOptions.lowOpen = range->lowerOpen();
    }

    if (!upperBound) {
        cursorOptions.highKey = IndexDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);
        cursorOptions.highOpen = false; // Included.

        if (!cursorOptions.forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper());
        if (!findLastIndexKeyEqualTo(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;
        cursorOptions.highOpen = range->upperOpen();
    }

    RefPtr<IndexKeyCursorImpl> cursor = IndexKeyCursorImpl::create(m_currentTransaction.get(), cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    ASSERT(m_currentTransaction);
    CursorOptions cursorOptions;
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    cursorOptions.forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);
    cursorOptions.unique = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::PREV_NO_DUPLICATE);

    if (!lowerBound) {
        cursorOptions.lowKey = IndexDataKey::encodeMinKey(databaseId, objectStoreId, indexId);
        cursorOptions.lowOpen = false; // Included.
    } else {
        cursorOptions.lowKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->lower());
        cursorOptions.lowOpen = range->lowerOpen();
    }

    if (!upperBound) {
        cursorOptions.highKey = IndexDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);
        cursorOptions.highOpen = false; // Included.

        if (!cursorOptions.forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper());
        if (!findLastIndexKeyEqualTo(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;
        cursorOptions.highOpen = range->upperOpen();
    }

    RefPtr<IndexCursorImpl> cursor = IndexCursorImpl::create(m_currentTransaction.get(), cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Transaction> IDBLevelDBBackingStore::createTransaction()
{
    return Transaction::create(this);
}

PassRefPtr<IDBLevelDBBackingStore::Transaction> IDBLevelDBBackingStore::Transaction::create(IDBLevelDBBackingStore* backingStore)
{
    return adoptRef(new Transaction(backingStore));
}

IDBLevelDBBackingStore::Transaction::Transaction(IDBLevelDBBackingStore* backingStore)
    : m_backingStore(backingStore)
{
}

void IDBLevelDBBackingStore::Transaction::begin()
{
    ASSERT(!m_backingStore->m_currentTransaction);
    m_backingStore->m_currentTransaction = LevelDBTransaction::create(m_backingStore->m_db.get());
}

void IDBLevelDBBackingStore::Transaction::commit()
{
    ASSERT(m_backingStore->m_currentTransaction);
    m_backingStore->m_currentTransaction->commit();
    m_backingStore->m_currentTransaction.clear();
}

void IDBLevelDBBackingStore::Transaction::rollback()
{
    ASSERT(m_backingStore->m_currentTransaction);
    m_backingStore->m_currentTransaction->rollback();
    m_backingStore->m_currentTransaction.clear();
}

bool IDBLevelDBBackingStore::backingStoreExists(SecurityOrigin* securityOrigin, const String&, const String& pathBaseArg)
{
    String pathBase = pathBaseArg;

    if (pathBase.isEmpty())
        return false;

    // FIXME: We should eventually use the same LevelDB database for all origins.
    String path = pathByAppendingComponent(pathBase, securityOrigin->databaseIdentifier() + ".indexeddb.leveldb");

    // FIXME: this is checking for presence of the domain, not the database itself
    return fileExists(path+"/CURRENT");
}

} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
