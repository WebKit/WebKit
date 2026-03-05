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
#include "EnhancedSecuritySitesPersistence.h"

#include "Logging.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

#define ENHANCEDSECURITY_RELEASE_LOG(fmt, ...) RELEASE_LOG(EnhancedSecurity, "EnhancedSecuritySitesPersistence::" fmt, ##__VA_ARGS__)

namespace WebKit {

static constexpr auto sitesTableName = "sites"_s;
static constexpr auto siteIndexName = "idx_sites_site"_s;
static constexpr auto enhancedSecurityStateIndexName = "idx_sites_enhanced_security_state"_s;

static constexpr auto createSitesTableSQL = "CREATE TABLE sites (site TEXT PRIMARY KEY NOT NULL, enhanced_security_state INT NOT NULL)"_s;
static constexpr auto createEnhancedSecurityStateIndexSQL = "CREATE INDEX idx_sites_enhanced_security_state ON sites(enhanced_security_state)"_s;

static constexpr auto selectAllSitesSQL = "SELECT site FROM sites"_s;

static_assert(!std::to_underlying(EnhancedSecurity::Disabled), "EnhancedSecurity::Disabled is not 0 as expected");
static constexpr auto selectEnhancedSecurityOnlySitesSQL = "SELECT site FROM sites WHERE enhanced_security_state != 0"_s;

static constexpr auto selectSpecificSiteSQL = "SELECT enhanced_security_state FROM sites WHERE site = ?"_s;
static constexpr auto deleteAllSitesSQL = "DELETE FROM sites"_s;
static constexpr auto deleteSiteSQL = "DELETE FROM sites WHERE site = ?"_s;
static constexpr auto insertSiteSQL = "INSERT OR REPLACE INTO sites (site, enhanced_security_state) VALUES (?, ?)"_s;

EnhancedSecuritySitesPersistence::EnhancedSecuritySitesPersistence(const String& databaseDirectoryPath)
{
    ASSERT(!isMainRunLoop());
    openDatabase(databaseDirectoryPath);
}

EnhancedSecuritySitesPersistence::~EnhancedSecuritySitesPersistence()
{
    ASSERT(!isMainRunLoop());

    if (m_sqliteDB)
        closeDatabase();
}

void EnhancedSecuritySitesPersistence::reportSQLError(ASCIILiteral method, ASCIILiteral action)
{
    RELEASE_LOG_ERROR(EnhancedSecurity, "EnhancedSecuritySitesPersistence::%" PUBLIC_LOG_STRING ": Failed to %" PUBLIC_LOG_STRING " (%d) - %" PUBLIC_LOG_STRING, method.characters(), action.characters(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
}

static String databasePath(const String& directoryPath)
{
    ASSERT(!directoryPath.isEmpty());
    return FileSystem::pathByAppendingComponent(directoryPath, "EnhancedSecuritySites.db"_s);
}

WebCore::SQLiteStatementAutoResetScope EnhancedSecuritySitesPersistence::cachedStatement(StatementType type)
{
    ASSERT(m_sqliteDB);

    switch (type) {
    case StatementType::SelectSite:
        return WebCore::SQLiteStatementAutoResetScope { m_selectSpecificSiteSQLStatement.get() };
    case StatementType::InsertSite:
        return WebCore::SQLiteStatementAutoResetScope { m_insertSiteSQLStatement.get() };
    case StatementType::DeleteSite:
        return WebCore::SQLiteStatementAutoResetScope { m_deleteSQLStatement.get() };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool EnhancedSecuritySitesPersistence::openDatabase(const String& directoryPath)
{
    ASSERT(!isMainRunLoop());
    ASSERT(!directoryPath.isEmpty());

    FileSystem::makeAllDirectories(directoryPath);

    m_sqliteDB = makeUnique<WebCore::SQLiteDatabase>();

    CheckedPtr checkedDB = m_sqliteDB.get();

    // This database is accessed from serial queue EnhancedSecuritySitesHolder::sharedWorkQueueSingleton().
    checkedDB->disableThreadingChecks();

    auto reportErrorAndCloseDatabase = [&](ASCIILiteral action) {
        reportSQLError(__FUNCTION__, action);
        checkedDB = nullptr;
        closeDatabase();
        return false;
    };

    const auto path = databasePath(directoryPath);

    if (!checkedDB->open(path, WebCore::SQLiteDatabase::OpenMode::ReadWriteCreate, WebCore::SQLiteDatabase::OpenOptions::CanSuspendWhileLocked))
        return reportErrorAndCloseDatabase("open database"_s);

    if (!checkedDB->tableExists(sitesTableName)) {
        if (!checkedDB->executeCommand(createSitesTableSQL))
            return reportErrorAndCloseDatabase("create `sites` table"_s);

        ENHANCEDSECURITY_RELEASE_LOG("%s: Table %" PUBLIC_LOG_STRING " created", __FUNCTION__, sitesTableName.characters());
    }

    if (!checkedDB->indexExists(enhancedSecurityStateIndexName)) {
        if (!checkedDB->executeCommand(createEnhancedSecurityStateIndexSQL))
            return reportErrorAndCloseDatabase("create `enhanced_security_state` index on `sites` table"_s);

        ENHANCEDSECURITY_RELEASE_LOG("%s: Index %" PUBLIC_LOG_STRING " created", __FUNCTION__, enhancedSecurityStateIndexName.characters());
    }

    m_insertSiteSQLStatement = checkedDB->prepareStatement(insertSiteSQL);
    if (!m_insertSiteSQLStatement)
        return reportErrorAndCloseDatabase("prepare insert statement"_s);

    m_selectSpecificSiteSQLStatement = checkedDB->prepareStatement(selectSpecificSiteSQL);
    if (!m_selectSpecificSiteSQLStatement)
        return reportErrorAndCloseDatabase("prepare select specific site statement"_s);

    m_deleteSQLStatement = checkedDB->prepareStatement(deleteSiteSQL);
    if (!m_deleteSQLStatement)
        return reportErrorAndCloseDatabase("prepare delete statement"_s);

    checkedDB->turnOnIncrementalAutoVacuum();

    return true;
}

void EnhancedSecuritySitesPersistence::deleteSite(const WebCore::RegistrableDomain& site)
{
    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    CheckedPtr deleteStatement = cachedStatement(StatementType::DeleteSite).get();

    if (!deleteStatement
        || deleteStatement->bindText(1, site.string()) != SQLITE_OK
        || !deleteStatement->executeCommand())
        reportSQLError(__FUNCTION__, "failed to delete site"_s);
}

void EnhancedSecuritySitesPersistence::deleteSites(const Vector<WebCore::RegistrableDomain>& sites)
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    for (auto& site : sites)
        deleteSite(site);
}

void EnhancedSecuritySitesPersistence::deleteAllSites()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Delete all attempted on closed database, trying file deletion instead.", __FUNCTION__);
        return;
    }

    auto deleteStatement = protect(m_sqliteDB)->prepareStatement(deleteAllSitesSQL);
    if (!deleteStatement || !deleteStatement->executeCommand())
        return reportSQLError(__FUNCTION__, "delete all sites"_s);
}

HashSet<WebCore::RegistrableDomain> EnhancedSecuritySitesPersistence::enhancedSecurityOnlyDomains()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Attempted operation on closed database.", __FUNCTION__);
        return { };
    }

