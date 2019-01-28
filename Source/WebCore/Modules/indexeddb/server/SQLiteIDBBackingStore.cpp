/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "IDBBindingUtilities.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetAllResult.h"
#include "IDBGetRecordData.h"
#include "IDBGetResult.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyData.h"
#include "IDBObjectStoreInfo.h"
#include "IDBSerialization.h"
#include "IDBTransactionInfo.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "ThreadSafeDataBuffer.h"
#include <JavaScriptCore/AuxiliaryBarrierInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {
using namespace JSC;
namespace IDBServer {

// Current version of the metadata schema being used in the metadata database.
static const int currentMetadataVersion = 1;

// The IndexedDatabase spec defines the max key generator value as 2^53.
static const uint64_t maxGeneratorValue = 0x20000000000000;

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

static const String v3RecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL, recordID INTEGER PRIMARY KEY)");
}

static const String& v3RecordsTableSchema()
{
    static NeverDestroyed<WTF::String> v3RecordsTableSchemaString(v3RecordsTableSchema("Records"));
    return v3RecordsTableSchemaString;
}

static const String& v3RecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> v3RecordsTableSchemaString(v3RecordsTableSchema("\"Records\""));
    return v3RecordsTableSchemaString;
}

static const String v1IndexRecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v1IndexRecordsTableSchema()
{
    static NeverDestroyed<WTF::String> v1IndexRecordsTableSchemaString(v1IndexRecordsTableSchema("IndexRecords"));
    return v1IndexRecordsTableSchemaString;
}

static const String& v1IndexRecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> v1IndexRecordsTableSchemaString(v1IndexRecordsTableSchema("\"IndexRecords\""));
    return v1IndexRecordsTableSchemaString;
}

static const String v2IndexRecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL)");
}

static const String& v2IndexRecordsTableSchema()
{
    static NeverDestroyed<WTF::String> v2IndexRecordsTableSchemaString(v2IndexRecordsTableSchema("IndexRecords"));
    return v2IndexRecordsTableSchemaString;
}

static const String& v2IndexRecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> v2IndexRecordsTableSchemaString(v2IndexRecordsTableSchema("\"IndexRecords\""));
    return v2IndexRecordsTableSchemaString;
}

static const String v3IndexRecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, objectStoreRecordID INTEGER NOT NULL ON CONFLICT FAIL)");
}

static const String v3IndexRecordsTableSchema()
{
    static NeverDestroyed<WTF::String> indexRecordsTableSchemaString = v3IndexRecordsTableSchema("IndexRecords");
    return indexRecordsTableSchemaString;
}

static const String v3IndexRecordsTableSchemaAlternate()
{
    static NeverDestroyed<WTF::String> indexRecordsTableSchemaString = v3IndexRecordsTableSchema("\"IndexRecords\"");
    return indexRecordsTableSchemaString;
}

static const String& v1IndexRecordsIndexSchema()
{
    static NeverDestroyed<WTF::String> indexRecordsIndexSchemaString("CREATE INDEX IndexRecordsIndex ON IndexRecords (key)");
    return indexRecordsIndexSchemaString;
}

static const String blobRecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreRow INTEGER NOT NULL ON CONFLICT FAIL, blobURL TEXT NOT NULL ON CONFLICT FAIL)");
}

static const String& blobRecordsTableSchema()
{
    static NeverDestroyed<String> blobRecordsTableSchemaString(blobRecordsTableSchema("BlobRecords"));
    return blobRecordsTableSchemaString;
}

static const String& blobRecordsTableSchemaAlternate()
{
    static NeverDestroyed<String> blobRecordsTableSchemaString(blobRecordsTableSchema("\"BlobRecords\""));
    return blobRecordsTableSchemaString;
}

static const String blobFilesTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (blobURL TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, fileName TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL)");
}

static const String& blobFilesTableSchema()
{
    static NeverDestroyed<String> blobFilesTableSchemaString(blobFilesTableSchema("BlobFiles"));
    return blobFilesTableSchemaString;
}

static const String& blobFilesTableSchemaAlternate()
{
    static NeverDestroyed<String> blobFilesTableSchemaString(blobFilesTableSchema("\"BlobFiles\""));
    return blobFilesTableSchemaString;
}

SQLiteIDBBackingStore::SQLiteIDBBackingStore(const IDBDatabaseIdentifier& identifier, const String& databaseRootDirectory, IDBBackingStoreTemporaryFileHandler& fileHandler, uint64_t quota)
    : m_identifier(identifier)
    , m_temporaryFileHandler(fileHandler)
    , m_quota(quota)
{
    m_absoluteDatabaseDirectory = identifier.databaseDirectoryRelativeToRoot(databaseRootDirectory);
}

SQLiteIDBBackingStore::~SQLiteIDBBackingStore()
{
    if (m_sqliteDB)
        closeSQLiteDB();

    if (m_vm) {
        JSLockHolder locker(m_vm.get());
        m_globalObject.clear();
        m_vm = nullptr;
    }
}


void SQLiteIDBBackingStore::initializeVM()
{
    if (!m_vm) {
        ASSERT(!m_globalObject);
        m_vm = VM::create();

        JSLockHolder locker(m_vm.get());
        m_globalObject.set(*m_vm, JSGlobalObject::create(*m_vm, JSGlobalObject::createStructure(*m_vm, jsNull())));
    }
}

VM& SQLiteIDBBackingStore::vm()
{
    initializeVM();
    return *m_vm;
}

