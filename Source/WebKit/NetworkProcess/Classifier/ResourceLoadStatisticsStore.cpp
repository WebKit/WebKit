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
#include "ResourceLoadStatisticsStore.h"

#include "DidFilterKnownLinkDecoration.h"
#include "ITPThirdPartyData.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "PrivateClickMeasurementManager.h"
#include "PrivateClickMeasurementManagerProxy.h"
#include "StorageAccessStatus.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/OrganizationStorageAccessPromptQuirk.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/Scope.h>
#include <wtf/StdSet.h>
#include <wtf/SuspendableWorkQueue.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

#define ITP_DEBUG_MODE_RELEASE_LOG(fmt, ...) RELEASE_LOG_INFO(ITPDebug, "ResourceLoadStatisticsStore: " fmt, ##__VA_ARGS__)
#define ITP_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ResourceLoadStatistics, "%p - [sessionID=%" PRIu64 "] - ResourceLoadStatisticsStore::" fmt, this, m_sessionID.toUInt64(), ##__VA_ARGS__)
#define ITP_RELEASE_LOG_DATABASE_ERROR(fmt, ...) RELEASE_LOG_ERROR(ResourceLoadStatistics, "%p - [sessionID=%" PRIu64 ", error=%d, message=%" PRIVATE_LOG_STRING "] - ResourceLoadStatisticsStore::" fmt, this, m_sessionID.toUInt64(), m_database.lastError(), m_database.lastErrorMsg(), ##__VA_ARGS__)


constexpr unsigned operatingDatesWindowLong { 30 }; // days
constexpr unsigned operatingDatesWindowShort { 7 }; // days

constexpr Seconds operatingTimeWindowForLiveOnTesting { 1_h };
constexpr Seconds minimumStatisticsProcessingInterval { 5_s };


// COUNT Queries
constexpr auto observedDomainCountQuery = "SELECT COUNT(*) FROM ObservedDomains"_s;
constexpr auto countSubframeUnderTopFrameQuery = "SELECT COUNT(*) FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? AND topFrameDomainID = ?;"_s;
constexpr auto countSubresourceUnderTopFrameQuery = "SELECT COUNT(*) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ? AND topFrameDomainID = ?;"_s;
constexpr auto countSubresourceUniqueRedirectsToQuery = "SELECT COUNT(*) FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? AND toDomainID = ?;"_s;

// INSERT OR IGNORE Queries
constexpr auto insertObservedDomainQuery = "INSERT INTO ObservedDomains (registrableDomain, lastSeen, hadUserInteraction,"
    "mostRecentUserInteractionTime, grandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, timesAccessedAsFirstPartyDueToUserInteraction,"
    "timesAccessedAsFirstPartyDueToStorageAccessAPI, isScheduledForAllButCookieDataRemoval, mostRecentWebPushInteractionTime) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
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
constexpr auto updateMostRecentWebPushInteractionTimeQuery = "UPDATE ObservedDomains SET mostRecentWebPushInteractionTime = ? WHERE registrableDomain = ?"_s;

// SELECT Queries
constexpr auto domainIDFromStringQuery = "SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto domainStringFromDomainIDQuery = "SELECT registrableDomain FROM ObservedDomains WHERE domainID = ?"_s;
constexpr auto isPrevalentResourceQuery = "SELECT isPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isVeryPrevalentResourceQuery = "SELECT isVeryPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto hadUserInteractionQuery = "SELECT hadUserInteraction, mostRecentUserInteractionTime FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isGrandfatheredQuery = "SELECT grandfathered FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto isScheduledForAllButCookieDataRemovalQuery = "SELECT isScheduledForAllButCookieDataRemoval FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto getResourceDataByDomainNameQuery = "SELECT * FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto getMostRecentlyUpdatedTimestampQuery = "SELECT MAX(lastUpdated) FROM (SELECT lastUpdated FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? and topFrameDomainID = ?"_s
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
    "isScheduledForAllButCookieDataRemoval INTEGER NOT NULL, "
    "mostRecentWebPushInteractionTime REAL NOT NULL)"_s;

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
    IsScheduledForAllButCookieDataRemovalIndex,
    MostRecentWebPushInteractionTimeIndex
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

// Add one to the count of the above index queries to account for the ObservedDomains table, which has an index automatically created by SQLite because of its primary key.
constexpr int expectedIndexCount = 13;

static String domainsToString(const Vector<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (auto& domain : domains)
        builder.append(builder.isEmpty() ? ""_s : ", "_s, domain.string());
    return builder.toString();
}

static String domainsToString(const RegistrableDomainsToDeleteOrRestrictWebsiteDataFor& domainsToRemoveOrRestrictWebsiteDataFor)
{
    StringBuilder builder;
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllCookiesFor)
        builder.append(builder.isEmpty() ? ""_s : ", "_s, domain.string(), "(all data)"_s);
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllButHttpOnlyCookiesFor)
        builder.append(builder.isEmpty() ? ""_s : ", "_s, domain.string(), "(all but HttpOnly cookies)"_s);
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllScriptWrittenStorageFor)
        builder.append(builder.isEmpty() ? ""_s : ", "_s, domain.string(), "(all but cookies)"_s);
    return builder.toString();
}

static String dataRemovalFrequencyToString(DataRemovalFrequency dataRemovalFrequency)
{
    switch (dataRemovalFrequency) {
    case DataRemovalFrequency::Never: return "Never"_s;
    case DataRemovalFrequency::Short: return "Short"_s;
    case DataRemovalFrequency::Long: return "Long"_s;
    }
    ASSERT_NOT_REACHED();
    return "Unknown"_s;
}

static bool needsNewCreateTableSchema(const String& schema)
{
    return schema.contains("REFERENCES TopLevelDomains"_s);
}

static WallTime nowTime(Seconds timeAdvanceForTesting)
{
    return WallTime::now() + timeAdvanceForTesting;
}

OperatingDate OperatingDate::fromWallTime(WallTime time)
{
    double ms = time.secondsSinceEpoch().milliseconds();
    int year = msToYear(ms);
    int yearDay = dayInYear(ms, year);
    int month = monthFromDayInYear(yearDay, isLeapYear(year));
    int monthDay = dayInMonthFromDayInYear(yearDay, isLeapYear(year));
    return OperatingDate { year, month, monthDay };
}

OperatingDate OperatingDate::today(Seconds timeAdvanceForTesting)
{
    return OperatingDate::fromWallTime(nowTime(timeAdvanceForTesting));
}

Seconds OperatingDate::secondsSinceEpoch() const
{
    return Seconds { dateToDaysFrom1970(m_year, m_month, m_monthDay) * secondsPerDay };
}

bool OperatingDate::operator<(const OperatingDate& other) const
{
    return secondsSinceEpoch() < other.secondsSinceEpoch();
}

bool OperatingDate::operator<=(const OperatingDate& other) const
{
    return secondsSinceEpoch() <= other.secondsSinceEpoch();
}

static DataRemovalFrequency toDataRemovalFrequency(int value)
{
    switch (value) {
    case 0: return DataRemovalFrequency::Never;
    case 1: return DataRemovalFrequency::Short;
    case 2: return DataRemovalFrequency::Long;
    default:
        ASSERT_NOT_REACHED();
        return DataRemovalFrequency::Short;
    };
}

template <typename ContainerType>
static String buildList(const ContainerType& values)
{
    StringBuilder builder;
    for (auto& value : values) {
        if constexpr (std::is_arithmetic_v<std::remove_reference_t<decltype(value)>>)
            builder.append(builder.isEmpty() ? ""_s : ", "_s, value);
        else
            builder.append(builder.isEmpty() ? ""_s : ", "_s, '"', value, '"');
    }
    return builder.toString();
}

static WeakHashSet<ResourceLoadStatisticsStore>& allStores()
{
    ASSERT(!RunLoop::isMain());

    static NeverDestroyed<WeakHashSet<ResourceLoadStatisticsStore>> map;
    return map;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResourceLoadStatisticsStore);

ResourceLoadStatisticsStore::ResourceLoadStatisticsStore(WebResourceLoadStatisticsStore& store, SuspendableWorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost, const String& storageDirectoryPath, PAL::SessionID sessionID)
    : DatabaseUtilities(FileSystem::pathByAppendingComponent(storageDirectoryPath, "observations.db"_s))
    , m_store(store)
    , m_workQueue(workQueue)
    , m_sessionID(sessionID)
    , m_shouldIncludeLocalhost(shouldIncludeLocalhost)
{
    ASSERT(!RunLoop::isMain());

    openAndUpdateSchemaIfNecessary();
    enableForeignKeys();

    if (!m_database.turnOnIncrementalAutoVacuum())
        ITP_RELEASE_LOG_DATABASE_ERROR("ResourceLoadStatisticsStore: failed to turn on auto vacuum");

    includeTodayAsOperatingDateIfNecessary();
    allStores().add(*this);
}

ResourceLoadStatisticsStore::~ResourceLoadStatisticsStore()
{
    ASSERT(!RunLoop::isMain());
    close();
    allStores().remove(*this);
}

void ResourceLoadStatisticsStore::openITPDatabase()
{
    m_isNewResourceLoadStatisticsDatabaseFile = openDatabaseAndCreateSchemaIfNecessary() == CreatedNewFile::Yes;
}

void ResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned = value;
}

bool ResourceLoadStatisticsStore::shouldSkip(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());
    return !(parameters().isRunningTest)
    && m_shouldIncludeLocalhost == ShouldIncludeLocalhost::No && domain.string() == "localhost"_s;
}

void ResourceLoadStatisticsStore::setIsRunningTest(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.isRunningTest = value;
}

void ResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void ResourceLoadStatisticsStore::removeDataRecords(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (!shouldRemoveDataRecords()) {
        completionHandler();
        return;
    }

    auto domainsToDeleteOrRestrictWebsiteDataFor = registrableDomainsToDeleteOrRestrictWebsiteDataFor();
    if (domainsToDeleteOrRestrictWebsiteDataFor.isEmpty()) {
        completionHandler();
        return;
    }

    if (UNLIKELY(m_debugLoggingEnabled)) {
        ITP_DEBUG_MODE_RELEASE_LOG("About to remove data records for %" PUBLIC_LOG_STRING ".", domainsToString(domainsToDeleteOrRestrictWebsiteDataFor).utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to remove data records for: ["_s, domainsToString(domainsToDeleteOrRestrictWebsiteDataFor), "]."_s));
    }

    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([store = Ref { m_store }, domainsToDeleteOrRestrictWebsiteDataFor = crossThreadCopy(WTFMove(domainsToDeleteOrRestrictWebsiteDataFor)), completionHandler = WTFMove(completionHandler), weakThis = WeakPtr { *this }, shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue] () mutable {
        store->deleteAndRestrictWebsiteDataForRegistrableDomains(WebResourceLoadStatisticsStore::monitoredDataTypes(), WTFMove(domainsToDeleteOrRestrictWebsiteDataFor), shouldNotifyPagesWhenDataRecordsWereScanned, [completionHandler = WTFMove(completionHandler), weakThis = WTFMove(weakThis), workQueue](HashSet<RegistrableDomain>&& domainsWithDeletedWebsiteData) mutable {
            workQueue->dispatch([domainsWithDeletedWebsiteData = crossThreadCopy(WTFMove(domainsWithDeletedWebsiteData)), completionHandler = WTFMove(completionHandler), weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis) {
                    completionHandler();
                    return;
                }

                weakThis->incrementRecordsDeletedCountForDomains(WTFMove(domainsWithDeletedWebsiteData));
                weakThis->setDataRecordsBeingRemoved(false);

                auto dataRecordRemovalCompletionHandlers = WTFMove(weakThis->m_dataRecordRemovalCompletionHandlers);
                completionHandler();

                for (auto& dataRecordRemovalCompletionHandler : dataRecordRemovalCompletionHandlers)
                    dataRecordRemovalCompletionHandler();

                if (UNLIKELY(weakThis->m_debugLoggingEnabled)) {
                    ITP_DEBUG_MODE_RELEASE_LOG("Done removing data records.");
                    weakThis->debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Done removing data records"_s);
                }
            });
        });
    });
}

void ResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    ASSERT(!RunLoop::isMain());

    if (parameters().isRunningTest && !m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
        return;

    if (m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval)
        classifyPrevalentResources();
    
    removeDataRecords([this, weakThis = WeakPtr { *this }] () mutable {
        ASSERT(!RunLoop::isMain());
        if (!weakThis)
            return;

        pruneStatisticsIfNeeded();

        logTestingEvent("Storage Synced"_s);

        if (!m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
            return;

        RunLoop::main().dispatch([store = Ref { m_store }] {
            store->notifyResourceLoadStatisticsProcessed();
        });
    });
}

void ResourceLoadStatisticsStore::grandfatherExistingWebsiteData(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, callback = WTFMove(callback), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue, store = Ref { m_store }] () mutable {
        store->registrableDomainsWithWebsiteData(WebResourceLoadStatisticsStore::monitoredDataTypes(), shouldNotifyPagesWhenDataRecordsWereScanned, [weakThis = WTFMove(weakThis), callback = WTFMove(callback), workQueue] (HashSet<RegistrableDomain>&& domainsWithWebsiteData) mutable {
            workQueue->dispatch([weakThis = WTFMove(weakThis), domainsWithWebsiteData = crossThreadCopy(WTFMove(domainsWithWebsiteData)), callback = WTFMove(callback)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }

                weakThis->grandfatherDataForDomains(domainsWithWebsiteData);
                weakThis->m_endOfGrandfatheringTimestamp = nowTime(weakThis->m_timeAdvanceForTesting) + weakThis->m_parameters.grandfatheringTime;
                callback();
                weakThis->logTestingEvent("Grandfathered"_s);
            });
        });
    });
}

void ResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool enable)
{
    ASSERT(!RunLoop::isMain());

    if (m_debugModeEnabled == enable)
        return;

    m_debugModeEnabled = enable;
    m_debugLoggingEnabled = enable;

    if (m_debugLoggingEnabled) {
        ITP_DEBUG_MODE_RELEASE_LOG("Turned ITP Debug Mode on.");
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Turned Debug Mode on."_s);
    } else {
        ITP_DEBUG_MODE_RELEASE_LOG("Turned ITP Debug Mode off.");
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Turned Debug Mode off."_s);
    }

    ensurePrevalentResourcesForDebugMode();
    // This will log the current cookie blocking state.
    if (enable)
        updateCookieBlocking([]() { });
}

void ResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const RegistrableDomain& domain)
{
    m_debugManualPrevalentResource = domain;
}

#if ENABLE(APP_BOUND_DOMAINS)
void ResourceLoadStatisticsStore::setAppBoundDomains(HashSet<RegistrableDomain>&& domains)
{
    m_appBoundDomains = WTFMove(domains);
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void ResourceLoadStatisticsStore::setManagedDomains(HashSet<RegistrableDomain>&& domains)
{
    m_managedDomains = WTFMove(domains);
}
#endif

void ResourceLoadStatisticsStore::scheduleStatisticsProcessingRequestIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = ++m_lastStatisticsProcessingRequestIdentifier;
    m_workQueue->dispatchAfter(minimumStatisticsProcessingInterval, [this, weakThis = WeakPtr { *this }, statisticsProcessingRequestIdentifier = *m_pendingStatisticsProcessingRequestIdentifier] {
        if (!weakThis)
            return;

        if (!m_pendingStatisticsProcessingRequestIdentifier || *m_pendingStatisticsProcessingRequestIdentifier != statisticsProcessingRequestIdentifier) {
            // This request has been canceled.
            return;
        }

        updateCookieBlocking([]() { });
        processStatisticsAndDataRecords();
    });
}

void ResourceLoadStatisticsStore::cancelPendingStatisticsProcessingRequest()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = std::nullopt;
}

void ResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.minimumTimeBetweenDataRecordsRemoval = seconds;
}

void ResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.grandfatheringTime = seconds;
}

void ResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.cacheMaxAgeCapTime = seconds;
    updateCacheMaxAgeCap();
}

void ResourceLoadStatisticsStore::updateCacheMaxAgeCap()
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = Ref { m_store }, seconds = m_parameters.cacheMaxAgeCapTime] () {
        store->setCacheMaxAgeCap(seconds, [] { });
    });
}

void ResourceLoadStatisticsStore::updateClientSideCookiesAgeCap()
{
    ASSERT(!RunLoop::isMain());

    Seconds capTime;
#if ENABLE(JS_COOKIE_CHECKING)
    capTime = m_parameters.clientSideCookiesForLinkDecorationTargetPageAgeCapTime;
#else
    capTime = m_parameters.clientSideCookiesAgeCapTime;
#endif

    RunLoop::main().dispatch([store = Ref { m_store }, seconds = capTime] () {
        if (auto* networkSession = store->networkSession()) {
            if (auto* storageSession = networkSession->networkStorageSession())
                storageSession->setAgeCapForClientSideCookies(seconds);
        }
    });
}

bool ResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());

    if (m_dataRecordsBeingRemoved)
        return false;

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_parameters.minimumTimeBetweenDataRecordsRemoval) || parameters().isRunningTest;
}

void ResourceLoadStatisticsStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

void ResourceLoadStatisticsStore::updateCookieBlockingForDomains(RegistrableDomainsToBlockCookiesFor&& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = Ref { m_store }, domainsToBlock = crossThreadCopy(WTFMove(domainsToBlock)), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}

bool ResourceLoadStatisticsStore::shouldEnforceSameSiteStrictForSpecificDomain(const RegistrableDomain& domain) const
{
    // We currently know of no domains that need this protection.
    UNUSED_PARAM(domain);
    return false;
}

void ResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.maxStatisticsEntries = maximumEntryCount;
}

void ResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.pruneEntriesDownTo = pruneTargetCount;
}

void ResourceLoadStatisticsStore::resetParametersToDefaultValues()
{
    ASSERT(!RunLoop::isMain());

    m_parameters = { };
    m_appBoundDomains.clear();
    m_timeAdvanceForTesting = { };
    m_operatingDatesSize = 0;
    m_shortWindowOperatingDate = std::nullopt;
    m_longWindowOperatingDate = std::nullopt;
}

void ResourceLoadStatisticsStore::logTestingEvent(String&& event)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([store = Ref { m_store }, event = WTFMove(event).isolatedCopy()] {
        store->logTestingEvent(event);
    });
}

void ResourceLoadStatisticsStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([store = Ref { m_store }, completionHandler = WTFMove(completionHandler)]() mutable {
        store->removeAllStorageAccess([store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}

void ResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(!RunLoop::isMain());

    updateCookieBlocking([]() { });
    updateCacheMaxAgeCap();
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsStore::debugBroadcastConsoleMessage(MessageSource source, MessageLevel level, const String& message)
{
    if (!RunLoop::isMain()) {
        RunLoop::main().dispatch([&, weakThis = WeakPtr { *this }, source = crossThreadCopy(source), level = crossThreadCopy(level), message = crossThreadCopy(message)]() {
            if (!weakThis)
                return;

            debugBroadcastConsoleMessage(source, level, message);
        });
        return;
    }

    if (auto* networkSession = m_store.networkSession())
        networkSession->networkProcess().broadcastConsoleMessage(networkSession->sessionID(), source, level, message);
}

void ResourceLoadStatisticsStore::debugLogDomainsInBatches(ASCIILiteral action, const RegistrableDomainsToBlockCookiesFor& domainsToBlock)
{
    ASSERT(debugLoggingEnabled());

    Vector<RegistrableDomain> domains;
    domains.appendVector(domainsToBlock.domainsToBlockAndDeleteCookiesFor);
    domains.appendVector(domainsToBlock.domainsToBlockButKeepCookiesFor);
    if (domains.isEmpty())
        return;

    debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] "_s, action, " to: ["_s, domainsToString(domains), "]."_s));

    static const auto maxNumberOfDomainsInOneLogStatement = 50;

    if (domains.size() <= maxNumberOfDomainsInOneLogStatement) {
        ITP_DEBUG_MODE_RELEASE_LOG("%" PUBLIC_LOG_STRING " to: %" PUBLIC_LOG_STRING ".", action.characters(), domainsToString(domains).utf8().data());
        return;
    }

    Vector<RegistrableDomain> batch;
    batch.reserveInitialCapacity(maxNumberOfDomainsInOneLogStatement);
#if !RELEASE_LOG_DISABLED
    auto batchNumber = 1;
    unsigned numberOfBatches = std::ceil(domains.size() / static_cast<float>(maxNumberOfDomainsInOneLogStatement));
#endif

    for (auto& domain : domains) {
        if (batch.size() == maxNumberOfDomainsInOneLogStatement) {
            ITP_DEBUG_MODE_RELEASE_LOG("%" PUBLIC_LOG_STRING " to (%d of %u): %" PUBLIC_LOG_STRING ".", action.characters(), batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
            batch.shrink(0);
#if !RELEASE_LOG_DISABLED
            ++batchNumber;
#endif
        }
        batch.append(domain);
    }
    if (!batch.isEmpty())
        ITP_DEBUG_MODE_RELEASE_LOG("%" PUBLIC_LOG_STRING " to (%d of %u): %" PUBLIC_LOG_STRING ".", action.characters(), batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
}

bool ResourceLoadStatisticsStore::shouldExemptFromWebsiteDataDeletion(const RegistrableDomain& domain) const
{
    return !domain.isEmpty() && domainsExemptFromWebsiteDataDeletion().contains(domain);
}

HashSet<RegistrableDomain> ResourceLoadStatisticsStore::domainsExemptFromWebsiteDataDeletion() const
{
    auto result = m_appBoundDomains.unionWith(m_managedDomains);
    result = result.unionWith(m_persistedDomains);

    if (!m_standaloneApplicationDomain.isEmpty())
        result.add(m_standaloneApplicationDomain);

    return result;
}

const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& ResourceLoadStatisticsStore::expectedTableAndIndexQueries()
{
    static NeverDestroyed expectedTableAndIndexQueries = MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair> {
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
    };
    return expectedTableAndIndexQueries;
}

std::span<const ASCIILiteral> ResourceLoadStatisticsStore::sortedTables()
{
    static constexpr std::array sortedTables {
        "ObservedDomains"_s,
        "TopLevelDomains"_s,
        "StorageAccessUnderTopFrameDomains"_s,
        "TopFrameUniqueRedirectsTo"_s,
        "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement"_s,
        "TopFrameUniqueRedirectsFrom"_s,
        "TopFrameLinkDecorationsFrom"_s,
        "TopFrameLoadedThirdPartyScripts"_s,
        "SubframeUnderTopFrameDomains"_s,
        "SubresourceUnderTopFrameDomains"_s,
        "SubresourceUniqueRedirectsTo"_s,
        "SubresourceUniqueRedirectsFrom"_s,
        "OperatingDates"_s
    };

    return sortedTables;
}

std::optional<Vector<String>> ResourceLoadStatisticsStore::checkForMissingTablesInSchema()
{
    Vector<String> missingTables;
    for (auto& table : expectedTableAndIndexQueries().keys()) {
        if (!this->tableExists(table))
            missingTables.append(String(table));
    }
    if (missingTables.isEmpty())
        return std::nullopt;

    return missingTables;
}

bool ResourceLoadStatisticsStore::tableExists(StringView tableName)
{
    constexpr auto checkIfTableExistsQuery = "SELECT 1 from sqlite_master WHERE type='table' and tbl_name=?"_s;
    auto scopedStatement = this->scopedStatement(m_checkIfTableExistsStatement, checkIfTableExistsQuery, "tableExists"_s);
    if (!scopedStatement) {
        ITP_RELEASE_LOG_DATABASE_ERROR("tableExists: failed to prepare statement");
        return false;
    }
    if (scopedStatement->bindText(1, tableName) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("tableExists: failed to bind parameter");
        return false;
    }
    return scopedStatement->step() == SQLITE_ROW;
}

void ResourceLoadStatisticsStore::deleteTable(StringView tableName)
{
    ASSERT(tableExists(tableName));
    auto dropTableQuery = m_database.prepareStatementSlow(makeString("DROP TABLE "_s, tableName));
    ASSERT(dropTableQuery);
    if (!dropTableQuery || dropTableQuery->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("deleteTable: failed to step statement");
}

bool ResourceLoadStatisticsStore::missingUniqueIndices()
{
    auto statement = m_database.prepareStatement("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index'"_s);
    if (!statement) {
        ITP_RELEASE_LOG_DATABASE_ERROR("missingUniqueIndices: failed to prepare statement");
        return false;
    }

    if (statement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("missingUniqueIndices: failed to step statement");
        return false;
    }

    return statement->columnInt(0) < expectedIndexCount;
}

bool ResourceLoadStatisticsStore::missingReferenceToObservedDomains()
{
    // Check a table for a reference to TopLevelDomains, a sign of the old schema.
    auto oldSchema = currentTableAndIndexQueries("TopFrameUniqueRedirectsTo"_s).first;
    return needsNewCreateTableSchema(oldSchema);
}

bool ResourceLoadStatisticsStore::needsUpdatedSchema()
{
    // There are 3 cases where we expect potential schema changes due to upgrades.
    // All other tables should be up-to-date, so we should ASSERT that they are correct.
    if (missingReferenceToObservedDomains() || missingUniqueIndices())
        return true;

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        UNUSED_PARAM(table);
        ASSERT(currentTableAndIndexQueries(table) == expectedTableAndIndexQueries().get(table));
    }

    return false;
}

void ResourceLoadStatisticsStore::migrateDataToPCMDatabaseIfNecessary()
{
    if (!tableExists("UnattributedPrivateClickMeasurement"_s)
        && !tableExists("AttributedPrivateClickMeasurement"_s))
        return;

    Vector<WebCore::PrivateClickMeasurement> unattributed;
    {
        constexpr auto allUnattributedPrivateClickMeasurementAttributionsQuery = "SELECT * FROM UnattributedPrivateClickMeasurement"_s;
        auto unattributedScopedStatement = m_database.prepareStatement(allUnattributedPrivateClickMeasurementAttributionsQuery);
        if (!unattributedScopedStatement) {
            ITP_RELEASE_LOG_DATABASE_ERROR("migrateDataToPCMDatabaseIfNecessary: failed to prepare unattributedScopedStatement");
            return;
        }
        while (unattributedScopedStatement->step() == SQLITE_ROW)
            unattributed.append(buildPrivateClickMeasurementFromDatabase(unattributedScopedStatement.value(), PrivateClickMeasurementAttributionType::Unattributed));
    }

    Vector<WebCore::PrivateClickMeasurement> attributed;
    {
        constexpr auto allAttributedPrivateClickMeasurementQuery = "SELECT * FROM AttributedPrivateClickMeasurement"_s;
        auto attributedScopedStatement = m_database.prepareStatement(allAttributedPrivateClickMeasurementQuery);
        if (!attributedScopedStatement) {
            ITP_RELEASE_LOG_DATABASE_ERROR("migrateDataToPCMDatabaseIfNecessary: failed to prepare attributedScopedStatement");
            return;
        }
        while (attributedScopedStatement->step() == SQLITE_ROW)
            attributed.append(buildPrivateClickMeasurementFromDatabase(attributedScopedStatement.value(), PrivateClickMeasurementAttributionType::Attributed));
    }

    if (!unattributed.isEmpty() || !attributed.isEmpty()) {
        RunLoop::main().dispatch([store = Ref { store() }, unattributed = crossThreadCopy(WTFMove(unattributed)), attributed = crossThreadCopy(WTFMove(attributed))] () mutable {
            auto* networkSession = store->networkSession();
            if (!networkSession)
                return;

            auto& manager = networkSession->privateClickMeasurement();
            for (auto&& pcm : WTFMove(attributed))
                manager.migratePrivateClickMeasurementFromLegacyStorage(WTFMove(pcm), PrivateClickMeasurementAttributionType::Attributed);
            for (auto&& pcm : WTFMove(unattributed))
                manager.migratePrivateClickMeasurementFromLegacyStorage(WTFMove(pcm), PrivateClickMeasurementAttributionType::Unattributed);
        });

    }

    auto transactionScope = beginTransactionIfNecessary();
    deleteTable("UnattributedPrivateClickMeasurement"_s);
    deleteTable("AttributedPrivateClickMeasurement"_s);
}

void ResourceLoadStatisticsStore::addMissingTablesIfNecessary()
{
    auto missingTables = checkForMissingTablesInSchema();
    if (!missingTables)
        return;

    auto transaction = beginTransactionIfNecessary();

    for (auto& table : *missingTables) {
        auto createTableQuery = expectedTableAndIndexQueries().get(table).first;
        if (!m_database.executeCommandSlow(createTableQuery))
            ITP_RELEASE_LOG_DATABASE_ERROR("addMissingTablesIfNecessary: failed to execute statement");
    }

    if (!createUniqueIndices())
        ITP_RELEASE_LOG_ERROR("addMissingTablesIfNecessary: failed to create unique indices");
}

template<typename T, typename U, size_t size> bool vectorEqualsArray(const Vector<T>& vector, const std::array<U, size> array)
{
    if (vector.size() != size)
        return false;
    for (size_t i = 0; i < size; i++) {
        if (vector[i] != array[i])
            return false;
    }
    return true;
}

void ResourceLoadStatisticsStore::openAndUpdateSchemaIfNecessary()
{
    openITPDatabase();
    addMissingTablesIfNecessary();

    auto columns = columnsForTable("ObservedDomains"_s);

    std::array<ASCIILiteral, 12> columnsWithoutMostRecentWebPushInteractionTime {
        "domainID"_s,
        "registrableDomain"_s,
        "lastSeen"_s,
        "hadUserInteraction"_s,
        "mostRecentUserInteractionTime"_s,
        "grandfathered"_s,
        "isPrevalent"_s,
        "isVeryPrevalent"_s,
        "dataRecordsRemoved"_s,
        "timesAccessedAsFirstPartyDueToUserInteraction"_s,
        "timesAccessedAsFirstPartyDueToStorageAccessAPI"_s,
        "isScheduledForAllButCookieDataRemoval"_s
    };

    if (vectorEqualsArray<String, ASCIILiteral, 12>(columns, columnsWithoutMostRecentWebPushInteractionTime)
        && addMissingColumnToTable("ObservedDomains"_s, "mostRecentWebPushInteractionTime REAL DEFAULT 0.0 NOT NULL"_s))
        columns.append("mostRecentWebPushInteractionTime"_s);

    std::array<ASCIILiteral, 13> currentSchemaColumns {
        "domainID"_s,
        "registrableDomain"_s,
        "lastSeen"_s,
        "hadUserInteraction"_s,
        "mostRecentUserInteractionTime"_s,
        "grandfathered"_s,
        "isPrevalent"_s,
        "isVeryPrevalent"_s,
        "dataRecordsRemoved"_s,
        "timesAccessedAsFirstPartyDueToUserInteraction"_s,
        "timesAccessedAsFirstPartyDueToStorageAccessAPI"_s,
        "isScheduledForAllButCookieDataRemoval"_s,
        "mostRecentWebPushInteractionTime"_s
    };

    if (!vectorEqualsArray<String, ASCIILiteral, 13>(columns, currentSchemaColumns)) {
        close();
        ITP_RELEASE_LOG_ERROR("openAndUpdateSchemaIfNecessary: failed at scheme check, will create new database");
        FileSystem::deleteFile(m_storageFilePath);
        openITPDatabase();
        return;
    }

    migrateDataToPCMDatabaseIfNecessary();
    migrateDataToNewTablesIfNecessary();
}

void ResourceLoadStatisticsStore::interruptAllDatabases()
{
    ASSERT(!RunLoop::isMain());
    for (auto& store : allStores())
        store.interrupt();
}

bool ResourceLoadStatisticsStore::isEmpty() const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_observedDomainCountStatement, observedDomainCountQuery, "isEmpty"_s);
    if (!scopedStatement
        || scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("isEmpty: failed to step statement");
        return false;
    }
    return !scopedStatement->columnInt(0);
}

bool ResourceLoadStatisticsStore::createUniqueIndices()
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
        || !m_database.executeCommand(createUniqueIndexOperatingDates)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createUniqueIndices: failed to execute statement");
        return false;
    }
    return true;
}

