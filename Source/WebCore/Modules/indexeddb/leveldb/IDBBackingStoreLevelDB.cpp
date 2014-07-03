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
#include "IDBBackingStoreLevelDB.h"

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "FileSystem.h"
#include "IDBDatabaseMetadata.h"
#include "IDBIndexWriterLevelDB.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBLevelDBCoding.h"
#include "IDBTransactionBackend.h"
#include "LevelDBComparator.h"
#include "LevelDBDatabase.h"
#include "LevelDBIterator.h"
#include "LevelDBSlice.h"
#include "LevelDBTransaction.h"
#include "Logging.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>

namespace WebCore {

using namespace IDBLevelDBCoding;

const int64_t KeyGeneratorInitialNumber = 1; // From the IndexedDB specification.

enum IDBBackingStoreLevelDBErrorSource {
    // 0 - 2 are no longer used.
    FindKeyInIndex = 3,
    GetIDBDatabaseMetaData,
    GetIndexes,
    GetKeyGeneratorCurrentNumber,
    GetObjectStores,
    GetRecord,
    KeyExistsInObjectStore,
    LoadCurrentRow,
    SetupMetadata,
    GetPrimaryKeyViaIndex,
    KeyExistsInIndex,
    VersionExists,
    DeleteObjectStore,
    SetMaxObjectStoreId,
    SetMaxIndexId,
    GetNewDatabaseId,
    GetNewVersionNumber,
    CreateIDBDatabaseMetaData,
    DeleteDatabase,
    IDBLevelDBBackingStoreInternalErrorMax,
};

// Use to signal conditions that usually indicate developer error, but could be caused by data corruption.
// A macro is used instead of an inline function so that the assert and log report the line number.
#define REPORT_ERROR(type, location) \
    do { \
        LOG_ERROR("IndexedDB %s Error: %s", type, #location); \
        ASSERT_NOT_REACHED(); \
    } while (0)

#define INTERNAL_READ_ERROR(location) REPORT_ERROR("Read", location)
#define INTERNAL_CONSISTENCY_ERROR(location) REPORT_ERROR("Consistency", location)
#define INTERNAL_WRITE_ERROR(location) REPORT_ERROR("Write", location)

static void putBool(LevelDBTransaction* transaction, const LevelDBSlice& key, bool value)
{
    transaction->put(key, encodeBool(value));
}

template <typename DBOrTransaction>
static bool getInt(DBOrTransaction* db, const LevelDBSlice& key, int64_t& foundInt, bool& found)
{
    Vector<char> result;
    bool ok = db->safeGet(key, result, found);
    if (!ok)
        return false;
    if (!found)
        return true;

    foundInt = decodeInt(result.begin(), result.end());
    return true;
}

static void putInt(LevelDBTransaction* transaction, const LevelDBSlice& key, int64_t value)
{
    ASSERT(value >= 0);
    transaction->put(key, encodeInt(value));
}

template <typename DBOrTransaction>
WARN_UNUSED_RETURN static bool getVarInt(DBOrTransaction* db, const LevelDBSlice& key, int64_t& foundInt, bool& found)
{
    Vector<char> result;
    bool ok = db->safeGet(key, result, found);
    if (!ok)
        return false;
    if (!found)
        return true;

    found = decodeVarInt(result.begin(), result.end(), foundInt) == result.end();
    return true;
}

static void putVarInt(LevelDBTransaction* transaction, const LevelDBSlice& key, int64_t value)
{
    transaction->put(key, encodeVarInt(value));
}

template <typename DBOrTransaction>
WARN_UNUSED_RETURN static bool getString(DBOrTransaction* db, const LevelDBSlice& key, String& foundString, bool& found)
{
    Vector<char> result;
    found = false;
    bool ok = db->safeGet(key, result, found);
    if (!ok)
        return false;
    if (!found)
        return true;

    foundString = decodeString(result.begin(), result.end());
    return true;
}

static void putString(LevelDBTransaction* transaction, const LevelDBSlice& key, const String& value)
{
    transaction->put(key, encodeString(value));
}

static void putIDBKeyPath(LevelDBTransaction* transaction, const LevelDBSlice& key, const IDBKeyPath& value)
{
    transaction->put(key, encodeIDBKeyPath(value));
}

static int compareKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return compare(a, b);
}

int IDBBackingStoreLevelDB::compareIndexKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return compare(a, b, true);
}

class Comparator : public LevelDBComparator {
public:
    virtual int compare(const LevelDBSlice& a, const LevelDBSlice& b) const { return IDBLevelDBCoding::compare(a, b); }
    virtual const char* name() const { return "idb_cmp1"; }
};

// 0 - Initial version.
// 1 - Adds UserIntVersion to DatabaseMetaData.
// 2 - Adds DataVersion to to global metadata.
const int64_t latestKnownSchemaVersion = 2;
WARN_UNUSED_RETURN static bool isSchemaKnown(LevelDBDatabase* db, bool& known)
{
    int64_t dbSchemaVersion = 0;
    bool found = false;
    bool ok = getInt(db, SchemaVersionKey::encode(), dbSchemaVersion, found);
    if (!ok)
        return false;
    if (!found) {
        known = true;
        return true;
    }
    if (dbSchemaVersion > latestKnownSchemaVersion) {
        known = false;
        return true;
    }

    const uint32_t latestKnownDataVersion = SerializedScriptValue::wireFormatVersion();
    int64_t dbDataVersion = 0;
    ok = getInt(db, DataVersionKey::encode(), dbDataVersion, found);
    if (!ok)
        return false;
    if (!found) {
        known = true;
        return true;
    }

    if (dbDataVersion > latestKnownDataVersion) {
        known = false;
        return true;
    }

    known = true;
    return true;
}

WARN_UNUSED_RETURN static bool setUpMetadata(LevelDBDatabase* db, const String& origin)
{
    const uint32_t latestKnownDataVersion = SerializedScriptValue::wireFormatVersion();
    const Vector<char> schemaVersionKey = SchemaVersionKey::encode();
    const Vector<char> dataVersionKey = DataVersionKey::encode();

    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(db);

    int64_t dbSchemaVersion = 0;
    int64_t dbDataVersion = 0;
    bool found = false;
    bool ok = getInt(transaction.get(), schemaVersionKey, dbSchemaVersion, found);
    if (!ok) {
        INTERNAL_READ_ERROR(SetupMetadata);
        return false;
    }
    if (!found) {
        // Initialize new backing store.
        dbSchemaVersion = latestKnownSchemaVersion;
        putInt(transaction.get(), schemaVersionKey, dbSchemaVersion);
        dbDataVersion = latestKnownDataVersion;
        putInt(transaction.get(), dataVersionKey, dbDataVersion);
    } else {
        // Upgrade old backing store.
        ASSERT(dbSchemaVersion <= latestKnownSchemaVersion);
        if (dbSchemaVersion < 1) {
            dbSchemaVersion = 1;
            putInt(transaction.get(), schemaVersionKey, dbSchemaVersion);
            const Vector<char> startKey = DatabaseNameKey::encodeMinKeyForOrigin(origin);
            const Vector<char> stopKey = DatabaseNameKey::encodeStopKeyForOrigin(origin);
            std::unique_ptr<LevelDBIterator> it = db->createIterator();
            for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
                int64_t databaseId = 0;
                found = false;
                bool ok = getInt(transaction.get(), it->key(), databaseId, found);
                if (!ok) {
                    INTERNAL_READ_ERROR(SetupMetadata);
                    return false;
                }
                if (!found) {
                    INTERNAL_CONSISTENCY_ERROR(SetupMetadata);
                    return false;
                }
                Vector<char> intVersionKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::UserIntVersion);
                putVarInt(transaction.get(), intVersionKey, IDBDatabaseMetadata::DefaultIntVersion);
            }
        }
        if (dbSchemaVersion < 2) {
            dbSchemaVersion = 2;
            putInt(transaction.get(), schemaVersionKey, dbSchemaVersion);
            dbDataVersion = SerializedScriptValue::wireFormatVersion();
            putInt(transaction.get(), dataVersionKey, dbDataVersion);
        }
    }

    // All new values will be written using this serialization version.
    found = false;
    ok = getInt(transaction.get(), dataVersionKey, dbDataVersion, found);
    if (!ok) {
        INTERNAL_READ_ERROR(SetupMetadata);
        return false;
    }
    if (!found) {
        INTERNAL_CONSISTENCY_ERROR(SetupMetadata);
        return false;
    }
    if (dbDataVersion < latestKnownDataVersion) {
        dbDataVersion = latestKnownDataVersion;
        putInt(transaction.get(), dataVersionKey, dbDataVersion);
    }

    ASSERT(dbSchemaVersion == latestKnownSchemaVersion);
    ASSERT(dbDataVersion == latestKnownDataVersion);

    if (!transaction->commit()) {
        INTERNAL_WRITE_ERROR(SetupMetadata);
        return false;
    }
    return true;
}

