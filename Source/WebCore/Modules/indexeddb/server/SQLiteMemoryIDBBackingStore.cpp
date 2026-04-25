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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SQLiteMemoryIDBBackingStore);

SQLiteMemoryIDBBackingStore::SQLiteMemoryIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : SQLiteIDBBackingStore(identifier, emptyString())
{
}

SQLiteMemoryIDBBackingStore::~SQLiteMemoryIDBBackingStore() = default;

IDBError SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo(IDBDatabaseInfo& info)
{
    LOG(IndexedDB, "SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo - database %s (in-memory)", identifier().databaseName().utf8().data());

    if (databaseInfo()) {
        info = *databaseInfo();
        return IDBError { };
    }

    // Open SQLite in-memory database using the special ":memory:" path
    setSqliteDB(makeUnique<SQLiteDatabase>());
    if (CheckedPtr db = sqliteDB(); !db->open(SQLiteDatabase::inMemoryPath())) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - SQLiteMemoryIDBBackingStore::getOrEstablishDatabaseInfo: Failed to open in-memory database (%d) - %s", this, db->lastError(), db->lastErrorMsg());
        db = nullptr;
        closeSQLiteDB();
    }

    if (!sqliteDB())
        return IDBError { ExceptionCode::UnknownError, "Unable to open in-memory database"_s };

    {
        CheckedRef db = *sqliteDB();
        db->disableThreadingChecks();

        // Note: WAL mode and automatic truncation are not relevant for in-memory databases
        // as they are file-based features. In-memory databases use default journaling.

        // Use a smaller cache size for private browsing to reduce memory footprint.
        // Negative value specifies size in KB rather than number of pages.
        if (!db->executeCommand("PRAGMA cache_size = -512;"_s))
            LOG_ERROR("SQLite in-memory database could not set cache_size");

        // Set up the IDBKEY collation function for proper IndexedDB key sorting
        db->setCollationFunction("IDBKEY"_s, [](int aLength, const void* a, int bLength, const void* b) {
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

    // Create blob tables for schema compatibility with the base class.
    // Note: Blobs are not actually supported in ephemeral sessions - see webkit.org/b/156347.
    // The blob rejection happens at the SerializedScriptValue level, so these tables stay empty.
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

    auto databaseInfoResult = result.value() ? std::exchange(result.value(), nullptr) : createAndPopulateInitialDatabaseInfo();
    if (!databaseInfoResult) {
        LOG_ERROR("Unable to establish IDB in-memory database");
        closeSQLiteDB();
        return IDBError { ExceptionCode::UnknownError, "Unable to establish IDB in-memory database"_s };
    }

    setDatabaseInfo(WTF::move(databaseInfoResult));
    info = *databaseInfo();
    return IDBError { };
}

} // namespace IDBServer
} // namespace WebCore