JSGlobalObject& SQLiteIDBBackingStore::globalObject()
{
    initializeVM();
    return **m_globalObject;
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
            if (!database.executeCommand(v3RecordsTableSchema())) {
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
    if (currentSchema == v3RecordsTableSchema() || currentSchema == v3RecordsTableSchemaAlternate())
        return true;

    // If the record table is not the current schema then it must be one of the previous schemas.
    // If it is not then the database is in an unrecoverable state and this should be considered a fatal error.
    if (currentSchema != v1RecordsTableSchema() && currentSchema != v1RecordsTableSchemaAlternate()
        && currentSchema != v2RecordsTableSchema() && currentSchema != v2RecordsTableSchemaAlternate())
        RELEASE_ASSERT_NOT_REACHED();

    SQLiteTransaction transaction(database);
    transaction.begin();

    // Create a temporary table with the correct schema and migrate all existing content over.
    if (!database.executeCommand(v3RecordsTableSchema("_Temp_Records"))) {
        LOG_ERROR("Could not create temporary records table in database (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("INSERT INTO _Temp_Records (objectStoreID, key, value) SELECT objectStoreID, CAST(key AS TEXT), value FROM Records")) {
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

bool SQLiteIDBBackingStore::ensureValidBlobTables()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String currentSchema;
    {
        // Fetch the schema for an existing blob record table.
        SQLiteStatement statement(*m_sqliteDB, "SELECT type, sql FROM sqlite_master WHERE tbl_name='BlobRecords'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the BlobRecords table.");
            return false;
        }

        int sqliteResult = statement.step();

        // If there is no BlobRecords table at all, create it..
        if (sqliteResult == SQLITE_DONE) {
            if (!m_sqliteDB->executeCommand(blobRecordsTableSchema())) {
                LOG_ERROR("Could not create BlobRecords table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
                return false;
            }

            currentSchema = blobRecordsTableSchema();
        } else if (sqliteResult != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the BlobRecords table.");
            return false;
        } else
            currentSchema = statement.getColumnText(1);
    }

    if (currentSchema != blobRecordsTableSchema() && currentSchema != blobRecordsTableSchemaAlternate()) {
        LOG_ERROR("Invalid BlobRecords table schema found");
        return false;
    }

    {
        // Fetch the schema for an existing blob file table.
        SQLiteStatement statement(*m_sqliteDB, "SELECT type, sql FROM sqlite_master WHERE tbl_name='BlobFiles'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the BlobFiles table.");
            return false;
        }

        int sqliteResult = statement.step();

        // If there is no BlobFiles table at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!m_sqliteDB->executeCommand(blobFilesTableSchema())) {
                LOG_ERROR("Could not create BlobFiles table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
                return false;
            }

            return true;
        }

        if (sqliteResult != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the BlobFiles table.");
            return false;
        }

        currentSchema = statement.getColumnText(1);
    }

    if (currentSchema != blobFilesTableSchema() && currentSchema != blobFilesTableSchemaAlternate()) {
        LOG_ERROR("Invalid BlobFiles table schema found");
        return false;
    }

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

bool SQLiteIDBBackingStore::ensureValidIndexRecordsTable()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String currentSchema;
    {
        // Fetch the schema for an existing index record table.
        SQLiteStatement statement(*m_sqliteDB, "SELECT type, sql FROM sqlite_master WHERE tbl_name='IndexRecords'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the IndexRecords table.");
            return false;
        }

        int sqliteResult = statement.step();

        // If there is no IndexRecords table at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!m_sqliteDB->executeCommand(v3IndexRecordsTableSchema())) {
                LOG_ERROR("Could not create IndexRecords table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
                return false;
            }

            return true;
        }

        if (sqliteResult != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the IndexRecords table.");
            return false;
        }

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the schema in the backing store is the current schema, we're done.
    if (currentSchema == v3IndexRecordsTableSchema() || currentSchema == v3IndexRecordsTableSchemaAlternate())
        return true;

    // If the record table is not the current schema then it must be one of the previous schemas.
    // If it is not then the database is in an unrecoverable state and this should be considered a fatal error.
    if (currentSchema != v1IndexRecordsTableSchema() && currentSchema != v1IndexRecordsTableSchemaAlternate()
        && currentSchema != v2IndexRecordsTableSchema() && currentSchema != v2IndexRecordsTableSchemaAlternate())
        RELEASE_ASSERT_NOT_REACHED();

    SQLiteTransaction transaction(*m_sqliteDB);
    transaction.begin();

    // Create a temporary table with the correct schema and migrate all existing content over.
    if (!m_sqliteDB->executeCommand(v3IndexRecordsTableSchema("_Temp_IndexRecords"))) {
        LOG_ERROR("Could not create temporary index records table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    if (!m_sqliteDB->executeCommand("INSERT INTO _Temp_IndexRecords SELECT IndexRecords.indexID, IndexRecords.objectStoreID, IndexRecords.key, IndexRecords.value, Records.rowid FROM IndexRecords INNER JOIN Records ON Records.key = IndexRecords.value AND Records.objectStoreID = IndexRecords.objectStoreID")) {
        LOG_ERROR("Could not migrate existing IndexRecords content (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    if (!m_sqliteDB->executeCommand("DROP TABLE IndexRecords")) {
        LOG_ERROR("Could not drop existing IndexRecords table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    if (!m_sqliteDB->executeCommand("ALTER TABLE _Temp_IndexRecords RENAME TO IndexRecords")) {
        LOG_ERROR("Could not rename temporary IndexRecords table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    transaction.commit();

    return true;
}

bool SQLiteIDBBackingStore::ensureValidIndexRecordsIndex()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String currentSchema;
    {
        // Fetch the schema for an existing index record index.
        SQLiteStatement statement(*m_sqliteDB, "SELECT sql FROM sqlite_master WHERE name='IndexRecordsIndex'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the IndexRecordsIndex index.");
            return false;
        }

        int sqliteResult = statement.step();

        // If there is no IndexRecordsIndex index at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!m_sqliteDB->executeCommand(v1IndexRecordsIndexSchema())) {
                LOG_ERROR("Could not create IndexRecordsIndex index in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
                return false;
            }

            return true;
        }

        if (sqliteResult != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the IndexRecordsIndex index.");
            return false;
        }

        currentSchema = statement.getColumnText(0);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the schema in the backing store is the current schema, we're done.
    if (currentSchema == v1IndexRecordsIndexSchema())
        return true;

    // There is currently no outdated schema for the IndexRecordsIndex, so any other existing schema means this database is invalid.
    return false;
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::createAndPopulateInitialDatabaseInfo()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    if (!m_sqliteDB->executeCommand("CREATE TABLE IDBDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IDBDatabaseInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE ObjectStoreInfo (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, autoInc INTEGER NOT NULL ON CONFLICT FAIL, maxIndexID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create ObjectStoreInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE IndexInfo (id INTEGER NOT NULL ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, isUnique INTEGER NOT NULL ON CONFLICT FAIL, multiEntry INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IndexInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE KeyGenerators (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, currentKey INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create KeyGenerators table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, "INSERT INTO IDBDatabaseInfo VALUES ('MetadataVersion', ?);"_s);
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt(1, currentMetadataVersion) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database metadata version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            closeSQLiteDB();
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_sqliteDB, "INSERT INTO IDBDatabaseInfo VALUES ('DatabaseName', ?);"_s);
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, m_identifier.databaseName()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database name into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            closeSQLiteDB();
            return nullptr;
        }
    }
    {
        // Database versions are defined to be a uin64_t in the spec but sqlite3 doesn't support native binding of unsigned integers.
        // Therefore we'll store the version as a String.
        SQLiteStatement sql(*m_sqliteDB, "INSERT INTO IDBDatabaseInfo VALUES ('DatabaseVersion', ?);"_s);
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, String::number(0)) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert default version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            closeSQLiteDB();
            return nullptr;
        }
    }

    if (!m_sqliteDB->executeCommand("INSERT INTO IDBDatabaseInfo VALUES ('MaxObjectStoreID', 1);"_s)) {
        LOG_ERROR("Could not insert default version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    // This initial database info matches the default values we just put into the metadata database.
    return std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::extractExistingDatabaseInfo()
{
    ASSERT(m_sqliteDB);

    if (!m_sqliteDB->tableExists("IDBDatabaseInfo"_s))
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
        SQLiteStatement sql(*m_sqliteDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';"_s);
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
        SQLiteStatement sql(*m_sqliteDB, "SELECT id, name, keyPath, autoInc, maxIndexID FROM ObjectStoreInfo;"_s);
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            uint64_t objectStoreID = sql.getColumnInt64(0);
            String objectStoreName = sql.getColumnText(1);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(2, keyPathBuffer);

            Optional<IDBKeyPath> objectStoreKeyPath;
            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), objectStoreKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }

            bool autoIncrement = sql.getColumnInt(3);

            databaseInfo->addExistingObjectStore({ objectStoreID, objectStoreName, WTFMove(objectStoreKeyPath), autoIncrement });

            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching object store info from database on disk");
            return nullptr;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, "SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"_s);
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            uint64_t indexID = sql.getColumnInt64(0);
            String indexName = sql.getColumnText(1);
            uint64_t objectStoreID = sql.getColumnInt64(2);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(3, keyPathBuffer);

            Optional<IDBKeyPath> indexKeyPath;
            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), indexKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }
            if (!indexKeyPath) {
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

            objectStore->addExistingIndex({ indexID, objectStoreID, indexName, WTFMove(indexKeyPath.value()), unique, multiEntry });

            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching index info from database on disk");
            return nullptr;
        }
    }

    return databaseInfo;
}

String SQLiteIDBBackingStore::databaseNameFromEncodedFilename(const String& encodedName)
{
    if (encodedName == "%00"_s)
        return { };

    String partiallyDecoded = encodedName;
    partiallyDecoded.replace("%2E"_s, "."_s);

    return FileSystem::decodeFromFilename(partiallyDecoded);
}

String SQLiteIDBBackingStore::filenameForDatabaseName() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    if (m_identifier.databaseName().isEmpty())
        return "%00";

    String filename = FileSystem::encodeForFileName(m_identifier.databaseName());
    filename.replace('.', "%2E");

    return filename;
}

String SQLiteIDBBackingStore::fullDatabaseDirectory() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    return FileSystem::pathByAppendingComponent(m_absoluteDatabaseDirectory, filenameForDatabaseName());
}

String SQLiteIDBBackingStore::fullDatabasePath() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    return FileSystem::pathByAppendingComponent(fullDatabaseDirectory(), "IndexedDB.sqlite3");
}

IDBError SQLiteIDBBackingStore::getOrEstablishDatabaseInfo(IDBDatabaseInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getOrEstablishDatabaseInfo - database %s", m_identifier.databaseName().utf8().data());

    if (m_databaseInfo) {
        info = *m_databaseInfo;
        return IDBError { };
    }

    FileSystem::makeAllDirectories(fullDatabaseDirectory());
    String dbFilename = fullDatabasePath();

    m_sqliteDB = std::make_unique<SQLiteDatabase>();
    if (!m_sqliteDB->open(dbFilename)) {
        LOG_ERROR("Failed to open SQLite database at path '%s'", dbFilename.utf8().data());
        closeSQLiteDB();
    }

    if (!m_sqliteDB)
        return IDBError { UnknownError, "Unable to open database file on disk"_s };

    m_sqliteDB->setCollationFunction("IDBKEY", [](int aLength, const void* a, int bLength, const void* b) {
        return idbKeyCollate(aLength, a, bLength, b);
    });

    if (!ensureValidRecordsTable()) {
        LOG_ERROR("Error creating or migrating Records table in database");
        closeSQLiteDB();
        return IDBError { UnknownError, "Error creating or migrating Records table in database"_s };
    }

    if (!ensureValidIndexRecordsTable()) {
        LOG_ERROR("Error creating or migrating Index Records table in database");
        closeSQLiteDB();
        return IDBError { UnknownError, "Error creating or migrating Index Records table in database"_s };
    }

    if (!ensureValidIndexRecordsIndex()) {
        LOG_ERROR("Error creating or migrating Index Records index in database");
        closeSQLiteDB();
        return IDBError { UnknownError, "Error creating or migrating Index Records index in database"_s };
    }

    if (!ensureValidBlobTables()) {
        LOG_ERROR("Error creating or confirming Blob Records tables in database");
        closeSQLiteDB();
        return IDBError { UnknownError, "Error creating or confirming Blob Records tables in database"_s };
    }

    auto databaseInfo = extractExistingDatabaseInfo();
    if (!databaseInfo)
        databaseInfo = createAndPopulateInitialDatabaseInfo();

    if (!databaseInfo) {
        LOG_ERROR("Unable to establish IDB database at path '%s'", dbFilename.utf8().data());
        closeSQLiteDB();
        return IDBError { UnknownError, "Unable to establish IDB database file"_s };
    }

    m_databaseInfo = WTFMove(databaseInfo);
    info = *m_databaseInfo;
    return IDBError { };
}

