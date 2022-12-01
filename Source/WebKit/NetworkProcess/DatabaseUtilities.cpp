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
#include "DatabaseUtilities.h"

#include "Logging.h"
#include "PrivateClickMeasurementManager.h"
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>

namespace WebKit {

DatabaseUtilities::DatabaseUtilities(String&& storageFilePath)
    : m_storageFilePath(WTFMove(storageFilePath))
    , m_transaction(m_database)
{
    ASSERT(!RunLoop::isMain());
}

DatabaseUtilities::~DatabaseUtilities()
{
    ASSERT(!RunLoop::isMain());
}

WebCore::SQLiteStatementAutoResetScope DatabaseUtilities::scopedStatement(std::unique_ptr<WebCore::SQLiteStatement>& statement, ASCIILiteral query, ASCIILiteral logString) const
{
    ASSERT(!RunLoop::isMain());
    if (!statement) {
        auto statementOrError = m_database.prepareHeapStatement(query);
        if (!statementOrError) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::%s failed to prepare statement, error message: %" PUBLIC_LOG_STRING, this, logString.characters(), m_database.lastErrorMsg());
            return WebCore::SQLiteStatementAutoResetScope { };
        }
        statement = statementOrError.value().moveToUniquePtr();
        ASSERT(m_database.isOpen());
    }
    return WebCore::SQLiteStatementAutoResetScope { statement.get() };
}

ScopeExit<Function<void()>> DatabaseUtilities::beginTransactionIfNecessary()
{
    ASSERT(!RunLoop::isMain());
    if (m_transaction.inProgress())
        return makeScopeExit(Function<void()> { [] { } });

    m_transaction.begin();
    return makeScopeExit(Function<void()> { [this] {
        m_transaction.commit();
    } });
}

auto DatabaseUtilities::openDatabaseAndCreateSchemaIfNecessary() -> CreatedNewFile
{
    ASSERT(!RunLoop::isMain());
    CreatedNewFile createdNewFile = CreatedNewFile::No;
    if (!FileSystem::fileExists(m_storageFilePath)) {
        if (!FileSystem::makeAllDirectories(FileSystem::parentPath(m_storageFilePath))) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::open failed, error message: Failed to create directory database path: %" PUBLIC_LOG_STRING, this, m_storageFilePath.utf8().data());
            return createdNewFile;
        }
        createdNewFile = CreatedNewFile::Yes;
    }

    if (!m_database.open(m_storageFilePath)) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::open failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageFilePath.utf8().data());
        return createdNewFile;
    }
    
    // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
    m_database.disableThreadingChecks();

    auto setBusyTimeout = m_database.prepareStatement("PRAGMA busy_timeout = 5000"_s);
    if (!setBusyTimeout || setBusyTimeout->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::setBusyTimeout failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
    }

    if (createdNewFile == CreatedNewFile::Yes) {
        if (!createSchema()) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::createSchema failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageFilePath.utf8().data());
        }
    }
    return createdNewFile;
}

void DatabaseUtilities::enableForeignKeys()
{
    auto enableForeignKeys = m_database.prepareStatement("PRAGMA foreign_keys = ON"_s);
    if (!enableForeignKeys || enableForeignKeys->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::enableForeignKeys failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
    }
}

void DatabaseUtilities::close()
{
    ASSERT(!RunLoop::isMain());
    destroyStatements();
    if (m_database.isOpen())
        m_database.close();
}

void DatabaseUtilities::interrupt()
{
    ASSERT(!RunLoop::isMain());
    if (m_database.isOpen())
        m_database.interrupt();
}

WebCore::PrivateClickMeasurement DatabaseUtilities::buildPrivateClickMeasurementFromDatabase(WebCore::SQLiteStatement& statement, PrivateClickMeasurementAttributionType attributionType) const
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomain = getDomainStringFromDomainID(statement.columnInt(0));
    auto destinationSiteDomain = getDomainStringFromDomainID(statement.columnInt(1));
    auto sourceID = statement.columnInt(2);
    auto timeOfAdClick = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnDouble(5) : statement.columnDouble(3);
    auto token = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(7) : statement.columnText(4);
    auto signature = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(8) : statement.columnText(5);
    auto keyID = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(9) : statement.columnText(6);
    auto bundleID = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(11) : statement.columnText(7);

    // Safari was the only application that used PCM when it was stored with ResourceLoadStatistics.
