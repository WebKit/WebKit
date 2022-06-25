/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StorageAreaSync.h"

#include "StorageAreaImpl.h"
#include "StorageSyncManager.h"
#include "StorageTracker.h"
#include <WebCore/SQLiteDatabaseTracker.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/SuddenTermination.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

// If the StorageArea undergoes rapid changes, don't sync each change to disk.
// Instead, queue up a batch of items to sync and actually do the sync at the following interval.
static const Seconds StorageSyncInterval { 1_s };

// A sane limit on how many items we'll schedule to sync all at once.  This makes it
// much harder to starve the rest of LocalStorage and the OS's IO subsystem in general.
static const int MaxiumItemsToSync = 100;

inline StorageAreaSync::StorageAreaSync(RefPtr<StorageSyncManager>&& storageSyncManager, Ref<StorageAreaImpl>&& storageArea, const String& databaseIdentifier)
    : m_syncTimer(*this, &StorageAreaSync::syncTimerFired)
    , m_itemsCleared(false)
    , m_finalSyncScheduled(false)
    , m_storageArea(WTFMove(storageArea))
    , m_syncManager(WTFMove(storageSyncManager))
    , m_databaseIdentifier(databaseIdentifier.isolatedCopy())
    , m_clearItemsWhileSyncing(false)
    , m_syncScheduled(false)
    , m_syncInProgress(false)
    , m_databaseOpenFailed(false)
    , m_syncCloseDatabase(false)
    , m_importComplete(false)
{
    ASSERT(isMainThread());
    ASSERT(m_storageArea);
    ASSERT(m_syncManager);

    // FIXME: If it can't import, then the default WebKit behavior should be that of private browsing,
    // not silently ignoring it. https://bugs.webkit.org/show_bug.cgi?id=25894
    RefPtr<StorageAreaSync> protector(this);
    m_syncManager->dispatch([protector] {
        protector->performImport();
    });
}

Ref<StorageAreaSync> StorageAreaSync::create(RefPtr<StorageSyncManager>&& storageSyncManager, Ref<StorageAreaImpl>&& storageArea, const String& databaseIdentifier)
{
    return adoptRef(*new StorageAreaSync(WTFMove(storageSyncManager), WTFMove(storageArea), databaseIdentifier));
}

StorageAreaSync::~StorageAreaSync()
{
    ASSERT(isMainThread());
    ASSERT(!m_syncTimer.isActive());
    ASSERT(m_finalSyncScheduled);
}