uint64_t SQLiteIDBBackingStore::quotaForOrigin() const
{
    ASSERT(!isMainThread());
    uint64_t diskFreeSpaceSize = 0;
    FileSystem::getVolumeFreeSpace(m_absoluteDatabaseDirectory, diskFreeSpaceSize);
    return std::min(diskFreeSpaceSize / 2, m_quota);
}

uint64_t SQLiteIDBBackingStore::maximumSize() const
{
    ASSERT(!isMainThread());

    // The maximum size for one database file is the quota for its origin, minus size of all databases within that origin,
    // and plus current size of the database file.
    uint64_t databaseFileSize = SQLiteFileSystem::getDatabaseFileSize(fullDatabasePath());
    uint64_t quota = quotaForOrigin();

    uint64_t diskUsage = 0;
    for (auto& directory : FileSystem::listDirectory(m_absoluteDatabaseDirectory, "*")) {
        for (auto& file : FileSystem::listDirectory(directory, "*.sqlite3"_s))
            diskUsage += SQLiteFileSystem::getDatabaseFileSize(file);
    }
    ASSERT(diskUsage >= databaseFileSize);

    if (quota < diskUsage)
        return databaseFileSize;

    return quota - diskUsage + databaseFileSize;
}

IDBError SQLiteIDBBackingStore::beginTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::beginTransaction - %s", info.identifier().loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());
    ASSERT(m_databaseInfo);

    m_sqliteDB->setMaximumSize(maximumSize());
    auto addResult = m_transactions.add(info.identifier(), nullptr);
    if (!addResult.isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return IDBError { UnknownError, "Attempt to establish transaction identifier that already exists"_s };
    }

    addResult.iterator->value = std::make_unique<SQLiteIDBTransaction>(*this, info);

    auto error = addResult.iterator->value->begin(*m_sqliteDB);
    if (error.isNull() && info.mode() == IDBTransactionMode::Versionchange) {
        m_originalDatabaseInfoBeforeVersionChange = std::make_unique<IDBDatabaseInfo>(*m_databaseInfo);

        SQLiteStatement sql(*m_sqliteDB, "UPDATE IDBDatabaseInfo SET value = ? where key = 'DatabaseVersion';"_s);
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, String::number(info.newVersion())) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                error = IDBError { QuotaExceededError, "Failed to store new database version in database because no enough space for domain"_s };
            else
                error = IDBError { UnknownError, "Failed to store new database version in database"_s };
        }
    }

    return error;
}

IDBError SQLiteIDBBackingStore::abortTransaction(const IDBResourceIdentifier& identifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::abortTransaction - %s", identifier.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto transaction = m_transactions.take(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return IDBError { UnknownError, "Attempt to abort a transaction that hasn't been established"_s };
    }

    if (transaction->mode() == IDBTransactionMode::Versionchange && m_originalDatabaseInfoBeforeVersionChange)
        m_databaseInfo = WTFMove(m_originalDatabaseInfoBeforeVersionChange);

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
        return IDBError { UnknownError, "Attempt to commit a transaction that hasn't been established"_s };
    }

    auto error = transaction->commit();
    if (!error.isNull()) {
        if (transaction->mode() == IDBTransactionMode::Versionchange) {
            ASSERT(m_originalDatabaseInfoBeforeVersionChange);
            m_databaseInfo = WTFMove(m_originalDatabaseInfoBeforeVersionChange);
        }
    } else
        m_originalDatabaseInfoBeforeVersionChange = nullptr;

    return error;
}