template <typename DBOrTransaction>
WARN_UNUSED_RETURN static bool getMaxObjectStoreId(DBOrTransaction* db, int64_t databaseId, int64_t& maxObjectStoreId)
{
    const Vector<char> maxObjectStoreIdKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::MaxObjectStoreId);
    bool ok = getMaxObjectStoreId(db, maxObjectStoreIdKey, maxObjectStoreId);
    return ok;
}

template <typename DBOrTransaction>
WARN_UNUSED_RETURN static bool getMaxObjectStoreId(DBOrTransaction* db, const Vector<char>& maxObjectStoreIdKey, int64_t& maxObjectStoreId)
{
    maxObjectStoreId = -1;
    bool found = false;
    bool ok = getInt(db, maxObjectStoreIdKey, maxObjectStoreId, found);
    if (!ok)
        return false;
    if (!found)
        maxObjectStoreId = 0;

    ASSERT(maxObjectStoreId >= 0);
    return true;
}

class DefaultLevelDBFactory : public LevelDBFactory {
public:
    virtual std::unique_ptr<LevelDBDatabase> openLevelDB(const String& fileName, const LevelDBComparator* comparator)
    {
        return LevelDBDatabase::open(fileName, comparator);
    }
    virtual bool destroyLevelDB(const String& fileName)
    {
        return LevelDBDatabase::destroy(fileName);
    }
};

IDBBackingStoreLevelDB::IDBBackingStoreLevelDB(const String& identifier, std::unique_ptr<LevelDBDatabase> db, std::unique_ptr<LevelDBComparator> comparator)
    : m_identifier(identifier)
    , m_db(WTF::move(db))
    , m_comparator(WTF::move(comparator))
    , m_weakFactory(this)
{
}

IDBBackingStoreLevelDB::~IDBBackingStoreLevelDB()
{
    // m_db's destructor uses m_comparator. The order of destruction is important.
    m_db = nullptr;
    m_comparator = nullptr;
}

PassRefPtr<IDBBackingStoreLevelDB> IDBBackingStoreLevelDB::open(const SecurityOrigin& securityOrigin, const String& pathBaseArg, const String& fileIdentifier)
{
    DefaultLevelDBFactory levelDBFactory;
    return IDBBackingStoreLevelDB::open(securityOrigin, pathBaseArg, fileIdentifier, &levelDBFactory);
}

PassRefPtr<IDBBackingStoreLevelDB> IDBBackingStoreLevelDB::open(const SecurityOrigin& securityOrigin, const String& pathBaseArg, const String& fileIdentifier, LevelDBFactory* levelDBFactory)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::open");
    ASSERT(!pathBaseArg.isEmpty());
    String pathBase = pathBaseArg;

    std::unique_ptr<LevelDBComparator> comparator = std::make_unique<Comparator>();
    std::unique_ptr<LevelDBDatabase> db;

    if (!makeAllDirectories(pathBase)) {
        LOG_ERROR("Unable to create IndexedDB database path %s", pathBase.utf8().data());
        return PassRefPtr<IDBBackingStoreLevelDB>();
    }

    String path = pathByAppendingComponent(pathBase, securityOrigin.databaseIdentifier() + ".indexeddb.leveldb");

    db = levelDBFactory->openLevelDB(path, comparator.get());
    if (db) {
        bool known = false;
        bool ok = isSchemaKnown(db.get(), known);
        if (!ok) {
            LOG_ERROR("IndexedDB had IO error checking schema, treating it as failure to open");
            db = nullptr;
        } else if (!known) {
            LOG_ERROR("IndexedDB backing store had unknown schema, treating it as failure to open");
            db = nullptr;
        }
    }

    if (!db) {
        LOG_ERROR("IndexedDB backing store open failed, attempting cleanup");
        bool success = levelDBFactory->destroyLevelDB(path);
        if (!success) {
            LOG_ERROR("IndexedDB backing store cleanup failed");
            return PassRefPtr<IDBBackingStoreLevelDB>();
        }

        LOG_ERROR("IndexedDB backing store cleanup succeeded, reopening");
        db = levelDBFactory->openLevelDB(path, comparator.get());
        if (!db) {
            LOG_ERROR("IndexedDB backing store reopen after recovery failed");
            return PassRefPtr<IDBBackingStoreLevelDB>();
        }
    }

    if (!db) {
        ASSERT_NOT_REACHED();
        return PassRefPtr<IDBBackingStoreLevelDB>();
    }

    return create(fileIdentifier, WTF::move(db), WTF::move(comparator));
}

PassRefPtr<IDBBackingStoreLevelDB> IDBBackingStoreLevelDB::openInMemory(const String& identifier)
{
    DefaultLevelDBFactory levelDBFactory;
    return IDBBackingStoreLevelDB::openInMemory(identifier, &levelDBFactory);
}

PassRefPtr<IDBBackingStoreLevelDB> IDBBackingStoreLevelDB::openInMemory(const String& identifier, LevelDBFactory*)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::openInMemory");

    std::unique_ptr<LevelDBComparator> comparator = std::make_unique<Comparator>();
    std::unique_ptr<LevelDBDatabase> db = LevelDBDatabase::openInMemory(comparator.get());
    if (!db) {
        LOG_ERROR("LevelDBDatabase::openInMemory failed.");
        return PassRefPtr<IDBBackingStoreLevelDB>();
    }

    return create(identifier, WTF::move(db), WTF::move(comparator));
}

PassRefPtr<IDBBackingStoreLevelDB> IDBBackingStoreLevelDB::create(const String& identifier, std::unique_ptr<LevelDBDatabase> db, std::unique_ptr<LevelDBComparator> comparator)
{
    // FIXME: Handle comparator name changes.
    RefPtr<IDBBackingStoreLevelDB> backingStore(adoptRef(new IDBBackingStoreLevelDB(identifier, WTF::move(db), WTF::move(comparator))));

    if (!setUpMetadata(backingStore->m_db.get(), identifier))
        return PassRefPtr<IDBBackingStoreLevelDB>();

    return backingStore.release();
}

