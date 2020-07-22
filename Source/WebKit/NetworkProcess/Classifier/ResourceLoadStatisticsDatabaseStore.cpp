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
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
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

#define RELEASE_LOG_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_ERROR_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)

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
constexpr auto topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery = "INSERT OR IGNORE into TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement (sourceDomainID, toDomainID) SELECT ?, domainID FROM ObservedDomains where registrableDomain in ( "_s;
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

// EXISTS for testing queries
constexpr auto linkDecorationExistsQuery = "SELECT EXISTS (SELECT * FROM TopFrameLinkDecorationsFrom WHERE toDomainID = ? OR fromDomainID = ?)"_s;
constexpr auto scriptLoadExistsQuery = "SELECT EXISTS (SELECT * FROM TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ? OR subresourceDomainID = ?)"_s;
constexpr auto subFrameExistsQuery = "SELECT EXISTS (SELECT * FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? OR topFrameDomainID = ?)"_s;
constexpr auto subResourceExistsQuery = "SELECT EXISTS (SELECT * FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? OR topFrameDomainID = ?)"_s;
constexpr auto uniqueRedirectExistsQuery = "SELECT EXISTS (SELECT * FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? OR toDomainID = ?)"_s;
constexpr auto observedDomainsExistsQuery = "SELECT EXISTS (SELECT * FROM ObservedDomains WHERE domainID = ?)"_s;

