/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "SQLiteMemoryIDBBackingStore.h"

#include "IDBDatabaseInfo.h"
#include "IDBKeyData.h"
#include "IDBSerialization.h"
#include "Logging.h"
#include "SQLiteDatabase.h"
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SQLiteMemoryIDBBackingStore);

static String createTemporaryBlobDirectory()
{
#if PLATFORM(PLAYSTATION)
    auto directory = String();
#elif PLATFORM(WIN)
    auto directory = FileSystem::createTemporaryDirectory();
#else
    auto directory = FileSystem::createTemporaryDirectory("IndexedDB-Blobs"_s);
#endif
    if (directory.isEmpty()) {
        RELEASE_LOG_ERROR(IndexedDB, "SQLiteMemoryIDBBackingStore: FileSystem::createTemporaryDirectory returned empty string");
        return emptyString();
    }
    RELEASE_LOG(IndexedDB, "SQLiteMemoryIDBBackingStore: Created temporary blob directory at %s", directory.utf8().data());
    return directory;
}

SQLiteMemoryIDBBackingStore::SQLiteMemoryIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : SQLiteIDBBackingStore(identifier, createTemporaryBlobDirectory())
{
    if (m_databaseDirectory.isEmpty()) {
        RELEASE_LOG_ERROR(IndexedDB, "SQLiteMemoryIDBBackingStore: Constructor - no temporary directory available for blobs!");
        return;
    }

    RELEASE_LOG(IndexedDB, "SQLiteMemoryIDBBackingStore: Constructor - using blob directory: %s", m_databaseDirectory.utf8().data());

    // Verify the directory actually exists
    auto fileType = FileSystem::fileType(m_databaseDirectory);
    if (!fileType)
        RELEASE_LOG_ERROR(IndexedDB, "SQLiteMemoryIDBBackingStore: Temporary blob directory does not exist: %s", m_databaseDirectory.utf8().data());
    else if (*fileType != FileSystem::FileType::Directory)
        RELEASE_LOG_ERROR(IndexedDB, "SQLiteMemoryIDBBackingStore: Blob path is not a directory: %s", m_databaseDirectory.utf8().data());
    else
        RELEASE_LOG(IndexedDB, "SQLiteMemoryIDBBackingStore: Temporary blob directory verified: %s", m_databaseDirectory.utf8().data());
}

SQLiteMemoryIDBBackingStore::~SQLiteMemoryIDBBackingStore()
{
    // Clean up temporary blob directory
    if (!m_databaseDirectory.isEmpty()) {
        if (!FileSystem::deleteNonEmptyDirectory(m_databaseDirectory))
            RELEASE_LOG_ERROR(IndexedDB, "SQLiteMemoryIDBBackingStore: Failed to delete temporary blob directory at %s", m_databaseDirectory.utf8().data());
    }
}

IDBError SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo(IDBDatabaseInfo& info)
{
    LOG(IndexedDB, "SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo - database %s (in-memory)", m_identifier.databaseName().utf8().data());

    if (m_databaseInfo) {
        info = *m_databaseInfo;
        return IDBError { };
    }

    // Open SQLite in-memory database using the special ":memory:" path
    m_sqliteDB = makeUnique<SQLiteDatabase>();
    if (CheckedPtr sqliteDB = m_sqliteDB.get(); !sqliteDB->open(SQLiteDatabase::inMemoryPath())) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo: Failed to open in-memory database (%d) - %s", this, sqliteDB->lastError(), sqliteDB->lastErrorMsg());
        sqliteDB = nullptr;
        closeSQLiteDB();
    }

    if (!m_sqliteDB)
        return IDBError { ExceptionCode::UnknownError, "Unable to open in-memory database"_s };

    {
        CheckedRef sqliteDB = *m_sqliteDB;
        sqliteDB->disableThreadingChecks();

        // Note: WAL mode and automatic truncation are not relevant for in-memory databases
        // as they are file-based features. In-memory databases use default journaling.

        // Set up the IDBKEY collation function for proper IndexedDB key sorting
        sqliteDB->setCollationFunction("IDBKEY"_s, [](int aLength, const void* a, int bLength, const void* b) {
            IDBKeyData aKey, bKey;
            if (!deserializeIDBKeyData(unsafeMakeSpan(static_cast<const uint8_t*>(a), aLength), aKey)) {
                LOG_ERROR("Unable to deserialize key A in collation function.");
                return 1;
            }
            if (!deserializeIDBKeyData(unsafeMakeSpan(static_cast<const uint8_t*>(b), bLength), bKey)) {
                LOG_ERROR("Unable to deserialize key B in collation function.");
                return -1;
            }

            auto comparison = aKey <=> bKey;
            if (is_eq(comparison))
                return 0;
            if (is_lt(comparison))
                return -1;
            return 1;
        });
    }

    // Create the required tables
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

    // Create blob tables to support blobs stored in temporary files
    error = ensureValidBlobTables();
    if (!error.isNull()) {
        closeSQLiteDB();
        return error;
    }

    auto result = extractExistingDatabaseInfo();
    if (!result) {
        ASSERT(!result.error().isNull());
        closeSQLiteDB();
        return result.error();
    }

    auto databaseInfo = result.value() ? std::exchange(result.value(), nullptr) : createAndPopulateInitialDatabaseInfo();
    if (!databaseInfo) {
        LOG_ERROR("Unable to establish IDB in-memory database");
        closeSQLiteDB();
        return IDBError { ExceptionCode::UnknownError, "Unable to establish IDB in-memory database"_s };
    }

    m_databaseInfo = WTFMove(databaseInfo);
    info = *m_databaseInfo;
    return IDBError { };
}

} // namespace IDBServer
} // namespace WebCore
