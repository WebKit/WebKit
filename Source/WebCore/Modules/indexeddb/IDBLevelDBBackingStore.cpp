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
#include "IDBTracing.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

#include <wtf/Assertions.h>
#include "FileSystem.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
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

const int64_t KeyGeneratorInitialNumber = 1; // From the IndexedDB specification.

template <typename DBOrTransaction>
static bool getBool(DBOrTransaction* db, const Vector<char>& key, bool& foundBool)
{
    Vector<char> result;
    if (!db->get(key, result))
        return false;

    foundBool = decodeBool(result.begin(), result.end());
    return true;
}

template <typename DBOrTransaction>
static bool putBool(DBOrTransaction* db, const Vector<char>& key, bool value)
{
    return db->put(key, encodeBool(value));
}

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
    ASSERT(value >= 0);
    if (value < 0)
        return false;
    return db->put(key, encodeInt(value));
}

template <typename DBOrTransaction>
static bool putVarInt(DBOrTransaction* db, const Vector<char>& key, int64_t value)
{
    return db->put(key, encodeVarInt(value));
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

template <typename DBOrTransaction>
static bool putIDBKeyPath(DBOrTransaction* db, const Vector<char> key, const IDBKeyPath& value)
{
    if (!db->put(key, encodeIDBKeyPath(value)))
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

static bool setUpMetadata(LevelDBDatabase* db, const String& origin)
{
    const Vector<char> metaDataKey = SchemaVersionKey::encode();

    int64_t schemaVersion = 0;
    if (!getInt(db, metaDataKey, schemaVersion)) {
        schemaVersion = 0;
        if (!putInt(db, metaDataKey, schemaVersion))
            return false;
    } else {
        if (!schemaVersion) {
            schemaVersion = 1;
            RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(db);
            transaction->put(metaDataKey, encodeInt(schemaVersion));

            const Vector<char> startKey = DatabaseNameKey::encodeMinKeyForOrigin(origin);
            const Vector<char> stopKey = DatabaseNameKey::encodeStopKeyForOrigin(origin);
            OwnPtr<LevelDBIterator> it = db->createIterator();
            for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
                Vector<char> value;
                bool ok = transaction->get(it->key(), value);
                if (!ok) {
                    ASSERT_NOT_REACHED();
                    return false;
                }
                int databaseId = decodeInt(value.begin(), value.end());
                Vector<char> intVersionKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::UserIntVersion);
                transaction->put(intVersionKey, encodeVarInt(IDBDatabaseMetadata::DefaultIntVersion));
                ok = transaction->get(it->key(), value);
                if (!ok) {
                    ASSERT_NOT_REACHED();
                    return false;
                }
            }
            bool ok = transaction->commit();
            if (!ok) {
                ASSERT_NOT_REACHED();
                return false;
            }
        }
        ASSERT(schemaVersion == 1);
    }

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
    IDB_TRACE("IDBLevelDBBackingStore::open");
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

        if (!db) {
            LOG_ERROR("IndexedDB backing store open failed, attempting cleanup");
            bool success = LevelDBDatabase::destroy(path);
            if (!success) {
                LOG_ERROR("IndexedDB backing store cleanup failed");
                return PassRefPtr<IDBBackingStore>();
            }

            LOG_ERROR("IndexedDB backing store cleanup succeeded, reopening");
            db = LevelDBDatabase::open(path, comparator.get());
            if (!db) {
                LOG_ERROR("IndexedDB backing store reopen after recovery failed");
                return PassRefPtr<IDBBackingStore>();
            }
        }
    }

    if (!db)
        return PassRefPtr<IDBBackingStore>();

    // FIXME: Handle comparator name changes.

    RefPtr<IDBLevelDBBackingStore> backingStore(adoptRef(new IDBLevelDBBackingStore(fileIdentifier, factory, db.release())));
    backingStore->m_comparator = comparator.release();

    if (!setUpMetadata(backingStore->m_db.get(), fileIdentifier))
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
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        DatabaseNameKey databaseNameKey;
        p = DatabaseNameKey::decode(p, limit, &databaseNameKey);
        ASSERT(p);

        foundNames.append(databaseNameKey.databaseName());
    }
}

