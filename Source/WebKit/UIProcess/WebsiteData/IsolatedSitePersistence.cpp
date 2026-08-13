/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "IsolatedSitePersistence.h"

#include "Logging.h"
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <iterator>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WallTime.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IsolatedSitePersistence);

// A migration path, not the current schema: a database opened at version N runs the statements from N
// onwards, so a schema change appends an array rather than editing an earlier one.
static constexpr std::array<ASCIILiteral, 2> isolatedSitesSchemaV1Statements {
    "CREATE TABLE sites (domain TEXT PRIMARY KEY NOT NULL, signals INT NOT NULL, last_updated REAL NOT NULL)"_s,
    "CREATE TABLE metadata (key TEXT PRIMARY KEY NOT NULL, value INT NOT NULL)"_s,
};

static constexpr std::array<std::span<const ASCIILiteral>, 1> isolatedSitesSchemaStatements {
    std::span { isolatedSitesSchemaV1Statements },
};

static constexpr int currentIsolatedSitesSchemaVersion = std::size(isolatedSitesSchemaStatements);
static constexpr auto selectAllIsolatedSitesSQL = "SELECT domain, signals, last_updated FROM sites"_s;
static constexpr auto insertIsolatedSiteSQL = "INSERT OR REPLACE INTO sites (domain, signals, last_updated) VALUES (?, ?, ?)"_s;
static constexpr auto deleteAllIsolatedSitesSQL = "DELETE FROM sites"_s;
static constexpr auto deleteSitesUpdatedSinceSQL = "DELETE FROM sites WHERE last_updated >= ?"_s;
static constexpr auto deleteIsolatedSiteSQL = "DELETE FROM sites WHERE domain = ?"_s;
static constexpr auto didImportUserInteractionsKey = "didImportUserInteractions"_s;
static constexpr auto selectMetadataSQL = "SELECT value FROM metadata WHERE key = ?"_s;
static constexpr auto insertMetadataSQL = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)"_s;

IsolatedSitePersistence::IsolatedSitePersistence(const String& databaseDirectoryPath)
{
    ASSERT(!isMainRunLoop());
    openDatabase(databaseDirectoryPath);
}

IsolatedSitePersistence::~IsolatedSitePersistence()
{
    ASSERT(!isMainRunLoop());

    if (m_sqliteDB)
        closeDatabase();
}

void IsolatedSitePersistence::reportSQLError(ASCIILiteral method, ASCIILiteral action)
{
    RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Failed to %" PUBLIC_LOG_STRING " (%d) - %" PUBLIC_LOG_STRING, method.characters(), action.characters(), protect(m_sqliteDB)->lastError(), protect(m_sqliteDB)->lastErrorMsg());
}

IsolatedSitePersistence::OpenResult IsolatedSitePersistence::reportSQLErrorAndClassify(ASCIILiteral method, ASCIILiteral action)
{
    reportSQLError(method, action);

    // Only a file we cannot interpret is worth starting over for; discarding credential evidence over
    // a full disk or a locked volume would be far worse than not persisting this run.
    auto error = protect(m_sqliteDB)->lastError();
    if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB)
        return OpenResult::Discard;

    return OpenResult::Retain;
}

static String isolatedSitesDatabasePath(const String& directoryPath)
{
    ASSERT(!directoryPath.isEmpty());
    return FileSystem::pathByAppendingComponent(directoryPath, "IsolatedSites.db"_s);
}

IsolatedSitePersistence::OpenResult IsolatedSitePersistence::openDatabase(const String& directoryPath)
{
    ASSERT(!isMainRunLoop());
    ASSERT(!directoryPath.isEmpty());

    FileSystem::makeAllDirectories(directoryPath);

    auto path = isolatedSitesDatabasePath(directoryPath);
    auto result = openDatabaseAtPath(path);
    if (result != OpenResult::Discard)
        return result;

    closeDatabase();

    if (!WebCore::SQLiteFileSystem::deleteDatabaseFile(path)) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Failed to delete unusable database; site isolation signals will not persist.", __FUNCTION__);
        return OpenResult::Retain;
    }

    RELEASE_LOG(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Deleted unusable database and recreating it from scratch.", __FUNCTION__);
    return openDatabaseAtPath(path);
}

IsolatedSitePersistence::OpenResult IsolatedSitePersistence::openDatabaseAtPath(const String& path)
{
    ASSERT(!isMainRunLoop());

    m_sqliteDB = makeUnique<WebCore::SQLiteDatabase>();

    CheckedPtr checkedDB = m_sqliteDB.get();

    // This database is only accessed from the serial queue IsolatedSiteStore::sharedWorkQueueSingleton().
    checkedDB->disableThreadingChecks();

    if (!checkedDB->open(path, WebCore::SQLiteDatabase::OpenMode::ReadWriteCreate, WebCore::SQLiteDatabase::OpenOptions::CanSuspendWhileLocked))
        return reportSQLErrorAndClassify(__FUNCTION__, "open database"_s);

    int version = 0;
    {
        auto versionStatement = checkedDB->prepareStatement("PRAGMA user_version"_s);
        if (!versionStatement || versionStatement->step() != SQLITE_ROW)
            return reportSQLErrorAndClassify(__FUNCTION__, "read schema version"_s);
        version = versionStatement->columnInt(0);
    }

    if (version < 0 || version > currentIsolatedSitesSchemaVersion) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Found unexpected schema version %d (expected at most %d).", __FUNCTION__, version, currentIsolatedSitesSchemaVersion);
        return OpenResult::Discard;
    }

    if (version < currentIsolatedSitesSchemaVersion) {
        WebCore::SQLiteTransaction transaction(*checkedDB);
        transaction.begin();

        for (int i = version; i < currentIsolatedSitesSchemaVersion; ++i) {
            for (auto statement : isolatedSitesSchemaStatements[i]) {
                if (!checkedDB->executeCommand(statement))
                    return reportSQLErrorAndClassify(__FUNCTION__, "execute schema statement"_s);
            }
        }

        if (!checkedDB->executeCommandSlow(makeString("PRAGMA user_version = "_s, currentIsolatedSitesSchemaVersion)))
            return reportSQLErrorAndClassify(__FUNCTION__, "set schema version"_s);

        transaction.commit();
    }

    m_insertSiteSQLStatement = checkedDB->prepareStatement(insertIsolatedSiteSQL);
    if (!m_insertSiteSQLStatement)
        return reportSQLErrorAndClassify(__FUNCTION__, "prepare insert statement"_s);

    m_deleteSiteSQLStatement = checkedDB->prepareStatement(deleteIsolatedSiteSQL);
    if (!m_deleteSiteSQLStatement)
        return reportSQLErrorAndClassify(__FUNCTION__, "prepare delete statement"_s);

    checkedDB->turnOnIncrementalAutoVacuum();

    return OpenResult::Success;
}

