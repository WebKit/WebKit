/*
 * Copyright (C) 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

#include "WorkQueue.h"
#include <WebCore/FileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LocalStorageDatabase> LocalStorageDatabase::create(const String& databaseFilename, PassRefPtr<WorkQueue> queue)
{
    return adoptRef(new LocalStorageDatabase(databaseFilename, queue));
}

LocalStorageDatabase::LocalStorageDatabase(const String& databaseFilename, PassRefPtr<WorkQueue> queue)
    : m_databaseFilename(databaseFilename)
    , m_queue(queue)
    , m_failedToOpenDatabase(false)
{
}

LocalStorageDatabase::~LocalStorageDatabase()
{
}

void LocalStorageDatabase::openDatabase(DatabaseOpeningStrategy openingStrategy)
{
    ASSERT(!m_database.isOpen());
    ASSERT(!m_failedToOpenDatabase);

    if (!tryToOpenDatabase(openingStrategy))
        m_failedToOpenDatabase = true;
}

bool LocalStorageDatabase::tryToOpenDatabase(DatabaseOpeningStrategy openingStrategy)
{
    if (!fileExists(m_databaseFilename) && openingStrategy == SkipIfNonExistent)
        return true;

    if (m_databaseFilename.isEmpty()) {
        LOG_ERROR("Filename for local storage database is empty - cannot open for persistent storage");
        return false;
    }

    // FIXME:
    // A StorageTracker thread may have been scheduled to delete the db we're
    // reopening, so cancel possible deletion.

    if (!m_database.open(m_databaseFilename)) {
        LOG_ERROR("Failed to open database file %s for local storage", m_databaseFilename.utf8().data());
        return false;
    }

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

    SQLiteTransaction transaction(m_database, false);
    transaction.begin();

    for (size_t i = 0; commands[i]; ++i) {
        if (m_database.executeCommand(commands[i]))
            continue;

        LOG_ERROR("Failed to migrate table ItemTable for local storage when executing: %s", commands[i]);
        transaction.rollback();

        return false;
    }

    transaction.commit();
    return true;
}

void LocalStorageDatabase::importItems(StorageMap&)
{
    // FIXME: If it can't import, then the default WebKit behavior should be that of private browsing,
    // not silently ignoring it. https://bugs.webkit.org/show_bug.cgi?id=25894

    openDatabase(SkipIfNonExistent);
    if (!m_database.isOpen())
        return;

    // FIXME: Actually import the items.
}

} // namespace WebKit