IDBError SQLiteIDBBackingStore::createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::createObjectStore - adding OS %s with ID %" PRIu64, info.name().utf8().data(), info.identifier());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to create an object store without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to create an object store without an in-progress transaction"_s };
    }
    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to create an object store in a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to create an object store in a non-version-change transaction"_s };
    }

    RefPtr<SharedBuffer> keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database for new object store");
        return IDBError { UnknownError, "Unable to serialize IDBKeyPath to save in database for new object store"_s };
    }

    {
        auto* sql = cachedStatement(SQL::CreateObjectStoreInfo, "INSERT INTO ObjectStoreInfo VALUES (?, ?, ?, ?, ?);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindText(2, info.name()) != SQLITE_OK
            || sql->bindBlob(3, keyPathBlob->data(), keyPathBlob->size()) != SQLITE_OK
            || sql->bindInt(4, info.autoIncrement()) != SQLITE_OK
            || sql->bindInt64(5, info.maxIndexID()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not add object store '%s' to ObjectStoreInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Could not create object store because no enough space for domain"_s };
            return IDBError { UnknownError, "Could not create object store"_s };
        }
    }

    {
        auto* sql = cachedStatement(SQL::CreateObjectStoreKeyGenerator, "INSERT INTO KeyGenerators VALUES (?, 0);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not seed initial key generator value for ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Could not seed initial key generator value for object store because no enough space for domain"_s };
            return IDBError { UnknownError, "Could not seed initial key generator value for object store"_s };
        }
    }

    m_databaseInfo->addExistingObjectStore(info);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteObjectStore - object store %" PRIu64, objectStoreIdentifier);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to delete an object store without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to delete an object store without an in-progress transaction"_s };
    }
    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to delete an object store in a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to delete an object store in a non-version-change transaction"_s };
    }

    // Delete the ObjectStore record
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreInfo, "DELETE FROM ObjectStoreInfo WHERE id = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store id %" PRIi64 " from ObjectStoreInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete object store"_s };
        }
    }

    // Delete the ObjectStore's key generator record if there is one.
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreKeyGenerator, "DELETE FROM KeyGenerators WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete key generator for deleted object store"_s };
        }
    }

    // Delete all associated records
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreRecords, "DELETE FROM Records WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records for object store %" PRIi64 " (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete records for deleted object store"_s };
        }
    }

    // Delete all associated Indexes
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreIndexInfo, "DELETE FROM IndexInfo WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete IDBIndex for deleted object store"_s };
        }
    }

    // Delete all associated Index records
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreIndexRecords, "DELETE FROM IndexRecords WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete IDBIndex records for deleted object store"_s };
        }
    }

    // Delete all unused Blob URL records.
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreBlobRecords, "DELETE FROM BlobRecords WHERE objectStoreRow NOT IN (SELECT recordID FROM Records)"_s);
        if (!sql
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete Blob URL records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Could not delete stored blob records for deleted object store"_s };
        }
    }

    // Delete all unused Blob File records.
    auto error = deleteUnusedBlobFileRecords(*transaction);
    if (!error.isNull())
        return error;

    m_databaseInfo->deleteObjectStore(objectStoreIdentifier);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::renameObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::renameObjectStore - object store %" PRIu64, objectStoreIdentifier);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to rename an object store without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to rename an object store without an in-progress transaction"_s };
    }
    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to rename an object store in a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to rename an object store in a non-version-change transaction"_s };
    }

    {
        auto* sql = cachedStatement(SQL::RenameObjectStore, "UPDATE ObjectStoreInfo SET name = ? WHERE id = ?;"_s);
        if (!sql
            || sql->bindText(1, newName) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not update name for object store id %" PRIi64 " in ObjectStoreInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Could not rename object store because no enough space for domain"_s };
            return IDBError { UnknownError, "Could not rename object store"_s };
        }
    }

    m_databaseInfo->renameObjectStore(objectStoreIdentifier, newName);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::clearObjectStore - object store %" PRIu64, objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to clear an object store without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to clear an object store without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to clear an object store in a read-only transaction");
        return IDBError { UnknownError, "Attempt to clear an object store in a read-only transaction"_s };
    }

    {
        auto* sql = cachedStatement(SQL::ClearObjectStoreRecords, "DELETE FROM Records WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not clear records from object store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Unable to clear object store"_s };
        }
    }

    {
        auto* sql = cachedStatement(SQL::ClearObjectStoreIndexRecords, "DELETE FROM IndexRecords WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records from index record store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Unable to delete index records while clearing object store"_s };
        }
    }

    transaction->notifyCursorsOfChanges(objectStoreID);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::createIndex - ObjectStore %" PRIu64 ", Index %" PRIu64, info.objectStoreIdentifier(), info.identifier());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to create an index without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to create an index without an in-progress transaction"_s };
    }
    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to create an index in a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to create an index in a non-version-change transaction"_s };
    }

    RefPtr<SharedBuffer> keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database");
        return IDBError { UnknownError, "Unable to serialize IDBKeyPath to create index in database"_s };
    }

    auto* sql = cachedStatement(SQL::CreateIndexInfo, "INSERT INTO IndexInfo VALUES (?, ?, ?, ?, ?, ?);"_s);
    if (!sql
        || sql->bindInt64(1, info.identifier()) != SQLITE_OK
        || sql->bindText(2, info.name()) != SQLITE_OK
        || sql->bindInt64(3, info.objectStoreIdentifier()) != SQLITE_OK
        || sql->bindBlob(4, keyPathBlob->data(), keyPathBlob->size()) != SQLITE_OK
        || sql->bindInt(5, info.unique()) != SQLITE_OK
        || sql->bindInt(6, info.multiEntry()) != SQLITE_OK
        || sql->step() != SQLITE_DONE) {
        LOG_ERROR("Could not add index '%s' to IndexInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        if (m_sqliteDB->lastError() == SQLITE_FULL)
            return IDBError { QuotaExceededError, "Unable to create index in database because no enough space for domain"_s };
        return IDBError { UnknownError, "Unable to create index in database"_s };
    }

    // Write index records for any records that already exist in this object store.

    auto cursor = transaction->maybeOpenBackingStoreCursor(info.objectStoreIdentifier(), 0, IDBKeyRangeData::allKeys());

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to populate indexes in database");
        return IDBError { UnknownError, "Unable to populate indexes in database"_s };
    }

    while (!cursor->currentKey().isNull()) {
        auto& key = cursor->currentKey();
        auto* value = cursor->currentValue();
        ThreadSafeDataBuffer valueBuffer = value ? value->data() : ThreadSafeDataBuffer();

        ASSERT(cursor->currentRecordRowID());

        IDBError error = updateOneIndexForAddRecord(info, key, valueBuffer, cursor->currentRecordRowID());
        if (!error.isNull()) {
            auto* sql = cachedStatement(SQL::DeleteIndexInfo, "DELETE FROM IndexInfo WHERE id = ? AND objectStoreID = ?;"_s);
            if (!sql
                || sql->bindInt64(1, info.identifier()) != SQLITE_OK
                || sql->bindInt64(2, info.objectStoreIdentifier()) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Index creation failed due to uniqueness constraint failure, but there was an error deleting the Index record from the database");
                return IDBError { UnknownError, "Index creation failed due to uniqueness constraint failure, but there was an error deleting the Index record from the database"_s };
            }

            return error;
        }

        if (!cursor->advance(1)) {
            LOG_ERROR("Error advancing cursor while indexing existing records for new index.");
            return IDBError { UnknownError, "Error advancing cursor while indexing existing records for new index"_s };
        }
    }

    auto* objectStore = m_databaseInfo->infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->addExistingIndex(info);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedHasIndexRecord(const IDBIndexInfo& info, const IDBKeyData& indexKey, bool& hasRecord)
{
    hasRecord = false;

    RefPtr<SharedBuffer> indexKeyBuffer = serializeIDBKeyData(indexKey);
    if (!indexKeyBuffer) {
        LOG_ERROR("Unable to serialize index key to be stored in the database");
        return IDBError { UnknownError, "Unable to serialize IDBKey to check for index record in database"_s };
    }

    auto* sql = cachedStatement(SQL::HasIndexRecord, "SELECT rowid FROM IndexRecords WHERE indexID = ? AND objectStoreID = ? AND key = CAST(? AS TEXT);"_s);
    if (!sql
        || sql->bindInt64(1, info.identifier()) != SQLITE_OK
        || sql->bindInt64(2, info.objectStoreIdentifier()) != SQLITE_OK
        || sql->bindBlob(3, indexKeyBuffer->data(), indexKeyBuffer->size()) != SQLITE_OK) {
        LOG_ERROR("Error checking for index record in database");
        return IDBError { UnknownError, "Error checking for index record in database"_s };
    }

    int sqlResult = sql->step();
    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE)
        return IDBError { };

    if (sqlResult != SQLITE_ROW) {
        // There was an error fetching the record from the database.
        LOG_ERROR("Could not check if key exists in index (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Error checking for existence of IDBKey in index"_s };
    }

    hasRecord = true;
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedPutIndexKey(const IDBIndexInfo& info, const IDBKeyData& key, const IndexKey& indexKey, int64_t recordID)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::uncheckedPutIndexKey - (%" PRIu64 ") %s, %s", info.identifier(), key.loggingString().utf8().data(), indexKey.asOneKey().loggingString().utf8().data());

    Vector<IDBKeyData> indexKeys;
    if (info.multiEntry())
        indexKeys = indexKey.multiEntry();
    else
        indexKeys.append(indexKey.asOneKey());

    if (info.unique()) {
        bool hasRecord;
        IDBError error;
        for (auto& indexKey : indexKeys) {
            if (!indexKey.isValid())
                continue;
            error = uncheckedHasIndexRecord(info, indexKey, hasRecord);
            if (!error.isNull())
                return error;
            if (hasRecord)
                return IDBError(ConstraintError);
        }
    }

    for (auto& indexKey : indexKeys) {
        if (!indexKey.isValid())
            continue;
        auto error = uncheckedPutIndexRecord(info.objectStoreIdentifier(), info.identifier(), key, indexKey, recordID);
        if (!error.isNull()) {
            LOG_ERROR("Unable to put index record for newly created index");
            return error;
        }
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedPutIndexRecord(int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyData& keyValue, const WebCore::IDBKeyData& indexKey, int64_t recordID)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::uncheckedPutIndexRecord - %s, %s", keyValue.loggingString().utf8().data(), indexKey.loggingString().utf8().data());

    RefPtr<SharedBuffer> indexKeyBuffer = serializeIDBKeyData(indexKey);
    if (!indexKeyBuffer) {
        LOG_ERROR("Unable to serialize index key to be stored in the database");
        return IDBError { UnknownError, "Unable to serialize index key to be stored in the database"_s };
    }

    RefPtr<SharedBuffer> valueBuffer = serializeIDBKeyData(keyValue);
    if (!valueBuffer) {
        LOG_ERROR("Unable to serialize the value to be stored in the database");
        return IDBError { UnknownError, "Unable to serialize value to be stored in the database"_s };
    }

    {
        auto* sql = cachedStatement(SQL::PutIndexRecord, "INSERT INTO IndexRecords VALUES (?, ?, CAST(? AS TEXT), CAST(? AS TEXT), ?);"_s);
        if (!sql
            || sql->bindInt64(1, indexID) != SQLITE_OK
            || sql->bindInt64(2, objectStoreID) != SQLITE_OK
            || sql->bindBlob(3, indexKeyBuffer->data(), indexKeyBuffer->size()) != SQLITE_OK
            || sql->bindBlob(4, valueBuffer->data(), valueBuffer->size()) != SQLITE_OK
            || sql->bindInt64(5, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not put index record for index %" PRIi64 " in object store %" PRIi64 " in Records table (%i) - %s", indexID, objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Error putting index record into database because no enough space for domain"_s };
            return IDBError { UnknownError, "Error putting index record into database"_s };
        }
    }

    return IDBError { };
}