bool IDBLevelDBBackingStore::getIDBDatabaseMetaData(const String& name, String& foundStringVersion, int64_t& foundIntVersion, int64_t& foundId)
{
    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);

    bool ok = getInt(m_db.get(), key, foundId);
    if (!ok)
        return false;

    ok = getString(m_db.get(), DatabaseMetaDataKey::encode(foundId, DatabaseMetaDataKey::UserVersion), foundStringVersion);
    if (!ok)
        return false;

    ok = getInt(m_db.get(), DatabaseMetaDataKey::encode(foundId, DatabaseMetaDataKey::UserIntVersion), foundIntVersion);
    if (!ok)
        return false;
    if (foundIntVersion == IDBDatabaseMetadata::DefaultIntVersion)
        foundIntVersion = IDBDatabaseMetadata::NoIntVersion;

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

bool IDBLevelDBBackingStore::createIDBDatabaseMetaData(const String& name, const String& version, int64_t intVersion, int64_t& rowId)
{
    rowId = getNewDatabaseId(m_db.get());
    if (rowId < 0)
        return false;

    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    if (!putInt(m_db.get(), key, rowId))
        return false;
    if (!putString(m_db.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::UserVersion), version))
        return false;
    if (intVersion == IDBDatabaseMetadata::NoIntVersion)
        intVersion = IDBDatabaseMetadata::DefaultIntVersion;
    if (!putVarInt(m_db.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::UserIntVersion), intVersion))
        return false;
    return true;
}

bool IDBLevelDBBackingStore::updateIDBDatabaseIntVersion(int64_t rowId, int64_t intVersion)
{
    ASSERT(m_currentTransaction);
    // FIXME: Change this to strictly greater than 0 once we throw TypeError for
    // bad versions.
    ASSERT_WITH_MESSAGE(intVersion >= 0, "intVersion was %lld", static_cast<long long>(intVersion));
    if (!putVarInt(m_currentTransaction.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::UserIntVersion), intVersion))
        return false;

    return true;
}

bool IDBLevelDBBackingStore::updateIDBDatabaseMetaData(int64_t rowId, const String& version)
{
    ASSERT(m_currentTransaction);
    if (!putString(m_currentTransaction.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::UserVersion), version))
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
    IDB_TRACE("IDBLevelDBBackingStore::deleteDatabase");
    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(m_db.get());

    int64_t databaseId;
    String version;
    int64_t intVersion;
    if (!getIDBDatabaseMetaData(name, version, intVersion, databaseId)) {
        transaction->rollback();
        return true;
    }

    const Vector<char> startKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::OriginName);
    const Vector<char> stopKey = DatabaseMetaDataKey::encode(databaseId + 1, DatabaseMetaDataKey::OriginName);
    if (!deleteRange(transaction.get(), startKey, stopKey)) {
        transaction->rollback();
        return false;
    }

    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    transaction->remove(key);

    return transaction->commit();
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

