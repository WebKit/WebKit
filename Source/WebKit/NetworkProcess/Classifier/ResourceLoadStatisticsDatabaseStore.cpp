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
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/CallbackAggregator.h>
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

constexpr auto observedDomainCountQuery = "SELECT COUNT(*) FROM ObservedDomains"_s;
constexpr auto insertObservedDomainQuery = "INSERT INTO ObservedDomains (registrableDomain, lastSeen, hadUserInteraction,"
    "mostRecentUserInteractionTime, grandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, timesAccessedAsFirstPartyDueToUserInteraction,"
    "timesAccessedAsFirstPartyDueToStorageAccessAPI) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
constexpr auto insertTopLevelDomainQuery = "INSERT INTO TopLevelDomains VALUES (?)"_s;
constexpr auto domainIDFromStringQuery = "SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto storageAccessUnderTopFrameDomainsQuery = "INSERT INTO StorageAccessUnderTopFrameDomains (domainID, topLevelDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain == ?"_s;
constexpr auto topFrameUniqueRedirectsToQuery = "INSERT INTO TopFrameUniqueRedirectsTo (sourceDomainID, toDomainID) SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain == ?"_s;
constexpr auto topFrameUniqueRedirectsToExistsQuery = "SELECT EXISTS (SELECT 1 FROM TopFrameUniqueRedirectsTo WHERE sourceDomainID = ? "
    "AND toDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto topFrameUniqueRedirectsFromQuery = "INSERT INTO TopFrameUniqueRedirectsFrom (targetDomainID, fromDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto topFrameUniqueRedirectsFromExistsQuery = "SELECT EXISTS (SELECT 1 FROM TopFrameUniqueRedirectsFrom WHERE targetDomainID = ? "
    "AND fromDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto topFrameLinkDecorationsFromQuery = "INSERT INTO TopFrameLinkDecorationsFrom (fromDomainID, toDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto topFrameLinkDecorationsFromExistsQuery = "SELECT EXISTS (SELECT 1 FROM TopFrameLinkDecorationsFrom WHERE fromDomainID = ? "
    "AND toDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subframeUnderTopFrameDomainsQuery = "INSERT INTO SubframeUnderTopFrameDomains (subFrameDomainID, topFrameDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto subframeUnderTopFrameDomainExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubframeUnderTopFrameDomains WHERE subFrameDomainID = ? "
    "AND topFrameDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subresourceUnderTopFrameDomainsQuery = "INSERT INTO SubresourceUnderTopFrameDomains (subresourceDomainID, topFrameDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto subresourceUnderTopFrameDomainExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubresourceUnderTopFrameDomains "
    "WHERE subresourceDomainID = ? AND topFrameDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subresourceUniqueRedirectsToQuery = "INSERT INTO SubresourceUniqueRedirectsTo (subresourceDomainID, toDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain == ?"_s;
constexpr auto subresourceUniqueRedirectsToExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubresourceUniqueRedirectsTo WHERE subresourceDomainID = ? "
    "AND toDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto subresourceUniqueRedirectsFromQuery = "INSERT INTO SubresourceUniqueRedirectsFrom (subresourceDomainID, fromDomainID) "
    "SELECT ?, domainID FROM ObservedDomains WHERE registrableDomain == ?"_s;
constexpr auto subresourceUniqueRedirectsFromExistsQuery = "SELECT EXISTS (SELECT 1 FROM SubresourceUniqueRedirectsFrom WHERE subresourceDomainID = ? "
    "AND fromDomainID = (SELECT domainID FROM ObservedDomains WHERE registrableDomain = ?))"_s;
constexpr auto mostRecentUserInteractionQuery = "UPDATE ObservedDomains SET hadUserInteraction = ?, mostRecentUserInteractionTime = ? "
    "WHERE registrableDomain = ?"_s;
constexpr auto updateLastSeenQuery = "UPDATE ObservedDomains SET lastSeen = ? WHERE registrableDomain = ?"_s;
constexpr auto updatePrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto isPrevalentResourceQuery = "SELECT isPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto updateVeryPrevalentResourceQuery = "UPDATE ObservedDomains SET isVeryPrevalent = ? WHERE registrableDomain = ?"_s;
constexpr auto isVeryPrevalentResourceQuery = "SELECT isVeryPrevalent FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto clearPrevalentResourceQuery = "UPDATE ObservedDomains SET isPrevalent = 0, isVeryPrevalent = 0 WHERE registrableDomain = ?"_s;
constexpr auto hadUserInteractionQuery = "SELECT hadUserInteraction, mostRecentUserInteractionTime FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto updateGrandfatheredQuery = "UPDATE ObservedDomains SET grandfathered = ? WHERE registrableDomain = ?"_s;
constexpr auto isGrandfatheredQuery = "SELECT grandfathered FROM ObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto findExpiredUserInteractionQuery = "SELECT domainID FROM ObservedDomains WHERE hadUserInteraction = 1 AND mostRecentUserInteractionTime < ?"_s;

constexpr auto createObservedDomain = "CREATE TABLE ObservedDomains ("
    "domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, "
    "hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, "
    "isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,"
    "timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL);"_s;
    
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
    "fromDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(fromDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;

constexpr auto createSubframeUnderTopFrameDomains = "CREATE TABLE SubframeUnderTopFrameDomains ("
    "subFrameDomainID INTEGER NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subFrameDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUnderTopFrameDomains = "CREATE TABLE SubresourceUnderTopFrameDomains ("
    "subresourceDomainID INTEGER NOT NULL, topFrameDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(topFrameDomainID) REFERENCES TopLevelDomains(topLevelDomainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsTo = "CREATE TABLE SubresourceUniqueRedirectsTo ("
    "subresourceDomainID INTEGER NOT NULL, toDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(toDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
constexpr auto createSubresourceUniqueRedirectsFrom = "CREATE TABLE SubresourceUniqueRedirectsFrom ("
    "subresourceDomainID INTEGER NOT NULL, fromDomainID INTEGER NOT NULL, "
    "FOREIGN KEY(subresourceDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE, "
    "FOREIGN KEY(fromDomainID) REFERENCES ObservedDomains(domainID) ON DELETE CASCADE);"_s;
    
ResourceLoadStatisticsDatabaseStore::ResourceLoadStatisticsDatabaseStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost, const String& storageDirectoryPath)
    : ResourceLoadStatisticsStore(store, workQueue, shouldIncludeLocalhost)
    , m_storageDirectoryPath(storageDirectoryPath + "/observations.db")
    , m_observedDomainCount(m_database, observedDomainCountQuery)
    , m_insertObservedDomainStatement(m_database, insertObservedDomainQuery)
    , m_insertTopLevelDomainStatement(m_database, insertTopLevelDomainQuery)
    , m_domainIDFromStringStatement(m_database, domainIDFromStringQuery)
    , m_storageAccessUnderTopFrameDomainsStatement(m_database, storageAccessUnderTopFrameDomainsQuery)
    , m_topFrameUniqueRedirectsTo(m_database, topFrameUniqueRedirectsToQuery)
    , m_topFrameUniqueRedirectsToExists(m_database, topFrameUniqueRedirectsToExistsQuery)
    , m_topFrameUniqueRedirectsFrom(m_database, topFrameUniqueRedirectsFromQuery)
    , m_topFrameUniqueRedirectsFromExists(m_database, topFrameUniqueRedirectsFromExistsQuery)
    , m_topFrameLinkDecorationsFrom(m_database, topFrameLinkDecorationsFromQuery)
    , m_topFrameLinkDecorationsFromExists(m_database, topFrameLinkDecorationsFromExistsQuery)
    , m_subframeUnderTopFrameDomains(m_database, subframeUnderTopFrameDomainsQuery)
    , m_subframeUnderTopFrameDomainExists(m_database, subframeUnderTopFrameDomainExistsQuery)
    , m_subresourceUnderTopFrameDomains(m_database, subresourceUnderTopFrameDomainsQuery)
    , m_subresourceUnderTopFrameDomainExists(m_database, subresourceUnderTopFrameDomainExistsQuery)
    , m_subresourceUniqueRedirectsTo(m_database, subresourceUniqueRedirectsToQuery)
    , m_subresourceUniqueRedirectsToExists(m_database, subresourceUniqueRedirectsToExistsQuery)
    , m_subresourceUniqueRedirectsFrom(m_database, subresourceUniqueRedirectsFromQuery)
    , m_subresourceUniqueRedirectsFromExists(m_database, subresourceUniqueRedirectsFromExistsQuery)
    , m_mostRecentUserInteractionStatement(m_database, mostRecentUserInteractionQuery)
    , m_updateLastSeenStatement(m_database, updateLastSeenQuery)
    , m_updatePrevalentResourceStatement(m_database, updatePrevalentResourceQuery)
    , m_isPrevalentResourceStatement(m_database, isPrevalentResourceQuery)
    , m_updateVeryPrevalentResourceStatement(m_database, updateVeryPrevalentResourceQuery)
    , m_isVeryPrevalentResourceStatement(m_database, isVeryPrevalentResourceQuery)
    , m_clearPrevalentResourceStatement(m_database, clearPrevalentResourceQuery)
    , m_hadUserInteractionStatement(m_database, hadUserInteractionQuery)
    , m_updateGrandfatheredStatement(m_database, updateGrandfatheredQuery)
    , m_isGrandfatheredStatement(m_database, isGrandfatheredQuery)
    , m_findExpiredUserInteractionStatement(m_database, findExpiredUserInteractionQuery)
{
    ASSERT(!RunLoop::isMain());

#if PLATFORM(COCOA)
    registerUserDefaultsIfNeeded();
#endif

    if (!m_database.open(m_storageDirectoryPath)) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::open failed, error message: %{public}s, database path: %{public}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
        ASSERT_NOT_REACHED();
        return;
    }
    
    // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
    m_database.disableThreadingChecks();

    if (!m_database.tableExists("ObservedDomains"_s)) {
        if (!createSchema()) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createSchema failed, error message: %{public}s, database path: %{public}s", this, m_database.lastErrorMsg(), m_storageDirectoryPath.utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }
    }

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

    return true;
}

bool ResourceLoadStatisticsDatabaseStore::prepareStatements()
{
    ASSERT(!RunLoop::isMain());
        
    if (m_observedDomainCount.prepare() != SQLITE_OK
        || m_insertObservedDomainStatement.prepare() != SQLITE_OK
        || m_insertTopLevelDomainStatement.prepare() != SQLITE_OK
        || m_domainIDFromStringStatement.prepare() != SQLITE_OK
        || m_storageAccessUnderTopFrameDomainsStatement.prepare() != SQLITE_OK
        || m_topFrameUniqueRedirectsTo.prepare() != SQLITE_OK
        || m_topFrameUniqueRedirectsToExists.prepare() != SQLITE_OK
        || m_topFrameUniqueRedirectsFrom.prepare() != SQLITE_OK
        || m_topFrameUniqueRedirectsFromExists.prepare() != SQLITE_OK
        || m_subframeUnderTopFrameDomains.prepare() != SQLITE_OK
        || m_subframeUnderTopFrameDomainExists.prepare() != SQLITE_OK
        || m_subresourceUnderTopFrameDomains.prepare() != SQLITE_OK
        || m_subresourceUnderTopFrameDomainExists.prepare() != SQLITE_OK
        || m_subresourceUniqueRedirectsTo.prepare() != SQLITE_OK
        || m_subresourceUniqueRedirectsToExists.prepare() != SQLITE_OK
        || m_subresourceUniqueRedirectsFrom.prepare() != SQLITE_OK
        || m_subresourceUniqueRedirectsFromExists.prepare() != SQLITE_OK
        || m_updateLastSeenStatement.prepare() != SQLITE_OK
        || m_mostRecentUserInteractionStatement.prepare() != SQLITE_OK
        || m_updatePrevalentResourceStatement.prepare() != SQLITE_OK
        || m_isPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_updateVeryPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_isVeryPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_clearPrevalentResourceStatement.prepare() != SQLITE_OK
        || m_hadUserInteractionStatement.prepare() != SQLITE_OK
        || m_updateGrandfatheredStatement.prepare() != SQLITE_OK
        || m_isGrandfatheredStatement.prepare() != SQLITE_OK
        || m_findExpiredUserInteractionStatement.prepare() != SQLITE_OK
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

#ifndef NDEBUG
    ASSERT(confirmDomainDoesNotExist(loadStatistics.registrableDomain));
#endif

    if (m_insertObservedDomainStatement.bindText(1, loadStatistics.registrableDomain.string()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindDouble(2, loadStatistics.lastSeen.secondsSinceEpoch().value()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(3, loadStatistics.hadUserInteraction) != SQLITE_OK
        || m_insertObservedDomainStatement.bindDouble(4, loadStatistics.mostRecentUserInteractionTime.secondsSinceEpoch().value()) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(5, loadStatistics.grandfathered) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(6, loadStatistics.isPrevalentResource) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(7, loadStatistics.isVeryPrevalentResource) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(8, loadStatistics.dataRecordsRemoved) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(9, loadStatistics.timesAccessedAsFirstPartyDueToUserInteraction) != SQLITE_OK
        || m_insertObservedDomainStatement.bindInt(10, loadStatistics.timesAccessedAsFirstPartyDueToStorageAccessAPI) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_insertObservedDomainStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::insertObservedDomain failed to commit, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    int resetResult = m_insertObservedDomainStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    return true;
}

bool ResourceLoadStatisticsDatabaseStore::relationshipExists(WebCore::SQLiteStatement& statement, unsigned firstDomainID, const RegistrableDomain& secondDomain) const
{
    ASSERT(!RunLoop::isMain());

    if (statement.bindInt(1, firstDomainID) != SQLITE_OK
        || statement.bindText(2, secondDomain.string()) != SQLITE_OK
        || statement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::m_insertDomainRelationshipStatement failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
    
    bool relationShipExists = !!statement.getColumnInt(0);

    int resetResult = statement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return relationShipExists;
}

bool ResourceLoadStatisticsDatabaseStore::insertDomainRelationship(WebCore::SQLiteStatement& statement, unsigned domainID, const RegistrableDomain& topFrame)
{
    ASSERT(!RunLoop::isMain());

    if (statement.bindInt(1, domainID) != SQLITE_OK
        || statement.bindText(2, topFrame.string()) != SQLITE_OK
        || statement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::m_insertDomainRelationshipStatement failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    int resetResult = statement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return true;
}

#ifndef NDEBUG
bool ResourceLoadStatisticsDatabaseStore::confirmDomainDoesNotExist(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (m_domainIDFromStringStatement.bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::confirmDomainDoesNotExist failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }
        
    if (m_domainIDFromStringStatement.step() == SQLITE_ROW) {
        int resetResult = m_domainIDFromStringStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
        return false;
    }

    int resetResult = m_domainIDFromStringStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return true;
}
#endif

unsigned ResourceLoadStatisticsDatabaseStore::domainID(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    unsigned domainID = 0;

    if (m_domainIDFromStringStatement.bindText(1, domain.string()) != SQLITE_OK
        || m_domainIDFromStringStatement.step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::domainIDFromString failed, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return domainID;
    }

    domainID = m_domainIDFromStringStatement.getColumnInt(0);

    int resetResult = m_domainIDFromStringStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    
    return domainID;
}

void ResourceLoadStatisticsDatabaseStore::insertDomainRelationships(const WebCore::ResourceLoadStatistics& loadStatistics)
{
    ASSERT(!RunLoop::isMain());

    auto registrableDomainID = domainID(loadStatistics.registrableDomain);
    for (auto& topFrameDomain : loadStatistics.storageAccessUnderTopFrameDomains)
        insertDomainRelationship(m_storageAccessUnderTopFrameDomainsStatement, registrableDomainID, topFrameDomain);

    for (auto& toDomain : loadStatistics.topFrameUniqueRedirectsTo)
        insertDomainRelationship(m_topFrameUniqueRedirectsTo, registrableDomainID, toDomain);
    
    for (auto& fromDomain : loadStatistics.topFrameUniqueRedirectsFrom)
        insertDomainRelationship(m_topFrameUniqueRedirectsFrom, registrableDomainID, fromDomain);
    
    for (auto& topFrameDomain : loadStatistics.subframeUnderTopFrameDomains)
        insertDomainRelationship(m_subframeUnderTopFrameDomains, registrableDomainID, topFrameDomain);
    
    for (auto& topFrameDomain : loadStatistics.subresourceUnderTopFrameDomains)
        insertDomainRelationship(m_subresourceUnderTopFrameDomains, registrableDomainID, topFrameDomain);
    
    for (auto& toDomain : loadStatistics.subresourceUniqueRedirectsTo)
        insertDomainRelationship(m_subresourceUniqueRedirectsTo, registrableDomainID, toDomain);
    
    for (auto& fromDomain : loadStatistics.subresourceUniqueRedirectsFrom)
        insertDomainRelationship(m_subresourceUniqueRedirectsFrom, registrableDomainID, fromDomain);
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

    m_database.runVacuumCommand();
}

void ResourceLoadStatisticsDatabaseStore::calculateAndSubmitTelemetry() const
{
    ASSERT(!RunLoop::isMain());

    // FIXME(195088): Implement for Database version.
}

static String domainsToString(const HashSet<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (auto domainName : domains) {
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::incrementStatisticsForDomains failed, error message: %{public}s", this, m_database.lastErrorMsg());
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
    SQLiteStatement findSubresources(m_database, makeString("SELECT SubresourceUniqueRedirectsFrom.fromDomainID from SubresourceUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = SubresourceUniqueRedirectsFrom.fromDomainID WHERE subresourceDomainID = ", String::number(primaryDomainID), "AND ObservedDomains.isPrevalent = 0"));
    if (findSubresources.prepare() == SQLITE_OK) {
        while (findSubresources.step() == SQLITE_ROW) {
            auto newDomainID = findSubresources.getColumnInt(0);
            auto insertResult = nonPrevalentRedirectionSources.insert(newDomainID);
            if (insertResult.second)
                newlyIdentifiedDomains.insert(newDomainID);
        }
    }

    SQLiteStatement findTopFrames(m_database, makeString("SELECT TopFrameUniqueRedirectsFrom.fromDomainID from TopFrameUniqueRedirectsFrom INNER JOIN ObservedDomains ON ObservedDomains.domainID = TopFrameUniqueRedirectsFrom.fromDomainID WHERE targetDomainID = ", String::number(primaryDomainID), "AND ObservedDomains.isPrevalent = 0"));
    if (findTopFrames.prepare() == SQLITE_OK) {
        while (findTopFrames.step() == SQLITE_ROW) {
            auto newDomainID = findTopFrames.getColumnInt(0);
            auto insertResult = nonPrevalentRedirectionSources.insert(newDomainID);
            if (insertResult.second)
                newlyIdentifiedDomains.insert(newDomainID);
        }
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::markAsPrevalentIfHasRedirectedToPrevalent failed to execute, error message: %{public}s", this, m_database.lastErrorMsg());
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

    SQLiteStatement subframeUnderTopFrameDomainsCountStatement(m_database, makeString("SELECT subresourceDomainID, COUNT(topFrameDomainID) FROM SubresourceUnderTopFrameDomains WHERE subresourceDomainID IN (", domainIDsOfInterest, ") GROUP BY subresourceDomainID"));
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
        if (shouldSkip(resourceStatistic.registerableDomain))
            continue;

        auto newPrevalence = classifier().calculateResourcePrevalence(resourceStatistic.subresourceUnderTopFrameDomainsCount, resourceStatistic.subresourceUniqueRedirectsToCount, resourceStatistic.subframeUnderTopFrameDomainsCount, resourceStatistic.topFrameUniqueRedirectsToCount, resourceStatistic.prevalence);
        if (newPrevalence != resourceStatistic.prevalence)
            setPrevalentResource(resourceStatistic.registerableDomain, newPrevalence);
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

void ResourceLoadStatisticsDatabaseStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, Optional<FrameID> frameID, PageID pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(subFrameDomain);

    switch (cookieTreatmentForOrigin(subFrameDomain)) {
    case CookieTreatmentResult::BlockAndPurge:
        completionHandler(false);
        return;
    case CookieTreatmentResult::BlockAndKeep:
        completionHandler(true);
        return;
    case CookieTreatmentResult::Allow:
        // Do nothing
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

void ResourceLoadStatisticsDatabaseStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameID frameID, PageID pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);

    switch (cookieTreatmentForOrigin(subFrameDomain)) {
    case CookieTreatmentResult::BlockAndPurge: {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ResourceLoadStatisticsDebug, "Cannot grant storage access to %{public}s since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
#endif
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        }
        return;
    case CookieTreatmentResult::BlockAndKeep: {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ResourceLoadStatisticsDebug, "No need to grant storage access to %{public}s since its cookies are not blocked in third-party contexts.", subFrameDomain.string().utf8().data());
#endif
        completionHandler(StorageAccessStatus::HasAccess);
        }
        return;
    case CookieTreatmentResult::Allow:
        // Do nothing
        break;
    };

    auto userWasPromptedEarlier = promptEnabled && hasUserGrantedStorageAccessThroughPrompt(subFrameStatus.second, topFrameDomain);
    if (promptEnabled && !userWasPromptedEarlier) {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ResourceLoadStatisticsDebug, "About to ask the user whether they want to grant storage access to %{public}s under %{public}s or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
#endif
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

#if !RELEASE_LOG_DISABLED
    if (userWasPromptedEarlier)
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ResourceLoadStatisticsDebug, "Storage access was granted to %{public}s under %{public}s.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
#endif

    SQLiteStatement incrementStorageAccess(m_database, makeString("UPDATE ObservedDomains SET timesAccessedAsFirstPartyDueToStorageAccessAPI = timesAccessedAsFirstPartyDueToStorageAccessAPI + 1 WHERE domainID = ", String::number(subFrameStatus.second)));
    if (incrementStorageAccess.prepare() != SQLITE_OK
        || incrementStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::requestStorageAccess failed, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
    
    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, [completionHandler = WTFMove(completionHandler)] (bool wasGrantedAccess) mutable {
        completionHandler(wasGrantedAccess ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsDatabaseStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, OpenerPageID openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    ensureResourceStatisticsForRegistrableDomain(domainInNeedOfStorageAccess);

    if (cookieTreatmentForOrigin(domainInNeedOfStorageAccess) != CookieTreatmentResult::Allow)
        return;

#if !RELEASE_LOG_DISABLED
    RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ResourceLoadStatisticsDebug, "[Temporary combatibility fix] Storage access was granted for %{public}s under opener page from %{public}s, with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
#endif
    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID, false, [](bool) { });
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (userWasPromptedNow) {
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain));
        insertDomainRelationship(m_storageAccessUnderTopFrameDomainsStatement, subFrameStatus.second, topFrameDomain);
    }

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedNow, WTFMove(completionHandler));
}

void ResourceLoadStatisticsDatabaseStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, Optional<FrameID> frameID, PageID pageID, bool userWasPromptedNowOrEarlier, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(!RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        callback(true);
        return;
    }

    // FIXME: Remove m_storageAccessPromptsEnabled check if prompting is no longer experimental.
    if (userWasPromptedNowOrEarlier && storageAccessPromptsEnabled()) {
#ifndef NDEBUG
        auto subFrameStatus = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatus.first == AddedRecord::No);
        ASSERT(hasHadUserInteraction(subFrameDomain));
        ASSERT(hasUserGrantedStorageAccessThroughPrompt(subFrameStatus.second, topFrameDomain));
#endif
        setUserInteraction(subFrameDomain, true, WallTime::now());
    }

    RunLoop::main().dispatch([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, store = makeRef(store()), callback = WTFMove(callback)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, [callback = WTFMove(callback), store = store.copyRef()](bool value) mutable {
            store->statisticsQueue().dispatch([callback = WTFMove(callback), value] () mutable {
                callback(value);
            });
        });
    });

}

void ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement domainsToUpdateStatement(m_database, makeString("UPDATE ObservedDomains SET grandfathered = 1 WHERE registrableDomain IN (", domainsToString(domains), ")"));
    if (domainsToUpdateStatement.prepare() != SQLITE_OK
        || domainsToUpdateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::grandfatherDataForDomains failed, error message: %{public}s", this, m_database.lastErrorMsg());
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
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "Did set %{public}s as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
#endif
    }

    return primaryDomainsToBlock;
}

void ResourceLoadStatisticsDatabaseStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndTopFrameDomainsSameSite = targetDomain == topFrameDomain;
    bool areTargetAndSourceDomainsSameSite = targetDomain == sourceDomain;

    bool statisticsWereUpdated = false;
    if (!isMainFrame && !(areTargetAndTopFrameDomainsSameSite || areTargetAndSourceDomainsSameSite)) {
        auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        updateLastSeen(targetDomain, ResourceLoadStatistics::reduceTimeResolution(WallTime::now()));

        if (!relationshipExists(m_subframeUnderTopFrameDomainExists, targetResult.second, topFrameDomain)) {
            insertDomainRelationship(m_subframeUnderTopFrameDomains, targetResult.second, topFrameDomain);
            statisticsWereUpdated = true;
        }
    }

    if (isRedirect && !areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            if (!relationshipExists(m_topFrameUniqueRedirectsToExists, redirectingDomainResult.second, targetDomain)) {
                insertDomainRelationship(m_topFrameUniqueRedirectsTo, redirectingDomainResult.second, targetDomain);
                statisticsWereUpdated = true;
            }

            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (!relationshipExists(m_topFrameUniqueRedirectsFromExists, targetResult.second, sourceDomain)) {
                insertDomainRelationship(m_topFrameUniqueRedirectsFrom, targetResult.second, sourceDomain);
                statisticsWereUpdated = true;
            }
        } else {
            auto redirectingDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            if (!relationshipExists(m_subresourceUniqueRedirectsToExists, redirectingDomainResult.second, targetDomain)) {
                insertDomainRelationship(m_subresourceUniqueRedirectsTo, redirectingDomainResult.second, targetDomain);
                statisticsWereUpdated = true;
            }

            auto targetResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (!relationshipExists(m_subresourceUniqueRedirectsFromExists, targetResult.second, sourceDomain)) {
                insertDomainRelationship(m_subresourceUniqueRedirectsFrom, targetResult.second, sourceDomain);
                statisticsWereUpdated = true;
            }
        }
    }

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsDatabaseStore::logSubresourceLoading(const SubResourceDomain& targetDomain, const TopFrameDomain& topFrameDomain, WallTime lastSeen)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(targetDomain);
    updateLastSeen(targetDomain, lastSeen);

    auto targetDomainID = result.second;
    if (!relationshipExists(m_subresourceUnderTopFrameDomainExists, targetDomainID, topFrameDomain)) {
        insertDomainRelationship(m_subresourceUnderTopFrameDomains, targetDomainID, topFrameDomain);
        scheduleStatisticsProcessingRequestIfNecessary();
    }
}

void ResourceLoadStatisticsDatabaseStore::logSubresourceRedirect(const RedirectedFromDomain& sourceDomain, const RedirectedToDomain& targetDomain)
{
    ASSERT(!RunLoop::isMain());

    auto sourceDomainResult = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
    auto targetDomainResult = ensureResourceStatisticsForRegistrableDomain(targetDomain);

    bool isNewRedirectToEntry = false;
    if (!relationshipExists(m_subresourceUniqueRedirectsToExists, sourceDomainResult.second, targetDomain)) {
        insertDomainRelationship(m_subresourceUniqueRedirectsTo, sourceDomainResult.second, targetDomain);
        isNewRedirectToEntry = true;
    }

    bool isNewRedirectFromEntry = false;
    if (!relationshipExists(m_subresourceUniqueRedirectsFromExists, targetDomainResult.second, sourceDomain)) {
        insertDomainRelationship(m_subresourceUniqueRedirectsFrom, targetDomainResult.second, sourceDomain);
        isNewRedirectFromEntry = true;
    }

    if (isNewRedirectToEntry || isNewRedirectFromEntry)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsDatabaseStore::logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain& fromDomain, const NavigatedToDomain& toDomain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(fromDomain != toDomain);

    auto fromDomainResult = ensureResourceStatisticsForRegistrableDomain(fromDomain);

    if (!relationshipExists(m_topFrameLinkDecorationsFromExists, fromDomainResult.second, toDomain)) {
        insertDomainRelationship(m_topFrameLinkDecorationsFrom, fromDomainResult.second, toDomain);
        scheduleStatisticsProcessingRequestIfNecessary();
    }
}

void ResourceLoadStatisticsDatabaseStore::setUserInteraction(const RegistrableDomain& domain, bool hadUserInteraction, WallTime mostRecentInteraction)
{
    ASSERT(!RunLoop::isMain());

    if (m_mostRecentUserInteractionStatement.bindInt(1, hadUserInteraction)
        || m_mostRecentUserInteractionStatement.bindDouble(2, mostRecentInteraction.secondsSinceEpoch().value()) != SQLITE_OK
        || m_mostRecentUserInteractionStatement.bindText(3, domain.string()) != SQLITE_OK
        || m_mostRecentUserInteractionStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::setUserInteraction, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }

    int resetResult = m_mostRecentUserInteractionStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
}

void ResourceLoadStatisticsDatabaseStore::logUserInteraction(const TopFrameDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    ensureResourceStatisticsForRegistrableDomain(domain);
    setUserInteraction(domain, true, WallTime::now());
}

void ResourceLoadStatisticsDatabaseStore::clearUserInteraction(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto targetResult = ensureResourceStatisticsForRegistrableDomain(domain);
    setUserInteraction(domain, false, { });

    SQLiteStatement removeStorageAccess(m_database, makeString("DELETE FROM StorageAccessUnderTopFrameDomains WHERE domainID = ", String::number(targetResult.second)));
    if (removeStorageAccess.prepare() != SQLITE_OK
        || removeStorageAccess.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::logUserInteraction failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

bool ResourceLoadStatisticsDatabaseStore::hasHadUserInteraction(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (m_hadUserInteractionStatement.bindText(1, domain.string()) != SQLITE_OK
        || m_hadUserInteractionStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::m_updatePrevalentResourceStatement failed, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return false;
    }

    bool hadUserInteraction = !!m_hadUserInteractionStatement.getColumnInt(0);
    if (!hadUserInteraction)
        return false;
    
    WallTime mostRecentUserInteractionTime = WallTime::fromRawSeconds(m_hadUserInteractionStatement.getColumnDouble(1));

    if (hasStatisticsExpired(mostRecentUserInteractionTime)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        clearUserInteraction(domain);
        hadUserInteraction = false;
    }
    
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::m_updatePrevalentResourceStatement failed, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_updatePrevalentResourceStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);

    if (newPrevalence == ResourceLoadPrevalence::VeryHigh) {
        if (m_updateVeryPrevalentResourceStatement.bindInt(1, 1) != SQLITE_OK
            || m_updateVeryPrevalentResourceStatement.bindText(2, domain.string()) != SQLITE_OK
            || m_updateVeryPrevalentResourceStatement.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::m_updateVeryPrevalentResourceStatement failed, error message: %{public}s", this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
            return;
        }

        int resetResult = m_updateVeryPrevalentResourceStatement.reset();
        ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
    }

    StdSet<unsigned> nonPrevalentRedirectionSources;
    recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(domainID(domain), nonPrevalentRedirectionSources, 0);
    setDomainsAsPrevalent(WTFMove(nonPrevalentRedirectionSources));
}

void ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent(StdSet<unsigned>&& domains)
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement domainsToUpdateStatement(m_database, makeString("UPDATE ObservedDomains SET isPrevalent = 1 WHERE domainID IN (", buildList(WTF::IteratorRange<StdSet<unsigned>::iterator>(domains.begin(), domains.end())), ")"));
    if (domainsToUpdateStatement.prepare() != SQLITE_OK
        || domainsToUpdateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::setDomainsAsPrevalent failed, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
}

String ResourceLoadStatisticsDatabaseStore::dumpResourceLoadStatistics() const
{
    ASSERT(!RunLoop::isMain());

    // FIXME(195088): Implement SQLite-based dumping routines.
    ASSERT_NOT_REACHED();
    return { };
}

bool ResourceLoadStatisticsDatabaseStore::predicateValueForDomain(WebCore::SQLiteStatement& predicateStatement, const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());
    
    if (predicateStatement.bindText(1, domain.string()) != SQLITE_OK
        || predicateStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::predicateValueForDomain failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::clearPrevalentResource, error message: %{public}s", this, m_database.lastErrorMsg());
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
        || m_updateGrandfatheredStatement.bindText(1, domain.string()) != SQLITE_OK
        || m_updateGrandfatheredStatement.step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::setGrandfathered failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return;
    }
    
    int resetResult = m_updateGrandfatheredStatement.reset();
    ASSERT_UNUSED(resetResult, resetResult == SQLITE_OK);
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
    ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    insertDomainRelationship(m_subframeUnderTopFrameDomains, result.second, topFrameDomain);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the top frame domain.
    ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    insertDomainRelationship(m_subresourceUnderTopFrameDomains, result.second, topFrameDomain);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);

    insertDomainRelationship(m_subresourceUniqueRedirectsTo, result.second, redirectDomain);
}

void ResourceLoadStatisticsDatabaseStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);

    insertDomainRelationship(m_subresourceUniqueRedirectsFrom, result.second, redirectDomain);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);

    insertDomainRelationship(m_topFrameUniqueRedirectsTo, result.second, redirectDomain);
}

void ResourceLoadStatisticsDatabaseStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto result = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);

    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);

    insertDomainRelationship(m_topFrameUniqueRedirectsFrom, result.second, redirectDomain);
}

