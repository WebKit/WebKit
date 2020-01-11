/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsDatabaseStore.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Logging.h"
#include "NetworkSession.h"
#include "PluginProcessManager.h"
#include "PluginProcessProxy.h"
#include "ResourceLoadStatisticsMemoryStore.h"
#include "StorageAccessStatus.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataStore.h"
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/DateMath.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/StdSet.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

#if PLATFORM(COCOA)
#define RELEASE_LOG_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_ERROR_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)
#else
#define RELEASE_LOG_IF_ALLOWED(sessionID, fmt, ...)  ((void)0)
#define RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, fmt, ...)  ((void)0)
#endif

// COUNT Queries
constexpr auto observedDomainCountQuery = "SELECT COUNT(*) FROM ObservedDomains"_s;
constexpr auto countSubframeUnderTopFrameQuery = "SELECT COUNT(*) FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? AND topFrameDomainID = ?;"_s;
constexpr auto countSubresourceUnderTopFrameQuery = "SELECT COUNT(*) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? AND topFrameDomainID = ?;"_s;
constexpr auto countSubresourceUniqueRedirectsToQuery = "SELECT COUNT(*) FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? AND toDomainID = ?;"_s;
constexpr auto countPrevalentResourcesQuery = "SELECT COUNT(DISTINCT registrableDomain) FROM ObservedDomains WHERE isPrevalent = 1;"_s;
constexpr auto countPrevalentResourcesWithUserInteractionQuery = "SELECT COUNT(DISTINCT registrableDomain) FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 1;"_s;

constexpr auto countPrevalentResourcesWithoutUserInteractionQuery = "SELECT COUNT(DISTINCT registrableDomain) FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 0;"_s;

// INSERT OR IGNORE Queries
constexpr auto insertObservedDomainQuery = "INSERT INTO ObservedDomains (registrableDomain, lastSeen, hadUserInteraction,"
    "mostRecentUserInteractionTime, grandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, timesAccessedAsFirstPartyDueToUserInteraction,"
    "timesAccessedAsFirstPartyDueToStorageAccessAPI, isScheduledForAllButCookieDataRemoval) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
constexpr auto insertTopLevelDomainQuery = "INSERT INTO TopLevelDomains VALUES (?)"_s;
constexpr auto storageAccessUnderTopFrameDomainsQuery = "INSERT OR IGNORE INTO StorageAccessUnderTopFrameDomains (domainID, topLevelDomainID) SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;
constexpr auto topFrameUniqueRedirectsToQuery = "INSERT OR IGNORE into TopFrameUniqueRedirectsTo (sourceDomainID, toDomainID) SELECT ?, domainID FROM ObservedDomains where registrableDomain in ( "_s;
constexpr auto topFrameUniqueRedirectsFromQuery = "INSERT OR IGNORE INTO TopFrameUniqueRedirectsFrom (targetDomainID, fromDomainID) SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;
constexpr auto topFrameLoadedThirdPartyScriptsQuery = "INSERT OR IGNORE into TopFrameLoadedThirdPartyScripts (topFrameDomainID, subresourceDomainID) SELECT ?, domainID FROM ObservedDomains where registrableDomain in ( "_s;
constexpr auto subresourceUniqueRedirectsFromQuery = "INSERT OR IGNORE INTO SubresourceUniqueRedirectsFrom (subresourceDomainID, fromDomainID) SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;

// INSERT OR REPLACE Queries
constexpr auto subframeUnderTopFrameDomainsQuery = "INSERT OR REPLACE into SubframeUnderTopFrameDomains (subFrameDomainID, lastUpdated, topFrameDomainID) SELECT ?, ?, domainID FROM ObservedDomains where registrableDomain in ( "_s;
constexpr auto topFrameLinkDecorationsFromQuery = "INSERT OR REPLACE INTO TopFrameLinkDecorationsFrom (toDomainID, lastUpdated, fromDomainID) SELECT ?, ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;
constexpr auto subresourceUnderTopFrameDomainsQuery = "INSERT OR REPLACE INTO SubresourceUnderTopFrameDomains (subresourceDomainID, lastUpdated, topFrameDomainID) SELECT ?, ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;
constexpr auto subresourceUniqueRedirectsToQuery = "INSERT OR REPLACE INTO SubresourceUniqueRedirectsTo (subresourceDomainID, lastUpdated, toDomainID) SELECT ?, ?, domainID FROM ObservedDomains WHERE registrableDomain in ( "_s;

