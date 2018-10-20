/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SearchPopupMenuDB.h"

#include "FileSystem.h"
#include "SQLiteFileSystem.h"
#include "SQLiteTransaction.h"

namespace WebCore {

static const int schemaVersion = 1;
static constexpr auto createSearchTableSQL {
    "CREATE TABLE IF NOT EXISTS Search ("
    "    name TEXT NOT NULL,"
    "    position INTEGER NOT NULL,"
    "    value TEXT,"
    "    UNIQUE(name, position)"
    ");"_s
};
static constexpr auto loadSearchTermsForNameSQL {
    "SELECT value FROM Search "
    "WHERE name = ? "
    "ORDER BY position;"_s
};
static constexpr auto insertSearchTermSQL {
    "INSERT INTO Search(name, position, value) "
    "VALUES(?, ?, ?);"_s
};
static constexpr auto removeSearchTermsForNameSQL {
    "DELETE FROM Search where name = ?;"_s
};

SearchPopupMenuDB& SearchPopupMenuDB::singleton()
{
    static SearchPopupMenuDB instance;
    return instance;
}

SearchPopupMenuDB::SearchPopupMenuDB()
    : m_databaseFilename(FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "autosave-search.db"))
{
}

SearchPopupMenuDB::~SearchPopupMenuDB()
{
    closeDatabase();
}

void SearchPopupMenuDB::saveRecentSearches(const String& name, const Vector<RecentSearch>& searches)
{
    if (!m_database.isOpen()) {
        if (!openDatabase())
            return;
    }

    bool success = true;

    SQLiteTransaction transaction(m_database, false);

    m_removeSearchTermsForNameStatement->bindText(1, name);
    auto stepRet = m_removeSearchTermsForNameStatement->step();
    success = stepRet == SQLITE_DONE;
    m_removeSearchTermsForNameStatement->reset();

    int index = 0;
    for (const auto& search : searches) {
        m_insertSearchTermStatement->bindText(1, name);
        m_insertSearchTermStatement->bindInt(2, index);
        m_insertSearchTermStatement->bindText(3, search.string);
        if (success) {
            stepRet = m_insertSearchTermStatement->step();
            success = stepRet == SQLITE_DONE;
        }
        m_insertSearchTermStatement->reset();
        index++;
    }

    if (success)
        transaction.commit();
    checkSQLiteReturnCode(stepRet);
}

void SearchPopupMenuDB::loadRecentSearches(const String& name, Vector<RecentSearch>& searches)
{
    if (!m_database.isOpen()) {
        if (!openDatabase())
            return;
    }

    searches.clear();

    m_loadSearchTermsForNameStatement->bindText(1, name);
    while (m_loadSearchTermsForNameStatement->step() == SQLITE_ROW) {
        // We are choosing not to use or store search times on Windows at this time, so for now it's OK to use a "distant past" time as a placeholder.
        searches.append({ m_loadSearchTermsForNameStatement->getColumnText(0), -WallTime::infinity() });
    }
    m_loadSearchTermsForNameStatement->reset();
}


bool SearchPopupMenuDB::checkDatabaseValidity()
{
    ASSERT(m_database.isOpen());

    if (!m_database.tableExists("Search"))
        return false;

    SQLiteStatement integrity(m_database, "PRAGMA quick_check;");
    if (integrity.prepare() != SQLITE_OK) {
        LOG_ERROR("Failed to execute database integrity check");
        return false;
    }

    int resultCode = integrity.step();
    if (resultCode != SQLITE_ROW) {
        LOG_ERROR("Integrity quick_check step returned %d", resultCode);
        return false;
    }

    int columns = integrity.columnCount();
    if (columns != 1) {
        LOG_ERROR("Received %i columns performing integrity check, should be 1", columns);
        return false;
    }

    String resultText = integrity.getColumnText(0);

    if (resultText != "ok") {
        LOG_ERROR("Search autosave database integrity check failed - %s", resultText.ascii().data());
        return false;
    }

    return true;
}

void SearchPopupMenuDB::deleteAllDatabaseFiles()
{
    closeDatabase();

    FileSystem::deleteFile(m_databaseFilename);
    FileSystem::deleteFile(m_databaseFilename + "-shm");
    FileSystem::deleteFile(m_databaseFilename + "-wal");
}

