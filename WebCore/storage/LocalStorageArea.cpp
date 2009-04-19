/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "LocalStorageArea.h"

#include "CString.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "LocalStorage.h"
#include "Page.h"
#include "PageGroup.h"
#include "SQLiteStatement.h"
#include "SuddenTermination.h"

namespace WebCore {

// If the LocalStorageArea undergoes rapid changes, don't sync each change to disk.
// Instead, queue up a batch of items to sync and actually do the sync at the following interval.
static const double LocalStorageSyncInterval = 1.0;

LocalStorageArea::LocalStorageArea(SecurityOrigin* origin, LocalStorage* localStorage)
    : StorageArea(origin)
    , m_syncTimer(this, &LocalStorageArea::syncTimerFired)
    , m_itemsCleared(false)
    , m_finalSyncScheduled(false)
    , m_localStorage(localStorage)
    , m_clearItemsWhileSyncing(false)
    , m_syncScheduled(false)
    , m_importComplete(false)
{
    ASSERT(m_localStorage);
    
    if (!m_localStorage->scheduleImport(this))
        m_importComplete = true;
}

LocalStorageArea::~LocalStorageArea()
{
    ASSERT(!m_syncTimer.isActive());
}

void LocalStorageArea::scheduleFinalSync()
{
    if (m_syncTimer.isActive())
        m_syncTimer.stop();
    else {
        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
    syncTimerFired(&m_syncTimer);
    m_finalSyncScheduled = true;
}

unsigned LocalStorageArea::length() const
{
    ASSERT(isMainThread());

    if (m_importComplete)
        return internalLength();

    MutexLocker locker(m_importLock);
    if (m_importComplete)
        return internalLength();

    while (!m_importComplete)
        m_importCondition.wait(m_importLock);
    ASSERT(m_importComplete);
    
    return internalLength();
}

String LocalStorageArea::key(unsigned index, ExceptionCode& ec) const
{
    ASSERT(isMainThread());

    if (m_importComplete)
        return internalKey(index, ec);

    MutexLocker locker(m_importLock);
    if (m_importComplete)
        return internalKey(index, ec);

    while (!m_importComplete)
        m_importCondition.wait(m_importLock);
    ASSERT(m_importComplete);

    return internalKey(index, ec);
}

String LocalStorageArea::getItem(const String& key) const
{
    ASSERT(isMainThread());

    if (m_importComplete)
        return internalGetItem(key);

    MutexLocker locker(m_importLock);
    if (m_importComplete)
        return internalGetItem(key);

    String item = internalGetItem(key);
    if (!item.isNull())
        return item;

    while (!m_importComplete)
        m_importCondition.wait(m_importLock);
    ASSERT(m_importComplete);

    return internalGetItem(key);
}

void LocalStorageArea::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    ASSERT(isMainThread());

    if (m_importComplete) {
        internalSetItem(key, value, ec, frame);
        return;
    }

    MutexLocker locker(m_importLock);
    internalSetItem(key, value, ec, frame);
}

void LocalStorageArea::removeItem(const String& key, Frame* frame)
{    
    ASSERT(isMainThread());

    if (m_importComplete) {
        internalRemoveItem(key, frame);
        return;
    }

    MutexLocker locker(m_importLock);
    internalRemoveItem(key, frame);
}

bool LocalStorageArea::contains(const String& key) const
{
    ASSERT(isMainThread());

    if (m_importComplete)
        return internalContains(key);

    MutexLocker locker(m_importLock);
    if (m_importComplete)
        return internalContains(key);

    bool contained = internalContains(key);
    if (contained)
        return true;

    while (!m_importComplete)
        m_importCondition.wait(m_importLock);
    ASSERT(m_importComplete);

    return internalContains(key);
}

void LocalStorageArea::itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    scheduleItemForSync(key, newValue);
    dispatchStorageEvent(key, oldValue, newValue, sourceFrame);
}

void LocalStorageArea::itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    scheduleItemForSync(key, String());
    dispatchStorageEvent(key, oldValue, String(), sourceFrame);
}

void LocalStorageArea::areaCleared(Frame* sourceFrame)
{
    ASSERT(isMainThread());

    scheduleClear();
    dispatchStorageEvent(String(), String(), String(), sourceFrame);
}

void LocalStorageArea::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    Page* page = sourceFrame->page();
    if (!page)
        return;

    // Need to copy all relevant frames from every page to a vector, since sending the event to one frame might mutate the frame tree
    // of any given page in the group, or mutate the page group itself
    Vector<RefPtr<Frame> > frames;
    const HashSet<Page*>& pages = page->group().pages();
    
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->document()->securityOrigin()->equal(securityOrigin()))
                frames.append(frame);
        }
    }

    for (unsigned i = 0; i < frames.size(); ++i) {
        if (HTMLElement* body = frames[i]->document()->body())
            body->dispatchStorageEvent(eventNames().storageEvent, key, oldValue, newValue, sourceFrame);        
    }
}