bool ResourceLoadStatisticsStore::createSchema()
{
    ASSERT(!RunLoop::isMain());

    if (!m_database.executeCommand(createObservedDomain)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createObservedDomain");
        return false;
    }

    // FIXME: drop TopLevelDomains table as it is not used.
    if (!m_database.executeCommand(createTopLevelDomains)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopLevelDomains");
        return false;
    }

    if (!m_database.executeCommand(createStorageAccessUnderTopFrameDomains)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createStorageAccessUnderTopFrameDomains");
        return false;
    }

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsTo)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopFrameUniqueRedirectsTo");
        return false;
    }

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement");
        return false;
    }

    if (!m_database.executeCommand(createTopFrameUniqueRedirectsFrom)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopFrameUniqueRedirectsFrom");
        return false;
    }

    if (!m_database.executeCommand(createTopFrameLinkDecorationsFrom)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopFrameLinkDecorationsFrom");
        return false;
    }

    if (!m_database.executeCommand(createTopFrameLoadedThirdPartyScripts)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createTopFrameLoadedThirdPartyScripts");
        return false;
    }

    if (!m_database.executeCommand(createSubframeUnderTopFrameDomains)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createSubframeUnderTopFrameDomains");
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUnderTopFrameDomains)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createSubresourceUnderTopFrameDomains");
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUniqueRedirectsTo)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createSubresourceUniqueRedirectsTo");
        return false;
    }

    if (!m_database.executeCommand(createSubresourceUniqueRedirectsFrom)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createSubresourceUniqueRedirectsFrom");
        return false;
    }

    if (!m_database.executeCommand(createOperatingDates)) {
        ITP_RELEASE_LOG_DATABASE_ERROR("createSchema: failed to execute statement createOperatingDates");
        return false;
    }

    if (!createUniqueIndices())
        return false;

    return true;
}

void ResourceLoadStatisticsStore::destroyStatements()
{
    ASSERT(!RunLoop::isMain());

    m_observedDomainCountStatement = nullptr;
    m_insertObservedDomainStatement = nullptr;
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
    m_isScheduledForAllButCookieDataRemovalStatement = nullptr;
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
    m_checkIfTableExistsStatement = nullptr;
    m_updateMostRecentWebPushInteractionTimeStatement = nullptr;
}

bool ResourceLoadStatisticsStore::insertObservedDomain(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    if (domainID(loadStatistics.registrableDomain)) {
        ITP_RELEASE_LOG_ERROR("insertObservedDomain: failed to find domain");
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
        || scopedStatement->bindInt(IsScheduledForAllButCookieDataRemovalIndex, loadStatistics.gotLinkDecorationFromPrevalentResource
        || scopedStatement->bindDouble(MostRecentWebPushInteractionTimeIndex, 0.0)) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("insertObservedDomain: failed to bind parameters");
        return false;
    }

    if (scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("insertObservedDomain: failed to step statement");
        return false;
    }
    return true;
}

bool ResourceLoadStatisticsStore::relationshipExists(SQLiteStatementAutoResetScope& statement, std::optional<unsigned> firstDomainID, const RegistrableDomain& secondDomain) const
{
    if (!firstDomainID)
        return false;

    ASSERT(!RunLoop::isMain());

    if (!statement
        || statement->bindInt(1, *firstDomainID) != SQLITE_OK
        || statement->bindText(2, secondDomain.string()) != SQLITE_OK
        || statement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("relationshipExists: failed to step statement");
        return false;
    }
    return !!statement->columnInt(0);
}

std::optional<unsigned> ResourceLoadStatisticsStore::domainID(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "domainID"_s);
    if (!scopedStatement || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("domainID: failed to bind parameter");
        return std::nullopt;
    }

    if (scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("domainID: failed to step statement");
        return std::nullopt;
    }

    return scopedStatement->columnInt(0);
}

String ResourceLoadStatisticsStore::ensureAndMakeDomainList(const HashSet<RegistrableDomain>& domainList)
{
    StringBuilder builder;
    for (auto& topFrameResource : domainList) {
        // Insert query will fail if top frame domain is not already in the database
        if (ensureResourceStatisticsForRegistrableDomain(topFrameResource, "ensureAndMakeDomainList"_s).second)
            builder.append(builder.isEmpty() ? ""_s : ", "_s, '"', topFrameResource.string(), '"');
    }
    return builder.toString();
}

