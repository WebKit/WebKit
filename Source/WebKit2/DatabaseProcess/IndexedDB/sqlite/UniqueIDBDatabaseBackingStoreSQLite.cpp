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
#include "SQLiteIDBCursor.h"
#include "SQLiteIDBTransaction.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRange.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

// Current version of the metadata schema being used in the metadata database.
static const int currentMetadataVersion = 1;

static int64_t generateDatabaseId()
{
    static int64_t databaseID = 0;

    ASSERT(!isMainThread());
    return ++databaseID;
}

UniqueIDBDatabaseBackingStoreSQLite::UniqueIDBDatabaseBackingStoreSQLite(const UniqueIDBDatabaseIdentifier& identifier, const String& databaseDirectory)
    : m_identifier(identifier)
    , m_absoluteDatabaseDirectory(databaseDirectory)
{
    // The backing store is meant to be created and used entirely on a background thread.
    ASSERT(!isMainThread());
}

UniqueIDBDatabaseBackingStoreSQLite::~UniqueIDBDatabaseBackingStoreSQLite()
{
    ASSERT(!isMainThread());
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::createAndPopulateInitialMetadata()
{
    ASSERT(!isMainThread());
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

    if (!m_sqliteDB->executeCommand("CREATE TABLE IndexInfo (id INTEGER PRIMARY KEY NOT NULL ON CONFLICT FAIL, name TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, keyPath BLOB NOT NULL ON CONFLICT FAIL, isUnique INTEGER NOT NULL ON CONFLICT FAIL, multiEntry INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IndexInfo table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE Records (objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create Records table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        m_sqliteDB = nullptr;
        return nullptr;
    }

    if (!m_sqliteDB->executeCommand("CREATE TABLE IndexRecords (indexID INTEGER NOT NULL ON CONFLICT FAIL, objectStoreID INTEGER NOT NULL ON CONFLICT FAIL, key TEXT COLLATE IDBKEY NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create Records table in database (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt(1, currentMetadataVersion) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not insert database metadata version into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            m_sqliteDB = nullptr;
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseName', ?);"));
        if (sql.prepare() != SQLResultOk
            || sql.bindText(1, m_identifier.databaseName()) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not insert database name into IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            m_sqliteDB = nullptr;
            return nullptr;
        }
    }
    {
        // Database versions are defined to be a uin64_t in the spec but sqlite3 doesn't support native binding of unsigned integers.
        // Therefore we'll store the version as a String.
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IDBDatabaseInfo VALUES ('DatabaseVersion', ?);"));
        if (sql.prepare() != SQLResultOk
            || sql.bindText(1, String::number(0)) != SQLResultOk
            || sql.step() != SQLResultDone) {
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
    metadata->version = 0;
    metadata->maxObjectStoreId = 1;

    return metadata;
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::extractExistingMetadata()
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk)
            return nullptr;

        int result = sql.step();
        while (result == SQLResultRow) {
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

        if (result != SQLResultDone) {
            LOG_ERROR("Error fetching object store metadata from database on disk");
            return nullptr;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT id, name, objectStoreID, keyPath, isUnique, multiEntry FROM IndexInfo;"));
        if (sql.prepare() != SQLResultOk)
            return nullptr;

        int result = sql.step();
        while (result == SQLResultRow) {
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

        if (result != SQLResultDone) {
            LOG_ERROR("Error fetching index metadata from database on disk");
            return nullptr;
        }
    }

    return metadata;
}

std::unique_ptr<SQLiteDatabase> UniqueIDBDatabaseBackingStoreSQLite::openSQLiteDatabaseAtPath(const String& path)
{
    ASSERT(!isMainThread());

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
    ASSERT(!isMainThread());

    String dbFilename = pathByAppendingComponent(m_absoluteDatabaseDirectory, "IndexedDB.sqlite3");
    m_sqliteDB = openSQLiteDatabaseAtPath(dbFilename);
    if (!m_sqliteDB)
        return nullptr;

    m_sqliteDB->setCollationFunction("IDBKEY", [this](int aLength, const void* a, int bLength, const void* b) {
        return idbKeyCollate(aLength, a, bLength, b);
    });

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
    ASSERT(!isMainThread());

    if (!m_transactions.add(transactionIdentifier, SQLiteIDBTransaction::create(*this, transactionIdentifier, mode)).isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return false;
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::beginTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to begin a transaction that hasn't been established");
        return false;
    }

    return transaction->begin(*m_sqliteDB);
}

bool UniqueIDBDatabaseBackingStoreSQLite::commitTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return false;
    }

    return transaction->commit();
}

bool UniqueIDBDatabaseBackingStoreSQLite::resetTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());

    std::unique_ptr<SQLiteIDBTransaction> transaction = m_transactions.take(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to reset a transaction that hasn't been established");
        return false;
    }

    return transaction->reset();
}

bool UniqueIDBDatabaseBackingStoreSQLite::rollbackTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        LOG_ERROR("Attempt to rollback a transaction that hasn't been established");
        return false;
    }

    return transaction->rollback();
}