#if PLATFORM(MAC)
    constexpr auto safariBundleID = "com.apple.Safari"_s;
#else
    constexpr auto safariBundleID = "com.apple.mobilesafari"_s;
#endif
    if (bundleID.isEmpty())
        bundleID = safariBundleID;

    WebCore::PrivateClickMeasurement attribution(WebCore::PrivateClickMeasurement::SourceID(sourceID), WebCore::PCM::SourceSite(WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(sourceSiteDomain)), WebCore::PCM::AttributionDestinationSite(WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(destinationSiteDomain)), bundleID, WallTime::fromRawSeconds(timeOfAdClick), WebCore::PCM::AttributionEphemeral::No);

    // These indices are zero-based: https://www.sqlite.org/c3ref/column_blob.html "The leftmost column of the result set has the index 0".
    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = statement.columnInt(3);
        auto priority = statement.columnInt(4);
        auto sourceEarliestTimeToSendValue = statement.columnDouble(6);
        auto destinationEarliestTimeToSendValue = statement.columnDouble(10);
        auto destinationToken = statement.columnText(12);
        auto destinationSignature = statement.columnText(13);
        auto destinationKeyID = statement.columnText(14);

        if (attributionTriggerData != -1)
            attribution.setAttribution(WebCore::PCM::AttributionTriggerData { static_cast<uint8_t>(attributionTriggerData), WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(priority), { }, { }, { }, { }, { }, { } });

        WebCore::PCM::DestinationSecretToken destinationSecretToken;
        destinationSecretToken.tokenBase64URL = destinationToken;
        destinationSecretToken.signatureBase64URL = destinationSignature;
        destinationSecretToken.keyIDBase64URL = destinationKeyID;

        attribution.setDestinationSecretToken(WTFMove(destinationSecretToken));

        std::optional<WallTime> sourceEarliestTimeToSend;
        std::optional<WallTime> destinationEarliestTimeToSend;

        // A value of 0.0 indicates that the report has been sent to the respective site.
        if (sourceEarliestTimeToSendValue > 0.0)
            sourceEarliestTimeToSend = WallTime::fromRawSeconds(sourceEarliestTimeToSendValue);

        if (destinationEarliestTimeToSendValue > 0.0)
            destinationEarliestTimeToSend = WallTime::fromRawSeconds(destinationEarliestTimeToSendValue);

        attribution.setTimesToSend({ sourceEarliestTimeToSend, destinationEarliestTimeToSend });
    }

    WebCore::PCM::SourceSecretToken sourceSecretToken;
    sourceSecretToken.tokenBase64URL = token;
    sourceSecretToken.signatureBase64URL = signature;
    sourceSecretToken.keyIDBase64URL = keyID;

    attribution.setSourceSecretToken(WTFMove(sourceSecretToken));

    return attribution;
}

String DatabaseUtilities::stripIndexQueryToMatchStoredValue(const char* originalQuery)
{
    return makeStringByReplacingAll(String::fromLatin1(originalQuery), "CREATE UNIQUE INDEX IF NOT EXISTS"_s, "CREATE UNIQUE INDEX"_s);
}