void ResourceLoadStatisticsStore::insertDomainRelationshipList(const String& statement, const HashSet<RegistrableDomain>& domainList, unsigned domainID)
{
    auto insertRelationshipStatement = m_database.prepareStatementSlow(makeString(statement, ensureAndMakeDomainList(domainList), " );"_s));

    if (!insertRelationshipStatement || insertRelationshipStatement->bindInt(1, domainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("insertDomainRelationshipList: failed to bind first parameter");
        return;
    }

    if (statement.contains("REPLACE"_s)) {
        if (insertRelationshipStatement->bindDouble(2, nowTime(m_timeAdvanceForTesting).secondsSinceEpoch().value()) != SQLITE_OK) {
            ITP_RELEASE_LOG_DATABASE_ERROR("insertDomainRelationshipList: failed to bind second parameter");
            return;
        }
    }

    if (insertRelationshipStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("insertDomainRelationshipList: failed to step statement");
}

void ResourceLoadStatisticsStore::insertDomainRelationships(const ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

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

void ResourceLoadStatisticsStore::merge(WebCore::SQLiteStatement* current, const ResourceLoadStatistics& other)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

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
        setGrandfathered(other.registrableDomain, true);
    if (other.isPrevalentResource && !currentIsPrevalent)
        setPrevalentResource(other.registrableDomain);
    if (other.isVeryPrevalentResource && !currentIsVeryPrevalent)
        setVeryPrevalentResource(other.registrableDomain);
    if (other.dataRecordsRemoved > currentDataRecordsRemoved)
        updateDataRecordsRemoved(other.registrableDomain, other.dataRecordsRemoved);
    if (other.gotLinkDecorationFromPrevalentResource && !currentIsScheduledForAllButCookieDataRemoval)
        setIsScheduledForAllScriptWrittenStorageRemoval(other.registrableDomain, DataRemovalFrequency::Short);
}

void ResourceLoadStatisticsStore::mergeStatistic(const ResourceLoadStatistics& statistic)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();
    auto scopedStatement = this->scopedStatement(m_getResourceDataByDomainNameStatement, getResourceDataByDomainNameQuery, "mergeStatistic"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, statistic.registrableDomain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("mergeStatistic: failed to step statement");
        return;
    }

    merge(scopedStatement.get(), statistic);
}

void ResourceLoadStatisticsStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());
    if (statistics.isEmpty())
        return;

    auto transactionScope = beginTransactionIfNecessary();

    for (auto& statistic : statistics) {
        if (!domainID(statistic.registrableDomain)) {
            if (!insertObservedDomain(statistic)) {
                ITP_RELEASE_LOG_ERROR("mergeStatistics: failed to insert observed domain");
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

Vector<ITPThirdPartyDataForSpecificFirstParty> ResourceLoadStatisticsStore::getThirdPartyDataForSpecificFirstPartyDomains(unsigned thirdPartyDomainID, const RegistrableDomain& thirdPartyDomain) const
{
    auto scopedStatement = this->scopedStatement(m_getAllSubStatisticsStatement, getAllSubStatisticsUnderDomainQuery, "getThirdPartyDataForSpecificFirstPartyDomains"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, thirdPartyDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, thirdPartyDomainID) != SQLITE_OK
        || scopedStatement->bindInt(3, thirdPartyDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("getThirdPartyDataForSpecificFirstPartyDomains: failed to bind parameters");
        return { };
    }
    Vector<ITPThirdPartyDataForSpecificFirstParty> thirdPartyDataForSpecificFirstPartyDomains;
    while (scopedStatement->step() == SQLITE_ROW) {
        RegistrableDomain firstPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(m_getAllSubStatisticsStatement->columnInt(0)));
        thirdPartyDataForSpecificFirstPartyDomains.appendIfNotContains(ITPThirdPartyDataForSpecificFirstParty { firstPartyDomain, hasStorageAccess(firstPartyDomain, thirdPartyDomain), getMostRecentlyUpdatedTimestamp(thirdPartyDomain, firstPartyDomain) });
    }
    return thirdPartyDataForSpecificFirstPartyDomains;
}

static bool hasBeenThirdParty(unsigned timesUnderFirstParty)
{
    return timesUnderFirstParty > 0;
}

Vector<ITPThirdPartyData> ResourceLoadStatisticsStore::aggregatedThirdPartyData() const
{
    ASSERT(!RunLoop::isMain());

    Vector<ITPThirdPartyData> thirdPartyDataList;
    const auto prevalentDomainsBindParameter = thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::All ? "%"_s : "1"_s;
    auto sortedStatistics = m_database.prepareStatement(joinSubStatisticsForSorting());
    if (!sortedStatistics
        || sortedStatistics->bindText(1, prevalentDomainsBindParameter)
        || sortedStatistics->bindText(2, "%"_s) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("aggregatedThirdPartyData: failed to bind parameters");
        return thirdPartyDataList;
    }
    while (sortedStatistics->step() == SQLITE_ROW) {
        if (hasBeenThirdParty(sortedStatistics->columnInt(1))) {
            auto thirdPartyDomainID = sortedStatistics->columnInt(0);
            auto thirdPartyDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(getDomainStringFromDomainID(thirdPartyDomainID));
            thirdPartyDataList.append(ITPThirdPartyData { thirdPartyDomain, getThirdPartyDataForSpecificFirstPartyDomains(thirdPartyDomainID, thirdPartyDomain) });
        }
    }
    return thirdPartyDataList;
}

void ResourceLoadStatisticsStore::incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&& domains)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET dataRecordsRemoved = dataRecordsRemoved + 1 WHERE registrableDomain IN ("_s, buildList(domains), ')'));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("incrementRecordsDeletedCountForDomains: failed to step statement");
}

static constexpr unsigned maxNumberOfRecursiveCallsInRedirectTraceBack { 50 };

unsigned ResourceLoadStatisticsStore::recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(unsigned primaryDomainID, StdSet<unsigned>& nonPrevalentRedirectionSources, unsigned numberOfRecursiveCalls)
{
    ASSERT(!RunLoop::isMain());

    if (numberOfRecursiveCalls >= maxNumberOfRecursiveCallsInRedirectTraceBack) {
        ITP_RELEASE_LOG_ERROR("recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain: hit %u recursive calls in redirect backtrace", maxNumberOfRecursiveCallsInRedirectTraceBack);
        return numberOfRecursiveCalls;
    }

    ++numberOfRecursiveCalls;

    StdSet<unsigned> newlyIdentifiedDomains;
    auto findSubresources = m_database.prepareStatement("SELECT SubresourceUniqueRedirectsFrom.fromDomainID from SubresourceUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsFrom.fromDomainID WHERE subresourceDomainID = ? AND ObservedDomains.isPrevalent = 0"_s);
    if (!findSubresources || findSubresources->bindInt(1, primaryDomainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain: failed to bind parameter for findSubresources");
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
        ITP_RELEASE_LOG_DATABASE_ERROR("recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain: failed to bind parameter for findTopFrames");
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

void ResourceLoadStatisticsStore::markAsPrevalentIfHasRedirectedToPrevalent()
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

    auto markPrevalentStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN ("_s, buildList(prevalentDueToRedirect), ')'));
    if (!markPrevalentStatement || markPrevalentStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("markAsPrevalentIfHasRedirectedToPrevalent: failed to step statement");
}

HashMap<unsigned, ResourceLoadStatisticsStore::NotVeryPrevalentResources> ResourceLoadStatisticsStore::findNotVeryPrevalentResources()
{
    ASSERT(!RunLoop::isMain());

    HashMap<unsigned, NotVeryPrevalentResources> results;
    auto notVeryPrevalentResourcesStatement = m_database.prepareStatement("SELECT domainID, registrableDomain, isPrevalent FROM ObservedDomains WHERE isVeryPrevalent = 0"_s);
    if (notVeryPrevalentResourcesStatement) {
        while (notVeryPrevalentResourcesStatement->step() == SQLITE_ROW) {
            unsigned key = static_cast<unsigned>(notVeryPrevalentResourcesStatement->columnInt(0));
            ASSERT(key);
            if (!key)
                continue;
            NotVeryPrevalentResources value({ RegistrableDomain::uncheckedCreateFromRegistrableDomainString(notVeryPrevalentResourcesStatement->columnText(1))
                , notVeryPrevalentResourcesStatement->columnInt(2) ? ResourceLoadPrevalence::High : ResourceLoadPrevalence::Low , 0, 0, 0, 0 });
            results.add(key, WTFMove(value));
        }
    }

    auto domainIDsOfInterest = buildList(results.keys());

    auto subresourceUnderTopFrameDomainsStatement = m_database.prepareStatementSlow(makeString("SELECT subresourceDomainID, COUNT(topFrameDomainID) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID IN ("_s, domainIDsOfInterest, ") GROUP BY subresourceDomainID"_s));
    if (subresourceUnderTopFrameDomainsStatement) {
        while (subresourceUnderTopFrameDomainsStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement->columnInt(0));
            ASSERT(domainID);
            if (!domainID)
                continue;
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUnderTopFrameDomainsCount = static_cast<unsigned>(subresourceUnderTopFrameDomainsStatement->columnInt(1));
        }
    }

    auto subresourceUniqueRedirectsToCountStatement = m_database.prepareStatementSlow(makeString("SELECT subresourceDomainID, COUNT(toDomainID) FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID IN ("_s, domainIDsOfInterest, ") GROUP BY subresourceDomainID"_s));
    if (subresourceUniqueRedirectsToCountStatement) {
        while (subresourceUniqueRedirectsToCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement->columnInt(0));
            ASSERT(domainID);
            if (!domainID)
                continue;
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subresourceUniqueRedirectsToCount = static_cast<unsigned>(subresourceUniqueRedirectsToCountStatement->columnInt(1));
        }
    }

    auto subframeUnderTopFrameDomainsCountStatement = m_database.prepareStatementSlow(makeString("SELECT subframeDomainID, COUNT(topFrameDomainID) FROM SubframeUnderTopFrameDomains WHERE subframeDomainID IN ("_s, domainIDsOfInterest, ") GROUP BY subframeDomainID"_s));
    if (subframeUnderTopFrameDomainsCountStatement) {
        while (subframeUnderTopFrameDomainsCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement->columnInt(0));
            ASSERT(domainID);
            if (!domainID)
                continue;
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.subframeUnderTopFrameDomainsCount = static_cast<unsigned>(subframeUnderTopFrameDomainsCountStatement->columnInt(1));
        }
    }

    auto topFrameUniqueRedirectsToCountStatement = m_database.prepareStatementSlow(makeString("SELECT sourceDomainID, COUNT(toDomainID) FROM TopFrameUniqueRedirectsTo WHERE sourceDomainID IN ("_s, domainIDsOfInterest, ") GROUP BY sourceDomainID"_s));
    if (topFrameUniqueRedirectsToCountStatement) {
        while (topFrameUniqueRedirectsToCountStatement->step() == SQLITE_ROW) {
            unsigned domainID = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement->columnInt(0));
            ASSERT(domainID);
            if (!domainID)
                continue;
            auto result = results.find(domainID);
            if (result != results.end())
                result->value.topFrameUniqueRedirectsToCount = static_cast<unsigned>(topFrameUniqueRedirectsToCountStatement->columnInt(1));
        }
    }

    return results;
}

void ResourceLoadStatisticsStore::reclassifyResources()
{
    ASSERT(!RunLoop::isMain());

    auto notVeryPrevalentResources = findNotVeryPrevalentResources();
    if (notVeryPrevalentResources.isEmpty())
        return;

    auto transaction = beginTransactionIfNecessary();

    for (auto& resourceStatistic : notVeryPrevalentResources.values()) {
        if (shouldSkip(resourceStatistic.registrableDomain))
            continue;

        auto newPrevalence = m_resourceLoadStatisticsClassifier.calculateResourcePrevalence(resourceStatistic.subresourceUnderTopFrameDomainsCount, resourceStatistic.subresourceUniqueRedirectsToCount, resourceStatistic.subframeUnderTopFrameDomainsCount, resourceStatistic.topFrameUniqueRedirectsToCount, resourceStatistic.prevalence);
        if (newPrevalence != resourceStatistic.prevalence)
            setPrevalentResource(resourceStatistic.registrableDomain, newPrevalence);
    }
}

void ResourceLoadStatisticsStore::classifyPrevalentResources()
{
    ASSERT(!RunLoop::isMain());
    ensurePrevalentResourcesForDebugMode();
    markAsPrevalentIfHasRedirectedToPrevalent();
    reclassifyResources();
}

void ResourceLoadStatisticsStore::runIncrementalVacuumCommand()
{
    ASSERT(!RunLoop::isMain());
    m_database.runIncrementalVacuumCommand();
}

bool ResourceLoadStatisticsStore::hasStorageAccess(const TopFrameDomain& topFrameDomain, const SubFrameDomain& subFrameDomain) const
{
    auto scopedStatement = this->scopedStatement(m_storageAccessExistsStatement, storageAccessExistsQuery, "hasStorageAccess"_s);
    return relationshipExists(scopedStatement, domainID(subFrameDomain), topFrameDomain);
}

void ResourceLoadStatisticsStore::hasStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain, "hasStorageAccess"_s);
    if (!result.second)
        return;

    switch (cookieAccess(subFrameDomain, topFrameDomain, canRequestStorageAccessWithoutUserInteraction)) {
    case CookieAccess::CannotRequest:
        completionHandler(false);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        RunLoop::main().dispatch([store = Ref { store() }, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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

    RunLoop::main().dispatch([store = Ref { store() }, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callHasStorageAccessForFrameHandler(subFrameDomain, topFrameDomain, frameID.value(), pageID, [store, completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessScope scope, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain, "requestStorageAccess"_s);
    if (!subFrameStatus.second)
        return completionHandler(StorageAccessStatus::CannotRequestAccess);

    switch (cookieAccess(subFrameDomain, topFrameDomain, canRequestStorageAccessWithoutUserInteraction)) {
    case CookieAccess::CannotRequest:
        if (UNLIKELY(debugLoggingEnabled())) {
            ITP_DEBUG_MODE_RELEASE_LOG("Cannot grant storage access to %" PRIVATE_LOG_STRING " since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Error, makeString("[ITP] Cannot grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are blocked in third-party contexts and it has not received user interaction as first-party."_s));
        }
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        if (UNLIKELY(debugLoggingEnabled())) {
            ITP_DEBUG_MODE_RELEASE_LOG("No need to grant storage access to %" PRIVATE_LOG_STRING " since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies.", subFrameDomain.string().utf8().data());
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
            ITP_DEBUG_MODE_RELEASE_LOG("About to ask the user whether they want to grant storage access to %" PRIVATE_LOG_STRING " under %" PRIVATE_LOG_STRING " or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to ask the user whether they want to grant storage access to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "' or not."_s));
        }
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes) {
        if (UNLIKELY(debugLoggingEnabled())) {
            ITP_DEBUG_MODE_RELEASE_LOG("Storage access was granted to %" PRIVATE_LOG_STRING " under %" PRIVATE_LOG_STRING ".", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "'."_s));
        }
    }

    auto transactionScope = beginTransactionIfNecessary();

    auto incrementStorageAccess = m_database.prepareStatement("UPDATE ObservedDomains SET timesAccessedAsFirstPartyDueToStorageAccessAPI = timesAccessedAsFirstPartyDueToStorageAccessAPI + 1 WHERE domainID = ?"_s);
    if (!incrementStorageAccess
        || incrementStorageAccess->bindInt(1, *subFrameStatus.second) != SQLITE_OK
        || incrementStorageAccess->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("requestStorageAccess: failed to step statement");
        return completionHandler(StorageAccessStatus::CannotRequestAccess);
    }

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, scope, canRequestStorageAccessWithoutUserInteraction, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    if (UNLIKELY(debugLoggingEnabled())) {
        ITP_DEBUG_MODE_RELEASE_LOG("[Temporary combatibility fix] Storage access was granted for %" PRIVATE_LOG_STRING " under opener page from %" PRIVATE_LOG_STRING ", with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted for '"_s, domainInNeedOfStorageAccess.string(), "' under opener page from '"_s, openerDomain.string(), "', with user interaction in the opened window."_s));
    }

    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), std::nullopt, openerPageID, StorageAccessPromptWasShown::No, StorageAccessScope::PerPage, canRequestStorageAccessWithoutUserInteraction, [](StorageAccessWasGranted) { });
}

void ResourceLoadStatisticsStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto addGrant = [&, frameID, pageID, promptWasShown, scope] (SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler) mutable {
        if (promptWasShown == StorageAccessPromptWasShown::Yes) {
            auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain, "grantStorageAccess"_s);
            if (!subFrameStatus.second)
                return completionHandler(StorageAccessWasGranted::No);

            ASSERT(subFrameStatus.first == AddedRecord::No);
#if ASSERT_ENABLED
            if (canRequestStorageAccessWithoutUserInteraction == CanRequestStorageAccessWithoutUserInteraction::No)
                ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
#endif
            insertDomainRelationshipList(storageAccessUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *subFrameStatus.second);
        }

        grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, canRequestStorageAccessWithoutUserInteraction, WTFMove(completionHandler));
    };

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), workQueue = m_workQueue, store = Ref { store() }, addGrant = WTFMove(addGrant), completionHandler = WTFMove(completionHandler)]() mutable {

        std::optional<OrganizationStorageAccessPromptQuirk> additionalDomainGrants;
        CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };

        if (auto* networkSession = store->networkSession()) {
            if (auto* storageSession = networkSession->networkStorageSession()) {
                additionalDomainGrants = storageSession->storageAccessQuirkForDomainPair(subFrameDomain, topFrameDomain);
                canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
            }
        }
        workQueue->dispatch([weakThis = WTFMove(weakThis), additionalDomainGrants = crossThreadCopy(WTFMove(additionalDomainGrants)), subFrameDomain = crossThreadCopy(WTFMove(subFrameDomain)), topFrameDomain = crossThreadCopy(WTFMove(topFrameDomain)), addGrant = WTFMove(addGrant), canRequestStorageAccessWithoutUserInteraction, completionHandler = WTFMove(completionHandler)] () mutable {
            if (!weakThis) {
                completionHandler(StorageAccessWasGranted::No);
                return;
            }
            if (additionalDomainGrants) {
                for (auto&& [quirkTopFrameDomain, subFrameDomains] : additionalDomainGrants->quirkDomains) {
                    for (auto&& quirkSubFrameDomain : subFrameDomains) {
                        if (quirkTopFrameDomain == topFrameDomain && quirkSubFrameDomain == subFrameDomain)
                            continue;
                        addGrant(SubFrameDomain { quirkSubFrameDomain }, TopFrameDomain { quirkTopFrameDomain }, canRequestStorageAccessWithoutUserInteraction, [] (StorageAccessWasGranted) { });
                    }
                }
            }
            addGrant(WTFMove(subFrameDomain), WTFMove(topFrameDomain), canRequestStorageAccessWithoutUserInteraction, WTFMove(completionHandler));
        });
    });
}

void ResourceLoadStatisticsStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, StorageAccessScope scope, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessWasGranted::Yes);
        return;
    }

    if (promptWasShownNowOrEarlier == StorageAccessPromptWasShown::Yes) {
        auto transactionScope = beginTransactionIfNecessary();
#ifndef NDEBUG
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain, "grantStorageAccessInternal"_s);
        if (!subFrameStatus.second)
            return;

        ASSERT(subFrameStatus.first == AddedRecord::No);
#if ASSERT_ENABLED
        if (canRequestStorageAccessWithoutUserInteraction == CanRequestStorageAccessWithoutUserInteraction::No)
            ASSERT(hasHadUserInteraction(subFrameDomain, OperatingDatesWindow::Long));
#endif
        ASSERT(hasUserGrantedStorageAccessThroughPrompt(*subFrameStatus.second, topFrameDomain) == StorageAccessPromptWasShown::Yes);
#endif
        setUserInteraction(subFrameDomain, true, nowTime(m_timeAdvanceForTesting));
    }

    RunLoop::main().dispatch([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, store = Ref { store() }, scope, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, scope, [completionHandler = WTFMove(completionHandler), store](StorageAccessWasGranted wasGranted) mutable {
            store->statisticsQueue().dispatch([wasGranted, completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(wasGranted);
            });
        });
    });

}

void ResourceLoadStatisticsStore::grandfatherDataForDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(!RunLoop::isMain());
    if (domains.isEmpty())
        return;

    auto transactionScope = beginTransactionIfNecessary();

    for (auto& registrableDomain : domains) {
        auto result = ensureResourceStatisticsForRegistrableDomain(registrableDomain, "grandfatherDataForDomains"_s);
        UNUSED_PARAM(result);
    }

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET grandfathered = 1 WHERE registrableDomain IN ("_s, buildList(domains), ')'));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("grandfatherDataForDomains: failed to step statement");
}

Vector<RegistrableDomain> ResourceLoadStatisticsStore::ensurePrevalentResourcesForDebugMode()
{
    ASSERT(!RunLoop::isMain());

    if (!debugModeEnabled())
        return { };

    Vector<RegistrableDomain> primaryDomainsToBlock;
    primaryDomainsToBlock.reserveInitialCapacity(2);

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(debugStaticPrevalentResource(), "ensurePrevalentResourcesForDebugMode"_s).second)
        return { };

    setPrevalentResource(debugStaticPrevalentResource(), ResourceLoadPrevalence::High);
    primaryDomainsToBlock.append(debugStaticPrevalentResource());

    if (!debugManualPrevalentResource().isEmpty()) {
        if (!ensureResourceStatisticsForRegistrableDomain(debugManualPrevalentResource(), "ensurePrevalentResourcesForDebugMode"_s).second)
            return { };
        setPrevalentResource(debugManualPrevalentResource(), ResourceLoadPrevalence::High);
        primaryDomainsToBlock.append(debugManualPrevalentResource());

        if (debugLoggingEnabled()) {
            ITP_DEBUG_MODE_RELEASE_LOG("Did set %" PRIVATE_LOG_STRING " as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Did set '"_s, debugManualPrevalentResource().string(), "' as prevalent resource for the purposes of ITP Debug Mode."_s));
        }
    }

    return primaryDomainsToBlock;
}

void ResourceLoadStatisticsStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndTopFrameDomainsSameSite = targetDomain == topFrameDomain;
    bool areTargetAndSourceDomainsSameSite = targetDomain == sourceDomain;

    auto transactionScope = beginTransactionIfNecessary();

    bool statisticsWereUpdated = false;
    if (!isMainFrame && !(areTargetAndTopFrameDomainsSameSite || areTargetAndSourceDomainsSameSite)) {
        auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain, "logFrameNavigation"_s);
        if (!targetResult.second)
            return;

        updateLastSeen(targetDomain, ResourceLoadStatistics::reduceTimeResolution(nowTime(m_timeAdvanceForTesting)));
        insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *targetResult.second);
        statisticsWereUpdated = true;
    }

    if (!areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            bool wasNavigatedAfterShortDelayWithoutUserInteraction = !wasPotentiallyInitiatedByUser && delayAfterMainFrameDocumentLoad < parameters().minDelayAfterMainFrameDocumentLoadToNotBeARedirect;
            if (isRedirect || wasNavigatedAfterShortDelayWithoutUserInteraction) {
                auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain, "logFrameNavigation"_s);
                auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain, "logFrameNavigation"_s);
                if (!targetResult.second || !redirectingDomainResult.second)
                    return;

                insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
                if (isRedirect) {
                    insertDomainRelationshipList(topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);

                    if (UNLIKELY(debugLoggingEnabled())) {
                        ITP_DEBUG_MODE_RELEASE_LOG("Did set %" PUBLIC_LOG_STRING " as making a top frame redirect to %" PUBLIC_LOG_STRING ".", sourceDomain.string().utf8().data(), targetDomain.string().utf8().data());
                        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Did set '"_s, sourceDomain.string(), "' as making a top frame redirect to '"_s, targetDomain.string(), "'."_s));
                    }
                }
                insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
                statisticsWereUpdated = true;
            }
        } else if (isRedirect) {
            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain, "logFrameNavigation"_s);
            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain, "logFrameNavigation"_s);
            if (!targetResult.second || !redirectingDomainResult.second)
                return;

            insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ targetDomain }), *redirectingDomainResult.second);
            insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ sourceDomain }), *targetResult.second);
            statisticsWereUpdated = true;
        }
    }

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsStore::logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain& fromDomain, const NavigatedToDomain& toDomain, DidFilterKnownLinkDecoration didFilterKnownLinkDecoration)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(fromDomain != toDomain);

    auto transactionScope = beginTransactionIfNecessary();

    auto toDomainResult = ensureResourceStatisticsForRegistrableDomain(toDomain, "logCrossSiteLoadWithLinkDecoration"_s);
    if (!toDomainResult.second)
        return;

    insertDomainRelationshipList(topFrameLinkDecorationsFromQuery, HashSet<RegistrableDomain>({ fromDomain }), *toDomainResult.second);

    auto currentDataRemovalFrequency = dataRemovalFrequency(toDomain);
    if (currentDataRemovalFrequency == DataRemovalFrequency::Short)
        return;

    auto newDataRemovalFrequency = didFilterKnownLinkDecoration == DidFilterKnownLinkDecoration::Yes ? DataRemovalFrequency::Long : DataRemovalFrequency::Short;
    if (currentDataRemovalFrequency == newDataRemovalFrequency)
        return;

    if (isPrevalentResource(fromDomain))
        setIsScheduledForAllScriptWrittenStorageRemoval(toDomain, newDataRemovalFrequency);
}

void ResourceLoadStatisticsStore::clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain, "clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement"_s);
    if (!targetResult.second)
        return completionHandler();

    auto removeRedirectsToSinceSameSite = m_database.prepareStatement("DELETE FROM TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement WHERE sourceDomainID = ?"_s);
    if (!removeRedirectsToSinceSameSite
        || removeRedirectsToSinceSameSite->bindInt(1, *targetResult.second) != SQLITE_OK
        || removeRedirectsToSinceSameSite->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement: failed to step statement");

    completionHandler();
}

void ResourceLoadStatisticsStore::setUserInteraction(const RegistrableDomain& domain, bool hadUserInteraction, WallTime mostRecentInteraction)
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_mostRecentUserInteractionStatement, mostRecentUserInteractionQuery, "setUserInteraction"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, hadUserInteraction) != SQLITE_OK
        || scopedStatement->bindDouble(2, mostRecentInteraction.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindText(3, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("setUserInteraction: failed to step statement");
}

void ResourceLoadStatisticsStore::logUserInteraction(const TopFrameDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "logUserInteraction"_s).second)
        return completionHandler();

    bool didHavePreviousUserInteraction = hasHadUserInteraction(domain, OperatingDatesWindow::Long);
    setUserInteraction(domain, true, nowTime(m_timeAdvanceForTesting));

    if (didHavePreviousUserInteraction) {
        completionHandler();
        return;
    }
    updateCookieBlocking(WTFMove(completionHandler));
}

void ResourceLoadStatisticsStore::clearUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain, "clearUserInteraction"_s);
    if (!targetResult.second)
        return completionHandler();

    setUserInteraction(domain, false, { });

    auto removeStorageAccess = m_database.prepareStatement("DELETE FROM StorageAccessUnderTopFrameDomains WHERE domainID = ?"_s);
    if (!removeStorageAccess
        || removeStorageAccess->bindInt(1, *targetResult.second) != SQLITE_OK
        || removeStorageAccess->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("clearUserInteraction: failed to step statement");
        return completionHandler();
    }

    // Update cookie blocking unconditionally since a call to hasHadUserInteraction()
    // to check the previous user interaction status could call clearUserInteraction(),
    // blowing the call stack.
    updateCookieBlocking(WTFMove(completionHandler));
}

bool ResourceLoadStatisticsStore::hasHadUserInteraction(const RegistrableDomain& domain, OperatingDatesWindow operatingDatesWindow)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();
    auto scopedStatement = this->scopedStatement(m_hadUserInteractionStatement, hadUserInteractionQuery, "hasHadUserInteraction"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("hasHadUserInteraction: failed to step statement");
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

void ResourceLoadStatisticsStore::setTimeAdvanceForTesting(Seconds time)
{
    ASSERT(!RunLoop::isMain());
    constexpr Seconds secondsPerDay { 3600 * 24 };
    for (Seconds t = m_timeAdvanceForTesting; t <= time; t += secondsPerDay) {
        m_timeAdvanceForTesting = t;
        includeTodayAsOperatingDateIfNecessary();
    }
}

void ResourceLoadStatisticsStore::setPrevalentResource(const RegistrableDomain& domain, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    auto transactionScope = beginTransactionIfNecessary();

    auto registrableDomainID = domainID(domain);
    if (!registrableDomainID) {
        ITP_RELEASE_LOG_ERROR("setPrevalentResource: failed to find domain");
        return;
    }

    auto scopedUpdatePrevalentResourceStatement = this->scopedStatement(m_updatePrevalentResourceStatement, updatePrevalentResourceQuery, "setPrevalentResource"_s);
    if (!scopedUpdatePrevalentResourceStatement
        || scopedUpdatePrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedUpdatePrevalentResourceStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("setPrevalentResource: failed at to step scopedUpdatePrevalentResourceStatement");
        return;
    }

    auto scopedUpdateVeryPrevalentResourceStatement = this->scopedStatement(m_updateVeryPrevalentResourceStatement, updateVeryPrevalentResourceQuery, "setPrevalentResource updateVeryPrevalentResource"_s);
    if (newPrevalence == ResourceLoadPrevalence::VeryHigh) {
        if (!scopedUpdateVeryPrevalentResourceStatement
            || scopedUpdateVeryPrevalentResourceStatement->bindInt(1, 1) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->bindText(2, domain.string()) != SQLITE_OK
            || scopedUpdateVeryPrevalentResourceStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_DATABASE_ERROR("setPrevalentResource: failed at to step scopedUpdateVeryPrevalentResourceStatement");
            return;
        }
    }

    StdSet<unsigned> nonPrevalentRedirectionSources;
    recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(*registrableDomainID, nonPrevalentRedirectionSources, 0);
    setDomainsAsPrevalent(WTFMove(nonPrevalentRedirectionSources));
}

void ResourceLoadStatisticsStore::setDomainsAsPrevalent(StdSet<unsigned>&& domains)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToUpdateStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN ("_s, buildList(domains), ')'));
    if (!domainsToUpdateStatement || domainsToUpdateStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("setDomainsAsPrevalent: failed to step statement");
}

void ResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    if (m_dataRecordsBeingRemoved) {
        m_dataRecordRemovalCompletionHandlers.append([this, completionHandler = WTFMove(completionHandler)]() mutable {
            dumpResourceLoadStatistics(WTFMove(completionHandler));
        });
        return;
    }

    auto scopedStatement = this->scopedStatement(m_getAllDomainsStatement, getAllDomainsQuery, "dumpResourceLoadStatistics"_s);
    if (!scopedStatement)
        return completionHandler({ });

    Vector<String> domains;
    while (scopedStatement->step() == SQLITE_ROW)
        domains.append(scopedStatement->columnText(0));
    std::sort(domains.begin(), domains.end(), WTF::codePointCompareLessThan);

    StringBuilder result;
    result.append("Resource load statistics:\n\n"_s);
    for (auto& domain : domains)
        resourceToString(result, domain);

    auto thirdPartyData = aggregatedThirdPartyData();
    if (!thirdPartyData.isEmpty()) {
        result.append("\nITP Data:\n"_s);
        for (auto thirdParty : thirdPartyData)
            result.append(thirdParty.toString(), '\n');
    }
    completionHandler(result.toString());
}

int ResourceLoadStatisticsStore::predicateValueForDomain(SQLiteStatementAutoResetScope& predicateStatement, const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (!predicateStatement
        || predicateStatement->bindText(1, domain.string()) != SQLITE_OK
        || predicateStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("predicateValueForDomain: failed to step statement");
        return false;
    }
    return predicateStatement->columnInt(0);
}

bool ResourceLoadStatisticsStore::isPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    auto scopedStatement = this->scopedStatement(m_isPrevalentResourceStatement, isPrevalentResourceQuery, "isPrevalentResource"_s);
    return !!predicateValueForDomain(scopedStatement, domain);
}

