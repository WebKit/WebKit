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
#include "SQLiteIDBTransaction.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
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

std::unique_ptr<WebCore::IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::createAndPopulateInitialMetadata()
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
            IDBObjectStoreMetadata metadata;
            metadata.id = sql.getColumnInt64(0);
            metadata.name = sql.getColumnText(1);

            int keyPathSize;
            const uint8_t* keyPathBuffer = static_cast<const uint8_t*>(sql.getColumnBlob(2, keyPathSize));

            std::unique_ptr<IDBKeyPath> keyPath = deserializeIDBKeyPath(keyPathBuffer, keyPathSize);

            if (!keyPath) {
                LOG_ERROR("Unable to extract key path metadata from database");
                return nullptr;
            }

            metadata.keyPath = *keyPath;
            metadata.autoIncrement = sql.getColumnInt(3);
            metadata.maxIndexId = sql.getColumnInt64(4);

            result = sql.step();
        }

        if (result != SQLResultDone) {
            LOG_ERROR("Error fetching object store metadata from database on disk");
            return nullptr;
        }
    }

    // FIXME: Once we save indexes we need to extract their metadata, also.

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

    std::unique_ptr<IDBDatabaseMetadata> metadata = extractExistingMetadata();
    if (!metadata)
        metadata = createAndPopulateInitialMetadata();

    if (!metadata)
        LOG_ERROR("Unable to establish IDB database at path '%s'", dbFilename.utf8().data());

    // The database id is a runtime concept and doesn't need to be stored in the metadata database.
    metadata->id = generateDatabaseId();

    return metadata;
}

bool UniqueIDBDatabaseBackingStoreSQLite::establishTransaction(const IDBTransactionIdentifier& identifier, const Vector<int64_t>&, WebCore::IndexedDB::TransactionMode mode)
{
    ASSERT(!isMainThread());

    if (!m_transactions.add(identifier, SQLiteIDBTransaction::create(identifier, mode)).isNewEntry) {
        LOG_ERROR("Attempt to establish transaction identifier that already exists");
        return false;
    }

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::beginTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to begin a transaction that hasn't been established");
        return false;
    }

    return transaction->begin(*m_sqliteDB);
}

bool UniqueIDBDatabaseBackingStoreSQLite::commitTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to commit a transaction that hasn't been established");
        return false;
    }

    return transaction->commit();
}

bool UniqueIDBDatabaseBackingStoreSQLite::resetTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to reset a transaction that hasn't been established");
        return false;
    }

    return transaction->reset();
}

bool UniqueIDBDatabaseBackingStoreSQLite::rollbackTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
    if (!transaction) {
        LOG_ERROR("Attempt to rollback a transaction that hasn't been established");
        return false;
    }

    return transaction->rollback();
}

bool UniqueIDBDatabaseBackingStoreSQLite::changeDatabaseVersion(const IDBTransactionIdentifier& identifier, uint64_t newVersion)
{
    ASSERT(!isMainThread());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
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

bool UniqueIDBDatabaseBackingStoreSQLite::createObjectStore(const IDBTransactionIdentifier& identifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!isMainThread());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
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

    return true;
}

bool UniqueIDBDatabaseBackingStoreSQLite::deleteObjectStore(const IDBTransactionIdentifier& identifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
    ASSERT(m_sqliteDB);
    ASSERT(m_sqliteDB->isOpen());

    SQLiteIDBTransaction* transaction = m_transactions.get(identifier);
    if (!transaction || !transaction->inProgress()) {
        LOG_ERROR("Attempt to change database version with an established, in-progress transaction");
        return false;
    }
    if (transaction->mode() != IndexedDB::TransactionMode::VersionChange) {
        LOG_ERROR("Attempt to change database version during a non version-change transaction");
        return false;
    }

    {
        SQLiteStatement sql(*m_sqliteDB, ASCIILiteral("DELETE FROM ObjectStoreInfo WHERE id = ?;"));
        if (sql.prepare() != SQLResultOk
            || sql.bindInt64(1, objectStoreID) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not delete object store id %lli from ObjectStoreInfo table (%i) - %s", objectStoreID, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
            return false;
        }
    }
    {
        // FIXME: Execute SQL here to drop all records and indexes related to this object store.
    }

    return true;
}


} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
