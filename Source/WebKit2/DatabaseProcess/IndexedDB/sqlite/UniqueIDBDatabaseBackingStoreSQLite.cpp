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

std::unique_ptr<WebCore::IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::createAndPopulateInitialMetadata()
{
    ASSERT(!isMainThread());
    ASSERT(m_metadataDB);
    ASSERT(m_metadataDB->isOpen());

    if (!m_metadataDB->executeCommand("CREATE TABLE IDBDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IDBDatabaseInfo table in database (%i) - %s", m_metadataDB->lastError(), m_metadataDB->lastErrorMsg());
        m_metadataDB = nullptr;
        return nullptr;
    }

    {
        SQLiteStatement sql(*m_metadataDB, "INSERT INTO IDBDatabaseInfo VALUES ('MetadataVersion', ?);");
        if (sql.prepare() != SQLResultOk
            || sql.bindInt(1, currentMetadataVersion) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not insert database metadata version into IDBDatabaseInfo table (%i) - %s", m_metadataDB->lastError(), m_metadataDB->lastErrorMsg());
            m_metadataDB = nullptr;
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_metadataDB, "INSERT INTO IDBDatabaseInfo VALUES ('DatabaseName', ?);");
        if (sql.prepare() != SQLResultOk
            || sql.bindText(1, m_identifier.databaseName()) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not insert database name into IDBDatabaseInfo table (%i) - %s", m_metadataDB->lastError(), m_metadataDB->lastErrorMsg());
            m_metadataDB = nullptr;
            return nullptr;
        }
    }
    {
        // Database versions are defined to be a uin64_t in the spec but sqlite3 doesn't support native binding of unsigned integers.
        // Therefore we'll store the version as a String.
        SQLiteStatement sql(*m_metadataDB, "INSERT INTO IDBDatabaseInfo VALUES ('DatabaseVersion', ?);");
        if (sql.prepare() != SQLResultOk
            || sql.bindText(1, String::number(0)) != SQLResultOk
            || sql.step() != SQLResultDone) {
            LOG_ERROR("Could not insert default version into IDBDatabaseInfo table (%i) - %s", m_metadataDB->lastError(), m_metadataDB->lastErrorMsg());
            m_metadataDB = nullptr;
            return nullptr;
        }
    }

    // This initial metadata matches the default values we just put into the metadata database.
    auto metadata = std::make_unique<IDBDatabaseMetadata>();
    metadata->name = m_identifier.databaseName();
    metadata->version = 0;

    return metadata;
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::extractExistingMetadata()
{
    ASSERT(!isMainThread());
    ASSERT(m_metadataDB);

    if (!m_metadataDB->tableExists("IDBDatabaseInfo"))
        return nullptr;

    auto metadata = std::make_unique<IDBDatabaseMetadata>();
    {
        SQLiteStatement sql(*m_metadataDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'MetadataVersion';");
        if (sql.isColumnNull(0))
            return nullptr;
        metadata->version = sql.getColumnInt(0);
    }
    {
        SQLiteStatement sql(*m_metadataDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseName';");
        if (sql.isColumnNull(0))
            return nullptr;
        metadata->name = sql.getColumnText(0);
        if (metadata->name != m_identifier.databaseName()) {
            LOG_ERROR("Database name in the metadata database ('%s') does not match the expected name ('%s')", metadata->name.utf8().data(), m_identifier.databaseName().utf8().data());
            return nullptr;
        }
    }
    {
        SQLiteStatement sql(*m_metadataDB, "SELECT value FROM IDBDatabaseInfo WHERE key = 'DatabaseVersion';");
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

    // FIXME: Once we save ObjectStores and indexes we need to extract their metadata, also.
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

    return sqliteDatabase;
}

std::unique_ptr<IDBDatabaseMetadata> UniqueIDBDatabaseBackingStoreSQLite::getOrEstablishMetadata()
{
    ASSERT(!isMainThread());

    String metadataDBFilename = pathByAppendingComponent(m_absoluteDatabaseDirectory, "IDBMetadata.sqlite3");
    m_metadataDB = openSQLiteDatabaseAtPath(metadataDBFilename);
    if (!m_metadataDB)
        return nullptr;

    std::unique_ptr<IDBDatabaseMetadata> metadata = extractExistingMetadata();
    if (!metadata)
        metadata = createAndPopulateInitialMetadata();

    if (!metadata)
        LOG_ERROR("Unable to establish IDB database at path '%s'", metadataDBFilename.utf8().data());

    // The database id is a runtime concept and doesn't need to be stored in the metadata database.
    metadata->id = generateDatabaseId();

    return metadata;
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
