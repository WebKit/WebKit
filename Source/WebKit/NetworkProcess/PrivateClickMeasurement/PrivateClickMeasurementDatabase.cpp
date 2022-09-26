/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PrivateClickMeasurementDatabase.h"

#include "Logging.h"
#include "PrivateClickMeasurementDebugInfo.h"
#include "PrivateClickMeasurementManager.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <WebCore/SQLiteTransaction.h>

namespace WebKit::PCM {

constexpr auto setUnattributedPrivateClickMeasurementAsExpiredQuery = "UPDATE UnattributedPrivateClickMeasurement SET timeOfAdClick = -1.0"_s;
constexpr auto insertUnattributedPrivateClickMeasurementQuery = "INSERT OR REPLACE INTO UnattributedPrivateClickMeasurement (sourceSiteDomainID, destinationSiteDomainID, "
    "sourceID, timeOfAdClick, token, signature, keyID, sourceApplicationBundleID) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"_s;
constexpr auto insertAttributedPrivateClickMeasurementQuery = "INSERT OR REPLACE INTO AttributedPrivateClickMeasurement (sourceSiteDomainID, destinationSiteDomainID, "
    "sourceID, attributionTriggerData, priority, timeOfAdClick, earliestTimeToSendToSource, token, signature, keyID, earliestTimeToSendToDestination, sourceApplicationBundleID, destinationToken, destinationSignature, destinationKeyID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
constexpr auto findUnattributedQuery = "SELECT * FROM UnattributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto findAttributedQuery = "SELECT * FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto removeUnattributedQuery = "DELETE FROM UnattributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto allAttributedPrivateClickMeasurementQuery = "SELECT *, MIN(earliestTimeToSendToSource, earliestTimeToSendToDestination) as minVal "
    "FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToSource IS NOT NULL AND earliestTimeToSendToDestination IS NOT NULL "
    "UNION ALL SELECT *, earliestTimeToSendToSource as minVal FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToDestination IS NULL "
    "UNION ALL SELECT *, earliestTimeToSendToDestination as minVal FROM AttributedPrivateClickMeasurement WHERE earliestTimeToSendToSource IS NULL ORDER BY minVal"_s;
constexpr auto allUnattributedPrivateClickMeasurementAttributionsQuery = "SELECT * FROM UnattributedPrivateClickMeasurement"_s;
constexpr auto clearExpiredPrivateClickMeasurementQuery = "DELETE FROM UnattributedPrivateClickMeasurement WHERE ? > timeOfAdClick"_s;
constexpr auto markReportAsSentToSourceQuery = "UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToSource = null WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto markReportAsSentToDestinationQuery = "UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToDestination = null WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto earliestTimesToSendQuery = "SELECT earliestTimeToSendToSource, earliestTimeToSendToDestination FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s;
constexpr auto domainIDFromStringQuery = "SELECT domainID FROM PCMObservedDomains WHERE registrableDomain = ?"_s;
constexpr auto domainStringFromDomainIDQuery = "SELECT registrableDomain FROM PCMObservedDomains WHERE domainID = ?"_s;
constexpr auto createUnattributedPrivateClickMeasurement = "CREATE TABLE UnattributedPrivateClickMeasurement ("
    "sourceSiteDomainID INTEGER NOT NULL, destinationSiteDomainID INTEGER NOT NULL, sourceID INTEGER NOT NULL, "
    "timeOfAdClick REAL NOT NULL, token TEXT, signature TEXT, keyID TEXT, sourceApplicationBundleID TEXT, FOREIGN KEY(sourceSiteDomainID) "
    "REFERENCES PCMObservedDomains(domainID) ON DELETE CASCADE, FOREIGN KEY(destinationSiteDomainID) REFERENCES "
    "PCMObservedDomains(domainID) ON DELETE CASCADE)"_s;
constexpr auto createAttributedPrivateClickMeasurement = "CREATE TABLE AttributedPrivateClickMeasurement ("
    "sourceSiteDomainID INTEGER NOT NULL, destinationSiteDomainID INTEGER NOT NULL, sourceID INTEGER NOT NULL, "
    "attributionTriggerData INTEGER NOT NULL, priority INTEGER NOT NULL, timeOfAdClick REAL NOT NULL, "
    "earliestTimeToSendToSource REAL, token TEXT, signature TEXT, keyID TEXT, earliestTimeToSendToDestination REAL, sourceApplicationBundleID TEXT, "
    "destinationToken TEXT, destinationSignature TEXT, destinationKeyID TEXT, "
    "FOREIGN KEY(sourceSiteDomainID) REFERENCES PCMObservedDomains(domainID) ON DELETE CASCADE, FOREIGN KEY(destinationSiteDomainID) REFERENCES "
    "PCMObservedDomains(domainID) ON DELETE CASCADE)"_s;
constexpr auto createUniqueIndexUnattributedPrivateClickMeasurement = "CREATE UNIQUE INDEX IF NOT EXISTS UnattributedPrivateClickMeasurement_sourceSiteDomainID_destinationSiteDomainID_sourceApplicationBundleID on UnattributedPrivateClickMeasurement ( sourceSiteDomainID, destinationSiteDomainID, sourceApplicationBundleID )"_s;
constexpr auto createUniqueIndexAttributedPrivateClickMeasurement = "CREATE UNIQUE INDEX IF NOT EXISTS AttributedPrivateClickMeasurement_sourceSiteDomainID_destinationSiteDomainID_sourceApplicationBundleID on AttributedPrivateClickMeasurement ( sourceSiteDomainID, destinationSiteDomainID, sourceApplicationBundleID )"_s;
constexpr auto createPCMObservedDomain = "CREATE TABLE PCMObservedDomains ("
    "domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL)"_s;
constexpr auto insertObservedDomainQuery = "INSERT INTO PCMObservedDomains (registrableDomain) VALUES (?)"_s;
constexpr auto clearAllPrivateClickMeasurementQuery = "DELETE FROM PCMObservedDomains WHERE domainID LIKE ?"_s;

static HashSet<Database*>& allDatabases()
{
    ASSERT(!RunLoop::isMain());
    static NeverDestroyed<HashSet<Database*>> set;
    return set;
}

Database::Database(const String& storageDirectory)
    : DatabaseUtilities(FileSystem::pathByAppendingComponent(storageDirectory, "pcm.db"_s))
{
    ASSERT(!RunLoop::isMain());
    openDatabaseAndCreateSchemaIfNecessary();
    enableForeignKeys();
    addDestinationTokenColumnsIfNecessary();
    allDatabases().add(this);
}

Database::~Database()
{
    ASSERT(!RunLoop::isMain());
    close();
    allDatabases().remove(this);
}

const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& Database::expectedTableAndIndexQueries()
{
    static NeverDestroyed expectedTableAndIndexQueries = MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair> {
        { "PCMObservedDomains"_s, std::make_pair<String, std::optional<String>>(createPCMObservedDomain, std::nullopt) },
        { "UnattributedPrivateClickMeasurement"_s, std::make_pair<String, std::optional<String>>(createUnattributedPrivateClickMeasurement, stripIndexQueryToMatchStoredValue(createUniqueIndexUnattributedPrivateClickMeasurement)) },
        { "AttributedPrivateClickMeasurement"_s, std::make_pair<String, std::optional<String>>(createAttributedPrivateClickMeasurement, stripIndexQueryToMatchStoredValue(createUniqueIndexAttributedPrivateClickMeasurement)) },
    };

    return expectedTableAndIndexQueries;
}

Span<const ASCIILiteral> Database::sortedTables()
{
    static std::array sortedTables {
        "PCMObservedDomains"_s,
        "UnattributedPrivateClickMeasurement"_s,
        "AttributedPrivateClickMeasurement"_s
    };

    return { sortedTables.data(), sortedTables.size() };
}

void Database::interruptAllDatabases()
{
    ASSERT(!RunLoop::isMain());
    for (auto database : allDatabases())
        database->interrupt();
}

bool Database::createUniqueIndices()
{
    if (!m_database.executeCommand(createUniqueIndexUnattributedPrivateClickMeasurement)
        || !m_database.executeCommand(createUniqueIndexAttributedPrivateClickMeasurement)) {
        LOG_ERROR("Error creating indexes");
        return false;
    }
    return true;
}

bool Database::createSchema()
{
    ASSERT(!RunLoop::isMain());

    if (!m_database.executeCommand(createPCMObservedDomain)) {
        LOG_ERROR("Could not create PCMObservedDomains table in database (%i) - %s", m_database.lastError(), m_database.lastErrorMsg());
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

    if (!m_database.executeCommand(createUniqueIndexUnattributedPrivateClickMeasurement)
        || !m_database.executeCommand(createUniqueIndexAttributedPrivateClickMeasurement)) {
        LOG_ERROR("Error creating indexes");
        return false;
    }
    return true;
}

void Database::insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&& attribution, PrivateClickMeasurementAttributionType attributionType)
{
    ASSERT(!RunLoop::isMain());

    auto transactionScope = beginTransactionIfNecessary();

    auto sourceID = ensureDomainID(attribution.sourceSite().registrableDomain);
    auto attributionDestinationID = ensureDomainID(attribution.destinationSite().registrableDomain);
    if (!sourceID || !attributionDestinationID)
        return;

    auto& sourceSecretToken = attribution.sourceSecretToken();
    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = attribution.attributionTriggerData() ? attribution.attributionTriggerData().value().data : -1;
        auto priority = attribution.attributionTriggerData() ? attribution.attributionTriggerData().value().priority : -1;
        auto sourceEarliestTimeToSend = attribution.timesToSend().sourceEarliestTimeToSend ? attribution.timesToSend().sourceEarliestTimeToSend.value().secondsSinceEpoch().value() : -1;
        auto destinationSecretToken = attribution.attributionTriggerData() ? attribution.attributionTriggerData().value().destinationSecretToken : std::nullopt;
        auto destinationEarliestTimeToSend = attribution.timesToSend().destinationEarliestTimeToSend ? attribution.timesToSend().destinationEarliestTimeToSend.value().secondsSinceEpoch().value() : -1;

        // We should never be inserting an attributed private click measurement value into the database without valid report times.
        ASSERT(sourceEarliestTimeToSend != -1 || destinationEarliestTimeToSend != -1);

        auto statement = m_database.prepareStatement(insertAttributedPrivateClickMeasurementQuery);
        if (!statement
            || statement->bindInt(1, *sourceID) != SQLITE_OK
            || statement->bindInt(2, *attributionDestinationID) != SQLITE_OK
            || statement->bindInt(3, attribution.sourceID()) != SQLITE_OK
            || statement->bindInt(4, attributionTriggerData) != SQLITE_OK
            || statement->bindInt(5, priority) != SQLITE_OK
            || statement->bindDouble(6, attribution.timeOfAdClick().secondsSinceEpoch().value()) != SQLITE_OK
            || statement->bindDouble(7, sourceEarliestTimeToSend) != SQLITE_OK
            || statement->bindText(8, sourceSecretToken ? sourceSecretToken->tokenBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(9, sourceSecretToken ? sourceSecretToken->signatureBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(10, sourceSecretToken ? sourceSecretToken->keyIDBase64URL : emptyString()) != SQLITE_OK
            || statement->bindDouble(11, destinationEarliestTimeToSend) != SQLITE_OK
            || statement->bindText(12, attribution.sourceApplicationBundleID()) != SQLITE_OK
            || statement->bindText(13, destinationSecretToken ? destinationSecretToken->tokenBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(14, destinationSecretToken ? destinationSecretToken->signatureBase64URL : emptyString()) != SQLITE_OK
            || statement->bindText(15, destinationSecretToken ? destinationSecretToken->keyIDBase64URL : emptyString()) != SQLITE_OK
            || statement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::insertPrivateClickMeasurement insertAttributedPrivateClickMeasurementQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
            ASSERT_NOT_REACHED();
        }
        return;
    }

    ASSERT(attributionType == PrivateClickMeasurementAttributionType::Unattributed);

    auto statement = m_database.prepareStatement(insertUnattributedPrivateClickMeasurementQuery);
    if (!statement
        || statement->bindInt(1, *sourceID) != SQLITE_OK
        || statement->bindInt(2, *attributionDestinationID) != SQLITE_OK
        || statement->bindInt(3, attribution.sourceID()) != SQLITE_OK
        || statement->bindDouble(4, attribution.timeOfAdClick().secondsSinceEpoch().value()) != SQLITE_OK
        || statement->bindText(5, sourceSecretToken ? sourceSecretToken->tokenBase64URL : emptyString()) != SQLITE_OK
        || statement->bindText(6, sourceSecretToken ? sourceSecretToken->signatureBase64URL : emptyString()) != SQLITE_OK
        || statement->bindText(7, sourceSecretToken ? sourceSecretToken->keyIDBase64URL : emptyString()) != SQLITE_OK
        || statement->bindText(8, attribution.sourceApplicationBundleID()) != SQLITE_OK
        || statement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::insertPrivateClickMeasurement insertUnattributedPrivateClickMeasurementQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting()
{
    ASSERT(!RunLoop::isMain());
    auto scopedStatement = this->scopedStatement(m_setUnattributedPrivateClickMeasurementAsExpiredStatement, setUnattributedPrivateClickMeasurementAsExpiredQuery, "markAllUnattributedPrivateClickMeasurementAsExpiredForTesting"_s);

    if (!scopedStatement || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

std::pair<std::optional<Database::UnattributedPrivateClickMeasurement>, std::optional<Database::AttributedPrivateClickMeasurement>> Database::findPrivateClickMeasurement(const WebCore::PCM::SourceSite& sourceSite, const WebCore::PCM::AttributionDestinationSite& destinationSite, const ApplicationBundleIdentifier& applicationBundleIdentifier)
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomainID = domainID(sourceSite.registrableDomain);
    auto destinationSiteDomainID = domainID(destinationSite.registrableDomain);
    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return std::make_pair(std::nullopt, std::nullopt);

    auto findUnattributedScopedStatement = this->scopedStatement(m_findUnattributedStatement, findUnattributedQuery, "findPrivateClickMeasurement"_s);
    if (!findUnattributedScopedStatement
        || findUnattributedScopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || findUnattributedScopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || findUnattributedScopedStatement->bindText(3, applicationBundleIdentifier) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::findPrivateClickMeasurement findUnattributedQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    auto findAttributedScopedStatement = this->scopedStatement(m_findAttributedStatement, findAttributedQuery, "findPrivateClickMeasurement"_s);
    if (!findAttributedScopedStatement
        || findAttributedScopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || findAttributedScopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || findAttributedScopedStatement->bindText(3, applicationBundleIdentifier) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::findPrivateClickMeasurement findAttributedQuery, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    std::optional<UnattributedPrivateClickMeasurement> unattributedPrivateClickMeasurement;
    if (findUnattributedScopedStatement->step() == SQLITE_ROW)
        unattributedPrivateClickMeasurement = buildPrivateClickMeasurementFromDatabase(*findUnattributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Unattributed);

    std::optional<AttributedPrivateClickMeasurement> attributedPrivateClickMeasurement;
    if (findAttributedScopedStatement->step() == SQLITE_ROW)
        attributedPrivateClickMeasurement = buildPrivateClickMeasurementFromDatabase(*findAttributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed);

    return std::make_pair(unattributedPrivateClickMeasurement, attributedPrivateClickMeasurement);
}

std::pair<std::optional<WebCore::PCM::AttributionSecondsUntilSendData>, DebugInfo> Database::attributePrivateClickMeasurement(const WebCore::PCM::SourceSite& sourceSite, const WebCore::PCM::AttributionDestinationSite& destinationSite, const ApplicationBundleIdentifier& applicationBundleIdentifier, WebCore::PCM::AttributionTriggerData&& attributionTriggerData, WebCore::PrivateClickMeasurement::IsRunningLayoutTest isRunningTest)
{
    ASSERT(!RunLoop::isMain());

    // We should always clear expired clicks from the database before scheduling an attribution.
    clearExpiredPrivateClickMeasurement();
    if (!attributionTriggerData.isValid()) {
        RELEASE_LOG_INFO(PrivateClickMeasurement, "Got an invalid attribution.");
        return { std::nullopt, {{{ MessageLevel::Error, "[Private Click Measurement] Got an invalid attribution."_s }}} };
    }

    DebugInfo debugInfo;
    auto data = attributionTriggerData.data;
    auto priority = attributionTriggerData.priority;
    RELEASE_LOG_INFO(PrivateClickMeasurement, "Got an attribution with attribution trigger data: %u and priority: %u.", data, priority);
    debugInfo.messages.append({ MessageLevel::Info, makeString("[Private Click Measurement] Got an attribution with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'."_s) });

    WebCore::PCM::AttributionSecondsUntilSendData secondsUntilSend { std::nullopt, std::nullopt };

    auto attribution = findPrivateClickMeasurement(sourceSite, destinationSite, applicationBundleIdentifier);
    auto& previouslyUnattributed = attribution.first;
    auto& previouslyAttributed = attribution.second;

    if (previouslyUnattributed) {
        // Always convert the pending attribution and remove it from the unattributed map.
        removeUnattributed(*previouslyUnattributed);
        secondsUntilSend = previouslyUnattributed.value().attributeAndGetEarliestTimeToSend(WTFMove(attributionTriggerData), isRunningTest);

        // We should always have a valid secondsUntilSend value for a previouslyUnattributed value because there can be no previous attribution with a higher priority.
        if (!secondsUntilSend.hasValidSecondsUntilSendValues()) {
            ASSERT_NOT_REACHED();
            return { std::nullopt, WTFMove(debugInfo) };
        }

        RELEASE_LOG_INFO(PrivateClickMeasurement, "Converted a stored ad click with attribution trigger data: %u and priority: %u.", data, priority);
        debugInfo.messages.append({ MessageLevel::Info, makeString("[Private Click Measurement] Converted a stored ad click with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'."_s) });

        // If there is no previous attribution, or the new attribution has higher priority, insert/update the database.
        if (!previouslyAttributed || previouslyUnattributed.value().hasHigherPriorityThan(*previouslyAttributed)) {
            insertPrivateClickMeasurement(WTFMove(*previouslyUnattributed), PrivateClickMeasurementAttributionType::Attributed);

            RELEASE_LOG_INFO(PrivateClickMeasurement, "Replaced a previously converted ad click with a new one with attribution data: %u and priority: %u because it had higher priority.", data, priority);
            debugInfo.messages.append({ MessageLevel::Info, makeString("[Private Click Measurement] Replaced a previously converted ad click with a new one with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "' because it had higher priority."_s) });
        }
    } else if (previouslyAttributed) {
        // If we have no new attribution, re-attribute the old one to respect the new priority, but only if this report has
        // not been sent to the source or destination site yet.
        if (!previouslyAttributed.value().hasPreviouslyBeenReported()) {
            auto secondsUntilSend = previouslyAttributed.value().attributeAndGetEarliestTimeToSend(WTFMove(attributionTriggerData), isRunningTest);
            if (!secondsUntilSend.hasValidSecondsUntilSendValues())
                return { std::nullopt, WTFMove(debugInfo) };

            insertPrivateClickMeasurement(WTFMove(*previouslyAttributed), PrivateClickMeasurementAttributionType::Attributed);

            RELEASE_LOG_INFO(PrivateClickMeasurement, "Re-converted an ad click with a new one with attribution trigger data: %u and priority: %u because it had higher priority.", data, priority);
            debugInfo.messages.append({ MessageLevel::Info, makeString("[Private Click Measurement] Re-converted an ad click with a new one with attribution trigger data: '"_s, data, "' and priority: '"_s, priority, "'' because it had higher priority."_s) });
        }
    }

    if (!secondsUntilSend.hasValidSecondsUntilSendValues())
        return { std::nullopt, WTFMove(debugInfo) };

    return { secondsUntilSend, WTFMove(debugInfo) };
}

void Database::removeUnattributed(WebCore::PrivateClickMeasurement& attribution)
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);
    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return;

    auto scopedStatement = this->scopedStatement(m_removeUnattributedStatement, removeUnattributedQuery, "removeUnattributed"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->bindText(3, attribution.sourceApplicationBundleID()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::removeUnattributed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

Vector<WebCore::PrivateClickMeasurement> Database::allAttributedPrivateClickMeasurement()
{
    ASSERT(!RunLoop::isMain());
    auto attributedScopedStatement = this->scopedStatement(m_allAttributedPrivateClickMeasurementStatement, allAttributedPrivateClickMeasurementQuery, "allAttributedPrivateClickMeasurement"_s);

    if (!attributedScopedStatement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::allAttributedPrivateClickMeasurement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    Vector<WebCore::PrivateClickMeasurement> attributions;
    while (attributedScopedStatement->step() == SQLITE_ROW)
        attributions.append(buildPrivateClickMeasurementFromDatabase(*attributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed));

    return attributions;
}

String Database::privateClickMeasurementToStringForTesting() const
{
    ASSERT(!RunLoop::isMain());
    auto privateClickMeasurementDataExists = m_database.prepareStatement("SELECT (SELECT COUNT(*) FROM UnattributedPrivateClickMeasurement) as cnt1, (SELECT COUNT(*) FROM AttributedPrivateClickMeasurement) as cnt2"_s);
    if (!privateClickMeasurementDataExists || privateClickMeasurementDataExists->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::privateClickMeasurementToStringForTesting failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    if (!privateClickMeasurementDataExists->columnInt(0) && !privateClickMeasurementDataExists->columnInt(1))
        return "\nNo stored Private Click Measurement data.\n"_s;

    auto unattributedScopedStatement = this->scopedStatement(m_allUnattributedPrivateClickMeasurementAttributionsStatement, allUnattributedPrivateClickMeasurementAttributionsQuery, "privateClickMeasurementToStringForTesting"_s);

    if (!unattributedScopedStatement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::privateClickMeasurementToStringForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    unsigned unattributedNumber = 0;
    StringBuilder builder;
    while (unattributedScopedStatement->step() == SQLITE_ROW) {
        const char* prefix = unattributedNumber ? "" : "Unattributed Private Click Measurements:";
        builder.append(prefix, "\nWebCore::PrivateClickMeasurement ", ++unattributedNumber, '\n',
            attributionToStringForTesting(buildPrivateClickMeasurementFromDatabase(*unattributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Unattributed)));
    }

    auto attributedScopedStatement = this->scopedStatement(m_allAttributedPrivateClickMeasurementStatement, allAttributedPrivateClickMeasurementQuery, "privateClickMeasurementToStringForTesting"_s);

    if (!attributedScopedStatement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::privateClickMeasurementToStringForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
    }

    unsigned attributedNumber = 0;
    while (attributedScopedStatement->step() == SQLITE_ROW) {
        if (!attributedNumber)
            builder.append(unattributedNumber ? "\n" : "", "Attributed Private Click Measurements:");
        builder.append("\nWebCore::PrivateClickMeasurement ", ++attributedNumber + unattributedNumber, '\n',
            attributionToStringForTesting(buildPrivateClickMeasurementFromDatabase(*attributedScopedStatement.get(), PrivateClickMeasurementAttributionType::Attributed)));
    }
    return builder.toString();
}

String Database::attributionToStringForTesting(const WebCore::PrivateClickMeasurement& pcm) const
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomain = pcm.sourceSite().registrableDomain;
    auto destinationSiteDomain = pcm.destinationSite().registrableDomain;
    auto sourceID = pcm.sourceID();

    StringBuilder builder;
    builder.append("Source site: ", sourceSiteDomain, "\nAttribute on site: ", destinationSiteDomain, "\nSource ID: ", sourceID);

    if (auto& triggerData = pcm.attributionTriggerData()) {
        auto attributionTriggerData = triggerData->data;
        auto priority = triggerData->priority;
        auto earliestTimeToSend = pcm.timesToSend().sourceEarliestTimeToSend;

        builder.append("\nAttribution trigger data: ", attributionTriggerData, "\nAttribution priority: ", priority, "\nAttribution earliest time to send: ");
        if (!earliestTimeToSend)
            builder.append("Not set");
        else {
            auto secondsUntilSend = *earliestTimeToSend - WallTime::now();
            builder.append((secondsUntilSend >= 24_h && secondsUntilSend <= 48_h) ? "Within 24-48 hours" : "Outside 24-48 hours");
        }

        builder.append("\nDestination token: ");
        if (!triggerData->destinationSecretToken)
            builder.append("Not set");
        else
            builder.append("\ntoken: ", triggerData->destinationSecretToken->tokenBase64URL, "\nsignature: ", triggerData->destinationSecretToken->signatureBase64URL, "\nkey: ", triggerData->destinationSecretToken->keyIDBase64URL);
    } else
        builder.append("\nNo attribution trigger data.");
    builder.append("\nApplication bundle identifier: ", pcm.sourceApplicationBundleID(), '\n');

    return builder.toString();
}

void Database::markAttributedPrivateClickMeasurementsAsExpiredForTesting()
{
    ASSERT(!RunLoop::isMain());
    auto expiredTimeToSend = WallTime::now() - 1_h;

    auto transactionScope = beginTransactionIfNecessary();

    auto earliestTimeToSendToSourceStatement = m_database.prepareStatement("UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToSource = ?"_s);
    auto earliestTimeToSendToDestinationStatement = m_database.prepareStatement("UPDATE AttributedPrivateClickMeasurement SET earliestTimeToSendToDestination = null"_s);

    if (!earliestTimeToSendToSourceStatement
        || earliestTimeToSendToSourceStatement->bindInt(1, expiredTimeToSend.secondsSinceEpoch().value()) != SQLITE_OK
        || earliestTimeToSendToSourceStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::markAttributedPrivateClickMeasurementsAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    if (!earliestTimeToSendToDestinationStatement || earliestTimeToSendToDestinationStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::markAttributedPrivateClickMeasurementsAsExpiredForTesting, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::clearPrivateClickMeasurement(std::optional<WebCore::RegistrableDomain> domain)
{
    ASSERT(!RunLoop::isMain());

    // Default to clear all entries if no domain is specified.
    String bindParameter;
    if (domain) {
        auto domainIDToMatch = domainID(*domain);
        if (!domainIDToMatch)
            return;

        bindParameter = String::number(*domainIDToMatch);
    } else
        bindParameter = "%"_s;

    auto transactionScope = beginTransactionIfNecessary();

    auto clearAllPrivateClickMeasurementScopedStatement = this->scopedStatement(m_clearAllPrivateClickMeasurementStatement, clearAllPrivateClickMeasurementQuery, "clearPrivateClickMeasurement"_s);

    if (!clearAllPrivateClickMeasurementScopedStatement
        || clearAllPrivateClickMeasurementScopedStatement->bindText(1, bindParameter) != SQLITE_OK
        || clearAllPrivateClickMeasurementScopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - ResourceLoadStatisticsStore::clearPrivateClickMeasurement clearAllPrivateClickMeasurementScopedStatement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::clearExpiredPrivateClickMeasurement()
{
    ASSERT(!RunLoop::isMain());
    auto expirationTimeFrame = WallTime::now() - WebCore::PrivateClickMeasurement::maxAge();
    auto scopedStatement = this->scopedStatement(m_clearExpiredPrivateClickMeasurementStatement, clearExpiredPrivateClickMeasurementQuery, "clearExpiredPrivateClickMeasurement"_s);

    if (!scopedStatement
        || scopedStatement->bindDouble(1, expirationTimeFrame.secondsSinceEpoch().value()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::clearExpiredPrivateClickMeasurement, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::clearSentAttribution(WebCore::PrivateClickMeasurement&& attribution, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    ASSERT(!RunLoop::isMain());
    auto timesToSend = earliestTimesToSend(attribution);
    auto sourceEarliestTimeToSend = timesToSend.first;
    auto destinationEarliestTimeToSend = timesToSend.second;

    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);
    auto sourceApplicationBundleID = attribution.sourceApplicationBundleID();

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return;

    switch (attributionReportEndpoint) {
    case WebCore::PCM::AttributionReportEndpoint::Source:
        if (!sourceEarliestTimeToSend) {
            ASSERT_NOT_REACHED();
            return;
        }
        markReportAsSentToSource(*sourceSiteDomainID, *destinationSiteDomainID, sourceApplicationBundleID);
        sourceEarliestTimeToSend = std::nullopt;
        break;
    case WebCore::PCM::AttributionReportEndpoint::Destination:
        if (!destinationEarliestTimeToSend) {
            ASSERT_NOT_REACHED();
            return;
        }
        markReportAsSentToDestination(*sourceSiteDomainID, *destinationSiteDomainID, sourceApplicationBundleID);
        destinationEarliestTimeToSend = std::nullopt;
    }

    // Don't clear the attribute from the database unless it has been reported both to the source and destination site.
    if (destinationEarliestTimeToSend || sourceEarliestTimeToSend)
        return;

    auto clearAttributedStatement = m_database.prepareStatement("DELETE FROM AttributedPrivateClickMeasurement WHERE sourceSiteDomainID = ? AND destinationSiteDomainID = ? AND sourceApplicationBundleID = ?"_s);
    if (!clearAttributedStatement
        || clearAttributedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || clearAttributedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || clearAttributedStatement->bindText(3, sourceApplicationBundleID) != SQLITE_OK
        || clearAttributedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::clearSentAttribution failed to step, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::markReportAsSentToDestination(SourceDomainID sourceSiteDomainID, DestinationDomainID destinationSiteDomainID, const ApplicationBundleIdentifier& sourceApplicationBundleID)
{
    ASSERT(!RunLoop::isMain());
    auto scopedStatement = this->scopedStatement(m_markReportAsSentToDestinationStatement, markReportAsSentToDestinationQuery, "markReportAsSentToDestination"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->bindText(3, sourceApplicationBundleID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "Database::markReportAsSentToDestination, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

void Database::markReportAsSentToSource(SourceDomainID sourceSiteDomainID, DestinationDomainID destinationSiteDomainID, const ApplicationBundleIdentifier& sourceApplicationBundleID)
{
    ASSERT(!RunLoop::isMain());
    auto scopedStatement = this->scopedStatement(m_markReportAsSentToSourceStatement, markReportAsSentToSourceQuery, "markReportAsSentToSource"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->bindText(3, sourceApplicationBundleID) != SQLITE_OK
        || scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "Database::markReportAsSentToSource, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }
}

std::pair<std::optional<Database::SourceEarliestTimeToSend>, std::optional<Database::DestinationEarliestTimeToSend>> Database::earliestTimesToSend(const WebCore::PrivateClickMeasurement& attribution)
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomainID = domainID(attribution.sourceSite().registrableDomain);
    auto destinationSiteDomainID = domainID(attribution.destinationSite().registrableDomain);

    if (!sourceSiteDomainID || !destinationSiteDomainID)
        return std::make_pair(std::nullopt, std::nullopt);

    auto scopedStatement = this->scopedStatement(m_earliestTimesToSendStatement, earliestTimesToSendQuery, "earliestTimesToSend"_s);

    if (!scopedStatement
        || scopedStatement->bindInt(1, *sourceSiteDomainID) != SQLITE_OK
        || scopedStatement->bindInt(2, *destinationSiteDomainID) != SQLITE_OK
        || scopedStatement->bindText(3, attribution.sourceApplicationBundleID()) != SQLITE_OK
        || scopedStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "Database::earliestTimesToSend, error message: %" PUBLIC_LOG_STRING, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return { };
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

std::optional<Database::DomainID> Database::domainID(const WebCore::RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto scopedStatement = this->scopedStatement(m_domainIDFromStringStatement, domainIDFromStringQuery, "domainID"_s);
    if (!scopedStatement || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::domainIDFromString failed. Error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    
    if (scopedStatement->step() != SQLITE_ROW)
        return std::nullopt;

    return scopedStatement->columnInt(0);
}

String Database::getDomainStringFromDomainID(DomainID domainID) const
{
    ASSERT(!RunLoop::isMain());
    auto result = emptyString();
    
    auto scopedStatement = this->scopedStatement(m_domainStringFromDomainIDStatement, domainStringFromDomainIDQuery, "getDomainStringFromDomainID"_s);
    if (!scopedStatement
        || scopedStatement->bindInt(1, domainID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::getDomainStringFromDomainID. Statement failed to prepare or bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return result;
    }
    
    if (scopedStatement->step() == SQLITE_ROW)
        result = m_domainStringFromDomainIDStatement->columnText(0);
    
    return result;
}

std::optional<Database::DomainID> Database::ensureDomainID(const WebCore::RegistrableDomain& domain)
{
    if (auto existingID = domainID(domain))
        return existingID;

    auto scopedStatement = this->scopedStatement(m_insertObservedDomainStatement, insertObservedDomainQuery, "insertObservedDomain"_s);
    if (!scopedStatement
        || scopedStatement->bindText(1, domain.string()) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::ensureDomainID failed to bind, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    if (scopedStatement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::ensureDomainID failed to commit, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return domainID(domain);
}

void Database::destroyStatements()
{
    m_setUnattributedPrivateClickMeasurementAsExpiredStatement = nullptr;
    m_findUnattributedStatement = nullptr;
    m_findAttributedStatement = nullptr;
    m_removeUnattributedStatement = nullptr;
    m_allAttributedPrivateClickMeasurementStatement = nullptr;
    m_allUnattributedPrivateClickMeasurementAttributionsStatement = nullptr;
    m_clearAllPrivateClickMeasurementStatement = nullptr;
    m_clearExpiredPrivateClickMeasurementStatement = nullptr;
    m_earliestTimesToSendStatement = nullptr;
    m_markReportAsSentToSourceStatement = nullptr;
    m_markReportAsSentToDestinationStatement = nullptr;
    m_domainIDFromStringStatement = nullptr;
    m_domainStringFromDomainIDStatement = nullptr;
    m_insertObservedDomainStatement = nullptr;
}

void Database::addDestinationTokenColumnsIfNecessary()
{
    constexpr auto attributedTableName = "AttributedPrivateClickMeasurement"_s;
    String destinationKeyIDColumnName("destinationKeyID"_s);
    auto columns = columnsForTable(attributedTableName);
    if (!columns.size() || columns.last() != destinationKeyIDColumnName) {
        addMissingColumnToTable(attributedTableName, "destinationToken TEXT"_s);
        addMissingColumnToTable(attributedTableName, "destinationSignature TEXT"_s);
        addMissingColumnToTable(attributedTableName, "destinationKeyID TEXT"_s);
    }
}

} // namespace WebKit::PCM
