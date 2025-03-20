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
#include "ResourceMonitorPersistence.h"

#include "Logging.h"
#include "SQLiteStatement.h"
#include <wtf/FileSystem.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

#if ENABLE(CONTENT_EXTENSIONS)

#define RESOURCEMONITOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(ResourceMonitoring, "ResourceMonitorPersistence::" fmt, ##__VA_ARGS__)

namespace WebCore {

ResourceMonitorPersistence::ResourceMonitorPersistence()
{
    ASSERT(!isMainThread());
}

ResourceMonitorPersistence::~ResourceMonitorPersistence()
{
    ASSERT(!isMainThread());

    if (m_sqliteDB)
        closeDatabase();
}

static constexpr auto recordTableName = "records"_s;
static constexpr auto hostIndexName = "idx_records_host"_s;
static constexpr auto accessIndexName = "idx_records_access"_s;

static constexpr auto createRecordTableSQL = "CREATE TABLE records (host TEXT NOT NULL, access REAL NOT NULL)"_s;
static constexpr auto createHostIndexSQL =  "CREATE INDEX idx_records_host ON records(host)"_s;
static constexpr auto createAccessIndexSQL = "CREATE INDEX idx_records_access ON records(access)"_s;

static constexpr auto selectAllRecordsSQL = "SELECT host,access FROM records ORDER BY access"_s;
static constexpr auto deleteAllRecordsSQL = "DELETE FROM records"_s;
static constexpr auto deleteExpiredRecordsSQL = "DELETE FROM records WHERE access < ?"_s;
static constexpr auto insertRecordSQL = "INSERT INTO records (host, access) VALUES (?, ?)"_s;

static double continuousApproximateTimeToDouble(ContinuousApproximateTime time)
{
    return time.approximateWallTime().secondsSinceEpoch().value();
}

static ContinuousApproximateTime doubleToContinuousApproximateTime(double timestamp)
{
    auto wallTime = WallTime::fromRawSeconds(timestamp);
    return ContinuousApproximateTime::fromWallTime(wallTime);
}

void ResourceMonitorPersistence::reportSQLError(ASCIILiteral method, ASCIILiteral action)
{
    RELEASE_LOG_ERROR(ResourceMonitoring, "ResourceMonitorPersistence::%" PUBLIC_LOG_STRING ": Failed to %" PUBLIC_LOG_STRING " (%d) - %" PUBLIC_LOG_STRING, method.characters(), action.characters(), m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
}

bool ResourceMonitorPersistence::openDatabase(String&& path)
{
    ASSERT(!isMainThread());

    FileSystem::makeAllDirectories(FileSystem::parentPath(path));

    m_sqliteDB = makeUnique<SQLiteDatabase>();

    auto reportErrorAndCloseDatabase = [&](ASCIILiteral action) {
        reportSQLError("openDatabase"_s, action);
        closeDatabase();
        return false;
    };

    if (!m_sqliteDB->open(path, SQLiteDatabase::OpenMode::ReadWriteCreate, SQLiteDatabase::OpenOptions::CanSuspendWhileLocked))
        return reportErrorAndCloseDatabase("open database"_s);

    if (!m_sqliteDB->tableExists(recordTableName)) {
        if (!m_sqliteDB->executeCommand(createRecordTableSQL))
            return reportErrorAndCloseDatabase("create `record` table"_s);

        RESOURCEMONITOR_RELEASE_LOG("openDatabase: Table %" PUBLIC_LOG_STRING " created", recordTableName.characters());
    }

    if (!m_sqliteDB->indexExists(hostIndexName)) {
        if (!m_sqliteDB->executeCommand(createHostIndexSQL))
            return reportErrorAndCloseDatabase("create `host` index on `record` table"_s);

        RESOURCEMONITOR_RELEASE_LOG("openDatabase: Index %" PUBLIC_LOG_STRING " created", hostIndexName.characters());
    }

    if (!m_sqliteDB->indexExists(accessIndexName)) {
        if (!m_sqliteDB->executeCommand(createAccessIndexSQL))
            return reportErrorAndCloseDatabase("create `access` index on `record` table"_s);

        RESOURCEMONITOR_RELEASE_LOG("openDatabase: Index %" PUBLIC_LOG_STRING " created", accessIndexName.characters());
    }

    auto insertStatement = m_sqliteDB->prepareHeapStatement(insertRecordSQL);
    if (!insertStatement)
        return reportErrorAndCloseDatabase("prepare insert statement"_s);

    m_insertSQLStatement = insertStatement.value().moveToUniquePtr();

    m_sqliteDB->turnOnIncrementalAutoVacuum();

    return true;
}

void ResourceMonitorPersistence::deleteAllRecords()
{
    ASSERT(!isMainThread());

    auto deleteStatement = m_sqliteDB->prepareStatement(deleteAllRecordsSQL);
    if (!deleteStatement || !deleteStatement->executeCommand())
        return reportSQLError("deleteAllRecords"_s, "delete all records"_s);

    RESOURCEMONITOR_RELEASE_LOG("deleteAllRecords: Deleted all records");
}

void ResourceMonitorPersistence::deleteExpiredRecords(ContinuousApproximateTime now, Seconds duration)
{
    ASSERT(!isMainThread());

    auto expirationTime = continuousApproximateTimeToDouble(now - duration);

    auto deleteStatement = m_sqliteDB->prepareStatement(deleteExpiredRecordsSQL);
    if (!deleteStatement || deleteStatement->bindDouble(1, expirationTime) != SQLITE_OK || !deleteStatement->executeCommand())
        return reportSQLError("deleteExpiredRecords"_s, "delete expired records"_s);

    RESOURCEMONITOR_RELEASE_LOG("deleteExpiredRecords: Deleted expired records");
}

Vector<ResourceMonitorPersistence::Record> ResourceMonitorPersistence::importRecords()
{
    ASSERT(!isMainThread());

    auto selectStatement = m_sqliteDB->prepareStatement(selectAllRecordsSQL);
    if (!selectStatement) {
        reportSQLError("importRecords"_s, "fetch records of host and access"_s);
        return { };
    }

    Vector<Record> records;
    while (selectStatement->step() == SQLITE_ROW) {
        auto host = selectStatement->columnText(0);
        auto access = selectStatement->columnDouble(1);

        records.append({ host, doubleToContinuousApproximateTime(access) });
    }

    return records;
}

void ResourceMonitorPersistence::recordAccess(const String& host, ContinuousApproximateTime time)
{
    ASSERT(!isMainThread());

    if (!m_insertSQLStatement)
        return;

    m_insertSQLStatement->reset();

    double access = continuousApproximateTimeToDouble(time);
    if (m_insertSQLStatement->bindText(1, host) != SQLITE_OK || m_insertSQLStatement->bindDouble(2, access) != SQLITE_OK || !m_insertSQLStatement->executeCommand())
        RESOURCEMONITOR_RELEASE_LOG("recordAccess: Failed to insert record (%d) - %" PUBLIC_LOG_STRING, m_sqliteDB->lastError(), m_sqliteDB->lastErrorMsg());
}

void ResourceMonitorPersistence::closeDatabase()
{
    ASSERT(!isMainThread());

    m_insertSQLStatement = nullptr;

    if (isDatabaseOpen()) {
        RESOURCEMONITOR_RELEASE_LOG("closeDatabase: Closing database");
        m_sqliteDB->close();
    }

    m_sqliteDB = nullptr;
}

} // namespace WebCore

#undef RESOURCEMONITOR_RELEASE_LOG

#endif