bool UniqueIDBDatabaseBackingStoreSQLite::changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindText(1, String::number(newVersion)) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not update database version in IDBDatabaseInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::createObjectStore(const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, metadata.id) != SQLResultOk
            || sql.bindText(2, metadata.name) != SQLResultOk
            || sql.bindBlob(3, keyPathBlob->data(), keyPathBlob->size()) != SQLResultOk
            || sql.bindInt(4, metadata.autoIncrement) != SQLResultOk
            || sql.bindInt64(5, metadata.maxIndexId) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not add object store '%s' to ObjectStoreInfo table (%i) - %s", metadata.name.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO KeyGenerators VALUES (?, 0);"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, metadata.id) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not seed initial key generator value for ObjectStoreInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete object store id %lli from ObjectStoreInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete the ObjectStore's key generator record if there is one.
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM KeyGenerators WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete object store from KeyGenerators table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM Records WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete records for object store %lli (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated Indexes
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexInfo WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete index from IndexInfo table (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    // Delete all associated Index records
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete index records(%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete records from object store id %lli (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete records from index record store id %lli (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata)
{
    ASSERT(!isMainThread());
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
    if (sql.prepare() != SQLResultOk
        || sql.bindInt64(1, metadata.id) != SQLResultOk
        || sql.bindText(2, metadata.name) != SQLResultOk
        || sql.bindInt64(3, objectStoreID) != SQLResultOk
        || sql.bindBlob(4, keyPathBlob->data(), keyPathBlob->size()) != SQLResultOk
        || sql.bindInt(5, metadata.unique) != SQLResultOk
        || sql.bindInt(6, metadata.multiEntry) != SQLResultOk
        || sql.step() != SQLResultDone) {
        LOG_ERROR("Could not add index '%s' to IndexInfo table (%i) - %s", metadata.name.utf8().data(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
        return false;
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, indexID) != SQLResultOk
            || sql.bindInt64(2, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete index id %lli from IndexInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM IndexRecords WHERE indexID = ? AND objectStoreID = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, indexID) != SQLResultOk
            || sql.bindInt64(2, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete index records for index id %lli from IndexRecords table (%i) - %s", indexID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::generateKeyNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t& generatedKey)
{
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk) {
            LOG_ERROR("Could not delete index id %lli from IndexInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
        int result = sql.step();
        if (result != SQLResultRow) {
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
    ASSERT(!isMainThread());
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
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.bindInt64(2, keyNumber) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not update key generator value (%i) - %s", m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::keyExistsInObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKey&, bool& keyExists)
{
    // FIXME: When Get support is implemented, we need to implement this also (<rdar://problem/15779644>)
    return false;
}

bool UniqueIDBDatabaseBackingStoreSQLite::putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKey& key, const uint8_t* valueBuffer, size_t valueSize)
{
    ASSERT(!isMainThread());
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

    RefPtr<SharedBuffer> keyBuffer = serializeIDBKeyData(IDBKeyData(&key));
    if (!keyBuffer) {
        LOG_ERROR("Unable to serialize IDBKey to be stored in the database");
        return false;
    }
    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO Records VALUES (?, CAST(? AS TEXT), ?);"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLResultOk
            || sql.bindBlob(3, valueBuffer, valueSize) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not put record for object store %lli in Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::putIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyData& keyValue, const IDBKeyData& indexKey)
{
    ASSERT(!isMainThread());
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
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("INSERT INTO IndexRecords VALUES (?, ?, CAST(? AS TEXT), ?);"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, indexID) != SQLResultOk
            || sql.bindInt64(2, objectStoreID) != SQLResultOk
            || sql.bindBlob(3, indexKeyBuffer->data(), indexKeyBuffer->size()) != SQLResultOk
            || sql.bindBlob(4, valueBuffer->data(), valueBuffer->size()) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not put index record for index %lli in object store %lli in Records table (%i) - %s", indexID, objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::getKeyRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKey& key, RefPtr<SharedBuffer>& result)
{
    ASSERT(!isMainThread());
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
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM Records WHERE objectStoreID = ? AND key = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.bindBlob(2, keyBuffer->data(), keyBuffer->size()) != SQLResultOk) {
            LOG_ERROR("Could not get record from object store %lli from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        int sqlResult = sql.step();
        if (sqlResult == SQLResultOk || sqlResult == SQLResultDone) {
            // There was no record for the key in the database.
            return true;
        }
        if (sqlResult != SQLResultRow) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %lli from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
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
    ASSERT(!isMainThread());
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
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("SELECT value FROM Records WHERE objectStoreID = ? AND key >= ? AND key <= ? ORDER BY key;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.bindBlob(2, lowerBuffer->data(), lowerBuffer->size()) != SQLResultOk
            || sql.bindBlob(3, upperBuffer->data(), upperBuffer->size()) != SQLResultOk) {
            LOG_ERROR("Could not get key range record from object store %lli from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }

        int sqlResult = sql.step();

        if (sqlResult == SQLResultOk || sqlResult == SQLResultDone) {
            // There was no record for the key in the database.
            return true;
        }
        if (sqlResult != SQLResultRow) {
            // There was an error fetching the record from the database.
            LOG_ERROR("Could not get record from object store %lli from Records table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
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
    ASSERT(!isMainThread());
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

bool UniqueIDBDatabaseBackingStoreSQLite::openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange, int64_t& cursorID)
{
    ASSERT(!isMainThread());
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

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::advanceCursor(const IDBIdentifier& cursorIdentifier, uint64_t count, IDBKeyData& key, IDBKeyData& primaryKey, Vector<char>& value)
{
    ASSERT(!isMainThread());
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
        LOG_ERROR("Attempt to advance cursor %lli steps failed", count);
        return false;
    }

    key = cursor->currentKey();
    primaryKey = cursor->currentPrimaryKey();
    value = cursor->currentValue();

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::iterateCursor(const IDBIdentifier& cursorIdentifier, const IDBKeyData& targetKey, IDBKeyData& key, IDBKeyData& primaryKey, Vector<char>& value)
{
    ASSERT(!isMainThread());
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
    value = cursor->currentValue();

    return true;
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