bool ResourceLoadStatisticsStore::isVeryPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    auto scopedStatement = this->scopedStatement(m_isVeryPrevalentResourceStatement, isVeryPrevalentResourceQuery, "isVeryPrevalentResource"_s);
    return !!predicateValueForDomain(scopedStatement, domain);
}

DataRemovalFrequency ResourceLoadStatisticsStore::dataRemovalFrequency(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return DataRemovalFrequency::Never;

    auto scopedStatement = this->scopedStatement(m_isScheduledForAllButCookieDataRemovalStatement, isScheduledForAllButCookieDataRemovalQuery, "isScheduledForAllButCookieDataRemoval"_s);

    return toDataRemovalFrequency(predicateValueForDomain(scopedStatement, domain));
}

bool ResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_subresourceUnderTopFrameDomainExistsStatement, subresourceUnderTopFrameDomainExistsQuery, "isRegisteredAsSubresourceUnder"_s);
    return relationshipExists(scopedStatement, domainID(subresourceDomain), topFrameDomain);
}

bool ResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_subframeUnderTopFrameDomainExistsStatement, subframeUnderTopFrameDomainExistsQuery, "isRegisteredAsSubFrameUnder"_s);
    return relationshipExists(scopedStatement, domainID(subFrameDomain), topFrameDomain);
}

bool ResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(const RedirectedFromDomain& redirectedFromDomain, const RedirectedToDomain& redirectedToDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_subresourceUniqueRedirectsToExistsStatement, subresourceUniqueRedirectsToExistsQuery, "isRegisteredAsRedirectingTo"_s);
    return relationshipExists(scopedStatement, domainID(redirectedFromDomain), redirectedToDomain);
}

void ResourceLoadStatisticsStore::clearPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "clearPrevalentResource"_s).second)
        return;

    auto scopedStatement = this->scopedStatement(m_clearPrevalentResourceStatement, clearPrevalentResourceQuery, "clearPrevalentResource"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("clearPrevalentResource: failed to step statement");
}

void ResourceLoadStatisticsStore::setGrandfathered(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setGrandfathered"_s).second)
        return;

    auto scopedStatement = this->scopedStatement(m_updateGrandfatheredStatement, updateGrandfatheredQuery, "setGrandfathered"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("setGrandfathered: failed to step statement");
}

void ResourceLoadStatisticsStore::setIsScheduledForAllScriptWrittenStorageRemoval(const RegistrableDomain& domain, DataRemovalFrequency dataRemovalFrequency)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();
    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setIsScheduledForAllScriptWrittenStorageRemoval"_s).second)
        return;

    auto scopedStatement = this->scopedStatement(m_updateIsScheduledForAllButCookieDataRemovalStatement, updateIsScheduledForAllButCookieDataRemovalQuery, "setIsScheduledForAllScriptWrittenStorageRemoval"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, static_cast<int>(dataRemovalFrequency)) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("setIsScheduledForAllScriptWrittenStorageRemoval: failed to step statement");
    }
}

void ResourceLoadStatisticsStore::setMostRecentWebPushInteractionTime(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto time = nowTime(m_timeAdvanceForTesting);

    auto transactionScope = beginTransactionIfNecessary();
    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setMostRecentWebPushInteractionTime"_s).second)
        return;

    auto scopedStatement = this->scopedStatement(m_updateMostRecentWebPushInteractionTimeStatement, updateMostRecentWebPushInteractionTimeQuery, "setMostRecentWebPushInteractionTime"_s);
    if (!scopedStatement
        || scopedStatement->bindDouble(1, time.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("setMostRecentWebPushInteractionTime: failed to step statement");
    }
}

Seconds ResourceLoadStatisticsStore::getMostRecentlyUpdatedTimestamp(const RegistrableDomain& subDomain, const TopFrameDomain& topFrameDomain) const
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
        ITP_RELEASE_LOG_DATABASE_ERROR("getMostRecentlyUpdatedTimestamp: failed to step statement");
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };
    }
    if (scopedStatement->step() != SQLITE_ROW)
        return Seconds { ResourceLoadStatistics::NoExistingTimestamp  };

    return Seconds { scopedStatement->columnDouble(0) };
}

bool ResourceLoadStatisticsStore::isGrandfathered(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_isGrandfatheredStatement, isGrandfatheredQuery, "isGrandfathered"_s);
    return !!predicateValueForDomain(scopedStatement, domain);
}

void ResourceLoadStatisticsStore::setSubframeUnderTopFrameDomain(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(subFrameDomain, "setSubframeUnderTopFrameDomain"_s);
    if (!result.second)
        return;

    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subframeUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);
}

void ResourceLoadStatisticsStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain, "setSubresourceUnderTopFrameDomain"_s);
    if (!result.second)
        return;

    // For consistency, make sure we also have a statistics entry for the top frame domain.
    insertDomainRelationshipList(subresourceUnderTopFrameDomainsQuery, HashSet<RegistrableDomain>({ topFrameDomain }), *result.second);
}

void ResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain, "setSubresourceUniqueRedirectTo"_s);
    if (!result.second)
        return;

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain, "setSubresourceUniqueRedirectFrom"_s);
    if (!result.second)
        return;

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(subresourceUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain, "setTopFrameUniqueRedirectTo"_s);
    if (!result.second)
        return;

    insertDomainRelationshipList(topFrameUniqueRedirectsToQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
    insertDomainRelationshipList(topFrameUniqueRedirectsToSinceSameSiteStrictEnforcementQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

void ResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain, "setTopFrameUniqueRedirectFrom"_s);
    if (!result.second)
        return;

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    insertDomainRelationshipList(topFrameUniqueRedirectsFromQuery, HashSet<RegistrableDomain>({ redirectDomain }), *result.second);
}

std::pair<ResourceLoadStatisticsStore::AddedRecord, std::optional<unsigned>> ResourceLoadStatisticsStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain, ASCIILiteral reason)
{
    ASSERT(!RunLoop::isMain());

    {
        auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "ensureResourceStatisticsForRegistrableDomain"_s);
        if (!scopedStatement
            || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
            ITP_RELEASE_LOG_DATABASE_ERROR("ensureResourceStatisticsForRegistrableDomain: reason %" PUBLIC_LOG_STRING ", failed to bind parameter", reason.characters());
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
        ITP_RELEASE_LOG_ERROR("ensureResourceStatisticsForRegistrableDomain: reason %" PUBLIC_LOG_STRING ", failed to insert observed domain", reason.characters());
        return { AddedRecord::No, std::nullopt };
    }

    return { AddedRecord::Yes, domainID(domain).value() };
}

void ResourceLoadStatisticsStore::clearDatabaseContents()
{
    m_database.clearAllTables();

    if (!createSchema())
        ITP_RELEASE_LOG_ERROR("clearDatabaseContents: failed to create schema");
}

void ResourceLoadStatisticsStore::removeDataForDomain(const RegistrableDomain& domain)
{
    auto domainIDToRemove = domainID(domain);
    if (!domainIDToRemove)
        return;

    auto scopedStatement = this->scopedStatement(m_removeAllDataStatement, removeAllDataQuery, "removeDataForDomain"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, *domainIDToRemove) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("removeDataForDomain: failed to step statement");
}

Vector<RegistrableDomain> ResourceLoadStatisticsStore::allDomains() const
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

HashMap<RegistrableDomain, WallTime> ResourceLoadStatisticsStore::allDomainsWithLastAccessedTime() const
{
    ASSERT(!RunLoop::isMain());

    HashMap<RegistrableDomain, WallTime> result;
    for (auto& domainData : domains()) {
        auto lastAccessedTime = std::max(domainData.mostRecentUserInteractionTime, domainData.mostRecentWebPushInteractionTime);
        result.add(WTFMove(domainData.registrableDomain), lastAccessedTime);
    }

    return result;
}

void ResourceLoadStatisticsStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    clearDatabaseContents();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator] { });

    auto registrableDomainsToBlockAndDeleteCookiesFor = ensurePrevalentResourcesForDebugMode();
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { }, { }, { } };
    updateCookieBlockingForDomains(WTFMove(domainsToBlock), [callbackAggregator] { });
}

bool ResourceLoadStatisticsStore::areAllThirdPartyCookiesBlockedUnder(const TopFrameDomain& topFrameDomain)
{
    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::All)
        return true;

    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction && !hasHadUserInteraction(topFrameDomain, OperatingDatesWindow::Long))
        return true;

    return false;
}

CookieAccess ResourceLoadStatisticsStore::cookieAccess(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction)
{
    ASSERT(!RunLoop::isMain());

    auto statement = m_database.prepareStatement("SELECT isPrevalent, hadUserInteraction FROM ObservedDomains WHERE registrableDomain = ?"_s);
    if (!statement || statement->bindText(1, subresourceDomain.string()) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("cookieAccess: failed to bind parameter");
        return CookieAccess::CannotRequest;
    }

    bool hasNoEntry = statement->step() != SQLITE_ROW;
    bool isPrevalent = !hasNoEntry && !!statement->columnInt(0);
    bool hadUserInteraction = !hasNoEntry && statement->columnInt(1);

    if (!areAllThirdPartyCookiesBlockedUnder(topFrameDomain) && !isPrevalent)
        return CookieAccess::BasedOnCookiePolicy;

    if (canRequestStorageAccessWithoutUserInteraction == CanRequestStorageAccessWithoutUserInteraction::No && !hadUserInteraction)
        return CookieAccess::CannotRequest;

    return CookieAccess::OnlyIfGranted;
}

StorageAccessPromptWasShown ResourceLoadStatisticsStore::hasUserGrantedStorageAccessThroughPrompt(unsigned requestingDomainID, const RegistrableDomain& firstPartyDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(firstPartyDomain, "hasUserGrantedStorageAccessThroughPrompt"_s);
    if (!result.second)
        return StorageAccessPromptWasShown::No;

    auto firstPartyPrimaryDomainID = *result.second;

    auto statement = m_database.prepareStatement("SELECT COUNT(*) FROM StorageAccessUnderTopFrameDomains WHERE domainID = ? AND topLevelDomainID = ?"_s);
    if (!statement
        || statement->bindInt(1, requestingDomainID) != SQLITE_OK
        || statement->bindInt(2, firstPartyPrimaryDomainID) != SQLITE_OK
        || statement->step() != SQLITE_ROW)
        return StorageAccessPromptWasShown::No;

    return !!statement->columnInt(0) ? StorageAccessPromptWasShown::Yes : StorageAccessPromptWasShown::No;
}

Vector<RegistrableDomain> ResourceLoadStatisticsStore::domainsToBlockAndDeleteCookiesFor() const
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

Vector<RegistrableDomain> ResourceLoadStatisticsStore::domainsToBlockButKeepCookiesFor() const
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

Vector<RegistrableDomain> ResourceLoadStatisticsStore::domainsWithUserInteractionAsFirstParty() const
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