Vector<String> IDBBackingStoreLevelDB::getDatabaseNames()
{
    Vector<String> foundNames;
    const Vector<char> startKey = DatabaseNameKey::encodeMinKeyForOrigin(m_identifier);
    const Vector<char> stopKey = DatabaseNameKey::encodeStopKeyForOrigin(m_identifier);

    ASSERT(foundNames.isEmpty());

    std::unique_ptr<LevelDBIterator> it = m_db->createIterator();
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        DatabaseNameKey databaseNameKey;
        p = DatabaseNameKey::decode(p, limit, &databaseNameKey);
        ASSERT(p);

        foundNames.append(databaseNameKey.databaseName());
    }
    return foundNames;
}

bool IDBBackingStoreLevelDB::getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata* metadata, bool& found)
{
    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    found = false;

    bool ok = getInt(m_db.get(), key, metadata->id, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        return false;
    }
    if (!found)
        return true;

    // FIXME: The string version is no longer supported, so the levelDB ports should consider refactoring off of it.
    String stringVersion;
    ok = getString(m_db.get(), DatabaseMetaDataKey::encode(metadata->id, DatabaseMetaDataKey::UserVersion), stringVersion, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        return false;
    }
    if (!found) {
        INTERNAL_CONSISTENCY_ERROR(GetIDBDatabaseMetaData);
        return false;
    }

    int64_t version;
    ok = getVarInt(m_db.get(), DatabaseMetaDataKey::encode(metadata->id, DatabaseMetaDataKey::UserIntVersion), version, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        return false;
    }
    if (!found) {
        INTERNAL_CONSISTENCY_ERROR(GetIDBDatabaseMetaData);
        return false;
    }

    // FIXME: The versioning semantics have changed since this original code was written, and what was once a negative number
    // stored in the database is no longer a valid version.
    if (version < 0)
        version = 0;
    metadata->version = version;

    ok = getMaxObjectStoreId(m_db.get(), metadata->id, metadata->maxObjectStoreId);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        return false;
    }

    return true;
}

void IDBBackingStoreLevelDB::getOrEstablishIDBDatabaseMetadata(const String& name, std::function<void (const IDBDatabaseMetadata&, bool success)> metadataFunction)
{
    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    bool found = false;

    IDBDatabaseMetadata resultMetadata;
    
    bool ok = getInt(m_db.get(), key, resultMetadata.id, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        metadataFunction(resultMetadata, false);
        return;
    }

    if (!found) {
        resultMetadata.name = name;
        resultMetadata.version = IDBDatabaseMetadata::DefaultIntVersion;

        metadataFunction(resultMetadata, createIDBDatabaseMetaData(resultMetadata));
        return;
    }

    int64_t version;
    ok = getVarInt(m_db.get(), DatabaseMetaDataKey::encode(resultMetadata.id, DatabaseMetaDataKey::UserIntVersion), version, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        metadataFunction(resultMetadata, false);
        return;
    }
    if (!found) {
        INTERNAL_CONSISTENCY_ERROR(GetIDBDatabaseMetaData);
        metadataFunction(resultMetadata, false);
        return;
    }

    // FIXME: The versioning semantics have changed since this original code was written, and what was once a negative number
    // stored in the database is no longer a valid version.
    if (version < 0)
        version = 0;
    resultMetadata.version = version;

    ok = getMaxObjectStoreId(m_db.get(), resultMetadata.id, resultMetadata.maxObjectStoreId);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        metadataFunction(resultMetadata, false);
        return;
    }

    ok = getObjectStores(resultMetadata.id, &resultMetadata.objectStores);
    if (!ok) {
        INTERNAL_READ_ERROR(GetIDBDatabaseMetaData);
        metadataFunction(resultMetadata, false);
        return;
    }

    metadataFunction(resultMetadata, true);
}

WARN_UNUSED_RETURN static bool getNewDatabaseId(LevelDBDatabase* db, int64_t& newId)
{
    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(db);

    newId = -1;
    int64_t maxDatabaseId = -1;
    bool found = false;
    bool ok = getInt(transaction.get(), MaxDatabaseIdKey::encode(), maxDatabaseId, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetNewDatabaseId);
        return false;
    }
    if (!found)
        maxDatabaseId = 0;

    ASSERT(maxDatabaseId >= 0);

    int64_t databaseId = maxDatabaseId + 1;
    putInt(transaction.get(), MaxDatabaseIdKey::encode(), databaseId);
    if (!transaction->commit()) {
        INTERNAL_WRITE_ERROR(GetNewDatabaseId);
        return false;
    }
    newId = databaseId;
    return true;
}

// FIXME: LevelDB needs to support uint64_t as the version type.
bool IDBBackingStoreLevelDB::createIDBDatabaseMetaData(IDBDatabaseMetadata& metadata)
{
    bool ok = getNewDatabaseId(m_db.get(), metadata.id);
    if (!ok)
        return false;
    ASSERT(metadata.id >= 0);

    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(m_db.get());
    putInt(transaction.get(), DatabaseNameKey::encode(m_identifier, metadata.name), metadata.id);
    putVarInt(transaction.get(), DatabaseMetaDataKey::encode(metadata.id, DatabaseMetaDataKey::UserIntVersion), metadata.version);
    if (!transaction->commit()) {
        INTERNAL_WRITE_ERROR(CreateIDBDatabaseMetaData);
        return false;
    }
    return true;
}

bool IDBBackingStoreLevelDB::updateIDBDatabaseVersion(IDBBackingStoreTransactionLevelDB& transaction, int64_t rowId, uint64_t version)
{
    if (version == IDBDatabaseMetadata::NoIntVersion)
        version = IDBDatabaseMetadata::DefaultIntVersion;
    putVarInt(IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::UserIntVersion), version);
    return true;
}

static void deleteRange(LevelDBTransaction* transaction, const Vector<char>& begin, const Vector<char>& end)
{
    std::unique_ptr<LevelDBIterator> it = transaction->createIterator();
    for (it->seek(begin); it->isValid() && compareKeys(it->key(), end) < 0; it->next())
        transaction->remove(it->key());
}

void IDBBackingStoreLevelDB::deleteDatabase(const String& name, std::function<void (bool success)> boolCallbackFunction)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::deleteDatabase");
    std::unique_ptr<LevelDBWriteOnlyTransaction> transaction = std::make_unique<LevelDBWriteOnlyTransaction>(m_db.get());

    IDBDatabaseMetadata metadata;
    bool success = false;
    bool ok = getIDBDatabaseMetaData(name, &metadata, success);
    if (!ok) {
        boolCallbackFunction(false);
        return;
    }

    if (!success) {
        boolCallbackFunction(true);
        return;
    }

    const Vector<char> startKey = DatabaseMetaDataKey::encode(metadata.id, DatabaseMetaDataKey::OriginName);
    const Vector<char> stopKey = DatabaseMetaDataKey::encode(metadata.id + 1, DatabaseMetaDataKey::OriginName);
    std::unique_ptr<LevelDBIterator> it = m_db->createIterator();
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next())
        transaction->remove(it->key());

    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
    transaction->remove(key);

    if (!transaction->commit()) {
        INTERNAL_WRITE_ERROR(DeleteDatabase);
        boolCallbackFunction(false);
        return;
    }
    boolCallbackFunction(true);
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