std::pair<ResourceLoadStatisticsDatabaseStore::AddedRecord, unsigned> ResourceLoadStatisticsDatabaseStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (m_domainIDFromStringStatement.bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::createSchema failed, error message: %{public}s", this, m_database.lastErrorMsg());
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

    return { AddedRecord::Yes, domainID(domain) };
}

void ResourceLoadStatisticsDatabaseStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    clearOperatingDates();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator = callbackAggregator.copyRef()] { });

    auto primaryDomainsToBlock = ensurePrevalentResourcesForDebugMode();
    updateCookieBlockingForDomains(primaryDomainsToBlock, [callbackAggregator = callbackAggregator.copyRef()] { });
}

ResourceLoadStatisticsDatabaseStore::CookieTreatmentResult ResourceLoadStatisticsDatabaseStore::cookieTreatmentForOrigin(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    SQLiteStatement statement(m_database, makeString("SELECT isPrevalent, hadUserInteraction FROM ObservedDomains WHERE registrableDomain = ", domain.string()));
    if (statement.prepare() != SQLITE_OK
        || statement.step() != SQLITE_ROW) {
        return CookieTreatmentResult::Allow;
    }
    
    bool isPrevalent = !!statement.getColumnInt(0);
    if (!isPrevalent)
        return CookieTreatmentResult::Allow;

    bool hadUserInteraction = statement.getColumnInt(1) ? true : false;
    return hadUserInteraction ? CookieTreatmentResult::BlockAndKeep : CookieTreatmentResult::BlockAndPurge;
}
    