void IDBLevelDBBackingStore::getObjectStores(int64_t databaseId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<IDBKeyPath>& foundKeyPaths, Vector<bool>& foundAutoIncrementFlags)
{
    IDB_TRACE("IDBLevelDBBackingStore::getObjectStores");
    const Vector<char> startKey = ObjectStoreMetaDataKey::encode(databaseId, 1, 0);
    const Vector<char> stopKey = ObjectStoreMetaDataKey::encodeMaxKey(databaseId);

    ASSERT(foundIds.isEmpty());
    ASSERT(foundNames.isEmpty());
    ASSERT(foundKeyPaths.isEmpty());
    ASSERT(foundAutoIncrementFlags.isEmpty());

    OwnPtr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        ObjectStoreMetaDataKey metaDataKey;
        p = ObjectStoreMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);
        if (metaDataKey.metaDataType() != ObjectStoreMetaDataKey::Name) {
            LOG_ERROR("Internal Indexed DB error.");
            // Possible stale metadata, but don't fail the load.
            it->next();
            continue;
        }

        int64_t objectStoreId = metaDataKey.objectStoreId();

        // FIXME: Do this by direct key lookup rather than iteration, to simplify.
        String objectStoreName = decodeString(it->value().begin(), it->value().end());

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::KeyPath)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        IDBKeyPath keyPath = decodeIDBKeyPath(it->value().begin(), it->value().end());

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::AutoIncrement)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        bool autoIncrement = decodeBool(it->value().begin(), it->value().end());

        it->next(); // Is evicatble.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::Evictable)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Last version.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::LastVersion)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Maximum index id allocated.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::MaxIndexId)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // [optional] has key path (is not null)
        if (checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::HasKeyPath)) {
            bool hasKeyPath = decodeBool(it->value().begin(), it->value().end());
            // This check accounts for two layers of legacy coding:
            // (1) Initially, hasKeyPath was added to distinguish null vs. string.
            // (2) Later, null vs. string vs. array was stored in the keyPath itself.
            // So this check is only relevant for string-type keyPaths.
            if (!hasKeyPath && (keyPath.type() == IDBKeyPath::StringType && !keyPath.string().isEmpty())) {
                LOG_ERROR("Internal Indexed DB error.");
                return;
            }
            if (!hasKeyPath)
                keyPath = IDBKeyPath();
            it->next();
        }

        int64_t keyGeneratorCurrentNumber = -1;
        if (checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber)) {
            keyGeneratorCurrentNumber = decodeInt(it->value().begin(), it->value().end());
            // FIXME: Return keyGeneratorCurrentNumber, cache in object store, and write lazily to backing store.
            // For now, just assert that if it was written it was valid.
            ASSERT_UNUSED(keyGeneratorCurrentNumber, keyGeneratorCurrentNumber >= KeyGeneratorInitialNumber);
            it->next();
        }

        foundIds.append(objectStoreId);
        foundNames.append(objectStoreName);
        foundKeyPaths.append(keyPath);
        foundAutoIncrementFlags.append(autoIncrement);
    }
}

static int64_t getNewObjectStoreId(LevelDBTransaction* transaction, int64_t databaseId)
{
    int64_t maxObjectStoreId = -1;
    const Vector<char> maxObjectStoreIdKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::MaxObjectStoreId);
    if (!getInt(transaction, maxObjectStoreIdKey, maxObjectStoreId))
        maxObjectStoreId = 0;

    ASSERT(maxObjectStoreId >= 0);

    int64_t objectStoreId = maxObjectStoreId + 1;
    if (!putInt(transaction, maxObjectStoreIdKey, objectStoreId))
        return -1;

    return objectStoreId;
}

bool IDBLevelDBBackingStore::createObjectStore(int64_t databaseId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement, int64_t& assignedObjectStoreId)
{
    IDB_TRACE("IDBLevelDBBackingStore::createObjectStore");
    ASSERT(m_currentTransaction);
    int64_t objectStoreId = getNewObjectStoreId(m_currentTransaction.get(), databaseId);
    if (objectStoreId < 0)
        return false;

    const Vector<char> nameKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::Name);
    const Vector<char> keyPathKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyPath);
    const Vector<char> autoIncrementKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::AutoIncrement);
    const Vector<char> evictableKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::Evictable);
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::LastVersion);
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::MaxIndexId);
    const Vector<char> hasKeyPathKey  = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::HasKeyPath);
    const Vector<char> keyGeneratorCurrentNumberKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber);
    const Vector<char> namesKey = ObjectStoreNamesKey::encode(databaseId, name);

    bool ok = putString(m_currentTransaction.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putIDBKeyPath(m_currentTransaction.get(), keyPathKey, keyPath);
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

    ok = putInt(m_currentTransaction.get(), maxIndexIdKey, MinimumIndexId);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putBool(m_currentTransaction.get(), hasKeyPathKey, !keyPath.isNull());
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_currentTransaction.get(), keyGeneratorCurrentNumberKey, KeyGeneratorInitialNumber);
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
    IDB_TRACE("IDBLevelDBBackingStore::deleteObjectStore");
    ASSERT(m_currentTransaction);

    String objectStoreName;
    getString(m_currentTransaction.get(), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::Name), objectStoreName);

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
    IDB_TRACE("IDBLevelDBBackingStore::getObjectStoreRecord");
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
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::LastVersion);

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
    IDB_TRACE("IDBLevelDBBackingStore::putObjectStoreRecord");
    ASSERT(key.isValid());
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
    IDB_TRACE("IDBLevelDBBackingStore::clearObjectStore");
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
    IDB_TRACE("IDBLevelDBBackingStore::deleteObjectStoreRecord");
    ASSERT(m_currentTransaction);
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);

    const Vector<char> objectStoreDataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, levelDBRecordIdentifier->primaryKey());
    m_currentTransaction->remove(objectStoreDataKey);

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, levelDBRecordIdentifier->primaryKey());
    m_currentTransaction->remove(existsEntryKey);
}