// DELETE Queries
constexpr auto removeAllDataQuery = "DELETE FROM ObservedDomains WHERE domainID = ?"_s;

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
    "FOREIGN KEY(topLevelDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createTopFrameUniqueRedirectsTo = "CREATE TABLE TopFrameUniqueRedirectsTo ("
    "sourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(sourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement = "CREATE TABLE TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement ("
    "sourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(sourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameUniqueRedirectsFrom = "CREATE TABLE TopFrameUniqueRedirectsFrom ("
    "targetDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(targetDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameLinkDecorationsFrom = "CREATE TABLE TopFrameLinkDecorationsFrom ("
    "toDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createTopFrameLoadedThirdPartyScripts = "CREATE TABLE TopFrameLoadedThirdPartyScripts ("
    "topFrameDomainID INTEGER NOT NULL, subresourceDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createSubframeUnderTopFrameDomains = "CREATE TABLE SubframeUnderTopFrameDomains ("
    "subFrameDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUnderTopFrameDomains = "CREATE TABLE SubresourceUnderTopFrameDomains ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsTo = "CREATE TABLE SubresourceUniqueRedirectsTo ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsFrom = "CREATE TABLE SubresourceUniqueRedirectsFrom ("
    "subresourceDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;

constexpr auto createOperatingDates = "CREATE TABLE OperatingDates ("
    "year INTEGER NOT NULL, month INTEGER NOT NULL, monthDay INTEGER NOT NULL);"_s;


// CREATE UNIQUE INDEX Queries
constexpr auto createUniqueIndexStorageAccessUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS StorageAccessUnderTopFrameDomains_domainID_topLevelDomainID on StorageAccessUnderTopFrameDomains ( domainID, topLevelDomainID );"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsTo_sourceDomainID_toDomainID on TopFrameUniqueRedirectsTo ( sourceDomainID, toDomainID );"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement_sourceDomainID_toDomainID on TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement ( sourceDomainID, toDomainID );"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsFrom_targetDomainID_fromDomainID on TopFrameUniqueRedirectsFrom ( targetDomainID, fromDomainID );"_s;
constexpr auto createUniqueIndexTopFrameLinkDecorationsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLinkDecorationsFrom_toDomainID_fromDomainID on TopFrameLinkDecorationsFrom ( toDomainID, fromDomainID );"_s;
constexpr auto createUniqueIndexTopFrameLoadedThirdPartyScripts = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLoadedThirdPartyScripts_topFrameDomainID_subresourceDomainID on TopFrameLoadedThirdPartyScripts ( topFrameDomainID, subresourceDomainID );"_s;
constexpr auto createUniqueIndexSubframeUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubframeUnderTopFrameDomains_subFrameDomainID_topFrameDomainID on SubframeUnderTopFrameDomains ( subFrameDomainID, topFrameDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUnderTopFrameDomains_subresourceDomainID_topFrameDomainID on SubresourceUnderTopFrameDomains ( subresourceDomainID, topFrameDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsTo_subresourceDomainID_toDomainID on SubresourceUniqueRedirectsTo ( subresourceDomainID, toDomainID );"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsFrom_subresourceDomainID_fromDomainID on SubresourceUnderTopFrameDomains ( subresourceDomainID, fromDomainID );"_s;
constexpr auto createUniqueIndexOperatingDates = "CREATE UNIQUE INDEX IF NOT EXISTS OperatingDates_year_month_monthDay on OperatingDates ( year, month, monthDay );"_s;

const unsigned minimumPrevalentResourcesForTelemetry = 3;

static const String ObservedDomainsTableSchemaV1()
{
    return createObservedDomain;
}

static const String ObservedDomainsTableSchemaV1Alternate()
{
    return "CREATE TABLE \"ObservedDomains\" (domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL,isScheduledForAllButCookieDataRemoval INTEGER NOT NULL)";
}

static bool needsNewCreateTableSchema(const String& schema)
{
    return schema.contains("REFERENCES TopLevelDomains");
}

static const HashMap<String, String>& createTableQueries()
{
    static auto createTableQueries = makeNeverDestroyed(HashMap<String, String> {
        { "ObservedDomains"_s, createObservedDomain},
        { "TopLevelDomains"_s, createTopLevelDomains},
        { "StorageAccessUnderTopFrameDomains"_s, createStorageAccessUnderTopFrameDomains},
        { "TopFrameUniqueRedirectsTo"_s, createTopFrameUniqueRedirectsTo},
        { "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement"_s, createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement},
        { "TopFrameUniqueRedirectsFrom"_s, createTopFrameUniqueRedirectsFrom},
        { "TopFrameLinkDecorationsFrom"_s, createTopFrameLinkDecorationsFrom},
        { "TopFrameLoadedThirdPartyScripts"_s, createTopFrameLoadedThirdPartyScripts},
        { "SubframeUnderTopFrameDomains"_s, createSubframeUnderTopFrameDomains},
        { "SubresourceUnderTopFrameDomains"_s, createSubresourceUnderTopFrameDomains},
        { "SubresourceUniqueRedirectsTo"_s, createSubresourceUniqueRedirectsTo},
        { "SubresourceUniqueRedirectsFrom"_s, createSubresourceUniqueRedirectsFrom},
        { "OperatingDates"_s, createOperatingDates}
    });
    
    return createTableQueries;
}

HashSet<ResourceLoadStatisticsDatabaseStore*>& ResourceLoadStatisticsDatabaseStore::allStores()
{
    ASSERT(!RunLoop::isMain());

    static NeverDestroyed<HashSet<ResourceLoadStatisticsDatabaseStore*>> map;
    return map;
}

ResourceLoadStatisticsDatabaseStore::ResourceLoadStatisticsDatabaseStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost, const String& storageDirectoryPath, PAL::SessionID sessionID)
    : ResourceLoadStatisticsStore(store, workQueue, shouldIncludeLocalhost)
    , m_storageDirectoryPath(FileSystem::pathByAppendingComponent(storageDirectoryPath, "observations.db"_s))
    , m_sessionID(sessionID)
{
    ASSERT(!RunLoop::isMain());

    openAndUpdateSchemaIfNecessary();
    enableForeignKeys();

    // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
    m_database.disableThreadingChecks();
    
    if (!m_database.turnOnIncrementalAutoVacuum())
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::turnOnIncrementalAutoVacuum failed, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());

    workQueue.dispatchAfter(5_s, [weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->calculateAndSubmitTelemetry();
    });

    includeTodayAsOperatingDateIfNecessary();
    allStores().add(this);
}

ResourceLoadStatisticsDatabaseStore::~ResourceLoadStatisticsDatabaseStore()
{
    close();
    allStores().remove(this);
}

void ResourceLoadStatisticsDatabaseStore::close()
{
    destroyStatements();
    if (m_database.isOpen())
        m_database.close();
}

void ResourceLoadStatisticsDatabaseStore::openITPDatabase()
{
    if (!FileSystem::fileExists(m_storageDirectoryPath)) {
        if (!FileSystem::makeAllDirectories(FileSystem::directoryName(m_storageDirectoryPath))) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::open failed, error message: Failed to create directory database path: %" PUBLIC_LOG_STRING, this, m_storageDirectoryPath.utf8().data());
            return;
        }
        m_isNewResourceLoadStatisticsDatabaseFile = true;
    } else
        m_isNewResourceLoadStatisticsDatabaseFile = false;

    if (!m_database.open(m_storageDirectoryPath)) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::open failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
        ASSERT_NOT_REACHED();
    }
    
    SQLiteStatement setBusyTimeout(m_database, "PRAGMA busy_timeout = 5000");
    if (setBusyTimeout.prepare() != SQLITE_OK || setBusyTimeout.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::setBusyTimeout failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_isNewResourceLoadStatisticsDatabaseFile) {
        if (!createSchema()) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createSchema failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }
    }
}

Optional<Vector<String>> ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema()
{
    Vector<String> missingTables;
    SQLiteStatement statement(m_database, "SELECT 1 from sqlite_master WHERE type='table' and tbl_name=?");
    if (statement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema failed to prepare, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
        return WTF::nullopt;
    }

    for (auto& table : createTableQueries().keys()) {
        if (statement.bindText(1, table) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema failed to bind, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
            return WTF::nullopt;
        }
        if (statement.step() != SQLITE_ROW) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema schema is missing table: %s", this, table.ascii().data());
            missingTables.append(String(table));
        }
        statement.reset();
    }
    if (missingTables.isEmpty())
        return WTF::nullopt;

    return missingTables;
}

void ResourceLoadStatisticsDatabaseStore::enableForeignKeys()
{
    SQLiteStatement enableForeignKeys(m_database, "PRAGMA foreign_keys = ON");
    if (enableForeignKeys.prepare() != SQLITE_OK || enableForeignKeys.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::enableForeignKeys failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

bool ResourceLoadStatisticsDatabaseStore::isMigrationNecessary()
{
    // Check a table for a reference to TopLevelDomains, a sign of the old schema.
    SQLiteStatement statement(m_database, "SELECT type, sql FROM sqlite_master WHERE type = 'table' AND tbl_name='TopFrameUniqueRedirectsTo'");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to prepare statement to fetch schema.");
        ASSERT_NOT_REACHED();
        return false;
    }

    // If there is no table at all, or there is an error executing the fetch, delete and reopen the file.
    if (statement.step() != SQLITE_ROW) {
        LOG_ERROR("Error executing statement to fetch schema.");
        close();
        FileSystem::deleteFile(m_storageDirectoryPath);
        openITPDatabase();
        return false;
    }
    
    auto oldSchema = String(statement.getColumnText(1));
    return needsNewCreateTableSchema(oldSchema);
}

void ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary()
{
    if (!isMigrationNecessary())
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (auto& table : createTableQueries().keys()) {
        auto query = makeString("ALTER TABLE ", table, " RENAME TO _", table);
        SQLiteStatement alterTable(m_database, query);
        if (alterTable.prepare() != SQLITE_OK || alterTable.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesWithCascadingDeletion failed to rename table, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    if (!createSchema()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesWithCascadingDeletion failed to create schema, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    for (auto& table : createTableQueries().keys()) {
        auto query = makeString("INSERT INTO ", table, " SELECT * FROM _", table);
        SQLiteStatement migrateTableData(m_database, query);
        if (migrateTableData.prepare() != SQLITE_OK || migrateTableData.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesWithCascadingDeletion failed to migrate schema, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
        
        auto dropQuery = makeString("DROP TABLE _", table);
        SQLiteStatement dropTableQuery(m_database, dropQuery);
        if (dropTableQuery.prepare() != SQLITE_OK || dropTableQuery.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesWithCascadingDeletion failed to drop temporary tables, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    transaction.commit();

    if (!createUniqueIndices()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesWithCascadingDeletion failed to create unique indices, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::addMissingTablesIfNecessary()
{
    auto missingTables = checkForMissingTablesInSchema();
    if (!missingTables)
        return;

    for (auto& table : *missingTables) {
        auto createTableQuery = createTableQueries().get(table);
        if (!m_database.executeCommand(createTableQuery))
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingTables failed to execute, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
    }

    if (!createUniqueIndices()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingTables failed to create unique indices, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::openAndUpdateSchemaIfNecessary()
{
    openITPDatabase();
    addMissingTablesIfNecessary();

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
            close();
            FileSystem::deleteFile(m_storageDirectoryPath);
            openITPDatabase();
            return;
        }

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the ObservedDomains schema in the ResourceLoadStatistics directory is not the current schema, delete the database file.
    // FIXME: Migrate old ObservedDomains data to new table schema.
    if (currentSchema != ObservedDomainsTableSchemaV1() && currentSchema != ObservedDomainsTableSchemaV1Alternate()) {
        close();
        FileSystem::deleteFile(m_storageDirectoryPath);
        openITPDatabase();
    } else
        migrateDataToNewTablesIfNecessary();
}

SQLiteStatementAutoResetScope ResourceLoadStatisticsDatabaseStore::scopedStatement(std::unique_ptr<WebCore::SQLiteStatement>& statement, const String& query, const String& logString) const
{
    if (!statement) {
        statement = makeUnique<WebCore::SQLiteStatement>(m_database, query);
        ASSERT(m_database.isOpen());
        if (statement->prepare() != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::%s failed to prepare statement, error message: %" PUBLIC_LOG_STRING, this, logString.ascii().data(), m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
    }
    return SQLiteStatementAutoResetScope { statement.get() };
}

void ResourceLoadStatisticsDatabaseStore::interrupt()
{
    if (m_database.isOpen())
        m_database.interrupt();
}

bool ResourceLoadStatisticsDatabaseStore::isEmpty() const
{
    ASSERT(!RunLoop::isMain());
    
    auto scopedStatement = this->scopedStatement(m_observedDomainCountStatement, observedDomainCountQuery, "isEmpty"_s);
    if (!scopedStatement
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::isEmpty failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return !scopedStatement->getColumnInt(0);
}

bool ResourceLoadStatisticsDatabaseStore::createUniqueIndices()
{
    if (!m_database.executeCommand(createUniqueIndexStorageAccessUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexTopFrameUniqueRedirectsTo)
        || !m_database.executeCommand(createUniqueIndexTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement)
        || !m_database.executeCommand(createUniqueIndexTopFrameUniqueRedirectsFrom)
        || !m_database.executeCommand(createUniqueIndexTopFrameLinkDecorationsFrom)
        || !m_database.executeCommand(createUniqueIndexTopFrameLoadedThirdPartyScripts)
        || !m_database.executeCommand(createUniqueIndexSubframeUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexSubresourceUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexSubresourceUniqueRedirectsTo)
        || !m_database.executeCommand(createUniqueIndexSubresourceUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexOperatingDates)) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createUniqueIndices failed to execute, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
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

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement)) {
        LOG_ERROR("Could not create TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
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
    
    if (!m_database.executeCommand(createOperatingDates)) {
        LOG_ERROR("Could not create OperatingDates table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }
    
    if (!createUniqueIndices())
        return false;

    return true;
}

void ResourceLoadStatisticsDatabaseStore::destroyStatements()
{
    ASSERT(!RunLoop::isMain());

    m_observedDomainCountStatement = nullptr;
    m_insertObservedDomainStatement = nullptr;
    m_insertTopLevelDomainStatement = nullptr;
    m_domainIDFromStringStatement = nullptr;
    m_subframeUnderTopFrameDomainExistsStatement = nullptr;
    m_subresourceUnderTopFrameDomainExistsStatement = nullptr;
    m_subresourceUniqueRedirectsToExistsStatement = nullptr;
    m_updateLastSeenStatement = nullptr;
    m_updateDataRecordsRemovedStatement = nullptr;
    m_mostRecentUserInteractionStatement = nullptr;
    m_updatePrevalentResourceStatement = nullptr;
    m_isPrevalentResourceStatement = nullptr;
    m_updateVeryPrevalentResourceStatement = nullptr;
    m_isVeryPrevalentResourceStatement = nullptr;
    m_clearPrevalentResourceStatement = nullptr;
    m_hadUserInteractionStatement = nullptr;
    m_updateGrandfatheredStatement = nullptr;
    m_updateIsScheduledForAllButCookieDataRemovalStatement = nullptr;
    m_isGrandfatheredStatement = nullptr;
    m_findExpiredUserInteractionStatement = nullptr;
    m_topFrameLinkDecorationsFromExistsStatement = nullptr;
    m_topFrameLoadedThirdPartyScriptsExistsStatement = nullptr;
    m_countPrevalentResourcesStatement = nullptr;
    m_countPrevalentResourcesWithUserInteractionStatement = nullptr;
    m_countPrevalentResourcesWithoutUserInteractionStatement = nullptr;
    m_getResourceDataByDomainNameStatement = nullptr;
    m_getAllDomainsStatement = nullptr;
    m_domainStringFromDomainIDStatement = nullptr;
    m_getAllSubStatisticsStatement = nullptr;
    m_storageAccessExistsStatement = nullptr;
    m_getMostRecentlyUpdatedTimestampStatement = nullptr;
    m_linkDecorationExistsStatement = nullptr;
    m_scriptLoadExistsStatement = nullptr;
    m_subFrameExistsStatement = nullptr;
    m_subResourceExistsStatement = nullptr;
    m_uniqueRedirectExistsStatement = nullptr;
    m_observedDomainsExistsStatement = nullptr;
    m_removeAllDataStatement = nullptr;
}

bool ResourceLoadStatisticsDatabaseStore::insertObservedDomain(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    if (domainID(loadStatistics.registrableDomain)) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "ResourceLoadStatisticsDatabaseStore::insertObservedDomain can only be called on domains not in the database.");
        ASSERT_NOT_REACHED();
        return false;
    }
    auto scopedStatement = this->scopedStatement(m_insertObservedDomainStatement, insertObservedDomainQuery, "insertObservedDomain"_s);
    if (!scopedStatement
        || scopedStatement->bindText(RegistrableDomainIndex, loadStatistics.registrableDomain.string()) != SQLITE_OK
        || scopedStatement->bindDouble(LastSeenIndex, loadStatistics.lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindInt(HadUserInteractionIndex, loadStatistics.hadUserInteraction) != SQLITE_OK
        || scopedStatement->bindDouble(MostRecentUserInteractionTimeIndex, loadStatistics.mostRecentUserInteractionTime.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindInt(GrandfatheredIndex, loadStatistics.grandfathered) != SQLITE_OK
        || scopedStatement->bindInt(IsPrevalentIndex, loadStatistics.isPrevalentResource) != SQLITE_OK
        || scopedStatement->bindInt(IsVeryPrevalentIndex, loadStatistics.isVeryPrevalentResource) != SQLITE_OK
        || scopedStatement->bindInt(DataRecordsRemovedIndex, loadStatistics.dataRecordsRemoved) != SQLITE_OK
        || scopedStatement->bindInt(TimesAccessedAsFirstPartyDueToUserInteractionIndex, loadStatistics.timesAccessedAsFirstPartyDueToUserInteraction) != SQLITE_OK
        || scopedStatement->bindInt(TimesAccessedAsFirstPartyDueToStorageAccessAPIIndex, loadStatistics.timesAccessedAsFirstPartyDueToStorageAccessAPI) != SQLITE_OK
        || scopedStatement->bindInt(IsScheduledForAllButCookieDataRemovalIndex, loadStatistics.gotLinkDecorationFromPrevalentResource) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to commit, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool ResourceLoadStatisticsDatabaseStore::relationshipExists(SQLiteStatementAutoResetScope& statement, Optional<unsigned> firstDomainID, const RegistrableDomain& secondDomain) const
{
    if (!firstDomainID)
        return false;
    
    ASSERT(!RunLoop::isMain());

    if (!statement
        || statement->bindInt(1, *firstDomainID) != SQLITE_OK
        || statement->bindText(2, secondDomain.string()) != SQLITE_OK
        || statement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::relationshipExists failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return !!statement->getColumnInt(0);
}

Optional<unsigned> ResourceLoadStatisticsDatabaseStore::domainID(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "domainID"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDFromString failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return WTF::nullopt;
    }
    
    if (scopedStatement->step() != SQLITE_ROW)
        return WTF::nullopt;

    return scopedStatement->getColumnInt(0);
}

String ResourceLoadStatisticsDatabaseStore::ensureAndMakeDomainList(const HashSet<RegistrableDomain>& domainList)
{
    StringBuilder builder;
    
    for (auto& topFrameResource : domainList) {
        
        // Insert query will fail if top frame domain is not already in the database
        auto result = ensureResourceStatisticsForRegistrableDomain(topFrameResource);
        if (result.second) {
            if (!builder.isEmpty())
                builder.appendLiteral(", ");
            builder.append('"');
            builder.append(topFrameResource.string());
            builder.append('"');
        }
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
    insertDomainRelationshipList(topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, loadStatistics.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement, registrableDomainID.value());
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
    for (const auto& statistic : statisticsMap) {
        auto result = insertObservedDomain(statistic.value);
        if (!result) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::populateFromMemoryStore insertObservedDomain failed to complete, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries
    for (auto& statistic : statisticsMap)
        insertDomainRelationships(statistic.value);
}

void ResourceLoadStatisticsDatabaseStore::merge(WebCore::SQLiteStatement* current, const ResourceLoadStatistics& other)
{
    ASSERT(!RunLoop::isMain());

    auto currentRegistrableDomain = current->getColumnText(RegistrableDomainIndex);
    auto currentLastSeen = current->getColumnDouble(LastSeenIndex);
    auto currentMostRecentUserInteraction = current->getColumnDouble(MostRecentUserInteractionTimeIndex);
    bool currentGrandfathered = current->getColumnInt(GrandfatheredIndex);
    bool currentIsPrevalent = current->getColumnInt(IsPrevalentIndex);
    bool currentIsVeryPrevalent = current->getColumnInt(IsVeryPrevalentIndex);
    unsigned currentDataRecordsRemoved = current->getColumnInt(DataRecordsRemovedIndex);
    bool currentIsScheduledForAllButCookieDataRemoval = current->getColumnInt(IsScheduledForAllButCookieDataRemovalIndex);

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

    auto scopedStatement = this->scopedStatement(m_getResourceDataByDomainNameStatement, getResourceDataByDomainNameQuery, "mergeStatistic"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, statistic.registrableDomain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::mergeStatistic. Statement failed to bind or domain was not found, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    merge(scopedStatement.get(), statistic);
}

void ResourceLoadStatisticsDatabaseStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());

    for (auto& statistic : statistics) {
        if (!domainID(statistic.registrableDomain)) {
            auto result = insertObservedDomain(statistic);
            if (!result) {
                RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::mergeStatistics insertObservedDomain failed to complete, error message: %{private}s", this, m_database.lastErrorMsg());
                ASSERT_NOT_REACHED();
                return;
            }
        } else
            mergeStatistic(statistic);
    }

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries.
    for (auto& statistic : statistics)
        insertDomainRelationships(statistic);
}

static const StringView joinSubStatisticsForSorting()
{
    return "domainID,"
        "(countSubFrameUnderTopFrame + countSubResourceUnderTopFrame + countUniqueRedirectTo) as sum  "
        "FROM ( "
        "SELECT "
            "domainID, "
            "COUNT(DISTINCT f.topFrameDomainID) as countSubFrameUnderTopFrame, "
            "COUNT(DISTINCT r.topFrameDomainID) as countSubResourceUnderTopFrame, "
            "COUNT(DISTINCT toDomainID) as countUniqueRedirectTo "
        "FROM "
        "ObservedDomains o "
        "LEFT JOIN SubframeUnderTopFrameDomains f ON o.domainID = f.subFrameDomainID "
        "LEFT JOIN SubresourceUnderTopFrameDomains r ON o.domainID = r.subresourceDomainID "
        "LEFT JOIN SubresourceUniqueRedirectsTo u ON o.domainID = u.subresourceDomainID "
        "WHERE isPrevalent LIKE ? "
        "and hadUserInteraction LIKE ? "
        "GROUP BY domainID) ORDER BY sum DESC ";
}

static SQLiteStatement makeMedianWithUIQuery(SQLiteDatabase& database)
{
    return SQLiteStatement(database, makeString("SELECT mostRecentUserInteractionTime FROM ObservedDomains INNER JOIN (SELECT ", joinSubStatisticsForSorting(), ") as q ON ObservedDomains.domainID = q.domainID LIMIT 1 OFFSET ?"));
}

Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> ResourceLoadStatisticsDatabaseStore::getThirdPartyDataForSpecificFirstPartyDomains(unsigned thirdPartyDomainID, const RegistrableDomain& thirdPartyDomain) const
{
    auto scopedStatement = this->scopedStatement(m_getAllSubStatisticsStatement, getAllSubStatisticsUnderDomainQuery, "getThirdPartyDataForSpecificFirstPartyDomains"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, thirdPartyDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, thirdPartyDomainID) != SQLITE_OK
        || scopedStatement->bindInt(3, thirdPartyDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getThirdPartyDataForSpecificFirstPartyDomain, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
    Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> thirdPartyDataForSpecificFirstPartyDomains;
    while (scopedStatement->step() == SQLITE_ROW) {
        RegistrableDomain firstPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(m_getAllSubStatisticsStatement->getColumnInt(0)));
        thirdPartyDataForSpecificFirstPartyDomains.appendIfNotContains(WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty { firstPartyDomain, hasStorageAccess(firstPartyDomain, thirdPartyDomain), getMostRecentlyUpdatedTimestamp(thirdPartyDomain, firstPartyDomain) });
    }
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
    const auto prevalentDomainsBindParameter = thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::All ? "%" : "1";
    SQLiteStatement sortedStatistics(m_database, makeString("SELECT ", joinSubStatisticsForSorting()));
    if (sortedStatistics.prepare() != SQLITE_OK
        || sortedStatistics.bindText(1, prevalentDomainsBindParameter)
        || sortedStatistics.bindText(2, "%") != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::aggregatedThirdPartyData, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
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
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Bind
    if (medianDaysSinceUIStatement.bindInt(1, 1) != SQLITE_OK || medianDaysSinceUIStatement.bindInt(2, 1) != SQLITE_OK || medianDaysSinceUIStatement.bindInt(3, (prevalentResourcesWithUserInteractionCount / 2) != SQLITE_OK)) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Step
    if (medianDaysSinceUIStatement.step() != SQLITE_ROW)
        return 0;

    double rawSeconds = medianDaysSinceUIStatement.getColumnDouble(0);
    WallTime wallTime = WallTime::fromRawSeconds(rawSeconds);
    unsigned median = wallTime <= WallTime() ? 0 : std::floor((WallTime::now() - wallTime) / 24_h);

    if (prevalentResourcesWithUserInteractionCount & 1)
        return median;

    SQLiteStatement lowerMedianDaysSinceUIStatement = makeMedianWithUIQuery(database);

    // Prepare
    if (lowerMedianDaysSinceUIStatement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Bind
    if (lowerMedianDaysSinceUIStatement.bindInt(1, 1) != SQLITE_OK || lowerMedianDaysSinceUIStatement.bindInt(2, 1) != SQLITE_OK || lowerMedianDaysSinceUIStatement.bindInt(3, ((prevalentResourcesWithUserInteractionCount - 1) / 2)) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getMedianOfPrevalentResourcesWithUserInteraction, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }

    // Step
    if (lowerMedianDaysSinceUIStatement.step() == SQLITE_ROW) {
        double rawSecondsLower = lowerMedianDaysSinceUIStatement.getColumnDouble(0);
        WallTime wallTimeLower = WallTime::fromRawSeconds(rawSecondsLower);
        return ((wallTimeLower <= WallTime() ? 0 : std::floor((WallTime::now() - wallTimeLower) / 24_h)) + median) / 2;
    }
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResources() const
{
    auto scopedStatement = this->scopedStatement(m_countPrevalentResourcesStatement, countPrevalentResourcesQuery, "getNumberOfPrevalentResources"_s);
    if (!scopedStatement)
        return 0;
    
    auto stepValue = scopedStatement->step();
    if (stepValue != SQLITE_ROW && stepValue != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResources failed to step, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (stepValue == SQLITE_ROW) {
        unsigned prevalentResourceCount = scopedStatement->getColumnInt(0);
        if (prevalentResourceCount >= minimumPrevalentResourcesForTelemetry)
            return prevalentResourceCount;
    }
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResourcesWithUI() const
{
    auto scopedStatement = this->scopedStatement(m_countPrevalentResourcesWithUserInteractionStatement, countPrevalentResourcesWithUserInteractionQuery, "getNumberOfPrevalentResourcesWithUI"_s);
    if (scopedStatement && scopedStatement->step() == SQLITE_ROW) {
        int count = scopedStatement->getColumnInt(0);
        return count;
    }
    return 0;
}

unsigned ResourceLoadStatisticsDatabaseStore::getTopPrevelentResourceDaysSinceUI() const
{
    SQLiteStatement topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement(m_database, makeString("SELECT mostRecentUserInteractionTime FROM ObservedDomains INNER JOIN (SELECT ", joinSubStatisticsForSorting(), " LIMIT 1) as q ON ObservedDomains.domainID = q.domainID;"));
    
    // Prepare
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement query failed to prepare, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    // Bind
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.bindInt(1, 1) != SQLITE_OK
        || topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.bindInt(2, 1) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement query failed to bind, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    // Step
    if (topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.step() == SQLITE_ROW) {
        double rawSeconds = topPrevalentResourceWithUserInteractionDaysSinceUserInteractionStatement.getColumnDouble(0);
        WallTime wallTime = WallTime::fromRawSeconds(rawSeconds);
        return wallTime <= WallTime() ? 0 : std::floor((WallTime::now() - wallTime) / 24_h);
    }
    return 0;
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
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::makeMedianWithoutUIQuery, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
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
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::makeMedianWithoutUIQuery, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
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
            RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::getNumberOfPrevalentResourcesInTopResources, error message: %" PUBLIC_LOG_STRING, database.lastErrorMsg());
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
    auto scopedStatement = this->scopedStatement(m_countPrevalentResourcesWithoutUserInteractionStatement, countPrevalentResourcesWithoutUserInteractionQuery, "getNumberOfPrevalentResourcesWithoutUI"_s);
    if (scopedStatement && scopedStatement->step() == SQLITE_ROW) {
        int count = m_countPrevalentResourcesWithoutUserInteractionStatement->getColumnInt(0);
        return count;
    }
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

void ResourceLoadStatisticsDatabaseStore::calculateAndSubmitTelemetry(NotifyPagesForTesting shouldNotifyPagesForTesting) const
{
    ASSERT(!RunLoop::isMain());

    if (parameters().shouldSubmitTelemetry) {
        PrevalentResourceDatabaseTelemetry prevalentResourceDatabaseTelemetry;
        calculateTelemetryData(prevalentResourceDatabaseTelemetry);
        WebResourceLoadStatisticsTelemetry::submitTelemetry(*this, prevalentResourceDatabaseTelemetry, shouldNotifyPagesForTesting);
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

void ResourceLoadStatisticsDatabaseStore::runIncrementalVacuumCommand()
{
    ASSERT(!RunLoop::isMain());
    m_database.runIncrementalVacuumCommand();
}

bool ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const TopFrameDomain& topFrameDomain, const SubFrameDomain& subFrameDomain) const
{
    auto scopedStatement = this->scopedStatement(m_storageAccessExistsStatement, storageAccessExistsQuery, "hasStorageAccess"_s);
    return relationshipExists(scopedStatement, domainID(subFrameDomain), topFrameDomain);
}

void ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::hasStorageAccess was not completed due to failed insert attempt", this);
        return;
    }

    switch (cookieAccess(subFrameDomain, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        completionHandler(false);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->hasCookies(subFrameDomain, [store, completionHandler = WTFMove(completionHandler)](bool result) mutable {
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
        store->callHasStorageAccessForFrameHandler(subFrameDomain, topFrameDomain, frameID.value(), pageID, [store, completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessScope scope, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (!subFrameStatus.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess was not completed due to failed insert attempt", this);
        return;
    }
    
    switch (cookieAccess(subFrameDomain, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Cannot grant storage access to %{private}s since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Error, makeString("[ITP] Cannot grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are blocked in third-party contexts and it has not received user interaction as first-party."_s));
        }
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "No need to grant storage access to %{private}s since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] No need to grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies."_s));
        }
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    case CookieAccess::OnlyIfGranted:
        // Handled below.
        break;
    }

    auto userWasPromptedEarlier = hasUserGrantedStorageAccessThroughPrompt(*subFrameStatus.second, topFrameDomain);
    if (userWasPromptedEarlier == StorageAccessPromptWasShown::No) {
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "About to ask the user whether they want to grant storage access to %{private}s under %{private}s or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to ask the user whether they want to grant storage access to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "' or not."_s));
        }
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes) {
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Storage access was granted to %{private}s under %{private}s.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "'."_s));
        }
    }

    SQLiteStatement incrementStorageAccess(m_database, "UPDATE ObservedDomains SET timesAccessedAsFirstPartyDueToStorageAccessAPI = timesAccessedAsFirstPartyDueToStorageAccessAPI + 1 WHERE domainID = ?");
    if (incrementStorageAccess.prepare() != SQLITE_OK
        || incrementStorageAccess.bindInt(1, *subFrameStatus.second) != SQLITE_OK
        || incrementStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, scope, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    if (UNLIKELY(debugLoggingEnabled())) {
        RELEASE_LOG_INFO(ITPDebug, "[Temporary combatibility fix] Storage access was granted for %{private}s under opener page from %{private}s, with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted for '"_s, domainInNeedOfStorageAccess.string(), "' under opener page from '"_s, openerDomain.string(), "', with user interaction in the opened window."_s));
    }

    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID, StorageAccessPromptWasShown::No, StorageAccessScope::PerPage, [](StorageAccessWasGranted) { });
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (promptWasShown == StorageAccessPromptWasShown::Yes) {
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        if (!subFrameStatus.second) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grantStorageAccess was not completed due to failed insert attempt", this);
            return;
        }
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
        insertDomainRelationshipList(storageAccessUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *subFrameStatus.second);
    }

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, WTFMove(completionHandler));
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessWasGranted::Yes);
        return;
    }

    if (promptWasShownNowOrEarlier == StorageAccessPromptWasShown::Yes) {
#ifndef NDEBUG
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        if (!subFrameStatus.second) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal was not completed due to failed insert attempt", this);
            return;
        }
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
        ASSERT(hasUserGrantedStorageAccessThroughPrompt(*subFrameStatus.second, topFrameDomain) == StorageAccessPromptWasShown::Yes);
#endif
        setUserInteraction(subFrameDomain, true, WallTime::now());
    }

    RunLoop::main().dispatch([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, store = makeRef(store()), scope, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, scope, [completionHandler = WTFMove(completionHandler), store](StorageAccessWasGranted wasGranted) mutable {
            store->statisticsQueue().dispatch([wasGranted, completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(wasGranted);
            });
        });
    });

}

void ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(!RunLoop::isMain());

    for (auto& registrableDomain : domains) {
        auto result = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
        if (!result.second)
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains was not completed due to failed insert attempt", this);
    }

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

    auto result = ensureResourceStatisticsForRegistrableDomain(debugStaticPrevalentResource());
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode was not completed due to failed insert attempt for debugStaticPrevalentResource", this);
        return { };
    }

    setPrevalentResource(debugStaticPrevalentResource(), ResourceLoadPrevalence::High);
    primaryDomainsToBlock.uncheckedAppend(debugStaticPrevalentResource());

    if (!debugManualPrevalentResource().isEmpty()) {
        auto result = ensureResourceStatisticsForRegistrableDomain(debugManualPrevalentResource());
        if (!result.second) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode was not completed due to failed insert attempt for debugManualPrevalentResource", this);
            return { };
        }
        setPrevalentResource(debugManualPrevalentResource(), ResourceLoadPrevalence::High);
        primaryDomainsToBlock.uncheckedAppend(debugManualPrevalentResource());

        if (debugLoggingEnabled()) {
            RELEASE_LOG_INFO(ITPDebug, "Did set %{private}s as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Did set '"_s, debugManualPrevalentResource().string(), "' as prevalent resource for the purposes of ITP Debug Mode."_s));
        }
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
        if (!targetResult.second) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target domain", this);
            return;
        }
        updateLastSeen(targetDomain, ResourceLoadStatistics::reduceTimeResolution(WallTime::now()));
        insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *targetResult.second);
        statisticsWereUpdated = true;
    }

    if (!areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            bool wasNavigatedAfterShortDelayWithoutUserInteraction = !wasPotentiallyInitiatedByUser && delayAfterMainFrameDocumentLoad < parameters().minDelayAfterMainFrameDocumentLoadToNotBeARedirect;
            if (isRedirect || wasNavigatedAfterShortDelayWithoutUserInteraction) {
                auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
                auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
                if (!targetResult.second || !redirectingDomainResult.second) {
                    RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target or redirecting domain (isMainFrame)", this);
                    return;
                }
                insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
                if (isRedirect) {
                    insertDomainRelationshipList(topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);

                    if (UNLIKELY(debugLoggingEnabled())) {
                        RELEASE_LOG_INFO(ITPDebug, "Did set %" PUBLIC_LOG_STRING " as making a top frame redirect to %" PUBLIC_LOG_STRING ".", sourceDomain.string().utf8().data(), targetDomain.string().utf8().data());
                        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Did set '", sourceDomain.string(), "' as making a top frame redirect to '", targetDomain.string(), "'."));
                    }
                }
                insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
                statisticsWereUpdated = true;
            }
        } else if (isRedirect) {
            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (!targetResult.second || !redirectingDomainResult.second) {
                RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target or redirecting domain (isRedirect)", this);
                return;
            }
            insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
            insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
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
    if (!toDomainResult.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logCrossSiteLoadWithLinkDecoration was not completed due to failed insert attempt", this);
        return;
    }
    insertDomainRelationshipList(topFrameLinkDecorationsFromQuery, HashSet<RegistrableDomain>({ fromDomain }), *toDomainResult.second);
    
    if (isPrevalentResource(fromDomain))
        setIsScheduledForAllButCookieDataRemoval(toDomain, true);
}

void ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!targetResult.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement was not completed due to failed insert attempt", this);
        completionHandler();
        return;
    }

    SQLiteStatement removeRedirectsToSinceSameSite(m_database, "DELETE FROM TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement WHERE sourceDomainID = ?");
    if (removeRedirectsToSinceSameSite.prepare() != SQLITE_OK
        || removeRedirectsToSinceSameSite.bindInt(1, *targetResult.second) != SQLITE_OK
        || removeRedirectsToSinceSameSite.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
    
    completionHandler();
}

void ResourceLoadStatisticsDatabaseStore::setUserInteraction(const RegistrableDomain& domain, bool hadUserInteraction, WallTime mostRecentInteraction)
{
    ASSERT(!RunLoop::isMain());
    
    auto scopedStatement = this->scopedStatement(m_mostRecentUserInteractionStatement, mostRecentUserInteractionQuery, "setUserInteraction"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, hadUserInteraction) != SQLITE_OK
        || scopedStatement->bindDouble(2, mostRecentInteraction.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindText(3, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setUserInteraction, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::logUserInteraction(const TopFrameDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logUserInteraction was not completed due to failed insert attempt", this);
        return;
    }
    bool didHavePreviousUserInteraction = hasHadUserInteraction(domain, OperatingDatesWindow::Long);
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
    if (!targetResult.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearUserInteraction was not completed due to failed insert attempt", this);
        return;
    }
    setUserInteraction(domain, false, { });

    SQLiteStatement removeStorageAccess(m_database, "DELETE FROM StorageAccessUnderTopFrameDomains WHERE domainID = ?");
    if (removeStorageAccess.prepare() != SQLITE_OK
        || removeStorageAccess.bindInt(1, *targetResult.second) != SQLITE_OK
        || removeStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearUserInteraction failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
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
    
    auto scopedStatement = this->scopedStatement(m_hadUserInteractionStatement, hadUserInteractionQuery, "hasHadUserInteraction"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_hadUserInteractionStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());
        return false;
    }

    bool hadUserInteraction = !!scopedStatement->getColumnInt(0);
    if (!hadUserInteraction)
        return false;
    
    WallTime mostRecentUserInteractionTime = WallTime::fromRawSeconds(scopedStatement->getColumnDouble(1));

    if (hasStatisticsExpired(mostRecentUserInteractionTime, operatingDatesWindow)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        clearUserInteraction(domain, [] { });
        hadUserInteraction = false;
    }
    return hadUserInteraction;
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const RegistrableDomain& domain, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());
    if (shouldSkip(domain))
        return;

    auto scopedUpdatePrevalentResourceStatement = this->scopedStatement(m_updatePrevalentResourceStatement, updatePrevalentResourceQuery, "setPrevalentResource"_s);
    if (!scopedUpdatePrevalentResourceStatement
        || scopedUpdatePrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updatePrevalentResourceStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    auto scopedUpdateVeryPrevalentResourceStatement = this->scopedStatement(m_updateVeryPrevalentResourceStatement, updateVeryPrevalentResourceQuery, "setPrevalentResource updateVeryPrevalentResource"_s);
    if (newPrevalence == ResourceLoadPrevalence::VeryHigh) {
        if (!scopedUpdateVeryPrevalentResourceStatement
            || scopedUpdateVeryPrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updateVeryPrevalentResourceStatement failed, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
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

    auto scopedStatement = this->scopedStatement(m_getAllDomainsStatement, getAllDomainsQuery, "dumpResourceLoadStatistics"_s);
    if (!scopedStatement)
        return;
    
    Vector<String> domains;
    while (scopedStatement->step() == SQLITE_ROW)
        domains.append(scopedStatement->getColumnText(0));
    std::sort(domains.begin(), domains.end(), WTF::codePointCompareLessThan);

    StringBuilder result;
    result.appendLiteral("Resource load statistics:\n\n");
    for (auto& domain : domains)
        resourceToString(result, domain);

    auto thirdPartyData = aggregatedThirdPartyData();
    if (!thirdPartyData.isEmpty()) {
        result.append("\nITP Data:\n");
        for (auto thirdParty : thirdPartyData) {
            result.append(thirdParty.toString());
            result.append('\n');
        }
    }
    completionHandler(result.toString());
}

bool ResourceLoadStatisticsDatabaseStore::predicateValueForDomain(SQLiteStatementAutoResetScope& predicateStatement, const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (!predicateStatement
        || predicateStatement->bindText(1, domain.string()) != SQLITE_OK
        || predicateStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::predicateValueForDomain failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        return false;
    }
    return !!predicateStatement->getColumnInt(0);
}

bool ResourceLoadStatisticsDatabaseStore::isPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    auto scopedStatement = this->scopedStatement(m_isPrevalentResourceStatement, isPrevalentResourceQuery, "isPrevalentResource"_s);
    return predicateValueForDomain(scopedStatement, domain);
}

bool ResourceLoadStatisticsDatabaseStore::isVeryPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    auto scopedStatement = this->scopedStatement(m_isVeryPrevalentResourceStatement, isVeryPrevalentResourceQuery, "isVeryPrevalentResource"_s);
    return predicateValueForDomain(scopedStatement, domain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsSubresourceUnder(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_subresourceUnderTopFrameDomainExistsStatement, subresourceUnderTopFrameDomainExistsQuery, "isRegisteredAsSubresourceUnder"_s);
    return relationshipExists(scopedStatement, domainID(subresourceDomain), topFrameDomain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsSubFrameUnder(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());
    
    auto scopedStatement = this->scopedStatement(m_subframeUnderTopFrameDomainExistsStatement, subframeUnderTopFrameDomainExistsQuery, "isRegisteredAsSubFrameUnder"_s);
    return relationshipExists(scopedStatement, domainID(subFrameDomain), topFrameDomain);
}

bool ResourceLoadStatisticsDatabaseStore::isRegisteredAsRedirectingTo(const RedirectedFromDomain& redirectedFromDomain, const RedirectedToDomain& redirectedToDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_subresourceUniqueRedirectsToExistsStatement, subresourceUniqueRedirectsToExistsQuery, "isRegisteredAsRedirectingTo"_s);
    return relationshipExists(scopedStatement, domainID(redirectedFromDomain), redirectedToDomain);
}

void ResourceLoadStatisticsDatabaseStore::clearPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_clearPrevalentResourceStatement, clearPrevalentResourceQuery, "clearPrevalentResource"_s);
    
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::setGrandfathered(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_updateGrandfatheredStatement, updateGrandfatheredQuery, "setGrandfathered"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_updateIsScheduledForAllButCookieDataRemovalStatement, updateIsScheduledForAllButCookieDataRemovalQuery, "setIsScheduledForAllButCookieDataRemoval"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Seconds ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp(const RegistrableDomain& subDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    Optional<unsigned> subFrameDomainID = domainID(subDomain);
    Optional<unsigned> topFrameDomainID = domainID(topFrameDomain);

    if (!subFrameDomainID || !topFrameDomainID)
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp };
    
    auto scopedStatement = this->scopedStatement(m_getMostRecentlyUpdatedTimestampStatement, getMostRecentlyUpdatedTimestampQuery, "getMostRecentlyUpdatedTimestamp"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, *subFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, *topFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(3, *subFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(4, *topFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(5, *subFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(6, *topFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(7, *subFrameDomainID) != SQLITE_OK
        || scopedStatement->bindInt(8, *topFrameDomainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };
    }
    if (scopedStatement->step() != SQLITE_ROW)
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };

    return Seconds { scopedStatement->getColumnDouble(0) };
}

bool ResourceLoadStatisticsDatabaseStore::isGrandfathered(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_isGrandfatheredStatement, isGrandfatheredQuery, "isGrandfathered"_s);
    return predicateValueForDomain(scopedStatement, domain);
}

void ResourceLoadStatisticsDatabaseStore::setSubframeUnderTopFrameDomain(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubframeUnderTopFrameDomain was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subresourceUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo was not completed due to failed insert attempt", this);
        return;
    }

    insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
    insertDomainRelationshipList(topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

std::pair<ResourceLoadStatisticsDatabaseStore::AddedRecord, Optional<unsigned>> ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    {
        auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "ensureResourceStatisticsForRegistrableDomain"_s);
        if (!scopedStatement
            || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain failed, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return { AddedRecord::No, 0 };
        }

        if (scopedStatement->step() == SQLITE_ROW) {
            unsigned domainID = scopedStatement->getColumnInt(0);
            return { AddedRecord::No, domainID };
        }
    }

    ResourceLoadStatistics newObservation(domain);
    auto result = insertObservedDomain(newObservation);

    if (!result) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain insertObservedDomain failed to complete, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { AddedRecord::No, WTF::nullopt };
    }

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

void ResourceLoadStatisticsDatabaseStore::removeDataForDomain(const RegistrableDomain& domain)
{
    auto domainIDToRemove = domainID(domain);
    if (!domainIDToRemove)
        return;
    
    auto scopedStatement = this->scopedStatement(m_removeAllDataStatement, removeAllDataQuery, "removeDataForDomain"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, *domainIDToRemove) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::removeDataForDomain failed, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::allDomains() const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_getAllDomainsStatement, getAllDomainsQuery, "allDomains"_s);
    if (!scopedStatement)
        return { };

    Vector<RegistrableDomain> domains;
    while (scopedStatement->step() == SQLITE_ROW)
        domains.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(scopedStatement->getColumnText(0)));
    return domains;
}

void ResourceLoadStatisticsDatabaseStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    clearDatabaseContents();
    clearOperatingDates();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator] { });

    auto registrableDomainsToBlockAndDeleteCookiesFor = ensurePrevalentResourcesForDebugMode();
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { }, { } };
    updateCookieBlockingForDomains(domainsToBlock, [callbackAggregator] { });
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

    if (debugLoggingEnabled() && (!domainsToBlockAndDeleteCookiesFor.isEmpty() || !domainsToBlockButKeepCookiesFor.isEmpty()))
        debugLogDomainsInBatches("Applying cross-site tracking restrictions", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(store()), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();

                if (!weakThis)
                    return;

                if (UNLIKELY(weakThis->debugLoggingEnabled())) {
                    RELEASE_LOG_INFO(ITPDebug, "Done applying cross-site tracking restrictions.");
                    weakThis->debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Done applying cross-site tracking restrictions."_s);
                }
            });
        });
    });
}

Vector<ResourceLoadStatisticsDatabaseStore::DomainData> ResourceLoadStatisticsDatabaseStore::domains() const
{
    ASSERT(!RunLoop::isMain());
    
    Vector<DomainData> results;
    SQLiteStatement statement(m_database, "SELECT domainID, registrableDomain, mostRecentUserInteractionTime, hadUserInteraction, grandfathered, isScheduledForAllButCookieDataRemoval, countOfTopFrameRedirects FROM ObservedDomains LEFT JOIN (SELECT sourceDomainID, COUNT(*) as countOfTopFrameRedirects from TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement GROUP BY sourceDomainID) as z ON z.sourceDomainID = domainID"_s);

    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW) {
        results.append({ static_cast<unsigned>(statement.getColumnInt(0))
            , RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(1))
            , WallTime::fromRawSeconds(statement.getColumnDouble(2))
            , statement.getColumnInt(3) ? true : false
            , statement.getColumnInt(4) ? true : false
            , statement.getColumnInt(5) ? true : false
            , static_cast<unsigned>(statement.getColumnInt(6))
        });
    }
    
    return results;
}

