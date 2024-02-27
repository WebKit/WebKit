/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "IDBBindingUtilities.h"
#include "IDBCursorInfo.h"
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
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {
using namespace JSC;
namespace IDBServer {

constexpr auto objectStoreInfoTableName = "ObjectStoreInfo"_s;
constexpr auto objectStoreInfoTableNameAlternate = "\"ObjectStoreInfo\""_s;
constexpr auto v2ObjectStoreInfoSchema = "CREATE TABLE ObjectStoreInfo (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, autoInc INTEGER NOT NULL ON CONFLICT FAIL)"_s;
constexpr auto v1IndexRecordsRecordIndexSchema = "CREATE INDEX IndexRecordsRecordIndex ON IndexRecords (objectStoreID, objectStoreRecordID)"_s;
constexpr auto IndexRecordsIndexSchema = "CREATE INDEX IndexRecordsIndex ON IndexRecords (indexID, key, value)"_s;

// Current version of the metadata schema being used in the metadata database.
static const int currentMetadataVersion = 1;

// The IndexedDatabase spec defines the max key generator value as 2^53.
static const uint64_t maxGeneratorValue = 0x20000000000000;

#define TABLE_SCHEMA_PREFIX "CREATE TABLE "
#define V3_RECORDS_TABLE_SCHEMA_SUFFIX " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL, recordID INTEGER PRIMARY KEY)"_s
#define V3_INDEX_RECORDS_TABLE_SCHEMA_SUFFIX " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, objectStoreRecordID INTEGER NOT NULL ON CONFLICT FAIL)"_s;
#define INDEX_INFO_TABLE_SCHEMA_SUFFIX " (id INTEGER NOT NULL ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, isUnique INTEGER NOT NULL ON CONFLICT FAIL, multiEntry INTEGER NOT NULL ON CONFLICT FAIL)"_s;
#define BLOB_RECORDS_TABLE_SCHEMA_SUFFIX " (objectStoreRow INTEGER NOT NULL ON CONFLICT FAIL, blobURL TEXT NOT NULL ON CONFLICT FAIL)"_s;
#define BLOB_FILES_TABLE_SCHEMA_SUFFIX " (blobURL TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, fileName TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL)"_s;

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

static const String v1RecordsTableSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v1RecordsTableSchema()
{
    static NeverDestroyed<String> v1RecordsTableSchemaString(v1RecordsTableSchema("Records"_s));
    return v1RecordsTableSchemaString;
}

static const String& v1RecordsTableSchemaAlternate()
{
    static NeverDestroyed<String> v1RecordsTableSchemaString(v1RecordsTableSchema("\"Records\""_s));
    return v1RecordsTableSchemaString;
}

static const String v2RecordsTableSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v2RecordsTableSchema()
{
    static NeverDestroyed<String> v2RecordsTableSchemaString(v2RecordsTableSchema("Records"_s));
    return v2RecordsTableSchemaString;
}

static const String& v2RecordsTableSchemaAlternate()
{
    static NeverDestroyed<String> v2RecordsTableSchemaString(v2RecordsTableSchema("\"Records\""_s));
    return v2RecordsTableSchemaString;
}

static ASCIILiteral v3RecordsTableSchema()
{
    return TABLE_SCHEMA_PREFIX "Records" V3_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral v3RecordsTableSchemaAlternate()
{
    return TABLE_SCHEMA_PREFIX "\"Records\"" V3_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral v3RecordsTableSchemaTemp()
{
    return TABLE_SCHEMA_PREFIX "_Temp_Records" V3_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static const String v1IndexRecordsTableSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value NOT NULL ON CONFLICT FAIL)");
}

static const String& v1IndexRecordsTableSchema()
{
    static NeverDestroyed<String> v1IndexRecordsTableSchemaString(v1IndexRecordsTableSchema("IndexRecords"_s));
    return v1IndexRecordsTableSchemaString;
}

static const String& v1IndexRecordsTableSchemaAlternate()
{
    static NeverDestroyed<String> v1IndexRecordsTableSchemaString(v1IndexRecordsTableSchema("\"IndexRecords\""_s));
    return v1IndexRecordsTableSchemaString;
}

static const String v2IndexRecordsTableSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL, value TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL)");
}

static const String& v2IndexRecordsTableSchema()
{
    static NeverDestroyed<String> v2IndexRecordsTableSchemaString(v2IndexRecordsTableSchema("IndexRecords"_s));
    return v2IndexRecordsTableSchemaString;
}

static const String& v2IndexRecordsTableSchemaAlternate()
{
    static NeverDestroyed<String> v2IndexRecordsTableSchemaString(v2IndexRecordsTableSchema("\"IndexRecords\""_s));
    return v2IndexRecordsTableSchemaString;
}

static ASCIILiteral v3IndexRecordsTableSchema()
{
    return TABLE_SCHEMA_PREFIX "IndexRecords" V3_INDEX_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral v3IndexRecordsTableSchemaAlternate()
{
    return TABLE_SCHEMA_PREFIX "\"IndexRecords\"" V3_INDEX_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral v3IndexRecordsTableSchemaTemp()
{
    return TABLE_SCHEMA_PREFIX "_Temp_IndexRecords" V3_INDEX_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral blobRecordsTableSchema()
{
    return TABLE_SCHEMA_PREFIX "BlobRecords" BLOB_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral blobRecordsTableSchemaAlternate()
{
    return TABLE_SCHEMA_PREFIX "\"BlobRecords\"" BLOB_RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral blobFilesTableSchema()
{
    return TABLE_SCHEMA_PREFIX "BlobFiles" BLOB_FILES_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral blobFilesTableSchemaAlternate()
{
    return TABLE_SCHEMA_PREFIX "\"BlobFiles\"" BLOB_FILES_TABLE_SCHEMA_SUFFIX;
}

static String createV1ObjectStoreInfoSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, autoInc INTEGER NOT NULL ON CONFLICT FAIL, maxIndexID INTEGER NOT NULL ON CONFLICT FAIL)");
}

static String createV2ObjectStoreInfoSchema(ASCIILiteral tableName)
{
    return makeString("CREATE TABLE ", tableName, " (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, autoInc INTEGER NOT NULL ON CONFLICT FAIL)");
}

static ASCIILiteral indexInfoTableSchema()
{
    return TABLE_SCHEMA_PREFIX "IndexInfo" INDEX_INFO_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral indexInfoTableSchemaTemp()
{
    return TABLE_SCHEMA_PREFIX "_Temp_IndexInfo" INDEX_INFO_TABLE_SCHEMA_SUFFIX;
}

SQLiteIDBBackingStore::SQLiteIDBBackingStore(const IDBDatabaseIdentifier& identifier, const String& databaseDirectory)
    : m_identifier(identifier)
    , m_databaseDirectory(databaseDirectory)
{
}

SQLiteIDBBackingStore::~SQLiteIDBBackingStore()
{
    if (m_sqliteDB)
        closeSQLiteDB();
}

static IDBError createOrMigrateRecordsTableIfNecessary(SQLiteDatabase& database)
{
    String tableStatement = database.tableSQL("Records"_s);
    if (tableStatement.isEmpty()) {
        if (!database.executeCommand(v3RecordsTableSchema()))
            return IDBError { ExceptionCode::UnknownError, makeString("Error creating Records table (", database.lastError(), ") - ", database.lastErrorMsg()) };

        return IDBError { };
    }

    // If the schema in the backing store is the current schema, we're done.
    if (tableStatement == v3RecordsTableSchema() || tableStatement == v3RecordsTableSchemaAlternate())
        return IDBError { };

    // If the record table is not the current schema then it must be one of the previous schemas.
    // If it is not then the database is in an unrecoverable state and this should be considered a fatal error.
    if (tableStatement != v1RecordsTableSchema() && tableStatement != v1RecordsTableSchemaAlternate()
        && tableStatement != v2RecordsTableSchema() && tableStatement != v2RecordsTableSchemaAlternate())
        RELEASE_ASSERT_NOT_REACHED();

    SQLiteTransaction transaction(database);
    transaction.begin();

    // Create a temporary table with the correct schema and migrate all existing content over.
    if (!database.executeCommand(v3RecordsTableSchemaTemp()))
        return IDBError { ExceptionCode::UnknownError, makeString("Error creating temporary Records table (", database.lastError(), ") - ", database.lastErrorMsg()) };

    if (!database.executeCommand("INSERT INTO _Temp_Records (objectStoreID, key, value) SELECT objectStoreID, CAST(key AS TEXT), value FROM Records"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error migrating Records table (", database.lastError(), ") - ", database.lastErrorMsg()) };

    if (!database.executeCommand("DROP TABLE Records"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error dropping Records table (", database.lastError(), ") - ", database.lastErrorMsg()) };

    if (!database.executeCommand("ALTER TABLE _Temp_Records RENAME TO Records"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error renaming temporary Records table (", database.lastError(), ") - ", database.lastErrorMsg()) };

    transaction.commit();

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::ensureValidBlobTables()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String recordsTableStatement = m_sqliteDB->tableSQL("BlobRecords"_s);
    if (recordsTableStatement.isEmpty()) {
        if (!m_sqliteDB->executeCommand(blobRecordsTableSchema()))
            return IDBError { ExceptionCode::UnknownError, makeString("Error creating BlobRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

        recordsTableStatement = blobRecordsTableSchema();
    }

    RELEASE_ASSERT(recordsTableStatement == blobRecordsTableSchema() || recordsTableStatement == blobRecordsTableSchemaAlternate());

    String filesTableStatement = m_sqliteDB->tableSQL("BlobFiles"_s);
    if (filesTableStatement.isEmpty()) {
        if (!m_sqliteDB->executeCommand(blobFilesTableSchema()))
            return IDBError { ExceptionCode::UnknownError, makeString("Error creating BlobFiles table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

        filesTableStatement = blobFilesTableSchema();
    }

    RELEASE_ASSERT(filesTableStatement == blobFilesTableSchema() || filesTableStatement == blobFilesTableSchemaAlternate());
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::ensureValidRecordsTable()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    IDBError error = createOrMigrateRecordsTableIfNecessary(*m_sqliteDB);
    if (!error.isNull())
        return error;

    // Whether the updated records table already existed or if it was just created and the data migrated over,
    // make sure the uniqueness index exists.
    if (!m_sqliteDB->executeCommand("CREATE UNIQUE INDEX IF NOT EXISTS RecordsIndex ON Records (objectStoreID, key);"_s))
        error = IDBError { ExceptionCode::UnknownError, makeString("Error creating RecordsIndex on Records table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    return error;
}

IDBError SQLiteIDBBackingStore::ensureValidIndexRecordsTable()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String tableStatement = m_sqliteDB->tableSQL("IndexRecords"_s);
    if (tableStatement.isEmpty()) {
        if (!m_sqliteDB->executeCommand(v3IndexRecordsTableSchema()))
            return IDBError { ExceptionCode::UnknownError, makeString("Error creating IndexRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

        return IDBError { };
    }

    // If the schema in the backing store is the current schema, we're done.
    if (tableStatement == v3IndexRecordsTableSchema() || tableStatement == v3IndexRecordsTableSchemaAlternate())
        return IDBError { };

    RELEASE_ASSERT(tableStatement == v1IndexRecordsTableSchema() || tableStatement == v1IndexRecordsTableSchemaAlternate() || tableStatement == v2IndexRecordsTableSchema() || tableStatement == v2IndexRecordsTableSchemaAlternate());

    SQLiteTransaction transaction(*m_sqliteDB);
    transaction.begin();

    // Create a temporary table with the correct schema and migrate all existing content over.
    if (!m_sqliteDB->executeCommand(v3IndexRecordsTableSchemaTemp()))
        return IDBError { ExceptionCode::UnknownError, makeString("Error creating temporary IndexRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    if (!m_sqliteDB->executeCommand("INSERT INTO _Temp_IndexRecords SELECT IndexRecords.indexID, IndexRecords.objectStoreID, IndexRecords.key, IndexRecords.value, Records.rowid FROM IndexRecords INNER JOIN Records ON Records.key = IndexRecords.value AND Records.objectStoreID = IndexRecords.objectStoreID"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error migrating IndexRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    if (!m_sqliteDB->executeCommand("DROP TABLE IndexRecords"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error dropping IndexRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    if (!m_sqliteDB->executeCommand("ALTER TABLE _Temp_IndexRecords RENAME TO IndexRecords"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error renaming temporary IndexRecords table (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    transaction.commit();

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::ensureValidIndexRecordsIndex()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String indexStatement = m_sqliteDB->indexSQL("IndexRecordsIndex"_s);
    if (indexStatement == IndexRecordsIndexSchema)
        return IDBError { };
    
    if (!m_sqliteDB->executeCommand("DROP INDEX IF EXISTS IndexRecordsIndex"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error dropping IndexRecordsIndex index (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    if (!m_sqliteDB->executeCommand(IndexRecordsIndexSchema))
        return IDBError { ExceptionCode::UnknownError, makeString("Error creating IndexRecordsIndex index (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::ensureValidIndexRecordsRecordIndex()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String indexStatement = m_sqliteDB->indexSQL("IndexRecordsRecordIndex"_s);
    if (indexStatement == v1IndexRecordsRecordIndexSchema)
        return IDBError { };
    
    if (!m_sqliteDB->executeCommand("DROP INDEX IF EXISTS IndexRecordsRecordIndex"_s))
        return IDBError { ExceptionCode::UnknownError, makeString("Error dropping IndexRecordsRecordIndex index (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    if (!m_sqliteDB->executeCommand(v1IndexRecordsRecordIndexSchema))
        return IDBError { ExceptionCode::UnknownError, makeString("Error creating IndexRecordsRecordIndex index (", m_sqliteDB->lastError(), ") - ", m_sqliteDB->lastErrorMsg()) };

    return IDBError { };
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::createAndPopulateInitialDatabaseInfo()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    if (!m_sqliteDB->executeCommand("CREATE TABLE IDBDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL);"_s)) {
        LOG_ERROR("Could not create IDBDatabaseInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand(v2ObjectStoreInfoSchema)) {
        LOG_ERROR("Could not create ObjectStoreInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand(indexInfoTableSchema())) {
        LOG_ERROR("Could not create IndexInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE KeyGenerators (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, currentKey INTEGER NOT NULL ON CONFLICT FAIL);"_s)) {
        LOG_ERROR("Could not create KeyGenerators table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
        return nullptr;
    }

    {
        auto sql = m_sqliteDB->prepareStatement("INSERT INTO IDBDatabaseInfo VALUES ('MetadataVersion', ?);"_s);
        if (!sql
            || sql->bindInt(1, currentMetadataVersion) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database metadata version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            closeSQLiteDB();
            return nullptr;
        }
    }
    {
        auto sql = m_sqliteDB->prepareStatement("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseName', ?);"_s);
        if (!sql
            || sql->bindText(1, m_identifier.databaseName()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not insert database name into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            closeSQLiteDB();
            return nullptr;
        }
    }
    {
        // Database versions are defined to be a uin64_t in the spec but sqlite3 doesn't support native binding of unsigned integers.
        // Therefore we'll store the version as a String.
        auto sql = m_sqliteDB->prepareStatement("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseVersion', ?);"_s);
        if (!sql
            || sql->bindText(1, String::number(0)) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
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
    return makeUnique<IDBDatabaseInfo>(m_identifier.databaseName(), 0, 0);
}

std::optional<IsSchemaUpgraded> SQLiteIDBBackingStore::ensureValidObjectStoreInfoTable()
{
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    String tableStatement = m_sqliteDB->tableSQL("ObjectStoreInfo"_s);
    if (tableStatement.isEmpty())
        return std::nullopt;
    
    if (tableStatement == v2ObjectStoreInfoSchema || tableStatement == createV2ObjectStoreInfoSchema(objectStoreInfoTableNameAlternate))
        return { IsSchemaUpgraded::No };
    
    RELEASE_ASSERT(tableStatement == createV1ObjectStoreInfoSchema(objectStoreInfoTableName) || tableStatement == createV1ObjectStoreInfoSchema(objectStoreInfoTableNameAlternate));

    // Drop column maxIndexID from table.
    SQLiteTransaction transaction(*m_sqliteDB);
    transaction.begin();

    if (!m_sqliteDB->executeCommandSlow(createV2ObjectStoreInfoSchema("_Temp_ObjectStoreInfo"_s))) {
        LOG_ERROR("Could not create temporary ObjectStoreInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return std::nullopt;
    }

    if (!m_sqliteDB->executeCommand("INSERT INTO _Temp_ObjectStoreInfo (id, name, keyPath, autoInc) SELECT id, name, keyPath, autoInc FROM ObjectStoreInfo"_s)) {
        LOG_ERROR("Could not migrate existing ObjectStoreInfo content (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return std::nullopt;
    }

    if (!m_sqliteDB->executeCommand("DROP TABLE ObjectStoreInfo"_s)) {
        LOG_ERROR("Could not drop existing ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return std::nullopt;
    }

    if (!m_sqliteDB->executeCommand("ALTER TABLE _Temp_ObjectStoreInfo RENAME TO ObjectStoreInfo"_s)) {
        LOG_ERROR("Could not rename temporary ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return std::nullopt;
    }

    transaction.commit();

    return { IsSchemaUpgraded::Yes };
}

bool SQLiteIDBBackingStore::migrateIndexInfoTableForIDUpdate(const HashMap<std::pair<uint64_t, uint64_t>, uint64_t>& indexIDMap)
{
    SQLiteDatabase& database = *m_sqliteDB;
    SQLiteTransaction transaction(database);
    transaction.begin();

    if (!database.executeCommand(indexInfoTableSchemaTemp())) {
        LOG_ERROR("Error creating _Temp_IndexInfo table in database (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    {
        auto statement = database.prepareStatement("SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"_s);
        if (!statement) {
            LOG_ERROR("Error preparing statement to fetch records from IndexInfo table (%i) - %s", database.lastError(), database.lastErrorMsg());
            return false;
        }

        int result = statement->step();
        while (result == SQLITE_ROW) {
            uint64_t id = statement->columnInt64(0);
            String name = statement->columnText(1);
            uint64_t objectStoreID = statement->columnInt64(2);
            uint64_t newID = indexIDMap.get({ objectStoreID, id });
            auto keyPathBufferSpan = statement->columnBlobAsSpan(3);
            bool unique = statement->columnInt(4);
            bool multiEntry = statement->columnInt(5);

            auto sql = cachedStatement(SQL::CreateTempIndexInfo, "INSERT INTO _Temp_IndexInfo VALUES (?, ?, ?, ?, ?, ?);"_s);
            if (!sql
                || sql->bindInt64(1, newID) != SQLITE_OK
                || sql->bindText(2, name) != SQLITE_OK
                || sql->bindInt64(3, objectStoreID) != SQLITE_OK
                || sql->bindBlob(4, keyPathBufferSpan) != SQLITE_OK
                || sql->bindInt(5, unique) != SQLITE_OK
                || sql->bindInt(6, multiEntry) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Error adding index '%s' to _Temp_IndexInfo table (%i) - %s", name.utf8().data(), database.lastError(), database.lastErrorMsg());
                return false;
            }

            result = statement->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching indices from IndexInfo table (%i) - %s",  database.lastError(), database.lastErrorMsg());
            return false;
        }
    }

    if (!database.executeCommand("DROP TABLE IndexInfo"_s)) {
        LOG_ERROR("Error dropping existing IndexInfo table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("ALTER TABLE _Temp_IndexInfo RENAME TO IndexInfo"_s)) {
        LOG_ERROR("Error renaming _Temp_IndexInfo table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    transaction.commit();
    return true;
}

bool SQLiteIDBBackingStore::migrateIndexRecordsTableForIDUpdate(const HashMap<std::pair<uint64_t, uint64_t>, uint64_t>& indexIDMap)
{
    SQLiteDatabase& database = *m_sqliteDB;
    SQLiteTransaction transaction(database);
    transaction.begin();

    if (!database.executeCommand(v3IndexRecordsTableSchemaTemp())) {
        LOG_ERROR("Error creating _Temp_IndexRecords table in database (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    {
        auto statement = database.prepareStatement("SELECT indexID, objectStoreID, key, value, objectStoreRecordID FROM IndexRecords;"_s);
        if (!statement) {
            LOG_ERROR("Error preparing statement to fetch records from the IndexRecords table (%i) - %s", database.lastError(), database.lastErrorMsg());
            return false;
        }

        int result = statement->step();
        while (result == SQLITE_ROW) {
            uint64_t id = statement->columnInt64(0);
            uint64_t objectStoreID = statement->columnInt64(1);
            uint64_t newID = indexIDMap.get({ objectStoreID, id });
            auto keyBufferSpan = statement->columnBlobAsSpan(2);
            auto valueBufferSpan = statement->columnBlobAsSpan(3);
            uint64_t recordID = statement->columnInt64(4);

            auto sql = cachedStatement(SQL::PutTempIndexRecord, "INSERT INTO _Temp_IndexRecords VALUES (?, ?, CAST(? AS TEXT), CAST(? AS TEXT), ?);"_s);
            if (!sql
                || sql->bindInt64(1, newID) != SQLITE_OK
                || sql->bindInt64(2, objectStoreID) != SQLITE_OK
                || sql->bindBlob(3, keyBufferSpan) != SQLITE_OK
                || sql->bindBlob(4, valueBufferSpan) != SQLITE_OK
                || sql->bindInt64(5, recordID) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Error adding index record to _Temp_IndexRecords table (%i) - %s", database.lastError(), database.lastErrorMsg());
                return false;
            }

            result = statement->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching index record from database on disk (%i) - %s", database.lastError(), database.lastErrorMsg());
            return false;
        }
    }

    if (!database.executeCommand("DROP TABLE IndexRecords"_s)) {
        LOG_ERROR("Error dropping existing IndexRecords table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    if (!database.executeCommand("ALTER TABLE _Temp_IndexRecords RENAME TO IndexRecords"_s)) {
        LOG_ERROR("Error renaming temporary IndexRecords table (%i) - %s", database.lastError(), database.lastErrorMsg());
        return false;
    }

    transaction.commit();
    return true;
}

bool SQLiteIDBBackingStore::removeExistingIndex(uint64_t indexID)
{
    SQLiteTransaction transaction(*m_sqliteDB);
    transaction.begin();

    {
        auto sql = cachedStatement(SQL::RemoveIndexInfo, "DELETE FROM IndexInfo WHERE id = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Unable to remove existing index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        auto sql = cachedStatement(SQL::DeleteIndexRecords, "DELETE FROM IndexRecords WHERE indexID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Unable to remove existing index from IndexRecords table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    transaction.commit();
    return true;
}

bool SQLiteIDBBackingStore::addExistingIndex(IDBObjectStoreInfo& objectStoreInfo, const IDBIndexInfo& info)
{
    SQLiteTransaction transaction(*m_sqliteDB);
    transaction.begin();

    auto keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database");
        return false;
    }

    {
        auto sql = cachedStatement(SQL::CreateIndexInfo, "INSERT INTO IndexInfo VALUES (?, ?, ?, ?, ?, ?);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindText(2, info.name()) != SQLITE_OK
            || sql->bindInt64(3, info.objectStoreIdentifier()) != SQLITE_OK
            || sql->bindBlob(4, keyPathBlob->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindInt(5, info.unique()) != SQLITE_OK
            || sql->bindInt(6, info.multiEntry()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not add index '%s' to IndexInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        auto sql = cachedStatement(SQL::GetObjectStoreRecords, "SELECT key, value, recordID FROM Records WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, info.objectStoreIdentifier()) != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement or bind values (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        int result = sql->step();
        while (result == SQLITE_ROW) {
            auto keyBufferSpan = sql->columnBlobAsSpan(0);
            IDBKeyData keyData;
            if (!deserializeIDBKeyData(keyBufferSpan.data(), keyBufferSpan.size(), keyData)) {
                LOG_ERROR("Unable to deserialize key data from database while getting all records");
                return false;
            }

            auto value = ThreadSafeDataBuffer::create(sql->columnBlob(1));

            uint64_t recordID = sql->columnInt64(2);
            IDBError error = updateOneIndexForAddRecord(objectStoreInfo, info, keyData, value, recordID);
            if (!error.isNull())
                return false;

            result = sql->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching object records from Records table on disk");
            return false;
        }
    }

    transaction.commit();
    return true;
}

bool SQLiteIDBBackingStore::handleDuplicateIndexIDs(const HashMap<uint64_t, Vector<IDBIndexInfo>>& indexInfoMap, IDBDatabaseInfo& databaseInfo)
{
    for (auto& iter : indexInfoMap) {
        if (iter.value.size() == 1)
            continue;

        if (!removeExistingIndex(iter.key))
            return false;

        for (auto info : iter.value) {
            auto objectStoreInfo = databaseInfo.infoForExistingObjectStore(info.objectStoreIdentifier());
            ASSERT(objectStoreInfo);
            objectStoreInfo->deleteIndex(info.identifier());

            // A new index with the same name may already be added.
            if (objectStoreInfo->hasIndex(info.name()))
                continue;

            info.setIdentifier(databaseInfo.generateNextIndexID());
            if (!addExistingIndex(*objectStoreInfo, info))
                return false;

            objectStoreInfo->addExistingIndex(info);
        }
    }

    return true;
}

std::unique_ptr<IDBDatabaseInfo> SQLiteIDBBackingStore::extractExistingDatabaseInfo()
{
    ASSERT(m_sqliteDB);

    if (!m_sqliteDB->tableExists("IDBDatabaseInfo"_s))
        return nullptr;

    String databaseName;
    {
        auto sql = m_sqliteDB->prepareStatement("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseName';"_s);
        databaseName = sql->columnText(0);
        if (databaseName != m_identifier.databaseName()) {
            LOG_ERROR("Database name in the info database ('%s') does not match the expected name ('%s')", databaseName.utf8().data(), m_identifier.databaseName().utf8().data());
            return nullptr;
        }
    }
    uint64_t databaseVersion;
    {
        auto sql = m_sqliteDB->prepareStatement("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';"_s);
        String stringVersion = sql->columnText(0);
        auto parsedVersion = parseInteger<uint64_t>(stringVersion);
        if (!parsedVersion) {
            LOG_ERROR("Database version on disk ('%s') does not cleanly convert to an unsigned 64-bit integer version", stringVersion.utf8().data());
            return nullptr;
        }
        databaseVersion = *parsedVersion;
    }

    auto databaseInfo = makeUnique<IDBDatabaseInfo>(databaseName, databaseVersion, 0);

    auto result = ensureValidObjectStoreInfoTable();
    if (!result)
        return nullptr;

    bool shouldUpdateIndexID = (result.value() == IsSchemaUpgraded::Yes);

    {
        auto sql = m_sqliteDB->prepareStatement("SELECT id, name, keyPath, autoInc FROM ObjectStoreInfo;"_s);
        if (!sql)
            return nullptr;

        int result = sql->step();
        while (result == SQLITE_ROW) {
            uint64_t objectStoreID = sql->columnInt64(0);
            String objectStoreName = sql->columnText(1);
            auto keyPathBufferSpan = sql->columnBlobAsSpan(2);

            std::optional<IDBKeyPath> objectStoreKeyPath;
            if (!deserializeIDBKeyPath(keyPathBufferSpan.data(), keyPathBufferSpan.size(), objectStoreKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }

            bool autoIncrement = sql->columnInt(3);

            databaseInfo->addExistingObjectStore({ objectStoreID, objectStoreName, WTFMove(objectStoreKeyPath), autoIncrement });

            result = sql->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching object store info from database on disk");
            return nullptr;
        }
    }

    uint64_t maxIndexID = 0;
    HashMap<std::pair<uint64_t, uint64_t>, uint64_t> indexIDMap;
    HashMap<uint64_t, Vector<IDBIndexInfo>> indexInfoMap;
    {
        auto sql = m_sqliteDB->prepareStatement("SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"_s);
        if (!sql) {
            LOG_ERROR("Unable to prepare statement to fetch records from the IndexInfo table.");
            return nullptr;
        }

        int result = sql->step();
        while (result == SQLITE_ROW) {
            uint64_t indexID = sql->columnInt64(0);
            String indexName = sql->columnText(1);
            uint64_t objectStoreID = sql->columnInt64(2);
            auto keyPathBufferSpan = sql->columnBlobAsSpan(3);

            std::optional<IDBKeyPath> indexKeyPath;
            if (!deserializeIDBKeyPath(keyPathBufferSpan.data(), keyPathBufferSpan.size(), indexKeyPath)) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }
            if (!indexKeyPath) {
                LOG_ERROR("Unable to extract key path from database");
                return nullptr;
            }

            bool unique = sql->columnInt(4);
            bool multiEntry = sql->columnInt(5);

            auto objectStore = databaseInfo->infoForExistingObjectStore(objectStoreID);
            if (!objectStore) {
                LOG_ERROR("Found index referring to a non-existant object store");
                return nullptr;
            }

            if (shouldUpdateIndexID) {
                indexIDMap.set({ objectStoreID, indexID }, ++maxIndexID);
                indexID = maxIndexID;
            }

            auto indexInfo = IDBIndexInfo { indexID, objectStoreID, indexName, WTFMove(indexKeyPath.value()), unique, multiEntry };
            if (!shouldUpdateIndexID) {
                auto& indexInfos = indexInfoMap.ensure(indexID, []() -> Vector<IDBIndexInfo> {
                    return { };
                }).iterator->value;
                indexInfos.append(indexInfo);
            }

            objectStore->addExistingIndex(indexInfo);
            maxIndexID = maxIndexID < indexID ? indexID : maxIndexID;

            result = sql->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching index info from database on disk");
            return nullptr;
        }
        databaseInfo->setMaxIndexID(maxIndexID);
    }

    if (shouldUpdateIndexID) {
        if (!migrateIndexInfoTableForIDUpdate(indexIDMap) || !migrateIndexRecordsTableForIDUpdate(indexIDMap))
            return nullptr;
    } else {
        if (!handleDuplicateIndexIDs(indexInfoMap, *databaseInfo))
            return nullptr;
    }

    return databaseInfo;
}

String SQLiteIDBBackingStore::encodeDatabaseName(const String& databaseName)
{
    ASSERT(!databaseName.isNull());
    if (databaseName.isEmpty())
        return "%00"_s;

    return makeStringByReplacingAll(FileSystem::encodeForFileName(databaseName), '.', "%2E"_s);
}

String SQLiteIDBBackingStore::decodeDatabaseName(const String& encodedName)
{
    if (encodedName == "%00"_s)
        return emptyString();

    return FileSystem::decodeFromFilename(makeStringByReplacingAll(encodedName, "%2E"_s, "."_s));
}

String SQLiteIDBBackingStore::fullDatabasePathForDirectory(const String& fullDatabaseDirectory)
{
    return FileSystem::pathByAppendingComponent(fullDatabaseDirectory, "IndexedDB.sqlite3"_s);
}

String SQLiteIDBBackingStore::fullDatabasePath() const
{
    return fullDatabasePathForDirectory(m_databaseDirectory);
}

std::optional<IDBDatabaseNameAndVersion> SQLiteIDBBackingStore::databaseNameAndVersionFromFile(const String& databasePath)
{
    SQLiteDatabase database;
    if (!database.open(databasePath)) {
        LOG_ERROR("Failed to open SQLite database at path '%s' when getting database name", databasePath.utf8().data());
        return std::nullopt;
    }
    if (!database.tableExists("IDBDatabaseInfo"_s)) {
        LOG_ERROR("Could not find IDBDatabaseInfo table and get database name(%i) - %s", database.lastError(), database.lastErrorMsg());
        return std::nullopt;
    }

    auto namesql = database.prepareStatement("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseName';"_s);
    if (!namesql) {
        LOG_ERROR("Could not prepare statement to get database name(%i) - %s", database.lastError(), database.lastErrorMsg());
        return std::nullopt;
    }
    auto databaseName = namesql->columnText(0);

    auto versql = database.prepareStatement("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';"_s);
    String stringVersion = versql ? versql->columnText(0) : String();
    auto databaseVersion = parseInteger<uint64_t>(stringVersion);
    if (!databaseVersion) {
        LOG_ERROR("Database version on disk ('%s') does not cleanly convert to an unsigned 64-bit integer version", stringVersion.utf8().data());
        return std::nullopt;
    }

    return IDBDatabaseNameAndVersion { databaseName, *databaseVersion };
}

IDBError SQLiteIDBBackingStore::getOrEstablishDatabaseInfo(IDBDatabaseInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getOrEstablishDatabaseInfo - database %s", m_identifier.databaseName().utf8().data());

    if (m_databaseInfo) {
        info = *m_databaseInfo;
        return IDBError { };
    }

    String databasePath = fullDatabasePath();
    FileSystem::makeAllDirectories(FileSystem::parentPath(databasePath));
    m_sqliteDB = makeUnique<SQLiteDatabase>();
    if (!m_sqliteDB->open(databasePath, SQLiteDatabase::OpenMode::ReadWriteCreate, SQLiteDatabase::OpenOptions::CanSuspendWhileLocked)) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - SQLiteIDBBackingStore::getOrEstablishDatabaseInfo: Failed to open database at path '%" PUBLIC_LOG_STRING "' (%d) - %" PUBLIC_LOG_STRING, this, databasePath.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        closeSQLiteDB();
    }

    if (!m_sqliteDB)
        return IDBError { ExceptionCode::UnknownError, "Unable to open database file on disk"_s };

    m_sqliteDB->disableThreadingChecks();
    m_sqliteDB->enableAutomaticWALTruncation();

    m_sqliteDB->setCollationFunction("IDBKEY"_s, [](int aLength, const void* a, int bLength, const void* b) {
        return idbKeyCollate(aLength, a, bLength, b);
    });

    IDBError error = ensureValidRecordsTable();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }

    error = ensureValidIndexRecordsTable();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }

    error = ensureValidIndexRecordsIndex();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }
    
    error = ensureValidIndexRecordsRecordIndex();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }
    
    error = ensureValidBlobTables();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }

    auto databaseInfo = extractExistingDatabaseInfo();
    if (!databaseInfo)
        databaseInfo = createAndPopulateInitialDatabaseInfo();

    if (!databaseInfo) {
        LOG_ERROR("Unable to establish IDB database at path '%s'", databasePath.utf8().data());
        closeSQLiteDB();
        return IDBError { ExceptionCode::UnknownError, "Unable to establish IDB database file"_s };
    }

    m_databaseInfo = WTFMove(databaseInfo);
    info = *m_databaseInfo;
    return IDBError { };
}

uint64_t SQLiteIDBBackingStore::databaseVersion()
{
    if (m_databaseInfo)
        return m_databaseInfo->version();

    String dbFilename = fullDatabasePath();
    if (!FileSystem::fileExists(dbFilename))
        return 0;

    auto databaseNameAndVersion = databaseNameAndVersionFromFile(dbFilename);
    return databaseNameAndVersion ? databaseNameAndVersion->version : 0;
}

uint64_t SQLiteIDBBackingStore::databasesSizeForDirectory(const String& directory)
{
    uint64_t diskUsage = 0;
    for (auto& dbDirectoryName : FileSystem::listDirectory(directory)) {
        auto dbDirectoryPath = FileSystem::pathByAppendingComponent(directory, dbDirectoryName);
        for (auto& fileName : FileSystem::listDirectory(dbDirectoryPath)) {
            if (fileName.endsWith(".sqlite3"_s))
                diskUsage += SQLiteFileSystem::databaseFileSize(FileSystem::pathByAppendingComponent(dbDirectoryPath, fileName));
        }
    }
    return diskUsage;
}

IDBError SQLiteIDBBackingStore::beginTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::beginTransaction - %s", info.identifier().loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());
    ASSERT(m_databaseInfo);

    auto addResult = m_transactions.add(info.identifier(), nullptr);
    if (!addResult.isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return IDBError { ExceptionCode::UnknownError, "Attempt to establish transaction identifier that already exists"_s };
    }

    addResult.iterator->value = makeUnique<SQLiteIDBTransaction>(*this, info);

    auto error = addResult.iterator->value->begin(*m_sqliteDB);
    if (error.isNull() && info.mode() == IDBTransactionMode::Versionchange) {
        m_originalDatabaseInfoBeforeVersionChange = makeUnique<IDBDatabaseInfo>(*m_databaseInfo);

        auto sql = m_sqliteDB->prepareStatement("UPDATE IDBDatabaseInfo SET value = ? where key = 'DatabaseVersion';"_s);
        if (!sql
            || sql->bindText(1, String::number(info.newVersion())) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            error = IDBError { ExceptionCode::UnknownError, "Failed to store new database version in database"_s };
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
        return IDBError { ExceptionCode::UnknownError, "Attempt to abort a transaction that hasn't been established"_s };
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
        return IDBError { ExceptionCode::UnknownError, "Attempt to commit a transaction that hasn't been established"_s };
    }

    auto error = transaction->commit();
    if (!error.isNull()) {
        if (transaction->mode() == IDBTransactionMode::Versionchange) {
            ASSERT(m_originalDatabaseInfoBeforeVersionChange);
            m_databaseInfo = WTFMove(m_originalDatabaseInfoBeforeVersionChange);
        }
    } else {
        m_originalDatabaseInfoBeforeVersionChange = nullptr;
        if (transaction->durability() == IDBTransactionDurability::Strict)
            m_sqliteDB->checkpoint(SQLiteDatabase::CheckpointMode::Full);
    }

    return error;
}

IDBError SQLiteIDBBackingStore::createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::createObjectStore - adding OS %s with ID %" PRIu64, info.name().utf8().data(), info.identifier());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to create an object store without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to create an object store in a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to create an object store in a non-version-change transaction"_s };
    }

    auto keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database for new object store");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKeyPath to save in database for new object store"_s };
    }

    {
        auto sql = cachedStatement(SQL::CreateObjectStoreInfo, "INSERT INTO ObjectStoreInfo VALUES (?, ?, ?, ?);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindText(2, info.name()) != SQLITE_OK
            || sql->bindBlob(3, keyPathBlob->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindInt(4, info.autoIncrement()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not add object store '%s' to ObjectStoreInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not create object store"_s };
        }
    }

    {
        auto sql = cachedStatement(SQL::CreateObjectStoreKeyGenerator, "INSERT INTO KeyGenerators VALUES (?, 0);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not seed initial key generator value for ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not seed initial key generator value for object store"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete an object store without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to delete an object store in a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete an object store in a non-version-change transaction"_s };
    }

    // Delete the ObjectStore record
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreInfo, "DELETE FROM ObjectStoreInfo WHERE id = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store id %" PRIi64 " from ObjectStoreInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete object store"_s };
        }
    }

    // Delete the ObjectStore's key generator record if there is one.
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreKeyGenerator, "DELETE FROM KeyGenerators WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete key generator for deleted object store"_s };
        }
    }

    // Delete all associated records
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreRecords, "DELETE FROM Records WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records for object store %" PRIi64 " (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete records for deleted object store"_s };
        }
    }

    // Delete all associated Indexes
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreIndexInfo, "DELETE FROM IndexInfo WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete IDBIndex for deleted object store"_s };
        }
    }

    // Delete all associated Index records
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreIndexRecords, "DELETE FROM IndexRecords WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete IDBIndex records for deleted object store"_s };
        }
    }

    // Delete all unused Blob URL records.
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreBlobRecords, "DELETE FROM BlobRecords WHERE objectStoreRow NOT IN (SELECT recordID FROM Records)"_s);
        if (!sql
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete Blob URL records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not delete stored blob records for deleted object store"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to rename an object store without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to rename an object store in a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to rename an object store in a non-version-change transaction"_s };
    }

    {
        auto sql = cachedStatement(SQL::RenameObjectStore, "UPDATE ObjectStoreInfo SET name = ? WHERE id = ?;"_s);
        if (!sql
            || sql->bindText(1, newName) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not update name for object store id %" PRIi64 " in ObjectStoreInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not rename object store"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to clear an object store without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to clear an object store in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to clear an object store in a read-only transaction"_s };
    }

    {
        auto sql = cachedStatement(SQL::ClearObjectStoreRecords, "DELETE FROM Records WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not clear records from object store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to clear object store"_s };
        }
    }

    {
        auto sql = cachedStatement(SQL::ClearObjectStoreIndexRecords, "DELETE FROM IndexRecords WHERE objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records from index record store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to delete index records while clearing object store"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to create an index without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to create an index in a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to create an index in a non-version-change transaction"_s };
    }

    auto keyPathBlob = serializeIDBKeyPath(info.keyPath());
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKeyPath to create index in database"_s };
    }

    {
        auto sql = cachedStatement(SQL::CreateIndexInfo, "INSERT INTO IndexInfo VALUES (?, ?, ?, ?, ?, ?);"_s);
        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindText(2, info.name()) != SQLITE_OK
            || sql->bindInt64(3, info.objectStoreIdentifier()) != SQLITE_OK
            || sql->bindBlob(4, keyPathBlob->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindInt(5, info.unique()) != SQLITE_OK
            || sql->bindInt(6, info.multiEntry()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not add index '%s' to IndexInfo table (%i) - %s", info.name().utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to create index in database"_s };
        }
    }

    // Write index records for any records that already exist in this object store.

    auto cursor = transaction->maybeOpenBackingStoreCursor(info.objectStoreIdentifier(), 0, IDBKeyRangeData::allKeys());

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to populate indexes in database");
        return IDBError { ExceptionCode::UnknownError, "Unable to populate indexes in database"_s };
    }

    while (!cursor->currentKey().isNull()) {
        auto& key = cursor->currentKey();
        auto value = cursor->currentValue();
        ThreadSafeDataBuffer valueBuffer = value.data();

        ASSERT(cursor->currentRecordRowID());

        auto* objectStoreInfo = infoForObjectStore(info.objectStoreIdentifier());
        ASSERT(objectStoreInfo);
        IDBError error = updateOneIndexForAddRecord(*objectStoreInfo, info, key, valueBuffer, cursor->currentRecordRowID());
        if (!error.isNull()) {
            auto sql = cachedStatement(SQL::DeleteIndexInfo, "DELETE FROM IndexInfo WHERE id = ? AND objectStoreID = ?;"_s);
            if (!sql
                || sql->bindInt64(1, info.identifier()) != SQLITE_OK
                || sql->bindInt64(2, info.objectStoreIdentifier()) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Index creation failed due to uniqueness constraint failure, but there was an error deleting the Index record from the database");
                return IDBError { ExceptionCode::UnknownError, "Index creation failed due to uniqueness constraint failure, but there was an error deleting the Index record from the database"_s };
            }

            return error;
        }

        if (!cursor->advance(1)) {
            LOG_ERROR("Error advancing cursor while indexing existing records for new index.");
            return IDBError { ExceptionCode::UnknownError, "Error advancing cursor while indexing existing records for new index"_s };
        }
    }

    ASSERT(m_databaseInfo);
    if (!m_databaseInfo) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - SQLiteIDBBackingStore::clearObjectStore: m_databaseInfo is null", this);
        return IDBError { ExceptionCode::UnknownError, "Database info is invalid"_s };
    }

    auto* objectStore = m_databaseInfo->infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->addExistingIndex(info);
    m_databaseInfo->setMaxIndexID(info.identifier());

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedHasIndexRecord(const IDBIndexInfo& info, const IDBKeyData& indexKey, bool& hasRecord)
{
    hasRecord = false;

    auto indexKeyBuffer = serializeIDBKeyData(indexKey);
    if (!indexKeyBuffer) {
        LOG_ERROR("Unable to serialize index key to be stored in the database");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKey to check for index record in database"_s };
    }

    auto sql = cachedStatement(SQL::HasIndexRecord, "SELECT rowid FROM IndexRecords WHERE indexID = ? AND key = CAST(? AS TEXT);"_s);
    if (!sql
        || sql->bindInt64(1, info.identifier()) != SQLITE_OK
        || sql->bindBlob(2, indexKeyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
        LOG_ERROR("Error checking for index record in database");
        return IDBError { ExceptionCode::UnknownError, "Error checking for index record in database"_s };
    }

    int sqlResult = sql->step();
    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE)
        return IDBError { };

    if (sqlResult != SQLITE_ROW) {
        // There was an error fetching the record from the database.
        LOG_ERROR("Could not check if key exists in index (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Error checking for existence of IDBKey in index"_s };
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
                return IDBError(ExceptionCode::ConstraintError);
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

    auto indexKeyBuffer = serializeIDBKeyData(indexKey);
    if (!indexKeyBuffer) {
        LOG_ERROR("Unable to serialize index key to be stored in the database");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize index key to be stored in the database"_s };
    }

    auto valueBuffer = serializeIDBKeyData(keyValue);
    if (!valueBuffer) {
        LOG_ERROR("Unable to serialize the value to be stored in the database");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize value to be stored in the database"_s };
    }

    {
        auto sql = cachedStatement(SQL::PutIndexRecord, "INSERT INTO IndexRecords VALUES (?, ?, CAST(? AS TEXT), CAST(? AS TEXT), ?);"_s);
        if (!sql
            || sql->bindInt64(1, indexID) != SQLITE_OK
            || sql->bindInt64(2, objectStoreID) != SQLITE_OK
            || sql->bindBlob(3, indexKeyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindBlob(4, valueBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindInt64(5, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not put index record for index %" PRIi64 " in object store %" PRIi64 " in Records table (%i) - %s", indexID, objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error putting index record into database"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete index without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to delete index during a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete index during a non-version-change transaction"_s };
    }

    {
        auto sql = cachedStatement(SQL::DeleteIndexInfo, "DELETE FROM IndexInfo WHERE id = ? AND objectStoreID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexIdentifier) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index id %" PRIi64 " from IndexInfo table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error deleting index from database"_s };
        }
    }

    {
        auto sql = cachedStatement(SQL::DeleteIndexRecords, "DELETE FROM IndexRecords WHERE indexID = ?;"_s);
        if (!sql
            || sql->bindInt64(1, indexIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records for index id %" PRIi64 " from IndexRecords table (%i) - %s", indexIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error deleting index records from database"_s };
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
        return IDBError { ExceptionCode::UnknownError, "Could not rename index"_s };

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo)
        return IDBError { ExceptionCode::UnknownError, "Could not rename index"_s };

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to rename an index without an in-progress transaction"_s };

    if (transaction->mode() != IDBTransactionMode::Versionchange) {
        LOG_ERROR("Attempt to rename an index in a non-version-change transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to rename an index in a non-version-change transaction"_s };
    }

    {
        auto sql = cachedStatement(SQL::RenameIndex, "UPDATE IndexInfo SET name = ? WHERE objectStoreID = ? AND id = ?;"_s);
        if (!sql
            || sql->bindText(1, newName) != SQLITE_OK
            || sql->bindInt64(2, objectStoreIdentifier) != SQLITE_OK
            || sql->bindInt64(3, indexIdentifier) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not update name for index id (%" PRIi64 ", %" PRIi64 ") in IndexInfo table (%i) - %s", objectStoreIdentifier, indexIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Could not rename index"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to see if key exists in objectstore without an in-progress transaction"_s };

    auto keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to check for existence in object store");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKey to check for existence in object store"_s };
    }

    auto sql = cachedStatement(SQL::KeyExistsInObjectStore, "SELECT key FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT) LIMIT 1;"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK
        || sql->bindBlob(2, keyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
        LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Unable to check for existence of IDBKey in object store"_s };
    }

    int sqlResult = sql->step();
    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE)
        return IDBError { };

    if (sqlResult != SQLITE_ROW) {
        // There was an error fetching the record from the database.
        LOG_ERROR("Could not check if key exists in object store (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Error checking for existence of IDBKey in object store"_s };
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
        auto sql = cachedStatement(SQL::GetUnusedBlobFilenames, "SELECT fileName FROM BlobFiles WHERE blobURL NOT IN (SELECT blobURL FROM BlobRecords)"_s);

        if (!sql) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not gather unused blobURLs) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error deleting stored blobs"_s };
        }

        int result = sql->step();
        while (result == SQLITE_ROW) {
            removedBlobFilenames.add(sql->columnText(0));
            result = sql->step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not gather unused blobURLs) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error deleting stored blobs"_s };
        }
    }

    // Remove the blob records that are no longer in use.
    if (!removedBlobFilenames.isEmpty()) {
        auto sql = cachedStatement(SQL::DeleteUnusedBlobs, "DELETE FROM BlobFiles WHERE blobURL NOT IN (SELECT blobURL FROM BlobRecords)"_s);

        if (!sql
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Error deleting stored blobs (%i) (Could not delete blobFile records) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error deleting stored blobs"_s };
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

    auto keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKeyData to be removed from the database");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKeyData to be removed from the database"_s };
    }

    // Get the record ID and value.
    int64_t recordID;
    ThreadSafeDataBuffer value;
    {
        auto sql = cachedStatement(SQL::GetObjectStoreRecord, "SELECT recordID, value FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to delete record from object store"_s };
        }

        int result = sql->step();

        // If there's no record ID, there's no record to delete.
        if (result == SQLITE_DONE)
            return IDBError { };

        if (result != SQLITE_ROW) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (unable to fetch record ID) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to delete record from object store"_s };
        }

        recordID = sql->columnInt64(0);
        value = ThreadSafeDataBuffer::create(sql->columnBlob(1));
    }

    if (recordID < 1) {
        LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (record ID is invalid) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Failed to delete record from object store"_s };
    }

    // Delete the blob records for this object store record.
    {
        auto sql = cachedStatement(SQL::DeleteBlobRecord, "DELETE FROM BlobRecords WHERE objectStoreRow = ?;"_s);

        if (!sql
            || sql->bindInt64(1, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) (Could not delete BlobRecords records) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to delete record from object store"_s };
        }
    }

    auto error = deleteUnusedBlobFileRecords(transaction);
    if (!error.isNull())
        return error;

    // Delete record from object store
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreRecord, "DELETE FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to delete record from object store"_s };
        }
    }

    // Delete record from indexes store
    {
        auto sql = cachedStatement(SQL::DeleteObjectStoreIndexRecord, "DELETE FROM IndexRecords WHERE objectStoreID = ? AND objectStoreRecordID = ?;"_s);

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindInt64(2, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from indexes for object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to delete index entries for object store record"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete range from database without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to delete records from an object store in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to delete records from an object store in a read-only transaction"_s };
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
        return IDBError { ExceptionCode::UnknownError, "Cannot open cursor to delete range of records from the database"_s };
    }

    Vector<IDBKeyData> keys;
    while (!cursor->didComplete() && !cursor->didError()) {
        keys.append(cursor->currentKey());
        cursor->advance(1);
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while accumulating range of records from the database");
        return IDBError { ExceptionCode::UnknownError, "Cursor failed while accumulating range of records from the database"_s };
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

IDBError SQLiteIDBBackingStore::updateOneIndexForAddRecord(IDBObjectStoreInfo& objectStoreInfo, const IDBIndexInfo& info, const IDBKeyData& key, const ThreadSafeDataBuffer& value, int64_t recordID)
{
    std::optional<IndexKey> resultIndexKey;
    callOnIDBSerializationThreadAndWait([objectStoreInfo = objectStoreInfo.isolatedCopy(), info = info.isolatedCopy(), key = key.isolatedCopy(), value, &resultIndexKey](auto& globalObject) {
        auto jsValue = deserializeIDBValueToJSValue(globalObject, value);
        if (jsValue.isUndefinedOrNull())
            return;

        IndexKey indexKey;
        generateIndexKeyForValue(globalObject, info, jsValue, indexKey, objectStoreInfo.keyPath(), key);
        resultIndexKey = WTFMove(indexKey).isolatedCopy();
    });

    if (!resultIndexKey)
        return IDBError { };

    if (resultIndexKey->isNull())
        return IDBError { };

    return uncheckedPutIndexKey(info, key, *resultIndexKey, recordID);
}

IDBError SQLiteIDBBackingStore::updateAllIndexesForAddRecord(const IDBObjectStoreInfo& info, const IDBKeyData& key, const IndexIDToIndexKeyMap& indexKeys, int64_t recordID)
{
    IDBError error;
    const auto& indexMap = info.indexMap();
    bool anyRecordsSucceeded = false;

    for (const auto& [indexID, indexKey] : indexKeys) {
        auto indexIterator = indexMap.find(indexID);
        ASSERT(indexIterator != indexMap.end());

        if (indexIterator == indexMap.end()) {
            error = IDBError { ExceptionCode::InvalidStateError, "Missing index metadata"_s };
            break;
        }

        error = uncheckedPutIndexKey(indexIterator->value, key, indexKey, recordID);
        if (!error.isNull())
            break;

        anyRecordsSucceeded = true;
    }

    if (!error.isNull() && anyRecordsSucceeded) {
        auto sql = cachedStatement(SQL::DeleteObjectStoreIndexRecord, "DELETE FROM IndexRecords WHERE objectStoreID = ? AND objectStoreRecordID = ?;"_s);

        if (!sql
            || sql->bindInt64(1, info.identifier()) != SQLITE_OK
            || sql->bindInt64(2, recordID) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Adding one Index record failed, but failed to remove all others that previously succeeded");
            return IDBError { ExceptionCode::UnknownError, "Adding one Index record failed, but failed to remove all others that previously succeeded"_s };
        }
    }

    return error;
}

IDBError SQLiteIDBBackingStore::addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& objectStoreInfo, const IDBKeyData& keyData, const IndexIDToIndexKeyMap& indexKeys, const IDBValue& value)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::addRecord - key %s, object store %" PRIu64, keyData.loggingString().utf8().data(), objectStoreInfo.identifier());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());
    ASSERT(value.data().data());
    ASSERT(value.blobURLs().size() == value.blobFilePaths().size());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to store a record in an object store without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to store a record in an object store in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to store a record in an object store in a read-only transaction"_s };
    }

    auto keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in an object store");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKey to be stored in an object store"_s };
    }

    int64_t recordID = 0;
    {
        auto sql = cachedStatement(SQL::AddObjectStoreRecord, "INSERT INTO Records VALUES (?, CAST(? AS TEXT), ?, NULL);"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreInfo.identifier()) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindBlob(3, *value.data().data()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Could not put record for object store %" PRIi64 " in Records table (%i) - %s", objectStoreInfo.identifier(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to store record in object store"_s };
        }

        recordID = m_sqliteDB->lastInsertRowID();
    }

    auto error = updateAllIndexesForAddRecord(objectStoreInfo, keyData, indexKeys, recordID);

    if (!error.isNull()) {
        auto sql = cachedStatement(SQL::DeleteObjectStoreRecord, "DELETE FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreInfo.identifier()) != SQLITE_OK
            || sql->bindBlob(2, keyBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->step() != SQLITE_DONE) {
            LOG_ERROR("Indexing new object store record failed, but unable to remove the object store record itself");
            return IDBError { ExceptionCode::UnknownError, "Indexing new object store record failed, but unable to remove the object store record itself"_s };
        }

        return error;
    }

    const Vector<String>& blobURLs = value.blobURLs();
    const Vector<String>& blobFiles = value.blobFilePaths();
    for (size_t i = 0; i < blobURLs.size(); ++i) {
        auto& url = blobURLs[i];
        {
            auto sql = cachedStatement(SQL::AddBlobRecord, "INSERT INTO BlobRecords VALUES (?, ?);"_s);
            if (!sql
                || sql->bindInt64(1, recordID) != SQLITE_OK
                || sql->bindText(2, url) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Unable to record Blob record in database");
                return IDBError { ExceptionCode::UnknownError, "Unable to record Blob record in database"_s };
            }
        }
        int64_t potentialFileNameInteger = m_sqliteDB->lastInsertRowID();

        // If we already have a file for this blobURL, nothing left to do.
        {
            auto sql = cachedStatement(SQL::BlobFilenameForBlobURL, "SELECT fileName FROM BlobFiles WHERE blobURL = ?;"_s);
            if (!sql
                || sql->bindText(1, url) != SQLITE_OK) {
                LOG_ERROR("Unable to examine Blob filenames in database");
                return IDBError { ExceptionCode::UnknownError, "Unable to examine Blob filenames in database"_s };
            }

            int result = sql->step();
            if (result != SQLITE_ROW && result != SQLITE_DONE) {
                LOG_ERROR("Unable to examine Blob filenames in database");
                return IDBError { ExceptionCode::UnknownError, "Unable to examine Blob filenames in database"_s };
            }

            if (result == SQLITE_ROW)
                continue;
        }

        // We don't already have a file for this blobURL, so commit our file as a unique filename
        auto storedFilename = makeString(potentialFileNameInteger, ".blob");
        {
            auto sql = cachedStatement(SQL::AddBlobFilename, "INSERT INTO BlobFiles VALUES (?, ?);"_s);
            if (!sql
                || sql->bindText(1, url) != SQLITE_OK
                || sql->bindText(2, storedFilename) != SQLITE_OK
                || sql->step() != SQLITE_DONE) {
                LOG_ERROR("Unable to record Blob file record in database");
                return IDBError { ExceptionCode::UnknownError, "Unable to record Blob file record in database"_s };
            }
        }

        transaction->addBlobFile(blobFiles[i], storedFilename);
    }

    transaction->notifyCursorsOfChanges(objectStoreInfo.identifier());

    return error;
}

IDBError SQLiteIDBBackingStore::getBlobRecordsForObjectStoreRecord(int64_t objectStoreRecord, Vector<String>& blobURLs, Vector<String>& blobFilePaths)
{
    ASSERT(objectStoreRecord);

    HashSet<String> blobURLSet;
    {
        auto sql = cachedStatement(SQL::GetBlobURL, "SELECT blobURL FROM BlobRecords WHERE objectStoreRow = ?"_s);
        if (!sql
            || sql->bindInt64(1, objectStoreRecord) != SQLITE_OK) {
            LOG_ERROR("Could not prepare statement to fetch blob URLs for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        int sqlResult = sql->step();
        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There are no blobURLs in the database for this object store record.
            return IDBError { };
        }

        while (sqlResult == SQLITE_ROW) {
            blobURLSet.add(sql->columnText(0));
            sqlResult = sql->step();
        }

        if (sqlResult != SQLITE_DONE) {
            LOG_ERROR("Could not fetch blob URLs for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }
    }

    ASSERT(!blobURLSet.isEmpty());
    for (auto& blobURL : blobURLSet) {
        auto sql = cachedStatement(SQL::BlobFilenameForBlobURL, "SELECT fileName FROM BlobFiles WHERE blobURL = ?;"_s);
        if (!sql
            || sql->bindText(1, blobURL) != SQLITE_OK) {
            LOG_ERROR("Could not prepare statement to fetch blob filename for object store record (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        if (sql->step() != SQLITE_ROW) {
            LOG_ERROR("Entry for blob filename for blob url %s does not exist (%i) - %s", blobURL.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to look up blobURL records in object store by key range"_s };
        }

        blobURLs.append(blobURL);

        String fileName = sql->columnText(0);
        blobFilePaths.append(FileSystem::pathByAppendingComponent(m_databaseDirectory, fileName));
    }
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, const IDBKeyRangeData& keyRange, IDBGetRecordDataType type, IDBGetResult& resultValue)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getRecord - key range %s, object store %" PRIu64, keyRange.loggingString().utf8().data(), objectStoreID);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to get a record from database without an in-progress transaction"_s };

    auto* objectStoreInfo = infoForObjectStore(objectStoreID);
    if (!objectStoreInfo)
        return IDBError { ExceptionCode::InvalidStateError, "Object store cannot be found in the database"_s };

    auto key = keyRange.lowerKey;
    if (key.isNull())
        key = IDBKeyData::minimum();
    auto lowerBuffer = serializeIDBKeyData(key);
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize lower IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize lower IDBKey in lookup range"_s };
    }

    key = keyRange.upperKey;
    if (key.isNull())
        key = IDBKeyData::maximum();
    auto upperBuffer = serializeIDBKeyData(key);
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize upper IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize upper IDBKey in lookup range"_s };
    }

    int64_t recordID = 0;
    ThreadSafeDataBuffer keyResultBuffer, valueResultBuffer;
    {
        SQLiteStatementAutoResetScope sql;

        switch (type) {
        case IDBGetRecordDataType::KeyAndValue:
            if (keyRange.lowerOpen) {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetValueRecordsLowerOpenUpperOpen, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
                else
                    sql = cachedStatement(SQL::GetValueRecordsLowerOpenUpperClosed, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
            } else {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetValueRecordsLowerClosedUpperOpen, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
                else
                    sql = cachedStatement(SQL::GetValueRecordsLowerClosedUpperClosed, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
            }
            break;
        case IDBGetRecordDataType::KeyOnly:
            if (keyRange.lowerOpen) {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetKeyRecordsLowerOpenUpperOpen, "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
                else
                    sql = cachedStatement(SQL::GetKeyRecordsLowerOpenUpperClosed, "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
            } else {
                if (keyRange.upperOpen)
                    sql = cachedStatement(SQL::GetKeyRecordsLowerClosedUpperOpen, "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
                else
                    sql = cachedStatement(SQL::GetKeyRecordsLowerClosedUpperClosed, "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
            }
        }

        if (!sql
            || sql->bindInt64(1, objectStoreID) != SQLITE_OK
            || sql->bindBlob(2, lowerBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || sql->bindBlob(3, upperBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
            LOG_ERROR("Could not get key range record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Failed to look up record in object store by key range"_s };
        }

        int sqlResult = sql->step();

        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There was no record for the key in the database.
            return IDBError { };
        }
        if (sqlResult != SQLITE_ROW) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Error looking up record in object store by key range"_s };
        }

        keyResultBuffer = ThreadSafeDataBuffer::create(sql->columnBlob(0));

        if (type == IDBGetRecordDataType::KeyAndValue) {
            valueResultBuffer = ThreadSafeDataBuffer::create(sql->columnBlob(1));
            recordID = sql->columnInt64(2);
        }
    }

    auto* keyVector = keyResultBuffer.data();
    if (!keyVector) {
        LOG_ERROR("Unable to deserialize key data from database for IDBObjectStore");
        return IDBError { ExceptionCode::UnknownError, "Error extracting key data from database executing IDBObjectStore get"_s };
    }
    
    IDBKeyData keyData;
    if (!deserializeIDBKeyData(keyVector->data(), keyVector->size(), keyData)) {
        LOG_ERROR("Unable to deserialize key data from database for IDBObjectStore");
        return IDBError { ExceptionCode::UnknownError, "Error extracting key data from database executing IDBObjectStore get"_s };
    }

    if (type == IDBGetRecordDataType::KeyOnly) {
        resultValue = { keyData };
        return IDBError { };
    }

    ASSERT(recordID);
    Vector<String> blobURLs, blobFilePaths;
    auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, blobFilePaths);
    ASSERT(blobURLs.size() == blobFilePaths.size());

    if (!error.isNull())
        return error;

    resultValue = { keyData, { valueResultBuffer, WTFMove(blobURLs), WTFMove(blobFilePaths) }, objectStoreInfo->keyPath() };
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    return getAllRecordsData.indexIdentifier ? getAllIndexRecords(transactionIdentifier, getAllRecordsData, result) : getAllObjectStoreRecords(transactionIdentifier, getAllRecordsData, result);
}

SQLiteStatementAutoResetScope SQLiteIDBBackingStore::cachedStatementForGetAllObjectStoreRecords(const IDBGetAllRecordsData& getAllRecordsData)
{
    if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Keys) {
        if (getAllRecordsData.keyRangeData.lowerOpen) {
            if (getAllRecordsData.keyRangeData.upperOpen)
                return cachedStatement(SQL::GetAllKeyRecordsLowerOpenUpperOpen, "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
            return cachedStatement(SQL::GetAllKeyRecordsLowerOpenUpperClosed, "SELECT key FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
        }

        if (getAllRecordsData.keyRangeData.upperOpen)
            return cachedStatement(SQL::GetAllKeyRecordsLowerClosedUpperOpen, "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
        return cachedStatement(SQL::GetAllKeyRecordsLowerClosedUpperClosed, "SELECT key FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
    }

    if (getAllRecordsData.keyRangeData.lowerOpen) {
        if (getAllRecordsData.keyRangeData.upperOpen)
            return cachedStatement(SQL::GetValueRecordsLowerOpenUpperOpen, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
        return cachedStatement(SQL::GetValueRecordsLowerOpenUpperClosed, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
    }

    if (getAllRecordsData.keyRangeData.upperOpen)
        return cachedStatement(SQL::GetValueRecordsLowerClosedUpperOpen, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"_s);
    return cachedStatement(SQL::GetValueRecordsLowerClosedUpperClosed, "SELECT key, value, ROWID FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"_s);
}

IDBError SQLiteIDBBackingStore::getAllObjectStoreRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getAllObjectStoreRecords");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to get records from database without an in-progress transaction"_s };

    auto key = getAllRecordsData.keyRangeData.lowerKey;
    if (key.isNull())
        key = IDBKeyData::minimum();
    auto lowerBuffer = serializeIDBKeyData(key);
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize lower IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize lower IDBKey in lookup range"_s };
    }

    key = getAllRecordsData.keyRangeData.upperKey;
    if (key.isNull())
        key = IDBKeyData::maximum();
    auto upperBuffer = serializeIDBKeyData(key);
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize upper IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize upper IDBKey in lookup range"_s };
    }

    auto sql = cachedStatementForGetAllObjectStoreRecords(getAllRecordsData);
    if (!sql
        || sql->bindInt64(1, getAllRecordsData.objectStoreIdentifier) != SQLITE_OK
        || sql->bindBlob(2, lowerBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
        || sql->bindBlob(3, upperBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
        LOG_ERROR("Could not get key range record from object store %" PRIi64 " from Records table (%i) - %s", getAllRecordsData.objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Failed to look up record in object store by key range"_s };
    }

    auto* objectStoreInfo = infoForObjectStore(getAllRecordsData.objectStoreIdentifier);
    if (!objectStoreInfo)
        return IDBError { ExceptionCode::InvalidStateError, "Object store cannot be found in the database"_s };

    result = { getAllRecordsData.getAllType, objectStoreInfo->keyPath() };

    uint32_t targetResults;
    if (getAllRecordsData.count && getAllRecordsData.count.value())
        targetResults = getAllRecordsData.count.value();
    else
        targetResults = std::numeric_limits<uint32_t>::max();

    int sqlResult = sql->step();
    uint32_t returnedResults = 0;

    while (sqlResult == SQLITE_ROW && returnedResults < targetResults) {
        auto keyBufferSpan = sql->columnBlobAsSpan(0);
        IDBKeyData keyData;
        if (!deserializeIDBKeyData(keyBufferSpan.data(), keyBufferSpan.size(), keyData)) {
            LOG_ERROR("Unable to deserialize key data from database while getting all records");
            return IDBError { ExceptionCode::UnknownError, "Unable to deserialize key data while getting all records"_s };
        }
        result.addKey(WTFMove(keyData));

        if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Values) {
            ThreadSafeDataBuffer valueResultBuffer = ThreadSafeDataBuffer::create(sql->columnBlob(1));

            auto recordID = sql->columnInt64(2);

            ASSERT(recordID);
            Vector<String> blobURLs, blobFilePaths;
            auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, blobFilePaths);
            ASSERT(blobURLs.size() == blobFilePaths.size());

            if (!error.isNull())
                return error;

            result.addValue({ valueResultBuffer, WTFMove(blobURLs), WTFMove(blobFilePaths) });
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
    return IDBError { ExceptionCode::UnknownError, "Error looking up record in object store by key range"_s };
}

IDBError SQLiteIDBBackingStore::getAllIndexRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getAllIndexRecords - %s", getAllRecordsData.keyRangeData.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to get all index records from database without an in-progress transaction"_s };

    auto cursor = transaction->maybeOpenBackingStoreCursor(getAllRecordsData.objectStoreIdentifier, getAllRecordsData.indexIdentifier, getAllRecordsData.keyRangeData);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index gets in database");
        return IDBError { ExceptionCode::UnknownError, "Cannot open cursor to perform index gets in database"_s };
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index records in database");
        return IDBError { ExceptionCode::UnknownError, "Cursor failed while looking up index records in database"_s };
    }

    auto* objectStoreInfo = infoForObjectStore(getAllRecordsData.objectStoreIdentifier);
    if (!objectStoreInfo)
        return IDBError { ExceptionCode::InvalidStateError, "Object store cannot be found in the database"_s };

    result = { getAllRecordsData.getAllType, objectStoreInfo->keyPath() };

    uint32_t currentCount = 0;
    uint32_t targetCount = getAllRecordsData.count ? getAllRecordsData.count.value() : 0;
    if (!targetCount)
        targetCount = std::numeric_limits<uint32_t>::max();
    while (!cursor->didComplete() && !cursor->didError() && currentCount < targetCount) {
        IDBKeyData keyCopy = cursor->currentPrimaryKey();
        result.addKey(WTFMove(keyCopy));
        if (getAllRecordsData.getAllType == IndexedDB::GetAllType::Values)
            result.addValue(IDBValue(cursor->currentValue()));

        ++currentCount;
        cursor->advance(1);
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index records in database");
        return IDBError { ExceptionCode::UnknownError, "Cursor failed while looking up index records in database"_s };
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t indexID, IndexedDB::IndexRecordType type, const IDBKeyRangeData& range, IDBGetResult& getResult)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getIndexRecord - %s", range.loggingString().utf8().data());

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to get an index record from database without an in-progress transaction"_s };

    if (range.isExactlyOneKey())
        return uncheckedGetIndexRecordForOneKey(indexID, objectStoreID, type, range.lowerKey, getResult);

    auto cursor = transaction->maybeOpenBackingStoreCursor(objectStoreID, indexID, range);
    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index get in database");
        return IDBError { ExceptionCode::UnknownError, "Cannot open cursor to perform index get in database"_s };
    }

    if (cursor->didError()) {
        LOG_ERROR("Cursor failed while looking up index record in database");
        return IDBError { ExceptionCode::UnknownError, "Cursor failed while looking up index record in database"_s };
    }

    if (cursor->didComplete())
        getResult = { };
    else {
        if (type == IndexedDB::IndexRecordType::Key)
            getResult = { cursor->currentPrimaryKey() };
        else {
            auto* objectStoreInfo = infoForObjectStore(objectStoreID);
            ASSERT(objectStoreInfo);
            getResult = { cursor->currentPrimaryKey(), cursor->currentPrimaryKey(), IDBValue(cursor->currentValue()), objectStoreInfo->keyPath() };
        }
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedGetIndexRecordForOneKey(int64_t indexID, int64_t objectStoreID, IndexedDB::IndexRecordType type, const IDBKeyData& key, IDBGetResult& getResult)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::uncheckedGetIndexRecordForOneKey");

    ASSERT(key.isValid() && key.type() != IndexedDB::KeyType::Max && key.type() != IndexedDB::KeyType::Min);

    auto buffer = serializeIDBKeyData(key);
    if (!buffer) {
        LOG_ERROR("Unable to serialize IDBKey to look up one index record");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize IDBKey to look up one index record"_s };
    }

    auto sql = cachedStatement(SQL::GetIndexRecordForOneKey, "SELECT IndexRecords.value, Records.value, Records.recordID FROM Records INNER JOIN IndexRecords ON Records.objectStoreID = IndexRecords.objectStoreID AND Records.recordID = IndexRecords.objectStoreRecordID WHERE IndexRecords.indexID = ? AND IndexRecords.key = CAST(? AS TEXT) ORDER BY IndexRecords.key, IndexRecords.value"_s);

    if (!sql
        || sql->bindInt64(1, indexID) != SQLITE_OK
        || sql->bindBlob(2, buffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
        LOG_ERROR("Unable to lookup index record in database");
        return IDBError { ExceptionCode::UnknownError, "Unable to lookup index record in database"_s };
    }

    int result = sql->step();
    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        LOG_ERROR("Unable to lookup index record in database");
        return IDBError { ExceptionCode::UnknownError, "Unable to lookup index record in database"_s };
    }

    if (result == SQLITE_DONE)
        return IDBError { };

    IDBKeyData objectStoreKey;
    auto keySpan = sql->columnBlobAsSpan(0);

    if (!deserializeIDBKeyData(keySpan.data(), keySpan.size(), objectStoreKey)) {
        LOG_ERROR("Unable to deserialize key looking up index record in database");
        return IDBError { ExceptionCode::UnknownError, "Unable to deserialize key looking up index record in database"_s };
    }

    if (type == IndexedDB::IndexRecordType::Key) {
        getResult = { objectStoreKey };
        return IDBError { };
    }

    auto valueVector = sql->columnBlob(1);
    int64_t recordID = sql->columnInt64(2);
    Vector<String> blobURLs, blobFilePaths;
    auto error = getBlobRecordsForObjectStoreRecord(recordID, blobURLs, blobFilePaths);
    ASSERT(blobURLs.size() == blobFilePaths.size());

    if (!error.isNull())
        return error;

    auto* objectStoreInfo = infoForObjectStore(objectStoreID);
    ASSERT(objectStoreInfo);
    getResult = { objectStoreKey, objectStoreKey, { ThreadSafeDataBuffer::create(WTFMove(valueVector)), WTFMove(blobURLs), WTFMove(blobFilePaths) }, objectStoreInfo->keyPath() };
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, uint64_t& outCount)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getCount - object store %" PRIu64, objectStoreIdentifier);
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to get count from database without an in-progress transaction"_s };

    outCount = 0;

    auto lowerKey = range.lowerKey.isNull() ? IDBKeyData::minimum() : range.lowerKey;
    auto lowerBuffer = serializeIDBKeyData(lowerKey);
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize lower IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize lower IDBKey in lookup range for count operation"_s };
    }

    auto upperKey = range.upperKey.isNull() ? IDBKeyData::maximum() : range.upperKey;
    auto upperBuffer = serializeIDBKeyData(upperKey);
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize upper IDBKey in lookup range");
        return IDBError { ExceptionCode::UnknownError, "Unable to serialize upper IDBKey in lookup range for count operation"_s };
    }

    SQLiteStatementAutoResetScope statement;

    if (!indexIdentifier) {
        if (range.lowerOpen && range.upperOpen)
            statement = cachedStatement(SQL::CountRecordsLowerOpenUpperOpen, "SELECT COUNT(*) FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT);"_s);
        else if (range.lowerOpen && !range.upperOpen)
            statement = cachedStatement(SQL::CountRecordsLowerOpenUpperClosed, "SELECT COUNT(*) FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT);"_s);
        else if (!range.lowerOpen && range.upperOpen)
            statement = cachedStatement(SQL::CountRecordsLowerClosedUpperOpen, "SELECT COUNT(*) FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT);"_s);
        else
            statement = cachedStatement(SQL::CountRecordsLowerClosedUpperClosed, "SELECT COUNT(*) FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT);"_s);
        
        if (!statement
            || statement->bindInt64(1, objectStoreIdentifier) != SQLITE_OK
            || statement->bindBlob(2, lowerBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || statement->bindBlob(3, upperBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
            LOG_ERROR("Could not count records in object store %" PRIi64 " from Records table (%i) - %s", objectStoreIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to count records in object store due to binding failure"_s };
        }
    } else {
        if (range.lowerOpen && range.upperOpen)
            statement = cachedStatement(SQL::CountIndexRecordsLowerOpenUpperOpen, "SELECT COUNT(*) FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT);"_s);
        else if (range.lowerOpen && !range.upperOpen)
            statement = cachedStatement(SQL::CountIndexRecordsLowerOpenUpperClosed, "SELECT COUNT(*) FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT);"_s);
        else if (!range.lowerOpen && range.upperOpen)
            statement = cachedStatement(SQL::CountIndexRecordsLowerClosedUpperOpen, "SELECT COUNT(*) FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT);"_s);
        else
            statement = cachedStatement(SQL::CountIndexRecordsLowerClosedUpperClosed, "SELECT COUNT(*) FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT);"_s);

        if (!statement
            || statement->bindInt64(1, indexIdentifier) != SQLITE_OK
            || statement->bindBlob(2, lowerBuffer->dataAsSpanForContiguousData()) != SQLITE_OK
            || statement->bindBlob(3, upperBuffer->dataAsSpanForContiguousData()) != SQLITE_OK) {
            LOG_ERROR("Could not count records with index %" PRIi64 " from IndexRecords table (%i) - %s", indexIdentifier, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return IDBError { ExceptionCode::UnknownError, "Unable to count records for index due to binding failure"_s };
        }
    }

    if (statement->step() != SQLITE_ROW)
        return IDBError { ExceptionCode::UnknownError, "Unable to count records"_s };

    outCount = statement->columnInt(0);
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedGetKeyGeneratorValue(int64_t objectStoreID, uint64_t& outValue)
{
    auto sql = cachedStatement(SQL::GetKeyGeneratorValue, "SELECT currentKey FROM KeyGenerators WHERE objectStoreID = ?;"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK) {
        LOG_ERROR("Could not retrieve currentKey from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::UnknownError, "Error getting current key generator value from database"_s };
    }
    int result = sql->step();
    if (result != SQLITE_ROW) {
        LOG_ERROR("Could not retreive key generator value for object store, but it should be there.");
        return IDBError { ExceptionCode::UnknownError, "Error finding current key generator value in database"_s };
    }

    int64_t value = sql->columnInt64(0);
    if (value < 0)
        return IDBError { ExceptionCode::ConstraintError, "Current key generator value from database is invalid"_s };

    outValue = value;
    return IDBError { };
}

IDBError SQLiteIDBBackingStore::uncheckedSetKeyGeneratorValue(int64_t objectStoreID, uint64_t value)
{
    auto sql = cachedStatement(SQL::SetKeyGeneratorValue, "INSERT INTO KeyGenerators VALUES (?, ?);"_s);
    if (!sql
        || sql->bindInt64(1, objectStoreID) != SQLITE_OK
        || sql->bindInt64(2, value) != SQLITE_OK
        || sql->step() != SQLITE_DONE) {
        LOG_ERROR("Could not update key generator value (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return IDBError { ExceptionCode::ConstraintError, "Error storing new key generator value in database"_s };
    }

    return IDBError { };
}

IDBError SQLiteIDBBackingStore::generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t& generatedKey)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::generateKeyNumber");

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to generate key in database without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to generate key in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to generate key in a read-only transaction"_s };
    }

    uint64_t currentValue;
    auto error = uncheckedGetKeyGeneratorValue(objectStoreID, currentValue);
    if (!error.isNull())
        return error;

    if (currentValue + 1 > maxGeneratorValue)
        return IDBError { ExceptionCode::ConstraintError, "Cannot generate new key value over 2^53 for object store operation"_s };

    generatedKey = currentValue + 1;
    return uncheckedSetKeyGeneratorValue(objectStoreID, generatedKey);
}

IDBError SQLiteIDBBackingStore::revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreID, uint64_t newKeyNumber)
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::revertGeneratedKeyNumber - object store %" PRIu64 ", reverted number %" PRIu64, objectStoreID, newKeyNumber);

    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    auto* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to revert key generator value in database without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to revert key generator value in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to revert key generator value in a read-only transaction"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to update key generator value in database without an in-progress transaction"_s };

    if (transaction->mode() == IDBTransactionMode::Readonly) {
        LOG_ERROR("Attempt to update key generator value in a read-only transaction");
        return IDBError { ExceptionCode::UnknownError, "Attempt to update key generator value in a read-only transaction"_s };
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
    if (!transaction || !transaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to open a cursor in database without an in-progress transaction"_s };

    auto* cursor = transaction->maybeOpenCursor(info);
    if (!cursor) {
        LOG_ERROR("Unable to open cursor");
        return IDBError { ExceptionCode::UnknownError, "Unable to open cursor"_s };
    }

    m_cursors.set(cursor->identifier(), cursor);

    auto* objectStoreInfo = infoForObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStoreInfo);
    cursor->currentData(result, objectStoreInfo->keyPath());
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
        return IDBError { ExceptionCode::UnknownError, "Attempt to iterate a cursor that doesn't exist"_s };
    }

    ASSERT_UNUSED(transactionIdentifier, cursor->transaction()->transactionIdentifier() == transactionIdentifier);

    if (!cursor->transaction() || !cursor->transaction()->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Attempt to iterate a cursor without an in-progress transaction"_s };

    auto key = data.keyData;
    auto primaryKey = data.primaryKeyData;
    auto count = data.count;

    if (key.isValid()) {
        if (!cursor->iterate(key, primaryKey)) {
            LOG_ERROR("Attempt to iterate cursor failed");
            return IDBError { ExceptionCode::UnknownError, "Attempt to iterate cursor failed"_s };
        }
    } else {
        ASSERT(!primaryKey.isValid());
        if (!count)
            count = 1;
        if (!cursor->advance(count)) {
            LOG_ERROR("Attempt to advance cursor failed");
            return IDBError { ExceptionCode::UnknownError, "Attempt to advance cursor failed"_s };
        }
    }

    if (data.option == IndexedDB::CursorIterateOption::Reply) {
        auto* objectStoreInfo = infoForObjectStore(cursor->objectStoreID());
        ASSERT(objectStoreInfo);

        bool shouldPrefetch = key.isNull() && primaryKey.isNull();
        if (shouldPrefetch)
            cursor->prefetch();

        cursor->currentData(result, objectStoreInfo->keyPath(), shouldPrefetch ? SQLiteIDBCursor::ShouldIncludePrefetchedRecords::Yes : SQLiteIDBCursor::ShouldIncludePrefetchedRecords::No);
    }

    return IDBError { };
}

IDBObjectStoreInfo* SQLiteIDBBackingStore::infoForObjectStore(uint64_t objectStoreIdentifier)
{
    ASSERT(m_databaseInfo);
    return m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
}

void SQLiteIDBBackingStore::deleteBackingStore()
{
    String databasePath = fullDatabasePath();

    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteBackingStore deleting file '%s' on disk", databasePath.utf8().data());

    if (FileSystem::fileExists(databasePath) && !m_sqliteDB) {
        m_sqliteDB = makeUnique<SQLiteDatabase>();
        if (!m_sqliteDB->open(databasePath))
            closeSQLiteDB();
    }

    if (m_sqliteDB) {
        Vector<String> blobFiles;
        {
            auto sql = m_sqliteDB->prepareStatement("SELECT fileName FROM BlobFiles;"_s);
            if (!sql)
                LOG_ERROR("Error preparing statement to get blob filenames (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            else {
                int result = sql->step();
                while (result == SQLITE_ROW) {
                    blobFiles.append(sql->columnText(0));
                    result = sql->step();
                }

                if (result != SQLITE_DONE)
                    LOG_ERROR("Error getting blob filenames (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            }
        }

        for (auto& file : blobFiles) {
            String blobPath = FileSystem::pathByAppendingComponent(m_databaseDirectory, file);
            if (!FileSystem::deleteFile(blobPath))
                LOG_ERROR("Error deleting blob file '%s'", blobPath.utf8().data());
        }

        closeSQLiteDB();
    }

    SQLiteFileSystem::deleteDatabaseFile(databasePath);
    SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_databaseDirectory);
}

void SQLiteIDBBackingStore::unregisterCursor(SQLiteIDBCursor& cursor)
{
    ASSERT(m_cursors.contains(cursor.identifier()));
    m_cursors.remove(cursor.identifier());
}

SQLiteStatementAutoResetScope SQLiteIDBBackingStore::cachedStatement(SQLiteIDBBackingStore::SQL sql, ASCIILiteral query)
{
    if (sql >= SQL::Invalid) {
        LOG_ERROR("Invalid SQL statement ID passed to cachedStatement()");
        return SQLiteStatementAutoResetScope { };
    }

    if (m_cachedStatements[static_cast<size_t>(sql)])
        return SQLiteStatementAutoResetScope { m_cachedStatements[static_cast<size_t>(sql)].get() };

    if (m_sqliteDB) {
        auto statement = m_sqliteDB->prepareHeapStatement(query);
        if (statement)
            m_cachedStatements[static_cast<size_t>(sql)] = statement.value().moveToUniquePtr();
    }

    return SQLiteStatementAutoResetScope { m_cachedStatements[static_cast<size_t>(sql)].get() };
}

void SQLiteIDBBackingStore::close()
{
    closeSQLiteDB();
}

void SQLiteIDBBackingStore::closeSQLiteDB()
{
    for (size_t i = 0; i < static_cast<int>(SQL::Invalid); ++i)
        m_cachedStatements[i] = nullptr;

    if (m_sqliteDB)
        m_sqliteDB->close();

    m_sqliteDB = nullptr;
}

bool SQLiteIDBBackingStore::hasTransaction(const IDBResourceIdentifier& transactionIdentifier) const
{
    ASSERT(isMainThread());
    return m_transactions.contains(transactionIdentifier);
}

void SQLiteIDBBackingStore::handleLowMemoryWarning()
{
    if (m_sqliteDB)
        m_sqliteDB->releaseMemory();
}

#undef TABLE_SCHEMA_PREFIX
#undef V3_RECORDS_TABLE_SCHEMA_SUFFIX
#undef V3_INDEX_RECORDS_TABLE_SCHEMA_SUFFIX
#undef INDEX_INFO_TABLE_SCHEMA_SUFFIX
#undef BLOB_RECORDS_TABLE_SCHEMA_SUFFIX
#undef BLOB_FILES_TABLE_SCHEMA_SUFFIX

} // namespace IDBServer
} // namespace WebCore