IDBError SQLiteIDBBackingStore::deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteIndex - object store %" PRIu64, objectStoreIdentifier);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to delete index without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to delete index without an in-progress transaction"_s };
    }

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to delete index during a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to delete index during a non-version-change transaction"_s };
    }

    {
        auto* sql = cachedStatement(SQL::DeleteIndexInfo, "DELETE FROM IndexInfo WHERE id = ? AND objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexIdentifier) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index id %" PRIi64 " from IndexInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error deleting index from database"_s };
        }
    }

    {
        auto* sql = cachedStatement(SQL::DeleteIndexRecords, "DELETE FROM IndexRecords WHERE indexID = ? AND objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexIdentifier) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records for index id %" PRIi64 " from IndexRecords table (%i) - %s", indexIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error deleting index records from database"_s };
        }
    }

    auto* objectStore = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    ASSERT(objectStore);
    objectStore->deleteIndex(indexIdentifier);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::renameIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::renameIndex - object store %" PRIu64 ", index %" PRIu64, objectStoreIdentifier, indexIdentifier);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo)
        return IDBError { UnknownError, "Could not rename index"_s };

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo)
        return IDBError { UnknownError, "Could not rename index"_s };

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to rename an index without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to rename an index without an in-progress transaction"_s };
    }

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to rename an index in a non-version-change transaction");
        return IDBError { UnknownError, "Attempt to rename an index in a non-version-change transaction"_s };
    }

    {
        auto* sql = cachedStatement(SQL::RenameIndex, "UPDATE IndexInfo SET name = ? WHERE objectStoreID = ? AND id = ?;"_s);
        if (!sql
            || sql->bindText(1, newName) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->bindInt64(3, indexIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not update name for index id (%" PRIi64 ", %" PRIi64 ") in IndexInfo table (%i) - %s", objectStoreIdentifier, indexIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Could not rename index because no enough space for domain"_s };
            return IDBError { UnknownError, "Could not rename index"_s };
        }
    }

    indexInfo->rename(newName);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, const IDBKeyData& keyData, bool& keyExists)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::keyExistsInObjectStore - key %s, object store %" PRIu64, keyData.loggingString().utf8().data(), objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    keyExists = false;

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to see if key exists in objectstore without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to see if key exists in objectstore without an in-progress transaction"_s };
    }

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to check for existence in object store");
        return IDBError { UnknownError, "Unable to serialize IDBKey to check for existence in object store"_s };
    }
    auto* sql = cachedStatement(SQL::KeyExistsInObjectStore, "SELECT key FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT) LIMIT 1;"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK
        || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Unable to check for existence of IDBKey in object store"_s };
    }

    int sqlResult = sql->step();
    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE)
        return IDBError { };

    if (sqlResult != SQLITE_ROW) {
        // There was an error fetching the record from the database.
        LOG_ERROR("Could not check if key exists in object store (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Error checking for existence of IDBKey in object store"_s };
    }

    keyExists = true;
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::deleteUnusedBlobFileRecords(SQLiteIDBTransaction& transaction)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteUnusedBlobFileRecords");

    // Gather the set of blob URLs and filenames that are no longer in use.
    HashSet<String> removedBlobFilenames;
    {
        auto* sql = cachedStatement(SQL::GetUnusedBlobFilenames, "SELECT fileName FROM BlobFiles WHERE blobURL NOT IN (SELECT blobURL FROM BlobRecords)"_s);

        if (!sql) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not gather unused blobURLs) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error deleting stored blobs"_s };
        }

        int result = sql->step();
        while (result == SQLITE_ROW) {
            removedBlobFilenames.add(sql->getColumnText(0));
            result = sql->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not gather unused blobURLs) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error deleting stored blobs"_s };
        }
    }

    // Remove the blob records that are no longer in use.
    if (!removedBlobFilenames.isEmpty()) {
        auto* sql = cachedStatement(SQL::DeleteUnusedBlobs, "DELETE FROM BlobFiles WHERE blobURL NOT IN (SELECT blobURL FROM BlobRecords)"_s);

        if (!sql
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not delete blobFile records) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error deleting stored blobs"_s };
        }
    }

    for (auto& file : removedBlobFilenames)
        transaction.addRemovedBlobFile(file);

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::deleteRecord(SQLiteIDBTransaction& transaction, int64_t objectStoreID, const IDBKeyData& keyData)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteRecord - key %s, object store %" PRIu64, keyData.loggingString().utf8().data(), objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());
    ASSERT(transaction.inProgress());
    ASSERT(transaction.mode() != IDBTransactionMode::Readonly);
    UNUSED_PARAM(transaction);

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKeyData to be removed from the database");
        return IDBError { UnknownError, "Unable to serialize IDBKeyData to be removed from the database"_s };
    }

    // Get the record ID
    int64_t recordID;
    {
        auto* sql = cachedStatement(SQL::GetObjectStoreRecordID, "SELECT recordID FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to delete record from object store"_s };
        }

        int result = sql->step();

        // If there's no record ID, there's no record to delete.
        if (result == SQLITE_DONE)
            return IDBError { };

        if (result != SQLITE_ROW) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (unable to fetch record ID) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to delete record from object store"_s };
        }

        recordID = sql->getColumnInt64(0);
    }

    if (recordID < 1) {
        LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (record ID is invalid) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Failed to delete record from object store"_s };
    }

    // Delete the blob records for this object store record.
    {
        auto* sql = cachedStatement(SQL::DeleteBlobRecord, "DELETE FROM BlobRecords WHERE objectStoreRow = ?;"_s);

        if (!sql
            || sql->bindInt64(1, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (Could not delete BlobRecords records) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to delete record from object store"_s };
        }
    }

    auto error = deleteUnusedBlobFileRecords(transaction);
    if (!error.isNull())
        return error;

    // Delete record from object store
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreRecord, "DELETE FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to delete record from object store"_s };
        }
    }

    // Delete record from indexes store
    {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreIndexRecord, "DELETE FROM IndexRecords WHERE objectStoreID = ? AND value = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from indexes for object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to delete index entries for object store record"_s };
        }
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, const IDBKeyRangeData& keyRange)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteRange - range %s, object store %" PRIu64, keyRange.loggingString().utf8().data(), objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to delete range from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to delete range from database without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to delete records from an object store in a read-only transaction");
        return IDBError { UnknownError, "Attempt to delete records from an object store in a read-only transaction"_s };
    }

    // If the range to delete is exactly one key we can delete it right now.
    if (keyRange.isExactlyOneKey()) {
        auto error = deleteRecord(*transaction, objectStoreID, keyRange.lowerKey);
        if (!error.isNull()) {
            LOG_ERROR("Failed to delete record for key '%s'", keyRange.lowerKey.loggingString().utf8().data());
            return error;
        }

        transaction->notifyCursorsOfChanges(objectStoreID);

        return IDBError { };
    }

    auto cursor = transaction->maybeOpenBackingStoreCursor(objectStoreID, 0, keyRange);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to delete range of records from the database");
        return IDBError { UnknownError, "Cannot open cursor to delete range of records from the database"_s };
    }

    Vector<IDBKeyData> keys;
    while (!cursor->didComplete() && !cursor->didError()) {
        keys.append(cursor->currentKey());
        cursor->advance(1);
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while accumulating range of records from the database");
        return IDBError { UnknownError, "Cursor failed while accumulating range of records from the database"_s };
    }

    IDBError error;
    for (auto& key : keys) {
        error = deleteRecord(*transaction, objectStoreID, key);
        if (!error.isNull()) {
            LOG_ERROR("deleteRange: Error deleting keys in range");
            break;
        }
    }

    transaction->notifyCursorsOfChanges(objectStoreID);

    return error;
}

IDBError SQLiteIDBBackingStore::updateOneIndexForAddRecord(const IDBIndexInfo& info, const IDBKeyData& key, const ThreadSafeDataBuffer& value, int64_t recordID)
{
    JSLockHolder locker(vm());

    auto jsValue = deserializeIDBValueToJSValue(*globalObject().globalExec(), value);
    if (jsValue.isUndefinedOrNull())
        return IDBError { };

    IndexKey indexKey;
    generateIndexKeyForValue(*m_globalObject->globalExec(), info, jsValue, indexKey);

    if (indexKey.isNull())
        return IDBError { };

    return uncheckedPutIndexKey(info, key, indexKey, recordID);
}

IDBError SQLiteIDBBackingStore::updateAllIndexesForAddRecord(const IDBObjectStoreInfo& info, const IDBKeyData& key, const ThreadSafeDataBuffer& value, int64_t recordID)
{
    JSLockHolder locker(vm());

    auto jsValue = deserializeIDBValueToJSValue(*globalObject().globalExec(), value);
    if (jsValue.isUndefinedOrNull())
        return IDBError { };

    IDBError error;
    bool anyRecordsSucceeded = false;
    for (auto& index : info.indexMap().values()) {
        IndexKey indexKey;
        generateIndexKeyForValue(*m_globalObject->globalExec(), index, jsValue, indexKey);

        if (indexKey.isNull())
            continue;

        error = uncheckedPutIndexKey(index, key, indexKey, recordID);
        if (!error.isNull())
            break;

        anyRecordsSucceeded = true;
    }

    if (!error.isNull() && anyRecordsSucceeded) {
        RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(key);

        auto* sql = cachedStatement(SQL::DeleteObjectStoreIndexRecord, "DELETE FROM IndexRecords WHERE objectStoreID = ? AND value = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Adding one Index record failed, but failed to remove all others that previously succeeded");
            return IDBError { UnknownError, "Adding one Index record failed, but failed to remove all others that previously succeeded"_s };
        }
    }

    return error;
}

IDBError SQLiteIDBBackingStore::addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& objectStoreInfo, const IDBKeyData& keyData, const IDBValue& value)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::addRecord - key %s, object store %" PRIu64, keyData.loggingString().utf8().data(), objectStoreInfo.identifier());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());
    ASSERT(value.data().data());
    ASSERT(value.blobURLs().size() == value.blobFilePaths().size());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to store a record in an object store without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to store a record in an object store without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to store a record in an object store in a read-only transaction");
        return IDBError { UnknownError, "Attempt to store a record in an object store in a read-only transaction"_s };
    }

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in an object store");
        return IDBError { UnknownError, "Unable to serialize IDBKey to be stored in an object store"_s };
    }

    int64_t recordID = 0;
    {
        auto* sql = cachedStatement(SQL::AddObjectStoreRecord, "INSERT INTO Records VALUES (?, CAST(? AS TEXT), ?, NULL);"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreInfo.identifier()) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql->bindBlob(3, value.data().data()->data(), value.data().data()->size()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not put record for object store %" PRIi64 " in Records table (%i) - %s", objectStoreInfo.identifier(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            if (m_sqliteDB->lastError() == SQLITE_FULL)
                return IDBError { QuotaExceededError, "Unable to store record in object store because no enough space for domain"_s };
            return IDBError { UnknownError, "Unable to store record in object store"_s };
        }

        recordID = m_sqliteDB->lastInsertRowID();
    }

    auto error = updateAllIndexesForAddRecord(objectStoreInfo, keyData, value.data(), recordID);

    if (!error.isNull()) {
        auto* sql = cachedStatement(SQL::DeleteObjectStoreRecord, "DELETE FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreInfo.identifier()) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Indexing new object store record failed, but unable to remove the object store record itself");
            return IDBError { UnknownError, "Indexing new object store record failed, but unable to remove the object store record itself"_s };
        }

        return error;
    }

    const Vector<String>& blobURLs = value.blobURLs();
    const Vector<String>& blobFiles = value.blobFilePaths();
    for (size_t i = 0; i < blobURLs.size(); ++i) {
        auto& url = blobURLs[i];
        {
            auto* sql = cachedStatement(SQL::AddBlobRecord, "INSERT INTO BlobRecords VALUES (?, ?);"_s);
            if (!sql
                || sql->bindInt64(1, recordID) != SQLITE_OK
                || sql->bindText(2, url) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Unable to record Blob record in database");
                if (m_sqliteDB->lastError() == SQLITE_FULL)
                    return IDBError { QuotaExceededError, "Unable to record Blob record in database because no enough space for domain"_s };
                return IDBError { UnknownError, "Unable to record Blob record in database"_s };
            }
        }
        int64_t potentialFileNameInteger = m_sqliteDB->lastInsertRowID();

        // If we already have a file for this blobURL, nothing left to do.
        {
            auto* sql = cachedStatement(SQL::BlobFilenameForBlobURL, "SELECT fileName FROM BlobFiles WHERE blobURL = ?;"_s);
            if (!sql
                || sql->bindText(1, url) != SQLITE_OK) {
                LOG_ERROR("Unable to examine Blob filenames in database");
                return IDBError { UnknownError, "Unable to examine Blob filenames in database"_s };
            }

            int result = sql->step();
            if (result != SQLITE_ROW && result != SQLITE_DONE) {
                LOG_ERROR("Unable to examine Blob filenames in database");
                return IDBError { UnknownError, "Unable to examine Blob filenames in database"_s };
            }

            if (result == SQLITE_ROW)
                continue;
        }

        // We don't already have a file for this blobURL, so commit our file as a unique filename
        String storedFilename = makeString(potentialFileNameInteger, ".blob");
        {
            auto* sql = cachedStatement(SQL::AddBlobFilename, "INSERT INTO BlobFiles VALUES (?, ?);"_s);
            if (!sql
                || sql->bindText(1, url) != SQLITE_OK
                || sql->bindText(2, storedFilename) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Unable to record Blob file record in database");
                if (m_sqliteDB->lastError() == SQLITE_FULL)
                    return IDBError { QuotaExceededError, "Unable to record Blob file in database because no enough space for domain"_s };
                return IDBError { UnknownError, "Unable to record Blob file record in database"_s };
            }
        }

        transaction->addBlobFile(blobFiles[i], storedFilename);
    }

    transaction->notifyCursorsOfChanges(objectStoreInfo.identifier());

    return error;
}