    auto selectStatement = protect(m_sqliteDB)->prepareStatement(selectEnhancedSecurityOnlySitesSQL);
    if (!selectStatement) {
        reportSQLError(__FUNCTION__, "fetch enhanced security only sites"_s);
        return { };
    }

    HashSet<WebCore::RegistrableDomain> sites;
    while (selectStatement->step() == SQLITE_ROW) {
        auto site = selectStatement->columnText(0);
        sites.add(WebCore::RegistrableDomain::fromRawString(String { site }));
    }

    return sites;
}

HashSet<WebCore::RegistrableDomain> EnhancedSecuritySitesPersistence::allEnhancedSecuritySites()
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Attempted operation on closed database.", __FUNCTION__);
        return { };
    }

    auto selectStatement = protect(m_sqliteDB)->prepareStatement(selectAllSitesSQL);
    if (!selectStatement) {
        reportSQLError(__FUNCTION__, "fetch all sites"_s);
        return { };
    }

    HashSet<WebCore::RegistrableDomain> sites;
    while (selectStatement->step() == SQLITE_ROW) {
        auto site = selectStatement->columnText(0);
        sites.add(WebCore::RegistrableDomain::fromRawString(String { site }));
    }

    return sites;
}

void EnhancedSecuritySitesPersistence::trackEnhancedSecurityForDomain(WebCore::RegistrableDomain&& site, EnhancedSecurity reason)
{
    ASSERT(!isMainRunLoop());

    if (!isDatabaseOpen()) {
        RELEASE_LOG_ERROR(EnhancedSecurity, "%s: Attempted operation on closed database.", __FUNCTION__);
        return;
    }

    CheckedPtr selectSiteStatement = cachedStatement(StatementType::SelectSite).get();

    if (!selectSiteStatement
        || selectSiteStatement->bindText(1, site.string()) != SQLITE_OK)
        reportSQLError(__FUNCTION__, "Failed to query specific site"_s);

    if (selectSiteStatement->step() == SQLITE_ROW) {
        if (static_cast<EnhancedSecurity>(selectSiteStatement->columnInt(0)) == EnhancedSecurity::Disabled)
            return;
    }

    CheckedPtr insertSiteStatement = cachedStatement(StatementType::InsertSite).get();

    auto enhancedSecurityReason = std::to_underlying(reason);
    if (!insertSiteStatement
        || insertSiteStatement->bindText(1, site.string()) != SQLITE_OK
        || insertSiteStatement->bindInt(2, enhancedSecurityReason) != SQLITE_OK
        || !insertSiteStatement->executeCommand())
        reportSQLError(__FUNCTION__, "Failed to insert or replace site"_s);
}

void EnhancedSecuritySitesPersistence::closeDatabase()
{
    ASSERT(!isMainRunLoop());

    m_insertSiteSQLStatement = nullptr;
    m_selectSpecificSiteSQLStatement = nullptr;
    m_deleteSQLStatement = nullptr;

    if (isDatabaseOpen()) {
        ENHANCEDSECURITY_RELEASE_LOG("%s: Closing database", __FUNCTION__);
        protect(m_sqliteDB)->close();
    }

    m_sqliteDB = nullptr;
}

} // namespace WebKit

#undef ENHANCEDSECURITY_RELEASE_LOG
