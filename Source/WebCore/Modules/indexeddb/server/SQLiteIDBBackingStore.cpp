/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SQLiteIDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "FileSystem.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabaseException.h"
#include "IDBKeyData.h"
#include "IDBSerialization.h"
#include "IDBTransactionInfo.h"
#include "IndexKey.h"
#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {
namespace IDBServer {

// Current version of the metadata schema being used in the metadata database.
static const int currentMetadataVersion = 1;

static int idbKeyCollate(int aLength, const void* aBuffer, int bLength, const void* bBuffer)
{
    IDBKeyData a, b;
    if (!deserializeIDBKeyData(static_cast<const uint8_t*>(aBuffer), aLength, a)) {
        LOG_ERROR("Unable to deserialize key A in collation function.");

        // There's no way to indicate an error to SQLite - we have to return a sorting decision.
        // We arbitrarily choose "A > B"
        return 1;
    }
    if (!deserializeIDBKeyData(static_cast<const uint8_t*>(bBuffer), bLength, b)) {
        LOG_ERROR("Unable to deserialize key B in collation function.");

        // There's no way to indicate an error to SQLite - we have to return a sorting decision.
        // We arbitrarily choose "A > B"
        return 1;
    }

    return a.compare(b);
}

static const String v1RecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v1RecordsTableSchema()
{
    static NeverDestroyed<WTF::String> v1RecordsTableSchemaString(v1RecordsTableSchema("Records"));
    return v1RecordsTableSchemaString;
}

static const String& v1RecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> v1RecordsTableSchemaString(v1RecordsTableSchema("\"Records\""));
    return v1RecordsTableSchemaString;
}

static const String v2RecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v2RecordsTableSchema()
{
    static NeverDestroyed<WTF::String> v2RecordsTableSchemaString(v2RecordsTableSchema("Records"));
    return v2RecordsTableSchemaString;
}

static const String& v2RecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> v2RecordsTableSchemaString(v2RecordsTableSchema("\"Records\""));
    return v2RecordsTableSchemaString;
}

SQLiteIDBBackingStore::SQLiteIDBBackingStore(const IDBDatabaseIdentifier& identifier, const String& databaseRootDirectory)
    : m_identifier(identifier)
{
    m_absoluteDatabaseDirectory = identifier.databaseDirectoryRelativeToRoot(databaseRootDirectory);
}

SQLiteIDBBackingStore::~SQLiteIDBBackingStore()
{
    if (m_sqliteDB)
        m_sqliteDB->close();
}

