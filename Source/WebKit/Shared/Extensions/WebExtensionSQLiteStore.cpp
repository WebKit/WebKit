/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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
#include "WebExtensionSQLiteStore.h"

#include "Logging.h"
#include "WebExtensionSQLiteHelpers.h"
#include "WebExtensionSQLiteRow.h"
#include <sqlite3.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionSQLiteStore);

WebExtensionSQLiteStore::WebExtensionSQLiteStore(const String& uniqueIdentifier, const String& directory, bool useInMemoryDatabase)
    : m_uniqueIdentifier(uniqueIdentifier)
    , m_directory(URL(URL("file:"_s), directory))
    , m_queue(WorkQueue::create("com.apple.WebKit.WKWebExtensionSQLiteStore"_s))
    , m_useInMemoryDatabase(useInMemoryDatabase)
{
}

void WebExtensionSQLiteStore::close()
{
    if (!database())
        return;

    if (isMainRunLoop()) {
        m_queue->dispatchSync([db = RefPtr { database() }] {
            db->close();
        });

        return;
    }

    assertIsCurrent(queue());
    database()->close();
}

void WebExtensionSQLiteStore::deleteDatabase(CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    m_queue->dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto deleteDatabaseErrorMessage = protectedThis->deleteDatabase();
        WorkQueue::mainSingleton().dispatch([deleteDatabaseErrorMessage = crossThreadCopy(deleteDatabaseErrorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(deleteDatabaseErrorMessage);
        });
    });
}

// MARK: Database Management

void WebExtensionSQLiteStore::vacuum()
{
    assertIsCurrent(queue());
    ASSERT(m_database);

    DatabaseResult result = SQLiteDatabaseExecute(*m_database, "VACUUM"_s);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to vacuum database for extension %s: %s (%d)", m_uniqueIdentifier.utf8().data(), m_database->m_lastErrorMessage.data(), result);
}

bool WebExtensionSQLiteStore::openDatabaseIfNecessary(String& outErrorMessage, bool createIfNecessary)
{
    assertIsCurrent(queue());

    if (isDatabaseOpen()) {
        outErrorMessage = nullString();
        return true;
    }

    auto accessType = createIfNecessary ? WebExtensionSQLiteDatabase::AccessType::ReadWriteCreate : WebExtensionSQLiteDatabase::AccessType::ReadWrite;
    outErrorMessage = openDatabase(databaseURL(), accessType, true);
    return isDatabaseOpen();
}

String WebExtensionSQLiteStore::openDatabase(const URL& databaseURL, WebExtensionSQLiteDatabase::AccessType accessType, bool deleteDatabaseFileOnError)
{
    assertIsCurrent(queue());
    ASSERT(!isDatabaseOpen());

    bool usingDatabaseFile = !m_useInMemoryDatabase;

    if (usingDatabaseFile) {
        if (!FileSystem::makeAllDirectories(m_directory.fileSystemPath()) || FileSystem::fileType(m_directory.fileSystemPath()) != FileSystem::FileType::Directory) {
            RELEASE_LOG_ERROR(Extensions, "Failed to create extension storage directory for extension %s", m_uniqueIdentifier.utf8().data());
            return "Failed to create extension storage directory."_s;
        }
    }

    m_database = WebExtensionSQLiteDatabase::create(databaseURL, m_queue.copyRef());

    // FIXME: rdar://87898825 (unlimitedStorage: Allow the SQLite database to be opened as SQLiteDatabaseAccessTypeReadOnly if the request is to calculate storage size).
    RefPtr<API::Error> error;
    if (RefPtr db = m_database; !db->openWithAccessType(accessType, error)) {
        if (!error && accessType != WebExtensionSQLiteDatabase::AccessType::ReadWriteCreate) {
            // The file didn't exist and we were not asked to create it.
            m_database = nullptr;
            return nullString();
        }

        if (error)
            RELEASE_LOG_ERROR(Extensions, "Failed to open database for extension %s: %s", m_uniqueIdentifier.utf8().data(), error->localizedDescription().utf8().data());

        if (usingDatabaseFile && deleteDatabaseFileOnError)
            return deleteDatabaseFileAtURL(databaseURL, true);

        db->close();
        m_database = nullptr;

        return "Failed to open extension storage database."_s;
    }

    // Enable write-ahead logging to minimize the impact of SQLite's disk I/O.
    if (RefPtr db = m_database; !db->enableWAL(error)) {
        if (error)
            RELEASE_LOG_ERROR(Extensions, "Failed to enable write-ahead logging on database for extension %s: %s", m_uniqueIdentifier.utf8().data(), error->localizedDescription().utf8().data());

        if (usingDatabaseFile && deleteDatabaseFileOnError)
            return deleteDatabaseFileAtURL(databaseURL, true);

        db->close();
        m_database = nullptr;

        return "Failed to open extension storage database."_s;
    }

    return handleSchemaVersioning(deleteDatabaseFileOnError);
}