void StorageAreaSync::scheduleFinalSync()
{
    ASSERT(isMainThread());
    // FIXME: We do this to avoid races, but it'd be better to make things safe without blocking.
    blockUntilImportComplete();
    m_storageArea = nullptr; // This is done in blockUntilImportComplete() but this is here as a form of documentation that we must be absolutely sure the ref count cycle is broken.

    if (m_syncTimer.isActive())
        m_syncTimer.stop();
    else {
        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
    // FIXME: This is synchronous. We should do it on the background process, but
    // we should do it safely.
    m_finalSyncScheduled = true;
    syncTimerFired();

    RefPtr<StorageAreaSync> protector(this);
    m_syncManager->dispatch([protector] {
        protector->deleteEmptyDatabase();
    });
}

void StorageAreaSync::scheduleItemForSync(const String& key, const String& value)
{
    ASSERT(isMainThread());
    ASSERT(!m_finalSyncScheduled);

    m_changedItems.set(key, value);
    if (!m_syncTimer.isActive()) {
        m_syncTimer.startOneShot(StorageSyncInterval);

        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
}

void StorageAreaSync::scheduleClear()
{
    ASSERT(isMainThread());
    ASSERT(!m_finalSyncScheduled);

    m_changedItems.clear();
    m_itemsCleared = true;
    if (!m_syncTimer.isActive()) {
        m_syncTimer.startOneShot(StorageSyncInterval);

        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
}

void StorageAreaSync::scheduleCloseDatabase()
{
    ASSERT(isMainThread());
    ASSERT(!m_finalSyncScheduled);

    if (!m_database.isOpen())
        return;

    m_syncCloseDatabase = true;
    
    if (!m_syncTimer.isActive()) {
        m_syncTimer.startOneShot(StorageSyncInterval);
        
        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
}

void StorageAreaSync::syncTimerFired()
{
    ASSERT(isMainThread());

    bool partialSync = false;
    {
        Locker locker { m_syncLock };

        // Do not schedule another sync if we're still trying to complete the
        // previous one. But, if we're shutting down, schedule it anyway.
        if (m_syncInProgress && !m_finalSyncScheduled) {
            ASSERT(!m_syncTimer.isActive());
            m_syncTimer.startOneShot(StorageSyncInterval);
            return;
        }

        if (m_itemsCleared) {
            m_itemsPendingSync.clear();
            m_clearItemsWhileSyncing = true;
            m_itemsCleared = false;
        }

        HashMap<String, String>::iterator changed_it = m_changedItems.begin();
        HashMap<String, String>::iterator changed_end = m_changedItems.end();
        for (int count = 0; changed_it != changed_end; ++count, ++changed_it) {
            if (count >= MaxiumItemsToSync && !m_finalSyncScheduled) {
                partialSync = true;
                break;
            }
            m_itemsPendingSync.set(changed_it->key.isolatedCopy(), changed_it->value.isolatedCopy());
        }

        if (partialSync) {
            // We can't do the fast path of simply clearing all items, so we'll need to manually
            // remove them one by one. Done under lock since m_itemsPendingSync is modified by
            // the background thread.
            HashMap<String, String>::iterator pending_it = m_itemsPendingSync.begin();
            HashMap<String, String>::iterator pending_end = m_itemsPendingSync.end();
            for (; pending_it != pending_end; ++pending_it)
                m_changedItems.remove(pending_it->key);
        }

        if (!m_syncScheduled) {
            m_syncScheduled = true;

            // The following is balanced by the call to enableSuddenTermination in the
            // performSync function.
            disableSuddenTermination();

            RefPtr<StorageAreaSync> protector(this);
            m_syncManager->dispatch([protector] {
                protector->performSync();
            });
        }
    }

    if (partialSync) {
        // If we didn't finish syncing, then we need to finish the job later.
        ASSERT(!m_syncTimer.isActive());
        m_syncTimer.startOneShot(StorageSyncInterval);
    } else {
        // The following is balanced by the calls to disableSuddenTermination in the
        // scheduleItemForSync, scheduleClear, and scheduleFinalSync functions.
        enableSuddenTermination();

        m_changedItems.clear();
    }
}

void StorageAreaSync::openDatabase(OpenDatabaseParamType openingStrategy)
{
    ASSERT(!isMainThread());
    ASSERT(!m_database.isOpen());
    ASSERT(!m_databaseOpenFailed);

    SQLiteTransactionInProgressAutoCounter transactionCounter;

    String databaseFilename = m_syncManager->fullDatabaseFilename(m_databaseIdentifier);

    if (!FileSystem::fileExists(databaseFilename) && openingStrategy == SkipIfNonExistent)
        return;

    if (databaseFilename.isEmpty()) {
        LOG_ERROR("Filename for local storage database is empty - cannot open for persistent storage");
        markImported();
        m_databaseOpenFailed = true;
        return;
    }

    // A StorageTracker thread may have been scheduled to delete the db we're
    // reopening, so cancel possible deletion.
    StorageTracker::tracker().cancelDeletingOrigin(m_databaseIdentifier);

    if (!m_database.open(databaseFilename)) {
        LOG_ERROR("Failed to open database file %s for local storage", databaseFilename.utf8().data());
        markImported();
        m_databaseOpenFailed = true;
        return;
    }

    migrateItemTableIfNeeded();

    if (!m_database.executeCommand("CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)"_s)) {
        LOG_ERROR("Failed to create table ItemTable for local storage");
        markImported();
        m_databaseOpenFailed = true;
        return;
    }

    StorageTracker::tracker().setOriginDetails(m_databaseIdentifier, databaseFilename);
}

void StorageAreaSync::migrateItemTableIfNeeded()
{
    if (!m_database.tableExists("ItemTable"_s))
        return;

    {
        auto query = m_database.prepareStatement("SELECT value FROM ItemTable LIMIT 1"_s);
        // this query isn't ever executed.
        if (query && query->isColumnDeclaredAsBlob(0))
            return;
    }

    // alter table for backward compliance, change the value type from TEXT to BLOB.
    static const ASCIILiteral commands[] = {
        "DROP TABLE IF EXISTS ItemTable2"_s,
        "CREATE TABLE ItemTable2 (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)"_s,
        "INSERT INTO ItemTable2 SELECT * from ItemTable"_s,
        "DROP TABLE ItemTable"_s,
        "ALTER TABLE ItemTable2 RENAME TO ItemTable"_s,
        { },
    };

    SQLiteTransaction transaction(m_database, false);
    transaction.begin();
    for (size_t i = 0; commands[i]; ++i) {
        if (!m_database.executeCommand(commands[i])) {
            LOG_ERROR("Failed to migrate table ItemTable for local storage when executing: %s", commands[i].characters());
            transaction.rollback();

            // finally it will try to keep a backup of ItemTable for the future restoration.
            // NOTICE: this will essentially DELETE the current database, but that's better
            // than continually hitting this case and never being able to use the local storage.
            // if this is ever hit, it's definitely a bug.
            ASSERT_NOT_REACHED();
            if (!m_database.executeCommand("ALTER TABLE ItemTable RENAME TO Backup_ItemTable"_s))
                LOG_ERROR("Failed to save ItemTable after migration job failed.");

            return;
        }
    }
    transaction.commit();
}

void StorageAreaSync::performImport()
{
    ASSERT(!isMainThread());
    ASSERT(!m_database.isOpen());

    openDatabase(SkipIfNonExistent);
    if (!m_database.isOpen()) {
        markImported();
        return;
    }

    auto query = m_database.prepareStatement("SELECT key, value FROM ItemTable"_s);
    if (!query) {
        LOG_ERROR("Unable to select items from ItemTable for local storage");
        markImported();
        return;
    }

    HashMap<String, String> itemMap;

    int result = query->step();
    while (result == SQLITE_ROW) {
        itemMap.set(query->columnText(0), query->columnBlobAsString(1));
        result = query->step();
    }

    if (result != SQLITE_DONE) {
        LOG_ERROR("Error reading items from ItemTable for local storage");
        markImported();
        return;
    }

    m_storageArea->importItems(WTFMove(itemMap));

    markImported();
}

void StorageAreaSync::markImported()
{
    Locker locker { m_importLock };
    m_importComplete = true;
    m_importCondition.notifyOne();
}

// FIXME: In the future, we should allow use of StorageAreas while it's importing (when safe to do so).
// Blocking everything until the import is complete is by far the simplest and safest thing to do, but
// there is certainly room for safe optimization: Key/length will never be able to make use of such an
// optimization (since the order of iteration can change as items are being added). Get can return any
// item currently in the map. Get/remove can work whether or not it's in the map, but we'll need a list
// of items the import should not overwrite. Clear can also work, but it'll need to kill the import
// job first.
void StorageAreaSync::blockUntilImportComplete()
{
    ASSERT(isMainThread());

    // Fast path. We set m_storageArea to 0 only after m_importComplete being true.
    if (!m_storageArea)
        return;

    Locker locker { m_importLock };
    while (!m_importComplete)
        m_importCondition.wait(m_importLock);
    m_storageArea = nullptr;
}

void StorageAreaSync::sync(bool clearItems, const HashMap<String, String>& items)
{
    ASSERT(!isMainThread());

    if (items.isEmpty() && !clearItems && !m_syncCloseDatabase)
        return;
    if (m_databaseOpenFailed)
        return;

    if (!m_database.isOpen() && m_syncCloseDatabase) {
        m_syncCloseDatabase = false;
        return;
    }

    if (!m_database.isOpen())
        openDatabase(CreateIfNonExistent);
    if (!m_database.isOpen())
        return;

    // Closing this db because it is about to be deleted by StorageTracker.
    // The delete will be cancelled if StorageAreaSync needs to reopen the db
    // to write new items created after the request to delete the db.
    if (m_syncCloseDatabase) {
        m_syncCloseDatabase = false;
        m_database.close();
        return;
    }
    
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    // If the clear flag is set, then we clear all items out before we write any new ones in.
    if (clearItems) {
        auto clear = m_database.prepareStatement("DELETE FROM ItemTable"_s);
        if (!clear) {
            LOG_ERROR("Failed to prepare clear statement - cannot write to local storage database");
            return;
        }

        int result = clear->step();
        if (result != SQLITE_DONE) {
            LOG_ERROR("Failed to clear all items in the local storage database - %i", result);
            return;
        }
    }

    auto insert = m_database.prepareStatement("INSERT INTO ItemTable VALUES (?, ?)"_s);
    if (!insert) {
        LOG_ERROR("Failed to prepare insert statement - cannot write to local storage database");
        return;
    }

    auto remove = m_database.prepareStatement("DELETE FROM ItemTable WHERE key=?"_s);
    if (!remove) {
        LOG_ERROR("Failed to prepare delete statement - cannot write to local storage database");
        return;
    }

    HashMap<String, String>::const_iterator end = items.end();

    SQLiteTransaction transaction(m_database);
    transaction.begin();
    for (HashMap<String, String>::const_iterator it = items.begin(); it != end; ++it) {
        // Based on the null-ness of the second argument, decide whether this is an insert or a delete.
        auto& query = it->value.isNull() ? remove : insert;

        query->bindText(1, it->key);

        // If the second argument is non-null, we're doing an insert, so bind it as the value.
        if (!it->value.isNull())
            query->bindBlob(2, it->value);

        int result = query->step();
        if (result != SQLITE_DONE) {
            LOG_ERROR("Failed to update item in the local storage database - %i", result);
            break;
        }

        query->reset();
    }
    transaction.commit();
}

void StorageAreaSync::performSync()
{
    ASSERT(!isMainThread());

    bool clearItems;
    HashMap<String, String> items;
    {
        Locker locker { m_syncLock };

        ASSERT(m_syncScheduled);

        clearItems = m_clearItemsWhileSyncing;
        m_itemsPendingSync.swap(items);

        m_clearItemsWhileSyncing = false;
        m_syncScheduled = false;
        m_syncInProgress = true;
    }

    sync(clearItems, items);

    {
        Locker locker { m_syncLock };
        m_syncInProgress = false;
    }

    // The following is balanced by the call to disableSuddenTermination in the
    // syncTimerFired function.
    enableSuddenTermination();
}

void StorageAreaSync::deleteEmptyDatabase()
{
    ASSERT(!isMainThread());
    if (!m_database.isOpen())
        return;

    auto count = [&] {
        auto query = m_database.prepareStatement("SELECT COUNT(*) FROM ItemTable"_s);
        if (!query) {
            LOG_ERROR("Unable to count number of rows in ItemTable for local storage");
            return -1;
        }

        int result = query->step();
        if (result != SQLITE_ROW) {
            LOG_ERROR("No results when counting number of rows in ItemTable for local storage");
            return -1;
        }

        return query->columnInt(0);
    }();
    if (count)
        return;

    m_database.close();
    if (StorageTracker::tracker().isActive()) {
        callOnMainThread([databaseIdentifier = m_databaseIdentifier.isolatedCopy()] {
            StorageTracker::tracker().deleteOriginWithIdentifier(databaseIdentifier);
        });
    } else {
        String databaseFilename = m_syncManager->fullDatabaseFilename(m_databaseIdentifier);
        if (!FileSystem::deleteFile(databaseFilename))
            LOG_ERROR("Failed to delete database file %s\n", databaseFilename.utf8().data());
    }
}

void StorageAreaSync::scheduleSync()
{
    syncTimerFired();
}

} // namespace WebCore