static bool createOrMigrateRecordsTableIfNecessary(SQLiteDatabase& database)
{
    String currentSchema;
    {
        // Fetch the schema for an existing records table.
        SQLiteStatement statement(database, "SELECT type, sql FROM sqlite_master WHERE tbl_name='Records'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the Records table.");
            return false;
        }

        int sqliteResult = statement.step();

        // If there is no Records table at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!database.executeCommand(v2RecordsTableSchema())) {
                LOG_ERROR("Could not create Records table in database (%i) - %s", database.lastError(), database.lastErrorMsg());
                return false;
            }

            return true;
        }

        if (sqliteResult != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the Records table.");
            return false;
        }

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the schema in the backing store is the current schema, we're done.
    if (currentSchema == v2RecordsTableSchema() || currentSchema == v2RecordsTableSchemaAlternate())
        return true;

    // If the record table is not the current schema then it must be one of the previous schemas.
    // If it is not then the database is in an unrecoverable state and this should be considered a fatal error.
    if (currentSchema != v1RecordsTableSchema() && currentSchema != v1RecordsTableSchemaAlternate())
        RELEASE_ASSERT_NOT_REACHED();

    SQLiteTransaction transaction(database);
    transaction.begin();

    // Create a temporary table with the correct schema and migrate all existing content over.
    if (!database.executeCommand(v2RecordsTableSchema("_Temp_Records"))) {
        LOG_ERROR("Could not create temporary records table in database (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("INSERT INTO _Temp_Records SELECT * FROM Records")) {
        LOG_ERROR("Could not migrate existing Records content (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("DROP TABLE Records")) {
        LOG_ERROR("Could not drop existing Records table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("ALTER TABLE _Temp_Records RENAME TO Records")) {
        LOG_ERROR("Could not rename temporary Records table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    transaction.commit();

    return true;
}

bool SQLiteIDBBackingStore::ensureValidRecordsTable()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    if (!createOrMigrateRecordsTableIfNecessary(*m_sqliteDB))
        return false;

    // Whether the updated records table already existed or if it was just created and the data migrated over,
    // make sure the uniqueness index exists.
    if (!m_sqliteDB->executeCommand("CREATE UNIQUE INDEX IF NOT EXISTS RecordsIndex ON Records (objectStoreID, key);")) {
        LOG_ERROR("Could not create RecordsIndex on Records table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    return true;
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::createAndPopulateInitialDatabaseInfo()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    if (!m_sqliteDB->executeCommand("CREATE TABLE IDBDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IDBDatabaseInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE ObjectStoreInfo (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, autoInc INTEGER NOT NULL ON CONFLICT FAIL, maxIndexID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create ObjectStoreInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE IndexInfo (id INTEGER NOT NULL ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, isUnique INTEGER NOT NULL ON CONFLICT FAIL, multiEntry INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IndexInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE IndexRecords (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IndexRecords table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE KeyGenerators (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, currentKey INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create KeyGenerators table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('MetadataVersion', ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt(1, currentMetadataVersion) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database metadata version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            m_sqliteDB = nullptr;
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseName', ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, m_identifier.databaseName()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database name into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            m_sqliteDB = nullptr;
            return nullptr;
        }
    }
    {
        // Database versions are defined to be a uin64_t in the spec but sqlite3 doesn't support native binding of unsigned integers.
        // Therefore we'll store the version as a String.
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseVersion', ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, String::number(0)) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert default version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            m_sqliteDB = nullptr;
            return nullptr;
        }
    }

    if (!m_sqliteDB->executeCommand(ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('MaxObjectStoreID', 1);"))) {
        LOG_ERROR("Could not insert default version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    // This initial database info matches the default values we just put into the metadata database.
    return std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::extractExistingDatabaseInfo()
{
    ASSERT(m_sqliteDB);

    if (!m_sqliteDB->tableExists(ASCIILiteral("IDBDatabaseInfo")))
        return nullptr;

    String databaseName;
    {
        SQLiteStatement sql(*m_sqliteDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseName';");
        if (sql.isColumnNull(0))
            return nullptr;
        databaseName = sql.getColumnText(0);
        if (databaseName != m_identifier.databaseName()) {
            LOG_ERROR("Database name in the info database ('%s') does not match the expected name ('%s')", databaseName.utf8().data(), m_identifier.databaseName().utf8().data());
            return nullptr;
        }
    }
    uint64_t databaseVersion;
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';"));
        if (sql.isColumnNull(0))
            return nullptr;
        String stringVersion = sql.getColumnText(0);
        bool ok;
        databaseVersion = stringVersion.toUInt64Strict(&ok);
        if (!ok) {
            LOG_ERROR("Database version on disk ('%s') does not cleanly convert to an unsigned 64-bit integer version", stringVersion.utf8().data());
            return nullptr;
        }
    }

    auto databaseInfo = std::make_unique<IDBDatabaseInfo>(databaseName, databaseVersion);

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT id, name, keyPath, autoInc, maxIndexID FROM ObjectStoreInfo;"));
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            uint64_t objectStoreID = sql.getColumnInt64(0);
            String objectStoreName = sql.getColumnText(1);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(2, keyPathBuffer);

            IDBKeyPath objectStoreKeyPath;
            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), objectStoreKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }

            bool autoIncrement = sql.getColumnInt(3);

            databaseInfo->addExistingObjectStore({ objectStoreID, objectStoreName, objectStoreKeyPath, autoIncrement });

            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching object store info from database on disk");
            return nullptr;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"));
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            uint64_t indexID = sql.getColumnInt64(0);
            String indexName = sql.getColumnText(1);
            uint64_t objectStoreID = sql.getColumnInt64(2);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(3, keyPathBuffer);

            IDBKeyPath indexKeyPath;
            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), indexKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }

            bool unique = sql.getColumnInt(4);
            bool multiEntry = sql.getColumnInt(5);

            auto objectStore = databaseInfo->infoForExistingObjectStore(objectStoreID);
            if (!objectStore) {
                LOG_ERROR("Found index referring to a non-existant object store");
                return nullptr;
            }

            objectStore->addExistingIndex({ indexID, objectStoreID, indexName, indexKeyPath, unique, multiEntry });

            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching index info from database on disk");
            return nullptr;
        }
    }

    return databaseInfo;
}

String SQLiteIDBBackingStore::filenameForDatabaseName() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    if (m_identifier.databaseName().isEmpty())
        return "%00";

    String filename = encodeForFileName(m_identifier.databaseName());
    filename.replace('.', "%2E");

    return filename;
}

String SQLiteIDBBackingStore::fullDatabaseDirectory() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    return pathByAppendingComponent(m_absoluteDatabaseDirectory, filenameForDatabaseName());
}

