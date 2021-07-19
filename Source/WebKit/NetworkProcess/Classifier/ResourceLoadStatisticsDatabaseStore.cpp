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
#include "PrivateClickMeasurementManager.h"
#include "ResourceLoadStatisticsMemoryStore.h"
#include "StorageAccessStatus.h"
#include "WebProcessProxy.h"
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
#include <wtf/MathExtras.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/Scope.h>
#include <wtf/StdSet.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

#define ITP_RELEASE_LOG(sessionID, fmt, ...) RELEASE_LOG(Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)
#define ITP_RELEASE_LOG_ERROR(sessionID, fmt, ...) RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::" fmt, this, ##__VA_ARGS__)

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
constexpr auto insertUnattributedPrivateClickMeasurementQuery = "INSERT OR REPLACE INTO UnattributedPrivateClickMeasurement (sourceSiteDomainID, destinationSiteDomainID, "
    "sourceID, timeOfAdClick, token, signature, keyID) VALUES (?, ?, ?, ?, ?, ?, ?)"_s;
constexpr auto insertAttributedPrivateClickMeasurementQuery = "INSERT OR REPLACE INTO AttributedPrivateClickMeasurement (sourceSiteDomainID, destinationSiteDomainID, "
    "sourceID, attributionTriggerData, priority, timeOfAdClick, earliestTimeToSendToSource, token, signature, keyID, earliestTimeToSendToDestination) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;

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
constexpr auto mostRecentUserInteractionQuery = "UPDATE ObservedDomains SET hadUserInteraction = ?, mostRecentUserInteractionTime = ? "_s
    "WHERE registrableDomain = ?"_s;
constexpr auto updateLastSeenQuery = "UPDATE ObservedDomains SET lastSeen = ? WHERE registrableDomain = ?"_s;
constexpr auto updateDataRecordsRemovedQuery = "UPDATE ObservedDomains SET dataRecordsRemoved = ? WHERE registrableDomain = ?"_s;
constexpr auto updatePrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto updateVeryPrevalentResourceQuery = "UPDATE ObservedDomains SET isVeryPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto clearPrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = 0, isVeryPrevalent = 0 WHERE registrableDomain = ?"_s;
constexpr auto updateGrandfatheredQuery = "UPDATE ObservedDomains SET grandfathered = ? WHERE registrableDomain = ?"_s;
constexpr auto updateIsScheduledForAllButCookieDataRemovalQuery = "UPDATE ObservedDomains SET isScheduledForAllButCookieDataRemoval = ? WHERE registrableDomain = ?"_s;
constexpr auto setUnattributedPrivateClickMeasurementAsExpiredQuery = "UPDATE UnattributedPrivateClickMeasurement SET timeOfAdClick = -1.0"_s;
constexpr auto markReportAsSentToSourceQuery = "UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToSource = null WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;
constexpr auto markReportAsSentToDestinationQuery = "UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToDestination = null WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;