void IsolatedSitePersistence::closeDatabase()
{
    ASSERT(!isMainRunLoop());

    m_insertSiteSQLStatement = nullptr;
    m_deleteSiteSQLStatement = nullptr;

    if (isDatabaseOpen())
        protect(m_sqliteDB)->close();

    m_sqliteDB = nullptr;
}

HashMap<WebCore::RegistrableDomain, IsolatedSitePersistence::SiteRecord> IsolatedSitePersistence::allSites()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return { };
    }

    auto selectStatement = protect(m_sqliteDB)->prepareStatement(selectAllIsolatedSitesSQL);
    if (!selectStatement) {
        reportSQLError(__FUNCTION__, "prepare select all sites statement"_s);
        return { };
    }

    HashMap<WebCore::RegistrableDomain, SiteRecord> sites;
    while (selectStatement->step() == SQLITE_ROW) {
        auto domain = WebCore::RegistrableDomain::fromRawString(selectStatement->columnText(0));
        if (domain.isEmpty())
            continue;
        sites.set(WTF::move(domain), SiteRecord {
            static_cast<uint64_t>(selectStatement->columnInt64(1)),
            WallTime::fromRawSeconds(selectStatement->columnDouble(2))
        });
    }

    return sites;
}

void IsolatedSitePersistence::setRecordForSite(const WebCore::RegistrableDomain& domain, SiteRecord record)
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    auto insertStatement = WebCore::SQLiteStatementAutoResetScope { protect(m_insertSiteSQLStatement).get() };
    if (!insertStatement
        || insertStatement->bindText(1, domain.string()) != SQLITE_OK
        || insertStatement->bindInt64(2, static_cast<int64_t>(record.signals)) != SQLITE_OK
        || insertStatement->bindDouble(3, record.lastUpdated.secondsSinceEpoch().value()) != SQLITE_OK
        || !insertStatement->executeCommand())
        reportSQLError(__FUNCTION__, "insert or replace site"_s);
}

void IsolatedSitePersistence::deleteAllSites()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    auto deleteStatement = protect(m_sqliteDB)->prepareStatement(deleteAllIsolatedSitesSQL);
    if (!deleteStatement || !deleteStatement->executeCommand())
        reportSQLError(__FUNCTION__, "delete all sites"_s);
}

void IsolatedSitePersistence::deleteSitesUpdatedSince(WallTime cutoff)
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    auto deleteStatement = protect(m_sqliteDB)->prepareStatement(deleteSitesUpdatedSinceSQL);
    if (!deleteStatement
        || deleteStatement->bindDouble(1, cutoff.secondsSinceEpoch().value()) != SQLITE_OK
        || !deleteStatement->executeCommand())
        reportSQLError(__FUNCTION__, "delete sites updated since a cutoff"_s);
}

void IsolatedSitePersistence::deleteSites(const Vector<WebCore::RegistrableDomain>& domains)
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    for (auto& domain : domains) {
        auto deleteStatement = WebCore::SQLiteStatementAutoResetScope { protect(m_deleteSiteSQLStatement).get() };
        if (!deleteStatement
            || deleteStatement->bindText(1, domain.string()) != SQLITE_OK
            || !deleteStatement->executeCommand())
            reportSQLError(__FUNCTION__, "delete site"_s);
    }
}

bool IsolatedSitePersistence::didImportUserInteractions()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen())
        return false;

    auto selectStatement = protect(m_sqliteDB)->prepareStatement(selectMetadataSQL);
    if (!selectStatement || selectStatement->bindText(1, didImportUserInteractionsKey) != SQLITE_OK) {
        reportSQLError(__FUNCTION__, "query metadata"_s);
        return false;
    }

    return selectStatement->step() == SQLITE_ROW && selectStatement->columnInt(0);
}

void IsolatedSitePersistence::setDidImportUserInteractions()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(SiteIsolation, "IsolatedSitePersistence::%" PUBLIC_LOG_STRING ": Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    auto insertStatement = protect(m_sqliteDB)->prepareStatement(insertMetadataSQL);
    if (!insertStatement
        || insertStatement->bindText(1, didImportUserInteractionsKey) != SQLITE_OK
        || insertStatement->bindInt(2, 1) != SQLITE_OK
        || !insertStatement->executeCommand())
        reportSQLError(__FUNCTION__, "set metadata"_s);
}

} // namespace WebKit