HashMap<TopFrameDomain, Vector<SubResourceDomain>> ResourceLoadStatisticsStore::domainsWithStorageAccess() const
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>> results;
    auto statement = m_database.prepareStatement("SELECT subFrameDomain, registrableDomain FROM (SELECT o.registrableDomain as subFrameDomain, s.topLevelDomainID as topLevelDomainID FROM ObservedDomains as o INNER JOIN StorageAccessUnderTopFrameDomains as s WHERE o.domainID = s.domainID) as z INNER JOIN ObservedDomains ON domainID = z.topLevelDomainID;"_s);
    if (!statement)
        return results;

    while (statement->step() == SQLITE_ROW)
        results.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(1)), Vector<SubResourceDomain> { }).iterator->value.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(0)));

    return results;
}

void ResourceLoadStatisticsStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
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
        debugLogDomainsInBatches("Applying cross-site tracking restrictions"_s, domainsToBlock);

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, store = Ref { store() }, domainsToBlock = crossThreadCopy(WTFMove(domainsToBlock)), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();

                if (!weakThis)
                    return;

                if (UNLIKELY(weakThis->debugLoggingEnabled())) {
                    ITP_DEBUG_MODE_RELEASE_LOG("Done applying cross-site tracking restrictions.");
                    weakThis->debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Done applying cross-site tracking restrictions."_s);
                }
            });
        });
    });
}

Vector<ResourceLoadStatisticsStore::DomainData> ResourceLoadStatisticsStore::domains() const
{
    ASSERT(!RunLoop::isMain());

    Vector<DomainData> results;
    auto statement = m_database.prepareStatement("SELECT domainID, registrableDomain, mostRecentUserInteractionTime, mostRecentWebPushInteractionTime, hadUserInteraction, grandfathered, isScheduledForAllButCookieDataRemoval, countOfTopFrameRedirects FROM ObservedDomains LEFT JOIN (SELECT sourceDomainID, COUNT(*) as countOfTopFrameRedirects from TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement GROUP BY sourceDomainID) as z ON z.sourceDomainID = domainID"_s);
    if (!statement)
        return results;

    while (statement->step() == SQLITE_ROW) {
        results.append({ static_cast<unsigned>(statement->columnInt(0))
            , RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement->columnText(1))
            , WallTime::fromRawSeconds(statement->columnDouble(2))
            , WallTime::fromRawSeconds(statement->columnDouble(3))
            , !!statement->columnInt(4)
            , !!statement->columnInt(5)
            , toDataRemovalFrequency(statement->columnInt(6))
            , static_cast<unsigned>(statement->columnInt(7))
        });
    }

    return results;
}

void ResourceLoadStatisticsStore::clearGrandfathering(Vector<unsigned>&& domainIDsToClear)
{
    ASSERT(!RunLoop::isMain());

    if (domainIDsToClear.isEmpty())
        return;

    auto listToClear = buildList(domainIDsToClear);

    auto clearGrandfatheringStatement = m_database.prepareStatementSlow(makeString("UPDATE ObservedDomains SET grandfathered = 0 WHERE domainID IN ("_s, listToClear, ')'));
    if (!clearGrandfatheringStatement)
        return;

    if (clearGrandfatheringStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("clearGrandfathering: failed to step statement");
}

bool ResourceLoadStatisticsStore::hasHadRecentWebPushInteraction(const DomainData& resourceStatistic) const
{
    return resourceStatistic.mostRecentWebPushInteractionTime && !hasStatisticsExpired(resourceStatistic.mostRecentWebPushInteractionTime, OperatingDatesWindow::Long);
}

bool ResourceLoadStatisticsStore::hasHadUnexpiredRecentUserInteraction(const DomainData& resourceStatistic, OperatingDatesWindow operatingDatesWindow)
{
    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic.mostRecentUserInteractionTime, operatingDatesWindow)) {

        // Drop privacy sensitive data if we no longer need it.
        if (operatingDatesWindow == OperatingDatesWindow::Long)
            clearUserInteraction(resourceStatistic.registrableDomain, [] { });

        return false;
    }

    return resourceStatistic.hadUserInteraction;
}

bool ResourceLoadStatisticsStore::shouldRemoveAllWebsiteDataFor(const DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
{
    return isPrevalentResource(resourceStatistic.registrableDomain) && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long) && (!shouldCheckForGrandfathering || !resourceStatistic.grandfathered);
}

bool ResourceLoadStatisticsStore::shouldRemoveAllButCookiesFor(const DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
{
    bool isRemovalEnabled = firstPartyWebsiteDataRemovalMode() != FirstPartyWebsiteDataRemovalMode::None || resourceStatistic.dataRemovalFrequency != DataRemovalFrequency::Never;
    bool isResourceGrandfathered = shouldCheckForGrandfathering && resourceStatistic.grandfathered;

    OperatingDatesWindow window { };
    switch (firstPartyWebsiteDataRemovalMode()) {
    case FirstPartyWebsiteDataRemovalMode::AllButCookies:
        FALLTHROUGH;
    case FirstPartyWebsiteDataRemovalMode::None:
        window = resourceStatistic.dataRemovalFrequency == DataRemovalFrequency::Short ? OperatingDatesWindow::Short : OperatingDatesWindow::Long;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout:
        window = OperatingDatesWindow::ForLiveOnTesting;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout:
        window = OperatingDatesWindow::ForReproTesting;
    }

    return isRemovalEnabled && !isResourceGrandfathered && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, window);
}

bool ResourceLoadStatisticsStore::shouldEnforceSameSiteStrictFor(DomainData& resourceStatistic, bool shouldCheckForGrandfathering)
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

std::optional<WallTime> ResourceLoadStatisticsStore::mostRecentUserInteractionTime(const DomainData& statistic)
{
    if (statistic.mostRecentUserInteractionTime.secondsSinceEpoch().value() <= 0)
        return std::nullopt;

    return statistic.mostRecentUserInteractionTime;
}

RegistrableDomainsToDeleteOrRestrictWebsiteDataFor ResourceLoadStatisticsStore::registrableDomainsToDeleteOrRestrictWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    auto now = nowTime(m_timeAdvanceForTesting);
    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > now;
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    auto oldestUserInteraction = now;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor toDeleteOrRestrictFor;

    auto transactionScope = beginTransactionIfNecessary();

    Vector<DomainData> domains = this->domains();
    Vector<unsigned> domainIDsToClearGrandfathering;
    for (auto& statistic : domains) {
        // Domains permanently configured to be exempt. This can be home screen web applications, app-bound domains, or similar.
        if (shouldExemptFromWebsiteDataDeletion(statistic.registrableDomain))
            continue;

        // Domains with WebPush that the user interacts with continuously should keep their data.
        if (hasHadRecentWebPushInteraction(statistic))
            continue;

        if (auto mostRecentUserInteractionTime = this->mostRecentUserInteractionTime(statistic))
            oldestUserInteraction = std::min(oldestUserInteraction, *mostRecentUserInteractionTime);

        if (shouldRemoveAllWebsiteDataFor(statistic, shouldCheckForGrandfathering)) {
            toDeleteOrRestrictFor.domainsToDeleteAllCookiesFor.append(statistic.registrableDomain);
            toDeleteOrRestrictFor.domainsToDeleteAllScriptWrittenStorageFor.append(statistic.registrableDomain);
        } else {
            if (shouldRemoveAllButCookiesFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToDeleteAllScriptWrittenStorageFor.append(statistic.registrableDomain);
                setIsScheduledForAllScriptWrittenStorageRemoval(statistic.registrableDomain, DataRemovalFrequency::Never);
            }
            if (shouldEnforceSameSiteStrictFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToEnforceSameSiteStrictFor.append(statistic.registrableDomain);

                if (UNLIKELY(debugLoggingEnabled())) {
                    ITP_DEBUG_MODE_RELEASE_LOG("Scheduled %" PUBLIC_LOG_STRING " to have its cookies set to SameSite=strict.", statistic.registrableDomain.string().utf8().data());
                    debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Scheduled '"_s, statistic.registrableDomain.string(), "' to have its cookies set to SameSite=strict'."_s));
                }
            }
        }
        if (shouldClearGrandfathering && statistic.grandfathered)
            domainIDsToClearGrandfathering.append(statistic.domainID);
    }

    // Give the user enough time to interact with websites until we remove non-cookie website data.
    if (!parameters().isRunningTest && now - oldestUserInteraction < parameters().minimumTimeBetweenDataRecordsRemoval)
        toDeleteOrRestrictFor.domainsToDeleteAllScriptWrittenStorageFor.clear();

    clearGrandfathering(WTFMove(domainIDsToClearGrandfathering));

    return toDeleteOrRestrictFor;
}

void ResourceLoadStatisticsStore::pruneStatisticsIfNeeded()
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
        ITP_RELEASE_LOG_DATABASE_ERROR("pruneStatisticsIfNeeded: failed to bind parameter");
        return;
    }

    Vector<unsigned> entriesToPrune;
    while (recordsToPrune->step() == SQLITE_ROW)
        entriesToPrune.append(recordsToPrune->columnInt(0));

    auto listToPrune = buildList(entriesToPrune);

    auto pruneCommand = m_database.prepareStatementSlow(makeString("DELETE from ObservedDomains WHERE domainID IN ("_s, listToPrune, ')'));
    if (!pruneCommand || pruneCommand->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("pruneStatisticsIfNeeded: failed to step statement");
}

void ResourceLoadStatisticsStore::updateLastSeen(const RegistrableDomain& domain, WallTime lastSeen)
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_updateLastSeenStatement, updateLastSeenQuery, "updateLastSeen"_s);
    if (!scopedStatement
        || scopedStatement->bindDouble(1, lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("updateLastSeen: failed to step statement");
}

void ResourceLoadStatisticsStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setLastSeen"_s).second)
        return;

    updateLastSeen(domain, WallTime::fromRawSeconds(seconds.seconds()));
}

void ResourceLoadStatisticsStore::setPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setPrevalentResource"_s).second)
        return;

    setPrevalentResource(domain, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsStore::setVeryPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;

    auto transactionScope = beginTransactionIfNecessary();

    if (!ensureResourceStatisticsForRegistrableDomain(domain, "setVeryPrevalentResource"_s).second)
        return;

    setPrevalentResource(domain, ResourceLoadPrevalence::VeryHigh);
}

void ResourceLoadStatisticsStore::updateDataRecordsRemoved(const RegistrableDomain& domain, int value)
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_updateDataRecordsRemovedStatement, updateDataRecordsRemovedQuery, "updateDataRecordsRemoved"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, value) != SQLITE_OK
        || scopedStatement->bindText(2, domain.string()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("updateDataRecordsRemoved: failed to step statement");
}

bool ResourceLoadStatisticsStore::isCorrectSubStatisticsCount(const RegistrableDomain& subframeDomain, const TopFrameDomain& topFrameDomain)
{
    auto subFrameUnderTopFrameCount = m_database.prepareStatement(countSubframeUnderTopFrameQuery);
    auto subresourceUnderTopFrameCount = m_database.prepareStatement(countSubresourceUnderTopFrameQuery);
    auto subresourceUniqueRedirectsTo = m_database.prepareStatement(countSubresourceUniqueRedirectsToQuery);

    if (!subFrameUnderTopFrameCount || !subresourceUnderTopFrameCount || !subresourceUniqueRedirectsTo) {
        ITP_RELEASE_LOG_DATABASE_ERROR("isCorrectSubStatisticsCount: failed to prepare statement");
        return false;
    }

    if (subFrameUnderTopFrameCount->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subFrameUnderTopFrameCount->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUnderTopFrameCount->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo->bindInt(1, domainID(subframeDomain).value()) != SQLITE_OK
        || subresourceUniqueRedirectsTo->bindInt(2, domainID(topFrameDomain).value()) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("isCorrectSubStatisticsCount: failed to bind parameters");
        return false;
    }

    if (subFrameUnderTopFrameCount->step() != SQLITE_ROW
        || subresourceUnderTopFrameCount->step() != SQLITE_ROW
        || subresourceUniqueRedirectsTo->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("isCorrectSubStatisticsCount: failed to step statement");
        return false;
    }

    return (subFrameUnderTopFrameCount->columnInt(0) == 1 && subresourceUnderTopFrameCount->columnInt(0) == 1 && subresourceUniqueRedirectsTo->columnInt(0) == 1);
}

static void appendBoolean(StringBuilder& builder, ASCIILiteral label, bool flag)
{
    builder.append("    "_s, label, ": "_s, flag ? "Yes"_s : "No"_s);
}

static void appendNextEntry(StringBuilder& builder, const String& entry)
{
    builder.append("        "_s, entry, '\n');
}

String ResourceLoadStatisticsStore::getDomainStringFromDomainID(unsigned domainID) const
{
    auto result = emptyString();

    auto scopedStatement = this->scopedStatement(m_domainStringFromDomainIDStatement, domainStringFromDomainIDQuery, "getDomainStringFromDomainID"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, domainID) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("getDomainStringFromDomainID: failed to bind parameter");
        return result;
    }

    if (scopedStatement->step() == SQLITE_ROW)
        result = m_domainStringFromDomainIDStatement->columnText(0);

    return result;
}