int64_t IDBLevelDBBackingStore::getKeyGeneratorCurrentNumber(int64_t databaseId, int64_t objectStoreId)
{
    ASSERT(m_currentTransaction);

    const Vector<char> keyGeneratorCurrentNumberKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber);

    int64_t keyGeneratorCurrentNumber = -1;
    Vector<char> data;

    if (m_currentTransaction->get(keyGeneratorCurrentNumberKey, data))
        keyGeneratorCurrentNumber = decodeInt(data.begin(), data.end());
    else {
        // Previously, the key generator state was not stored explicitly but derived from the
        // maximum numeric key present in existing data. This violates the spec as the data may
        // be cleared but the key generator state must be preserved.
        const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
        const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

        OwnPtr<LevelDBIterator> it = m_currentTransaction->createIterator();
        int64_t maxNumericKey = 0;

        for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
            const char* p = it->key().begin();
            const char* limit = it->key().end();

            ObjectStoreDataKey dataKey;
            p = ObjectStoreDataKey::decode(p, limit, &dataKey);
            ASSERT(p);

            if (dataKey.userKey()->type() == IDBKey::NumberType) {
                int64_t n = static_cast<int64_t>(dataKey.userKey()->number());
                if (n > maxNumericKey)
                    maxNumericKey = n;
            }
        }

        keyGeneratorCurrentNumber = maxNumericKey + 1;
    }

    return keyGeneratorCurrentNumber;
}

bool IDBLevelDBBackingStore::maybeUpdateKeyGeneratorCurrentNumber(int64_t databaseId, int64_t objectStoreId, int64_t newNumber, bool checkCurrent)
{
    ASSERT(m_currentTransaction);

    if (checkCurrent) {
        int64_t currentNumber = getKeyGeneratorCurrentNumber(databaseId, objectStoreId);
        if (newNumber <= currentNumber)
            return true;
    }

    const Vector<char> keyGeneratorCurrentNumberKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber);
    bool ok = putInt(m_currentTransaction.get(), keyGeneratorCurrentNumberKey, newNumber);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }
    return true;
}

bool IDBLevelDBBackingStore::keyExistsInObjectStore(int64_t databaseId, int64_t objectStoreId, const IDBKey& key, ObjectStoreRecordIdentifier* foundRecordIdentifier)
{
    IDB_TRACE("IDBLevelDBBackingStore::keyExistsInObjectStore");
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
        const char* p = it->key().begin();
        const char* limit = it->key().end();
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

        if (!callback.callback(ri.get(), idbValue))
            return false;
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


void IDBLevelDBBackingStore::getIndexes(int64_t databaseId, int64_t objectStoreId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<IDBKeyPath>& foundKeyPaths, Vector<bool>& foundUniqueFlags, Vector<bool>& foundMultiEntryFlags)
{
    IDB_TRACE("IDBLevelDBBackingStore::getIndexes");
    const Vector<char> startKey = IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0);
    const Vector<char> stopKey = IndexMetaDataKey::encode(databaseId, objectStoreId + 1, 0, 0);

    ASSERT(foundIds.isEmpty());
    ASSERT(foundNames.isEmpty());
    ASSERT(foundKeyPaths.isEmpty());
    ASSERT(foundUniqueFlags.isEmpty());
    ASSERT(foundMultiEntryFlags.isEmpty());

    OwnPtr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        IndexMetaDataKey metaDataKey;
        p = IndexMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);
        if (metaDataKey.metaDataType() != IndexMetaDataKey::Name) {
            LOG_ERROR("Internal Indexed DB error.");
            // Possible stale metadata due to http://webkit.org/b/85557 but don't fail the load.
            it->next();
            continue;
        }

        // FIXME: Do this by direct key lookup rather than iteration, to simplify.
        int64_t indexId = metaDataKey.indexId();
        String indexName = decodeString(it->value().begin(), it->value().end());

        it->next(); // unique flag
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::Unique)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        bool indexUnique = decodeBool(it->value().begin(), it->value().end());

        it->next(); // keyPath
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::KeyPath)) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        IDBKeyPath keyPath = decodeIDBKeyPath(it->value().begin(), it->value().end());

        it->next(); // [optional] multiEntry flag
        bool indexMultiEntry = false;
        if (checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::MultiEntry)) {
            indexMultiEntry = decodeBool(it->value().begin(), it->value().end());
            it->next();
        }

        foundIds.append(indexId);
        foundNames.append(indexName);
        foundKeyPaths.append(keyPath);
        foundUniqueFlags.append(indexUnique);
        foundMultiEntryFlags.append(indexMultiEntry);
    }
}