IDBError SQLiteIDBBackingStore::getBlobRecordsForObjectStoreRecord(int64_t objectStoreRecord, Vector<String>& blobURLs, PAL::SessionID& sessionID, Vector<String>& blobFilePaths)
{
    ASSERT(objectStoreRecord);

    HashSet<String> blobURLSet;
    {
        auto* sql = cachedStatement(SQL::GetBlobURL, "SELECT blobURL FROM BlobRecords WHERE objectStoreRow = ?"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreRecord) != SQLITE_OK) {
            LOG_ERROR("Could not prepare statement to fetch blob URLs for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        int sqlResult = sql->step();
        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There are no blobURLs in the database for this object store record.
            return IDBError { };
        }

        while (sqlResult == SQLITE_ROW) {
            blobURLSet.add(sql->getColumnText(0));
            sqlResult = sql->step();
        }

        if (sqlResult != SQLITE_DONE) {
            LOG_ERROR("Could not fetch blob URLs for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }
    }

    ASSERT(!blobURLSet.isEmpty());
    String databaseDirectory = fullDatabaseDirectory();
    for (auto& blobURL : blobURLSet) {
        auto* sql = cachedStatement(SQL::BlobFilenameForBlobURL, "SELECT fileName FROM BlobFiles WHERE blobURL = ?;"_s);
        if (!sql
            || sql->bindText(1, blobURL) != SQLITE_OK) {
            LOG_ERROR("Could not prepare statement to fetch blob filename for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        if (sql->step() != SQLITE_ROW) {
            LOG_ERROR("Entry for blob filename for blob url %s does not exist (%i) - %s", blobURL.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        blobURLs.append(blobURL);

        String fileName = sql->getColumnText(0);
        blobFilePaths.append(FileSystem::pathByAppendingComponent(databaseDirectory, fileName));
    }
    sessionID = m_identifier.sessionID();

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, const IDBKeyRangeData& keyRange, IDBGetRecordDataType type, IDBGetResult& resultValue)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getRecord - key range %s, object store %" PRIu64, keyRange.loggingString().utf8().data(), objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get a record from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to get a record from database without an in-progress transaction"_s };
    }

    auto key = keyRange.lowerKey;
    if (key.isNull())
        key = IDBKeyData::minimum();
    RefPtr<SharedBuffer> lowerBuffer = serializeIDBKeyData(key);
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize lower IDBKey in lookup range");
        return IDBError { UnknownError, "Unable to serialize lower IDBKey in lookup range"_s };
    }

    key = keyRange.upperKey;
    if (key.isNull())
        key = IDBKeyData::maximum();
    RefPtr<SharedBuffer> upperBuffer = serializeIDBKeyData(key);
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize upper IDBKey in lookup range");
        return IDBError { UnknownError, "Unable to serialize upper IDBKey in lookup range"_s };
    }

    int64_t recordID = 0;
    ThreadSafeDataBuffer resultBuffer;
    {
        static const char* const lowerOpenUpperOpen = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerOpenUpperClosed = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerClosedUpperOpen = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerClosedUpperClosed = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";

        static const char* const lowerOpenUpperOpenKeyOnly = "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerOpenUpperClosedKeyOnly = "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerClosedUpperOpenKeyOnly = "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
        static const char* const lowerClosedUpperClosedKeyOnly = "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";

        SQLiteStatement* sql = nullptr;

        switch (type) {
        case IDBGetRecordDataType::KeyAndValue:
            if (keyRange.lowerOpen) {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetValueRecordsLowerOpenUpperOpen, lowerOpenUpperOpen);
                else
                    sql = cachedStatement(SQL::GetValueRecordsLowerOpenUpperClosed, lowerOpenUpperClosed);
            } else {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetValueRecordsLowerClosedUpperOpen, lowerClosedUpperOpen);
                else
                    sql = cachedStatement(SQL::GetValueRecordsLowerClosedUpperClosed, lowerClosedUpperClosed);
            }
            break;
        case IDBGetRecordDataType::KeyOnly:
            if (keyRange.lowerOpen) {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetKeyRecordsLowerOpenUpperOpen, lowerOpenUpperOpenKeyOnly);
                else
                    sql = cachedStatement(SQL::GetKeyRecordsLowerOpenUpperClosed, lowerOpenUpperClosedKeyOnly);
            } else {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetKeyRecordsLowerClosedUpperOpen, lowerClosedUpperOpenKeyOnly);
                else
                    sql = cachedStatement(SQL::GetKeyRecordsLowerClosedUpperClosed, lowerClosedUpperClosedKeyOnly);
            }
        }

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, lowerBuffer->data(), lowerBuffer->size()) != SQLITE_OK
            || sql->bindBlob(3, upperBuffer->data(), upperBuffer->size()) != SQLITE_OK) {
            LOG_ERROR("Could not get key range record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Failed to look up record in object store by key range"_s };
        }

        int sqlResult = sql->step();

        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There was no record for the key in the database.
            return IDBError { };
        }
        if (sqlResult != SQLITE_ROW) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { UnknownError, "Error looking up record in object store by key range"_s };
        }

        Vector<uint8_t> buffer;
        sql->getColumnBlobAsVector(0, buffer);
        resultBuffer = ThreadSafeDataBuffer::create(WTFMove(buffer));

        if (type == IDBGetRecordDataType::KeyAndValue)
            recordID = sql->getColumnInt64(1);
    }

    if (type == IDBGetRecordDataType::KeyOnly) {
        auto* vector = resultBuffer.data();
        if (!vector) {
            LOG_ERROR("Unable to deserialize key data from database for IDBObjectStore.getKey()");
            return IDBError { UnknownError, "Error extracting key data from database executing IDBObjectStore.getKey()"_s };
        }

        IDBKeyData keyData;
        if (!deserializeIDBKeyData(vector->data(), vector->size(), keyData)) {
            LOG_ERROR("Unable to deserialize key data from database for IDBObjectStore.getKey()");
            return IDBError { UnknownError, "Error extracting key data from database executing IDBObjectStore.getKey()"_s };
        }

        resultValue = { keyData };
        return IDBError { };
    }

    ASSERT(recordID);
    Vector<String> blobURLs, blobFilePaths;
    PAL::SessionID sessionID;
    auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, sessionID, blobFilePaths);
    ASSERT(blobURLs.size() == blobFilePaths.size());

    if (!error.isNull())
        return error;

    resultValue = { { resultBuffer, WTFMove(blobURLs), sessionID, WTFMove(blobFilePaths) } };
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    return getAllRecordsData.indexIdentifier ? getAllIndexRecords(transactionIdentifier, getAllRecordsData, result) : getAllObjectStoreRecords(transactionIdentifier, getAllRecordsData, result);
}

SQLiteStatement* SQLiteIDBBackingStore::cachedStatementForGetAllObjectStoreRecords(const IDBGetAllRecordsData& getAllRecordsData)
{
    static const char* const lowerOpenUpperOpenKey ="SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerOpenUpperClosedKey = "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerClosedUpperOpenKey = "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerClosedUpperClosedKey = "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerOpenUpperOpenValue = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerOpenUpperClosedValue = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerClosedUpperOpenValue = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;";
    static const char* const lowerClosedUpperClosedValue = "SELECT value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;";

    if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Keys) {
        if (getAllRecordsData.keyRangeData.lowerOpen) {
            if (getAllRecordsData.keyRangeData.upperOpen)
                return cachedStatement(SQL::GetAllKeyRecordsLowerOpenUpperOpen, lowerOpenUpperOpenKey);
            return cachedStatement(SQL::GetAllKeyRecordsLowerOpenUpperClosed, lowerOpenUpperClosedKey);
        }

        if (getAllRecordsData.keyRangeData.upperOpen)
            return cachedStatement(SQL::GetAllKeyRecordsLowerClosedUpperOpen, lowerClosedUpperOpenKey);
        return cachedStatement(SQL::GetAllKeyRecordsLowerClosedUpperClosed, lowerClosedUpperClosedKey);
    }

    if (getAllRecordsData.keyRangeData.lowerOpen) {
        if (getAllRecordsData.keyRangeData.upperOpen)
            return cachedStatement(SQL::GetValueRecordsLowerOpenUpperOpen, lowerOpenUpperOpenValue);
        return cachedStatement(SQL::GetValueRecordsLowerOpenUpperClosed, lowerOpenUpperClosedValue);
    }

    if (getAllRecordsData.keyRangeData.upperOpen)
        return cachedStatement(SQL::GetValueRecordsLowerClosedUpperOpen, lowerClosedUpperOpenValue);
    return cachedStatement(SQL::GetValueRecordsLowerClosedUpperClosed, lowerClosedUpperClosedValue);
}