bool ResourceLoadStatisticsDatabaseStore::hasUserGrantedStorageAccessThroughPrompt(unsigned requestingDomainID, const RegistrableDomain& firstPartyDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto firstPartyPrimaryDomainID = domainID(firstPartyDomain);

    SQLiteStatement statement(m_database, makeString("SELECT COUNT(*) FROM StorageAccessUnderTopFrameDomains WHERE domainID = ", String::number(requestingDomainID), " AND topLevelDomainID = ", String::number(firstPartyPrimaryDomainID)));
    if (statement.prepare() != SQLITE_OK
        || statement.step() != SQLITE_ROW)
        return false;

    return !statement.getColumnInt(0);
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::domainsToBlock() const
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> results;
    SQLiteStatement statement(m_database, "SELECT registrableDomain FROM ObservedDomains WHERE isPrevalent = 1"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW)
        results.append(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(0)));

    return results;
}

void ResourceLoadStatisticsDatabaseStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto domainsToBlock = this->domainsToBlock();

    if (domainsToBlock.isEmpty()) {
        completionHandler();
        return;
    }

    if (debugLoggingEnabled() && !domainsToBlock.isEmpty())
        debugLogDomainsInBatches("block", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(store()), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
                if (!weakThis)
                    return;
#if !RELEASE_LOG_DISABLED
                RELEASE_LOG_INFO_IF(weakThis->debugLoggingEnabled(), ResourceLoadStatisticsDebug, "Done updating cookie blocking.");
#endif
            });
        });
    });
}