String SQLiteIDBBackingStore::fullDatabasePath() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    return pathByAppendingComponent(fullDatabaseDirectory(), "IndexedDB.sqlite3");
}

const IDBDatabaseInfo& SQLiteIDBBackingStore::getOrEstablishDatabaseInfo()
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getOrEstablishDatabaseInfo - database %s", m_identifier.databaseName().utf8().data());

    if (m_databaseInfo)
        return *m_databaseInfo;

    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

    makeAllDirectories(fullDatabaseDirectory());
    String dbFilename = fullDatabasePath();

    m_sqliteDB = std::make_unique<SQLiteDatabase>();
    if (!m_sqliteDB->open(dbFilename)) {
        LOG_ERROR("Failed to open SQLite database at path '%s'", dbFilename.utf8().data());
        m_sqliteDB = nullptr;
    }

    if (!m_sqliteDB)
        return *m_databaseInfo;

    m_sqliteDB->setCollationFunction("IDBKEY", [this](int aLength, const void* a, int bLength, const void* b) {
        return idbKeyCollate(aLength, a, bLength, b);
    });

    if (!ensureValidRecordsTable()) {
        LOG_ERROR("Error creating or migrating Records table in database");
        m_sqliteDB = nullptr;
        return *m_databaseInfo;
    }

    auto databaseInfo = extractExistingDatabaseInfo();
    if (!databaseInfo)
        databaseInfo = createAndPopulateInitialDatabaseInfo();

    if (!databaseInfo)
        LOG_ERROR("Unable to establish IDB database at path '%s'", dbFilename.utf8().data());
    else
        m_databaseInfo = WTFMove(databaseInfo);

    return *m_databaseInfo;
}

IDBError SQLiteIDBBackingStore::beginTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::beginTransaction - %s", info.identifier().loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto addResult = m_transactions.add(info.identifier(), nullptr);
    if (!addResult.isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to establish transaction identifier that already exists") };
    }

    addResult.iterator->value = std::make_unique<SQLiteIDBTransaction>(*this, info);
    return addResult.iterator->value->begin(*m_sqliteDB);
}

IDBError SQLiteIDBBackingStore::abortTransaction(const IDBResourceIdentifier& identifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::abortTransaction - %s", identifier.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto transaction = m_transactions.take(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to abort a transaction that hasn't been established") };
    }

    return transaction->abort();
}

IDBError SQLiteIDBBackingStore::commitTransaction(const IDBResourceIdentifier& identifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::commitTransaction - %s", identifier.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto transaction = m_transactions.take(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to commit a transaction that hasn't been established") };
    }

    return transaction->commit();
}