void LocalStorageArea::scheduleItemForSync(const String& key, const String& value)
{
    ASSERT(isMainThread());
    ASSERT(!m_finalSyncScheduled);

    m_changedItems.set(key, value);
    if (!m_syncTimer.isActive()) {
        m_syncTimer.startOneShot(LocalStorageSyncInterval);

        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
}

void LocalStorageArea::scheduleClear()
{
    ASSERT(isMainThread());
    ASSERT(!m_finalSyncScheduled);

    m_changedItems.clear();
    m_itemsCleared = true;
    if (!m_syncTimer.isActive()) {
        m_syncTimer.startOneShot(LocalStorageSyncInterval);

        // The following is balanced by the call to enableSuddenTermination in the
        // syncTimerFired function.
        disableSuddenTermination();
    }
}

void LocalStorageArea::syncTimerFired(Timer<LocalStorageArea>*)
{
    ASSERT(isMainThread());

    HashMap<String, String>::iterator it = m_changedItems.begin();
    HashMap<String, String>::iterator end = m_changedItems.end();
    
    {
        MutexLocker locker(m_syncLock);

        if (m_itemsCleared) {
            m_itemsPendingSync.clear();
            m_clearItemsWhileSyncing = true;
            m_itemsCleared = false;
        }

        for (; it != end; ++it)
            m_itemsPendingSync.set(it->first.copy(), it->second.copy());

        if (!m_syncScheduled) {
            m_syncScheduled = true;

            // The following is balanced by the call to enableSuddenTermination in the
            // performSync function.
            disableSuddenTermination();

            m_localStorage->scheduleSync(this);
        }
    }

    // The following is balanced by the calls to disableSuddenTermination in the
    // scheduleItemForSync, scheduleClear, and scheduleFinalSync functions.
    enableSuddenTermination();

    m_changedItems.clear();
}

void LocalStorageArea::performImport()
{
    ASSERT(!isMainThread());
    ASSERT(!m_database.isOpen());

    String databaseFilename = m_localStorage->fullDatabaseFilename(securityOrigin());
    
    if (databaseFilename.isEmpty()) {
        LOG_ERROR("Filename for local storage database is empty - cannot open for persistent storage");
        markImported();
        return;
    }

    if (!m_database.open(databaseFilename)) {
        LOG_ERROR("Failed to open database file %s for local storage", databaseFilename.utf8().data());
        markImported();
        return;
    }

    if (!m_database.executeCommand("CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL)")) {
        LOG_ERROR("Failed to create table ItemTable for local storage");
        markImported();
        return;
    }
    
    SQLiteStatement query(m_database, "SELECT key, value FROM ItemTable");
    if (query.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to select items from ItemTable for local storage");
        markImported();
        return;
    }
    
    HashMap<String, String> itemMap;

    int result = query.step();
    while (result == SQLResultRow) {
        itemMap.set(query.getColumnText(0), query.getColumnText(1));
        result = query.step();
    }

    if (result != SQLResultDone) {
        LOG_ERROR("Error reading items from ItemTable for local storage");
        markImported();
        return;
    }

    MutexLocker locker(m_importLock);
    
    HashMap<String, String>::iterator it = itemMap.begin();
    HashMap<String, String>::iterator end = itemMap.end();
    
    for (; it != end; ++it)
        importItem(it->first, it->second);
    
    m_importComplete = true;
    m_importCondition.signal();
}

void LocalStorageArea::markImported()
{
    ASSERT(!isMainThread());

    MutexLocker locker(m_importLock);
    m_importComplete = true;
    m_importCondition.signal();
}

void LocalStorageArea::sync(bool clearItems, const HashMap<String, String>& items)
{
    ASSERT(!isMainThread());

    if (!m_database.isOpen())
        return;

    // If the clear flag is set, then we clear all items out before we write any new ones in.
    if (clearItems) {
        SQLiteStatement clear(m_database, "DELETE FROM ItemTable");
        if (clear.prepare() != SQLResultOk) {
            LOG_ERROR("Failed to prepare clear statement - cannot write to local storage database");
            return;
        }
        
        int result = clear.step();
        if (result != SQLResultDone) {
            LOG_ERROR("Failed to clear all items in the local storage database - %i", result);
            return;
        }
    }

    SQLiteStatement insert(m_database, "INSERT INTO ItemTable VALUES (?, ?)");
    if (insert.prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare insert statement - cannot write to local storage database");
        return;
    }

    SQLiteStatement remove(m_database, "DELETE FROM ItemTable WHERE key=?");
    if (remove.prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare delete statement - cannot write to local storage database");
        return;
    }

    HashMap<String, String>::const_iterator end = items.end();

    for (HashMap<String, String>::const_iterator it = items.begin(); it != end; ++it) {
        // Based on the null-ness of the second argument, decide whether this is an insert or a delete.
        SQLiteStatement& query = it->second.isNull() ? remove : insert;        

        query.bindText(1, it->first);

        // If the second argument is non-null, we're doing an insert, so bind it as the value. 
        if (!it->second.isNull())
            query.bindText(2, it->second);

        int result = query.step();
        if (result != SQLResultDone) {
            LOG_ERROR("Failed to update item in the local storage database - %i", result);
            break;
        }

        query.reset();
    }
}

void LocalStorageArea::performSync()
{
    ASSERT(!isMainThread());

    bool clearItems;
    HashMap<String, String> items;
    {
        MutexLocker locker(m_syncLock);

        ASSERT(m_syncScheduled);

        clearItems = m_clearItemsWhileSyncing;
        m_itemsPendingSync.swap(items);

        m_clearItemsWhileSyncing = false;
        m_syncScheduled = false;
    }

    sync(clearItems, items);

    // The following is balanced by the call to disableSuddenTermination in the
    // syncTimerFired function.
    enableSuddenTermination();
}

} // namespace WebCore