static int64_t getNewIndexId(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId)
{
    int64_t maxIndexId = -1;
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::MaxIndexId);
    if (!getInt(transaction, maxIndexIdKey, maxIndexId))
        maxIndexId = MinimumIndexId;

    ASSERT(maxIndexId >= 0);

    int64_t indexId = maxIndexId + 1;
    if (!putInt(transaction, maxIndexIdKey, indexId))
        return -1;

    return indexId;
}

bool IDBLevelDBBackingStore::createIndex(int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool isUnique, bool isMultiEntry, int64_t& indexId)
{
    IDB_TRACE("IDBLevelDBBackingStore::createIndex");
    ASSERT(m_currentTransaction);
    indexId = getNewIndexId(m_currentTransaction.get(), databaseId, objectStoreId);
    if (indexId < 0)
        return false;

    const Vector<char> nameKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::Name);
    const Vector<char> uniqueKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::Unique);
    const Vector<char> keyPathKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::KeyPath);
    const Vector<char> multiEntryKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::MultiEntry);

    bool ok = putString(m_currentTransaction.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putBool(m_currentTransaction.get(), uniqueKey, isUnique);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putIDBKeyPath(m_currentTransaction.get(), keyPathKey, keyPath);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putBool(m_currentTransaction.get(), multiEntryKey, isMultiEntry);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    return true;
}

void IDBLevelDBBackingStore::deleteIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    IDB_TRACE("IDBLevelDBBackingStore::deleteIndex");
    ASSERT(m_currentTransaction);

    const Vector<char> indexMetaDataStart = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 0);
    const Vector<char> indexMetaDataEnd = IndexMetaDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);

    if (!deleteRange(m_currentTransaction.get(), indexMetaDataStart, indexMetaDataEnd)) {
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
    IDB_TRACE("IDBLevelDBBackingStore::putIndexDataForRecord");
    ASSERT(key.isValid());
    ASSERT(indexId >= MinimumIndexId);
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);

    ASSERT(m_currentTransaction);
    const Vector<char> indexDataKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, encodeIDBKey(key), levelDBRecordIdentifier->primaryKey());

    Vector<char> data;
    data.append(encodeVarInt(levelDBRecordIdentifier->version()));
    data.append(levelDBRecordIdentifier->primaryKey());

    return m_currentTransaction->put(indexDataKey, data);
}

static bool findGreatestKeyLessThanOrEqual(LevelDBTransaction* transaction, const Vector<char>& target, Vector<char>& foundKey)
{
    OwnPtr<LevelDBIterator> it = transaction->createIterator();
    it->seek(target);

    if (!it->isValid()) {
        it->seekToLast();
        if (!it->isValid())
            return false;
    }

    while (compareIndexKeys(it->key(), target) > 0) {
        it->prev();
        if (!it->isValid())
            return false;
    }

    do {
        foundKey.clear();
        foundKey.append(it->key().begin(), it->key().end() - it->key().begin());

        // There can be several index keys that compare equal. We want the last one.
        it->next();
    } while (it->isValid() && !compareIndexKeys(it->key(), target));

    return true;
}

bool IDBLevelDBBackingStore::deleteIndexDataForRecord(int64_t, int64_t, int64_t, const ObjectStoreRecordIdentifier*)
{
    // FIXME: This isn't needed since we invalidate index data via the version number mechanism.
    return true;
}

static bool versionExists(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t version, const Vector<char>& encodedPrimaryKey)
{
    const Vector<char> key = ExistsEntryKey::encode(databaseId, objectStoreId, encodedPrimaryKey);
    Vector<char> data;

    if (!transaction->get(key, data))
        return false;

    return decodeInt(data.begin(), data.end()) == version;
}