Vector<ResourceLoadStatisticsDatabaseStore::PrevalentDomainData> ResourceLoadStatisticsDatabaseStore::prevalentDomains() const
{
    ASSERT(!RunLoop::isMain());
    
    Vector<PrevalentDomainData> results;
    SQLiteStatement statement(m_database, "SELECT domainID, registrableDomain, mostRecentUserInteractionTime, hadUserInteraction, grandfathered FROM ObservedDomains WHERE isPrevalent = 1"_s);
    if (statement.prepare() != SQLITE_OK)
        return results;
    
    while (statement.step() == SQLITE_ROW) {
        results.append({ static_cast<unsigned>(statement.getColumnInt(0))
            , RegistrableDomain::uncheckedCreateFromRegistrableDomainString(statement.getColumnText(1))
            , WallTime::fromRawSeconds(statement.getColumnDouble(2))
            , statement.getColumnInt(3) ? true : false
            , statement.getColumnInt(4) ? true : false
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::logUserInteraction failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::clearGrandfathering failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Vector<RegistrableDomain> ResourceLoadStatisticsDatabaseStore::registrableDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    clearExpiredUserInteractions();
    
    Vector<RegistrableDomain> prevalentResources;

    Vector<PrevalentDomainData> prevalentDomains = this->prevalentDomains();
    Vector<unsigned> domainIDsToClearGrandfathering;
    for (auto& statistic : prevalentDomains) {
        if (!statistic.hadUserInteraction && (!shouldCheckForGrandfathering || !statistic.grandfathered))
            prevalentResources.append(statistic.registerableDomain);

        if (shouldClearGrandfathering && statistic.grandfathered)
            domainIDsToClearGrandfathering.append(statistic.domainID);
    }

    clearGrandfathering(WTFMove(domainIDsToClearGrandfathering));
    
    return prevalentResources;
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

    SQLiteStatement recordsToPrune(m_database, makeString("SELECT domainID FROM ObservedDomains ORDER BY hadUserInteraction, isPrevalent, lastSeen DESC LIMIT ", String::number(countLeftToPrune)));
    if (recordsToPrune.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %{public}s", this, m_database.lastErrorMsg());
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::pruneStatisticsIfNeeded failed, error message: %{public}s", this, m_database.lastErrorMsg());
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
        RELEASE_LOG_ERROR(Network, "%p - ResourceLoadStatisticsDatabaseStore::updateLastSeen failed to bind, error message: %{public}s", this, m_database.lastErrorMsg());
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

} // namespace WebKit

#endif