ASCIILiteral ResourceLoadStatisticsStore::getSubStatisticStatement(ASCIILiteral tableName) const
{
    if (tableName == "StorageAccessUnderTopFrameDomains"_s)
        return "SELECT topLevelDomainID from StorageAccessUnderTopFrameDomains WHERE domainID = ?"_s;
    if (tableName == "TopFrameUniqueRedirectsTo"_s)
        return "SELECT toDomainID from TopFrameUniqueRedirectsTo WHERE sourceDomainID = ?"_s;
    if (tableName == "TopFrameUniqueRedirectsFrom"_s)
        return "SELECT fromDomainID from TopFrameUniqueRedirectsFrom WHERE targetDomainID = ?"_s;
    if (tableName == "TopFrameLinkDecorationsFrom"_s)
        return "SELECT fromDomainID from TopFrameLinkDecorationsFrom WHERE toDomainID = ?"_s;
    if (tableName == "TopFrameLoadedThirdPartyScripts"_s)
        return "SELECT subresourceDomainID from TopFrameLoadedThirdPartyScripts WHERE topFrameDomainID = ?"_s;
    if (tableName == "SubframeUnderTopFrameDomains"_s)
        return "SELECT topFrameDomainID from SubframeUnderTopFrameDomains WHERE subFrameDomainID = ?"_s;
    if (tableName == "SubresourceUnderTopFrameDomains"_s)
        return "SELECT topFrameDomainID from SubresourceUnderTopFrameDomains WHERE subresourceDomainID = ?"_s;
    if (tableName == "SubresourceUniqueRedirectsTo"_s)
        return "SELECT toDomainID from SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ?"_s;
    if (tableName == "SubresourceUniqueRedirectsFrom"_s)
        return "SELECT fromDomainID from SubresourceUniqueRedirectsFrom WHERE subresourceDomainID = ?"_s;

    return ""_s;
}

void ResourceLoadStatisticsStore::appendSubStatisticList(StringBuilder& builder, ASCIILiteral tableName, const String& domain) const
{
    auto query = getSubStatisticStatement(tableName);

    if (!query.length())
        return;

    auto data = m_database.prepareStatement(query);

    if (!data || data->bindInt(1, domainID(RegistrableDomain::uncheckedCreateFromHost(domain)).value()) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("appendSubStatisticList: failed to bind parameter");
        return;
    }

    if (data->step() != SQLITE_ROW)
        return;

    builder.append("    "_s, tableName, ":\n"_s);

    auto result = getDomainStringFromDomainID(data->columnInt(0));
    appendNextEntry(builder, result);

    while (data->step() == SQLITE_ROW) {
        result = getDomainStringFromDomainID(data->columnInt(0));
        appendNextEntry(builder, result);
    }
}

static bool hasHadRecentUserInteraction(WTF::Seconds interactionTimeSeconds, WallTime now)
{
    return interactionTimeSeconds > Seconds(0) && now.secondsSinceEpoch() - interactionTimeSeconds < 24_h;
}

void ResourceLoadStatisticsStore::resourceToString(StringBuilder& builder, const String& domain) const
{
    auto scopedStatement = this->scopedStatement(m_getResourceDataByDomainNameStatement, getResourceDataByDomainNameQuery, "resourceToString"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("resourceToString: failed to step statement");
        return;
    }

    builder.append("Registrable domain: "_s, domain, '\n');

    // User interaction
    appendBoolean(builder, "hadUserInteraction"_s, m_getResourceDataByDomainNameStatement->columnInt(HadUserInteractionIndex));
    builder.append('\n');
    builder.append("    mostRecentUserInteraction: "_s);
    if (hasHadRecentUserInteraction(Seconds(m_getResourceDataByDomainNameStatement->columnDouble(MostRecentUserInteractionTimeIndex)), nowTime(m_timeAdvanceForTesting)))
        builder.append("within 24 hours"_s);
    else
        builder.append("-1"_s);
    builder.append('\n');
    appendBoolean(builder, "grandfathered"_s, m_getResourceDataByDomainNameStatement->columnInt(GrandfatheredIndex));
    builder.append('\n');

    // Storage access
    appendSubStatisticList(builder, "StorageAccessUnderTopFrameDomains"_s, domain);

    // Top frame stats
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsTo"_s, domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement"_s, domain);
    appendSubStatisticList(builder, "TopFrameUniqueRedirectsFrom"_s, domain);
    appendSubStatisticList(builder, "TopFrameLinkDecorationsFrom"_s, domain);
    appendSubStatisticList(builder, "TopFrameLoadedThirdPartyScripts"_s, domain);

    auto dataRemovalFrequencyValue = m_getResourceDataByDomainNameStatement->columnInt(IsScheduledForAllButCookieDataRemovalIndex);
    builder.append("    DataRemovalFrequency: "_s, dataRemovalFrequencyToString(toDataRemovalFrequency(dataRemovalFrequencyValue)), '\n');

    // Subframe stats
    appendSubStatisticList(builder, "SubframeUnderTopFrameDomains"_s, domain);

    // Subresource stats
    appendSubStatisticList(builder, "SubresourceUnderTopFrameDomains"_s, domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsTo"_s, domain);
    appendSubStatisticList(builder, "SubresourceUniqueRedirectsFrom"_s, domain);

    // Prevalent Resource
    appendBoolean(builder, "isPrevalentResource"_s, m_getResourceDataByDomainNameStatement->columnInt(IsPrevalentIndex));
    builder.append('\n');
    appendBoolean(builder, "isVeryPrevalentResource"_s, m_getResourceDataByDomainNameStatement->columnInt(IsVeryPrevalentIndex));
    builder.append('\n');
    builder.append("    dataRecordsRemoved: "_s, m_getResourceDataByDomainNameStatement->columnInt(DataRecordsRemovedIndex));
    builder.append('\n');
}

bool ResourceLoadStatisticsStore::domainIDExistsInDatabase(int domainID)
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
        ITP_RELEASE_LOG_DATABASE_ERROR("domainIDExistsInDatabase: failed to bind parameters");
        return false;
    }

    if (m_linkDecorationExistsStatement->step() != SQLITE_ROW
        || m_scriptLoadExistsStatement->step() != SQLITE_ROW
        || m_subFrameExistsStatement->step() != SQLITE_ROW
        || m_subResourceExistsStatement->step() != SQLITE_ROW
        || m_uniqueRedirectExistsStatement->step() != SQLITE_ROW
        || m_observedDomainsExistsStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("domainIDExistsInDatabase: failed to step statement");
        return false;
    }

    return m_linkDecorationExistsStatement->columnInt(0) || m_scriptLoadExistsStatement->columnInt(0) || m_subFrameExistsStatement->columnInt(0) || m_subResourceExistsStatement->columnInt(0) || m_uniqueRedirectExistsStatement->columnInt(0) || m_observedDomainsExistsStatement->columnInt(0);
}

void ResourceLoadStatisticsStore::updateOperatingDatesParameters()
{
    auto countOperatingDatesStatement = m_database.prepareStatement("SELECT COUNT(*) FROM OperatingDates;"_s);
    auto getMostRecentOperatingDateStatement = m_database.prepareStatement("SELECT * FROM OperatingDates ORDER BY year DESC, month DESC, monthDay DESC LIMIT 1;"_s);
    auto getOperatingDateWindowStatement = m_database.prepareStatement("SELECT * FROM OperatingDates ORDER BY year DESC, month DESC, monthDay DESC LIMIT 1 OFFSET ?;"_s);

    if (!countOperatingDatesStatement || countOperatingDatesStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("updateOperatingDatesParameters: failed to step countOperatingDatesStatement");
        return;
    }

    m_operatingDatesSize = countOperatingDatesStatement->columnInt(0);

    if (!getMostRecentOperatingDateStatement || getMostRecentOperatingDateStatement->step() != SQLITE_ROW) {
        ITP_RELEASE_LOG_DATABASE_ERROR("updateOperatingDatesParameters: failed to step getMostRecentOperatingDateStatement");
        return;
    }
    m_mostRecentOperatingDate = OperatingDate(getMostRecentOperatingDateStatement->columnInt(0), getMostRecentOperatingDateStatement->columnInt(1), getMostRecentOperatingDateStatement->columnInt(2));

    if (!getOperatingDateWindowStatement) {
        ITP_RELEASE_LOG_DATABASE_ERROR("updateOperatingDatesParameters: failed to prepare getOperatingDateWindowStatement");
        return;
    }

    auto updateWindowOperatingDate = [&] (auto& memberOperatingDate, auto window) {
        if (m_operatingDatesSize <= window - 1)
            memberOperatingDate = std::nullopt;
        else {
            getOperatingDateWindowStatement->reset();
            if (getOperatingDateWindowStatement->bindInt(1, window - 1) != SQLITE_OK
                || getOperatingDateWindowStatement->step() != SQLITE_ROW) {
                ITP_RELEASE_LOG_DATABASE_ERROR("updateOperatingDatesParameters: failed to step getOperatingDateWindowStatement");
                return;
            }
            memberOperatingDate = OperatingDate(getOperatingDateWindowStatement->columnInt(0), getOperatingDateWindowStatement->columnInt(1), getOperatingDateWindowStatement->columnInt(2));
        }
    };

    updateWindowOperatingDate(m_shortWindowOperatingDate, operatingDatesWindowShort);
    updateWindowOperatingDate(m_longWindowOperatingDate, operatingDatesWindowLong);
}

void ResourceLoadStatisticsStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    auto today = OperatingDate::today(m_timeAdvanceForTesting);
    if (m_operatingDatesSize > 0) {
        if (today <= m_mostRecentOperatingDate)
            return;
    }

    auto transactionScope = beginTransactionIfNecessary();

    int rowsToPrune = m_operatingDatesSize - operatingDatesWindowLong + 1;
    if (rowsToPrune > 0) {
        auto deleteLeastRecentOperatingDateStatement = m_database.prepareStatement("DELETE FROM OperatingDates ORDER BY year, month, monthDay LIMIT ?;"_s);
        if (!deleteLeastRecentOperatingDateStatement
            || deleteLeastRecentOperatingDateStatement->bindInt(1, rowsToPrune) != SQLITE_OK
            || deleteLeastRecentOperatingDateStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_DATABASE_ERROR("includeTodayAsOperatingDateIfNecessary: failed to step deleteLeastRecentOperatingDateStatement");
            return;
        }
    }

    auto insertOperatingDateStatement = m_database.prepareStatement("INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
    if (!insertOperatingDateStatement
        || insertOperatingDateStatement->bindInt(1, today.year()) != SQLITE_OK
        || insertOperatingDateStatement->bindInt(2, today.month()) != SQLITE_OK
        || insertOperatingDateStatement->bindInt(3, today.monthDay()) != SQLITE_OK
        || insertOperatingDateStatement->step() != SQLITE_DONE) {
        ITP_RELEASE_LOG_DATABASE_ERROR("includeTodayAsOperatingDateIfNecessary: failed to step insertOperatingDateStatement");
        return;
    }

    updateOperatingDatesParameters();
}

bool ResourceLoadStatisticsStore::hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow operatingDatesWindow) const
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
        return nowTime(m_timeAdvanceForTesting) > mostRecentUserInteractionTime + operatingTimeWindowForLiveOnTesting;
    case OperatingDatesWindow::ForReproTesting:
        return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (this->parameters().timeToLiveUserInteraction) {
        if (nowTime(m_timeAdvanceForTesting) > mostRecentUserInteractionTime + this->parameters().timeToLiveUserInteraction.value())
            return true;
    }
    return false;
}

void ResourceLoadStatisticsStore::insertExpiredStatisticForTesting(const RegistrableDomain& domain, unsigned numberOfOperatingDaysPassed, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent)
{
    // Populate the Operating Dates table with enough days to require pruning.
    double daysAgoInSeconds = 0;

    auto transactionScope = beginTransactionIfNecessary();

    for (unsigned i = 1; i <= numberOfOperatingDaysPassed; i++) {
        double daysToSubtract = Seconds::fromHours(24 * i).value();
        daysAgoInSeconds = nowTime(m_timeAdvanceForTesting).secondsSinceEpoch().value() - daysToSubtract;
        auto dateToInsert = OperatingDate::fromWallTime(WallTime::fromRawSeconds(daysAgoInSeconds));

        auto insertOperatingDateStatement = m_database.prepareStatement("INSERT OR IGNORE INTO OperatingDates (year, month, monthDay) SELECT ?, ?, ?;"_s);
        if (!insertOperatingDateStatement
            || insertOperatingDateStatement->bindInt(1, dateToInsert.year()) != SQLITE_OK
            || insertOperatingDateStatement->bindInt(2, dateToInsert.month()) != SQLITE_OK
            || insertOperatingDateStatement->bindInt(3, dateToInsert.monthDay()) != SQLITE_OK
            || insertOperatingDateStatement->step() != SQLITE_DONE) {
            ITP_RELEASE_LOG_DATABASE_ERROR("insertExpiredStatisticForTesting: failed to step insertOperatingDateStatement");
            return;
        }
        insertOperatingDateStatement->reset();
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
        || scopedInsertObservedDomainStatement->bindInt(IsScheduledForAllButCookieDataRemovalIndex, isScheduledForAllButCookieDataRemoval) != SQLITE_OK
        || scopedInsertObservedDomainStatement->bindDouble(MostRecentWebPushInteractionTimeIndex, 0.0) != SQLITE_OK) {
        ITP_RELEASE_LOG_DATABASE_ERROR("insertExpiredStatisticForTesting: failed to step scopedInsertObservedDomainStatement");
        return;
    }

    if (scopedInsertObservedDomainStatement->step() != SQLITE_DONE)
        ITP_RELEASE_LOG_DATABASE_ERROR("insertExpiredStatisticForTesting: failed to step statement");
}
} // namespace WebKit

#undef ITP_RELEASE_LOG
#undef ITP_RELEASE_LOG_ERROR
