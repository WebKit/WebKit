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
            ASSERT_NOT_REACHED();
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
            ASSERT_NOT_REACHED();
            return createdNewFile;
        }
        createdNewFile = CreatedNewFile::Yes;
    }

    if (!m_database.open(m_storageFilePath)) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::open failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageFilePath.utf8().data());
        ASSERT_NOT_REACHED();
        return createdNewFile;
    }
    
    // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
    m_database.disableThreadingChecks();

    auto setBusyTimeout = m_database.prepareStatement("PRAGMA busy_timeout = 5000"_s);
    if (!setBusyTimeout || setBusyTimeout->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::setBusyTimeout failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
    }

    if (createdNewFile == CreatedNewFile::Yes) {
        if (!createSchema()) {
            RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::createSchema failed, error message: %" PUBLIC_LOG_STRING ", database path: %" PUBLIC_LOG_STRING, this, m_database.lastErrorMsg(), m_storageFilePath.utf8().data());
            ASSERT_NOT_REACHED();
        }
    }
    return createdNewFile;
}

void DatabaseUtilities::enableForeignKeys()
{
    auto enableForeignKeys = m_database.prepareStatement("PRAGMA foreign_keys = ON"_s);
    if (!enableForeignKeys || enableForeignKeys->step() != SQLITE_DONE) {
        RELEASE_LOG_ERROR(PrivateClickMeasurement, "%p - DatabaseUtilities::enableForeignKeys failed, error message: %" PRIVATE_LOG_STRING, this, m_database.lastErrorMsg());
        ASSERT_NOT_REACHED();
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

WebCore::PrivateClickMeasurement DatabaseUtilities::buildPrivateClickMeasurementFromDatabase(WebCore::SQLiteStatement& statement, PrivateClickMeasurementAttributionType attributionType)
{
    ASSERT(!RunLoop::isMain());
    auto sourceSiteDomain = getDomainStringFromDomainID(statement.columnInt(0));
    auto destinationSiteDomain = getDomainStringFromDomainID(statement.columnInt(1));
    auto sourceID = statement.columnInt(2);
    auto timeOfAdClick = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnDouble(5) : statement.columnDouble(3);
    auto token = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(7) : statement.columnText(4);
    auto signature = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(8) : statement.columnText(5);
    auto keyID = attributionType == PrivateClickMeasurementAttributionType::Attributed ? statement.columnText(9) : statement.columnText(6);

    WebCore::PrivateClickMeasurement attribution(WebCore::PrivateClickMeasurement::SourceID(sourceID), WebCore::PrivateClickMeasurement::SourceSite(WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(sourceSiteDomain)), WebCore::PrivateClickMeasurement::AttributionDestinationSite(WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString(destinationSiteDomain)), { }, { }, WallTime::fromRawSeconds(timeOfAdClick));

    if (attributionType == PrivateClickMeasurementAttributionType::Attributed) {
        auto attributionTriggerData = statement.columnInt(3);
        auto priority = statement.columnInt(4);
        auto sourceEarliestTimeToSendValue = statement.columnDouble(6);
        auto destinationEarliestTimeToSendValue = statement.columnDouble(10);

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

} // namespace WebKit