bool IDBLevelDBBackingStore::findKeyInIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, Vector<char>& foundEncodedPrimaryKey)
{
    IDB_TRACE("IDBLevelDBBackingStore::findKeyInIndex");
    ASSERT(foundEncodedPrimaryKey.isEmpty());

    const Vector<char> leveldbKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key);
    OwnPtr<LevelDBIterator> it = m_currentTransaction->createIterator();
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

        if (!versionExists(m_currentTransaction.get(), databaseId, objectStoreId, version, foundEncodedPrimaryKey)) {
            // Delete stale index data entry and continue.
            m_currentTransaction->remove(it->key());
            it->next();
            continue;
        }

        return true;
    }
}

PassRefPtr<IDBKey> IDBLevelDBBackingStore::getPrimaryKeyViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    IDB_TRACE("IDBLevelDBBackingStore::getPrimaryKeyViaIndex");
    ASSERT(m_currentTransaction);

    Vector<char> foundEncodedPrimaryKey;
    if (findKeyInIndex(databaseId, objectStoreId, indexId, key, foundEncodedPrimaryKey)) {
        RefPtr<IDBKey> primaryKey;
        decodeIDBKey(foundEncodedPrimaryKey.begin(), foundEncodedPrimaryKey.end(), primaryKey);
        return primaryKey.release();
    }

    return 0;
}

bool IDBLevelDBBackingStore::keyExistsInIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey)
{
    IDB_TRACE("IDBLevelDBBackingStore::keyExistsInIndex");
    ASSERT(m_currentTransaction);

    Vector<char> foundEncodedPrimaryKey;
    if (!findKeyInIndex(databaseId, objectStoreId, indexId, indexKey, foundEncodedPrimaryKey))
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
    virtual bool advance(unsigned long);
    virtual bool continueFunction(const IDBKey* = 0, IteratorState = Seek);
    virtual PassRefPtr<IDBKey> key() { return m_currentKey; }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_currentKey; }
    virtual String value() = 0;
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() = 0;
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

    bool isPastBounds() const;
    bool haveEnteredRange() const;

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

    return continueFunction(0, Ready);
}

bool CursorImplCommon::advance(unsigned long count)
{
    while (count--) {
        if (!continueFunction())
            return false;
    }
    return true;
}

bool CursorImplCommon::continueFunction(const IDBKey* key, IteratorState nextState)
{
    RefPtr<IDBKey> previousKey = m_currentKey;

    // When iterating with PREV_NO_DUPLICATE, spec requires that the
    // value we yield for each key is the first duplicate in forwards
    // order.
    RefPtr<IDBKey> lastDuplicateKey;

    bool forward = m_cursorOptions.forward;

    for (;;) {
        if (nextState == Seek) {
            if (forward)
                m_iterator->next();
            else
                m_iterator->prev();
        } else
            nextState = Seek; // for subsequent iterations

        if (!m_iterator->isValid()) {
            if (!forward && lastDuplicateKey.get()) {
                // We need to walk forward because we hit the end of
                // the data.
                forward = true;
                continue;
            }

            return false;
        }

        if (isPastBounds()) {
            if (!forward && lastDuplicateKey.get()) {
                // We need to walk forward because now we're beyond the
                // bounds defined by the cursor.
                forward = true;
                continue;
            }

            return false;
        }

        if (!haveEnteredRange())
            continue;

        // The row may not load because there's a stale entry in the
        // index. This is not fatal.
        if (!loadCurrentRow())
            continue;

        if (key) {
            if (forward) {
                if (m_currentKey->isLessThan(key))
                    continue;
            } else {
                if (key->isLessThan(m_currentKey.get()))
                    continue;
            }
        }

        if (m_cursorOptions.unique) {

            if (m_currentKey->isEqual(previousKey.get())) {
                // We should never be able to walk forward all the way
                // to the previous key.
                ASSERT(!lastDuplicateKey.get());
                continue;
            }

            if (!forward) {
                if (!lastDuplicateKey.get()) {
                    lastDuplicateKey = m_currentKey;
                    continue;
                }

                // We need to walk forward because we hit the boundary
                // between key ranges.
                if (!lastDuplicateKey->isEqual(m_currentKey.get())) {
                    forward = true;
                    continue;
                }

                continue;
            }
        }
        break;
    }

    ASSERT(!lastDuplicateKey.get() || (forward && lastDuplicateKey->isEqual(m_currentKey.get())));
    return true;
}