// FIXME: This should do some error handling rather than plowing ahead when bad data is encountered.
bool IDBBackingStoreLevelDB::getObjectStores(int64_t databaseId, IDBDatabaseMetadata::ObjectStoreMap* objectStores)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::getObjectStores");
    if (!KeyPrefix::isValidDatabaseId(databaseId))
        return false;
    const Vector<char> startKey = ObjectStoreMetaDataKey::encode(databaseId, 1, 0);
    const Vector<char> stopKey = ObjectStoreMetaDataKey::encodeMaxKey(databaseId);

    ASSERT(objectStores->isEmpty());

    std::unique_ptr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        ObjectStoreMetaDataKey metaDataKey;
        p = ObjectStoreMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);
        if (metaDataKey.metaDataType() != ObjectStoreMetaDataKey::Name) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            // Possible stale metadata, but don't fail the load.
            it->next();
            continue;
        }

        int64_t objectStoreId = metaDataKey.objectStoreId();

        // FIXME: Do this by direct key lookup rather than iteration, to simplify.
        String objectStoreName = decodeString(it->value().begin(), it->value().end());

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::KeyPath)) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            break;
        }
        IDBKeyPath keyPath = decodeIDBKeyPath(it->value().begin(), it->value().end());

        it->next();
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::AutoIncrement)) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            break;
        }
        bool autoIncrement = decodeBool(it->value().begin(), it->value().end());

        it->next(); // Is evicatble.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::Evictable)) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            break;
        }

        it->next(); // Last version.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::LastVersion)) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            break;
        }

        it->next(); // Maximum index id allocated.
        if (!checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::MaxIndexId)) {
            INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
            break;
        }
        int64_t maxIndexId = decodeInt(it->value().begin(), it->value().end());

        it->next(); // [optional] has key path (is not null)
        if (checkObjectStoreAndMetaDataType(it.get(), stopKey, objectStoreId, ObjectStoreMetaDataKey::HasKeyPath)) {
            bool hasKeyPath = decodeBool(it->value().begin(), it->value().end());
            // This check accounts for two layers of legacy coding:
            // (1) Initially, hasKeyPath was added to distinguish null vs. string.
            // (2) Later, null vs. string vs. array was stored in the keyPath itself.
            // So this check is only relevant for string-type keyPaths.
            if (!hasKeyPath && (keyPath.type() == IDBKeyPath::StringType && !keyPath.string().isEmpty())) {
                INTERNAL_CONSISTENCY_ERROR(GetObjectStores);
                break;
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

        IDBObjectStoreMetadata metadata(objectStoreName, objectStoreId, keyPath, autoIncrement, maxIndexId);
        if (!getIndexes(databaseId, objectStoreId, &metadata.indexes))
            return false;
        objectStores->set(objectStoreId, metadata);
    }
    return true;
}

WARN_UNUSED_RETURN static bool setMaxObjectStoreId(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> maxObjectStoreIdKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::MaxObjectStoreId);
    int64_t maxObjectStoreId = -1;
    bool ok = getMaxObjectStoreId(transaction, maxObjectStoreIdKey, maxObjectStoreId);
    if (!ok) {
        INTERNAL_READ_ERROR(SetMaxObjectStoreId);
        return false;
    }

    if (objectStoreId <= maxObjectStoreId) {
        INTERNAL_CONSISTENCY_ERROR(SetMaxObjectStoreId);
        return false;
    }
    putInt(transaction, maxObjectStoreIdKey, objectStoreId);
    return true;
}

bool IDBBackingStoreLevelDB::createObjectStore(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::createObjectStore");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    if (!setMaxObjectStoreId(levelDBTransaction, databaseId, objectStoreId))
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

    putString(levelDBTransaction, nameKey, name);
    putIDBKeyPath(levelDBTransaction, keyPathKey, keyPath);
    putInt(levelDBTransaction, autoIncrementKey, autoIncrement);
    putInt(levelDBTransaction, evictableKey, false);
    putInt(levelDBTransaction, lastVersionKey, 1);
    putInt(levelDBTransaction, maxIndexIdKey, MinimumIndexId);
    putBool(levelDBTransaction, hasKeyPathKey, !keyPath.isNull());
    putInt(levelDBTransaction, keyGeneratorCurrentNumberKey, KeyGeneratorInitialNumber);
    putInt(levelDBTransaction, namesKey, objectStoreId);
    return true;
}

bool IDBBackingStoreLevelDB::deleteObjectStore(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::deleteObjectStore");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    String objectStoreName;
    bool found = false;
    bool ok = getString(levelDBTransaction, ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::Name), objectStoreName, found);
    if (!ok) {
        INTERNAL_READ_ERROR(DeleteObjectStore);
        return false;
    }
    if (!found) {
        INTERNAL_CONSISTENCY_ERROR(DeleteObjectStore);
        return false;
    }

    deleteRange(levelDBTransaction, ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0), ObjectStoreMetaDataKey::encodeMaxKey(databaseId, objectStoreId));

    levelDBTransaction->remove(ObjectStoreNamesKey::encode(databaseId, objectStoreName));

    deleteRange(levelDBTransaction, IndexFreeListKey::encode(databaseId, objectStoreId, 0), IndexFreeListKey::encodeMaxKey(databaseId, objectStoreId));
    deleteRange(levelDBTransaction, IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0), IndexMetaDataKey::encodeMaxKey(databaseId, objectStoreId));

    return clearObjectStore(transaction, databaseId, objectStoreId);
}

bool IDBBackingStoreLevelDB::getRecord(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, const IDBKey& key, Vector<char>& record)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::getRecord");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    record.clear();

    bool found = false;
    bool ok = levelDBTransaction->safeGet(leveldbKey, data, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetRecord);
        return false;
    }
    if (!found)
        return true;

    int64_t version;
    const char* p = decodeVarInt(data.begin(), data.end(), version);
    if (!p) {
        INTERNAL_READ_ERROR(GetRecord);
        return false;
    }

    record.appendRange(p, static_cast<const char*>(data.end()));
    return true;
}

WARN_UNUSED_RETURN static bool getNewVersionNumber(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t& newVersionNumber)
{
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::LastVersion);

    newVersionNumber = -1;
    int64_t lastVersion = -1;
    bool found = false;
    bool ok = getInt(transaction, lastVersionKey, lastVersion, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetNewVersionNumber);
        return false;
    }
    if (!found)
        lastVersion = 0;

    ASSERT(lastVersion >= 0);

    int64_t version = lastVersion + 1;
    putInt(transaction, lastVersionKey, version);

    ASSERT(version > lastVersion); // FIXME: Think about how we want to handle the overflow scenario.

    newVersionNumber = version;
    return true;
}

bool IDBBackingStoreLevelDB::putRecord(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, const IDBKey& key, PassRefPtr<SharedBuffer> prpValue, IDBRecordIdentifier* recordIdentifier)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::putRecord");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    ASSERT(key.isValid());

    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    int64_t version = -1;
    bool ok = getNewVersionNumber(levelDBTransaction, databaseId, objectStoreId, version);
    if (!ok)
        return false;
    ASSERT(version >= 0);
    const Vector<char> objectStoredataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);

    Vector<char> v;
    v.appendVector(encodeVarInt(version));
    RefPtr<SharedBuffer> value = prpValue;
    ASSERT(value);
    v.append(value->data(), value->size());

    levelDBTransaction->put(objectStoredataKey, v);

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, key);
    levelDBTransaction->put(existsEntryKey, encodeInt(version));

    recordIdentifier->reset(encodeIDBKey(key), version);
    return true;
}