bool WebExtensionSQLiteStore::isDatabaseOpen()
{
    assertIsCurrent(queue());
    return !!m_database;
}

String WebExtensionSQLiteStore::deleteDatabaseFileAtURL(const URL& databaseURL, bool reopenDatabase)
{
    assertIsCurrent(queue());
    ASSERT(!m_useInMemoryDatabase);

    String errorMessage;
    if (isDatabaseOpen()) {
        if (RefPtr db = database(); db->close() != SQLITE_OK)
            errorMessage = "Failed to close extension storage database"_s;
        m_database = nullptr;
    }

    String databaseFilePath = databaseURL.fileSystemPath();
    static constexpr std::array<ASCIILiteral, 2> databaseFileSuffixes { "-shm"_s, "-wal"_s };

    // -shm and -wal files may not exist, so don't report errors for those.
    for (auto& suffix : databaseFileSuffixes)
        FileSystem::deleteFile(makeString(databaseFilePath, suffix));

    if (FileSystem::fileExists(databaseFilePath) && !FileSystem::deleteFile(databaseFilePath)) {
        RELEASE_LOG_ERROR(Extensions, "Failed to delete database for extension %s", m_uniqueIdentifier.utf8().data());
        return "Failed to delete extension storage database file."_s;
    }

    if (!reopenDatabase) {
        m_database = nullptr;
        return errorMessage;
    }

    // Only try to recover from errors opening the database by deleting the file once.
    return openDatabase(databaseURL, WebExtensionSQLiteDatabase::AccessType::ReadWriteCreate, false);
}

String WebExtensionSQLiteStore::deleteDatabaseIfEmpty()
{
    assertIsCurrent(queue());
    if (!isDatabaseEmpty())
        return nullString();

    return deleteDatabase();
}

String WebExtensionSQLiteStore::deleteDatabase()
{
    assertIsCurrent(queue());

    String databaseCloseErrorMessage;
    if (isDatabaseOpen()) {
        if (RefPtr db = database(); db->close() != SQLITE_OK) {
            RELEASE_LOG_ERROR(Extensions, "Failed to close storage database for extension %s", m_uniqueIdentifier.utf8().data());
            databaseCloseErrorMessage = "Failed to close extension storage database."_s;
        }
        m_database = nullptr;
    }

    if (m_useInMemoryDatabase)
        return databaseCloseErrorMessage;

    String deleteDatabaseFileErrorMessage = deleteDatabaseFileAtURL(databaseURL(), false);

    // An error from closing the database takes precedence over an error deleting the database file.
    return databaseCloseErrorMessage.length() ? databaseCloseErrorMessage : deleteDatabaseFileErrorMessage;
}

String WebExtensionSQLiteStore::handleSchemaVersioning(bool deleteDatabaseFileOnError)
{
    SchemaVersion currentSchemaVersion = currentDatabaseSchemaVersion();
    SchemaVersion schemaVersion = migrateToCurrentSchemaVersionIfNeeded();

    if (schemaVersion != currentSchemaVersion) {
        RELEASE_LOG_ERROR(Extensions, "Schema version (%d) does not match the supported schema version (%d) in database for extension %s", schemaVersion, currentSchemaVersion, m_uniqueIdentifier.utf8().data());

        if (!m_useInMemoryDatabase && deleteDatabaseFileOnError)
            return deleteDatabaseFileAtURL(databaseURL(), true);

        RefPtr db = database();
        db->close();

        m_database = nullptr;

        return "Failed to open extension storage database because of an invalid schema version."_s;
    }

    return nullString();
}

SchemaVersion WebExtensionSQLiteStore::migrateToCurrentSchemaVersionIfNeeded()
{
    assertIsCurrent(queue());

    auto schemaVersion = databaseSchemaVersion();
    auto currentSchemaVersion = currentDatabaseSchemaVersion();
    if (schemaVersion == currentSchemaVersion)
        return schemaVersion;

    // The initial implementation of this class didn't store the schema version, which is indistinguishable from the database not existing at all.
    // Because of this, we still need to do the migration when schemaVersion is 0, even though this is unnecessary if the database we just created,
    // but we don't want to spam the log every time we create a new database.
    if (!!schemaVersion)
        RELEASE_LOG_INFO(Extensions, "Schema version (%d) does not match our supported schema version (%d) in database for extension %s, recreating database", schemaVersion, currentSchemaVersion, m_uniqueIdentifier.utf8().data());

    // Someday we might migrate existing data from an older schema version, but we just start over for now.
    if (resetDatabaseSchema() != SQLITE_DONE)
        return 0;

    if (setDatabaseSchemaVersion(0) != SQLITE_DONE)
        return 0;

    vacuum();

    // We're dealing with a fresh database. Create the schema from scratch.
    if (createFreshDatabaseSchema() != SQLITE_DONE)
        return 0;

    setDatabaseSchemaVersion(currentDatabaseSchemaVersion());

    return currentDatabaseSchemaVersion();
}