IDBError SQLiteIDBBackingStore::createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::createObjectStore - adding OS %s with ID %" PRIu64, info.name().utf8().data(), info.identifier());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to create an object store without an in-progress transaction");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to create an object store without an in-progress transaction") };
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to create an object store in a non-version-change transaction");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to create an object store in a non-version-change transaction") };
    }

    RefPtr<SharedBuffer> keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database for new object store");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Unable to serialize IDBKeyPath to save in database for new object store") };
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO ObjectStoreInfo VALUES (?, ?, ?, ?, ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, info.identifier()) != SQLITE_OK
            || sql.bindText(2, info.name()) != SQLITE_OK
            || sql.bindBlob(3, keyPathBlob->data(), keyPathBlob->size()) != SQLITE_OK
            || sql.bindInt(4, info.autoIncrement()) != SQLITE_OK
            || sql.bindInt64(5, info.maxIndexID()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not add object store '%s' to ObjectStoreInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not create object store") };
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO KeyGenerators VALUES (?, 0);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, info.identifier()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not seed initial key generator value for ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not seed initial key generator value for object store") };
        }
    }

    return { };
}

IDBError SQLiteIDBBackingStore::deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteObjectStore - object store %" PRIu64, objectStoreIdentifier);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to delete an object store without an in-progress transaction");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to delete an object store without an in-progress transaction") };
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to delete an object store in a non-version-change transaction");
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Attempt to delete an object store in a non-version-change transaction") };
    }

    // Delete the ObjectStore record
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM ObjectStoreInfo WHERE id = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store id %" PRIi64 " from ObjectStoreInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not delete object store") };
        }
    }

    // Delete the ObjectStore's key generator record if there is one.
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM KeyGenerators WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not delete key generator for deleted object store") };
        }
    }

    // Delete all associated records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM Records WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records for object store %" PRIi64 " (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not delete records for deleted object store") };
        }
    }

    // Delete all associated Indexes
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexInfo WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not delete IDBIndex for deleted object store") };
        }
    }

    // Delete all associated Index records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not delete IDBIndex records for deleted object store") };
        }
    }

    return true;
}

IDBError SQLiteIDBBackingStore::clearObjectStore(const IDBResourceIdentifier&, uint64_t)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::createIndex(const IDBResourceIdentifier&, const IDBIndexInfo&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::deleteIndex(const IDBResourceIdentifier&, uint64_t, const String&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::keyExistsInObjectStore(const IDBResourceIdentifier&, uint64_t, const IDBKeyData&, bool&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::deleteRange(const IDBResourceIdentifier&, uint64_t, const IDBKeyRangeData&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::addRecord(const IDBResourceIdentifier&, uint64_t, const IDBKeyData&, const ThreadSafeDataBuffer&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getRecord(const IDBResourceIdentifier&, uint64_t, const IDBKeyRangeData&, ThreadSafeDataBuffer&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getIndexRecord(const IDBResourceIdentifier&, uint64_t, uint64_t, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getCount(const IDBResourceIdentifier&, uint64_t, uint64_t, const IDBKeyRangeData&, uint64_t&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::generateKeyNumber(const IDBResourceIdentifier&, uint64_t, uint64_t&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::revertGeneratedKeyNumber(const IDBResourceIdentifier&, uint64_t, uint64_t)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier&, uint64_t, double)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::openCursor(const IDBResourceIdentifier&, const IDBCursorInfo&, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::iterateCursor(const IDBResourceIdentifier&, const IDBResourceIdentifier&, const IDBKeyData&, uint32_t, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

void SQLiteIDBBackingStore::deleteBackingStore()
{
    String dbFilename = fullDatabasePath();

    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteBackingStore deleting file '%s' on disk", dbFilename.utf8().data());

    if (m_sqliteDB) {
        m_sqliteDB->close();
        m_sqliteDB = nullptr;
    }

    SQLiteFileSystem::deleteDatabaseFile(dbFilename);
    SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_absoluteDatabaseDirectory);
}

void SQLiteIDBBackingStore::unregisterCursor(SQLiteIDBCursor& cursor)
{
    ASSERT(m_cursors.contains(cursor.identifier()));
    m_cursors.remove(cursor.identifier());
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
