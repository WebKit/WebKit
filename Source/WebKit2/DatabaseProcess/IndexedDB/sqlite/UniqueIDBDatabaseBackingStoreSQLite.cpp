/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "UniqueIDBDatabaseBackingStoreSQLite.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "ArgumentDecoder.h"
#include "IDBSerialization.h"
#include "Logging.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteIDBTransaction.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBBindingUtilities.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRange.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

using namespace JSC;
using namespace WebCore;

namespace WebKit {

// Current version of the metadata schema being used in the metadata database.
static const int currentMetadataVersion = 1;

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

static int64_t generateDatabaseId()
{
    static int64_t databaseID = 0;

    ASSERT(!RunLoop::isMain());
    return ++databaseID;
}

UniqueIDBDatabaseBackingStoreSQLite::UniqueIDBDatabaseBackingStoreSQLite(const LegacyUniqueIDBDatabaseIdentifier& identifier, const String& databaseDirectory)
    : m_identifier(identifier)
    , m_absoluteDatabaseDirectory(databaseDirectory)
{
    // The backing store is meant to be created and used entirely on a background thread.
    ASSERT(!RunLoop::isMain());
}

UniqueIDBDatabaseBackingStoreSQLite::~UniqueIDBDatabaseBackingStoreSQLite()
{
    ASSERT(!RunLoop::isMain());

    m_transactions.clear();
    m_sqliteDB = nullptr;

    if (m_vm) {
        JSLockHolder locker(m_vm.get());
        m_globalObject.clear();
        m_vm = nullptr;
    }
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

bool UniqueIDBDatabaseBackingStoreSQLite::ensureValidRecordsTable()
{
    ASSERT(!RunLoop::isMain());
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

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::createAndPopulateInitialMetadata()
{
    ASSERT(!RunLoop::isMain());
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
            || sql.bindText(1, String::number(IDBDatabaseMetadata::NoIntVersion)) != SQLITE_OK
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

    // This initial metadata matches the default values we just put into the metadata database.
    auto metadata = std::make_unique<IDBDatabaseMetadata>();
    metadata->name = m_identifier.databaseName();
    metadata->version = IDBDatabaseMetadata::NoIntVersion;
    metadata->maxObjectStoreId = 1;

    return metadata;
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::extractExistingMetadata()
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);

    if (!m_sqliteDB->tableExists(ASCIILiteral("IDBDatabaseInfo")))
        return nullptr;

    auto metadata = std::make_unique<IDBDatabaseMetadata>();
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM IDBDatabaseInfo WHERE key = 'MetadataVersion';"));
        if (sql.isColumnNull(0))
            return nullptr;
        metadata->version = sql.getColumnInt(0);
    }
    {
        SQLiteStatement sql(*m_sqliteDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseName';");
        if (sql.isColumnNull(0))
            return nullptr;
        metadata->name = sql.getColumnText(0);
        if (metadata->name != m_identifier.databaseName()) {
            LOG_ERROR("Database name in the metadata database ('%s') does not match the expected name ('%s')", metadata->name.utf8().data(), m_identifier.databaseName().utf8().data());
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';"));
        if (sql.isColumnNull(0))
            return nullptr;
        String stringVersion = sql.getColumnText(0);
        bool ok;
        metadata->version = stringVersion.toUInt64Strict(&ok);
        if (!ok) {
            LOG_ERROR("Database version on disk ('%s') does not cleanly convert to an unsigned 64-bit integer version", stringVersion.utf8().data());
            return nullptr;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT id, name, keyPath, autoInc, maxIndexID FROM ObjectStoreInfo;"));
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            IDBObjectStoreMetadata osMetadata;
            osMetadata.id = sql.getColumnInt64(0);
            osMetadata.name = sql.getColumnText(1);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(2, keyPathBuffer);

            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), osMetadata.keyPath)) {
                LOG_ERROR("Unable to extract key path metadata from database");
                return nullptr;
            }

            osMetadata.autoIncrement = sql.getColumnInt(3);
            osMetadata.maxIndexId = sql.getColumnInt64(4);

            metadata->objectStores.set(osMetadata.id, osMetadata);
            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching object store metadata from database on disk");
            return nullptr;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"));
        if (sql.prepare() != SQLITE_OK)
            return nullptr;

        int result = sql.step();
        while (result == SQLITE_ROW) {
            IDBIndexMetadata indexMetadata;

            indexMetadata.id = sql.getColumnInt64(0);
            indexMetadata.name = sql.getColumnText(1);
            int64_t objectStoreID = sql.getColumnInt64(2);

            Vector<char> keyPathBuffer;
            sql.getColumnBlobAsVector(3, keyPathBuffer);

            if (!deserializeIDBKeyPath(reinterpret_cast<const uint8_t*>(keyPathBuffer.data()), keyPathBuffer.size(), indexMetadata.keyPath)) {
                LOG_ERROR("Unable to extract key path metadata from database");
                return nullptr;
            }

            indexMetadata.unique = sql.getColumnInt(4);
            indexMetadata.multiEntry = sql.getColumnInt(5);

            auto objectStoreMetadataIt = metadata->objectStores.find(objectStoreID);
            if (objectStoreMetadataIt == metadata->objectStores.end()) {
                LOG_ERROR("Found index referring to a non-existant object store");
                return nullptr;
            }

            objectStoreMetadataIt->value.indexes.set(indexMetadata.id, indexMetadata);

            result = sql.step();
        }

        if (result != SQLITE_DONE) {
            LOG_ERROR("Error fetching index metadata from database on disk");
            return nullptr;
        }
    }

    return metadata;
}

std::unique_ptr<SQLiteDatabase> UniqueIDBDatabaseBackingStoreSQLite::openSQLiteDatabaseAtPath(const String& path)
{
    ASSERT(!RunLoop::isMain());

    auto sqliteDatabase = std::make_unique<SQLiteDatabase>();
    if (!sqliteDatabase->open(path)) {
        LOG_ERROR("Failed to open SQLite database at path '%s'", path.utf8().data());
        return nullptr;
    }

    // Since a WorkQueue isn't bound to a specific thread, we have to disable threading checks
    // even though we never access the database from different threads simultaneously.
    sqliteDatabase->disableThreadingChecks();

    return sqliteDatabase;
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::getOrEstablishMetadata()
{
    ASSERT(!RunLoop::isMain());

    String dbFilename = LegacyUniqueIDBDatabase::calculateAbsoluteDatabaseFilename(m_absoluteDatabaseDirectory);

    m_sqliteDB = openSQLiteDatabaseAtPath(dbFilename);
    if (!m_sqliteDB)
        return nullptr;

    m_sqliteDB->setCollationFunction("IDBKEY", [this](int aLength, const void* a, int bLength, const void* b) {
        return idbKeyCollate(aLength, a, bLength, b);
    });

    if (!ensureValidRecordsTable()) {
        LOG_ERROR("Error creating or migrating Records table in database");
        m_sqliteDB = nullptr;
        return nullptr;
    }

    std::unique_ptr<IDBDatabaseMetadata> metadata = extractExistingMetadata();
    if (!metadata)
        metadata = createAndPopulateInitialMetadata();

    if (!metadata)
        LOG_ERROR("Unable to establish IDB database at path '%s'", dbFilename.utf8().data());

    // The database id is a runtime concept and doesn't need to be stored in the metadata database.
    metadata->id = generateDatabaseId();

    return metadata;
}

bool UniqueIDBDatabaseBackingStoreSQLite::establishTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>&, IndexedDB::TransactionMode mode)
{
    ASSERT(!RunLoop::isMain());

    if (!m_transactions.add(transactionIdentifier, std::make_unique<SQLiteIDBTransaction>(*this, transactionIdentifier, mode)).isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return false;
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::beginTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to begin a transaction that hasn't been established");
        return false;
    }

    return transaction->begin(*m_sqliteDB);
}

bool UniqueIDBDatabaseBackingStoreSQLite::commitTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return false;
    }

    return transaction->commit();
}

