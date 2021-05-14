/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "LocalStorageDatabase.h"

#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/StorageMap.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebKit {
using namespace WebCore;

static const char getItemsQueryString[] = "SELECT key, value FROM ItemTable";

Ref<LocalStorageDatabase> LocalStorageDatabase::create(String&& databasePath, unsigned quotaInBytes)
{
    return adoptRef(*new LocalStorageDatabase(WTFMove(databasePath), quotaInBytes));
}

LocalStorageDatabase::LocalStorageDatabase(String&& databasePath, unsigned quotaInBytes)
    : m_databasePath(WTFMove(databasePath))
    , m_quotaInBytes(quotaInBytes)
{
    ASSERT(!RunLoop::isMain());
}

LocalStorageDatabase::~LocalStorageDatabase()
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_isClosed);
}

bool LocalStorageDatabase::openDatabase(ShouldCreateDatabase shouldCreateDatabase)
{
    ASSERT(!RunLoop::isMain());
    if (!FileSystem::fileExists(m_databasePath) && shouldCreateDatabase == ShouldCreateDatabase::No)
        return true;

    if (m_databasePath.isEmpty()) {
        LOG_ERROR("Filename for local storage database is empty - cannot open for persistent storage");
        return false;
    }

    if (!m_database.open(m_databasePath)) {
        LOG_ERROR("Failed to open database file %s for local storage", m_databasePath.utf8().data());
        return false;
    }

    // Since a WorkQueue isn't bound to a specific thread, we have to disable threading checks
    // even though we never access the database from different threads simultaneously.
    m_database.disableThreadingChecks();

    if (!migrateItemTableIfNeeded()) {
        // We failed to migrate the item table. In order to avoid trying to migrate the table over and over,
        // just delete it and start from scratch.
        if (!m_database.executeCommand("DROP TABLE ItemTable"))
            LOG_ERROR("Failed to delete table ItemTable for local storage");
    }

    if (!m_database.executeCommand("CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)")) {
        LOG_ERROR("Failed to create table ItemTable for local storage");
        return false;
    }

    return true;
}

bool LocalStorageDatabase::migrateItemTableIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.tableExists("ItemTable"))
        return true;

    SQLiteStatement query(m_database, "SELECT value FROM ItemTable LIMIT 1");

    // This query isn't ever executed, it's just used to check the column type.
    if (query.isColumnDeclaredAsBlob(0))
        return true;

    // Create a new table with the right type, copy all the data over to it and then replace the new table with the old table.
    static const char* commands[] = {
        "DROP TABLE IF EXISTS ItemTable2",
        "CREATE TABLE ItemTable2 (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)",
        "INSERT INTO ItemTable2 SELECT * from ItemTable",
        "DROP TABLE ItemTable",
        "ALTER TABLE ItemTable2 RENAME TO ItemTable",
        0,
    };

    SQLiteTransaction transaction(m_database);
    transaction.begin();

    for (size_t i = 0; commands[i]; ++i) {
        if (m_database.executeCommand(commands[i]))
            continue;

        LOG_ERROR("Failed to migrate table ItemTable for local storage when executing: %s", commands[i]);

        return false;
    }

    transaction.commit();
    return true;
}

HashMap<String, String> LocalStorageDatabase::items() const
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        return { };

    auto query = scopedStatement(m_getItemsStatement, getItemsQueryString);
    if (!query) {
        LOG_ERROR("Unable to select items from ItemTable for local storage");
        return { };
    }

    HashMap<String, String> items;
    int result = query->step();
    while (result == SQLITE_ROW) {
        String key = query->getColumnText(0);
        String value = query->getColumnBlobAsString(1);
        if (!key.isNull() && !value.isNull())
            items.add(WTFMove(key), WTFMove(value));
        result = query->step();
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Error reading items from ItemTable for local storage");

    return items;
}

void LocalStorageDatabase::removeItem(const String& key, String& oldValue)
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        return;

    oldValue = item(key);
    if (oldValue.isNull())
        return;

    auto deleteStatement = scopedStatement(m_deleteItemStatement, "DELETE FROM ItemTable WHERE key=?"_s);
    if (!deleteStatement) {
        LOG_ERROR("Failed to prepare delete statement - cannot write to local storage database");
        return;
    }
    deleteStatement->bindText(1, key);

    int result = deleteStatement->step();
    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to delete item in the local storage database - %i", result);
        return;
    }

    if (m_databaseSize) {
        auto sizeDecrease = key.sizeInBytes() + oldValue.sizeInBytes();
        if (sizeDecrease >= *m_databaseSize)
            *m_databaseSize = 0;
        else
            *m_databaseSize -= sizeDecrease;
    }
}