IDBError SQLiteIDBBackingStore::getAllObjectStoreRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getAllObjectStoreRecords");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get records from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to get records from database without an in-progress transaction"_s };
    }

    auto key = getAllRecordsData.keyRangeData.lowerKey;
    if (key.isNull())
        key = IDBKeyData::minimum();
    auto lowerBuffer = serializeIDBKeyData(key);
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize lower IDBKey in lookup range");
        return IDBError { UnknownError, "Unable to serialize lower IDBKey in lookup range"_s };
    }

    key = getAllRecordsData.keyRangeData.upperKey;
    if (key.isNull())
        key = IDBKeyData::maximum();
    auto upperBuffer = serializeIDBKeyData(key);
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize upper IDBKey in lookup range");
        return IDBError { UnknownError, "Unable to serialize upper IDBKey in lookup range"_s };
    }

    auto* sql = cachedStatementForGetAllObjectStoreRecords(getAllRecordsData);
    if (!sql
        || sql->bindInt64(1, getAllRecordsData.objectStoreIdentifier) != SQLITE_OK
        || sql->bindBlob(2, lowerBuffer->data(), lowerBuffer->size()) != SQLITE_OK
        || sql->bindBlob(3, upperBuffer->data(), upperBuffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not get key range record from object store %" PRIi64 " from Records table (%i) - %s", getAllRecordsData.objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Failed to look up record in object store by key range"_s };
    }

    result = { getAllRecordsData.getAllType };

    uint32_t targetResults;
    if (getAllRecordsData.count && getAllRecordsData.count.value())
        targetResults = getAllRecordsData.count.value();
    else
        targetResults = std::numeric_limits<uint32_t>::max();

    int sqlResult = sql->step();
    uint32_t returnedResults = 0;

    while (sqlResult == SQLITE_ROW && returnedResults < targetResults) {
        if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Values) {
            Vector<uint8_t> buffer;
            sql->getColumnBlobAsVector(0, buffer);
            ThreadSafeDataBuffer resultBuffer = ThreadSafeDataBuffer::create(WTFMove(buffer));

            auto recordID = sql->getColumnInt64(1);

            ASSERT(recordID);
            Vector<String> blobURLs, blobFilePaths;
            PAL::SessionID sessionID;
            auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, sessionID, blobFilePaths);
            ASSERT(blobURLs.size() == blobFilePaths.size());

            if (!error.isNull())
                return error;

            result.addValue({ resultBuffer, WTFMove(blobURLs), sessionID, WTFMove(blobFilePaths) });
        } else {
            Vector<uint8_t> keyData;
            IDBKeyData key;
            sql->getColumnBlobAsVector(0, keyData);

            if (!deserializeIDBKeyData(keyData.data(), keyData.size(), key)) {
                LOG_ERROR("Unable to deserialize key data from database while getting all key records");
                return IDBError { UnknownError, "Unable to deserialize key data while getting all key records"_s };
            }

            result.addKey(WTFMove(key));
        }

        ++returnedResults;
        sqlResult = sql->step();
    }

    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE || sqlResult == SQLITE_ROW) {
        // Finished getting results
        return IDBError { };
    }

    // There was an error fetching records from the database.
    LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", getAllRecordsData.objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
    return IDBError { UnknownError, "Error looking up record in object store by key range"_s };
}

IDBError SQLiteIDBBackingStore::getAllIndexRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getAllIndexRecords - %s", getAllRecordsData.keyRangeData.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get all index records from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to get all index records from database without an in-progress transaction"_s };
    }

    auto cursor = transaction->maybeOpenBackingStoreCursor(getAllRecordsData.objectStoreIdentifier, getAllRecordsData.indexIdentifier, getAllRecordsData.keyRangeData);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index gets in database");
        return IDBError { UnknownError, "Cannot open cursor to perform index gets in database"_s };
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index records in database");
        return IDBError { UnknownError, "Cursor failed while looking up index records in database"_s };
    }

    result = { getAllRecordsData.getAllType };
    uint32_t currentCount = 0;
    uint32_t targetCount = getAllRecordsData.count ? getAllRecordsData.count.value() : 0;
    if (!targetCount)
        targetCount = std::numeric_limits<uint32_t>::max();
    while (!cursor->didComplete() && !cursor->didError() && currentCount < targetCount) {
        if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Keys) {
            IDBKeyData keyCopy = cursor->currentPrimaryKey();
            result.addKey(WTFMove(keyCopy));
        } else
            result.addValue(cursor->currentValue() ? *cursor->currentValue() : IDBValue());

        ++currentCount;
        cursor->advance(1);
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index records in database");
        return IDBError { UnknownError, "Cursor failed while looking up index records in database"_s };
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t indexID, IndexedDB::IndexRecordType type, const IDBKeyRangeData& range, IDBGetResult& getResult)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getIndexRecord - %s", range.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get an index record from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to get an index record from database without an in-progress transaction"_s };
    }

    if (range.isExactlyOneKey())
        return uncheckedGetIndexRecordForOneKey(indexID, objectStoreID, type, range.lowerKey, getResult);

    auto cursor = transaction->maybeOpenBackingStoreCursor(objectStoreID, indexID, range);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index get in database");
        return IDBError { UnknownError, "Cannot open cursor to perform index get in database"_s };
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index record in database");
        return IDBError { UnknownError, "Cursor failed while looking up index record in database"_s };
    }

    if (cursor->didComplete())
        getResult = { };
    else {
        if (type == IndexedDB::IndexRecordType::Key)
            getResult = { cursor->currentPrimaryKey() };
        else
            getResult = { cursor->currentValue() ? *cursor->currentValue() : IDBValue(), cursor->currentPrimaryKey() };
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedGetIndexRecordForOneKey(int64_t indexID, int64_t objectStoreID, IndexedDB::IndexRecordType type, const IDBKeyData& key, IDBGetResult& getResult)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::uncheckedGetIndexRecordForOneKey");

    ASSERT(key.isValid() && key.type() != IndexedDB::KeyType::Max && key.type() != IndexedDB::KeyType::Min);

    RefPtr<SharedBuffer> buffer = serializeIDBKeyData(key);
    if (!buffer) {
        LOG_ERROR("Unable to serialize IDBKey to look up one index record");
        return IDBError { UnknownError, "Unable to serialize IDBKey to look up one index record"_s };
    }

    auto* sql = cachedStatement(SQL::GetIndexRecordForOneKey, "SELECT IndexRecords.value, Records.value, Records.recordID FROM Records INNER JOIN IndexRecords ON Records.recordID = IndexRecords.objectStoreRecordID WHERE IndexRecords.indexID = ? AND IndexRecords.objectStoreID = ? AND IndexRecords.key = CAST(? AS TEXT) ORDER BY IndexRecords.key, IndexRecords.value"_s);

    if (!sql
        || sql->bindInt64(1, indexID) != SQLITE_OK
        || sql->bindInt64(2, objectStoreID) != SQLITE_OK
        || sql->bindBlob(3, buffer->data(), buffer->size()) != SQLITE_OK) {
        LOG_ERROR("Unable to lookup index record in database");
        return IDBError { UnknownError, "Unable to lookup index record in database"_s };
    }

    int result = sql->step();
    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        LOG_ERROR("Unable to lookup index record in database");
        return IDBError { UnknownError, "Unable to lookup index record in database"_s };
    }

    if (result == SQLITE_DONE)
        return IDBError { };

    IDBKeyData objectStoreKey;
    Vector<uint8_t> keyVector;
    sql->getColumnBlobAsVector(0, keyVector);

    if (!deserializeIDBKeyData(keyVector.data(), keyVector.size(), objectStoreKey)) {
        LOG_ERROR("Unable to deserialize key looking up index record in database");
        return IDBError { UnknownError, "Unable to deserialize key looking up index record in database"_s };
    }

    if (type == IndexedDB::IndexRecordType::Key) {
        getResult = { objectStoreKey };
        return IDBError { };
    }

    sql->getColumnBlobAsVector(1, keyVector);

    int64_t recordID = sql->getColumnInt64(2);
    Vector<String> blobURLs, blobFilePaths;
    PAL::SessionID sessionID;
    auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, sessionID, blobFilePaths);
    ASSERT(blobURLs.size() == blobFilePaths.size());

    if (!error.isNull())
        return error;

    getResult = { { ThreadSafeDataBuffer::create(WTFMove(keyVector)), WTFMove(blobURLs), sessionID, WTFMove(blobFilePaths) }, objectStoreKey };
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, uint64_t& outCount)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getCount - object store %" PRIu64, objectStoreIdentifier);
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    outCount = 0;

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get count from database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to get count from database without an in-progress transaction"_s };
    }

    auto cursor = transaction->maybeOpenBackingStoreCursor(objectStoreIdentifier, indexIdentifier, range);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to populate indexes in database");
        return IDBError { UnknownError, "Unable to populate indexes in database"_s };
    }

    while (cursor->advance(1))
        ++outCount;

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedGetKeyGeneratorValue(int64_t objectStoreID, uint64_t& outValue)
{
    auto* sql = cachedStatement(SQL::GetKeyGeneratorValue, "SELECT currentKey FROM KeyGenerators WHERE objectStoreID = ?;"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK) {
        LOG_ERROR("Could not retrieve currentKey from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { UnknownError, "Error getting current key generator value from database"_s };
    }
    int result = sql->step();
    if (result != SQLITE_ROW) {
        LOG_ERROR("Could not retreive key generator value for object store, but it should be there.");
        return IDBError { UnknownError, "Error finding current key generator value in database"_s };
    }

    int64_t value = sql->getColumnInt64(0);
    if (value < 0)
        return IDBError { ConstraintError, "Current key generator value from database is invalid" };

    outValue = value;
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedSetKeyGeneratorValue(int64_t objectStoreID, uint64_t value)
{
    auto* sql = cachedStatement(SQL::SetKeyGeneratorValue, "INSERT INTO KeyGenerators VALUES (?, ?);"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK
        || sql->bindInt64(2, value) != SQLITE_OK
        || sql->step() != SQLITE_DONE) {
        LOG_ERROR("Could not update key generator value (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        if (m_sqliteDB->lastError() == SQLITE_FULL)
            return IDBError { QuotaExceededError, "Error storing new key generator value in database because no enough space for domain"_s };
        return IDBError { ConstraintError, "Error storing new key generator value in database" };
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t& generatedKey)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::generateKeyNumber");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to generate key in database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to generate key in database without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to generate key in a read-only transaction");
        return IDBError { UnknownError, "Attempt to generate key in a read-only transaction"_s };
    }

    uint64_t currentValue;
    auto error = uncheckedGetKeyGeneratorValue(objectStoreID, currentValue);
    if (!error.isNull())
        return error;

    if (currentValue + 1 > maxGeneratorValue)
        return IDBError { ConstraintError, "Cannot generate new key value over 2^53 for object store operation" };

    generatedKey = currentValue + 1;
    return uncheckedSetKeyGeneratorValue(objectStoreID, generatedKey);
}

IDBError SQLiteIDBBackingStore::revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t newKeyNumber)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::revertGeneratedKeyNumber - object store %" PRIu64 ", reverted number %" PRIu64, objectStoreID, newKeyNumber);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to revert key generator value in database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to revert key generator value in database without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to revert key generator value in a read-only transaction");
        return IDBError { UnknownError, "Attempt to revert key generator value in a read-only transaction"_s };
    }

    ASSERT(newKeyNumber);
    return uncheckedSetKeyGeneratorValue(objectStoreID, newKeyNumber - 1);
}