bool IDBBackingStoreLevelDB::clearObjectStore(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::clearObjectStore");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    const Vector<char> startKey = KeyPrefix(databaseId, objectStoreId).encode();
    const Vector<char> stopKey = KeyPrefix(databaseId, objectStoreId + 1).encode();

    deleteRange(levelDBTransaction, startKey, stopKey);
    return true;
}

bool IDBBackingStoreLevelDB::deleteRecord(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, const IDBRecordIdentifier& recordIdentifier)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::deleteRecord");

    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    const Vector<char> objectStoreDataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, recordIdentifier.encodedPrimaryKey());
    levelDBTransaction->remove(objectStoreDataKey);

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, recordIdentifier.encodedPrimaryKey());
    levelDBTransaction->remove(existsEntryKey);
    return true;
}


bool IDBBackingStoreLevelDB::getKeyGeneratorCurrentNumber(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t& keyGeneratorCurrentNumber)
{
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    const Vector<char> keyGeneratorCurrentNumberKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber);

    keyGeneratorCurrentNumber = -1;
    Vector<char> data;

    bool found = false;
    bool ok = levelDBTransaction->safeGet(keyGeneratorCurrentNumberKey, data, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetKeyGeneratorCurrentNumber);
        return false;
    }
    if (found)
        keyGeneratorCurrentNumber = decodeInt(data.begin(), data.end());
    else {
        // Previously, the key generator state was not stored explicitly but derived from the
        // maximum numeric key present in existing data. This violates the spec as the data may
        // be cleared but the key generator state must be preserved.
        const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
        const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

        std::unique_ptr<LevelDBIterator> it = levelDBTransaction->createIterator();
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

bool IDBBackingStoreLevelDB::maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t newNumber, bool checkCurrent)
{
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    if (checkCurrent) {
        int64_t currentNumber;
        bool ok = getKeyGeneratorCurrentNumber(transaction, databaseId, objectStoreId, currentNumber);
        if (!ok)
            return false;
        if (newNumber <= currentNumber)
            return true;
    }

    const Vector<char> keyGeneratorCurrentNumberKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber);
    putInt(levelDBTransaction, keyGeneratorCurrentNumberKey, newNumber);
    return true;
}

bool IDBBackingStoreLevelDB::keyExistsInObjectStore(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, const IDBKey& key, RefPtr<IDBRecordIdentifier>& foundIDBRecordIdentifier)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::keyExistsInObjectStore");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    bool found = false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    bool ok = levelDBTransaction->safeGet(leveldbKey, data, found);
    if (!ok) {
        INTERNAL_READ_ERROR(KeyExistsInObjectStore);
        return false;
    }
    if (!found)
        return true;

    int64_t version;
    if (!decodeVarInt(data.begin(), data.end(), version))
        return false;

    foundIDBRecordIdentifier = IDBRecordIdentifier::create(encodeIDBKey(key), version);
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


// FIXME: This should do some error handling rather than plowing ahead when bad data is encountered.
bool IDBBackingStoreLevelDB::getIndexes(int64_t databaseId, int64_t objectStoreId, IDBObjectStoreMetadata::IndexMap* indexes)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::getIndexes");
    if (!KeyPrefix::validIds(databaseId, objectStoreId))
        return false;
    const Vector<char> startKey = IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0);
    const Vector<char> stopKey = IndexMetaDataKey::encode(databaseId, objectStoreId + 1, 0, 0);

    ASSERT(indexes->isEmpty());

    std::unique_ptr<LevelDBIterator> it = m_db->createIterator();
    it->seek(startKey);
    while (it->isValid() && compareKeys(it->key(), stopKey) < 0) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        IndexMetaDataKey metaDataKey;
        p = IndexMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);
        if (metaDataKey.metaDataType() != IndexMetaDataKey::Name) {
            INTERNAL_CONSISTENCY_ERROR(GetIndexes);
            // Possible stale metadata due to http://webkit.org/b/85557 but don't fail the load.
            it->next();
            continue;
        }

        // FIXME: Do this by direct key lookup rather than iteration, to simplify.
        int64_t indexId = metaDataKey.indexId();
        String indexName = decodeString(it->value().begin(), it->value().end());

        it->next(); // unique flag
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::Unique)) {
            INTERNAL_CONSISTENCY_ERROR(GetIndexes);
            break;
        }
        bool indexUnique = decodeBool(it->value().begin(), it->value().end());

        it->next(); // keyPath
        if (!checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::KeyPath)) {
            INTERNAL_CONSISTENCY_ERROR(GetIndexes);
            break;
        }
        IDBKeyPath keyPath = decodeIDBKeyPath(it->value().begin(), it->value().end());

        it->next(); // [optional] multiEntry flag
        bool indexMultiEntry = false;
        if (checkIndexAndMetaDataKey(it.get(), stopKey, indexId, IndexMetaDataKey::MultiEntry)) {
            indexMultiEntry = decodeBool(it->value().begin(), it->value().end());
            it->next();
        }

        indexes->set(indexId, IDBIndexMetadata(indexName, indexId, keyPath, indexUnique, indexMultiEntry));
    }
    return true;
}

WARN_UNUSED_RETURN static bool setMaxIndexId(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    int64_t maxIndexId = -1;
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, ObjectStoreMetaDataKey::MaxIndexId);
    bool found = false;
    bool ok = getInt(transaction, maxIndexIdKey, maxIndexId, found);
    if (!ok) {
        INTERNAL_READ_ERROR(SetMaxIndexId);
        return false;
    }
    if (!found)
        maxIndexId = MinimumIndexId;

    if (indexId <= maxIndexId) {
        INTERNAL_CONSISTENCY_ERROR(SetMaxIndexId);
        return false;
    }

    putInt(transaction, maxIndexIdKey, indexId);
    return true;
}

bool IDBBackingStoreLevelDB::createIndex(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath& keyPath, bool isUnique, bool isMultiEntry)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::createIndex");
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    if (!setMaxIndexId(levelDBTransaction, databaseId, objectStoreId, indexId))
        return false;

    const Vector<char> nameKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::Name);
    const Vector<char> uniqueKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::Unique);
    const Vector<char> keyPathKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::KeyPath);
    const Vector<char> multiEntryKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, IndexMetaDataKey::MultiEntry);

    putString(levelDBTransaction, nameKey, name);
    putBool(levelDBTransaction, uniqueKey, isUnique);
    putIDBKeyPath(levelDBTransaction, keyPathKey, keyPath);
    putBool(levelDBTransaction, multiEntryKey, isMultiEntry);
    return true;
}

bool IDBBackingStoreLevelDB::deleteIndex(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::deleteIndex");
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);

    const Vector<char> indexMetaDataStart = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 0);
    const Vector<char> indexMetaDataEnd = IndexMetaDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);
    deleteRange(levelDBTransaction, indexMetaDataStart, indexMetaDataEnd);

    const Vector<char> indexDataStart = IndexDataKey::encodeMinKey(databaseId, objectStoreId, indexId);
    const Vector<char> indexDataEnd = IndexDataKey::encodeMaxKey(databaseId, objectStoreId, indexId);
    deleteRange(levelDBTransaction, indexDataStart, indexDataEnd);
    return true;
}