String LocalStorageDatabase::item(const String& key) const
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        return { };

    auto query = scopedStatement(m_getItemStatement, "SELECT value FROM ItemTable WHERE key=?"_s);
    if (!query) {
        LOG_ERROR("Unable to get item from ItemTable for local storage");
        return { };
    }
    query->bindText(1, key);

    int result = query->step();
    if (result == SQLITE_ROW)
        return query->getColumnBlobAsString(0);
    if (result != SQLITE_DONE)
        LOG_ERROR("Error get item from ItemTable for local storage");
    return { };
}

void LocalStorageDatabase::setItem(const String& key, const String& value, String& oldValue, bool& quotaException)
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        openDatabase(ShouldCreateDatabase::Yes);
    if (!m_database.isOpen())
        return;

    oldValue = item(key);

    if (m_quotaInBytes != WebCore::StorageMap::noQuota) {
        if (!m_databaseSize)
            m_databaseSize = SQLiteFileSystem::databaseFileSize(m_databasePath);
        CheckedUint64 newDatabaseSize = *m_databaseSize;
        newDatabaseSize -= oldValue.sizeInBytes();
        newDatabaseSize += value.sizeInBytes();
        if (oldValue.isNull())
            newDatabaseSize += key.sizeInBytes();
        if (newDatabaseSize.hasOverflowed() || newDatabaseSize.unsafeGet() > m_quotaInBytes) {
            quotaException = true;
            return;
        }
        m_databaseSize = newDatabaseSize.unsafeGet();
    }

    auto insertStatement = scopedStatement(m_insertStatement, "INSERT INTO ItemTable VALUES (?, ?)"_s);
    if (!insertStatement) {
        LOG_ERROR("Failed to prepare insert statement - cannot write to local storage database");
        return;
    }

    insertStatement->bindText(1, key);
    insertStatement->bindBlob(2, value);

    int result = insertStatement->step();
    if (result != SQLITE_DONE)
        LOG_ERROR("Failed to update item in the local storage database - %i", result);
}

bool LocalStorageDatabase::clear()
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        return false;

    auto clearStatement = scopedStatement(m_clearStatement, "DELETE FROM ItemTable"_s);
    if (!clearStatement) {
        LOG_ERROR("Failed to prepare clear statement - cannot write to local storage database");
        return false;
    }

    int result = clearStatement->step();
    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to clear all items in the local storage database - %i", result);
        return false;
    }

    m_databaseSize = 0;

    return m_database.lastChanges() > 0;
}

void LocalStorageDatabase::close()
{
    ASSERT(!RunLoop::isMain());
    if (m_isClosed)
        return;
    m_isClosed = true;

    bool isEmpty = databaseIsEmpty();

    m_clearStatement = nullptr;
    m_insertStatement = nullptr;
    m_getItemStatement = nullptr;
    m_getItemsStatement = nullptr;
    m_deleteItemStatement = nullptr;

    if (m_database.isOpen())
        m_database.close();

    if (isEmpty)
        SQLiteFileSystem::deleteDatabaseFile(m_databasePath);
}

bool LocalStorageDatabase::databaseIsEmpty() const
{
    ASSERT(!RunLoop::isMain());
    if (!m_database.isOpen())
        return false;

    SQLiteStatement query(m_database, "SELECT COUNT(*) FROM ItemTable");
    if (query.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to count number of rows in ItemTable for local storage");
        return false;
    }

    int result = query.step();
    if (result != SQLITE_ROW) {
        LOG_ERROR("No results when counting number of rows in ItemTable for local storage");
        return false;
    }

    return !query.getColumnInt(0);
}

void LocalStorageDatabase::openIfExisting()
{
    ASSERT(!RunLoop::isMain());
    if (m_database.isOpen())
        return;

    openDatabase(ShouldCreateDatabase::No);

    // Prewarm the getItems statement for performance since the pages block synchronously on retrieving the items.
    if (m_database.isOpen())
        scopedStatement(m_getItemsStatement, getItemsQueryString);
}

SQLiteStatementAutoResetScope LocalStorageDatabase::scopedStatement(std::unique_ptr<SQLiteStatement>& statement, const String& query) const
{
    ASSERT(!RunLoop::isMain());
    if (!statement) {
        statement = makeUnique<SQLiteStatement>(m_database, query);
        ASSERT(m_database.isOpen());
        if (statement->prepare() != SQLITE_OK)
            return SQLiteStatementAutoResetScope { };
    }
    return SQLiteStatementAutoResetScope { statement.get() };
}

void LocalStorageDatabase::handleLowMemoryWarning()
{
    if (m_database.isOpen())
        m_database.releaseMemory();
}

} // namespace WebKit