bool UniqueIDBDatabaseBackingStoreSQLite::resetTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());

    std::unique_ptr<SQLiteIDBTransaction> transaction = m_transactions.take(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to reset a transaction that hasn't been established");
        return false;
    }

    return transaction->reset();
}

bool UniqueIDBDatabaseBackingStoreSQLite::rollbackTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to rollback a transaction that hasn't been established");
        return false;
    }

    if (!transaction->inProgress()) {
        LOG_ERROR("Attempt to rollback a transaction that hasn't begun");
        return false;
    }

    return transaction->rollback();
}

bool UniqueIDBDatabaseBackingStoreSQLite::changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to change database version with an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to change database version during a non version-change transaction");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("UPDATE IDBDatabaseInfo SET value = ? where key = 'DatabaseVersion';"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, String::number(newVersion)) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not update database version in IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::createObjectStore(const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to change database version with an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to change database version during a non version-change transaction");
        return false;
    }

    RefPtr<SharedBuffer> keyPathBlob = serializeIDBKeyPath(metadata.keyPath);
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO ObjectStoreInfo VALUES (?, ?, ?, ?, ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, metadata.id) != SQLITE_OK
            || sql.bindText(2, metadata.name) != SQLITE_OK
            || sql.bindBlob(3, keyPathBlob->data(), keyPathBlob->size()) != SQLITE_OK
            || sql.bindInt(4, metadata.autoIncrement) != SQLITE_OK
            || sql.bindInt64(5, metadata.maxIndexId) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not add object store '%s' to ObjectStoreInfo table (%i) - %s", metadata.name.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO KeyGenerators VALUES (?, 0);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, metadata.id) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not seed initial key generator value for ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to change database version with an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to change database version during a non version-change transaction");
        return false;
    }

    // Delete the ObjectStore record
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM ObjectStoreInfo WHERE id = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store id %" PRIi64 " from ObjectStoreInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete the ObjectStore's key generator record if there is one.
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM KeyGenerators WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete object store from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM Records WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records for object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated Indexes
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexInfo WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated Index records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to change database version with an establish, in-progress transaction");
        return false;
    }
    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to change database version during a read-only transaction");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM Records WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records from object store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete records from index record store id %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to create index without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to create index during a non-version-change transaction");
        return false;
    }

    RefPtr<SharedBuffer> keyPathBlob = serializeIDBKeyPath(metadata.keyPath);
    if (!keyPathBlob) {
        LOG_ERROR("Unable to serialize IDBKeyPath to save in database");
        return false;
    }

    SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IndexInfo VALUES (?, ?, ?, ?, ?, ?);"));
    if (sql.prepare() != SQLITE_OK
        || sql.bindInt64(1, metadata.id) != SQLITE_OK
        || sql.bindText(2, metadata.name) != SQLITE_OK
        || sql.bindInt64(3, objectStoreID) != SQLITE_OK
        || sql.bindBlob(4, keyPathBlob->data(), keyPathBlob->size()) != SQLITE_OK
        || sql.bindInt(5, metadata.unique) != SQLITE_OK
        || sql.bindInt(6, metadata.multiEntry) != SQLITE_OK
        || sql.step() != SQLITE_DONE) {
        LOG_ERROR("Could not add index '%s' to IndexInfo table (%i) - %s", metadata.name.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    // Write index records for any records that already exist in this object store.
    SQLiteIDBCursor* cursor = transaction->openCursor(objectStoreID, IDBIndexMetadata::InvalidId, IndexedDB::CursorDirection::Next, IndexedDB::CursorType::KeyAndValue, IDBDatabaseBackend::NormalTask, IDBKeyRangeData());

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to populate indexes in database");
        return false;
    }

    m_cursors.set(cursor->identifier(), cursor);

    std::unique_ptr<JSLockHolder> locker;
    while (!cursor->currentKey().isNull()) {
        const IDBKeyData& key = cursor->currentKey();
        const Vector<uint8_t>& valueBuffer = cursor->currentValueBuffer();

        if (!m_globalObject) {
            ASSERT(!m_vm);
            m_vm = VM::create();
            locker = std::make_unique<JSLockHolder>(m_vm.get());
            m_globalObject.set(*m_vm, JSGlobalObject::create(*m_vm, JSGlobalObject::createStructure(*m_vm, jsNull())));
        }

        if (!locker)
            locker = std::make_unique<JSLockHolder>(m_vm.get());

        Deprecated::ScriptValue value = deserializeIDBValueBuffer(m_globalObject->globalExec(), Vector<uint8_t>(valueBuffer), true);
        Vector<IDBKeyData> indexKeys;
        generateIndexKeysForValue(m_globalObject->globalExec(), metadata, value, indexKeys);

        for (auto& indexKey : indexKeys) {
            if (!uncheckedPutIndexRecord(objectStoreID, metadata.id, key, indexKey)) {
                LOG_ERROR("Unable to put index record for newly created index");
                return false;
            }
        }

        if (!cursor->advance(1)) {
            LOG_ERROR("Error advancing cursor while indexing existing records for new index.");
            return false;
        }
    }

    transaction->closeCursor(*cursor);

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to delete index without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to delete index during a non-version-change transaction");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexInfo WHERE id = ? AND objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, indexID) != SQLITE_OK
            || sql.bindInt64(2, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index id %" PRIi64 " from IndexInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE indexID = ? AND objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, indexID) != SQLITE_OK
            || sql.bindInt64(2, objectStoreID) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete index records for index id %" PRIi64 " from IndexRecords table (%i) - %s", indexID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::generateKeyNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t& generatedKey)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    // The IndexedDatabase spec defines the max key generator value as 2^53;
    static const int64_t maxGeneratorValue = 9007199254740992LL;

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to generate key in database without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to generate key in database during read-only transaction");
        return false;
    }

    int64_t currentValue;
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT currentKey FROM KeyGenerators WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK) {
            LOG_ERROR("Could not delete index id %" PRIi64 " from IndexInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
        int result = sql.step();
        if (result != SQLITE_ROW) {
            LOG_ERROR("Could not retreive key generator value for object store, but it should be there.");
            return false;
        }

        currentValue = sql.getColumnInt64(0);
    }

    if (currentValue < 0 || currentValue > maxGeneratorValue)
        return false;

    generatedKey = currentValue + 1;
    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::updateKeyGeneratorNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t keyNumber, bool)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to update key generator in database without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to update key generator in database during read-only transaction");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO KeyGenerators VALUES (?, ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindInt64(2, keyNumber) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not update key generator value (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::keyExistsInObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyData& keyData, bool& keyExists)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    keyExists = false;

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to see if key exists in objectstore without established, in-progress transaction");
        return false;
    }

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to check for existence");
        return false;
    }

    SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT key FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT) LIMIT 1;"));
    if (sql.prepare() != SQLITE_OK
        || sql.bindInt64(1, objectStoreID) != SQLITE_OK
        || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    int sqlResult = sql.step();
    if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
        keyExists = false;
        return true;
    }

    if (sqlResult != SQLITE_ROW) {
        // There was an error fetching the record from the database.
        LOG_ERROR("Could not check if key exists in object store (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    keyExists = true;
    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyData& keyData, const uint8_t* valueBuffer, size_t valueSize)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to put a record into database without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to put a record into database during read-only transaction");
        return false;
    }

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(keyData);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in the database");
        return false;
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO Records VALUES (?, CAST(? AS TEXT), ?);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql.bindBlob(3, valueBuffer, valueSize) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not put record for object store %" PRIi64 " in Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::putIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyData& keyValue, const IDBKeyData& indexKey)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to put index record into database without an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to put index record into database during read-only transaction");
        return false;
    }

    return uncheckedPutIndexRecord(objectStoreID, indexID, keyValue, indexKey);
}