bool IDBBackingStoreLevelDB::putIndexDataForRecord(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, const IDBRecordIdentifier* recordIdentifier)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::putIndexDataForRecord");
    ASSERT(key.isValid());
    ASSERT(recordIdentifier);
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;

    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    const Vector<char> indexDataKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, encodeIDBKey(key), recordIdentifier->encodedPrimaryKey());

    Vector<char> data;
    data.appendVector(encodeVarInt(recordIdentifier->version()));
    data.appendVector(recordIdentifier->encodedPrimaryKey());

    levelDBTransaction->put(indexDataKey, data);
    return true;
}

static bool findGreatestKeyLessThanOrEqual(LevelDBTransaction* transaction, const Vector<char>& target, Vector<char>& foundKey)
{
    std::unique_ptr<LevelDBIterator> it = transaction->createIterator();
    it->seek(target);

    if (!it->isValid()) {
        it->seekToLast();
        if (!it->isValid())
            return false;
    }

    while (IDBBackingStoreLevelDB::compareIndexKeys(it->key(), target) > 0) {
        it->prev();
        if (!it->isValid())
            return false;
    }

    do {
        foundKey.clear();
        foundKey.append(it->key().begin(), it->key().end() - it->key().begin());

        // There can be several index keys that compare equal. We want the last one.
        it->next();
    } while (it->isValid() && !IDBBackingStoreLevelDB::compareIndexKeys(it->key(), target));

    return true;
}

static bool versionExists(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t version, const Vector<char>& encodedPrimaryKey, bool& exists)
{
    const Vector<char> key = ExistsEntryKey::encode(databaseId, objectStoreId, encodedPrimaryKey);
    Vector<char> data;

    bool ok = transaction->safeGet(key, data, exists);
    if (!ok) {
        INTERNAL_READ_ERROR(VersionExists);
        return false;
    }
    if (!exists)
        return true;

    exists = (decodeInt(data.begin(), data.end()) == version);
    return true;
}

bool IDBBackingStoreLevelDB::findKeyInIndex(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, Vector<char>& foundEncodedPrimaryKey, bool& found)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::findKeyInIndex");
    ASSERT(KeyPrefix::validIds(databaseId, objectStoreId, indexId));

    ASSERT(foundEncodedPrimaryKey.isEmpty());
    found = false;

    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    const Vector<char> leveldbKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key);
    std::unique_ptr<LevelDBIterator> it = levelDBTransaction->createIterator();
    it->seek(leveldbKey);

    for (;;) {
        if (!it->isValid())
            return true;
        if (compareIndexKeys(it->key(), leveldbKey) > 0)
            return true;

        int64_t version;
        const char* p = decodeVarInt(it->value().begin(), it->value().end(), version);
        if (!p) {
            INTERNAL_READ_ERROR(FindKeyInIndex);
            return false;
        }
        foundEncodedPrimaryKey.append(p, it->value().end() - p);

        bool exists = false;
        bool ok = versionExists(levelDBTransaction, databaseId, objectStoreId, version, foundEncodedPrimaryKey, exists);
        if (!ok)
            return false;
        if (!exists) {
            // Delete stale index data entry and continue.
            levelDBTransaction->remove(it->key());
            it->next();
            continue;
        }
        found = true;
        return true;
    }
}

bool IDBBackingStoreLevelDB::getPrimaryKeyViaIndex(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, RefPtr<IDBKey>& primaryKey)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::getPrimaryKeyViaIndex");
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;

    bool found = false;
    Vector<char> foundEncodedPrimaryKey;
    bool ok = findKeyInIndex(transaction, databaseId, objectStoreId, indexId, key, foundEncodedPrimaryKey, found);
    if (!ok) {
        INTERNAL_READ_ERROR(GetPrimaryKeyViaIndex);
        return false;
    }
    if (found) {
        decodeIDBKey(foundEncodedPrimaryKey.begin(), foundEncodedPrimaryKey.end(), primaryKey);
        return true;
    }

    return true;
}

bool IDBBackingStoreLevelDB::keyExistsInIndex(IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey, bool& exists)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::keyExistsInIndex");
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;

    exists = false;
    Vector<char> foundEncodedPrimaryKey;
    bool ok = findKeyInIndex(transaction, databaseId, objectStoreId, indexId, indexKey, foundEncodedPrimaryKey, exists);
    if (!ok) {
        INTERNAL_READ_ERROR(KeyExistsInIndex);
        return false;
    }
    if (!exists)
        return true;

    decodeIDBKey(foundEncodedPrimaryKey.begin(), foundEncodedPrimaryKey.end(), foundPrimaryKey);
    return true;
}


bool IDBBackingStoreLevelDB::makeIndexWriters(int64_t transactionID, int64_t databaseID, const IDBObjectStoreMetadata& objectStore, IDBKey& primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, Vector<RefPtr<IDBIndexWriterLevelDB>>& indexWriters, String* errorMessage, bool& completed)
{
    ASSERT(indexIDs.size() == indexKeys.size());
    completed = false;

    HashMap<int64_t, IndexKeys> indexKeyMap;
    for (size_t i = 0; i < indexIDs.size(); ++i)
        indexKeyMap.add(indexIDs[i], indexKeys[i]);

    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = objectStore.indexes.begin(); it != objectStore.indexes.end(); ++it) {
        const IDBIndexMetadata& index = it->value;

        IndexKeys keys = indexKeyMap.get(it->key);
        // If the objectStore is using autoIncrement, then any indexes with an identical keyPath need to also use the primary (generated) key as a key.
        if (keyWasGenerated && (index.keyPath == objectStore.keyPath))
            keys.append(&primaryKey);

        RefPtr<IDBIndexWriterLevelDB> indexWriter = IDBIndexWriterLevelDB::create(index, keys);
        bool canAddKeys = false;
        ASSERT(m_backingStoreTransactions.contains(transactionID));
        bool backingStoreSuccess = indexWriter->verifyIndexKeys(*this, *m_backingStoreTransactions.get(transactionID), databaseID, objectStore.id, index.id, canAddKeys, &primaryKey, errorMessage);
        if (!backingStoreSuccess)
            return false;
        if (!canAddKeys)
            return true;

        indexWriters.append(indexWriter.release());
    }

    completed = true;
    return true;
}

PassRefPtr<IDBKey> IDBBackingStoreLevelDB::generateKey(IDBTransactionBackend& transaction, int64_t databaseId, int64_t objectStoreId)
{
    const int64_t maxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    int64_t currentNumber;
    ASSERT(m_backingStoreTransactions.contains(transaction.id()));
    bool ok = getKeyGeneratorCurrentNumber(*m_backingStoreTransactions.get(transaction.id()), databaseId, objectStoreId, currentNumber);
    if (!ok) {
        LOG_ERROR("Failed to getKeyGeneratorCurrentNumber");
        return IDBKey::createInvalid();
    }
    if (currentNumber < 0 || currentNumber > maxGeneratorValue)
        return IDBKey::createInvalid();

    return IDBKey::createNumber(currentNumber);
}


bool IDBBackingStoreLevelDB::updateKeyGenerator(IDBTransactionBackend& transaction, int64_t databaseId, int64_t objectStoreId, const IDBKey& key, bool checkCurrent)
{
    ASSERT(key.type() == IDBKey::NumberType);
    ASSERT(m_backingStoreTransactions.contains(transaction.id()));

    return maybeUpdateKeyGeneratorCurrentNumber(*m_backingStoreTransactions.get(transaction.id()), databaseId, objectStoreId, static_cast<int64_t>(floor(key.number())) + 1, checkCurrent);
}