SchemaVersion WebExtensionSQLiteStore::databaseSchemaVersion()
{
    assertIsCurrent(queue());
    ASSERT(m_database);

    SchemaVersion schemaVersion = 0;
    Ref rows = SQLiteDatabaseFetch(*m_database, "PRAGMA user_version"_s);
    if (RefPtr row = rows->next())
        schemaVersion = row->getInt(0);
    rows->statement()->invalidate();

    return schemaVersion;
}

DatabaseResult WebExtensionSQLiteStore::setDatabaseSchemaVersion(SchemaVersion newVersion)
{
    assertIsCurrent(queue());
    ASSERT(m_database);

    DatabaseResult result = SQLiteDatabaseExecute(*m_database, makeString("PRAGMA user_version = "_s, newVersion));
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to set database version for extension %s: %s (%d)", m_uniqueIdentifier.utf8().data(), m_database->m_lastErrorMessage.data(), result);

    return result;
}

String WebExtensionSQLiteStore::savepointNameFromUUID(const WTF::UUID& savepointIdentifier)
{
    return makeString("S"_s, makeStringByReplacingAll(savepointIdentifier.toString(), "-"_s, ""_s));
}

void WebExtensionSQLiteStore::createSavepoint(CompletionHandler<void(Markable<WTF::UUID> savepointIdentifier, const String& errorMessage)>&& completionHandler)
{
    UUID savepointIdentifier = UUID::createVersion4();

    m_queue->dispatch([protectedThis = Ref { *this }, savepointIdentifier = crossThreadCopy(savepointIdentifier), completionHandler = WTFMove(completionHandler)]() mutable {
        String errorMessage;
        if (protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ }, errorMessage);
            });

            return;
        }

        ASSERT(!errorMessage.length());
        ASSERT(protectedThis->m_database);

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->m_database), makeString("SAVEPOINT "_s, protectedThis->savepointNameFromUUID(savepointIdentifier)));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to create storage savepoint for extension %s. %s (%d)", protectedThis->m_uniqueIdentifier.utf8().data(), protectedThis->m_database->m_lastErrorMessage.data(), result);
            errorMessage = "Failed to create savepoint."_s;
        }

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), savepointIdentifier = crossThreadCopy(savepointIdentifier), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(!errorMessage.length() ? savepointIdentifier : WTF::UUID { UInt128 { 0 } }, errorMessage);
        });
    });
}

void WebExtensionSQLiteStore::commitSavepoint(WTF::UUID& savepointIdentifier, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    m_queue->dispatch([protectedThis = Ref { *this }, savepointIdentifier = crossThreadCopy(savepointIdentifier), completionHandler = WTFMove(completionHandler)]() mutable {
        String errorMessage;
        if (protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });

            return;
        }

        ASSERT(errorMessage.isEmpty());
        ASSERT(protectedThis->m_database);

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->m_database), makeString("RELEASE SAVEPOINT "_s, protectedThis->savepointNameFromUUID(savepointIdentifier)));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to release storage savepoint for extension %s. %s (%d)", protectedThis->m_uniqueIdentifier.utf8().data(), protectedThis->m_database->m_lastErrorMessage.data(), result);
            errorMessage = "Failed to release savepoint."_s;
        }

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(errorMessage);
        });
    });
}

void WebExtensionSQLiteStore::rollbackToSavepoint(WTF::UUID& savepointIdentifier, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    m_queue->dispatch([protectedThis = Ref { *this }, savepointIdentifier = crossThreadCopy(savepointIdentifier), completionHandler = WTFMove(completionHandler)]() mutable {
        String errorMessage;
        if (protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });

            return;
        }

        ASSERT(errorMessage.isEmpty());
        ASSERT(protectedThis->m_database);

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->m_database), makeString("ROLLBACK TO SAVEPOINT "_s, protectedThis->savepointNameFromUUID(savepointIdentifier)));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to rollback to storage savepoint for extension %s. %s (%d)", protectedThis->m_uniqueIdentifier.utf8().data(), protectedThis->m_database->m_lastErrorMessage.data(), result);
            errorMessage = "Failed to rollback to savepoint."_s;
        }

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(errorMessage);
        });
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