// EXISTS Queries
constexpr auto subframeUnderTopFrameDomainExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? "
    "AND topFrameDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subresourceUnderTopFrameDomainExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubresourceUnderTopFrameDomains "
    "WHERE subresourceDomainID = ? AND topFrameDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subresourceUniqueRedirectsToExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? "
    "AND toDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto topFrameLinkDecorationsFromExistsQuery = "SELECT EXISTS (SELECT 1 FROM TopFrameLinkDecorationsFrom WHERE toDomainID = ? "
    "AND fromDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto topFrameLoadedThirdPartyScriptsExistsQuery = "SELECT EXISTS (SELECT 1 FROM TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ? "
    "AND subresourceDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto storageAccessExistsQuery = "SELECT EXISTS (SELECT 1 FROM StorageAccessUnderTopFrameDomains WHERE domainID = ? AND topLevelDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;

// UPDATE Queries
constexpr auto mostRecentUserInteractionQuery = "UPDATE ObservedDomains SET hadUserInteraction = ?, mostRecentUserInteractionTime = ? "
    "WHERE registrableDomain = ?"_s;
constexpr auto updateLastSeenQuery = "UPDATE ObservedDomains SET lastSeen = ? WHERE registrableDomain = ?"_s;
constexpr auto updateDataRecordsRemovedQuery = "UPDATE ObservedDomains SET dataRecordsRemoved = ? WHERE registrableDomain = ?"_s;
constexpr auto updatePrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto updateVeryPrevalentResourceQuery = "UPDATE ObservedDomains SET isVeryPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto clearPrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = 0, isVeryPrevalent = 0 WHERE registrableDomain = ?"_s;
constexpr auto updateGrandfatheredQuery = "UPDATE ObservedDomains SET grandfathered = ? WHERE registrableDomain = ?"_s;
constexpr auto updateIsScheduledForAllButCookieDataRemovalQuery = "UPDATE ObservedDomains SET isScheduledForAllButCookieDataRemoval = ? WHERE registrableDomain = ?"_s;

// SELECT Queries
constexpr auto domainIDFromStringQuery = "SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto domainStringFromDomainIDQuery = "SELECT registrableDomain FROM ObservedDomains WHERE domainID = ?"_s;
constexpr auto isPrevalentResourceQuery = "SELECT isPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isVeryPrevalentResourceQuery = "SELECT isVeryPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto hadUserInteractionQuery = "SELECT hadUserInteraction, mostRecentUserInteractionTime FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isGrandfatheredQuery = "SELECT grandfathered FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto findExpiredUserInteractionQuery = "SELECT domainID FROM ObservedDomains WHERE hadUserInteraction = 1 AND mostRecentUserInteractionTime < ?"_s;
constexpr auto getResourceDataByDomainNameQuery = "SELECT * FROM ObservedDomains WHERE registrableDomain = ?";
constexpr auto getMostRecentlyUpdatedTimestampQuery = "SELECT MAX(lastUpdated) FROM (SELECT lastUpdated FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? and topFrameDomainID = ?"
    "UNION ALL SELECT lastUpdated FROM TopFrameLinkDecorationsFrom WHERE toDomainID = ? and fromDomainID = ?"
    "UNION ALL SELECT lastUpdated FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? and topFrameDomainID = ?"
    "UNION ALL SELECT lastUpdated FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? and toDomainID = ?)"_s;
constexpr auto getAllDomainsQuery = "SELECT registrableDomain FROM ObservedDomains"_s;
constexpr auto getAllSubStatisticsUnderDomainQuery = "SELECT topFrameDomainID FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ?"
    "UNION ALL SELECT topFrameDomainID FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ?"
    "UNION ALL SELECT toDomainID FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ?"_s;

const char* tables[] = {
    "ObservedDomains",
    "TopLevelDomains",
    "StorageAccessUnderTopFrameDomains",
    "TopFrameUniqueRedirectsTo",
    "TopFrameUniqueRedirectsFrom",
    "TopFrameLinkDecorationsFrom",
    "TopFrameLoadedThirdPartyScripts",
    "SubframeUnderTopFrameDomains",
    "SubresourceUnderTopFrameDomains",
    "SubresourceUniqueRedirectsTo",
    "SubresourceUniqueRedirectsFrom"
};

// CREATE TABLE Queries
constexpr auto createObservedDomain = "CREATE TABLE ObservedDomains ("
    "domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, "
    "hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, "
    "isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,"
    "timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL,"
    "isScheduledForAllButCookieDataRemoval INTEGER NOT NULL)"_s;

enum {
    DomainIDIndex,
    RegistrableDomainIndex,
    LastSeenIndex,
    HadUserInteractionIndex,
    MostRecentUserInteractionTimeIndex,
    GrandfatheredIndex,
    IsPrevalentIndex,
    IsVeryPrevalentIndex,
    DataRecordsRemovedIndex,
    TimesAccessedAsFirstPartyDueToUserInteractionIndex,
    TimesAccessedAsFirstPartyDueToStorageAccessAPIIndex,
    IsScheduledForAllButCookieDataRemovalIndex
};

constexpr auto createTopLevelDomains = "CREATE TABLE TopLevelDomains ("
    "topLevelDomainID INTEGER PRIMARY KEY, CONSTRAINT fkDomainID FOREIGN KEY(topLevelDomainID) "
    "REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createStorageAccessUnderTopFrameDomains = "CREATE TABLE StorageAccessUnderTopFrameDomains ("
    "domainID INTEGER NOT NULL, topLevelDomainID INTEGER NOT NULL ON CONFLICT FAIL, "
    "CONSTRAINT fkDomainID FOREIGN KEY(domainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topLevelDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createTopFrameUniqueRedirectsTo = "CREATE TABLE TopFrameUniqueRedirectsTo ("
    "sourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(sourceDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createTopFrameUniqueRedirectsFrom = "CREATE TABLE TopFrameUniqueRedirectsFrom ("
    "targetDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(targetDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameLinkDecorationsFrom = "CREATE TABLE TopFrameLinkDecorationsFrom ("
    "toDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(toDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameLoadedThirdPartyScripts = "CREATE TABLE TopFrameLoadedThirdPartyScripts ("
    "topFrameDomainID INTEGER NOT NULL, subresourceDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createSubframeUnderTopFrameDomains = "CREATE TABLE SubframeUnderTopFrameDomains ("
    "subFrameDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUnderTopFrameDomains = "CREATE TABLE SubresourceUnderTopFrameDomains ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsTo = "CREATE TABLE SubresourceUniqueRedirectsTo ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsFrom = "CREATE TABLE SubresourceUniqueRedirectsFrom ("
    "subresourceDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

// CREATE UNIQUE INDEX Queries
constexpr auto createUniqueIndexStorageAccessUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS StorageAccessUnderTopFrameDomains_domainID_topLevelDomainID on StorageAccessUnderTopFrameDomains ( domainID, topLevelDomainID );"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsTo_sourceDomainID_toDomainID on TopFrameUniqueRedirectsTo ( sourceDomainID, toDomainID );"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsFrom_targetDomainID_fromDomainID on TopFrameUniqueRedirectsFrom ( targetDomainID, fromDomainID );"_s;
constexpr auto createUniqueIndexTopFrameLinkDecorationsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLinkDecorationsFrom_toDomainID_fromDomainID on TopFrameLinkDecorationsFrom ( toDomainID, fromDomainID );"_s;
constexpr auto createUniqueIndexTopFrameLoadedThirdPartyScripts = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLoadedThirdPartyScripts_topFrameDomainID_subresourceDomainID on TopFrameLoadedThirdPartyScripts ( topFrameDomainID, subresourceDomainID );"_s;
constexpr auto createUniqueIndexSubframeUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubframeUnderTopFrameDomains_subFrameDomainID_topFrameDomainID on SubframeUnderTopFrameDomains ( subFrameDomainID, topFrameDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUnderTopFrameDomains_subresourceDomainID_topFrameDomainID on SubresourceUnderTopFrameDomains ( subresourceDomainID, topFrameDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsTo_subresourceDomainID_toDomainID on SubresourceUniqueRedirectsTo ( subresourceDomainID, toDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsFrom_subresourceDomainID_fromDomainID on SubresourceUnderTopFrameDomains ( subresourceDomainID, fromDomainID );"_s;

const unsigned minimumPrevalentResourcesForTelemetry = 3;

static const String ObservedDomainsTableSchemaV1()
{
    return createObservedDomain;
}

static const String ObservedDomainsTableSchemaV1Alternate()
{
    return "CREATE TABLE \"ObservedDomains\" (domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL,isScheduledForAllButCookieDataRemoval INTEGER NOT NULL)";
}

ResourceLoadStatisticsDatabaseStore::ResourceLoadStatisticsDatabaseStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost, const String& storageDirectoryPath, PAL::SessionID sessionID)
    : ResourceLoadStatisticsStore(store, workQueue, shouldIncludeLocalhost)
    , m_storageDirectoryPath(storageDirectoryPath + "/observations.db")
    , m_observedDomainCount(m_database, observedDomainCountQuery)
    , m_insertObservedDomainStatement(m_database, insertObservedDomainQuery)
    , m_insertTopLevelDomainStatement(m_database, insertTopLevelDomainQuery)
    , m_domainIDFromStringStatement(m_database, domainIDFromStringQuery)
    , m_topFrameLinkDecorationsFromExists(m_database, topFrameLinkDecorationsFromExistsQuery)
    , m_topFrameLoadedThirdPartyScriptsExists(m_database, topFrameLoadedThirdPartyScriptsExistsQuery)
    , m_subframeUnderTopFrameDomainExists(m_database, subframeUnderTopFrameDomainExistsQuery)
    , m_subresourceUnderTopFrameDomainExists(m_database, subresourceUnderTopFrameDomainExistsQuery)
    , m_subresourceUniqueRedirectsToExists(m_database, subresourceUniqueRedirectsToExistsQuery)
    , m_mostRecentUserInteractionStatement(m_database, mostRecentUserInteractionQuery)
    , m_updateLastSeenStatement(m_database, updateLastSeenQuery)
    , m_updateDataRecordsRemovedStatement(m_database, updateDataRecordsRemovedQuery)
    , m_updatePrevalentResourceStatement(m_database, updatePrevalentResourceQuery)
    , m_isPrevalentResourceStatement(m_database, isPrevalentResourceQuery)
    , m_updateVeryPrevalentResourceStatement(m_database, updateVeryPrevalentResourceQuery)
    , m_isVeryPrevalentResourceStatement(m_database, isVeryPrevalentResourceQuery)
    , m_clearPrevalentResourceStatement(m_database, clearPrevalentResourceQuery)
    , m_hadUserInteractionStatement(m_database, hadUserInteractionQuery)
    , m_updateGrandfatheredStatement(m_database, updateGrandfatheredQuery)
    , m_updateIsScheduledForAllButCookieDataRemovalStatement(m_database, updateIsScheduledForAllButCookieDataRemovalQuery)
    , m_isGrandfatheredStatement(m_database, isGrandfatheredQuery)
    , m_findExpiredUserInteractionStatement(m_database, findExpiredUserInteractionQuery)
    , m_countPrevalentResourcesStatement(m_database, countPrevalentResourcesQuery)
    , m_countPrevalentResourcesWithUserInteractionStatement(m_database, countPrevalentResourcesWithUserInteractionQuery)
    , m_countPrevalentResourcesWithoutUserInteractionStatement(m_database, countPrevalentResourcesWithoutUserInteractionQuery)
    , m_getResourceDataByDomainNameStatement(m_database, getResourceDataByDomainNameQuery)
    , m_getAllDomainsStatement(m_database, getAllDomainsQuery)
    , m_domainStringFromDomainIDStatement(m_database, domainStringFromDomainIDQuery)
    , m_getAllSubStatisticsStatement(m_database, getAllSubStatisticsUnderDomainQuery)
    , m_storageAccessExistsStatement(m_database, storageAccessExistsQuery)
    , m_getMostRecentlyUpdatedTimestampStatement(m_database, getMostRecentlyUpdatedTimestampQuery)
    , m_sessionID(sessionID)
{
    ASSERT(!RunLoop::isMain());

    openAndDropOldDatabaseIfNecessary();
    
    // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
    m_database.disableThreadingChecks();

    if (!m_database.tableExists("ObservedDomains"_s)) {
        if (!createSchema()) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createSchema failed, error message: %{public}s, database path: %{public}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }
    }
    
    if (!m_database.turnOnIncrementalAutoVacuum())
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::turnOnIncrementalAutoVacuum failed, error message: %{public}s", this, m_database.lastErrorMsg());

    if (!prepareStatements()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::prepareStatements failed, error message: %{public}s, database path: %{public}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
        ASSERT_NOT_REACHED();
        return;
    }

    workQueue.dispatchAfter(5_s, [weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->calculateAndSubmitTelemetry();
    });
}

void ResourceLoadStatisticsDatabaseStore::openITPDatabase()
{
    if (!FileSystem::fileExists(m_storageDirectoryPath))
        m_isNewResourceLoadStatisticsDatabaseFile = true;
    else
        m_isNewResourceLoadStatisticsDatabaseFile = false;

    if (!m_database.open(m_storageDirectoryPath)) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::open failed, error message: %{public}s, database path: %{public}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
        ASSERT_NOT_REACHED();
    }
}

static void resetStatement(SQLiteStatement& statement)
{
    int resetResult = statement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

bool ResourceLoadStatisticsDatabaseStore::isCorrectTableSchema()
{
    SQLiteStatement statement(m_database, "SELECT 1 from sqlite_master WHERE type='table' and tbl_name=?");
    if (statement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::isCorrectTableSchema failed to prepare, error message: %{public}s", this, m_database.lastErrorMsg());
        return false;
    }

    bool hasAllTables = true;
    for (auto table : tables) {
        if (statement.bindText(1, table) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::isCorrectTableSchema failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
            return false;
        }
        if (statement.step() != SQLITE_ROW) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::isCorrectTableSchema schema is missing table: %s", this, table);
            hasAllTables = false;
        }
        resetStatement(statement);
    }
    return hasAllTables;
}

void ResourceLoadStatisticsDatabaseStore::openAndDropOldDatabaseIfNecessary()
{
    openITPDatabase();

    if (!isCorrectTableSchema()) {
        m_database.close();
        // FIXME: Migrate existing data to new database file instead of deleting it (204482).
        FileSystem::deleteFile(m_storageDirectoryPath);
        openITPDatabase();
        return;
    }

    String currentSchema;
    {
        // Fetch the schema for an existing Observed Domains table.
        SQLiteStatement statement(m_database, "SELECT type, sql FROM sqlite_master WHERE tbl_name='ObservedDomains'");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the ObservedDomains table.");
            ASSERT_NOT_REACHED();
            return;
        }

        // If there is no ObservedDomains table at all, or there is an error executing the fetch, delete the file.
        if (statement.step() != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the Observed Domains table.");
            m_database.close();
            FileSystem::deleteFile(m_storageDirectoryPath);
            openITPDatabase();
            return;
        }

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the schema in the ResourceLoadStatistics directory is not the current schema, delete the database file.
    if (currentSchema != ObservedDomainsTableSchemaV1() && currentSchema != ObservedDomainsTableSchemaV1Alternate()) {
        m_database.close();
        FileSystem::deleteFile(m_storageDirectoryPath);
        openITPDatabase();
    }
}

bool ResourceLoadStatisticsDatabaseStore::isEmpty() const
{
    ASSERT(!RunLoop::isMain());
    
    bool result = false;
    if (m_observedDomainCount.step() == SQLITE_ROW)
        result = !m_observedDomainCount.getColumnInt(0);
    
    int resetResult = m_observedDomainCount.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return result;
}

bool ResourceLoadStatisticsDatabaseStore::createUniqueIndices()
{
    if (!m_database.executeCommand(createUniqueIndexStorageAccessUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexTopFrameUniqueRedirectsTo)
        || !m_database.executeCommand(createUniqueIndexTopFrameUniqueRedirectsFrom)
        || !m_database.executeCommand(createUniqueIndexTopFrameLinkDecorationsFrom)
        || !m_database.executeCommand(createUniqueIndexTopFrameLoadedThirdPartyScripts)
        || !m_database.executeCommand(createUniqueIndexSubframeUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexSubresourceUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexSubresourceUniqueRedirectsTo)
        || !m_database.executeCommand(createUniqueIndexSubresourceUnderTopFrameDomains)) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createUniqueIndices failed to execute, error message: %{public}s", this, m_database.lastErrorMsg());
        return false;
    }
    return true;
}

bool ResourceLoadStatisticsDatabaseStore::createSchema()
{
    ASSERT(!RunLoop::isMain());

    if (!m_database.executeCommand(createObservedDomain)) {
        LOG_ERROR("Could not create ObservedDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createTopLevelDomains)) {
        LOG_ERROR("Could not create TopLevelDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createStorageAccessUnderTopFrameDomains)) {
        LOG_ERROR("Could not create StorageAccessUnderTopFrameDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsTo)) {
        LOG_ERROR("Could not create TopFrameUniqueRedirectsTo table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsFrom)) {
        LOG_ERROR("Could not create TopFrameUniqueRedirectsFrom table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createTopFrameLinkDecorationsFrom)) {
        LOG_ERROR("Could not create TopFrameLinkDecorationsFrom table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }
    
    if (!m_database.executeCommand(createTopFrameLoadedThirdPartyScripts)) {
        LOG_ERROR("Could not create TopFrameLoadedThirdPartyScripts table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }
    
    if (!m_database.executeCommand(createSubframeUnderTopFrameDomains)) {
        LOG_ERROR("Could not create SubframeUnderTopFrameDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUnderTopFrameDomains)) {
        LOG_ERROR("Could not create SubresourceUnderTopFrameDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUniqueRedirectsTo)) {
        LOG_ERROR("Could not create SubresourceUniqueRedirectsTo table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUniqueRedirectsFrom)) {
        LOG_ERROR("Could not create SubresourceUniqueRedirectsFrom table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }
    
    if (!createUniqueIndices())
        return false;

    return true;
}

bool ResourceLoadStatisticsDatabaseStore::prepareStatements()
{
    ASSERT(!RunLoop::isMain());
        
    if (m_observedDomainCount.prepare() != SQLITE_OK
        || m_insertObservedDomainStatement.prepare() != SQLITE_OK
        || m_insertTopLevelDomainStatement.prepare() != SQLITE_OK
        || m_domainIDFromStringStatement.prepare() != SQLITE_OK
        || m_subframeUnderTopFrameDomainExists.prepare() != SQLITE_OK
        || m_subresourceUnderTopFrameDomainExists.prepare() != SQLITE_OK
        || m_subresourceUniqueRedirectsToExists.prepare() != SQLITE_OK
        || m_updateLastSeenStatement.prepare() != SQLITE_OK
        || m_updateDataRecordsRemovedStatement.prepare() != SQLITE_OK
        || m_mostRecentUserInteractionStatement.prepare() != SQLITE_OK
        || m_updatePrevalentResourceStatement.prepare() != SQLITE_OK
        || m_isPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_updateVeryPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_isVeryPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_clearPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_hadUserInteractionStatement.prepare() != SQLITE_OK
        || m_updateGrandfatheredStatement.prepare() != SQLITE_OK
        || m_updateIsScheduledForAllButCookieDataRemovalStatement.prepare() != SQLITE_OK
        || m_isGrandfatheredStatement.prepare() != SQLITE_OK
        || m_findExpiredUserInteractionStatement.prepare() != SQLITE_OK
        || m_topFrameLinkDecorationsFromExists.prepare() != SQLITE_OK
        || m_topFrameLoadedThirdPartyScriptsExists.prepare() != SQLITE_OK
        || m_countPrevalentResourcesStatement.prepare() != SQLITE_OK
        || m_countPrevalentResourcesWithUserInteractionStatement.prepare() != SQLITE_OK
        || m_countPrevalentResourcesWithoutUserInteractionStatement.prepare() != SQLITE_OK
        || m_getResourceDataByDomainNameStatement.prepare() != SQLITE_OK
        || m_getAllDomainsStatement.prepare() != SQLITE_OK
        || m_domainStringFromDomainIDStatement.prepare() != SQLITE_OK
        || m_getAllSubStatisticsStatement.prepare() != SQLITE_OK
        || m_storageAccessExistsStatement.prepare() != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.prepare() != SQLITE_OK
        ) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::prepareStatements failed to prepare, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

bool ResourceLoadStatisticsDatabaseStore::insertObservedDomain(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    if (domainID(loadStatistics.registrableDomain)) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "ResourceLoadStatisticsDatabaseStore::insertObservedDomain can only be called on domains not in the database.");
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_insertObservedDomainStatement.bindText(RegistrableDomainIndex, loadStatistics.registrableDomain.string()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindDouble(LastSeenIndex, loadStatistics.lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(HadUserInteractionIndex, loadStatistics.hadUserInteraction) != SQLITE_OK
        || m_insertObservedDomainStatement.bindDouble(MostRecentUserInteractionTimeIndex, loadStatistics.mostRecentUserInteractionTime.secondsSinceEpoch().value()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(GrandfatheredIndex, loadStatistics.grandfathered) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(IsPrevalentIndex, loadStatistics.isPrevalentResource) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(IsVeryPrevalentIndex, loadStatistics.isVeryPrevalentResource) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(DataRecordsRemovedIndex, loadStatistics.dataRecordsRemoved) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(TimesAccessedAsFirstPartyDueToUserInteractionIndex, loadStatistics.timesAccessedAsFirstPartyDueToUserInteraction) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(TimesAccessedAsFirstPartyDueToStorageAccessAPIIndex, loadStatistics.timesAccessedAsFirstPartyDueToStorageAccessAPI) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(IsScheduledForAllButCookieDataRemovalIndex, loadStatistics.gotLinkDecorationFromPrevalentResource) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_insertObservedDomainStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to commit, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    int resetResult = m_insertObservedDomainStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    return true;
}

bool ResourceLoadStatisticsDatabaseStore::relationshipExists(WebCore::SQLiteStatement& statement, Optional<unsigned> firstDomainID, const RegistrableDomain& secondDomain) const
{
    if (!firstDomainID)
        return false;
    
    ASSERT(!RunLoop::isMain());

    if (statement.bindInt(1, *firstDomainID) != SQLITE_OK
        || statement.bindText(2, secondDomain.string()) != SQLITE_OK
        || statement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::relationshipExists failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    bool relationshipExists = !!statement.getColumnInt(0);

    int resetResult = statement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return relationshipExists;
}

bool ResourceLoadStatisticsDatabaseStore::insertDomainRelationship(WebCore::SQLiteStatement& statement, unsigned domainID, const RegistrableDomain& topFrame)
{
    ASSERT(!RunLoop::isMain());

    if (statement.bindInt(1, domainID) != SQLITE_OK
        || statement.bindText(2, topFrame.string()) != SQLITE_OK
        || statement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_insertDomainRelationshipStatement failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    int resetResult = statement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return true;
}

Optional<unsigned> ResourceLoadStatisticsDatabaseStore::domainID(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    unsigned domainID = 0;

    if (m_domainIDFromStringStatement.bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDFromString failed, error message: %{private}s", this, m_database.lastErrorMsg());
        return WTF::nullopt;
    }
    
    if (m_domainIDFromStringStatement.step() != SQLITE_ROW) {
        int resetResult = m_domainIDFromStringStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
        return WTF::nullopt;
    }

    domainID = m_domainIDFromStringStatement.getColumnInt(0);

    int resetResult = m_domainIDFromStringStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return domainID;
}

String ResourceLoadStatisticsDatabaseStore::ensureAndMakeDomainList(const HashSet<RegistrableDomain>& domainList)
{
    StringBuilder builder;
    
    for (auto& topFrameResource : domainList) {
        
        // Insert query will fail if top frame domain is not already in the database
        ensureResourceStatisticsForRegistrableDomain(topFrameResource);
        
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append('"');
        builder.append(topFrameResource.string());
        builder.append('"');
    }

    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList(const String& statement, const HashSet<RegistrableDomain>& domainList, unsigned domainID)
{
    SQLiteStatement insertRelationshipStatement(m_database, makeString(statement, ensureAndMakeDomainList(domainList), " );"));
    
    if (insertRelationshipStatement.prepare() != SQLITE_OK
        || insertRelationshipStatement.bindInt(1, domainID) != SQLITE_OK) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
    }
    
    if (statement.contains("REPLACE")) {
        if (insertRelationshipStatement.bindDouble(2, WallTime::now().secondsSinceEpoch().value()) != SQLITE_OK) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    if (insertRelationshipStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::insertDomainRelationships(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());
    auto registrableDomainID = domainID(loadStatistics.registrableDomain);
    
    if (!registrableDomainID)
        return;

    insertDomainRelationshipList(storageAccessUnderTopFrameDomainsQuery, loadStatistics.storageAccessUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, loadStatistics.topFrameUniqueRedirectsTo, registrableDomainID.value());
    insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, loadStatistics.topFrameUniqueRedirectsFrom, registrableDomainID.value());
    insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, loadStatistics.subframeUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(subresourceUnderTopFrameDomainsQuery, loadStatistics.subresourceUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, loadStatistics.subresourceUniqueRedirectsTo, registrableDomainID.value());
    insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, loadStatistics.subresourceUniqueRedirectsFrom, registrableDomainID.value());
    insertDomainRelationshipList(topFrameLinkDecorationsFromQuery, loadStatistics.topFrameLinkDecorationsFrom, registrableDomainID.value());
    insertDomainRelationshipList(topFrameLoadedThirdPartyScriptsQuery, loadStatistics.topFrameLoadedThirdPartyScripts, registrableDomainID.value());
}

void ResourceLoadStatisticsDatabaseStore::populateFromMemoryStore(const ResourceLoadStatisticsMemoryStore& memoryStore)
{
    ASSERT(!RunLoop::isMain());

    if (!isEmpty())
        return;

    auto& statisticsMap = memoryStore.data();
    for (const auto& statistic : statisticsMap)
        insertObservedDomain(statistic.value);

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries
    for (auto& statistic : statisticsMap)
        insertDomainRelationships(statistic.value);
}

void ResourceLoadStatisticsDatabaseStore::merge(WebCore::SQLiteStatement& current, const ResourceLoadStatistics& other)
{
    ASSERT(!RunLoop::isMain());

    auto currentRegistrableDomain = current.getColumnText(RegistrableDomainIndex);
    auto currentLastSeen = current.getColumnDouble(LastSeenIndex);
    auto currentMostRecentUserInteraction = current.getColumnDouble(MostRecentUserInteractionTimeIndex);
    bool currentGrandfathered = current.getColumnInt(GrandfatheredIndex);
    bool currentIsPrevalent = current.getColumnInt(IsPrevalentIndex);
    bool currentIsVeryPrevalent = current.getColumnInt(IsVeryPrevalentIndex);
    unsigned currentDataRecordsRemoved = current.getColumnInt(DataRecordsRemovedIndex);
    bool currentIsScheduledForAllButCookieDataRemoval = current.getColumnInt(IsScheduledForAllButCookieDataRemovalIndex);

    ASSERT(currentRegistrableDomain == other.registrableDomain.string());

    if (WallTime::fromRawSeconds(currentLastSeen) < other.lastSeen)
        updateLastSeen(other.registrableDomain, other.lastSeen);

    if (!other.hadUserInteraction) {
        // If most recent user interaction time has been reset do so here too.
        if (!other.mostRecentUserInteractionTime)
            setUserInteraction(other.registrableDomain, false, { });
    } else
        setUserInteraction(other.registrableDomain, true, std::max(WallTime::fromRawSeconds(currentMostRecentUserInteraction), other.mostRecentUserInteractionTime));

    if (other.grandfathered && !currentGrandfathered)
        setGrandfathered(other.registrableDomain, true);
    if (other.isPrevalentResource && !currentIsPrevalent)
        setPrevalentResource(other.registrableDomain);
    if (other.isVeryPrevalentResource && !currentIsVeryPrevalent)
        setVeryPrevalentResource(other.registrableDomain);
    if (other.dataRecordsRemoved > currentDataRecordsRemoved)
        updateDataRecordsRemoved(other.registrableDomain, other.dataRecordsRemoved);
    if (other.gotLinkDecorationFromPrevalentResource && !currentIsScheduledForAllButCookieDataRemoval)
        setIsScheduledForAllButCookieDataRemoval(other.registrableDomain, true);
}

void ResourceLoadStatisticsDatabaseStore::mergeStatistic(const ResourceLoadStatistics& statistic)
{
    ASSERT(!RunLoop::isMain());

    if (m_getResourceDataByDomainNameStatement.bindText(1, statistic.registrableDomain.string()) != SQLITE_OK
        || m_getResourceDataByDomainNameStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::mergeStatistic. Statement failed to bind or domain was not found, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    merge(m_getResourceDataByDomainNameStatement, statistic);
    resetStatement(m_getResourceDataByDomainNameStatement);
}

void ResourceLoadStatisticsDatabaseStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());

    for (auto& statistic : statistics) {
        if (!domainID(statistic.registrableDomain))
            insertObservedDomain(statistic);
        else
            mergeStatistic(statistic);
    }

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries.
    for (auto& statistic : statistics)
        insertDomainRelationships(statistic);
}

static const StringView joinSubStatisticsForSorting()
{
    return R"query(
        domainID,
        (countSubFrameUnderTopFrame + countSubResourceUnderTopFrame + countUniqueRedirectTo) as sum
        FROM (
        SELECT
            domainID,
            COUNT(DISTINCT f.topFrameDomainID) as countSubFrameUnderTopFrame,
            COUNT(DISTINCT r.topFrameDomainID) as countSubResourceUnderTopFrame,
            COUNT(DISTINCT toDomainID) as countUniqueRedirectTo
        FROM
        ObservedDomains o
        LEFT JOIN SubframeUnderTopFrameDomains f ON o.domainID = f.subFrameDomainID
        LEFT JOIN SubresourceUnderTopFrameDomains r ON o.domainID = r.subresourceDomainID
        LEFT JOIN SubresourceUniqueRedirectsTo u ON o.domainID = u.subresourceDomainID
        WHERE isPrevalent LIKE ?
        and hadUserInteraction LIKE ?
        GROUP BY domainID) ORDER BY sum DESC
        )query";
}

static SQLiteStatement makeMedianWithUIQuery(SQLiteDatabase& database)
{
    return SQLiteStatement(database, makeString("SELECT mostRecentUserInteractionTime FROM ObservedDomains INNER JOIN (SELECT ", joinSubStatisticsForSorting(), ") as q ON ObservedDomains.domainID = q.domainID LIMIT 1 OFFSET ?"));
}

Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> ResourceLoadStatisticsDatabaseStore::getThirdPartyDataForSpecificFirstPartyDomains(unsigned thirdPartyDomainID, const RegistrableDomain& thirdPartyDomain) const
{
    if (m_getAllSubStatisticsStatement.bindInt(1, thirdPartyDomainID) != SQLITE_OK
        || m_getAllSubStatisticsStatement.bindInt(2, thirdPartyDomainID) != SQLITE_OK
        || m_getAllSubStatisticsStatement.bindInt(3, thirdPartyDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getThirdPartyDataForSpecificFirstPartyDomain, error message: %{public}s", m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
    Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> thirdPartyDataForSpecificFirstPartyDomains;
    while (m_getAllSubStatisticsStatement.step() == SQLITE_ROW) {
        RegistrableDomain firstPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(m_getAllSubStatisticsStatement.getColumnInt(0)));
        thirdPartyDataForSpecificFirstPartyDomains.appendIfNotContains(WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty { firstPartyDomain, hasStorageAccess(firstPartyDomain, thirdPartyDomain), getMostRecentlyUpdatedTimestamp(thirdPartyDomain, firstPartyDomain) });
    }
    resetStatement(m_getAllSubStatisticsStatement);
    return thirdPartyDataForSpecificFirstPartyDomains;
}

static bool hasBeenThirdParty(unsigned timesUnderFirstParty)
{
    return timesUnderFirstParty > 0;
}

Vector<WebResourceLoadStatisticsStore::ThirdPartyData> ResourceLoadStatisticsDatabaseStore::aggregatedThirdPartyData() const
{
    ASSERT(!RunLoop::isMain());

    Vector<WebResourceLoadStatisticsStore::ThirdPartyData> thirdPartyDataList;
    SQLiteStatement sortedStatistics(m_database, makeString("SELECT ", joinSubStatisticsForSorting()));
    if (sortedStatistics.prepare() != SQLITE_OK
        || sortedStatistics.bindText(1, "1")
        || sortedStatistics.bindText(2, "%") != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::aggregatedThirdPartyData, error message: %{public}s", m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return thirdPartyDataList;
    }
    while (sortedStatistics.step() == SQLITE_ROW) {
        if (hasBeenThirdParty(sortedStatistics.getColumnInt(1))) {
            auto thirdPartyDomainID = sortedStatistics.getColumnInt(0);
            auto thirdPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(thirdPartyDomainID));
            thirdPartyDataList.append(WebResourceLoadStatisticsStore::ThirdPartyData { thirdPartyDomain, getThirdPartyDataForSpecificFirstPartyDomains(thirdPartyDomainID, thirdPartyDomain) });
        }
    }
    return thirdPartyDataList;
}

static std::pair<StringView, StringView> buildQueryStartAndEnd(PrevalentResourceDatabaseTelemetry::Statistic statistic)
{
    switch (statistic) {
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianSubFrameWithoutUI:
        return std::make_pair("SELECT countSubFrameUnderTopFrame FROM ObservedDomains o INNER JOIN(SELECT countSubFrameUnderTopFrame, ", ") as q ON o.domainID = q.domainID LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianSubResourceWithoutUI:
        return std::make_pair("SELECT countSubResourceUnderTopFrame FROM ObservedDomains o INNER JOIN(SELECT countSubResourceUnderTopFrame, ", ") as q ON o.domainID = q.domainID LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianUniqueRedirectsWithoutUI:
        return std::make_pair("SELECT countUniqueRedirectTo FROM ObservedDomains o INNER JOIN(SELECT countUniqueRedirectTo, ", ") as q ON o.domainID = q.domainID LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianDataRecordsRemovedWithoutUI:
        return std::make_pair("SELECT dataRecordsRemoved FROM (SELECT * FROM ObservedDomains o INNER JOIN(SELECT ", ") as q ON o.domainID = q.domainID) LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianTimesAccessedDueToUserInteractionWithoutUI:
        return std::make_pair("SELECT timesAccessedAsFirstPartyDueToUserInteraction FROM (SELECT * FROM ObservedDomains o INNER JOIN(SELECT ", ") as q ON o.domainID = q.domainID) LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianTimesAccessedDueToStorageAccessAPIWithoutUI:
        return std::make_pair("SELECT timesAccessedAsFirstPartyDueToStorageAccessAPI FROM (SELECT * FROM ObservedDomains o INNER JOIN(SELECT ", ") as q ON o.domainID = q.domainID) LIMIT 1 OFFSET ?");
    case PrevalentResourceDatabaseTelemetry::Statistic::NumberOfPrevalentResourcesWithUI:
        LOG_ERROR("ResourceLoadStatisticsDatabaseStore::makeMedianWithoutUIQuery was called for an incorrect statistic, undetermined query behavior will result.");
        RELEASE_ASSERT_NOT_REACHED();
    }
    return { };
}

static SQLiteStatement makeMedianWithoutUIQuery(SQLiteDatabase& database, PrevalentResourceDatabaseTelemetry::Statistic statistic)
{
    auto[queryStart, queryEnd] = buildQueryStartAndEnd(statistic);

    return SQLiteStatement(database, makeString(queryStart, joinSubStatisticsForSorting(), queryEnd));
}

static unsigned getMedianOfPrevalentResourcesWithUserInteraction(SQLiteDatabase& database, unsigned prevalentResourcesWithUserInteractionCount)
{
    SQLiteStatement medianDaysSinceUIStatement = makeMedianWithUIQuery(database);

    // Prepare
    if (medianDaysSinceUIStatement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Bind
    if (medianDaysSinceUIStatement.bindInt(1, 1) != SQLITE_OK || medianDaysSinceUIStatement.bindInt(2, 1) != SQLITE_OK || medianDaysSinceUIStatement.bindInt(3, (prevalentResourcesWithUserInteractionCount / 2) != SQLITE_OK)) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Step
    if (medianDaysSinceUIStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        return 0;
    }

    double rawSeconds = medianDaysSinceUIStatement.getColumnDouble(0);
    WallTime wallTime = WallTime::fromRawSeconds(rawSeconds);
    unsigned median = wallTime <= WallTime() ? 0 : std::floor((WallTime::now() - wallTime) / 24_h);

    if (prevalentResourcesWithUserInteractionCount & 1)
        return median;

    SQLiteStatement lowerMedianDaysSinceUIStatement = makeMedianWithUIQuery(database);

    // Prepare
    if (lowerMedianDaysSinceUIStatement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Bind
    if (lowerMedianDaysSinceUIStatement.bindInt(1, 1) != SQLITE_OK || lowerMedianDaysSinceUIStatement.bindInt(2, 1) != SQLITE_OK || lowerMedianDaysSinceUIStatement.bindInt(3, ((prevalentResourcesWithUserInteractionCount - 1) / 2)) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Step
    if (lowerMedianDaysSinceUIStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %{public}s", database.lastErrorMsg());
        return 0;
    }

    double rawSecondsLower = lowerMedianDaysSinceUIStatement.getColumnDouble(0);
    WallTime wallTimeLower = WallTime::fromRawSeconds(rawSecondsLower);
    return ((wallTimeLower <= WallTime() ? 0 : std::floor((WallTime::now() - wallTimeLower) / 24_h)) + median) / 2;
}

unsigned ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResources() const
{
    if (m_countPrevalentResourcesStatement.step() == SQLITE_ROW) {
        unsigned prevalentResourceCount = m_countPrevalentResourcesStatement.getColumnInt(0);
        if (prevalentResourceCount >= minimumPrevalentResourcesForTelemetry) {
            resetStatement(m_countPrevalentResourcesStatement);
            return prevalentResourceCount;
        }
    }
    resetStatement(m_countPrevalentResourcesStatement);
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResourcesWithUI() const
{
    if (m_countPrevalentResourcesWithUserInteractionStatement.step() == SQLITE_ROW) {
        int count = m_countPrevalentResourcesWithUserInteractionStatement.getColumnInt(0);
        resetStatement(m_countPrevalentResourcesWithUserInteractionStatement);
        return count;
    }
    resetStatement(m_countPrevalentResourcesWithUserInteractionStatement);
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getTopPrevelentResourceDaysSinceUI() const
{
    SQLiteStatement topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement(m_database, makeString("SELECT mostRecentUserInteractionTime FROM ObservedDomains INNER JOIN (SELECT ", joinSubStatisticsForSorting(), " LIMIT 1) as q ON ObservedDomains.domainID = q.domainID;"));
    
    // Prepare
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement query failed to prepare, error message: %{public}s", m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    // Bind
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.bindInt(1, 1) != SQLITE_OK
        || topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.bindInt(2, 1) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement query failed to bind, error message: %{public}s", m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    // Step
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement query failed to step, error message: %{public}s", m_database.lastErrorMsg());
        return 0;
    }
    
    double rawSeconds = topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.getColumnDouble(0);
    WallTime wallTime = WallTime::fromRawSeconds(rawSeconds);
    
    return wallTime <= WallTime() ? 0 : std::floor((WallTime::now() - wallTime) / 24_h);
}

static unsigned getMedianOfPrevalentResourceWithoutUserInteraction(SQLiteDatabase& database, unsigned bucketSize, PrevalentResourceDatabaseTelemetry::Statistic statistic, unsigned numberOfPrevalentResourcesWithoutUI)
{
    if (numberOfPrevalentResourcesWithoutUI < bucketSize)
        return 0;

    unsigned median = 0;
    SQLiteStatement getMedianStatistic = makeMedianWithoutUIQuery(database, statistic);

    if (getMedianStatistic.prepare() == SQLITE_OK) {
        if (getMedianStatistic.bindInt(1, 1) != SQLITE_OK
            || getMedianStatistic.bindInt(2, 0) != SQLITE_OK
            || getMedianStatistic.bindInt(3, (bucketSize / 2)) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::makeMedianWithoutUIQuery, error message: %{public}s", database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return 0;
        }
        if (getMedianStatistic.step() == SQLITE_ROW)
            median = getMedianStatistic.getColumnDouble(0);
    }

    if (bucketSize & 1)
        return median;

    SQLiteStatement getLowerMedianStatistic = makeMedianWithoutUIQuery(database, statistic);

    if (getLowerMedianStatistic.prepare() == SQLITE_OK) {
        if (getLowerMedianStatistic.bindInt(1, 1) != SQLITE_OK
            || getLowerMedianStatistic.bindInt(2, 0) != SQLITE_OK
            || getLowerMedianStatistic.bindInt(2, ((bucketSize-1) / 2)) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::makeMedianWithoutUIQuery, error message: %{public}s", database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return 0;
        }
        if (getLowerMedianStatistic.step() == SQLITE_ROW)
            return (getLowerMedianStatistic.getColumnDouble(0) + median) / 2;
    }

    return 0;
}

static unsigned getNumberOfPrevalentResourcesInTopResources(SQLiteDatabase& database, unsigned bucketSize)
{
    SQLiteStatement prevalentResourceCountInTop(database, makeString("SELECT COUNT(*) FROM (SELECT * FROM ObservedDomains o INNER JOIN(SELECT ", joinSubStatisticsForSorting(), ") as q on q.domainID = o.domainID LIMIT ?) as p WHERE p.hadUserInteraction = 1;"));

    if (prevalentResourceCountInTop.prepare() == SQLITE_OK) {
        if (prevalentResourceCountInTop.bindInt(1, 1) != SQLITE_OK
            || prevalentResourceCountInTop.bindText(2, "%") != SQLITE_OK
            || prevalentResourceCountInTop.bindInt(3, bucketSize) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResourcesInTopResources, error message: %{public}s", database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return 0;
        }

        if (prevalentResourceCountInTop.step() == SQLITE_ROW)
            return prevalentResourceCountInTop.getColumnInt(0);
    }

    return 0;
}

static unsigned makeStatisticQuery(SQLiteDatabase& database, PrevalentResourceDatabaseTelemetry::Statistic statistic, int bucketSize, unsigned totalWithUI, unsigned totalWithoutUI)
{
    switch (statistic) {
    case PrevalentResourceDatabaseTelemetry::Statistic::NumberOfPrevalentResourcesWithUI:
        return getNumberOfPrevalentResourcesInTopResources(database, bucketSize);
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianSubFrameWithoutUI:
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianSubResourceWithoutUI:
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianUniqueRedirectsWithoutUI:
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianDataRecordsRemovedWithoutUI:
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianTimesAccessedDueToUserInteractionWithoutUI:
    case PrevalentResourceDatabaseTelemetry::Statistic::MedianTimesAccessedDueToStorageAccessAPIWithoutUI:
        return getMedianOfPrevalentResourceWithoutUserInteraction(database, bucketSize, statistic, totalWithoutUI);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResourcesWithoutUI() const
{
    if (m_countPrevalentResourcesWithoutUserInteractionStatement.step() == SQLITE_ROW) {
        int count = m_countPrevalentResourcesWithoutUserInteractionStatement.getColumnInt(0);
        resetStatement(m_countPrevalentResourcesWithoutUserInteractionStatement);
        return count;
    }
    resetStatement(m_countPrevalentResourcesWithoutUserInteractionStatement);
    return 0;
}

void ResourceLoadStatisticsDatabaseStore::calculateTelemetryData(PrevalentResourceDatabaseTelemetry& data) const
{
    data.numberOfPrevalentResources = getNumberOfPrevalentResources();
    data.numberOfPrevalentResourcesWithUserInteraction = getNumberOfPrevalentResourcesWithUI();
    data.numberOfPrevalentResourcesWithoutUserInteraction = getNumberOfPrevalentResourcesWithoutUI();
    data.topPrevalentResourceWithUserInteractionDaysSinceUserInteraction = getTopPrevelentResourceDaysSinceUI();
    data.medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction = getMedianOfPrevalentResourcesWithUserInteraction(m_database, data.numberOfPrevalentResourcesWithUserInteraction);

    for (unsigned bucketIndex = 0; bucketIndex < bucketSizes.size(); bucketIndex++) {
        unsigned bucketSize = bucketSizes[bucketIndex];

        if (data.numberOfPrevalentResourcesWithoutUserInteraction < bucketSize)
            return;

        for (unsigned statisticIndex = 0; statisticIndex < numberOfStatistics; statisticIndex++) {
            auto statistic = static_cast<PrevalentResourceDatabaseTelemetry::Statistic>(statisticIndex);
            data.statistics[statisticIndex][bucketIndex] = makeStatisticQuery(m_database, statistic, bucketSize, data.numberOfPrevalentResourcesWithUserInteraction, data.numberOfPrevalentResourcesWithoutUserInteraction);
        }
    }
}

void ResourceLoadStatisticsDatabaseStore::calculateAndSubmitTelemetry() const
{
    ASSERT(!RunLoop::isMain());

    if (parameters().shouldSubmitTelemetry) {
        PrevalentResourceDatabaseTelemetry prevalentResourceDatabaseTelemetry;
        calculateTelemetryData(prevalentResourceDatabaseTelemetry);
        WebResourceLoadStatisticsTelemetry::submitTelemetry(*this, prevalentResourceDatabaseTelemetry);
    }
}

static String domainsToString(const HashSet<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (const auto& domainName : domains) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append('"');
        builder.append(domainName.string());
        builder.append('"');
    }

    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&& domains)
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement domainsToUpdateStatement(m_database, makeString("UPDATE ObservedDomains SET dataRecordsRemoved = dataRecordsRemoved + 1 WHERE registrableDomain IN (", domainsToString(domains), ")"));
    if (domainsToUpdateStatement.prepare() != SQLITE_OK
        || domainsToUpdateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::incrementStatisticsForDomains failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

unsigned ResourceLoadStatisticsDatabaseStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(unsigned primaryDomainID, StdSet<unsigned>& nonPrevalentRedirectionSources, unsigned numberOfRecursiveCalls)
{
    ASSERT(!RunLoop::isMain());

    if (numberOfRecursiveCalls >= maxNumberOfRecursiveCallsInRedirectTraceBack) {
        RELEASE_LOG(ResourceLoadStatistics, "Hit %u recursive calls in redirect backtrace. Returning early.", maxNumberOfRecursiveCallsInRedirectTraceBack);
        return numberOfRecursiveCalls;
    }
    
    ++numberOfRecursiveCalls;

    StdSet<unsigned> newlyIdentifiedDomains;
    SQLiteStatement findSubresources(m_database, "SELECT SubresourceUniqueRedirectsFrom.fromDomainID from SubresourceUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsFrom.fromDomainID WHERE subresourceDomainID = ? AND ObservedDomains.isPrevalent = 0"_s);
    if (findSubresources.prepare() != SQLITE_OK
        || findSubresources.bindInt(1, primaryDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    while (findSubresources.step() == SQLITE_ROW) {
        int newDomainID = findSubresources.getColumnInt(0);
        auto insertResult = nonPrevalentRedirectionSources.insert(newDomainID);
        if (insertResult.second)
            newlyIdentifiedDomains.insert(newDomainID);
    }

    SQLiteStatement findTopFrames(m_database, "SELECT TopFrameUniqueRedirectsFrom.fromDomainID from TopFrameUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = TopFrameUniqueRedirectsFrom.fromDomainID WHERE targetDomainID = ? AND ObservedDomains.isPrevalent = 0"_s);
    if (findTopFrames.prepare() != SQLITE_OK
        || findTopFrames.bindInt(1, primaryDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    while (findTopFrames.step() == SQLITE_ROW) {
        int newDomainID = findTopFrames.getColumnInt(0);
        auto insertResult = nonPrevalentRedirectionSources.insert(newDomainID);
        if (insertResult.second)
            newlyIdentifiedDomains.insert(newDomainID);
    }

    if (newlyIdentifiedDomains.empty())
        return numberOfRecursiveCalls;

    for (auto domainID : newlyIdentifiedDomains)
        numberOfRecursiveCalls = recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(domainID, nonPrevalentRedirectionSources, numberOfRecursiveCalls);

    return numberOfRecursiveCalls;
}

template <typename IteratorType>
static String buildList(const WTF::IteratorRange<IteratorType>& values)
{
    StringBuilder builder;
    for (auto domainID : values) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.appendNumber(domainID);
    }
    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::markAsPrevalentIfHasRedirectedToPrevalent()
{
    ASSERT(!RunLoop::isMain());

    StdSet<unsigned> prevalentDueToRedirect;
    SQLiteStatement subresourceRedirectStatement(m_database, "SELECT DISTINCT SubresourceUniqueRedirectsTo.subresourceDomainID FROM SubresourceUniqueRedirectsTo JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsTo.toDomainID AND ObservedDomains.isPrevalent = 1"_s);
    if (subresourceRedirectStatement.prepare() == SQLITE_OK) {
        while (subresourceRedirectStatement.step() == SQLITE_ROW)
            prevalentDueToRedirect.insert(subresourceRedirectStatement.getColumnInt(0));
    }

    SQLiteStatement topFrameRedirectStatement(m_database, "SELECT DISTINCT TopFrameUniqueRedirectsTo.sourceDomainID FROM TopFrameUniqueRedirectsTo JOIN ObservedDomains ON ObservedDomains.domainID = TopFrameUniqueRedirectsTo.toDomainID AND ObservedDomains.isPrevalent = 1"_s);
    if (topFrameRedirectStatement.prepare() == SQLITE_OK) {
        while (topFrameRedirectStatement.step() == SQLITE_ROW)
            prevalentDueToRedirect.insert(topFrameRedirectStatement.getColumnInt(0));
    }

    SQLiteStatement markPrevalentStatement(m_database, makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN (", buildList(WTF::IteratorRange<StdSet<unsigned>::iterator>(prevalentDueToRedirect.begin(), prevalentDueToRedirect.end())), ")"));
    if (markPrevalentStatement.prepare() != SQLITE_OK
        || markPrevalentStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::markAsPrevalentIfHasRedirectedToPrevalent failed to execute, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

HashMap<unsigned, ResourceLoadStatisticsDatabaseStore::NotVeryPrevalentResources> ResourceLoadStatisticsDatabaseStore::findNotVeryPrevalentResources()
{
    ASSERT(!RunLoop::isMain());

    HashMap<unsigned, NotVeryPrevalentResources> results;
    SQLiteStatement notVeryPrevalentResourcesStatement(m_database, "SELECT domainID, registrableDomain, isPrevalent FROM ObservedDomains WHERE isVeryPrevalent = 0"_s);
    if (notVeryPrevalentResourcesStatement.prepare() == SQLITE_OK) {
        while (notVeryPrevalentResourcesStatement.step() == SQLITE_ROW) {
            unsigned key = static_cast<unsigned>(notVeryPrevalentResourcesStatement.getColumnInt(0));
            NotVeryPrevalentResources value({ RegistrableDomain::uncheckedCreateFromRegistrableDomainString(notVeryPrevalentResourcesStatement.getColumnText(1))
                , notVeryPrevalentResourcesStatement.getColumnInt(2) ? ResourceLoadPrevalence::High : ResourceLoadPrevalence::Low
                , 0, 0, 0, 0 });
            results.add(key, value);
        }
    }

    StringBuilder builder;
    for (auto value : results.keys()) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.appendNumber(value);
    }

    auto domainIDsOfInterest = builder.toString();

    SQLiteStatement subresourceUnderTopFrameDomainsStatement(m_database, makeString("SELECT subresourceDomainID, COUNT(topFrameDomainID) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID IN (", domainIDsOfInterest, ") GROUP BY subresourceDomainID"));
    if (subresourceUnderTopFrameDomainsStatement.prepare() == SQLITE_OK) {
        while (subresourceUnderTopFrameDomainsStatement.step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement.getColumnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUnderTopFrameDomainsCount = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement.getColumnInt(1));
        }
    }

    SQLiteStatement subresourceUniqueRedirectsToCountStatement(m_database, makeString("SELECT subresourceDomainID, COUNT(toDomainID) FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID IN (", domainIDsOfInterest, ") GROUP BY subresourceDomainID"));
    if (subresourceUniqueRedirectsToCountStatement.prepare() == SQLITE_OK) {
        while (subresourceUniqueRedirectsToCountStatement.step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement.getColumnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUniqueRedirectsToCount = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement.getColumnInt(1));
        }
    }

    SQLiteStatement subframeUnderTopFrameDomainsCountStatement(m_database, makeString("SELECT subframeDomainID, COUNT(topFrameDomainID) FROM SubframeUnderTopFrameDomains WHERE subframeDomainID IN (", domainIDsOfInterest, ") GROUP BY subframeDomainID"));
    if (subframeUnderTopFrameDomainsCountStatement.prepare() == SQLITE_OK) {
        while (subframeUnderTopFrameDomainsCountStatement.step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement.getColumnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subframeUnderTopFrameDomainsCount = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement.getColumnInt(1));
        }
    }

    SQLiteStatement topFrameUniqueRedirectsToCountStatement(m_database, makeString("SELECT sourceDomainID, COUNT(toDomainID) FROM TopFrameUniqueRedirectsTo WHERE sourceDomainID IN (", domainIDsOfInterest, ") GROUP BY sourceDomainID"));
    if (topFrameUniqueRedirectsToCountStatement.prepare() == SQLITE_OK) {
        while (topFrameUniqueRedirectsToCountStatement.step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement.getColumnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.topFrameUniqueRedirectsToCount = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement.getColumnInt(1));
        }
    }

    return results;
}

void ResourceLoadStatisticsDatabaseStore::reclassifyResources()
{
    ASSERT(!RunLoop::isMain());

    auto notVeryPrevalentResources = findNotVeryPrevalentResources();

    for (auto& resourceStatistic : notVeryPrevalentResources.values()) {
        if (shouldSkip(resourceStatistic.registrableDomain))
            continue;

        auto newPrevalence = classifier().calculateResourcePrevalence(resourceStatistic.subresourceUnderTopFrameDomainsCount, resourceStatistic.subresourceUniqueRedirectsToCount, resourceStatistic.subframeUnderTopFrameDomainsCount, resourceStatistic.topFrameUniqueRedirectsToCount, resourceStatistic.prevalence);
        if (newPrevalence != resourceStatistic.prevalence)
            setPrevalentResource(resourceStatistic.registrableDomain, newPrevalence);
    }
}

void ResourceLoadStatisticsDatabaseStore::classifyPrevalentResources()
{
    ASSERT(!RunLoop::isMain());
    ensurePrevalentResourcesForDebugMode();
    markAsPrevalentIfHasRedirectedToPrevalent();
    reclassifyResources();
}

void ResourceLoadStatisticsDatabaseStore::syncStorageIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    m_database.runVacuumCommand();
}

void ResourceLoadStatisticsDatabaseStore::syncStorageImmediately()
{
    ASSERT(!RunLoop::isMain());
    m_database.runVacuumCommand();
}

bool ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const TopFrameDomain& topFrameDomain, const SubFrameDomain& subFrameDomain) const
{
    return relationshipExists(m_storageAccessExistsStatement, domainID(subFrameDomain), topFrameDomain);
}

void ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(subFrameDomain);

    switch (cookieAccess(subFrameDomain, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        completionHandler(false);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->hasCookies(subFrameDomain, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
                store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                    completionHandler(result);
                });
            });
        });
        return;
    case CookieAccess::OnlyIfGranted:
        // Handled below.
        break;
    };

    RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callHasStorageAccessForFrameHandler(subFrameDomain, topFrameDomain, frameID.value(), pageID, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    
    switch (cookieAccess(subFrameDomain, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "Cannot grant storage access to %{private}s since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "No need to grant storage access to %{private}s since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies.", subFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    case CookieAccess::OnlyIfGranted:
        // Handled below.
        break;
    }

    auto userWasPromptedEarlier = hasUserGrantedStorageAccessThroughPrompt(subFrameStatus.second, topFrameDomain);
    if (userWasPromptedEarlier == StorageAccessPromptWasShown::No) {
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "About to ask the user whether they want to grant storage access to %{private}s under %{private}s or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes)
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "Storage access was granted to %{private}s under %{private}s.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());

    SQLiteStatement incrementStorageAccess(m_database, "UPDATE ObservedDomains SET timesAccessedAsFirstPartyDueToStorageAccessAPI = timesAccessedAsFirstPartyDueToStorageAccessAPI + 1 WHERE domainID = ?");
    if (incrementStorageAccess.prepare() != SQLITE_OK
        || incrementStorageAccess.bindInt(1, subFrameStatus.second) != SQLITE_OK
        || incrementStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "[Temporary combatibility fix] Storage access was granted for %{private}s under opener page from %{private}s, with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID, StorageAccessPromptWasShown::No, [](StorageAccessWasGranted) { });
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (promptWasShown == StorageAccessPromptWasShown::Yes) {
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
        insertDomainRelationshipList(storageAccessUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), subFrameStatus.second);
    }

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, WTFMove(completionHandler));
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessWasGranted::Yes);
        return;
    }

    if (promptWasShownNowOrEarlier == StorageAccessPromptWasShown::Yes) {
#ifndef NDEBUG
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
        ASSERT(hasUserGrantedStorageAccessThroughPrompt(subFrameStatus.second, topFrameDomain) == StorageAccessPromptWasShown::Yes);
#endif
        setUserInteraction(subFrameDomain, true, WallTime::now());
    }

    RunLoop::main().dispatch([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, store = makeRef(store()), completionHandler = WTFMove(completionHandler)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler), store = store.copyRef()](StorageAccessWasGranted wasGranted) mutable {
            store->statisticsQueue().dispatch([wasGranted, completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(wasGranted);
            });
        });
    });

}

void ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(!RunLoop::isMain());

    for (auto& registrableDomain : domains)
        ensureResourceStatisticsForRegistrableDomain(registrableDomain);

    SQLiteStatement domainsToUpdateStatement(m_database, makeString("UPDATE ObservedDomains SET grandfathered = 1 WHERE registrableDomain IN (", domainsToString(domains), ")"));
    if (domainsToUpdateStatement.prepare() != SQLITE_OK
        || domainsToUpdateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode()
{
    ASSERT(!RunLoop::isMain());

    if (!debugModeEnabled())
        return { };

    Vector<RegistrableDomain> primaryDomainsToBlock;
    primaryDomainsToBlock.reserveInitialCapacity(2);

    ensureResourceStatisticsForRegistrableDomain(debugStaticPrevalentResource());
    setPrevalentResource(debugStaticPrevalentResource(), ResourceLoadPrevalence::High);
    primaryDomainsToBlock.uncheckedAppend(debugStaticPrevalentResource());

    if (!debugManualPrevalentResource().isEmpty()) {
        ensureResourceStatisticsForRegistrableDomain(debugManualPrevalentResource());
        setPrevalentResource(debugManualPrevalentResource(), ResourceLoadPrevalence::High);
        primaryDomainsToBlock.uncheckedAppend(debugManualPrevalentResource());
        RELEASE_LOG_INFO(ITPDebug, "Did set %{private}s as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
    }

    return primaryDomainsToBlock;
}

void ResourceLoadStatisticsDatabaseStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndTopFrameDomainsSameSite = targetDomain == topFrameDomain;
    bool areTargetAndSourceDomainsSameSite = targetDomain == sourceDomain;

    bool statisticsWereUpdated = false;
    if (!isMainFrame && !(areTargetAndTopFrameDomainsSameSite || areTargetAndSourceDomainsSameSite)) {
        auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        updateLastSeen(targetDomain, ResourceLoadStatistics::reduceTimeResolution(WallTime::now()));
        insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), targetResult.second);
        statisticsWereUpdated = true;
    }

    if (!areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            bool wasNavigatedAfterShortDelayWithoutUserInteraction = !wasPotentiallyInitiatedByUser && delayAfterMainFrameDocumentLoad < parameters().minDelayAfterMainFrameDocumentLoadToNotBeARedirect;
            if (isRedirect || wasNavigatedAfterShortDelayWithoutUserInteraction) {
                auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
                auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
                insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), redirectingDomainResult.second);
                insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), targetResult.second);
                statisticsWereUpdated = true;
            }
        } else if (isRedirect) {
            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), redirectingDomainResult.second);
            insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), targetResult.second);
            statisticsWereUpdated = true;
        }
    }

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsDatabaseStore::logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain& fromDomain, const NavigatedToDomain& toDomain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(fromDomain != toDomain);

    auto toDomainResult = ensureResourceStatisticsForRegistrableDomain(toDomain);
    insertDomainRelationshipList(topFrameLinkDecorationsFromQuery, HashSet<RegistrableDomain>({ fromDomain }), toDomainResult.second);
    
    if (isPrevalentResource(fromDomain))
        setIsScheduledForAllButCookieDataRemoval(toDomain, true);
}

void ResourceLoadStatisticsDatabaseStore::setUserInteraction(const RegistrableDomain& domain, bool hadUserInteraction, WallTime mostRecentInteraction)
{
    ASSERT(!RunLoop::isMain());

    if (m_mostRecentUserInteractionStatement.bindInt(1, hadUserInteraction) != SQLITE_OK
        || m_mostRecentUserInteractionStatement.bindDouble(2, mostRecentInteraction.secondsSinceEpoch().value()) != SQLITE_OK
        || m_mostRecentUserInteractionStatement.bindText(3, domain.string()) != SQLITE_OK
        || m_mostRecentUserInteractionStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setUserInteraction, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    int resetResult = m_mostRecentUserInteractionStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

void ResourceLoadStatisticsDatabaseStore::logUserInteraction(const TopFrameDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool didHavePreviousUserInteraction = hasHadUserInteraction(domain, OperatingDatesWindow::Long);
    ensureResourceStatisticsForRegistrableDomain(domain);
    setUserInteraction(domain, true, WallTime::now());
    if (didHavePreviousUserInteraction) {
        completionHandler();
        return;
    }
    updateCookieBlocking(WTFMove(completionHandler));
}

void ResourceLoadStatisticsDatabaseStore::clearUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain);
    setUserInteraction(domain, false, { });

    SQLiteStatement removeStorageAccess(m_database, "DELETE FROM StorageAccessUnderTopFrameDomains WHERE domainID = ?");
    if (removeStorageAccess.prepare() != SQLITE_OK
        || removeStorageAccess.bindInt(1, targetResult.second) != SQLITE_OK
        || removeStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logUserInteraction failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    // Update cookie blocking unconditionally since a call to hasHadUserInteraction()
    // to check the previous user interaction status could call clearUserInteraction(),
    // blowing the call stack.
    updateCookieBlocking(WTFMove(completionHandler));
}

bool ResourceLoadStatisticsDatabaseStore::hasHadUserInteraction(const RegistrableDomain& domain, OperatingDatesWindow operatingDatesWindow)
{
    ASSERT(!RunLoop::isMain());

    if (m_hadUserInteractionStatement.bindText(1, domain.string()) != SQLITE_OK
        || m_hadUserInteractionStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_hadUserInteractionStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());

        int resetResult = m_hadUserInteractionStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
        return false;
    }

    bool hadUserInteraction = !!m_hadUserInteractionStatement.getColumnInt(0);
    if (!hadUserInteraction) {
        int resetResult = m_hadUserInteractionStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
        return false;
    }
    
    WallTime mostRecentUserInteractionTime = WallTime::fromRawSeconds(m_hadUserInteractionStatement.getColumnDouble(1));

    if (hasStatisticsExpired(mostRecentUserInteractionTime, operatingDatesWindow)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        clearUserInteraction(domain, [] { });
        hadUserInteraction = false;
    }
    
    int resetResult = m_hadUserInteractionStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    return hadUserInteraction;
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const RegistrableDomain& domain, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());
    if (shouldSkip(domain))
        return;

    if (m_updatePrevalentResourceStatement.bindInt(1, 1) != SQLITE_OK
        || m_updatePrevalentResourceStatement.bindText(2, domain.string()) != SQLITE_OK
        || m_updatePrevalentResourceStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updatePrevalentResourceStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_updatePrevalentResourceStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    if (newPrevalence == ResourceLoadPrevalence::VeryHigh) {
        if (m_updateVeryPrevalentResourceStatement.bindInt(1, 1) != SQLITE_OK
            || m_updateVeryPrevalentResourceStatement.bindText(2, domain.string()) != SQLITE_OK
            || m_updateVeryPrevalentResourceStatement.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updateVeryPrevalentResourceStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }

        int resetResult = m_updateVeryPrevalentResourceStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    }

    StdSet<unsigned> nonPrevalentRedirectionSources;
    recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(domainID(domain).value(), nonPrevalentRedirectionSources, 0);
    setDomainsAsPrevalent(WTFMove(nonPrevalentRedirectionSources));
}

void ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent(StdSet<unsigned>&& domains)
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement domainsToUpdateStatement(m_database, makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN (", buildList(WTF::IteratorRange<StdSet<unsigned>::iterator>(domains.begin(), domains.end())), ")"));
    if (domainsToUpdateStatement.prepare() != SQLITE_OK
        || domainsToUpdateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    if (dataRecordsBeingRemoved()) {
        m_dataRecordRemovalCompletionHandlers.append([this, completionHandler = WTFMove(completionHandler)]() mutable {
            dumpResourceLoadStatistics(WTFMove(completionHandler));
        });
        return;
    }

    StringBuilder result;
    result.appendLiteral("Resource load statistics:\n\n");
    while (m_getAllDomainsStatement.step() == SQLITE_ROW)
        resourceToString(result, m_getAllDomainsStatement.getColumnText(0));

    auto thirdPartyData = aggregatedThirdPartyData();
    if (!thirdPartyData.isEmpty()) {
        result.append("\nITP Data:\n");
        for (auto thirdParty : thirdPartyData) {
            result.append(thirdParty.toString());
            result.append('\n');
        }
    }
    resetStatement(m_getAllDomainsStatement);
    completionHandler(result.toString());
}

bool ResourceLoadStatisticsDatabaseStore::predicateValueForDomain(WebCore::SQLiteStatement& predicateStatement, const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());
    
    if (predicateStatement.bindText(1, domain.string()) != SQLITE_OK
        || predicateStatement.step() != SQLITE_ROW) {

        int resetResult = predicateStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::predicateValueForDomain failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        return false;
    }

    bool result = !!predicateStatement.getColumnInt(0);
    int resetResult = predicateStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    return result;
}

bool ResourceLoadStatisticsDatabaseStore::isPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    return predicateValueForDomain(m_isPrevalentResourceStatement, domain);
}

bool ResourceLoadStatisticsDatabaseStore::isVeryPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    return predicateValueForDomain(m_isVeryPrevalentResourceStatement, domain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsSubresourceUnder(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    return relationshipExists(m_subresourceUnderTopFrameDomainExists, domainID(subresourceDomain), topFrameDomain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsSubFrameUnder(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    return relationshipExists(m_subframeUnderTopFrameDomainExists, domainID(subFrameDomain), topFrameDomain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsRedirectingTo(const RedirectedFromDomain& redirectedFromDomain, const RedirectedToDomain& redirectedToDomain) const
{
    ASSERT(!RunLoop::isMain());

    return relationshipExists(m_subresourceUniqueRedirectsToExists, domainID(redirectedFromDomain), redirectedToDomain);
}

void ResourceLoadStatisticsDatabaseStore::clearPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(domain);
    
    if (m_clearPrevalentResourceStatement.bindText(1, domain.string()) != SQLITE_OK
        || m_clearPrevalentResourceStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_clearPrevalentResourceStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

void ResourceLoadStatisticsDatabaseStore::setGrandfathered(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(domain);
    
    if (m_updateGrandfatheredStatement.bindInt(1, value) != SQLITE_OK
        || m_updateGrandfatheredStatement.bindText(2, domain.string()) != SQLITE_OK
        || m_updateGrandfatheredStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_updateGrandfatheredStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

void ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(domain);
    
    if (m_updateIsScheduledForAllButCookieDataRemovalStatement.bindInt(1, value) != SQLITE_OK
        || m_updateIsScheduledForAllButCookieDataRemovalStatement.bindText(2, domain.string()) != SQLITE_OK
        || m_updateIsScheduledForAllButCookieDataRemovalStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    resetStatement(m_updateIsScheduledForAllButCookieDataRemovalStatement);
}
Seconds ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp(const RegistrableDomain& subDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    Optional<unsigned> subFrameDomainID = domainID(subDomain);
    Optional<unsigned> topFrameDomainID = domainID(topFrameDomain);

    if (!subFrameDomainID || !topFrameDomainID)
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp };

    if (m_getMostRecentlyUpdatedTimestampStatement.bindInt(1, *subFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(2, *topFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(3, *subFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(4, *topFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(5, *subFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(6, *topFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(7, *subFrameDomainID) != SQLITE_OK
        || m_getMostRecentlyUpdatedTimestampStatement.bindInt(8, *topFrameDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };
    }
    if (m_getMostRecentlyUpdatedTimestampStatement.step() != SQLITE_ROW) {
        resetStatement(m_getMostRecentlyUpdatedTimestampStatement);
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };
    }
    double mostRecentlyUpdatedTimestamp = m_getMostRecentlyUpdatedTimestampStatement.getColumnDouble(0);
    resetStatement(m_getMostRecentlyUpdatedTimestampStatement);
    return Seconds { mostRecentlyUpdatedTimestamp };
}

bool ResourceLoadStatisticsDatabaseStore::isGrandfathered(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    return predicateValueForDomain(m_isGrandfatheredStatement, domain);
}

void ResourceLoadStatisticsDatabaseStore::setSubframeUnderTopFrameDomain(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);

    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subresourceUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), result.second);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), result.second);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), result.second);
}

std::pair<ResourceLoadStatisticsDatabaseStore::AddedRecord, unsigned> ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (m_domainIDFromStringStatement.bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { AddedRecord::No, 0 };
    }
    
    if (m_domainIDFromStringStatement.step() == SQLITE_ROW) {
        unsigned domainID = m_domainIDFromStringStatement.getColumnInt(0);

        int resetResult = m_domainIDFromStringStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
        return { AddedRecord::No, domainID };
    }

    int resetResult = m_domainIDFromStringStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    ResourceLoadStatistics newObservation(domain);
    insertObservedDomain(newObservation);

    return { AddedRecord::Yes, domainID(domain).value() };
}

void ResourceLoadStatisticsDatabaseStore::clearDatabaseContents()
{
    m_database.clearAllTables();

    if (!createSchema()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::clearDatabaseContents failed, error message: %{private}s, database path: %{private}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    clearDatabaseContents();
    clearOperatingDates();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator = callbackAggregator.copyRef()] { });

    auto registrableDomainsToBlockAndDeleteCookiesFor = ensurePrevalentResourcesForDebugMode();
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { }, { } };
    updateCookieBlockingForDomains(domainsToBlock, [callbackAggregator = callbackAggregator.copyRef()] { });
}

bool ResourceLoadStatisticsDatabaseStore::areAllThirdPartyCookiesBlockedUnder(const TopFrameDomain& topFrameDomain)
{
    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::All)
        return true;

    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction && !hasHadUserInteraction(topFrameDomain, OperatingDatesWindow::Long))
        return true;

    return false;
}

CookieAccess ResourceLoadStatisticsDatabaseStore::cookieAccess(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement statement(m_database, "SELECT isPrevalent, hadUserInteraction FROM ObservedDomains WHERE registrableDomain = ?");
    if (statement.prepare() != SQLITE_OK
        || statement.bindText(1, subresourceDomain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::cookieAccess failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    bool hasNoEntry = statement.step() != SQLITE_ROW;
    bool isPrevalent = !hasNoEntry && !!statement.getColumnInt(0);
    bool hadUserInteraction = !hasNoEntry && statement.getColumnInt(1) ? true : false;

    if (!areAllThirdPartyCookiesBlockedUnder(topFrameDomain) && !isPrevalent)
        return CookieAccess::BasedOnCookiePolicy;

    if (!hadUserInteraction)
        return CookieAccess::CannotRequest;

    return CookieAccess::OnlyIfGranted;
}

StorageAccessPromptWasShown ResourceLoadStatisticsDatabaseStore::hasUserGrantedStorageAccessThroughPrompt(unsigned requestingDomainID, const RegistrableDomain& firstPartyDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto firstPartyPrimaryDomainID = domainID(firstPartyDomain).value();

    SQLiteStatement statement(m_database, "SELECT COUNT(*) FROM StorageAccessUnderTopFrameDomains WHERE domainID = ? AND topLevelDomainID = ?");
    if (statement.prepare() != SQLITE_OK
        || statement.bindInt(1, requestingDomainID) != SQLITE_OK
        || statement.bindInt(2, firstPartyPrimaryDomainID) != SQLITE_OK
        || statement.step() != SQLITE_ROW)
        return StorageAccessPromptWasShown::No;

    return !!statement.getColumnInt(0) ? StorageAccessPromptWasShown::Yes : StorageAccessPromptWasShown::No;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsToBlockAndDeleteCookiesFor() const
{
    ASSERT(!RunLoop::isMain());
    
    Vector<RegistrableDomain> results;
    SQLiteStatement statement(m_database, "SELECT registrableDomain FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 0"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(0)));
    
    return results;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsToBlockButKeepCookiesFor() const
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> results;
    SQLiteStatement statement(m_database, "SELECT registrableDomain FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 1"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(0)));

    return results;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsWithUserInteractionAsFirstParty() const
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> results;
    SQLiteStatement statement(m_database, "SELECT registrableDomain FROM ObservedDomains WHERE hadUserInteraction = 1"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(0)));

    return results;
}

void ResourceLoadStatisticsDatabaseStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToBlockAndDeleteCookiesFor = this->domainsToBlockAndDeleteCookiesFor();
    auto domainsToBlockButKeepCookiesFor = this->domainsToBlockButKeepCookiesFor();
    auto domainsWithUserInteractionAsFirstParty = this->domainsWithUserInteractionAsFirstParty();

    if (domainsToBlockAndDeleteCookiesFor.isEmpty() && domainsToBlockButKeepCookiesFor.isEmpty() && domainsWithUserInteractionAsFirstParty.isEmpty()) {
        completionHandler();
        return;
    }

    RegistrableDomainsToBlockCookiesFor domainsToBlock { domainsToBlockAndDeleteCookiesFor, domainsToBlockButKeepCookiesFor, domainsWithUserInteractionAsFirstParty };

    if (debugLoggingEnabled() && !domainsToBlockAndDeleteCookiesFor.isEmpty() && !domainsToBlockButKeepCookiesFor.isEmpty())
        debugLogDomainsInBatches("block", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(store()), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
                if (!weakThis)
                    return;
                RELEASE_LOG_INFO_IF(weakThis->debugLoggingEnabled(), ITPDebug, "Done updating cookie blocking.");
            });
        });
    });
}

Vector<ResourceLoadStatisticsDatabaseStore::DomainData> ResourceLoadStatisticsDatabaseStore::domains() const
{
    ASSERT(!RunLoop::isMain());
    
    Vector<DomainData> results;
    SQLiteStatement statement(m_database, "SELECT domainID, registrableDomain, mostRecentUserInteractionTime, hadUserInteraction, grandfathered, isScheduledForAllButCookieDataRemoval FROM ObservedDomains"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW) {
        results.append({ static_cast<unsigned>(statement.getColumnInt(0))
            , RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(1))
            , WallTime::fromRawSeconds(statement.getColumnDouble(2))
            , statement.getColumnInt(3) ? true : false
            , statement.getColumnInt(4) ? true : false
            , statement.getColumnInt(5) ? true : false
        });
    }
    
    return results;
}

Vector<unsigned> ResourceLoadStatisticsDatabaseStore::findExpiredUserInteractions() const
{
    ASSERT(!RunLoop::isMain());

    Vector<unsigned> results;
    Optional<Seconds> expirationDateTime = statisticsEpirationTime();
    if (!expirationDateTime)
        return results;

    if (m_findExpiredUserInteractionStatement.bindDouble(1, expirationDateTime.value().value()) != SQLITE_OK)
        return results;

    while (m_findExpiredUserInteractionStatement.step() == SQLITE_ROW)
        results.append(m_findExpiredUserInteractionStatement.getColumnInt(0));

    int resetResult = m_findExpiredUserInteractionStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    return results;
}

void ResourceLoadStatisticsDatabaseStore::clearExpiredUserInteractions()
{
    ASSERT(!RunLoop::isMain());

    auto expiredRecords = findExpiredUserInteractions();
    if (expiredRecords.isEmpty())
        return;

    auto expiredRecordIDs = buildList(WTF::IteratorRange<Vector<unsigned>::iterator>(expiredRecords.begin(), expiredRecords.end()));

    SQLiteStatement clearExpiredInteraction(m_database, makeString("UPDATE ObservedDomains SET mostRecentUserInteractionTime = 0, hadUserInteraction = 1 WHERE domainID IN (", expiredRecordIDs, ")"));
    if (clearExpiredInteraction.prepare() != SQLITE_OK)
        return;

    SQLiteStatement removeStorageAccess(m_database, makeString("DELETE FROM StorageAccessUnderTopFrameDomains ", expiredRecordIDs, ")"));
    if (removeStorageAccess.prepare() != SQLITE_OK)
        return;

    if (clearExpiredInteraction.step() != SQLITE_DONE
        || removeStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::clearExpiredUserInteractions statement(s) failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::clearGrandfathering(Vector<unsigned>&& domainIDsToClear)
{
    ASSERT(!RunLoop::isMain());

    if (domainIDsToClear.isEmpty())
        return;

    auto listToClear = buildList(WTF::IteratorRange<Vector<unsigned>::iterator>(domainIDsToClear.begin(), domainIDsToClear.end()));

    SQLiteStatement clearGrandfatheringStatement(m_database, makeString("UPDATE ObservedDomains SET grandfathered = 0 WHERE domainID IN (", listToClear, ")"));
    if (clearGrandfatheringStatement.prepare() != SQLITE_OK)
        return;
    
    if (clearGrandfatheringStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearGrandfathering failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

bool ResourceLoadStatisticsDatabaseStore::hasHadUnexpiredRecentUserInteraction(const DomainData& resourceStatistic, OperatingDatesWindow operatingDatesWindow)
{
    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic.mostRecentUserInteractionTime, operatingDatesWindow)) {

        // Drop privacy sensitive data if we no longer need it.
        if (operatingDatesWindow == OperatingDatesWindow::Long)
            clearUserInteraction(resourceStatistic.registrableDomain, [] { });

        return false;
    }

    return resourceStatistic.hadUserInteraction;
}

bool ResourceLoadStatisticsDatabaseStore::shouldRemoveAllWebsiteDataFor(const DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
{
    return isPrevalentResource(resourceStatistic.registrableDomain) && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long) && (!shouldCheckForGrandfathering || !resourceStatistic.grandfathered);
}

bool ResourceLoadStatisticsDatabaseStore::shouldRemoveAllButCookiesFor(const DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
{
    bool isRemovalEnabled = firstPartyWebsiteDataRemovalMode() != FirstPartyWebsiteDataRemovalMode::None || resourceStatistic.isScheduledForAllButCookieDataRemoval;
    bool isResourceGrandfathered = shouldCheckForGrandfathering && resourceStatistic.grandfathered;

    OperatingDatesWindow window { };
    switch (firstPartyWebsiteDataRemovalMode()) {
    case FirstPartyWebsiteDataRemovalMode::AllButCookies:
        FALLTHROUGH;
    case FirstPartyWebsiteDataRemovalMode::None:
        window = OperatingDatesWindow::Short;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout:
        window = OperatingDatesWindow::ForLiveOnTesting;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout:
        window = OperatingDatesWindow::ForReproTesting;
    }

    return isRemovalEnabled && !isResourceGrandfathered && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, window);
}

Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>> ResourceLoadStatisticsDatabaseStore::registrableDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    clearExpiredUserInteractions();
    
    Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>> domainsToRemoveWebsiteDataFor;

    Vector<DomainData> domains = this->domains();
    Vector<unsigned> domainIDsToClearGrandfathering;
    for (auto& statistic : domains) {
        if (shouldRemoveAllWebsiteDataFor(statistic, shouldCheckForGrandfathering))
            domainsToRemoveWebsiteDataFor.append(std::make_pair(statistic.registrableDomain, WebsiteDataToRemove::All));
        else if (shouldRemoveAllButCookiesFor(statistic, shouldCheckForGrandfathering)) {
            domainsToRemoveWebsiteDataFor.append(std::make_pair(statistic.registrableDomain, WebsiteDataToRemove::AllButCookies));
            setIsScheduledForAllButCookieDataRemoval(statistic.registrableDomain, false);
        }
        if (shouldClearGrandfathering && statistic.grandfathered)
            domainIDsToClearGrandfathering.append(statistic.domainID);
    }

    clearGrandfathering(WTFMove(domainIDsToClearGrandfathering));
    
    return domainsToRemoveWebsiteDataFor;
}

void ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    unsigned count = 0;
    if (m_observedDomainCount.step() == SQLITE_ROW)
        count = m_observedDomainCount.getColumnInt(0);

    int resetResult = m_observedDomainCount.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    if (count <= parameters().maxStatisticsEntries)
        return;

    ASSERT(parameters().pruneEntriesDownTo <= parameters().maxStatisticsEntries);

    size_t countLeftToPrune = count - parameters().pruneEntriesDownTo;
    ASSERT(countLeftToPrune);

    SQLiteStatement recordsToPrune(m_database, "SELECT domainID FROM ObservedDomains ORDER BY hadUserInteraction, isPrevalent, lastSeen LIMIT ?");
    if (recordsToPrune.prepare() != SQLITE_OK
        || recordsToPrune.bindInt(1, countLeftToPrune) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<unsigned> entriesToPrune;
    while (recordsToPrune.step() == SQLITE_ROW)
        entriesToPrune.append(recordsToPrune.getColumnInt(0));

    auto listToPrune = buildList(WTF::IteratorRange<Vector<unsigned>::iterator>(entriesToPrune.begin(), entriesToPrune.end()));

    SQLiteStatement pruneCommand(m_database, makeString("DELETE from ObservedDomains WHERE domainID IN (", listToPrune, ")"));
    if (pruneCommand.prepare() != SQLITE_OK
        || pruneCommand.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::updateLastSeen(const RegistrableDomain& domain, WallTime lastSeen)
{
    ASSERT(!RunLoop::isMain());

    if (m_updateLastSeenStatement.bindDouble(1, lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || m_updateLastSeenStatement.bindText(2, domain.string()) != SQLITE_OK
        || m_updateLastSeenStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateLastSeen failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_updateLastSeenStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

void ResourceLoadStatisticsDatabaseStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(domain);
    updateLastSeen(domain, WallTime::fromRawSeconds(seconds.seconds()));
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    ensureResourceStatisticsForRegistrableDomain(domain);
    setPrevalentResource(domain, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;
    
    ensureResourceStatisticsForRegistrableDomain(domain);
    setPrevalentResource(domain, ResourceLoadPrevalence::VeryHigh);
}

void ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved(const RegistrableDomain& domain, int value)
{
    ASSERT(!RunLoop::isMain());

    if (m_updateDataRecordsRemovedStatement.bindInt(1, value) != SQLITE_OK
        || m_updateDataRecordsRemovedStatement.bindText(2, domain.string()) != SQLITE_OK
        || m_updateDataRecordsRemovedStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    int resetResult = m_updateDataRecordsRemovedStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

bool ResourceLoadStatisticsDatabaseStore::isCorrectSubStatisticsCount(const RegistrableDomain& subframeDomain, const TopFrameDomain& topFrameDomain)
{
    SQLiteStatement subFrameUnderTopFrameCount(m_database, countSubframeUnderTopFrameQuery);
    SQLiteStatement subresourceUnderTopFrameCount(m_database, countSubresourceUnderTopFrameQuery);
    SQLiteStatement subresourceUniqueRedirectsTo(m_database, countSubresourceUniqueRedirectsToQuery);
    
    if (subFrameUnderTopFrameCount.prepare() != SQLITE_OK
        || subresourceUnderTopFrameCount.prepare() != SQLITE_OK
        || subresourceUniqueRedirectsTo.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::countSubStatisticsTesting failed to prepare, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    
    if (subFrameUnderTopFrameCount.bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subFrameUnderTopFrameCount.bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount.bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount.bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo.bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo.bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::countSubStatisticsTesting failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    
    if (subFrameUnderTopFrameCount.step() != SQLITE_ROW
        || subresourceUnderTopFrameCount.step() != SQLITE_ROW
        || subresourceUniqueRedirectsTo.step() != SQLITE_ROW)
        return false;
    
    return (subFrameUnderTopFrameCount.getColumnInt(0) == 1 && subresourceUnderTopFrameCount.getColumnInt(0) == 1 && subresourceUniqueRedirectsTo.getColumnInt(0) == 1);
}

static void appendBoolean(StringBuilder& builder, const String& label, bool flag)
{
    builder.appendLiteral("    ");
    builder.append(label);
    builder.appendLiteral(": ");
    builder.append(flag ? "Yes" : "No");
}

static void appendNextEntry(StringBuilder& builder, String entry)
{
    builder.appendLiteral("        ");
    builder.append(entry);
    builder.append('\n');
}

String ResourceLoadStatisticsDatabaseStore::getDomainStringFromDomainID(unsigned domainID) const
{
    auto result = emptyString();
    
    if (m_domainStringFromDomainIDStatement.bindInt(1, domainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getDomainStringFromDomainID. Statement failed to prepare or bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return result;
    }
    
    if (m_domainStringFromDomainIDStatement.step() == SQLITE_ROW)
        result = m_domainStringFromDomainIDStatement.getColumnText(0);
    
    resetStatement(m_domainStringFromDomainIDStatement);
    return result;
}

String ResourceLoadStatisticsDatabaseStore::getSubStatisticStatement(const String& tableName) const
{
    if (tableName == "StorageAccessUnderTopFrameDomains")
        return "SELECT topLevelDomainID from StorageAccessUnderTopFrameDomains WHERE domainID = ?";
    if (tableName == "TopFrameUniqueRedirectsTo")
        return "SELECT toDomainID from TopFrameUniqueRedirectsTo WHERE sourceDomainID = ?";
    if (tableName == "TopFrameUniqueRedirectsFrom")
        return "SELECT fromDomainID from TopFrameUniqueRedirectsFrom WHERE targetDomainID = ?";
    if (tableName == "TopFrameLinkDecorationsFrom")
        return "SELECT fromDomainID from TopFrameLinkDecorationsFrom WHERE toDomainID = ?";
    if (tableName == "TopFrameLoadedThirdPartyScripts")
        return "SELECT subresourceDomainID from TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ?";
    if (tableName == "SubframeUnderTopFrameDomains")
        return "SELECT topFrameDomainID from SubframeUnderTopFrameDomains WHERE subFrameDomainID = ?";
    if (tableName == "SubresourceUnderTopFrameDomains")
        return "SELECT topFrameDomainID from SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ?";
    if (tableName == "SubresourceUniqueRedirectsTo")
        return "SELECT toDomainID from SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ?";
    if (tableName == "SubresourceUniqueRedirectsFrom")
        return "SELECT fromDomainID from SubresourceUniqueRedirectsFrom WHERE subresourceDomainID = ?";
    
    return emptyString();
}

void ResourceLoadStatisticsDatabaseStore::appendSubStatisticList(StringBuilder& builder, const String& tableName, const String& domain) const
{
    auto query = getSubStatisticStatement(tableName);
    
    if (query.isEmpty())
        return;
    
    SQLiteStatement data(m_database, query);
    
    if (data.prepare() != SQLITE_OK
        || data.bindInt(1, domainID(RegistrableDomain::uncheckedCreateFromHost(domain)).value()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::appendSubStatisticList. Statement failed to prepare or bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    if (data.step() != SQLITE_ROW)
        return;
    
    builder.appendLiteral("    ");
    builder.append(tableName);
    builder.appendLiteral(":\n");
    
    auto result = getDomainStringFromDomainID(data.getColumnInt(0));
    appendNextEntry(builder, result);
    
    while (data.step() == SQLITE_ROW) {
        result = getDomainStringFromDomainID(data.getColumnInt(0));
        appendNextEntry(builder, result);
    }
}

static bool hasHadRecentUserInteraction(WTF::Seconds interactionTimeSeconds)
{
    return interactionTimeSeconds > Seconds(0) && WallTime::now().secondsSinceEpoch() - interactionTimeSeconds < 24_h;
}

void ResourceLoadStatisticsDatabaseStore::resourceToString(StringBuilder& builder, const String& domain) const
{
    if (m_getResourceDataByDomainNameStatement.bindText(1, domain) != SQLITE_OK
        || m_getResourceDataByDomainNameStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::resourceToString. Statement failed to bind or domain was not found, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    builder.appendLiteral("Registrable domain: ");
    builder.append(domain);
    builder.append('\n');
    
    // User interaction
    appendBoolean(builder, "hadUserInteraction", m_getResourceDataByDomainNameStatement.getColumnInt(HadUserInteractionIndex));
    builder.append('\n');
    builder.appendLiteral("    mostRecentUserInteraction: ");
    if (hasHadRecentUserInteraction(Seconds(m_getResourceDataByDomainNameStatement.getColumnDouble(MostRecentUserInteractionTimeIndex))))
        builder.appendLiteral("within 24 hours");
    else
        builder.appendLiteral("-1");
    builder.append('\n');
    appendBoolean(builder, "grandfathered", m_getResourceDataByDomainNameStatement.getColumnInt(GrandfatheredIndex));
    builder.append('\n');

    // Storage access
    appendSubStatisticList(builder, "StorageAccessUnderTopFrameDomains", domain);

    // Top frame stats
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLinkDecorationsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLoadedThirdPartyScripts", domain);

    appendBoolean(builder, "IsScheduledForAllButCookieDataRemoval", m_getResourceDataByDomainNameStatement.getColumnInt(IsScheduledForAllButCookieDataRemovalIndex));
    builder.append('\n');

    // Subframe stats
    appendSubStatisticList(builder, "SubframeUnderTopFrameDomains", domain);

    // Subresource stats
    appendSubStatisticList(builder, "SubresourceUnderTopFrameDomains", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsFrom", domain);

    // Prevalent Resource
    appendBoolean(builder, "isPrevalentResource", m_getResourceDataByDomainNameStatement.getColumnInt(IsPrevalentIndex));
    builder.append('\n');
    appendBoolean(builder, "isVeryPrevalentResource", m_getResourceDataByDomainNameStatement.getColumnInt(IsVeryPrevalentIndex));
    builder.append('\n');
    builder.appendLiteral("    dataRecordsRemoved: ");
    builder.appendNumber(m_getResourceDataByDomainNameStatement.getColumnInt(DataRecordsRemovedIndex));
    builder.append('\n');

    resetStatement(m_getResourceDataByDomainNameStatement);
}

} // namespace WebKit

#endif