// SELECT Queries
constexpr auto domainIDFromStringQuery = "SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto domainStringFromDomainIDQuery = "SELECT registrableDomain FROM ObservedDomains WHERE domainID = ?"_s;
constexpr auto isPrevalentResourceQuery = "SELECT isPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isVeryPrevalentResourceQuery = "SELECT isVeryPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto hadUserInteractionQuery = "SELECT hadUserInteraction, mostRecentUserInteractionTime FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isGrandfatheredQuery = "SELECT grandfathered FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto findExpiredUserInteractionQuery = "SELECT domainID FROM ObservedDomains WHERE hadUserInteraction = 1 AND mostRecentUserInteractionTime < ?"_s;
constexpr auto getResourceDataByDomainNameQuery = "SELECT * FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto getMostRecentlyUpdatedTimestampQuery = "SELECT MAX(lastUpdated) FROM (SELECT lastUpdated FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? and topFrameDomainID = ?"_s
    "UNION ALL SELECT lastUpdated FROM TopFrameLinkDecorationsFrom WHERE toDomainID = ? and fromDomainID = ?"
    "UNION ALL SELECT lastUpdated FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? and topFrameDomainID = ?"
    "UNION ALL SELECT lastUpdated FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? and toDomainID = ?)"_s;
constexpr auto getAllDomainsQuery = "SELECT registrableDomain FROM ObservedDomains"_s;
constexpr auto getAllSubStatisticsUnderDomainQuery = "SELECT topFrameDomainID FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ?"
    "UNION ALL SELECT topFrameDomainID FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ?"
    "UNION ALL SELECT toDomainID FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ?"_s;
constexpr auto allUnattributedPrivateClickMeasurementAttributionsQuery = "SELECT * FROM UnattributedPrivateClickMeasurement"_s;
constexpr auto allAttributedPrivateClickMeasurementQuery = "SELECT *, MIN(earliestTimeToSendToSource, earliestTimeToSendToDestination) as minVal "
    "FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToSource IS NOT NULL AND earliestTimeToSendToDestination IS NOT NULL "
    "UNION ALL SELECT *, earliestTimeToSendToSource as minVal FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToDestination IS NULL "
    "UNION ALL SELECT *, earliestTimeToSendToDestination as minVal FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToSource IS NULL ORDER BY minVal"_s;
constexpr auto findUnattributedQuery = "SELECT * FROM UnattributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;
constexpr auto findAttributedQuery = "SELECT * FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;
constexpr auto earliestTimesToSendQuery = "SELECT earliestTimeToSendToSource, earliestTimeToSendToDestination FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;

// EXISTS for testing queries
constexpr auto linkDecorationExistsQuery = "SELECT EXISTS (SELECT * FROM TopFrameLinkDecorationsFrom WHERE toDomainID = ? OR fromDomainID = ?)"_s;
constexpr auto scriptLoadExistsQuery = "SELECT EXISTS (SELECT * FROM TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ? OR subresourceDomainID = ?)"_s;
constexpr auto subFrameExistsQuery = "SELECT EXISTS (SELECT * FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? OR topFrameDomainID = ?)"_s;
constexpr auto subResourceExistsQuery = "SELECT EXISTS (SELECT * FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? OR topFrameDomainID = ?)"_s;
constexpr auto uniqueRedirectExistsQuery = "SELECT EXISTS (SELECT * FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? OR toDomainID = ?)"_s;
constexpr auto observedDomainsExistsQuery = "SELECT EXISTS (SELECT * FROM ObservedDomains WHERE domainID = ?)"_s;

// DELETE Queries
constexpr auto removeAllDataQuery = "DELETE FROM ObservedDomains WHERE domainID = ?"_s;
constexpr auto clearUnattributedPrivateClickMeasurementQuery = "DELETE FROM UnattributedPrivateClickMeasurement WHERE sourceSiteDomainID LIKE ? OR destinationSiteDomainID LIKE ?"_s;
constexpr auto clearAttributedPrivateClickMeasurementQuery = "DELETE FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID LIKE ? OR destinationSiteDomainID LIKE ?"_s;
constexpr auto clearExpiredPrivateClickMeasurementQuery = "DELETE FROM UnattributedPrivateClickMeasurement WHERE ? > timeOfAdClick"_s;
constexpr auto removeUnattributedQuery = "DELETE FROM UnattributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s;

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
    "REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;
    
constexpr auto createStorageAccessUnderTopFrameDomains = "CREATE TABLE StorageAccessUnderTopFrameDomains ("
    "domainID INTEGER NOT NULL, topLevelDomainID INTEGER NOT NULL ON CONFLICT FAIL, "
    "CONSTRAINT fkDomainID FOREIGN KEY(domainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topLevelDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;
    
constexpr auto createTopFrameUniqueRedirectsTo = "CREATE TABLE TopFrameUniqueRedirectsTo ("
    "sourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(sourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement = "CREATE TABLE TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement ("
    "sourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(sourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createTopFrameUniqueRedirectsFrom = "CREATE TABLE TopFrameUniqueRedirectsFrom ("
    "targetDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(targetDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createTopFrameLinkDecorationsFrom = "CREATE TABLE TopFrameLinkDecorationsFrom ("
    "toDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createTopFrameLoadedThirdPartyScripts = "CREATE TABLE TopFrameLoadedThirdPartyScripts ("
    "topFrameDomainID INTEGER NOT NULL, subresourceDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createSubframeUnderTopFrameDomains = "CREATE TABLE SubframeUnderTopFrameDomains ("
    "subFrameDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;
    
constexpr auto createSubresourceUnderTopFrameDomains = "CREATE TABLE SubresourceUnderTopFrameDomains ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;
    
constexpr auto createSubresourceUniqueRedirectsTo = "CREATE TABLE SubresourceUniqueRedirectsTo ("
    "subresourceDomainID INTEGER NOT NULL, lastUpdated REAL NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;
    
constexpr auto createSubresourceUniqueRedirectsFrom = "CREATE TABLE SubresourceUniqueRedirectsFrom ("
    "subresourceDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createOperatingDates = "CREATE TABLE OperatingDates ("
    "year INTEGER NOT NULL, month INTEGER NOT NULL, monthDay INTEGER NOT NULL)"_s;

constexpr auto createUnattributedPrivateClickMeasurement = "CREATE TABLE UnattributedPrivateClickMeasurement ("
    "sourceSiteDomainID INTEGER NOT NULL, destinationSiteDomainID INTEGER NOT NULL, sourceID INTEGER NOT NULL, "
    "timeOfAdClick REAL NOT NULL, token TEXT, signature TEXT, keyID TEXT, FOREIGN KEY(sourceSiteDomainID) "
    "REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, FOREIGN KEY(destinationSiteDomainID) REFERENCES "
    "ObservedDomains(domainID) ON DELETE CASCADE)"_s;

constexpr auto createAttributedPrivateClickMeasurement = "CREATE TABLE AttributedPrivateClickMeasurement ("
    "sourceSiteDomainID INTEGER NOT NULL, destinationSiteDomainID INTEGER NOT NULL, sourceID INTEGER NOT NULL, "
    "attributionTriggerData INTEGER NOT NULL, priority INTEGER NOT NULL, timeOfAdClick REAL NOT NULL, "
    "earliestTimeToSendToSource REAL, token TEXT, signature TEXT, keyID TEXT, earliestTimeToSendToDestination REAL, "
    "FOREIGN KEY(sourceSiteDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, FOREIGN KEY(destinationSiteDomainID) REFERENCES "
    "ObservedDomains(domainID) ON DELETE CASCADE)"_s;

// CREATE UNIQUE INDEX Queries.
constexpr auto createUniqueIndexStorageAccessUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS StorageAccessUnderTopFrameDomains_domainID_topLevelDomainID on StorageAccessUnderTopFrameDomains ( domainID, topLevelDomainID )"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsTo_sourceDomainID_toDomainID on TopFrameUniqueRedirectsTo ( sourceDomainID, toDomainID )"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement_sourceDomainID_toDomainID on TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement ( sourceDomainID, toDomainID )"_s;
constexpr auto createUniqueIndexTopFrameUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameUniqueRedirectsFrom_targetDomainID_fromDomainID on TopFrameUniqueRedirectsFrom ( targetDomainID, fromDomainID )"_s;
constexpr auto createUniqueIndexTopFrameLinkDecorationsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLinkDecorationsFrom_toDomainID_fromDomainID on TopFrameLinkDecorationsFrom ( toDomainID, fromDomainID )"_s;
constexpr auto createUniqueIndexTopFrameLoadedThirdPartyScripts = "CREATE UNIQUE INDEX IF NOT EXISTS TopFrameLoadedThirdPartyScripts_topFrameDomainID_subresourceDomainID on TopFrameLoadedThirdPartyScripts ( topFrameDomainID, subresourceDomainID )"_s;
constexpr auto createUniqueIndexSubframeUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubframeUnderTopFrameDomains_subFrameDomainID_topFrameDomainID on SubframeUnderTopFrameDomains ( subFrameDomainID, topFrameDomainID )"_s;
constexpr auto createUniqueIndexSubresourceUnderTopFrameDomains = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUnderTopFrameDomains_subresourceDomainID_topFrameDomainID on SubresourceUnderTopFrameDomains ( subresourceDomainID, topFrameDomainID )"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsTo = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsTo_subresourceDomainID_toDomainID on SubresourceUniqueRedirectsTo ( subresourceDomainID, toDomainID )"_s;
constexpr auto createUniqueIndexSubresourceUniqueRedirectsFrom = "CREATE UNIQUE INDEX IF NOT EXISTS SubresourceUniqueRedirectsFrom_subresourceDomainID_fromDomainID on SubresourceUniqueRedirectsFrom ( subresourceDomainID, fromDomainID )"_s;
constexpr auto createUniqueIndexOperatingDates = "CREATE UNIQUE INDEX IF NOT EXISTS OperatingDates_year_month_monthDay on OperatingDates ( year, month, monthDay )"_s;
constexpr auto createUniqueIndexUnattributedPrivateClickMeasurement = "CREATE UNIQUE INDEX IF NOT EXISTS UnattributedPrivateClickMeasurement_sourceSiteDomainID_destinationSiteDomainID on UnattributedPrivateClickMeasurement ( sourceSiteDomainID, destinationSiteDomainID )"_s;
constexpr auto createUniqueIndexAttributedPrivateClickMeasurement = "CREATE UNIQUE INDEX IF NOT EXISTS AttributedPrivateClickMeasurement_sourceSiteDomainID_destinationSiteDomainID on AttributedPrivateClickMeasurement ( sourceSiteDomainID, destinationSiteDomainID )"_s;

// Add one to the count of the above index queries to account for the ObservedDomains table, which has an index automatically created by SQLite because of its primary key.
const int expectedIndexCount = 14;

static const String ObservedDomainsTableSchemaV1()
{
    return createObservedDomain;
}

static const String ObservedDomainsTableSchemaV1Alternate()
{
    return "CREATE TABLE \"ObservedDomains\" (domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL,isScheduledForAllButCookieDataRemoval INTEGER NOT NULL)"_s;
}

static const ExpectedColumns& expectedUnattributedColumns()
{
    static const auto columns = makeNeverDestroyed(Vector<String> { "sourceSiteDomainID"_s, "destinationSiteDomainID"_s, "sourceID"_s, "timeOfAdClick"_s, "token"_s, "signature"_s, "keyID"_s });
    return columns.get();
}

static const ExpectedColumns& expectedAttributedColumns()
{
    static const auto columns = makeNeverDestroyed(Vector<String> { "sourceSiteDomainID"_s, "destinationSiteDomainID"_s, "sourceID"_s, "attributionTriggerData"_s, "priority"_s, "timeOfAdClick"_s, "earliestTimeToSendToSource"_s, "token"_s, "signature"_s, "keyID"_s, "earliestTimeToSendToDestination"_s });
    return columns.get();
}

static bool needsNewCreateTableSchema(const String& schema)
{
    return schema.contains("REFERENCES TopLevelDomains");
}

static String stripIndexQueryToMatchStoredValue(const char* originalQuery)
{
    return String(originalQuery).replace("CREATE UNIQUE INDEX IF NOT EXISTS", "CREATE UNIQUE INDEX");
}

static const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& expectedTableAndIndexQueries()
{
    static auto expectedTableAndIndexQueries = makeNeverDestroyed(MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair> {
        { "ObservedDomains"_s, std::make_pair<String, std::optional<String>>(createObservedDomain, std::nullopt) },
        { "TopLevelDomains"_s, std::make_pair<String, std::optional<String>>(createTopLevelDomains, std::nullopt) },
        { "StorageAccessUnderTopFrameDomains"_s, std::make_pair<String, std::optional<String>>(createStorageAccessUnderTopFrameDomains, stripIndexQueryToMatchStoredValue(createUniqueIndexStorageAccessUnderTopFrameDomains)) },
        { "TopFrameUniqueRedirectsTo"_s, std::make_pair<String, std::optional<String>>(createTopFrameUniqueRedirectsTo, stripIndexQueryToMatchStoredValue(createUniqueIndexTopFrameUniqueRedirectsTo)) },
        { "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement"_s, std::make_pair<String, std::optional<String>>(createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement, stripIndexQueryToMatchStoredValue(createUniqueIndexTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement)) },
        { "TopFrameUniqueRedirectsFrom"_s, std::make_pair<String, std::optional<String>>(createTopFrameUniqueRedirectsFrom, stripIndexQueryToMatchStoredValue(createUniqueIndexTopFrameUniqueRedirectsFrom)) },
        { "TopFrameLinkDecorationsFrom"_s, std::make_pair<String, std::optional<String>>(createTopFrameLinkDecorationsFrom, stripIndexQueryToMatchStoredValue(createUniqueIndexTopFrameLinkDecorationsFrom)) },
        { "TopFrameLoadedThirdPartyScripts"_s, std::make_pair<String, std::optional<String>>(createTopFrameLoadedThirdPartyScripts, stripIndexQueryToMatchStoredValue(createUniqueIndexTopFrameLoadedThirdPartyScripts)) },
        { "SubframeUnderTopFrameDomains"_s, std::make_pair<String, std::optional<String>>(createSubframeUnderTopFrameDomains, stripIndexQueryToMatchStoredValue(createUniqueIndexSubframeUnderTopFrameDomains)) },
        { "SubresourceUnderTopFrameDomains"_s, std::make_pair<String, std::optional<String>>(createSubresourceUnderTopFrameDomains, stripIndexQueryToMatchStoredValue(createUniqueIndexSubresourceUnderTopFrameDomains)) },
        { "SubresourceUniqueRedirectsTo"_s, std::make_pair<String, std::optional<String>>(createSubresourceUniqueRedirectsTo, stripIndexQueryToMatchStoredValue(createUniqueIndexSubresourceUniqueRedirectsTo)) },
        { "SubresourceUniqueRedirectsFrom"_s, std::make_pair<String, std::optional<String>>(createSubresourceUniqueRedirectsFrom, stripIndexQueryToMatchStoredValue(createUniqueIndexSubresourceUniqueRedirectsFrom)) },
        { "OperatingDates"_s, std::make_pair<String, std::optional<String>>(createOperatingDates, stripIndexQueryToMatchStoredValue(createUniqueIndexOperatingDates)) },
        { "UnattributedPrivateClickMeasurement"_s, std::make_pair<String, std::optional<String>>(createUnattributedPrivateClickMeasurement, stripIndexQueryToMatchStoredValue(createUniqueIndexUnattributedPrivateClickMeasurement)) },
        { "AttributedPrivateClickMeasurement"_s, std::make_pair<String, std::optional<String>>(createAttributedPrivateClickMeasurement, stripIndexQueryToMatchStoredValue(createUniqueIndexAttributedPrivateClickMeasurement)) }
    });
    
    return expectedTableAndIndexQueries;
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
        if (!FileSystem::makeAllDirectories(FileSystem::parentPath(m_storageDirectoryPath))) {
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
    
    auto setBusyTimeout = m_database.prepareStatement("PRAGMA busy_timeout = 5000"_s);
    if (!setBusyTimeout || setBusyTimeout->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::setBusyTimeout failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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

std::optional<Vector<String>> ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema()
{
    Vector<String> missingTables;
    auto statement = m_database.prepareStatement("SELECT 1 from sqlite_master WHERE type='table' and tbl_name=?"_s);
    if (!statement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema failed to prepare, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
        return std::nullopt;
    }

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        if (statement->bindText(1, table) != SQLITE_OK) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema failed to bind, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
            return std::nullopt;
        }
        if (statement->step() != SQLITE_ROW) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::checkForMissingTablesInSchema schema is missing table: %s", this, table.ascii().data());
            missingTables.append(String(table));
        }
        statement->reset();
    }
    if (missingTables.isEmpty())
        return std::nullopt;

    return missingTables;
}

void ResourceLoadStatisticsDatabaseStore::enableForeignKeys()
{
    auto enableForeignKeys = m_database.prepareStatement("PRAGMA foreign_keys = ON"_s);
    if (!enableForeignKeys || enableForeignKeys->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::enableForeignKeys failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

TableAndIndexPair ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries(const String& tableName)
{
    auto getTableStatement = m_database.prepareStatement("SELECT sql FROM sqlite_master WHERE tbl_name=? AND type = 'table'"_s);
    if (!getTableStatement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries Unable to prepare statement to fetch schema for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (getTableStatement->bindText(1, tableName) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries Unable to bind statement to fetch schema for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (getTableStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries error executing statement to fetch table schema, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    String createTableQuery = getTableStatement->columnText(0);

    auto getIndexStatement = m_database.prepareStatement("SELECT sql FROM sqlite_master WHERE tbl_name=? AND type = 'index'"_s);
    if (!getIndexStatement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries Unable to prepare statement to fetch index for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (getIndexStatement->bindText(1, tableName) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::currentTableAndIndexQueries Unable to bind statement to fetch index for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    std::optional<String> index;
    if (getIndexStatement->step() == SQLITE_ROW) {
        auto rawIndex = String(getIndexStatement->columnText(0));
        if (!rawIndex.isEmpty())
            index = rawIndex;
    }

    return std::make_pair<String, std::optional<String>>(WTFMove(createTableQuery), WTFMove(index));
}

bool ResourceLoadStatisticsDatabaseStore::needsUpdatedPrivateClickMeasurementSchema()
{
    auto currentSchema = currentTableAndIndexQueries("AttributedPrivateClickMeasurement"_s).first;

    if (!currentSchema.isEmpty() && currentSchema != createAttributedPrivateClickMeasurement)
        return true;

    return false;
}

bool ResourceLoadStatisticsDatabaseStore::missingUniqueIndices()
{
    auto statement = m_database.prepareStatement("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index'"_s);
    if (!statement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::missingUniqueIndices Unable to prepare statement to fetch index count, error message: %s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (statement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::missingUniqueIndices error executing statement to fetch index count, error message: %s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (statement->columnInt(0) < expectedIndexCount)
        return true;

    return false;
}

bool ResourceLoadStatisticsDatabaseStore::missingReferenceToObservedDomains()
{
    // Check a table for a reference to TopLevelDomains, a sign of the old schema.
    auto oldSchema = currentTableAndIndexQueries("TopFrameUniqueRedirectsTo"_s).first;
    return needsNewCreateTableSchema(oldSchema);
}

bool ResourceLoadStatisticsDatabaseStore::needsUpdatedSchema()
{
    // There are 3 cases where we expect potential schema changes due to upgrades.
    // All other tables should be up-to-date, so we should ASSERT that they are correct.
    if (missingReferenceToObservedDomains() || needsUpdatedPrivateClickMeasurementSchema() || missingUniqueIndices())
        return true;

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        UNUSED_PARAM(table);
        ASSERT(currentTableAndIndexQueries(table) == expectedTableAndIndexQueries().get(table));
    }

    return false;
}

static Expected<SQLiteStatement, int> insertDistinctValuesInTableStatement(SQLiteDatabase& database, const String& table)
{
    if (table == "SubframeUnderTopFrameDomains")
        return database.prepareStatement("INSERT INTO SubframeUnderTopFrameDomains SELECT subFrameDomainID, MAX(lastUpdated), topFrameDomainID FROM _SubframeUnderTopFrameDomains GROUP BY subFrameDomainID, topFrameDomainID"_s);

    if (table == "SubresourceUnderTopFrameDomains")
        return database.prepareStatement("INSERT INTO SubresourceUnderTopFrameDomains SELECT subresourceDomainID, MAX(lastUpdated), topFrameDomainID FROM _SubresourceUnderTopFrameDomains GROUP BY subresourceDomainID, topFrameDomainID"_s);

    if (table == "SubresourceUniqueRedirectsTo")
        return database.prepareStatement("INSERT INTO SubresourceUniqueRedirectsTo SELECT subresourceDomainID, MAX(lastUpdated), toDomainID FROM _SubresourceUniqueRedirectsTo GROUP BY subresourceDomainID, toDomainID"_s);

    if (table == "TopFrameLinkDecorationsFrom")
        return database.prepareStatement("INSERT INTO TopFrameLinkDecorationsFrom SELECT toDomainID, MAX(lastUpdated), fromDomainID FROM _TopFrameLinkDecorationsFrom GROUP BY toDomainID, fromDomainID"_s);

    if (table == "UnattributedPrivateClickMeasurement")
        return database.prepareStatement("INSERT INTO UnattributedPrivateClickMeasurement SELECT sourceSiteDomainID, destinationSiteDomainID, sourceID, MAX(timeOfAdClick), token, signature, keyID FROM _UnattributedPrivateClickMeasurement GROUP BY sourceSiteDomainID, destinationSiteDomainID"_s);

    if (table == "AttributedPrivateClickMeasurement")
        return database.prepareStatement("INSERT INTO AttributedPrivateClickMeasurement SELECT sourceSiteDomainID, destinationSiteDomainID, sourceID, attributionTriggerData, priority, MAX(timeOfAdClick), earliestTimeToSendToSource, token, signature, keyID, earliestTimeToSendToDestination FROM _AttributedPrivateClickMeasurement GROUP BY sourceSiteDomainID, destinationSiteDomainID"_s);

    return database.prepareStatementSlow(makeString("INSERT INTO ", table, " SELECT DISTINCT * FROM _", table));
}

void ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary()
{
    if (!needsUpdatedSchema())
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        auto alterTable = m_database.prepareStatementSlow(makeString("ALTER TABLE ", table, " RENAME TO _", table));
        if (!alterTable || alterTable->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary failed to rename table, error message: %s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    if (!createSchema()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary failed to create schema, error message: %s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        auto migrateTableData = insertDistinctValuesInTableStatement(m_database, table);
        if (!migrateTableData || migrateTableData->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary failed to migrate schema, error message: %s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }

        auto dropTableQuery = m_database.prepareStatementSlow(makeString("DROP TABLE _", table));
        if (!dropTableQuery || dropTableQuery->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary failed to drop temporary tables, error message: %s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    transaction.commit();

    if (!createUniqueIndices()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::migrateDataToNewTablesIfNecessary failed to create unique indices, error message: %s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

Vector<String> ResourceLoadStatisticsDatabaseStore::columnsForTable(const String& tableName)
{
    auto statement = m_database.prepareStatementSlow(makeString("PRAGMA table_info(", tableName, ")"));

    if (!statement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::getColumnsFromTableInfoStatement Unable to prepare statement to fetch schema for table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    Vector<String> columns;
    while (statement->step() == SQLITE_ROW) {
        auto name = statement->columnText(1);
        columns.append(name);
    }

    return columns;
}

void ResourceLoadStatisticsDatabaseStore::addMissingColumnsToTable(String&& tableName, const ExistingColumns& existingColumns, const ExpectedColumns& expectedColumns)
{
    ASSERT(existingColumns.size() <= expectedColumns.size());

    SQLiteTransaction transaction(m_database);
    for (auto& column : expectedColumns) {
        if (existingColumns.contains(column))
            continue;
        
        if (!transaction.inProgress())
            transaction.begin();

        auto statement = m_database.prepareStatementSlow(makeString("ALTER TABLE ", tableName, " ADD COLUMN ", column));
        if (!statement) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingColumnsToTable Unable to prepare statement to add missing columns to table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
        if (statement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingColumnsToTable error executing statement to add missing columns to table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
        statement->reset();
    }
    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::addMissingColumnsIfNecessary()
{
    const auto unattributedTableName = "UnattributedPrivateClickMeasurement"_s;
    const auto attributedTableName = "AttributedPrivateClickMeasurement"_s;

    addMissingColumnsToTable(unattributedTableName, columnsForTable(unattributedTableName), expectedUnattributedColumns());
    addMissingColumnsToTable(attributedTableName, columnsForTable(attributedTableName), expectedAttributedColumns());
}

void ResourceLoadStatisticsDatabaseStore::renameColumnInTable(String&& tableName, ExistingColumnName&& existingColumnName, ExpectedColumnName&& expectedColumnName)
{
    auto statement = m_database.prepareStatementSlow(makeString("ALTER TABLE ", tableName, " RENAME COLUMN ", existingColumnName, " TO ", expectedColumnName));
    if (!statement) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingColumnsToTable Unable to prepare statement to rename column in table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    if (statement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingColumnsToTable error executing statement to rename column in table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::renameColumnsIfNecessary()
{
    // Attributed Private Click Measurement case.
    const auto attributedTableName = "AttributedPrivateClickMeasurement"_s;
    const auto oldEarliestTimeToSendColumn = "earliestTimeToSend"_s;
    const auto newEarliestTimeToSendColumn = "earliestTimeToSendToSource"_s;
    const auto oldAttributionDestinationColumn = "attributeOnSiteDomainID"_s;
    const auto newAttributionDestinationColumn = "destinationSiteDomainID"_s;
    const auto foundAttributedTableColumns = columnsForTable(attributedTableName);

    if (foundAttributedTableColumns.contains(oldEarliestTimeToSendColumn))
        renameColumnInTable(attributedTableName, oldEarliestTimeToSendColumn, newEarliestTimeToSendColumn);

    if (foundAttributedTableColumns.contains(oldAttributionDestinationColumn))
        renameColumnInTable(attributedTableName, oldAttributionDestinationColumn, newAttributionDestinationColumn);

    // Unattributed Private Click Measurement case.
    auto unattributedTableName = "UnattributedPrivateClickMeasurement"_s;
    auto foundUnattributedTableColumns = columnsForTable(unattributedTableName);

    if (foundUnattributedTableColumns.contains(oldAttributionDestinationColumn))
        renameColumnInTable(unattributedTableName, oldAttributionDestinationColumn, newAttributionDestinationColumn);
}

void ResourceLoadStatisticsDatabaseStore::addMissingTablesIfNecessary()
{
    auto missingTables = checkForMissingTablesInSchema();
    if (!missingTables)
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();
    for (auto& table : *missingTables) {
        auto createTableQuery = expectedTableAndIndexQueries().get(table).first;
        if (!m_database.executeCommandSlow(createTableQuery))
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingTables failed to execute, error message: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg());
    }

    if (!createUniqueIndices()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::addMissingTables failed to create unique indices, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::openAndUpdateSchemaIfNecessary()
{
    openITPDatabase();
    addMissingTablesIfNecessary();

    String currentSchema;
    {
        // Fetch the schema for an existing Observed Domains table.
        auto statement = m_database.prepareStatement("SELECT type, sql FROM sqlite_master WHERE tbl_name='ObservedDomains'"_s);
        if (!statement) {
            LOG_ERROR("Unable to prepare statement to fetch schema for the ObservedDomains table.");
            ASSERT_NOT_REACHED();
            return;
        }

        // If there is no ObservedDomains table at all, or there is an error executing the fetch, delete the file.
        if (statement->step() != SQLITE_ROW) {
            LOG_ERROR("Error executing statement to fetch schema for the Observed Domains table.");
            close();
            FileSystem::deleteFile(m_storageDirectoryPath);
            openITPDatabase();
            return;
        }

        currentSchema = statement->columnText(1);
    }

    ASSERT(!currentSchema.isEmpty());

    // If the ObservedDomains schema in the ResourceLoadStatistics directory is not the current schema, delete the database file.
    // FIXME: Migrate old ObservedDomains data to new table schema.
    if (currentSchema != ObservedDomainsTableSchemaV1() && currentSchema != ObservedDomainsTableSchemaV1Alternate()) {
        close();
        FileSystem::deleteFile(m_storageDirectoryPath);
        openITPDatabase();
        return;
    }

    // Renaming and adding columns should be done before migrating to avoid mismatched or missing columns.
    renameColumnsIfNecessary();
    addMissingColumnsIfNecessary();
    migrateDataToNewTablesIfNecessary();
}

SQLiteStatementAutoResetScope ResourceLoadStatisticsDatabaseStore::scopedStatement(std::unique_ptr<WebCore::SQLiteStatement>& statement, ASCIILiteral query, const String& logString) const
{
    if (!statement) {
        auto statementOrError = m_database.prepareHeapStatement(query);
        if (!statementOrError) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::%s failed to prepare statement, error message: %" PUBLIC_LOG_STRING, this, logString.ascii().data(), m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return SQLiteStatementAutoResetScope { };
        }
        statement = statementOrError.value().moveToUniquePtr();
        ASSERT(m_database.isOpen());
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::isEmpty failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return !scopedStatement->columnInt(0);
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
        || !m_database.executeCommand(createUniqueIndexSubresourceUniqueRedirectsFrom)
        || !m_database.executeCommand(createUniqueIndexSubresourceUnderTopFrameDomains)
        || !m_database.executeCommand(createUniqueIndexOperatingDates)
        || !m_database.executeCommand(createUniqueIndexUnattributedPrivateClickMeasurement)
        || !m_database.executeCommand(createUniqueIndexAttributedPrivateClickMeasurement)) {
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

    if (!m_database.executeCommand(createUnattributedPrivateClickMeasurement)) {
        LOG_ERROR("Could not create UnattributedPrivateClickMeasurement table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
        return false;
    }

    if (!m_database.executeCommand(createAttributedPrivateClickMeasurement)) {
        LOG_ERROR("Could not create AttributedPrivateClickMeasurement table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
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
    m_insertUnattributedPrivateClickMeasurementStatement = nullptr;
    m_insertAttributedPrivateClickMeasurementStatement = nullptr;
    m_setUnattributedPrivateClickMeasurementAsExpiredStatement = nullptr;
    m_clearUnattributedPrivateClickMeasurementStatement = nullptr;
    m_clearAttributedPrivateClickMeasurementStatement = nullptr;
    m_clearExpiredPrivateClickMeasurementStatement = nullptr;
    m_allUnattributedPrivateClickMeasurementAttributionsStatement = nullptr;
    m_allAttributedPrivateClickMeasurementStatement = nullptr;
    m_findUnattributedStatement = nullptr;
    m_findAttributedStatement = nullptr;
    m_updateAttributionsEarliestTimeToSendStatement = nullptr;
    m_removeUnattributedStatement = nullptr;
    m_markReportAsSentToSourceStatement = nullptr;
    m_markReportAsSentToDestinationStatement = nullptr;
    m_earliestTimesToSendStatement = nullptr;
}

bool ResourceLoadStatisticsDatabaseStore::insertObservedDomain(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    if (domainID(loadStatistics.registrableDomain)) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "ResourceLoadStatisticsDatabaseStore::insertObservedDomain can only be called on domains not in the database.");
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to commit, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool ResourceLoadStatisticsDatabaseStore::relationshipExists(SQLiteStatementAutoResetScope& statement, std::optional<unsigned> firstDomainID, const RegistrableDomain& secondDomain) const
{
    if (!firstDomainID)
        return false;
    
    ASSERT(!RunLoop::isMain());

    if (!statement
        || statement->bindInt(1, *firstDomainID) != SQLITE_OK
        || statement->bindText(2, secondDomain.string()) != SQLITE_OK
        || statement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::relationshipExists failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    return !!statement->columnInt(0);
}

std::optional<unsigned> ResourceLoadStatisticsDatabaseStore::domainID(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "domainID"_s);
    if (!scopedStatement || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDFromString failed. Error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    
    if (scopedStatement->step() != SQLITE_ROW)
        return std::nullopt;

    return scopedStatement->columnInt(0);
}

String ResourceLoadStatisticsDatabaseStore::ensureAndMakeDomainList(const SQLiteTransaction&, const HashSet<RegistrableDomain>& domainList)
{
    ASSERT(m_database.transactionInProgress());

    StringBuilder builder;
    for (auto& topFrameResource : domainList) {
        // Insert query will fail if top frame domain is not already in the database
        if (ensureResourceStatisticsForRegistrableDomain(topFrameResource).second)
            builder.append(builder.isEmpty() ? "" : ", ", '"', topFrameResource.string(), '"');
    }
    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList(const SQLiteTransaction& transaction, const String& statement, const HashSet<RegistrableDomain>& domainList, unsigned domainID)
{
    auto insertRelationshipStatement = m_database.prepareStatementSlow(makeString(statement, ensureAndMakeDomainList(transaction, domainList), " );"));
    
    if (!insertRelationshipStatement
        || insertRelationshipStatement->bindInt(1, domainID) != SQLITE_OK) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
    }
    
    if (statement.contains("REPLACE")) {
        if (insertRelationshipStatement->bindDouble(2, WallTime::now().secondsSinceEpoch().value()) != SQLITE_OK) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    if (insertRelationshipStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertDomainRelationshipList failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::insertDomainRelationships(const SQLiteTransaction& transaction, const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    auto registrableDomainID = domainID(loadStatistics.registrableDomain);
    
    if (!registrableDomainID)
        return;

    insertDomainRelationshipList(transaction, storageAccessUnderTopFrameDomainsQuery, loadStatistics.storageAccessUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToQuery, loadStatistics.topFrameUniqueRedirectsTo, registrableDomainID.value());
    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, loadStatistics.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement, registrableDomainID.value());
    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsFromQuery, loadStatistics.topFrameUniqueRedirectsFrom, registrableDomainID.value());
    insertDomainRelationshipList(transaction, subframeUnderTopFrameDomainsQuery, loadStatistics.subframeUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(transaction, subresourceUnderTopFrameDomainsQuery, loadStatistics.subresourceUnderTopFrameDomains, registrableDomainID.value());
    insertDomainRelationshipList(transaction, subresourceUniqueRedirectsToQuery, loadStatistics.subresourceUniqueRedirectsTo, registrableDomainID.value());
    insertDomainRelationshipList(transaction, subresourceUniqueRedirectsFromQuery, loadStatistics.subresourceUniqueRedirectsFrom, registrableDomainID.value());
    insertDomainRelationshipList(transaction, topFrameLinkDecorationsFromQuery, loadStatistics.topFrameLinkDecorationsFrom, registrableDomainID.value());
    insertDomainRelationshipList(transaction, topFrameLoadedThirdPartyScriptsQuery, loadStatistics.topFrameLoadedThirdPartyScripts, registrableDomainID.value());
}

void ResourceLoadStatisticsDatabaseStore::populateFromMemoryStore(const ResourceLoadStatisticsMemoryStore& memoryStore)
{
    ASSERT(!RunLoop::isMain());

    if (!isEmpty())
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto& statisticsMap = memoryStore.data();
    for (const auto& statistic : statisticsMap.values()) {
        auto result = insertObservedDomain(statistic);
        if (!result) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::populateFromMemoryStore insertObservedDomain failed to complete, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries
    for (auto& statistic : statisticsMap.values())
        insertDomainRelationships(transaction, statistic);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::merge(const SQLiteTransaction& transaction, WebCore::SQLiteStatement* current, const ResourceLoadStatistics& other)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    auto currentRegistrableDomain = current->columnText(RegistrableDomainIndex);
    auto currentLastSeen = current->columnDouble(LastSeenIndex);
    auto currentMostRecentUserInteraction = current->columnDouble(MostRecentUserInteractionTimeIndex);
    bool currentGrandfathered = current->columnInt(GrandfatheredIndex);
    bool currentIsPrevalent = current->columnInt(IsPrevalentIndex);
    bool currentIsVeryPrevalent = current->columnInt(IsVeryPrevalentIndex);
    unsigned currentDataRecordsRemoved = current->columnInt(DataRecordsRemovedIndex);
    bool currentIsScheduledForAllButCookieDataRemoval = current->columnInt(IsScheduledForAllButCookieDataRemovalIndex);

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
        setGrandfathered(transaction, other.registrableDomain, true);
    if (other.isPrevalentResource && !currentIsPrevalent)
        setPrevalentResource(transaction, other.registrableDomain);
    if (other.isVeryPrevalentResource && !currentIsVeryPrevalent)
        setVeryPrevalentResource(transaction, other.registrableDomain);
    if (other.dataRecordsRemoved > currentDataRecordsRemoved)
        updateDataRecordsRemoved(other.registrableDomain, other.dataRecordsRemoved);
    if (other.gotLinkDecorationFromPrevalentResource && !currentIsScheduledForAllButCookieDataRemoval)
        setIsScheduledForAllButCookieDataRemoval(transaction, other.registrableDomain, true);
}

void ResourceLoadStatisticsDatabaseStore::mergeStatistic(const SQLiteTransaction& transaction, const ResourceLoadStatistics& statistic)
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_getResourceDataByDomainNameStatement, getResourceDataByDomainNameQuery, "mergeStatistic"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, statistic.registrableDomain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::mergeStatistic. Statement failed to bind or domain was not found, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    merge(transaction, scopedStatement.get(), statistic);
}

void ResourceLoadStatisticsDatabaseStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());
    if (statistics.isEmpty())
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (auto& statistic : statistics) {
        if (!domainID(statistic.registrableDomain)) {
            auto result = insertObservedDomain(statistic);
            if (!result) {
                ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::mergeStatistics insertObservedDomain failed to complete, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
                ASSERT_NOT_REACHED();
                return;
            }
        } else
            mergeStatistic(transaction, statistic);
    }

    // Make a separate pass for inter-domain relationships so we
    // can refer to the ObservedDomain table entries.
    for (auto& statistic : statistics)
        insertDomainRelationships(transaction, statistic);

    transaction.commit();
}

static ASCIILiteral joinSubStatisticsForSorting()
{
    return "SELECT domainID,"
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
        "GROUP BY domainID) ORDER BY sum DESC;"_s;
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
        RegistrableDomain firstPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(m_getAllSubStatisticsStatement->columnInt(0)));
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
    auto sortedStatistics = m_database.prepareStatement(joinSubStatisticsForSorting());
    if (!sortedStatistics
        || sortedStatistics->bindText(1, prevalentDomainsBindParameter)
        || sortedStatistics->bindText(2, "%") != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::aggregatedThirdPartyData, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return thirdPartyDataList;
    }
    while (sortedStatistics->step() == SQLITE_ROW) {
        if (hasBeenThirdParty(sortedStatistics->columnInt(1))) {
            auto thirdPartyDomainID = sortedStatistics->columnInt(0);
            auto thirdPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(thirdPartyDomainID));
            thirdPartyDataList.append(WebResourceLoadStatisticsStore::ThirdPartyData { thirdPartyDomain, getThirdPartyDataForSpecificFirstPartyDomains(thirdPartyDomainID, thirdPartyDomain) });
        }
    }
    return thirdPartyDataList;
}

static String domainsToString(const HashSet<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (const auto& domainName : domains)
        builder.append(builder.isEmpty() ? "" : ", ", '"', domainName.string(), '"');
    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&& domains)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET dataRecordsRemoved = dataRecordsRemoved + 1 WHERE registrableDomain IN (", domainsToString(domains), ")"));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::incrementStatisticsForDomains failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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
    auto findSubresources = m_database.prepareStatement("SELECT SubresourceUniqueRedirectsFrom.fromDomainID from SubresourceUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsFrom.fromDomainID WHERE subresourceDomainID = ? AND ObservedDomains.isPrevalent = 0"_s);
    if (!findSubresources || findSubresources->bindInt(1, primaryDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    while (findSubresources->step() == SQLITE_ROW) {
        int newDomainID = findSubresources->columnInt(0);
        auto insertResult = nonPrevalentRedirectionSources.insert(newDomainID);
        if (insertResult.second)
            newlyIdentifiedDomains.insert(newDomainID);
    }

    auto findTopFrames = m_database.prepareStatement("SELECT TopFrameUniqueRedirectsFrom.fromDomainID from TopFrameUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = TopFrameUniqueRedirectsFrom.fromDomainID WHERE targetDomainID = ? AND ObservedDomains.isPrevalent = 0"_s);
    if (!findTopFrames
        || findTopFrames->bindInt(1, primaryDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    while (findTopFrames->step() == SQLITE_ROW) {
        int newDomainID = findTopFrames->columnInt(0);
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
    for (auto domainID : values)
        builder.append(builder.isEmpty() ? "" : ", ", domainID);
    return builder.toString();
}

void ResourceLoadStatisticsDatabaseStore::markAsPrevalentIfHasRedirectedToPrevalent()
{
    ASSERT(!RunLoop::isMain());

    StdSet<unsigned> prevalentDueToRedirect;
    auto subresourceRedirectStatement = m_database.prepareStatement("SELECT DISTINCT SubresourceUniqueRedirectsTo.subresourceDomainID FROM SubresourceUniqueRedirectsTo JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsTo.toDomainID AND ObservedDomains.isPrevalent = 1"_s);
    if (subresourceRedirectStatement) {
        while (subresourceRedirectStatement->step() == SQLITE_ROW)
            prevalentDueToRedirect.insert(subresourceRedirectStatement->columnInt(0));
    }

    auto topFrameRedirectStatement = m_database.prepareStatement("SELECT DISTINCT TopFrameUniqueRedirectsTo.sourceDomainID FROM TopFrameUniqueRedirectsTo JOIN ObservedDomains ON ObservedDomains.domainID = TopFrameUniqueRedirectsTo.toDomainID AND ObservedDomains.isPrevalent = 1"_s);
    if (topFrameRedirectStatement) {
        while (topFrameRedirectStatement->step() == SQLITE_ROW)
            prevalentDueToRedirect.insert(topFrameRedirectStatement->columnInt(0));
    }

    auto markPrevalentStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN (", buildList(WTF::IteratorRange<StdSet<unsigned>::iterator>(prevalentDueToRedirect.begin(), prevalentDueToRedirect.end())), ")"));
    if (!markPrevalentStatement || markPrevalentStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::markAsPrevalentIfHasRedirectedToPrevalent failed to execute, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

HashMap<unsigned, ResourceLoadStatisticsDatabaseStore::NotVeryPrevalentResources> ResourceLoadStatisticsDatabaseStore::findNotVeryPrevalentResources()
{
    ASSERT(!RunLoop::isMain());

    HashMap<unsigned, NotVeryPrevalentResources> results;
    auto notVeryPrevalentResourcesStatement = m_database.prepareStatement("SELECT domainID, registrableDomain, isPrevalent FROM ObservedDomains WHERE isVeryPrevalent = 0"_s);
    if (notVeryPrevalentResourcesStatement) {
        while (notVeryPrevalentResourcesStatement->step() == SQLITE_ROW) {
            unsigned key = static_cast<unsigned>(notVeryPrevalentResourcesStatement->columnInt(0));
            NotVeryPrevalentResources value({ RegistrableDomain::uncheckedCreateFromRegistrableDomainString(notVeryPrevalentResourcesStatement->columnText(1))
                , notVeryPrevalentResourcesStatement->columnInt(2) ? ResourceLoadPrevalence::High : ResourceLoadPrevalence::Low
                , 0, 0, 0, 0 });
            results.add(key, value);
        }
    }

    StringBuilder builder;
    for (auto value : results.keys())
        builder.append(builder.isEmpty() ? "" : ", ", value);
    auto domainIDsOfInterest = builder.toString();

    auto subresourceUnderTopFrameDomainsStatement = m_database.prepareStatementSlow(makeString("SELECT subresourceDomainID, COUNT(topFrameDomainID) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID IN (", domainIDsOfInterest, ") GROUP BY subresourceDomainID"));
    if (subresourceUnderTopFrameDomainsStatement) {
        while (subresourceUnderTopFrameDomainsStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement->columnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUnderTopFrameDomainsCount = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement->columnInt(1));
        }
    }

    auto subresourceUniqueRedirectsToCountStatement = m_database.prepareStatementSlow(makeString("SELECT subresourceDomainID, COUNT(toDomainID) FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID IN (", domainIDsOfInterest, ") GROUP BY subresourceDomainID"));
    if (subresourceUniqueRedirectsToCountStatement) {
        while (subresourceUniqueRedirectsToCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement->columnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUniqueRedirectsToCount = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement->columnInt(1));
        }
    }

    auto subframeUnderTopFrameDomainsCountStatement = m_database.prepareStatementSlow(makeString("SELECT subframeDomainID, COUNT(topFrameDomainID) FROM SubframeUnderTopFrameDomains WHERE subframeDomainID IN (", domainIDsOfInterest, ") GROUP BY subframeDomainID"));
    if (subframeUnderTopFrameDomainsCountStatement) {
        while (subframeUnderTopFrameDomainsCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement->columnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subframeUnderTopFrameDomainsCount = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement->columnInt(1));
        }
    }

    auto topFrameUniqueRedirectsToCountStatement = m_database.prepareStatementSlow(makeString("SELECT sourceDomainID, COUNT(toDomainID) FROM TopFrameUniqueRedirectsTo WHERE sourceDomainID IN (", domainIDsOfInterest, ") GROUP BY sourceDomainID"));
    if (topFrameUniqueRedirectsToCountStatement) {
        while (topFrameUniqueRedirectsToCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement->columnInt(0));
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.topFrameUniqueRedirectsToCount = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement->columnInt(1));
        }
    }

    return results;
}

void ResourceLoadStatisticsDatabaseStore::reclassifyResources()
{
    ASSERT(!RunLoop::isMain());

    auto notVeryPrevalentResources = findNotVeryPrevalentResources();
    if (notVeryPrevalentResources.isEmpty())
        return;

    SQLiteTransaction transaction(m_database);
    for (auto& resourceStatistic : notVeryPrevalentResources.values()) {
        if (shouldSkip(resourceStatistic.registrableDomain))
            continue;

        auto newPrevalence = classifier().calculateResourcePrevalence(resourceStatistic.subresourceUnderTopFrameDomainsCount, resourceStatistic.subresourceUniqueRedirectsToCount, resourceStatistic.subframeUnderTopFrameDomainsCount, resourceStatistic.topFrameUniqueRedirectsToCount, resourceStatistic.prevalence);
        if (newPrevalence != resourceStatistic.prevalence) {
            if (!transaction.inProgress())
                transaction.begin();
            setPrevalentResource(transaction, resourceStatistic.registrableDomain, newPrevalence);
        }
    }
    transaction.commit();
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

void ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::hasStorageAccess was not completed due to failed insert attempt", this);
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess was not completed due to failed insert attempt", this);
        return;
    }
    
    switch (cookieAccess(subFrameDomain, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Cannot grant storage access to %" PRIVATE_LOG_STRING " since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Error, makeString("[ITP] Cannot grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are blocked in third-party contexts and it has not received user interaction as first-party."_s));
        }
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "No need to grant storage access to %" PRIVATE_LOG_STRING " since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies.", subFrameDomain.string().utf8().data());
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
            RELEASE_LOG_INFO(ITPDebug, "About to ask the user whether they want to grant storage access to %" PRIVATE_LOG_STRING " under %" PRIVATE_LOG_STRING " or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to ask the user whether they want to grant storage access to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "' or not."_s));
        }
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes) {
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Storage access was granted to %" PRIVATE_LOG_STRING " under %" PRIVATE_LOG_STRING ".", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "'."_s));
        }
    }

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto incrementStorageAccess = m_database.prepareStatement("UPDATE ObservedDomains SET timesAccessedAsFirstPartyDueToStorageAccessAPI = timesAccessedAsFirstPartyDueToStorageAccessAPI + 1 WHERE domainID = ?"_s);
    if (!incrementStorageAccess
        || incrementStorageAccess->bindInt(1, *subFrameStatus.second) != SQLITE_OK
        || incrementStorageAccess->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    grantStorageAccessInternal(transaction, WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, scope, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    if (UNLIKELY(debugLoggingEnabled())) {
        RELEASE_LOG_INFO(ITPDebug, "[Temporary combatibility fix] Storage access was granted for %" PRIVATE_LOG_STRING " under opener page from %" PRIVATE_LOG_STRING ", with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted for '"_s, domainInNeedOfStorageAccess.string(), "' under opener page from '"_s, openerDomain.string(), "', with user interaction in the opened window."_s));
    }

    SQLiteTransaction transaction(m_database);
    transaction.begin();
    grantStorageAccessInternal(transaction, WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), std::nullopt, openerPageID, StorageAccessPromptWasShown::No, StorageAccessScope::PerPage, [](StorageAccessWasGranted) { });
    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    if (promptWasShown == StorageAccessPromptWasShown::Yes) {
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        if (!subFrameStatus.second) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grantStorageAccess was not completed due to failed insert attempt", this);
            return;
        }
        ASSERT(subFrameStatus.first == AddedRecord::No);
#if ASSERT_ENABLED
        if (!NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain))
            ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
#endif
        insertDomainRelationshipList(transaction, storageAccessUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *subFrameStatus.second);
    }

    grantStorageAccessInternal(transaction, WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, WTFMove(completionHandler));

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal(const SQLiteTransaction& transaction, SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessWasGranted::Yes);
        return;
    }

    if (promptWasShownNowOrEarlier == StorageAccessPromptWasShown::Yes) {
#ifndef NDEBUG
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        if (!subFrameStatus.second) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal was not completed due to failed insert attempt", this);
            return;
        }
        ASSERT(subFrameStatus.first == AddedRecord::No);
#if ASSERT_ENABLED
        if (!NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain))
            ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
#endif
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
    if (domains.isEmpty())
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();
    for (auto& registrableDomain : domains) {
        auto result = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
        if (!result.second)
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains was not completed due to failed insert attempt", this);
    }

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET grandfathered = 1 WHERE registrableDomain IN (", domainsToString(domains), ")"));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
    transaction.commit();
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode()
{
    ASSERT(!RunLoop::isMain());

    if (!debugModeEnabled())
        return { };

    Vector<RegistrableDomain> primaryDomainsToBlock;
    primaryDomainsToBlock.reserveInitialCapacity(2);

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(debugStaticPrevalentResource());
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode was not completed due to failed insert attempt for debugStaticPrevalentResource", this);
        return { };
    }

    setPrevalentResource(transaction, debugStaticPrevalentResource(), ResourceLoadPrevalence::High);
    primaryDomainsToBlock.uncheckedAppend(debugStaticPrevalentResource());

    if (!debugManualPrevalentResource().isEmpty()) {
        auto result = ensureResourceStatisticsForRegistrableDomain(debugManualPrevalentResource());
        if (!result.second) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensurePrevalentResourcesForDebugMode was not completed due to failed insert attempt for debugManualPrevalentResource", this);
            return { };
        }
        setPrevalentResource(transaction, debugManualPrevalentResource(), ResourceLoadPrevalence::High);
        primaryDomainsToBlock.uncheckedAppend(debugManualPrevalentResource());

        if (debugLoggingEnabled()) {
            RELEASE_LOG_INFO(ITPDebug, "Did set %" PRIVATE_LOG_STRING " as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Did set '"_s, debugManualPrevalentResource().string(), "' as prevalent resource for the purposes of ITP Debug Mode."_s));
        }
    }

    transaction.commit();

    return primaryDomainsToBlock;
}

void ResourceLoadStatisticsDatabaseStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndTopFrameDomainsSameSite = targetDomain == topFrameDomain;
    bool areTargetAndSourceDomainsSameSite = targetDomain == sourceDomain;

    SQLiteTransaction transaction(m_database);

    bool statisticsWereUpdated = false;
    if (!isMainFrame && !(areTargetAndTopFrameDomainsSameSite || areTargetAndSourceDomainsSameSite)) {
        if (!transaction.inProgress())
            transaction.begin();

        auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        if (!targetResult.second) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target domain", this);
            return;
        }
        updateLastSeen(targetDomain, ResourceLoadStatistics::reduceTimeResolution(WallTime::now()));
        insertDomainRelationshipList(transaction, subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *targetResult.second);
        statisticsWereUpdated = true;
    }

    if (!areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            bool wasNavigatedAfterShortDelayWithoutUserInteraction = !wasPotentiallyInitiatedByUser && delayAfterMainFrameDocumentLoad < parameters().minDelayAfterMainFrameDocumentLoadToNotBeARedirect;
            if (isRedirect || wasNavigatedAfterShortDelayWithoutUserInteraction) {
                if (!transaction.inProgress())
                    transaction.begin();

                auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
                auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
                if (!targetResult.second || !redirectingDomainResult.second) {
                    ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target or redirecting domain (isMainFrame)", this);
                    return;
                }
                insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
                if (isRedirect) {
                    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);

                    if (UNLIKELY(debugLoggingEnabled())) {
                        RELEASE_LOG_INFO(ITPDebug, "Did set %" PUBLIC_LOG_STRING " as making a top frame redirect to %" PUBLIC_LOG_STRING ".", sourceDomain.string().utf8().data(), targetDomain.string().utf8().data());
                        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Did set '", sourceDomain.string(), "' as making a top frame redirect to '", targetDomain.string(), "'."));
                    }
                }
                insertDomainRelationshipList(transaction, topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
                statisticsWereUpdated = true;
            }
        } else if (isRedirect) {
            if (!transaction.inProgress())
                transaction.begin();

            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (!targetResult.second || !redirectingDomainResult.second) {
                ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logFrameNavigation was not completed due to failed insert attempt of target or redirecting domain (isRedirect)", this);
                return;
            }
            insertDomainRelationshipList(transaction, subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
            insertDomainRelationshipList(transaction, subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
            statisticsWereUpdated = true;
        }
    }
    transaction.commit();

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsDatabaseStore::logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain& fromDomain, const NavigatedToDomain& toDomain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(fromDomain != toDomain);

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto toDomainResult = ensureResourceStatisticsForRegistrableDomain(toDomain);
    if (!toDomainResult.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logCrossSiteLoadWithLinkDecoration was not completed due to failed insert attempt", this);
        return;
    }
    insertDomainRelationshipList(transaction, topFrameLinkDecorationsFromQuery, HashSet<RegistrableDomain>({ fromDomain }), *toDomainResult.second);
    
    if (isPrevalentResource(fromDomain))
        setIsScheduledForAllButCookieDataRemoval(transaction, toDomain, true);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!targetResult.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement was not completed due to failed insert attempt", this);
        completionHandler();
        return;
    }

    auto removeRedirectsToSinceSameSite = m_database.prepareStatement("DELETE FROM TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement WHERE sourceDomainID = ?"_s);
    if (!removeRedirectsToSinceSameSite
        || removeRedirectsToSinceSameSite->bindInt(1, *targetResult.second) != SQLITE_OK
        || removeRedirectsToSinceSameSite->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    transaction.commit();
    
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setUserInteraction, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::logUserInteraction(const TopFrameDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::logUserInteraction was not completed due to failed insert attempt", this);
        return;
    }
    bool didHavePreviousUserInteraction = hasHadUserInteraction(domain, OperatingDatesWindow::Long);
    setUserInteraction(domain, true, WallTime::now());

    transaction.commit();

    if (didHavePreviousUserInteraction) {
        completionHandler();
        return;
    }
    updateCookieBlocking(WTFMove(completionHandler));
}

void ResourceLoadStatisticsDatabaseStore::clearUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!targetResult.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearUserInteraction was not completed due to failed insert attempt", this);
        return;
    }
    setUserInteraction(domain, false, { });

    auto removeStorageAccess = m_database.prepareStatement("DELETE FROM StorageAccessUnderTopFrameDomains WHERE domainID = ?"_s);
    if (!removeStorageAccess
        || removeStorageAccess->bindInt(1, *targetResult.second) != SQLITE_OK
        || removeStorageAccess->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearUserInteraction failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    transaction.commit();

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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_hadUserInteractionStatement failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return false;
    }

    bool hadUserInteraction = !!scopedStatement->columnInt(0);
    if (!hadUserInteraction)
        return false;
    
    WallTime mostRecentUserInteractionTime = WallTime::fromRawSeconds(scopedStatement->columnDouble(1));

    if (hasStatisticsExpired(mostRecentUserInteractionTime, operatingDatesWindow)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        clearUserInteraction(domain, [] { });
        hadUserInteraction = false;
    }
    return hadUserInteraction;
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const SQLiteTransaction&, const RegistrableDomain& domain, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    if (shouldSkip(domain))
        return;

    auto registrableDomainID = domainID(domain);
    if (!registrableDomainID) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setPrevalentResource domainID should exist but was not found.", this);
        ASSERT_NOT_REACHED();
        return;
    }

    auto scopedUpdatePrevalentResourceStatement = this->scopedStatement(m_updatePrevalentResourceStatement, updatePrevalentResourceQuery, "setPrevalentResource"_s);
    if (!scopedUpdatePrevalentResourceStatement
        || scopedUpdatePrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updatePrevalentResourceStatement failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    auto scopedUpdateVeryPrevalentResourceStatement = this->scopedStatement(m_updateVeryPrevalentResourceStatement, updateVeryPrevalentResourceQuery, "setPrevalentResource updateVeryPrevalentResource"_s);
    if (newPrevalence == ResourceLoadPrevalence::VeryHigh) {
        if (!scopedUpdateVeryPrevalentResourceStatement
            || scopedUpdateVeryPrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::m_updateVeryPrevalentResourceStatement failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
    }

    StdSet<unsigned> nonPrevalentRedirectionSources;
    recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(*registrableDomainID, nonPrevalentRedirectionSources, 0);
    setDomainsAsPrevalent(WTFMove(nonPrevalentRedirectionSources));
}

void ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent(StdSet<unsigned>&& domains)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN (", buildList(WTF::IteratorRange<StdSet<unsigned>::iterator>(domains.begin(), domains.end())), ")"));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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
        domains.append(scopedStatement->columnText(0));
    std::sort(domains.begin(), domains.end(), WTF::codePointCompareLessThan);

    StringBuilder result;
    result.append("Resource load statistics:\n\n");
    for (auto& domain : domains)
        resourceToString(result, domain);

    auto thirdPartyData = aggregatedThirdPartyData();
    if (!thirdPartyData.isEmpty()) {
        result.append("\nITP Data:\n");
        for (auto thirdParty : thirdPartyData)
            result.append(thirdParty.toString(), '\n');
    }
    completionHandler(result.toString());
}

bool ResourceLoadStatisticsDatabaseStore::predicateValueForDomain(SQLiteStatementAutoResetScope& predicateStatement, const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (!predicateStatement
        || predicateStatement->bindText(1, domain.string()) != SQLITE_OK
        || predicateStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::predicateValueForDomain failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return false;
    }
    return !!predicateStatement->columnInt(0);
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

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_clearPrevalentResourceStatement, clearPrevalentResourceQuery, "clearPrevalentResource"_s);
    
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setGrandfathered(const RegistrableDomain& domain, bool value)
{
    SQLiteTransaction transaction(m_database);
    transaction.begin();

    setGrandfathered(transaction, domain, value);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setGrandfathered(const WebCore::SQLiteTransaction&, const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_updateGrandfatheredStatement, updateGrandfatheredQuery, "setGrandfathered"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval(const SQLiteTransaction&, const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval was not completed due to failed insert attempt", this);
        return;
    }
    
    auto scopedStatement = this->scopedStatement(m_updateIsScheduledForAllButCookieDataRemovalStatement, updateIsScheduledForAllButCookieDataRemovalQuery, "setIsScheduledForAllButCookieDataRemoval"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setIsScheduledForAllButCookieDataRemoval failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Seconds ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp(const RegistrableDomain& subDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    std::optional<unsigned> subFrameDomainID = domainID(subDomain);
    std::optional<unsigned> topFrameDomainID = domainID(topFrameDomain);

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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getMostRecentlyUpdatedTimestamp failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };
    }
    if (scopedStatement->step() != SQLITE_ROW)
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };

    return Seconds { scopedStatement->columnDouble(0) };
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

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubframeUnderTopFrameDomain was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(transaction, subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(transaction, subresourceUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(transaction, subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(transaction, subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo was not completed due to failed insert attempt", this);
        return;
    }

    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom was not completed due to failed insert attempt", this);
        return;
    }
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(transaction, topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);

    transaction.commit();
}

std::pair<ResourceLoadStatisticsDatabaseStore::AddedRecord, std::optional<unsigned>> ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    {
        auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "ensureResourceStatisticsForRegistrableDomain"_s);
        if (!scopedStatement
            || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return { AddedRecord::No, 0 };
        }

        if (scopedStatement->step() == SQLITE_ROW) {
            unsigned domainID = scopedStatement->columnInt(0);
            return { AddedRecord::No, domainID };
        }
    }

    ResourceLoadStatistics newObservation(domain);
    auto result = insertObservedDomain(newObservation);

    if (!result) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain insertObservedDomain failed to complete, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { AddedRecord::No, std::nullopt };
    }

    return { AddedRecord::Yes, domainID(domain).value() };
}

void ResourceLoadStatisticsDatabaseStore::clearDatabaseContents()
{
    m_database.clearAllTables();

    if (!createSchema()) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::clearDatabaseContents failed, error message: %" PRIVATE_LOG_STRING ", database path: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::removeDataForDomain failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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
        domains.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(scopedStatement->columnText(0)));
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
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { }, { }, { } };
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

    auto statement = m_database.prepareStatement("SELECT isPrevalent, hadUserInteraction FROM ObservedDomains WHERE registrableDomain = ?"_s);
    if (!statement || statement->bindText(1, subresourceDomain.string()) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::cookieAccess failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    bool hasNoEntry = statement->step() != SQLITE_ROW;
    bool isPrevalent = !hasNoEntry && !!statement->columnInt(0);
    bool hadUserInteraction = !hasNoEntry && statement->columnInt(1) ? true : false;

    if (!areAllThirdPartyCookiesBlockedUnder(topFrameDomain) && !isPrevalent)
        return CookieAccess::BasedOnCookiePolicy;

    if (!NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subresourceDomain, topFrameDomain) && !hadUserInteraction)
        return CookieAccess::CannotRequest;

    return CookieAccess::OnlyIfGranted;
}

StorageAccessPromptWasShown ResourceLoadStatisticsDatabaseStore::hasUserGrantedStorageAccessThroughPrompt(unsigned requestingDomainID, const RegistrableDomain& firstPartyDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(firstPartyDomain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::hasUserGrantedStorageAccessThroughPrompt was not completed due to failed insert attempt", this);
        return StorageAccessPromptWasShown::No;
    }

    auto firstPartyPrimaryDomainID = *result.second;

    auto statement = m_database.prepareStatement("SELECT COUNT(*) FROM StorageAccessUnderTopFrameDomains WHERE domainID = ? AND topLevelDomainID = ?"_s);
    if (!statement
        || statement->bindInt(1, requestingDomainID) != SQLITE_OK
        || statement->bindInt(2, firstPartyPrimaryDomainID) != SQLITE_OK
        || statement->step() != SQLITE_ROW)
        return StorageAccessPromptWasShown::No;

    return !!statement->columnInt(0) ? StorageAccessPromptWasShown::Yes : StorageAccessPromptWasShown::No;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsToBlockAndDeleteCookiesFor() const
{
    ASSERT(!RunLoop::isMain());
    
    Vector<RegistrableDomain> results;
    auto statement = m_database.prepareStatement("SELECT registrableDomain FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 0"_s);
    if (!statement)
        return results;
    
    while (statement->step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(0)));
    
    return results;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsToBlockButKeepCookiesFor() const
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> results;
    auto statement = m_database.prepareStatement("SELECT registrableDomain FROM ObservedDomains WHERE isPrevalent = 1 AND hadUserInteraction = 1"_s);
    if (!statement)
        return results;
    
    while (statement->step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(0)));

    return results;
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsWithUserInteractionAsFirstParty() const
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> results;
    auto statement = m_database.prepareStatement("SELECT registrableDomain FROM ObservedDomains WHERE hadUserInteraction = 1"_s);
    if (!statement)
        return results;
    
    while (statement->step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(0)));

    return results;
}

HashMap<TopFrameDomain, SubResourceDomain> ResourceLoadStatisticsDatabaseStore::domainsWithStorageAccess() const
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::RegistrableDomain, WebCore::RegistrableDomain> results;
    auto statement = m_database.prepareStatement("SELECT subFrameDomain, registrableDomain FROM (SELECT o.registrableDomain as subFrameDomain, s.topLevelDomainID as topLevelDomainID FROM ObservedDomains as o INNER JOIN StorageAccessUnderTopFrameDomains as s WHERE o.domainID = s.domainID) as z INNER JOIN ObservedDomains ON domainID = z.topLevelDomainID;"_s);
    if (!statement)
        return results;

    while (statement->step() == SQLITE_ROW)
        results.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(1)), RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(0)));

    return results;
}

void ResourceLoadStatisticsDatabaseStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToBlockAndDeleteCookiesFor = this->domainsToBlockAndDeleteCookiesFor();
    auto domainsToBlockButKeepCookiesFor = this->domainsToBlockButKeepCookiesFor();
    auto domainsWithUserInteractionAsFirstParty = this->domainsWithUserInteractionAsFirstParty();
    auto domainsWithStorageAccess = this->domainsWithStorageAccess();

    if (domainsToBlockAndDeleteCookiesFor.isEmpty() && domainsToBlockButKeepCookiesFor.isEmpty() && domainsWithUserInteractionAsFirstParty.isEmpty() && domainsWithStorageAccess.isEmpty()) {
        completionHandler();
        return;
    }

    RegistrableDomainsToBlockCookiesFor domainsToBlock { domainsToBlockAndDeleteCookiesFor, domainsToBlockButKeepCookiesFor, domainsWithUserInteractionAsFirstParty, domainsWithStorageAccess };

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
    auto statement = m_database.prepareStatement("SELECT domainID, registrableDomain, mostRecentUserInteractionTime, hadUserInteraction, grandfathered, isScheduledForAllButCookieDataRemoval, countOfTopFrameRedirects FROM ObservedDomains LEFT JOIN (SELECT sourceDomainID, COUNT(*) as countOfTopFrameRedirects from TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement GROUP BY sourceDomainID) as z ON z.sourceDomainID = domainID"_s);
    if (!statement)
        return results;
    
    while (statement->step() == SQLITE_ROW) {
        results.append({ static_cast<unsigned>(statement->columnInt(0))
            , RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(1))
            , WallTime::fromRawSeconds(statement->columnDouble(2))
            , statement->columnInt(3) ? true : false
            , statement->columnInt(4) ? true : false
            , statement->columnInt(5) ? true : false
            , static_cast<unsigned>(statement->columnInt(6))
        });
    }
    
    return results;
}

void ResourceLoadStatisticsDatabaseStore::clearGrandfathering(Vector<unsigned>&& domainIDsToClear)
{
    ASSERT(!RunLoop::isMain());

    if (domainIDsToClear.isEmpty())
        return;

    auto listToClear = buildList(WTF::IteratorRange<Vector<unsigned>::iterator>(domainIDsToClear.begin(), domainIDsToClear.end()));

    auto clearGrandfatheringStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET grandfathered = 0 WHERE domainID IN (", listToClear, ")"));
    if (!clearGrandfatheringStatement)
        return;
    
    if (clearGrandfatheringStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearGrandfathering failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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

std::optional<WallTime> ResourceLoadStatisticsDatabaseStore::mostRecentUserInteractionTime(const DomainData& statistic)
{
    if (statistic.mostRecentUserInteractionTime.secondsSinceEpoch().value() <= 0)
        return std::nullopt;

    return statistic.mostRecentUserInteractionTime;
}

RegistrableDomainsToDeleteOrRestrictWebsiteDataFor ResourceLoadStatisticsDatabaseStore::registrableDomainsToDeleteOrRestrictWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    auto now = WallTime::now();
    auto oldestUserInteraction = now;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor toDeleteOrRestrictFor;

    SQLiteTransaction transaction(m_database);

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
                if (!transaction.inProgress())
                    transaction.begin();
                setIsScheduledForAllButCookieDataRemoval(transaction, statistic.registrableDomain, false);
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

    transaction.commit();
    
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
        count = scopedStatement->columnInt(0);

    if (count <= parameters().maxStatisticsEntries)
        return;

    ASSERT(parameters().pruneEntriesDownTo <= parameters().maxStatisticsEntries);

    size_t countLeftToPrune = count - parameters().pruneEntriesDownTo;
    ASSERT(countLeftToPrune);

    auto recordsToPrune = m_database.prepareStatement("SELECT domainID FROM ObservedDomains ORDER BY hadUserInteraction, isPrevalent, lastSeen LIMIT ?"_s);
    if (!recordsToPrune || recordsToPrune->bindInt(1, countLeftToPrune) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<unsigned> entriesToPrune;
    while (recordsToPrune->step() == SQLITE_ROW)
        entriesToPrune.append(recordsToPrune->columnInt(0));

    auto listToPrune = buildList(WTF::IteratorRange<Vector<unsigned>::iterator>(entriesToPrune.begin(), entriesToPrune.end()));

    auto pruneCommand = m_database.prepareStatementSlow(makeString("DELETE from ObservedDomains WHERE domainID IN (", listToPrune, ")"));
    if (!pruneCommand || pruneCommand->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateLastSeen failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

void ResourceLoadStatisticsDatabaseStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setLastSeen was not completed due to failed insert attempt", this);
        return;
    }
    
    updateLastSeen(domain, WallTime::fromRawSeconds(seconds.seconds()));

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();
    setPrevalentResource(transaction, domain);
    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setPrevalentResource(const SQLiteTransaction& transaction, const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    if (shouldSkip(domain))
        return;

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }

    setPrevalentResource(transaction, domain, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    setVeryPrevalentResource(transaction, domain);

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource(const SQLiteTransaction& transaction, const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_database.transactionInProgress());

    if (shouldSkip(domain))
        return;

    auto result = ensureResourceStatisticsForRegistrableDomain(domain);
    if (!result.second) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::setVeryPrevalentResource was not completed due to failed insert attempt", this);
        return;
    }

    setPrevalentResource(transaction, domain, ResourceLoadPrevalence::VeryHigh);
}

void ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved(const RegistrableDomain& domain, int value)
{
    ASSERT(!RunLoop::isMain());
    
    auto scopedStatement = this->scopedStatement(m_updateDataRecordsRemovedStatement, updateDataRecordsRemovedQuery, "updateDataRecordsRemoved"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateDataRecordsRemoved failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

bool ResourceLoadStatisticsDatabaseStore::isCorrectSubStatisticsCount(const RegistrableDomain& subframeDomain, const TopFrameDomain& topFrameDomain)
{
    auto subFrameUnderTopFrameCount = m_database.prepareStatement(countSubframeUnderTopFrameQuery);
    auto subresourceUnderTopFrameCount = m_database.prepareStatement(countSubresourceUnderTopFrameQuery);
    auto subresourceUniqueRedirectsTo = m_database.prepareStatement(countSubresourceUniqueRedirectsToQuery);
    
    if (!subFrameUnderTopFrameCount || !subresourceUnderTopFrameCount || !subresourceUniqueRedirectsTo) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::countSubStatisticsTesting failed to prepare, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    
    if (subFrameUnderTopFrameCount->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subFrameUnderTopFrameCount->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::countSubStatisticsTesting failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    
    if (subFrameUnderTopFrameCount->step() != SQLITE_ROW
        || subresourceUnderTopFrameCount->step() != SQLITE_ROW
        || subresourceUniqueRedirectsTo->step() != SQLITE_ROW)
        return false;
    
    return (subFrameUnderTopFrameCount->columnInt(0) == 1 && subresourceUnderTopFrameCount->columnInt(0) == 1 && subresourceUniqueRedirectsTo->columnInt(0) == 1);
}

static void appendBoolean(StringBuilder& builder, const String& label, bool flag)
{
    builder.append("    ", label, ": ", flag ? "Yes" : "No");
}

static void appendNextEntry(StringBuilder& builder, const String& entry)
{
    builder.append("        ", entry, '\n');
}

String ResourceLoadStatisticsDatabaseStore::getDomainStringFromDomainID(unsigned domainID) const
{
    auto result = emptyString();
    
    auto scopedStatement = this->scopedStatement(m_domainStringFromDomainIDStatement, domainStringFromDomainIDQuery, "getDomainStringFromDomainID"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, domainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::getDomainStringFromDomainID. Statement failed to prepare or bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return result;
    }
    
    if (scopedStatement->step() == SQLITE_ROW)
        result = m_domainStringFromDomainIDStatement->columnText(0);
    
    return result;
}

ASCIILiteral ResourceLoadStatisticsDatabaseStore::getSubStatisticStatement(const String& tableName) const
{
    if (tableName == "StorageAccessUnderTopFrameDomains")
        return "SELECT topLevelDomainID from StorageAccessUnderTopFrameDomains WHERE domainID = ?"_s;
    if (tableName == "TopFrameUniqueRedirectsTo")
        return "SELECT toDomainID from TopFrameUniqueRedirectsTo WHERE sourceDomainID = ?"_s;
    if (tableName == "TopFrameUniqueRedirectsFrom")
        return "SELECT fromDomainID from TopFrameUniqueRedirectsFrom WHERE targetDomainID = ?"_s;
    if (tableName == "TopFrameLinkDecorationsFrom")
        return "SELECT fromDomainID from TopFrameLinkDecorationsFrom WHERE toDomainID = ?"_s;
    if (tableName == "TopFrameLoadedThirdPartyScripts")
        return "SELECT subresourceDomainID from TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ?"_s;
    if (tableName == "SubframeUnderTopFrameDomains")
        return "SELECT topFrameDomainID from SubframeUnderTopFrameDomains WHERE subFrameDomainID = ?"_s;
    if (tableName == "SubresourceUnderTopFrameDomains")
        return "SELECT topFrameDomainID from SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ?"_s;
    if (tableName == "SubresourceUniqueRedirectsTo")
        return "SELECT toDomainID from SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ?"_s;
    if (tableName == "SubresourceUniqueRedirectsFrom")
        return "SELECT fromDomainID from SubresourceUniqueRedirectsFrom WHERE subresourceDomainID = ?"_s;
    
    return ""_s;
}

void ResourceLoadStatisticsDatabaseStore::appendSubStatisticList(StringBuilder& builder, const String& tableName, const String& domain) const
{
    auto query = getSubStatisticStatement(tableName);
    
    if (!query.length())
        return;
    
    auto data = m_database.prepareStatement(query);
    
    if (!data || data->bindInt(1, domainID(RegistrableDomain::uncheckedCreateFromHost(domain)).value()) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::appendSubStatisticList. Statement failed to prepare or bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    if (data->step() != SQLITE_ROW)
        return;
    
    builder.append("    ", tableName, ":\n");
    
    auto result = getDomainStringFromDomainID(data->columnInt(0));
    appendNextEntry(builder, result);
    
    while (data->step() == SQLITE_ROW) {
        result = getDomainStringFromDomainID(data->columnInt(0));
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::resourceToString. Statement failed to bind or domain was not found, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    builder.append("Registrable domain: ", domain, '\n');
    
    // User interaction
    appendBoolean(builder, "hadUserInteraction", m_getResourceDataByDomainNameStatement->columnInt(HadUserInteractionIndex));
    builder.append('\n');
    builder.append("    mostRecentUserInteraction: ");
    if (hasHadRecentUserInteraction(Seconds(m_getResourceDataByDomainNameStatement->columnDouble(MostRecentUserInteractionTimeIndex))))
        builder.append("within 24 hours");
    else
        builder.append("-1");
    builder.append('\n');
    appendBoolean(builder, "grandfathered", m_getResourceDataByDomainNameStatement->columnInt(GrandfatheredIndex));
    builder.append('\n');

    // Storage access
    appendSubStatisticList(builder, "StorageAccessUnderTopFrameDomains", domain);

    // Top frame stats
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement", domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLinkDecorationsFrom", domain);
    appendSubStatisticList(builder, "TopFrameLoadedThirdPartyScripts", domain);

    appendBoolean(builder, "IsScheduledForAllButCookieDataRemoval", m_getResourceDataByDomainNameStatement->columnInt(IsScheduledForAllButCookieDataRemovalIndex));
    builder.append('\n');

    // Subframe stats
    appendSubStatisticList(builder, "SubframeUnderTopFrameDomains", domain);

    // Subresource stats
    appendSubStatisticList(builder, "SubresourceUnderTopFrameDomains", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsTo", domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsFrom", domain);

    // Prevalent Resource
    appendBoolean(builder, "isPrevalentResource", m_getResourceDataByDomainNameStatement->columnInt(IsPrevalentIndex));
    builder.append('\n');
    appendBoolean(builder, "isVeryPrevalentResource", m_getResourceDataByDomainNameStatement->columnInt(IsVeryPrevalentIndex));
    builder.append('\n');
    builder.append("    dataRecordsRemoved: ", m_getResourceDataByDomainNameStatement->columnInt(DataRecordsRemovedIndex));
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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDExistsInDatabase failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_linkDecorationExistsStatement->step() != SQLITE_ROW
        || m_scriptLoadExistsStatement->step() != SQLITE_ROW
        || m_subFrameExistsStatement->step() != SQLITE_ROW
        || m_subResourceExistsStatement->step() != SQLITE_ROW
        || m_uniqueRedirectExistsStatement->step() != SQLITE_ROW
        || m_observedDomainsExistsStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::domainIDExistsInDatabase failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    return m_linkDecorationExistsStatement->columnInt(0) || m_scriptLoadExistsStatement->columnInt(0) || m_subFrameExistsStatement->columnInt(0) || m_subResourceExistsStatement->columnInt(0) || m_uniqueRedirectExistsStatement->columnInt(0) || m_observedDomainsExistsStatement->columnInt(0);
}

void ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters()
{
    auto countOperatingDatesStatement = m_database.prepareStatement("SELECT COUNT(*) FROM OperatingDates;"_s);
    auto getMostRecentOperatingDateStatement = m_database.prepareStatement("SELECT * FROM OperatingDates ORDER BY year DESC, month DESC, monthDay DESC LIMIT 1;"_s);
    auto getOperatingDateWindowStatement = m_database.prepareStatement("SELECT * FROM OperatingDates ORDER BY year DESC, month DESC, monthDay DESC LIMIT 1 OFFSET ?;"_s);

    if (!countOperatingDatesStatement || countOperatingDatesStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters countOperatingDatesStatement failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    m_operatingDatesSize = countOperatingDatesStatement->columnInt(0);

    if (!getMostRecentOperatingDateStatement || getMostRecentOperatingDateStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getFirstOperatingDateStatement failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    m_mostRecentOperatingDate = OperatingDate(getMostRecentOperatingDateStatement->columnInt(0), getMostRecentOperatingDateStatement->columnInt(1), getMostRecentOperatingDateStatement->columnInt(2));

    if (!getOperatingDateWindowStatement) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getOperatingDateWindowStatement failed during the call to prepare() with error message: %" PRIVATE_LOG_STRING ".", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_operatingDatesSize <= operatingDatesWindowShort - 1)
        m_shortWindowOperatingDate = std::nullopt;
    else {
        if (getOperatingDateWindowStatement->bindInt(1, operatingDatesWindowShort - 1) != SQLITE_OK
            || getOperatingDateWindowStatement->step() != SQLITE_ROW) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getOperatingDateWindowStatement failed with error message: %" PRIVATE_LOG_STRING ". The error could be in the calls to bind() or step().", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
        m_shortWindowOperatingDate = OperatingDate(getOperatingDateWindowStatement->columnInt(0), getOperatingDateWindowStatement->columnInt(1), getOperatingDateWindowStatement->columnInt(2));
    }

    if (m_operatingDatesSize <= operatingDatesWindowLong - 1)
        m_longWindowOperatingDate = std::nullopt;
    else {
        getOperatingDateWindowStatement->reset();
        if (getOperatingDateWindowStatement->bindInt(1, operatingDatesWindowLong - 1) != SQLITE_OK
            || getOperatingDateWindowStatement->step() != SQLITE_ROW) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::updateOperatingDatesParameters getOperatingDateWindowStatement failed with error message: %" PRIVATE_LOG_STRING ". The error could be in the calls to bind() or step().", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }
        m_longWindowOperatingDate = OperatingDate(getOperatingDateWindowStatement->columnInt(0), getOperatingDateWindowStatement->columnInt(1), getOperatingDateWindowStatement->columnInt(2));
    }
}

void ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());
    
    auto today = OperatingDate::today();
    if (m_operatingDatesSize > 0) {
        if (today <= m_mostRecentOperatingDate)
            return;
    }

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    int rowsToPrune = m_operatingDatesSize - operatingDatesWindowLong + 1;
    if (rowsToPrune > 0) {
        auto deleteLeastRecentOperatingDateStatement = m_database.prepareStatement("DELETE FROM OperatingDates ORDER BY year, month, monthDay LIMIT ?;"_s);
        if (!deleteLeastRecentOperatingDateStatement
            || deleteLeastRecentOperatingDateStatement->bindInt(1, rowsToPrune) != SQLITE_OK
            || deleteLeastRecentOperatingDateStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary deleteLeastRecentOperatingDateStatement failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
    }
    
    auto insertOperatingDateStatement = m_database.prepareStatement("INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
    if (!insertOperatingDateStatement
        || insertOperatingDateStatement->bindInt(1, today.year()) != SQLITE_OK
        || insertOperatingDateStatement->bindInt(2, today.month()) != SQLITE_OK
        || insertOperatingDateStatement->bindInt(3, today.monthDay()) != SQLITE_OK
        || insertOperatingDateStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::includeTodayAsOperatingDateIfNecessary insertOperatingDateStatement failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    updateOperatingDatesParameters();

    transaction.commit();
}

bool ResourceLoadStatisticsDatabaseStore::hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow operatingDatesWindow) const
{
    ASSERT(!RunLoop::isMain());

    switch (operatingDatesWindow) {
    case OperatingDatesWindow::Long:
        if (m_longWindowOperatingDate && OperatingDate::fromWallTime(mostRecentUserInteractionTime) < *m_longWindowOperatingDate)
            return true;
        break;
    case OperatingDatesWindow::Short:
        if (m_shortWindowOperatingDate && OperatingDate::fromWallTime(mostRecentUserInteractionTime) < *m_shortWindowOperatingDate)
            return true;
        break;
    case OperatingDatesWindow::ForLiveOnTesting:
        return WallTime::now() > mostRecentUserInteractionTime + operatingTimeWindowForLiveOnTesting;
    case OperatingDatesWindow::ForReproTesting:
        return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (this->parameters().timeToLiveUserInteraction) {
        if (WallTime::now() > mostRecentUserInteractionTime + this->parameters().timeToLiveUserInteraction.value())
            return true;
    }
    return false;
}

void ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting(const RegistrableDomain& domain, unsigned numberOfOperatingDaysPassed, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent)
{
    // Populate the Operating Dates table with enough days to require pruning.
    double daysAgoInSeconds = 0;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (unsigned i = 1; i <= numberOfOperatingDaysPassed; i++) {
        double daysToSubtract = Seconds::fromHours(24 * i).value();
        daysAgoInSeconds = WallTime::now().secondsSinceEpoch().value() - daysToSubtract;
        auto dateToInsert = OperatingDate::fromWallTime(WallTime::fromRawSeconds(daysAgoInSeconds));

        auto insertOperatingDateStatement = m_database.prepareStatement("INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
        if (!insertOperatingDateStatement
            || insertOperatingDateStatement->bindInt(1, dateToInsert.year()) != SQLITE_OK
            || insertOperatingDateStatement->bindInt(2, dateToInsert.month()) != SQLITE_OK
            || insertOperatingDateStatement->bindInt(3, dateToInsert.monthDay()) != SQLITE_OK
            || insertOperatingDateStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting insertOperatingDateStatement failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
        insertOperatingDateStatement->reset();
    }

    updateOperatingDatesParameters();

    transaction.commit();

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
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    if (scopedInsertObservedDomainStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertExpiredStatisticForTesting failed to commit, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

PrivateClickMeasurement ResourceLoadStatisticsDatabaseStore::buildPrivateClickMeasurementFromDatabase(WebCore::SQLiteStatement* statement, PrivateClickMeasurementAttributionType attributionType)
{
    auto sourceSiteDomain = getDomainStringFromDomainID(statement->columnInt(0));
    auto destinationSiteDomain = getDomainStringFromDomainID(statement->columnInt(1));
    auto sourceID = statement->columnInt(2);
    auto timeOfAdClick = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement->columnDouble(5) : statement->columnDouble(3);
    auto token = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement->columnText(7) : statement->columnText(4);
    auto signature = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement->columnText(8) : statement->columnText(5);
    auto keyID = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement->columnText(9) : statement->columnText(6);

    PrivateClickMeasurement attribution(WebCore::PrivateClickMeasurement::SourceID(sourceID), WebCore::PrivateClickMeasurement::SourceSite(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(sourceSiteDomain)), WebCore::PrivateClickMeasurement::AttributionDestinationSite(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(destinationSiteDomain)), { }, { }, WallTime::fromRawSeconds(timeOfAdClick));
    
    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = statement->columnInt(3);
        auto priority = statement->columnInt(4);
        auto sourceEarliestTimeToSendValue = statement->columnDouble(6);
        auto destinationEarliestTimeToSendValue = statement->columnDouble(10);

        if (attributionTriggerData != -1)
            attribution.setAttribution(WebCore::PrivateClickMeasurement::AttributionTriggerData { static_cast<uint32_t>(attributionTriggerData), WebCore::PrivateClickMeasurement::Priority(priority) });

        std::optional<WallTime> sourceEarliestTimeToSend;
        std::optional<WallTime> destinationEarliestTimeToSend;
        
        // A value of 0.0 indicates that the report has been sent to the respective site.
        if (sourceEarliestTimeToSendValue > 0.0)
            sourceEarliestTimeToSend = WallTime::fromRawSeconds(sourceEarliestTimeToSendValue);

        if (destinationEarliestTimeToSendValue > 0.0)
            destinationEarliestTimeToSend = WallTime::fromRawSeconds(destinationEarliestTimeToSendValue);

        attribution.setTimesToSend({ sourceEarliestTimeToSend, destinationEarliestTimeToSend });
    }

    attribution.setSourceSecretToken({ token, signature, keyID });

    return attribution;
}

std::pair<std::optional<UnattributedPrivateClickMeasurement>, std::optional<AttributedPrivateClickMeasurement>> ResourceLoadStatisticsDatabaseStore::findPrivateClickMeasurement(const WebCore::PrivateClickMeasurement::SourceSite& sourceSite, const WebCore::PrivateClickMeasurement::AttributionDestinationSite& destinationSite)
{
    auto sourceSiteDomainID = domainID(sourceSite.registrableDomain);
    auto destinationSiteDomainID = domainID(destinationSite.registrableDomain);

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return std::make_pair(std::nullopt, std::nullopt);

    auto findUnattributedScopedStatement = this->scopedStatement(m_findUnattributedStatement, findUnattributedQuery, "findPrivateClickMeasurement"_s);
    if (!findUnattributedScopedStatement
        || findUnattributedScopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || findUnattributedScopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::findPrivateClickMeasurement findUnattributedQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    auto findAttributedScopedStatement = this->scopedStatement(m_findAttributedStatement, findAttributedQuery, "findPrivateClickMeasurement"_s);
    if (!findAttributedScopedStatement
        || findAttributedScopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || findAttributedScopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::findPrivateClickMeasurement findAttributedQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    std::optional<UnattributedPrivateClickMeasurement> unattributedPrivateClickMeasurement;
    if (findUnattributedScopedStatement->step() == SQLITE_ROW)
        unattributedPrivateClickMeasurement = buildPrivateClickMeasurementFromDatabase(findUnattributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Unattributed);

    std::optional<AttributedPrivateClickMeasurement> attributedPrivateClickMeasurement;
    if (findAttributedScopedStatement->step() == SQLITE_ROW)
        attributedPrivateClickMeasurement = buildPrivateClickMeasurementFromDatabase(findAttributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed);

    return std::make_pair(unattributedPrivateClickMeasurement, attributedPrivateClickMeasurement);
}

void ResourceLoadStatisticsDatabaseStore::insertPrivateClickMeasurement(PrivateClickMeasurement&& attribution, PrivateClickMeasurementAttributionType attributionType)
{
    SQLiteTransaction transaction(m_database);
    transaction.begin();
    auto commitTransaction = makeScopeExit([&transaction] { transaction.commit(); });

    auto sourceData = ensureResourceStatisticsForRegistrableDomain(attribution.sourceSite().registrableDomain);
    auto attributionDestinationData = ensureResourceStatisticsForRegistrableDomain(attribution.destinationSite().registrableDomain);

    if (!sourceData.second || !attributionDestinationData.second)
        return;

    auto& sourceUnlinkableToken = attribution.sourceUnlinkableToken();
    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = attribution.attributionTriggerData() ? attribution.attributionTriggerData().value().data : -1;
        auto priority = attribution.attributionTriggerData() ? attribution.attributionTriggerData().value().priority : -1;
        auto sourceEarliestTimeToSend = attribution.timesToSend().sourceEarliestTimeToSend ? attribution.timesToSend().sourceEarliestTimeToSend.value().secondsSinceEpoch().value() : -1;
        auto destinationEarliestTimeToSend = attribution.timesToSend().destinationEarliestTimeToSend ? attribution.timesToSend().destinationEarliestTimeToSend.value().secondsSinceEpoch().value() : -1;

        // We should never be inserting an attributed private click measurement value into the database without valid report times.
        ASSERT(sourceEarliestTimeToSend != -1 && destinationEarliestTimeToSend != -1);

        auto statement = m_database.prepareStatement(insertAttributedPrivateClickMeasurementQuery);
        if (!statement
            || statement->bindInt(1, *sourceData.second) != SQLITE_OK
            || statement->bindInt(2, *attributionDestinationData.second) != SQLITE_OK
            || statement->bindInt(3, attribution.sourceID().id) != SQLITE_OK
            || statement->bindInt(4, attributionTriggerData) != SQLITE_OK
            || statement->bindInt(5, priority) != SQLITE_OK
            || statement->bindDouble(6, attribution.timeOfAdClick().secondsSinceEpoch().value()) != SQLITE_OK
            || statement->bindDouble(7, sourceEarliestTimeToSend) != SQLITE_OK
            || statement->bindText(8, sourceUnlinkableToken ? sourceUnlinkableToken->tokenBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(9, sourceUnlinkableToken ? sourceUnlinkableToken->signatureBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(10, sourceUnlinkableToken ? sourceUnlinkableToken->keyIDBase64URL : emptyString()) != SQLITE_OK
            || statement->bindDouble(11, destinationEarliestTimeToSend) != SQLITE_OK
            || statement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertPrivateClickMeasurement insertAttributedPrivateClickMeasurementQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
        return;
    }

    auto statement = m_database.prepareStatement(insertUnattributedPrivateClickMeasurementQuery);
    if (!statement
        || statement->bindInt(1, *sourceData.second) != SQLITE_OK
        || statement->bindInt(2, *attributionDestinationData.second) != SQLITE_OK
        || statement->bindInt(3, attribution.sourceID().id) != SQLITE_OK
        || statement->bindDouble(4, attribution.timeOfAdClick().secondsSinceEpoch().value()) != SQLITE_OK
        || statement->bindText(5, sourceUnlinkableToken ? sourceUnlinkableToken->tokenBase64URL : emptyString()) != SQLITE_OK
        || statement->bindText(6, sourceUnlinkableToken ? sourceUnlinkableToken->signatureBase64URL : emptyString()) != SQLITE_OK
        || statement->bindText(7, sourceUnlinkableToken ? sourceUnlinkableToken->keyIDBase64URL : emptyString()) != SQLITE_OK
        || statement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::insertPrivateClickMeasurement insertUnattributedPrivateClickMeasurementQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting()
{
    auto scopedStatement = this->scopedStatement(m_setUnattributedPrivateClickMeasurementAsExpiredStatement, setUnattributedPrivateClickMeasurementAsExpiredQuery, "markAllUnattributedPrivateClickMeasurementAsExpiredForTesting"_s);

    if (!scopedStatement || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::removeUnattributed(PrivateClickMeasurement& attribution)
{
    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return;
    
    auto scopedStatement = this->scopedStatement(m_removeUnattributedStatement, removeUnattributedQuery, "removeUnattributed"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::removeUnattributed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

std::optional<PrivateClickMeasurement::AttributionSecondsUntilSendData> ResourceLoadStatisticsDatabaseStore::attributePrivateClickMeasurement(const SourceSite& sourceSite, const AttributionDestinationSite& destinationSite, AttributionTriggerData&& attributionTriggerData)
{
    // We should always clear expired clicks from the database before scheduling an attribution.
    clearExpiredPrivateClickMeasurement();

    if (!attributionTriggerData.isValid()) {
        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(PrivateClickMeasurement, "Got an invalid attribution.");
            debugBroadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Error, "[Private Click Measurement] Got an invalid attribution."_s);
        }
        return std::nullopt;
    }

    auto data = attributionTriggerData.data;
    auto priority = attributionTriggerData.priority;

    if (UNLIKELY(debugModeEnabled())) {
        RELEASE_LOG_INFO(PrivateClickMeasurement, "Got an attribution with attribution trigger data: %{public}u and priority: %{public}u.", data, priority);
        debugBroadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Info, makeString("[Private Click Measurement] Got an attribution with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'."_s));
    }

    PrivateClickMeasurement::AttributionSecondsUntilSendData secondsUntilSend { std::nullopt, std::nullopt };

    auto attribution = findPrivateClickMeasurement(sourceSite, destinationSite);
    auto& previouslyUnattributed = attribution.first;
    auto& previouslyAttributed = attribution.second;

    if (previouslyUnattributed) {
        // Always convert the pending attribution and remove it from the unattributed map.
        removeUnattributed(*previouslyUnattributed);
        secondsUntilSend = previouslyUnattributed.value().attributeAndGetEarliestTimeToSend(WTFMove(attributionTriggerData));

        // We should always have a valid secondsUntilSend value for a previouslyUnattributed value because there can be no previous attribution with a higher priority.
        if (!secondsUntilSend.hasValidSecondsUntilSendValues()) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }

        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(PrivateClickMeasurement, "Converted a stored ad click with attribution trigger data: %{public}u and priority: %{public}u.", data, priority);
            debugBroadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Info, makeString("[Private Click Measurement] Converted a stored ad click with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'."_s));
        }

        // If there is no previous attribution, or the new attribution has higher priority, insert/update the database.
        if (!previouslyAttributed || previouslyUnattributed.value().hasHigherPriorityThan(*previouslyAttributed)) {
            insertPrivateClickMeasurement(WTFMove(*previouslyUnattributed), PrivateClickMeasurementAttributionType::Attributed);

            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Replaced a previously converted ad click with a new one with attribution data: %{public}u and priority: %{public}u because it had higher priority.", data, priority);
                debugBroadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Info, makeString("[Private Click Measurement] Replaced a previously converted ad click with a new one with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "' because it had higher priority."_s));
            }
        }
    } else if (previouslyAttributed) {
        // If we have no new attribution, re-attribute the old one to respect the new priority, but only if this report has
        // not been sent to the source or destination site yet.
        if (!previouslyAttributed.value().hasPreviouslyBeenReported()) {
            auto secondsUntilSend = previouslyAttributed.value().attributeAndGetEarliestTimeToSend(WTFMove(attributionTriggerData));
            if (!secondsUntilSend.hasValidSecondsUntilSendValues())
                return std::nullopt;

            insertPrivateClickMeasurement(WTFMove(*previouslyAttributed), PrivateClickMeasurementAttributionType::Attributed);

            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Re-converted an ad click with a new one with attribution trigger data: %{public}u and priority: %{public}u because it had higher priority.", data, priority);
                debugBroadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Info, makeString("[Private Click Measurement] Re-converted an ad click with a new one with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'' because it had higher priority."_s));
            }
        }
    }

    if (!secondsUntilSend.hasValidSecondsUntilSendValues())
        return std::nullopt;

    return secondsUntilSend;
}

Vector<WebCore::PrivateClickMeasurement> ResourceLoadStatisticsDatabaseStore::allAttributedPrivateClickMeasurement()
{
    auto attributedScopedStatement = this->scopedStatement(m_allAttributedPrivateClickMeasurementStatement, allAttributedPrivateClickMeasurementQuery, "privateClickMeasurementToString"_s);

    if (!attributedScopedStatement) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::privateClickMeasurementToString, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    Vector<WebCore::PrivateClickMeasurement> attributions;
    while (attributedScopedStatement->step() == SQLITE_ROW)
        attributions.append(buildPrivateClickMeasurementFromDatabase(attributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed));

    return attributions;
}

void ResourceLoadStatisticsDatabaseStore::clearPrivateClickMeasurement(std::optional<RegistrableDomain> domain)
{
    // Default to clear all entries if no domain is specified.
    String bindParameter = "%";
    if (domain) {
        auto domainIDToMatch = domainID(*domain);
        if (!domainIDToMatch)
            return;

        bindParameter = String::number(*domainIDToMatch);
    }

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto clearUnattributedScopedStatement = this->scopedStatement(m_clearUnattributedPrivateClickMeasurementStatement, clearUnattributedPrivateClickMeasurementQuery, "clearPrivateClickMeasurement"_s);

    if (!clearUnattributedScopedStatement
        || clearUnattributedScopedStatement->bindText(1, bindParameter) != SQLITE_OK
        || clearUnattributedScopedStatement->bindText(2, bindParameter) != SQLITE_OK
        || clearUnattributedScopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrivateClickMeasurement clearUnattributedScopedStatement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    auto clearAttributedScopedStatement = this->scopedStatement(m_clearAttributedPrivateClickMeasurementStatement, clearAttributedPrivateClickMeasurementQuery, "clearPrivateClickMeasurement"_s);

    if (!clearAttributedScopedStatement
        || clearAttributedScopedStatement->bindText(1, bindParameter) != SQLITE_OK
        || clearAttributedScopedStatement->bindText(2, bindParameter) != SQLITE_OK
        || clearAttributedScopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearPrivateClickMeasurement clearAttributedScopedStatement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    transaction.commit();
}

void ResourceLoadStatisticsDatabaseStore::clearExpiredPrivateClickMeasurement()
{
    auto expirationTimeFrame = WallTime::now() - WebCore::PrivateClickMeasurement::maxAge();
    auto scopedStatement = this->scopedStatement(m_clearExpiredPrivateClickMeasurementStatement, clearExpiredPrivateClickMeasurementQuery, "clearExpiredPrivateClickMeasurement"_s);

    if (!scopedStatement
        || scopedStatement->bindDouble(1, expirationTimeFrame.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearExpiredPrivateClickMeasurement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

String ResourceLoadStatisticsDatabaseStore::attributionToString(WebCore::SQLiteStatement* statement, PrivateClickMeasurementAttributionType attributionType)
{
    auto sourceSiteDomain = getDomainStringFromDomainID(statement->columnInt(0));
    auto destinationSiteDomain = getDomainStringFromDomainID(statement->columnInt(1));
    auto sourceID = statement->columnInt(2);

    StringBuilder builder;
    builder.append("Source site: ", sourceSiteDomain, "\nAttribute on site: ", destinationSiteDomain, "\nSource ID: ", sourceID);

    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = statement->columnInt(3);
        auto priority = statement->columnInt(4);
        auto earliestTimeToSend = statement->columnInt(6);

        if (attributionTriggerData != -1) {
            builder.append("\nAttribution trigger data: ", attributionTriggerData, "\nAttribution priority: ", priority, "\nAttribution earliest time to send: ");
            if (earliestTimeToSend == -1)
                builder.append("Not set");
            else {
                auto secondsUntilSend = WallTime::fromRawSeconds(earliestTimeToSend) - WallTime::now();
                builder.append((secondsUntilSend >= 24_h && secondsUntilSend <= 48_h) ? "Within 24-48 hours" : "Outside 24-48 hours");
            }
        } else
            builder.append("\nNo attribution trigger data.");
    } else
        builder.append("\nNo attribution trigger data.");
    builder.append('\n');

    return builder.toString();
}

String ResourceLoadStatisticsDatabaseStore::privateClickMeasurementToString()
{
    auto privateClickMeasurementDataExists = m_database.prepareStatement("SELECT (SELECT COUNT(*) FROM UnattributedPrivateClickMeasurement) as cnt1, (SELECT COUNT(*) FROM AttributedPrivateClickMeasurement) as cnt2"_s);
    if (!privateClickMeasurementDataExists || privateClickMeasurementDataExists->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::privateClickMeasurementToString failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (!privateClickMeasurementDataExists->columnInt(0) && !privateClickMeasurementDataExists->columnInt(1))
        return "\nNo stored Private Click Measurement data.\n"_s;

    auto unattributedScopedStatement = this->scopedStatement(m_allUnattributedPrivateClickMeasurementAttributionsStatement, allUnattributedPrivateClickMeasurementAttributionsQuery, "privateClickMeasurementToString"_s);

    if (!unattributedScopedStatement) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::privateClickMeasurementToString, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    unsigned unattributedNumber = 0;
    StringBuilder builder;
    while (unattributedScopedStatement->step() == SQLITE_ROW) {
        const char* prefix = unattributedNumber ? "" : "Unattributed Private Click Measurements:";
        builder.append(prefix, "\nWebCore::PrivateClickMeasurement ", ++unattributedNumber, '\n',
            attributionToString(unattributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Unattributed));
    }

    auto attributedScopedStatement = this->scopedStatement(m_allAttributedPrivateClickMeasurementStatement, allAttributedPrivateClickMeasurementQuery, "privateClickMeasurementToString"_s);

    if (!attributedScopedStatement) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::privateClickMeasurementToString, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    unsigned attributedNumber = 0;
    while (attributedScopedStatement->step() == SQLITE_ROW) {
        if (!attributedNumber)
            builder.append(unattributedNumber ? "\n" : "", "Attributed Private Click Measurements:");
        builder.append("\nWebCore::PrivateClickMeasurement ", ++attributedNumber + unattributedNumber, '\n',
            attributionToString(attributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed));
    }
    return builder.toString();
}

std::pair<std::optional<SourceEarliestTimeToSend>, std::optional<DestinationEarliestTimeToSend>> ResourceLoadStatisticsDatabaseStore::earliestTimesToSend(const WebCore::PrivateClickMeasurement& attribution)
{
    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return std::make_pair(std::nullopt, std::nullopt);

    auto scopedStatement = this->scopedStatement(m_earliestTimesToSendStatement, earliestTimesToSendQuery, "earliestTimesToSend"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::earliestTimesToSend, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    std::optional<SourceEarliestTimeToSend> earliestTimeToSendToSource;
    std::optional<DestinationEarliestTimeToSend> earliestTimeToSendToDestination;
    
    // A value of 0.0 indicates that the report has been sent to the respective site.
    if (scopedStatement->columnDouble(0) > 0.0)
        earliestTimeToSendToSource = scopedStatement->columnDouble(0);
    
    if (scopedStatement->columnDouble(1) > 0.0)
        earliestTimeToSendToDestination = scopedStatement->columnDouble(1);
    
    return std::make_pair(earliestTimeToSendToSource, earliestTimeToSendToDestination);
}

void ResourceLoadStatisticsDatabaseStore::markReportAsSentToSource(SourceDomainID sourceSiteDomainID, DestinationDomainID destinationSiteDomainID)
{
    auto scopedStatement = this->scopedStatement(m_markReportAsSentToSourceStatement, markReportAsSentToSourceQuery, "markReportAsSentToSource"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::markReportAsSentToSource, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::markReportAsSentToDestination(SourceDomainID sourceSiteDomainID, DestinationDomainID destinationSiteDomainID)
{
    auto scopedStatement = this->scopedStatement(m_markReportAsSentToDestinationStatement, markReportAsSentToDestinationQuery, "markReportAsSentToDestination"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "ResourceLoadStatisticsDatabaseStore::markReportAsSentToDestination, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::clearSentAttribution(WebCore::PrivateClickMeasurement&& attribution, PrivateClickMeasurement::AttributionReportEndpoint attributionReportEndpoint)
{    
    auto timesToSend = earliestTimesToSend(attribution);
    auto sourceEarliestTimeToSend = timesToSend.first;
    auto destinationEarliestTimeToSend = timesToSend.second;

    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return;

    switch (attributionReportEndpoint) {
    case PrivateClickMeasurement::AttributionReportEndpoint::Source:
        if (!sourceEarliestTimeToSend) {
            ASSERT_NOT_REACHED();
            return;
        }
        markReportAsSentToSource(*sourceSiteDomainID, *destinationSiteDomainID);
        sourceEarliestTimeToSend = std::nullopt;
        break;
    case PrivateClickMeasurement::AttributionReportEndpoint::Destination:
        if (!destinationEarliestTimeToSend) {
            ASSERT_NOT_REACHED();
            return;
        }
        markReportAsSentToDestination(*sourceSiteDomainID, *destinationSiteDomainID);
        destinationEarliestTimeToSend = std::nullopt;
    }

    // Don't clear the attribute from the database unless it has been reported both to the source and destination site.
    if (destinationEarliestTimeToSend || sourceEarliestTimeToSend)
        return;

    auto clearAttributedStatement = m_database.prepareStatement("DELETE FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ?"_s);
    if (!clearAttributedStatement
        || clearAttributedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || clearAttributedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || clearAttributedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::clearSentAttribution failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void ResourceLoadStatisticsDatabaseStore::markAttributedPrivateClickMeasurementsAsExpiredForTesting()
{
    auto expiredTimeToSend = WallTime::now() - 1_h;

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    auto earliestTimeToSendToSourceStatement = m_database.prepareStatement("UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToSource = ?"_s);
    auto earliestTimeToSendToDestinationStatement = m_database.prepareStatement("UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToDestination = null"_s);

    if (!earliestTimeToSendToSourceStatement
        || earliestTimeToSendToSourceStatement->bindInt(1, expiredTimeToSend.secondsSinceEpoch().value()) != SQLITE_OK
        || earliestTimeToSendToSourceStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::markAttributedPrivateClickMeasurementsAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    if (!earliestTimeToSendToDestinationStatement || earliestTimeToSendToDestinationStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_ERROR(m_sessionID, "%p - ResourceLoadStatisticsDatabaseStore::markAttributedPrivateClickMeasurementsAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    transaction.commit();

    return;
}

} // namespace WebKit

#undef ITP_RELEASE_LOG
#undef ITP_RELEASE_LOG_ERROR

#endif