Vector<unsigned> ResourceLoadStatisticsDatabaseStore::findExpiredUserInteractions() const
{
    ASSERT(!RunLoop::isMain());

    Vector<unsigned> results;
    Optional<Seconds> expirationDateTime = statisticsExpirationTime();
    if (!expirationDateTime)
        return results;
    
    auto scopedStatement = this->scopedStatement(m_findExpiredUserInteractionStatement, findExpiredUserInteractionQuery, "findExpiredUserInteractions"_s);
    if (!scopedStatement
        || scopedStatement->bindDouble(1, expirationDateTime.value().value()) != SQLITE_OK)
        return results;

    while (scopedStatement->step() == SQLITE_ROW)
        results.append(scopedStatement->getColumnInt(0));

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

bool ResourceLoadStatisticsDatabaseStore::shouldEnforceSameSiteStrictFor(DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
{
    if ((!isSameSiteStrictEnforcementEnabled() && !shouldEnforceSameSiteStrictForSpecificDomain(resourceStatistic.registrableDomain))
        || (shouldCheckForGrandfathering && resourceStatistic.grandfathered))
        return false;

    if (resourceStatistic.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement > parameters().minimumTopFrameRedirectsForSameSiteStrictEnforcement) {
        clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(resourceStatistic.registrableDomain, [] { });
        return true;
    }

    return false;
}

Optional<WallTime> ResourceLoadStatisticsDatabaseStore::mostRecentUserInteractionTime(const DomainData& statistic)
{
    if (statistic.mostRecentUserInteractionTime.secondsSinceEpoch().value() <= 0)
        return WTF::nullopt;

    return statistic.mostRecentUserInteractionTime;
}

RegistrableDomainsToDeleteOrRestrictWebsiteDataFor ResourceLoadStatisticsDatabaseStore::registrableDomainsToDeleteOrRestrictWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    clearExpiredUserInteractions();

    auto now = WallTime::now();
    auto oldestUserInteraction = now;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor toDeleteOrRestrictFor;

    Vector<DomainData> domains = this->domains();
    Vector<unsigned> domainIDsToClearGrandfathering;
    for (auto& statistic : domains) {
        if (shouldExemptFromWebsiteDataDeletion(statistic.registrableDomain))
            continue;
        if (auto mostRecentUserInteractionTime = this->mostRecentUserInteractionTime(statistic))
            oldestUserInteraction = std::min(oldestUserInteraction, *mostRecentUserInteractionTime);
        if (shouldRemoveAllWebsiteDataFor(statistic, shouldCheckForGrandfathering)) {
            toDeleteOrRestrictFor.domainsToDeleteAllCookiesFor.append(statistic.registrableDomain);
            toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.append(statistic.registrableDomain);
        } else {
            if (shouldRemoveAllButCookiesFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.append(statistic.registrableDomain);
                setIsScheduledForAllButCookieDataRemoval(statistic.registrableDomain, false);
            }
            if (shouldEnforceSameSiteStrictFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToEnforceSameSiteStrictFor.append(statistic.registrableDomain);

                if (UNLIKELY(debugLoggingEnabled())) {
                    RELEASE_LOG_INFO(ITPDebug, "Scheduled %" PUBLIC_LOG_STRING " to have its cookies set to SameSite=strict.", statistic.registrableDomain.string().utf8().data());
                    debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Scheduled '", statistic.registrableDomain.string(), "' to have its cookies set to SameSite=strict'."));
                }
            }
        }
        if (shouldClearGrandfathering && statistic.grandfathered)
            domainIDsToClearGrandfathering.append(statistic.domainID);
    }

    // Give the user enough time to interact with websites until we remove non-cookie website data.
    if (!parameters().isRunningTest && now - oldestUserInteraction < parameters().minimumTimeBetweenDataRecordsRemoval)
        toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.clear();

    clearGrandfathering(WTFMove(domainIDsToClearGrandfathering));
    
    return toDeleteOrRestrictFor;
}

void ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    unsigned count = 0;
    auto scopedStatement = this->scopedStatement(m_observedDomainCountStatement, observedDomainCountQuery, "pruneStatisticsIfNeeded"_s);
    if (!scopedStatement)
        return;
    
    if (scopedStatement->step() == SQLITE_ROW)
        count = scopedStatement->getColumnInt(0);

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
    
    auto scopedStatement = this->scopedStatement(m_updateLastSeenStatement, updateLastSeenQuery, "updateLastSeen"_s);
    if (!scopedStatement
        || scopedStatement->bindDouble(1, lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateLastSeen failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setLastSeen was not completed due to failed insert attempt", this);
        return;
    }
    
    updateLastSeen(domain, WallTime::fromRawSeconds(seconds.seconds()));
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;
    
    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }

    setPrevalentResource(domain, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;
    
    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }

    setPrevalentResource(domain, ResourceLoadPrevalence::VeryHigh);
}

void ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved(const RegistrableDomain& domain, int value)
{
    ASSERT(!RunLoop::isMain());
    
    auto scopedStatement = this->scopedStatement(m_updateDataRecordsRemovedStatement, updateDataRecordsRemovedQuery, "updateDataRecordsRemoved"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
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
    
    auto scopedStatement = this->scopedStatement(m_domainStringFromDomainIDStatement, domainStringFromDomainIDQuery, "getDomainStringFromDomainID"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, domainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getDomainStringFromDomainID. Statement failed to prepare or bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return result;
    }
    
    if (scopedStatement->step() == SQLITE_ROW)
        result = m_domainStringFromDomainIDStatement->getColumnText(0);
    
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
    auto scopedStatement = this->scopedStatement(m_getResourceDataByDomainNameStatement, getResourceDataByDomainNameQuery, "resourceToString"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::resourceToString. Statement failed to bind or domain was not found, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    builder.appendLiteral("Registrable domain: ");
    builder.append(domain);
    builder.append('\n');
    
    // User interaction
    appendBoolean(builder, "hadUserInteraction", m_getResourceDataByDomainNameStatement->getColumnInt(HadUserInteractionIndex));
    builder.append('\n');
    builder.appendLiteral("    mostRecentUserInteraction: ");
    if (hasHadRecentUserInteraction(Seconds(m_getResourceDataByDomainNameStatement->getColumnDouble(MostRecentUserInteractionTimeIndex))))
        builder.appendLiteral("within 24 hours");
    else
        builder.appendLiteral("-1");
    builder.append('\n');
    appendBoolean(builder, "grandfathered", m_getResourceDataByDomainNameStatement->getColumnInt(GrandfatheredIndex));
    builder.append('\n');

    // Storage access
    appendSubStatisticList(builder, "StorageAccessUnderTopFrameDomains", domain);

    // Top frame stats
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement", domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLinkDecorationsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLoadedThirdPartyScripts", domain);

    appendBoolean(builder, "IsScheduledForAllButCookieDataRemoval", m_getResourceDataByDomainNameStatement->getColumnInt(IsScheduledForAllButCookieDataRemovalIndex));
    builder.append('\n');

    // Subframe stats
    appendSubStatisticList(builder, "SubframeUnderTopFrameDomains", domain);

    // Subresource stats
    appendSubStatisticList(builder, "SubresourceUnderTopFrameDomains", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsFrom", domain);

    // Prevalent Resource
    appendBoolean(builder, "isPrevalentResource", m_getResourceDataByDomainNameStatement->getColumnInt(IsPrevalentIndex));
    builder.append('\n');
    appendBoolean(builder, "isVeryPrevalentResource", m_getResourceDataByDomainNameStatement->getColumnInt(IsVeryPrevalentIndex));
    builder.append('\n');
    builder.appendLiteral("    dataRecordsRemoved: ");
    builder.appendNumber(m_getResourceDataByDomainNameStatement->getColumnInt(DataRecordsRemovedIndex));
    builder.append('\n');
}

bool ResourceLoadStatisticsDatabaseStore::domainIDExistsInDatabase(int domainID)
{
    auto scopedLinkDecorationExistsStatement = this->scopedStatement(m_linkDecorationExistsStatement, linkDecorationExistsQuery, "domainIDExistsInDatabase"_s);
    auto scopedScriptLoadExistsStatement = this->scopedStatement(m_scriptLoadExistsStatement, scriptLoadExistsQuery, "domainIDExistsInDatabase linkDecorationExistsStatement"_s);
    auto scopedSubFrameExistsStatement = this->scopedStatement(m_subFrameExistsStatement, subFrameExistsQuery, "domainIDExistsInDatabase subFrameExistsStatement"_s);
    auto scopedSubResourceExistsStatement = this->scopedStatement(m_subResourceExistsStatement, subResourceExistsQuery, "domainIDExistsInDatabase subResourceExistsStatement"_s);
    auto scopedUniqueRedirectExistsStatement = this->scopedStatement(m_uniqueRedirectExistsStatement, uniqueRedirectExistsQuery, "domainIDExistsInDatabase uniqueRedirectExistsStatement"_s);
    auto scopedObservedDomainsExistsStatement = this->scopedStatement(m_observedDomainsExistsStatement, observedDomainsExistsQuery, "domainIDExistsInDatabase observedDomainsExistsStatement"_s);
    
    if (!scopedLinkDecorationExistsStatement
        || !scopedScriptLoadExistsStatement
        || !scopedSubFrameExistsStatement
        || !scopedSubResourceExistsStatement
        || !scopedUniqueRedirectExistsStatement
        || !scopedObservedDomainsExistsStatement
        || m_linkDecorationExistsStatement->bindInt(1, domainID) != SQLITE_OK
        || m_linkDecorationExistsStatement->bindInt(2, domainID) != SQLITE_OK
        || m_scriptLoadExistsStatement->bindInt(1, domainID) != SQLITE_OK
        || m_scriptLoadExistsStatement->bindInt(2, domainID) != SQLITE_OK
        || m_subFrameExistsStatement->bindInt(1, domainID) != SQLITE_OK
        || m_subFrameExistsStatement->bindInt(2, domainID) != SQLITE_OK
        || m_subResourceExistsStatement->bindInt(1, domainID) != SQLITE_OK
        || m_subResourceExistsStatement->bindInt(2, domainID) != SQLITE_OK
        || m_uniqueRedirectExistsStatement->bindInt(1, domainID) != SQLITE_OK
        || m_uniqueRedirectExistsStatement->bindInt(2, domainID) != SQLITE_OK
        || m_observedDomainsExistsStatement->bindInt(1, domainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDExistsInDatabase failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_linkDecorationExistsStatement->step() != SQLITE_ROW
        || m_scriptLoadExistsStatement->step() != SQLITE_ROW
        || m_subFrameExistsStatement->step() != SQLITE_ROW
        || m_subResourceExistsStatement->step() != SQLITE_ROW
        || m_uniqueRedirectExistsStatement->step() != SQLITE_ROW
        || m_observedDomainsExistsStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDExistsInDatabase failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    return m_linkDecorationExistsStatement->getColumnInt(0) || m_scriptLoadExistsStatement->getColumnInt(0) || m_subFrameExistsStatement->getColumnInt(0) || m_subResourceExistsStatement->getColumnInt(0) || m_uniqueRedirectExistsStatement->getColumnInt(0) || m_observedDomainsExistsStatement->getColumnInt(0);
}

void ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters()
{
    SQLiteStatement countOperatingDatesStatement(m_database, "SELECT COUNT(*) FROM OperatingDates;"_s);
    SQLiteStatement getMostRecentOperatingDateStatement(m_database, "SELECT * FROM OperatingDates ORDER BY year DESC, month DESC, monthDay DESC LIMIT 1;"_s);
    SQLiteStatement getLeastRecentOperatingDateStatement(m_database, "SELECT * FROM OperatingDates ORDER BY year, month, monthDay LIMIT 1;"_s);

    if (countOperatingDatesStatement.prepare() != SQLITE_OK
        || countOperatingDatesStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters countOperatingDatesStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    m_operatingDatesSize = countOperatingDatesStatement.getColumnInt(0);

    if (getMostRecentOperatingDateStatement.prepare() != SQLITE_OK
        || getMostRecentOperatingDateStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getFirstOperatingDateStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    m_mostRecentOperatingDate = OperatingDate(getMostRecentOperatingDateStatement.getColumnInt(0), getMostRecentOperatingDateStatement.getColumnInt(1), getMostRecentOperatingDateStatement.getColumnInt(2));

    if (getLeastRecentOperatingDateStatement.prepare() != SQLITE_OK
        || getLeastRecentOperatingDateStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getLeastRecentOperatingDateStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    m_leastRecentOperatingDate = OperatingDate(getLeastRecentOperatingDateStatement.getColumnInt(0), getLeastRecentOperatingDateStatement.getColumnInt(1), getLeastRecentOperatingDateStatement.getColumnInt(2));
}

Optional<Seconds> ResourceLoadStatisticsDatabaseStore::statisticsExpirationTime() const
{
    ASSERT(!RunLoop::isMain());

    if (this->parameters().timeToLiveUserInteraction)
        return WallTime::now().secondsSinceEpoch() - this->parameters().timeToLiveUserInteraction.value();

    if (m_operatingDatesSize >= operatingDatesWindowLong)
        return m_leastRecentOperatingDate.secondsSinceEpoch();

    return WTF::nullopt;
}

void ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());
    
    auto today = OperatingDate::today();
    if (m_operatingDatesSize > 0) {
        if (today <= m_mostRecentOperatingDate)
            return;
    }

    int rowsToPrune = m_operatingDatesSize - operatingDatesWindowLong + 1;
    if (rowsToPrune > 0) {
        SQLiteStatement deleteLeastRecentOperatingDateStatement(m_database, "DELETE FROM OperatingDates ORDER BY year, month, monthDay LIMIT ?;"_s);
        if (deleteLeastRecentOperatingDateStatement.prepare() != SQLITE_OK
            || deleteLeastRecentOperatingDateStatement.bindInt(1, rowsToPrune) != SQLITE_OK
            || deleteLeastRecentOperatingDateStatement.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary deleteLeastRecentOperatingDateStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
    }
    
    SQLiteStatement insertOperatingDateStatement(m_database, "INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
    if (insertOperatingDateStatement.prepare() != SQLITE_OK
        || insertOperatingDateStatement.bindInt(1, today.year()) != SQLITE_OK
        || insertOperatingDateStatement.bindInt(2, today.month()) != SQLITE_OK
        || insertOperatingDateStatement.bindInt(3, today.monthDay()) != SQLITE_OK
        || insertOperatingDateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary insertOperatingDateStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    updateOperatingDatesParameters();
}

bool ResourceLoadStatisticsDatabaseStore::hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow operatingDatesWindow) const
{
    ASSERT(!RunLoop::isMain());

    unsigned operatingDatesWindowInDays = 0;
    switch (operatingDatesWindow) {
    case OperatingDatesWindow::Long:
        operatingDatesWindowInDays = operatingDatesWindowLong;
        break;
    case OperatingDatesWindow::Short:
        operatingDatesWindowInDays = operatingDatesWindowShort;
        break;
    case OperatingDatesWindow::ForLiveOnTesting:
        return WallTime::now() > mostRecentUserInteractionTime + operatingTimeWindowForLiveOnTesting;
    case OperatingDatesWindow::ForReproTesting:
        return true;
    }

    if (m_operatingDatesSize >= operatingDatesWindowInDays) {
        if (OperatingDate::fromWallTime(mostRecentUserInteractionTime) < m_leastRecentOperatingDate)
            return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (this->parameters().timeToLiveUserInteraction) {
        if (WallTime::now() > mostRecentUserInteractionTime + this->parameters().timeToLiveUserInteraction.value())
            return true;
    }
    return false;
}

void ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting(const RegistrableDomain& domain, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent)
{
    // Populate the Operating Dates table with enough days to require pruning.
    double daysAgoInSeconds = 0;
    for (unsigned i = 1; i <= operatingDatesWindowLong; i++) {
        double daysToSubtract = Seconds::fromHours(24 * i).value();
        daysAgoInSeconds = WallTime::now().secondsSinceEpoch().value() - daysToSubtract;
        auto dateToInsert = OperatingDate::fromWallTime(WallTime::fromRawSeconds(daysAgoInSeconds));

        SQLiteStatement insertOperatingDateStatement(m_database, "INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
        if (insertOperatingDateStatement.prepare() != SQLITE_OK
            || insertOperatingDateStatement.bindInt(1, dateToInsert.year()) != SQLITE_OK
            || insertOperatingDateStatement.bindInt(2, dateToInsert.month()) != SQLITE_OK
            || insertOperatingDateStatement.bindInt(3, dateToInsert.monthDay()) != SQLITE_OK
            || insertOperatingDateStatement.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting insertOperatingDateStatement failed to step, error message: %{private}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
        insertOperatingDateStatement.reset();
    }

    updateOperatingDatesParameters();

    // Make sure mostRecentUserInteractionTime is the least recent of all entries.
    daysAgoInSeconds -= Seconds::fromHours(24).value();
    auto scopedInsertObservedDomainStatement = this->scopedStatement(m_insertObservedDomainStatement, insertObservedDomainQuery, "insertExpiredStatisticForTesting"_s);
    if (scopedInsertObservedDomainStatement->bindText(RegistrableDomainIndex, domain.string()) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindDouble(LastSeenIndex, daysAgoInSeconds) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(HadUserInteractionIndex, hasUserInteraction) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindDouble(MostRecentUserInteractionTimeIndex, daysAgoInSeconds) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(GrandfatheredIndex, false) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(IsPrevalentIndex, isPrevalent) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(IsVeryPrevalentIndex, false) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(DataRecordsRemovedIndex, 0) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(TimesAccessedAsFirstPartyDueToUserInteractionIndex, 0) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(TimesAccessedAsFirstPartyDueToStorageAccessAPIIndex, 0) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindInt(IsScheduledForAllButCookieDataRemovalIndex, isScheduledForAllButCookieDataRemoval) != SQLITE_OK) {
        RELEASE_LOG_ERROR_IF_ALLOWED(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting failed to bind, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    if (scopedInsertObservedDomainStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting failed to commit, error message: %{private}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace WebKit

#endif