class ObjectStoreKeyCursorImpl : public IDBBackingStoreCursorLevelDB {
public:
    static PassRefPtr<ObjectStoreKeyCursorImpl> create(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
    {
        return adoptRef(new ObjectStoreKeyCursorImpl(cursorID, transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> clone()
    {
        return adoptRef(new ObjectStoreKeyCursorImpl(this));
    }

    // IDBBackingStoreCursorLevelDB
    virtual PassRefPtr<SharedBuffer> value() const override { ASSERT_NOT_REACHED(); return 0; }
    virtual bool loadCurrentRow() override;

protected:
    virtual Vector<char> encodeKey(const IDBKey &key)
    {
        return ObjectStoreDataKey::encode(m_cursorOptions.databaseId, m_cursorOptions.objectStoreId, key);
    }

private:
    ObjectStoreKeyCursorImpl(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
        : IDBBackingStoreCursorLevelDB(cursorID, transaction, cursorOptions)
    {
    }

    ObjectStoreKeyCursorImpl(const ObjectStoreKeyCursorImpl* other)
        : IDBBackingStoreCursorLevelDB(other)
    {
    }
};

bool ObjectStoreKeyCursorImpl::loadCurrentRow()
{
    const char* keyPosition = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    ObjectStoreDataKey objectStoreDataKey;
    keyPosition = ObjectStoreDataKey::decode(keyPosition, keyLimit, &objectStoreDataKey);
    if (!keyPosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    m_currentKey = objectStoreDataKey.userKey();

    int64_t version;
    const char* valuePosition = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), version);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    // FIXME: This re-encodes what was just decoded; try and optimize.
    m_recordIdentifier->reset(encodeIDBKey(*m_currentKey), version);

    return true;
}

class ObjectStoreCursorImpl : public IDBBackingStoreCursorLevelDB {
public:
    static PassRefPtr<ObjectStoreCursorImpl> create(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
    {
        return adoptRef(new ObjectStoreCursorImpl(cursorID, transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> clone()
    {
        return adoptRef(new ObjectStoreCursorImpl(this));
    }

    // IDBBackingStoreCursorLevelDB
    virtual PassRefPtr<SharedBuffer> value() const override { return m_currentValue; }
    virtual bool loadCurrentRow() override;

protected:
    virtual Vector<char> encodeKey(const IDBKey &key)
    {
        return ObjectStoreDataKey::encode(m_cursorOptions.databaseId, m_cursorOptions.objectStoreId, key);
    }

private:
    ObjectStoreCursorImpl(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
        : IDBBackingStoreCursorLevelDB(cursorID, transaction, cursorOptions)
    {
    }

    ObjectStoreCursorImpl(const ObjectStoreCursorImpl* other)
        : IDBBackingStoreCursorLevelDB(other)
        , m_currentValue(other->m_currentValue)
    {
    }

    RefPtr<SharedBuffer> m_currentValue;
};

bool ObjectStoreCursorImpl::loadCurrentRow()
{
    const char* keyPosition = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    ObjectStoreDataKey objectStoreDataKey;
    keyPosition = ObjectStoreDataKey::decode(keyPosition, keyLimit, &objectStoreDataKey);
    if (!keyPosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    m_currentKey = objectStoreDataKey.userKey();

    int64_t version;
    const char* valuePosition = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), version);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    // FIXME: This re-encodes what was just decoded; try and optimize.
    m_recordIdentifier->reset(encodeIDBKey(*m_currentKey), version);

    Vector<char> value;
    value.append(valuePosition, m_iterator->value().end() - valuePosition);
    m_currentValue = SharedBuffer::adoptVector(value);
    return true;
}

class IndexKeyCursorImpl final : public IDBBackingStoreCursorLevelDB {
public:
    static PassRefPtr<IndexKeyCursorImpl> create(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
    {
        return adoptRef(new IndexKeyCursorImpl(cursorID, transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> clone()
    {
        return adoptRef(new IndexKeyCursorImpl(this));
    }

    // IDBBackingStoreCursorLevelDB
    virtual PassRefPtr<SharedBuffer> value() const override { ASSERT_NOT_REACHED(); return 0; }
    virtual PassRefPtr<IDBKey> primaryKey() const override { return m_primaryKey; }
    virtual const IDBRecordIdentifier& recordIdentifier() const override { ASSERT_NOT_REACHED(); return *m_recordIdentifier; }
    virtual bool loadCurrentRow() override;

protected:
    virtual Vector<char> encodeKey(const IDBKey &key)
    {
        return IndexDataKey::encode(m_cursorOptions.databaseId, m_cursorOptions.objectStoreId, m_cursorOptions.indexId, key);
    }

private:
    IndexKeyCursorImpl(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
        : IDBBackingStoreCursorLevelDB(cursorID, transaction, cursorOptions)
    {
    }

    IndexKeyCursorImpl(const IndexKeyCursorImpl* other)
        : IDBBackingStoreCursorLevelDB(other)
        , m_primaryKey(other->m_primaryKey)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
};

bool IndexKeyCursorImpl::loadCurrentRow()
{
    const char* keyPosition = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    IndexDataKey indexDataKey;
    keyPosition = IndexDataKey::decode(keyPosition, keyLimit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    int64_t indexDataVersion;
    const char* valuePosition = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), indexDataVersion);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    valuePosition = decodeIDBKey(valuePosition, m_iterator->value().end(), m_primaryKey);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    Vector<char> primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    bool found = false;
    bool ok = m_transaction->safeGet(primaryLevelDBKey, result, found);
    if (!ok) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }
    if (!found) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    int64_t objectStoreDataVersion;
    const char* t = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    if (!t) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    if (objectStoreDataVersion != indexDataVersion) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    return true;
}

class IndexCursorImpl final : public IDBBackingStoreCursorLevelDB {
public:
    static PassRefPtr<IndexCursorImpl> create(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
    {
        return adoptRef(new IndexCursorImpl(cursorID, transaction, cursorOptions));
    }

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> clone()
    {
        return adoptRef(new IndexCursorImpl(this));
    }

    // IDBBackingStoreCursorLevelDB
    virtual PassRefPtr<SharedBuffer> value() const override { return m_currentValue; }
    virtual PassRefPtr<IDBKey> primaryKey() const override { return m_primaryKey; }
    virtual const IDBRecordIdentifier& recordIdentifier() const override { ASSERT_NOT_REACHED(); return *m_recordIdentifier; }
    virtual bool loadCurrentRow() override;

protected:
    virtual Vector<char> encodeKey(const IDBKey &key)
    {
        return IndexDataKey::encode(m_cursorOptions.databaseId, m_cursorOptions.objectStoreId, m_cursorOptions.indexId, key);
    }

private:
    IndexCursorImpl(int64_t cursorID, LevelDBTransaction* transaction, const IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
        : IDBBackingStoreCursorLevelDB(cursorID, transaction, cursorOptions)
    {
    }

    IndexCursorImpl(const IndexCursorImpl* other)
        : IDBBackingStoreCursorLevelDB(other)
        , m_primaryKey(other->m_primaryKey)
        , m_currentValue(other->m_currentValue)
        , m_primaryLevelDBKey(other->m_primaryLevelDBKey)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
    RefPtr<SharedBuffer> m_currentValue;
    Vector<char> m_primaryLevelDBKey;
};

bool IndexCursorImpl::loadCurrentRow()
{
    const char* keyPosition = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    IndexDataKey indexDataKey;
    keyPosition = IndexDataKey::decode(keyPosition, keyLimit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    const char* valuePosition = m_iterator->value().begin();
    const char* valueLimit = m_iterator->value().end();

    int64_t indexDataVersion;
    valuePosition = decodeVarInt(valuePosition, valueLimit, indexDataVersion);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }
    valuePosition = decodeIDBKey(valuePosition, valueLimit, m_primaryKey);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    m_primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    bool found = false;
    bool ok = m_transaction->safeGet(m_primaryLevelDBKey, result, found);
    if (!ok) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }
    if (!found) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    int64_t objectStoreDataVersion;
    valuePosition = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    if (!valuePosition) {
        INTERNAL_READ_ERROR(LoadCurrentRow);
        return false;
    }

    if (objectStoreDataVersion != indexDataVersion) {
        m_transaction->remove(m_iterator->key());
        return false;
    }

    Vector<char> value;
    value.append(valuePosition, result.end() - valuePosition);
    m_currentValue = SharedBuffer::adoptVector(value);
    return true;
}

static bool objectStoreCursorOptions(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange* range, IndexedDB::CursorDirection direction, IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
{
    cursorOptions.databaseId = databaseId;
    cursorOptions.objectStoreId = objectStoreId;

    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    cursorOptions.forward = (direction == IndexedDB::CursorDirection::NextNoDuplicate || direction == IndexedDB::CursorDirection::Next);
    cursorOptions.unique = (direction == IndexedDB::CursorDirection::NextNoDuplicate || direction == IndexedDB::CursorDirection::PrevNoDuplicate);

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
            if (!findGreatestKeyLessThanOrEqual(transaction, cursorOptions.highKey, cursorOptions.highKey))
                return false;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();

        if (!cursorOptions.forward) {
            // For reverse cursors, we need a key that exists.
            Vector<char> foundHighKey;
            if (!findGreatestKeyLessThanOrEqual(transaction, cursorOptions.highKey, foundHighKey))
                return false;

            // If the target key should not be included, but we end up with a smaller key, we should include that.
            if (cursorOptions.highOpen && IDBBackingStoreLevelDB::compareIndexKeys(foundHighKey, cursorOptions.highKey) < 0)
                cursorOptions.highOpen = false;

            cursorOptions.highKey = foundHighKey;
        }
    }

    return true;
}

static bool indexCursorOptions(LevelDBTransaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IndexedDB::CursorDirection direction, IDBBackingStoreCursorLevelDB::CursorOptions& cursorOptions)
{
    ASSERT(transaction);
    if (!KeyPrefix::validIds(databaseId, objectStoreId, indexId))
        return false;

    cursorOptions.databaseId = databaseId;
    cursorOptions.objectStoreId = objectStoreId;
    cursorOptions.indexId = indexId;

    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    cursorOptions.forward = (direction == IndexedDB::CursorDirection::NextNoDuplicate || direction == IndexedDB::CursorDirection::Next);
    cursorOptions.unique = (direction == IndexedDB::CursorDirection::NextNoDuplicate || direction == IndexedDB::CursorDirection::PrevNoDuplicate);

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
            if (!findGreatestKeyLessThanOrEqual(transaction, cursorOptions.highKey, cursorOptions.highKey))
                return false;
            cursorOptions.highOpen = false;
        }
    } else {
        cursorOptions.highKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper());
        cursorOptions.highOpen = range->upperOpen();

        Vector<char> foundHighKey;
        if (!findGreatestKeyLessThanOrEqual(transaction, cursorOptions.highKey, foundHighKey)) // Seek to the *last* key in the set of non-unique keys.
            return false;

        // If the target key should not be included, but we end up with a smaller key, we should include that.
        if (cursorOptions.highOpen && IDBBackingStoreLevelDB::compareIndexKeys(foundHighKey, cursorOptions.highKey) < 0)
            cursorOptions.highOpen = false;

        cursorOptions.highKey = foundHighKey;
    }

    return true;
}
PassRefPtr<IDBBackingStoreCursorLevelDB> IDBBackingStoreLevelDB::openObjectStoreCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseID, int64_t objectStoreId, const IDBKeyRange* range, IndexedDB::CursorDirection direction)

{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::openObjectStoreCursor");
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    IDBBackingStoreCursorLevelDB::CursorOptions cursorOptions;
    if (!objectStoreCursorOptions(levelDBTransaction, databaseID, objectStoreId, range, direction, cursorOptions))
        return 0;
    RefPtr<ObjectStoreCursorImpl> cursor = ObjectStoreCursorImpl::create(cursorID, levelDBTransaction, cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStoreCursorLevelDB> IDBBackingStoreLevelDB::openObjectStoreKeyCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseID, int64_t objectStoreId, const IDBKeyRange* range, IndexedDB::CursorDirection direction)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::openObjectStoreKeyCursor");
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    IDBBackingStoreCursorLevelDB::CursorOptions cursorOptions;
    if (!objectStoreCursorOptions(levelDBTransaction, databaseID, objectStoreId, range, direction, cursorOptions))
        return 0;
    RefPtr<ObjectStoreKeyCursorImpl> cursor = ObjectStoreKeyCursorImpl::create(cursorID, levelDBTransaction, cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStoreCursorLevelDB> IDBBackingStoreLevelDB::openIndexKeyCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseID, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IndexedDB::CursorDirection direction)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::openIndexKeyCursor");
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    IDBBackingStoreCursorLevelDB::CursorOptions cursorOptions;
    if (!indexCursorOptions(levelDBTransaction, databaseID, objectStoreId, indexId, range, direction, cursorOptions))
        return 0;
    RefPtr<IndexKeyCursorImpl> cursor = IndexKeyCursorImpl::create(cursorID, levelDBTransaction, cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStoreCursorLevelDB> IDBBackingStoreLevelDB::openIndexCursor(int64_t cursorID, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseID, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IndexedDB::CursorDirection direction)
{
    LOG(StorageAPI, "IDBBackingStoreLevelDB::openIndexCursor");
    LevelDBTransaction* levelDBTransaction = IDBBackingStoreTransactionLevelDB::levelDBTransactionFrom(transaction);
    IDBBackingStoreCursorLevelDB::CursorOptions cursorOptions;
    if (!indexCursorOptions(levelDBTransaction, databaseID, objectStoreId, indexId, range, direction, cursorOptions))
        return 0;
    RefPtr<IndexCursorImpl> cursor = IndexCursorImpl::create(cursorID, levelDBTransaction, cursorOptions);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

void IDBBackingStoreLevelDB::establishBackingStoreTransaction(int64_t transactionID)
{
    ASSERT(!m_backingStoreTransactions.contains(transactionID));
    m_backingStoreTransactions.set(transactionID, IDBBackingStoreTransactionLevelDB::create(transactionID, this));
}

void IDBBackingStoreLevelDB::removeBackingStoreTransaction(IDBBackingStoreTransactionLevelDB* transaction)
{
    ASSERT(m_backingStoreTransactions.contains(transaction->transactionID()));
    m_backingStoreTransactions.remove(transaction->transactionID());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)
