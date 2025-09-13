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
#include "SiteIsolationEnforcementManager.h"

#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>

namespace WebKit {

using WebCore::SQLiteDatabase;

static constexpr auto recordTableName = "records"_s;
static constexpr auto registrableDomainIndexName = "index_records_registrable_domain"_s;

static constexpr auto createRecordTableSQL = "CREATE TABLE records (registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, isIsolated INTEGER NOT NULL)"_s;
static constexpr auto createHostIndexSQL =  "CREATE INDEX index_records_registrable_domain ON records(registrableDomain)"_s;
static constexpr auto selectRecordSQL = "SELECT isIsolated FROM records WHERE registrableDomain = ?"_s;
static constexpr auto insertRecordSQL = "INSERT INTO records (registrableDomain, isIsolated) VALUES (?, ?)"_s;

WTF_MAKE_TZONE_ALLOCATED_IMPL(SiteIsolationEnforcementManager);

SiteIsolationEnforcementManager::SiteIsolationEnforcementManager(const String& path)
    : m_path(path)
{
}

SiteIsolationEnforcementManager::~SiteIsolationEnforcementManager()
{
    closeDatabase();
}

void SiteIsolationEnforcementManager::closeDatabase()
{
    if (CheckedPtr sqliteDB = m_sqliteDB.get(); sqliteDB && sqliteDB->isOpen())
        sqliteDB->close();
    m_sqliteDB = nullptr;
}

bool SiteIsolationEnforcementManager::openDatabaseIfExists()
{
    if (!m_sqliteDB)
        m_sqliteDB = makeUnique<SQLiteDatabase>();

    CheckedRef sqliteDB = *m_sqliteDB;
    if (!sqliteDB->isOpen()) {
        if (!sqliteDB->open(m_path, SQLiteDatabase::OpenMode::ReadWrite, SQLiteDatabase::OpenOptions::CanSuspendWhileLocked))
            return false;
    }

    if (!sqliteDB->tableExists(recordTableName) || !sqliteDB->indexExists(registrableDomainIndexName)) {
        closeDatabase();
        return false;
    }

    if (!prepareStatements()) {
        closeDatabase();
        return false;
    }

    return true;
}

bool SiteIsolationEnforcementManager::prepareStatements()
{
    CheckedPtr sqliteDB = m_sqliteDB.get();
    RELEASE_ASSERT(sqliteDB);

    if (!m_selectSQLStatement) {
        auto selectSQLStatement = sqliteDB->prepareHeapStatement(selectRecordSQL);
        if (!selectSQLStatement) {
            closeDatabase();
            return false;
        }
        m_selectSQLStatement = selectSQLStatement.value().moveToUniquePtr();
    }

    if (!m_insertSQLStatement) {
        auto insertSQLStatement = sqliteDB->prepareHeapStatement(insertRecordSQL);
        if (!insertSQLStatement) {
            closeDatabase();
            return false;
        }
        m_insertSQLStatement = insertSQLStatement.value().moveToUniquePtr();
    }

    return true;
}

bool SiteIsolationEnforcementManager::shouldIsolateSite(const WebCore::Site& site)
{
    ASSERT(!site.isEmpty());

    CheckedPtr selectSQLStatement = m_selectSQLStatement.get();
    if (!selectSQLStatement) {
        if (!openDatabaseIfExists())
            return false;
        selectSQLStatement = m_selectSQLStatement.get();
    }

    selectSQLStatement->reset();
    if (selectSQLStatement->bindText(1, site.domain().string()) != SQLITE_OK)
        return false;

    if (selectSQLStatement->step() != SQLITE_ROW)
        return false;

    auto isIsolated = selectSQLStatement->columnInt(0);
    return !!isIsolated;
}

}