bool UniqueIDBDatabaseBackingStoreSQLite::uncheckedPutIndexRecord(int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyData& keyValue, const WebCore::IDBKeyData& indexKey)
{
    RefPtr<SharedBuffer> indexKeyBuffer = serializeIDBKeyData(indexKey);
    if (!indexKeyBuffer) {
        LOG_ERROR("Unable to serialize index key to be stored in the database");
        return false;
    }

    RefPtr<SharedBuffer> valueBuffer = serializeIDBKeyData(keyValue);
    if (!valueBuffer) {
        LOG_ERROR("Unable to serialize the value to be stored in the database");
        return false;
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IndexRecords VALUES (?, ?, CAST(? AS TEXT), CAST(? AS TEXT));"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, indexID) != SQLITE_OK
            || sql.bindInt64(2, objectStoreID) != SQLITE_OK
            || sql.bindBlob(3, indexKeyBuffer->data(), indexKeyBuffer->size()) != SQLITE_OK
            || sql.bindBlob(4, valueBuffer->data(), valueBuffer->size()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not put index record for index %" PRIi64 " in object store %" PRIi64 " in Records table (%i) - %s", indexID, objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::getIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, IndexedDB::CursorType cursorType, IDBGetResult& result)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get count from database without an established, in-progress transaction");
        return false;
    }

    SQLiteIDBCursor* cursor = transaction->openCursor(objectStoreID, indexID, IndexedDB::CursorDirection::Next, cursorType, IDBDatabaseBackend::NormalTask, keyRangeData);

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index get in database");
        return false;
    }

    // Even though we're only using this cursor locally, add it to our cursor set.
    m_cursors.set(cursor->identifier(), cursor);

    if (cursorType == IndexedDB::CursorType::KeyOnly)
        result = IDBGetResult(cursor->currentPrimaryKey());
    else {
        result = IDBGetResult(SharedBuffer::create(cursor->currentValueBuffer().data(), cursor->currentValueBuffer().size()));
        result.setKeyData(cursor->currentPrimaryKey());
    }

    // Closing the cursor will destroy the cursor object and remove it from our cursor set.
    transaction->closeCursor(*cursor);

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData& keyData)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get count from database without an established, in-progress transaction");
        return false;
    }

    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to delete range from a read-only transaction");
        return false;
    }

    return deleteRecord(*transaction, objectStoreID, keyData);
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRangeData& keyRangeData)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get count from database without an established, in-progress transaction");
        return false;
    }

    if (transaction->mode() == IndexedDB::TransactionMode::ReadOnly) {
        LOG_ERROR("Attempt to delete range from a read-only transaction");
        return false;
    }

    // If the range to delete is exactly one key we can delete it right now.
    if (keyRangeData.isExactlyOneKey()) {
        if (!deleteRecord(*transaction, objectStoreID, keyRangeData.lowerKey)) {
            LOG_ERROR("Failed to delete record for key '%s'", keyRangeData.lowerKey.loggingString().utf8().data());
            return false;
        }
        return true;
    }

    // Otherwise the range might span multiple keys so we collect them with a cursor.
    SQLiteIDBCursor* cursor = transaction->openCursor(objectStoreID, IDBIndexMetadata::InvalidId, IndexedDB::CursorDirection::Next, IndexedDB::CursorType::KeyAndValue, IDBDatabaseBackend::NormalTask, keyRangeData);

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to perform index get in database");
        return false;
    }

    m_cursors.set(cursor->identifier(), cursor);

    Vector<IDBKeyData> keys;
    do
        keys.append(cursor->currentKey());
    while (cursor->advance(1));

    bool cursorDidError = cursor->didError();

    // closeCursor() will remove the cursor from m_cursors and delete the cursor object
    transaction->closeCursor(*cursor);

    if (cursorDidError) {
        LOG_ERROR("Unable to iterate over object store to deleteRange in database");
        return false;
    }

    for (auto& key : keys) {
        if (!deleteRecord(*transaction, objectStoreID, key)) {
            LOG_ERROR("Failed to delete record for key '%s'", key.loggingString().utf8().data());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteRecord(SQLiteIDBTransaction& transaction, int64_t objectStoreID, const WebCore::IDBKeyData& key)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(key);
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKeyData to be removed from the database");
        return false;
    }

    // Delete record from object store
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"));

        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete record from indexes store
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ? AND value = CAST(? AS TEXT);"));

        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            LOG_ERROR("Could not delete record from indexes for object store %" PRIi64 " (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::getKeyRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKey& key, RefPtr<SharedBuffer>& result)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to put a record into database without an established, in-progress transaction");
        return false;
    }

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(IDBKeyData(&key));
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in the database");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM Records WHERE objectStoreID = ? AND key = CAST(? AS TEXT);"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLITE_OK) {
            LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        int sqlResult = sql.step();
        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There was no record for the key in the database.
            return true;
        }
        if (sqlResult != SQLITE_ROW) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        Vector<char> buffer;
        sql.getColumnBlobAsVector(0, buffer);
        result = SharedBuffer::create(static_cast<const char*>(buffer.data()), buffer.size());
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::getKeyRangeRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRange& keyRange, RefPtr<SharedBuffer>& result, RefPtr<IDBKey>& resultKey)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to put a record into database without an established, in-progress transaction");
        return false;
    }

    RefPtr<SharedBuffer> lowerBuffer = serializeIDBKeyData(IDBKeyData(keyRange.lower().get()));
    if (!lowerBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in the database");
        return false;
    }

    RefPtr<SharedBuffer> upperBuffer = serializeIDBKeyData(IDBKeyData(keyRange.upper().get()));
    if (!upperBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in the database");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"));
        if (sql.prepare() != SQLITE_OK
            || sql.bindInt64(1, objectStoreID) != SQLITE_OK
            || sql.bindBlob(2, lowerBuffer->data(), lowerBuffer->size()) != SQLITE_OK
            || sql.bindBlob(3, upperBuffer->data(), upperBuffer->size()) != SQLITE_OK) {
            LOG_ERROR("Could not get key range record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        int sqlResult = sql.step();

        if (sqlResult == SQLITE_OK || sqlResult == SQLITE_DONE) {
            // There was no record for the key in the database.
            return true;
        }
        if (sqlResult != SQLITE_ROW) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %" PRIi64 " from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        Vector<char> buffer;
        sql.getColumnBlobAsVector(0, buffer);
        result = SharedBuffer::create(static_cast<const char*>(buffer.data()), buffer.size());
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, int64_t& count)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to get count from database without an established, in-progress transaction");
        return false;
    }

    SQLiteIDBCursor* cursor = transaction->openCursor(objectStoreID, indexID, IndexedDB::CursorDirection::Next, IndexedDB::CursorType::KeyOnly, IDBDatabaseBackend::NormalTask, keyRangeData);

    if (!cursor) {
        LOG_ERROR("Cannot open cursor to get count in database");
        return false;
    }

    m_cursors.set(cursor->identifier(), cursor);

    count = 0;
    while (cursor->advance(1))
        ++count;

    transaction->closeCursor(*cursor);

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange, int64_t& cursorID, IDBKeyData& key, IDBKeyData& primaryKey, Vector<uint8_t>& valueBuffer)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to open a cursor in database without an established, in-progress transaction");
        return false;
    }

    SQLiteIDBCursor* cursor = transaction->openCursor(objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRange);
    if (!cursor)
        return false;

    m_cursors.set(cursor->identifier(), cursor);
    cursorID = cursor->identifier().id();
    key = cursor->currentKey();
    primaryKey = cursor->currentPrimaryKey();
    valueBuffer = cursor->currentValueBuffer();

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::advanceCursor(const IDBIdentifier& cursorIdentifier, uint64_t count, IDBKeyData& key, IDBKeyData& primaryKey, Vector<uint8_t>& valueBuffer)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBCursor* cursor = m_cursors.get(cursorIdentifier);
    if (!cursor) {
        LOG_ERROR("Attempt to advance a cursor that doesn't exist");
        return false;
    }
    if (!cursor->transaction() || !cursor->transaction()->inProgress()) {
        LOG_ERROR("Attempt to advance a cursor without an established, in-progress transaction");
        return false;
    }

    if (!cursor->advance(count)) {
        LOG_ERROR("Attempt to advance cursor %" PRIi64 " steps failed", count);
        return false;
    }

    key = cursor->currentKey();
    primaryKey = cursor->currentPrimaryKey();
    valueBuffer = cursor->currentValueBuffer();

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::iterateCursor(const IDBIdentifier& cursorIdentifier, const IDBKeyData& targetKey, IDBKeyData& key, IDBKeyData& primaryKey, Vector<uint8_t>& valueBuffer)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBCursor* cursor = m_cursors.get(cursorIdentifier);
    if (!cursor) {
        LOG_ERROR("Attempt to iterate a cursor that doesn't exist");
        return false;
    }
    if (!cursor->transaction() || !cursor->transaction()->inProgress()) {
        LOG_ERROR("Attempt to iterate a cursor without an established, in-progress transaction");
        return false;
    }

    if (!cursor->iterate(targetKey)) {
        LOG_ERROR("Attempt to iterate cursor failed");
        return false;
    }

    key = cursor->currentKey();
    primaryKey = cursor->currentPrimaryKey();
    valueBuffer = cursor->currentValueBuffer();

    return true;
}

void UniqueIDBDatabaseBackingStoreSQLite::notifyCursorsOfChanges(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to notify cursors of changes in database without an established, in-progress transaction");
        return;
    }

    transaction->notifyCursorsOfChanges(objectStoreID);
}

int UniqueIDBDatabaseBackingStoreSQLite::idbKeyCollate(int aLength, const void* aBuffer, int bLength, const void* bBuffer)
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

void UniqueIDBDatabaseBackingStoreSQLite::unregisterCursor(SQLiteIDBCursor* cursor)
{
    ASSERT(m_cursors.contains(cursor->identifier()));
    m_cursors.remove(cursor->identifier());
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