IDBError SQLiteIDBBackingStore::maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, double newKeyNumber)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::maybeUpdateKeyGeneratorNumber");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to update key generator value in database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to update key generator value in database without an in-progress transaction"_s };
    }
    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to update key generator value in a read-only transaction");
        return IDBError { UnknownError, "Attempt to update key generator value in a read-only transaction"_s };
    }

    uint64_t currentValue;
    auto error = uncheckedGetKeyGeneratorValue(objectStoreID, currentValue);
    if (!error.isNull())
        return error;

    if (newKeyNumber <= currentValue)
        return IDBError { };

    return uncheckedSetKeyGeneratorValue(objectStoreID, std::min(newKeyNumber, (double)maxGeneratorValue));
}

IDBError SQLiteIDBBackingStore::openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo& info, IDBGetResult& result)
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to open a cursor in database without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to open a cursor in database without an in-progress transaction"_s };
    }

    auto* cursor = transaction->maybeOpenCursor(info);
    if (!cursor) {
        LOG_ERROR("Unable to open cursor");
        return IDBError { UnknownError, "Unable to open cursor"_s };
    }

    m_cursors.set(cursor->identifier(), cursor);

    cursor->currentData(result);
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData& data, IDBGetResult& result)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::iterateCursor");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* cursor = m_cursors.get(cursorIdentifier);
    if (!cursor) {
        LOG_ERROR("Attempt to iterate a cursor that doesn't exist");
        return IDBError { UnknownError, "Attempt to iterate a cursor that doesn't exist"_s };
    }

    ASSERT_UNUSED(transactionIdentifier, cursor->transaction()->transactionIdentifier() == transactionIdentifier);

    if (!cursor->transaction() || !cursor->transaction()->inProgress()) {
        LOG_ERROR("Attempt to iterate a cursor without an in-progress transaction");
        return IDBError { UnknownError, "Attempt to iterate a cursor without an in-progress transaction"_s };
    }

    auto key = data.keyData;
    auto primaryKey = data.primaryKeyData;
    auto count = data.count;

    if (key.isValid()) {
        if (!cursor->iterate(key, primaryKey)) {
            LOG_ERROR("Attempt to iterate cursor failed");
            return IDBError { UnknownError, "Attempt to iterate cursor failed"_s };
        }
    } else {
        ASSERT(!primaryKey.isValid());
        if (!count)
            count = 1;
        if (!cursor->advance(count)) {
            LOG_ERROR("Attempt to advance cursor failed");
            return IDBError { UnknownError, "Attempt to advance cursor failed"_s };
        }
    }

    cursor->currentData(result);
    return IDBError { };
}

bool SQLiteIDBBackingStore::prefetchCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::prefetchCursor");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* cursor = m_cursors.get(cursorIdentifier);
    if (!cursor || !cursor->transaction() || !cursor->transaction()->inProgress())
        return false;

    ASSERT_UNUSED(transactionIdentifier, cursor->transaction()->transactionIdentifier() == transactionIdentifier);

    return cursor->prefetch();
}

IDBObjectStoreInfo* SQLiteIDBBackingStore::infoForObjectStore(uint64_t objectStoreIdentifier)
{
    ASSERT(m_databaseInfo);
    return m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
}

void SQLiteIDBBackingStore::deleteBackingStore()
{
    String dbFilename = fullDatabasePath();

    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteBackingStore deleting file '%s' on disk", dbFilename.utf8().data());

    Vector<String> blobFiles;
    {
        bool errored = true;

        if (m_sqliteDB) {
            SQLiteStatement sql(*m_sqliteDB, "SELECT fileName FROM BlobFiles;"_s);
            if (sql.prepare() == SQLITE_OK) {
                int result = sql.step();
                while (result == SQLITE_ROW) {
                    blobFiles.append(sql.getColumnText(0));
                    result = sql.step();
                }

                if (result == SQLITE_DONE)
                    errored = false;
            }
        }

        if (errored)
            LOG_ERROR("Error getting all blob filenames to be deleted");
    }

    String databaseDirectory = fullDatabaseDirectory();
    for (auto& file : blobFiles) {
        String fullPath = FileSystem::pathByAppendingComponent(databaseDirectory, file);
        if (!FileSystem::deleteFile(fullPath))
            LOG_ERROR("Error deleting blob file %s", fullPath.utf8().data());
    }

    if (m_sqliteDB)
        closeSQLiteDB();

    SQLiteFileSystem::deleteDatabaseFile(dbFilename);
    SQLiteFileSystem::deleteEmptyDatabaseDirectory(fullDatabaseDirectory());
    SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_absoluteDatabaseDirectory);
}

void SQLiteIDBBackingStore::unregisterCursor(SQLiteIDBCursor& cursor)
{
    ASSERT(m_cursors.contains(cursor.identifier()));
    m_cursors.remove(cursor.identifier());
}

SQLiteStatement* SQLiteIDBBackingStore::cachedStatement(SQLiteIDBBackingStore::SQL sql, const char* statement)
{
    if (sql >= SQL::Count) {
        LOG_ERROR("Invalid SQL statement ID passed to cachedStatement()");
        return nullptr;
    }

    if (m_cachedStatements[static_cast<size_t>(sql)]) {
        if (m_cachedStatements[static_cast<size_t>(sql)]->reset() == SQLITE_OK)
            return m_cachedStatements[static_cast<size_t>(sql)].get();
        m_cachedStatements[static_cast<size_t>(sql)] = nullptr;
    }

    if (m_sqliteDB) {
        m_cachedStatements[static_cast<size_t>(sql)] = std::make_unique<SQLiteStatement>(*m_sqliteDB, statement);
        if (m_cachedStatements[static_cast<size_t>(sql)]->prepare() != SQLITE_OK)
            m_cachedStatements[static_cast<size_t>(sql)] = nullptr;
    }

    return m_cachedStatements[static_cast<size_t>(sql)].get();
}

void SQLiteIDBBackingStore::closeSQLiteDB()
{
    for (size_t i = 0; i < static_cast<int>(SQL::Count); ++i)
        m_cachedStatements[i] = nullptr;

    if (m_sqliteDB)
        m_sqliteDB->close();

    m_sqliteDB = nullptr;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