bool CursorImplCommon::haveEnteredRange() const
{
    if (m_cursorOptions.forward) {
        if (m_cursorOptions.lowOpen)
            return compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) > 0;

        return compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) >= 0;
    }
    if (m_cursorOptions.highOpen)
        return compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) < 0;

    return compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) <= 0;
}

bool CursorImplCommon::isPastBounds() const
{
    if (m_cursorOptions.forward) {
        if (m_cursorOptions.highOpen)
            return compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) >= 0;
        return compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) > 0;
    }

    if (m_cursorOptions.lowOpen)
        return compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) <= 0;
    return compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) < 0;
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
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() OVERRIDE
    {
        return m_identifier;
    }
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
    RefPtr<LevelDBRecordIdentifier> m_identifier;
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

    // FIXME: This re-encodes what was just decoded; try and optimize.
    m_identifier = LevelDBRecordIdentifier::create(encodeIDBKey(*m_currentKey), version);

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
    if (!m_transaction->get(primaryLevelDBKey, result)) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

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
    const char* p = m_iterator->key().begin();
    const char* limit = m_iterator->key().end();

    IndexDataKey indexDataKey;
    p = IndexDataKey::decode(p, limit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    const char* q = m_iterator->value().begin();
    const char* valueLimit = m_iterator->value().end();

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
    if (!m_transaction->get(m_primaryLevelDBKey, result)) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

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

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openObjectStoreCursor(int64_t databaseId, int64_t objectStoreId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    IDB_TRACE("IDBLevelDBBackingStore::openObjectStoreCursor");
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

        if (cursorOptions.forward)
            cursorOptions.highOpen = true; // Not included.
        else {
            // We need a key that exists.
            if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();

        if (!cursorOptions.forward) {
            // For reverse cursors, we need a key that exists.
            Vector<char> foundHighKey;
            if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, foundHighKey))
                return 0;

            // If the target key should not be included, but we end up with a smaller key, we should include that.
            if (cursorOptions.highOpen && compareIndexKeys(foundHighKey, cursorOptions.highKey) < 0)
                cursorOptions.highOpen = false;

            cursorOptions.highKey = foundHighKey;
        }
    }

    RefPtr<ObjectStoreCursorImpl> cursor = ObjectStoreCursorImpl::create(m_currentTransaction.get(), cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexKeyCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    IDB_TRACE("IDBLevelDBBackingStore::openIndexKeyCursor");
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
            if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();

        Vector<char> foundHighKey;
        if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, foundHighKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;

        // If the target key should not be included, but we end up with a smaller key, we should include that.
        if (cursorOptions.highOpen && compareIndexKeys(foundHighKey, cursorOptions.highKey) < 0)
            cursorOptions.highOpen = false;

        cursorOptions.highKey = foundHighKey;
    }

    RefPtr<IndexKeyCursorImpl> cursor = IndexKeyCursorImpl::create(m_currentTransaction.get(), cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    IDB_TRACE("IDBLevelDBBackingStore::openIndexCursor");
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
            if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, cursorOptions.highKey))
                return 0;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();

        Vector<char> foundHighKey;
        if (!findGreatestKeyLessThanOrEqual(m_currentTransaction.get(), cursorOptions.highKey, foundHighKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;

        // If the target key should not be included, but we end up with a smaller key, we should include that.
        if (cursorOptions.highOpen && compareIndexKeys(foundHighKey, cursorOptions.highKey) < 0)
            cursorOptions.highOpen = false;

        cursorOptions.highKey = foundHighKey;
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

bool IDBLevelDBBackingStore::Transaction::commit()
{
    IDB_TRACE("IDBLevelDBBackingStore::Transaction::commit");
    ASSERT(m_backingStore->m_currentTransaction);
    bool result = m_backingStore->m_currentTransaction->commit();
    m_backingStore->m_currentTransaction.clear();
    return result;
}

void IDBLevelDBBackingStore::Transaction::rollback()
{
    IDB_TRACE("IDBLevelDBBackingStore::Transaction::rollback");
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