bool SearchPopupMenuDB::openDatabase()
{
    bool existsDatabaseFile = SQLiteFileSystem::ensureDatabaseFileExists(m_databaseFilename, false);

    if (existsDatabaseFile) {
        if (m_database.open(m_databaseFilename, false)) {
            if (!checkDatabaseValidity()) {
                // delete database and try to re-create again
                LOG_ERROR("Search autosave database validity check failed, attempting to recreate the database");
                m_database.close();
                deleteAllDatabaseFiles();
                existsDatabaseFile = false;
            }
        } else {
            LOG_ERROR("Failed to open search autosave database: %s, attempting to recreate the database", m_databaseFilename.utf8().data());
            deleteAllDatabaseFiles();
            existsDatabaseFile = false;
        }
    }

    if (!existsDatabaseFile) {
        if (!FileSystem::makeAllDirectories(FileSystem::directoryName(m_databaseFilename)))
            LOG_ERROR("Failed to create the search autosave database path %s", m_databaseFilename.utf8().data());

        m_database.open(m_databaseFilename, false);
    }

    if (!m_database.isOpen())
        return false;

    if (!m_database.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum (%d %s)", m_database.lastError(), m_database.lastErrorMsg());

    verifySchemaVersion();

    bool databaseValidity = true;
    if (!existsDatabaseFile || !m_database.tableExists("Search"))
        databaseValidity = databaseValidity && (executeSimpleSql(createSearchTableSQL) == SQLITE_DONE);

    if (!databaseValidity) {
        // give up create database at this time (search terms will not be saved)
        m_database.close();
        deleteAllDatabaseFiles();
        return false;
    }

    m_database.setSynchronous(SQLiteDatabase::SyncNormal);
    
    m_loadSearchTermsForNameStatement = createPreparedStatement(loadSearchTermsForNameSQL);
    m_insertSearchTermStatement = createPreparedStatement(insertSearchTermSQL);
    m_removeSearchTermsForNameStatement = createPreparedStatement(removeSearchTermsForNameSQL);

    return true;
}

void SearchPopupMenuDB::closeDatabase()
{
    if (m_database.isOpen()) {
        m_loadSearchTermsForNameStatement->finalize();
        m_insertSearchTermStatement->finalize();
        m_removeSearchTermsForNameStatement->finalize();
        m_database.close();
    }
}

void SearchPopupMenuDB::verifySchemaVersion()
{
    int version = SQLiteStatement(m_database, "PRAGMA user_version").getColumnInt(0);
    if (version == schemaVersion)
        return;

    switch (version) {
        // Placeholder for schema version upgrade logic
        // Ensure cases fall through to the next version's upgrade logic
    case 0:
        m_database.clearAllTables();
        break;
    default:
        // This case can be reached when downgrading versions
        LOG_ERROR("Unknown search autosave database version: %d", version);
        m_database.clearAllTables();
        break;
    }

    // Update version
    executeSimpleSql(String::format("PRAGMA user_version=%d", schemaVersion));
}

void SearchPopupMenuDB::checkSQLiteReturnCode(int actual)
{
    switch (actual) {
    case SQLITE_CORRUPT:
    case SQLITE_SCHEMA:
    case SQLITE_FORMAT:
    case SQLITE_NOTADB:
        // Database has been corrupted during the run
        // so we'll recreate the db.
        deleteAllDatabaseFiles();
        openDatabase();
    }
}

int SearchPopupMenuDB::executeSimpleSql(const String& sql, bool ignoreError)
{
    SQLiteStatement statement(m_database, sql);
    int ret = statement.prepareAndStep();
    statement.finalize();

    checkSQLiteReturnCode(ret);
    if (ret != SQLITE_OK && ret != SQLITE_DONE && ret != SQLITE_ROW && !ignoreError)
        LOG_ERROR("Failed to execute %s error: %s", sql.ascii().data(), m_database.lastErrorMsg());

    return ret;
}

std::unique_ptr<SQLiteStatement> SearchPopupMenuDB::createPreparedStatement(const String& sql)
{
    auto statement = std::make_unique<SQLiteStatement>(m_database, sql);
    int ret = statement->prepare();
    ASSERT(ret == SQLITE_OK);
    return statement;
}

}