TableAndIndexPair DatabaseUtilities::currentTableAndIndexQueries(const String& tableName)
{
    auto getTableStatement = m_database.prepareStatement("SELECT sql FROM sqlite_master WHERE tbl_name=? AND type = 'table'"_s);
    if (!getTableStatement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::currentTableAndIndexQueries Unable to prepare statement to fetch schema for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return { };
    }

    if (getTableStatement->bindText(1, tableName) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::currentTableAndIndexQueries Unable to bind statement to fetch schema for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return { };
    }

    if (getTableStatement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::currentTableAndIndexQueries error executing statement to fetch table schema, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return { };
    }

    String createTableQuery = getTableStatement->columnText(0);

    auto getIndexStatement = m_database.prepareStatement("SELECT sql FROM sqlite_master WHERE tbl_name=? AND type = 'index'"_s);
    if (!getIndexStatement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::currentTableAndIndexQueries Unable to prepare statement to fetch index for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return { };
    }

    if (getIndexStatement->bindText(1, tableName) != SQLITE_OK) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::currentTableAndIndexQueries Unable to bind statement to fetch index for the table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
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

static Expected<WebCore::SQLiteStatement, int> insertDistinctValuesInTableStatement(WebCore::SQLiteDatabase& database, const String& table)
{
    if (table == "SubframeUnderTopFrameDomains"_s)
        return database.prepareStatement("INSERT INTO SubframeUnderTopFrameDomains SELECT subFrameDomainID, MAX(lastUpdated), topFrameDomainID FROM _SubframeUnderTopFrameDomains GROUP BY subFrameDomainID, topFrameDomainID"_s);

    if (table == "SubresourceUnderTopFrameDomains"_s)
        return database.prepareStatement("INSERT INTO SubresourceUnderTopFrameDomains SELECT subresourceDomainID, MAX(lastUpdated), topFrameDomainID FROM _SubresourceUnderTopFrameDomains GROUP BY subresourceDomainID, topFrameDomainID"_s);

    if (table == "SubresourceUniqueRedirectsTo"_s)
        return database.prepareStatement("INSERT INTO SubresourceUniqueRedirectsTo SELECT subresourceDomainID, MAX(lastUpdated), toDomainID FROM _SubresourceUniqueRedirectsTo GROUP BY subresourceDomainID, toDomainID"_s);

    if (table == "TopFrameLinkDecorationsFrom"_s)
        return database.prepareStatement("INSERT INTO TopFrameLinkDecorationsFrom SELECT toDomainID, MAX(lastUpdated), fromDomainID FROM _TopFrameLinkDecorationsFrom GROUP BY toDomainID, fromDomainID"_s);

    return database.prepareStatementSlow(makeString("INSERT INTO ", table, " SELECT DISTINCT * FROM _", table));
}

void DatabaseUtilities::migrateDataToNewTablesIfNecessary()
{
    ASSERT(!RunLoop::isMain());
    if (!needsUpdatedSchema())
        return;

    WebCore::SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (auto& table : expectedTableAndIndexQueries().keys()) {
        auto alterTable = m_database.prepareStatementSlow(makeString("ALTER TABLE ", table, " RENAME TO _", table));
        if (!alterTable || alterTable->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::migrateDataToNewTablesIfNecessary failed to rename table, error message: %s", this, m_database.lastErrorMsg());
            transaction.rollback();
            return;
        }
    }

    if (!createSchema()) {
        transaction.rollback();
        return;
    }

    // Maintain the order of tables to make sure the ObservedDomains table is created first. Other tables have foreign key constraints referencing it.
    for (auto& table : sortedTables()) {
        auto migrateTableData = insertDistinctValuesInTableStatement(m_database, table);
        if (!migrateTableData || migrateTableData->step() != SQLITE_DONE) {
            transaction.rollback();
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::migrateDataToNewTablesIfNecessary (table %s) failed to migrate schema, error message: %s", this, table.characters(), m_database.lastErrorMsg());
            return;
        }
    }

    // Drop all tables at the end to avoid trashing data that references data in other tables.
    for (auto& table : sortedTables()) {
        auto dropTableQuery = m_database.prepareStatementSlow(makeString("DROP TABLE _", table));
        if (!dropTableQuery || dropTableQuery->step() != SQLITE_DONE) {
            transaction.rollback();
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::migrateDataToNewTablesIfNecessary failed to drop temporary tables, error message: %s", this, m_database.lastErrorMsg());
            return;
        }
    }

    if (!createUniqueIndices()) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::migrateDataToNewTablesIfNecessary failed to create unique indices, error message: %s", this, m_database.lastErrorMsg());
        transaction.rollback();
        return;
    }

    transaction.commit();
}

Vector<String> DatabaseUtilities::columnsForTable(ASCIILiteral tableName)
{
    auto statement = m_database.prepareStatementSlow(makeString("PRAGMA table_info(", tableName, ")"));

    if (!statement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::columnsForTable Unable to prepare statement to fetch schema for table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return { };
    }

    Vector<String> columns;
    while (statement->step() == SQLITE_ROW) {
        auto name = statement->columnText(1);
        columns.append(name);
    }

    return columns;
}

bool DatabaseUtilities::addMissingColumnToTable(ASCIILiteral tableName, ASCIILiteral columnName)
{
    auto statement = m_database.prepareStatementSlow(makeString("ALTER TABLE ", tableName, " ADD COLUMN ", columnName));
    if (!statement) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::addMissingColumnToTable Unable to prepare statement to add missing columns to table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return false;
    }
    if (statement->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - Database::addMissingColumnToTable error executing statement to add missing columns to table, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        return false;
    }
    return true;
}

} // namespace WebKit
