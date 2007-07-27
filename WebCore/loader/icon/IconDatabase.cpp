/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "IconDatabase.h"

#include "CString.h"
#include "IconDataCache.h"
#include "Image.h"
#include "Logging.h"
#include "SQLStatement.h"
#include "SQLTransaction.h"
#include "SystemTime.h"

#if PLATFORM(WIN_OS)
#include <windows.h>
#include <winbase.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#endif

namespace WebCore {

static IconDatabase* sharedIconDatabase = 0;

// This version number is in the DB and marks the current generation of the schema
// Theoretically once the switch is flipped this should never change
// Currently, an out-of-date schema causes the DB to be wiped and reset.  This isn't 
// so bad during development but in the future, we would need to write a conversion
// function to advance older released schemas to "current"
const int currentDatabaseVersion = 5;

// Icons expire once a day
const int iconExpirationTime = 60*60*24; 
// Absent icons are rechecked once a week
const int missingIconExpirationTime = 60*60*24*7; 

const int updateTimerDelay = 5; 

const String& IconDatabase::defaultDatabaseFilename()
{
    static String defaultDatabaseFilename = "Icons.db";
    return defaultDatabaseFilename;
}

IconDatabase* iconDatabase()
{
    if (!sharedIconDatabase)
        sharedIconDatabase = new IconDatabase;
    return sharedIconDatabase;
}

IconDatabase::IconDatabase()
    : m_timeStampForIconURLStatement(0)
    , m_iconURLForPageURLStatement(0)
    , m_hasIconForIconURLStatement(0)
    , m_forgetPageURLStatement(0)
    , m_setIconIDForPageURLStatement(0)
    , m_getIconIDForIconURLStatement(0)
    , m_addIconForIconURLStatement(0)
    , m_imageDataForIconURLStatement(0)
    , m_importedStatement(0)
    , m_setImportedStatement(0)
    , m_currentDB(&m_mainDB)
    , m_defaultIconDataCache(0)
    , m_isEnabled(false)
    , m_privateBrowsingEnabled(false)
    , m_startupTimer(this, &IconDatabase::pruneUnretainedIconsOnStartup)
    , m_updateTimer(this, &IconDatabase::updateDatabase)
    , m_initialPruningComplete(false)
    , m_imported(false)
    , m_isImportedSet(false)
{
    
}

bool makeAllDirectories(const String& path)
{
#if PLATFORM(WIN_OS)
    String fullPath = path;
    if (!SHCreateDirectoryEx(0, fullPath.charactersWithNullTermination(), 0)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            LOG_ERROR("Failed to create path %s", path.ascii().data());
            return false;
        }
    }
#else
    CString fullPath = path.utf8();
    char* p = fullPath.mutableData() + 1;
    int length = fullPath.length();
    
    if(p[length - 1] == '/')
        p[length - 1] = '\0';
    for (; *p; ++p)
        if (*p == '/') {
            *p = '\0';
            if (access(fullPath.data(), F_OK))
                if (mkdir(fullPath.data(), S_IRWXU))
                    return false;
            *p = '/';
        }
    if (access(fullPath.data(), F_OK))        
        if (mkdir(fullPath.data(), S_IRWXU))
            return false;
#endif   
    return true;
}

bool IconDatabase::open(const String& databasePath)
{
    if (!m_isEnabled)
        return false;
        
    if (isOpen()) {
        LOG_ERROR("Attempt to reopen the IconDatabase which is already open.  Must close it first.");
        return false;
    }
    
    // <rdar://problem/4730811> - Need to create the database path if it doesn't already exist
    makeAllDirectories(databasePath);
    
    // First we'll formulate the full path for the database file
    String dbFilename;
#if PLATFORM(WIN_OS)
    if (databasePath[databasePath.length()] == '\\')
        dbFilename = databasePath + defaultDatabaseFilename();
    else
        dbFilename = databasePath + "\\" + defaultDatabaseFilename();
#else
    if (databasePath[databasePath.length()] == '/')
        dbFilename = databasePath + defaultDatabaseFilename();
    else
        dbFilename = databasePath + "/" + defaultDatabaseFilename();
#endif

    // <rdar://problem/4707718> - If user's Icon directory is unwritable, Safari will crash at startup
    // Now, we'll see if we can open the on-disk database.  And, if we can't, we'll return false.  
    // WebKit will then ignore us and act as if the database is disabled
    if (!m_mainDB.open(dbFilename)) {
        LOG_ERROR("Unable to open icon database at path %s - %s", dbFilename.ascii().data(), m_mainDB.lastErrorMsg());
        return false;
    }
    
    if (!isValidDatabase(m_mainDB)) {
        LOG(IconDatabase, "%s is missing or in an invalid state - reconstructing", dbFilename.ascii().data());
        m_mainDB.clearAllTables();
        createDatabaseTables(m_mainDB);
    }

    // These are actually two different SQLite config options - not my fault they are named confusingly  ;)
    m_mainDB.setSynchronous(SQLDatabase::SyncOff);
    m_mainDB.setFullsync(false);
    
    // Reduce sqlite RAM cache size from default 2000 pages (~1.5kB per page). 3MB of cache for icon database is overkill
    if (!SQLStatement(m_mainDB, "PRAGMA cache_size = 200;").executeCommand())         
        LOG_ERROR("SQLite database could not set cache_size");
    
    // Open the in-memory table for private browsing
    if (!m_privateBrowsingDB.open(":memory:"))
        LOG_ERROR("Unable to open in-memory database for private browsing - %s", m_privateBrowsingDB.lastErrorMsg());

    // Only if we successfully remained open will we start our "initial purge timer"
    // rdar://4690949 - when we have deferred reads and writes all the way in, the prunetimer
    // will become "deferredTimer" or something along those lines, and will be set only when
    // a deferred read/write is queued
    if (isOpen())
        m_startupTimer.startOneShot(0);
    
    return isOpen();
}

bool IconDatabase::isOpen() const
{
    return m_mainDB.isOpen() && m_privateBrowsingDB.isOpen();
}

void IconDatabase::close()
{
    // This will close all the SQL statements and transactions we have open,
    // syncing the DB at the appropriate point
    deleteAllPreparedStatements(true);
 
    m_mainDB.close();
    m_privateBrowsingDB.close();
}

String IconDatabase::databasePath() const
{
    return m_mainDB.isOpen() ? m_mainDB.path() : String();
}
    
void IconDatabase::removeAllIcons()
{
    if (!isOpen())
        return;
        
    // We don't need to sync anything anymore since we're wiping everything.  
    // So we can kill the update timer, and clear all the hashes of "items that need syncing"
    m_updateTimer.stop();
    m_iconDataCachesPendingUpdate.clear();
    m_pageURLsPendingAddition.clear();
    m_pageURLsPendingDeletion.clear();
    m_iconURLsPendingDeletion.clear();
    
    //  Now clear all in-memory URLs and Icons
    m_pageURLToIconURLMap.clear();
    m_pageURLToRetainCount.clear();
    m_iconURLToRetainCount.clear();
    
    deleteAllValues(m_iconURLToIconDataCacheMap);
    m_iconURLToIconDataCacheMap.clear();
        
    // Wipe any pre-prepared statements, otherwise resetting the SQLDatabases themselves will fail
    deleteAllPreparedStatements(false);
    
    // The easiest way to wipe the in-memory database is by closing and reopening it
    m_privateBrowsingDB.close();
    if (!m_privateBrowsingDB.open(":memory:"))
        LOG_ERROR("Unable to open in-memory database for private browsing - %s", m_privateBrowsingDB.lastErrorMsg());
    createDatabaseTables(m_privateBrowsingDB);
        
    // To reset the on-disk database, we'll wipe all its tables then vacuum it
    // This is easier and safer than closing it, deleting the file, and recreating from scratch
    m_mainDB.clearAllTables();
    m_mainDB.runVacuumCommand();
    createDatabaseTables(m_mainDB);
}

// There are two instances where you'd want to deleteAllPreparedStatements - one with sync, and one without
// A - Closing down the database on application exit - in this case, you *do* want to save the icons out
// B - Resetting the DB via removeAllIcons() - in this case, you *don't* want to sync, because it would be a waste of time
void IconDatabase::deleteAllPreparedStatements(bool withSync)
{
    // Sync, if desired
    if (withSync)
        syncDatabase();
        
    // Order doesn't matter on these
    delete m_timeStampForIconURLStatement;
    m_timeStampForIconURLStatement = 0;
    delete m_iconURLForPageURLStatement;
    m_iconURLForPageURLStatement = 0;
    delete m_hasIconForIconURLStatement;
    m_hasIconForIconURLStatement = 0;
    delete m_forgetPageURLStatement;
    m_forgetPageURLStatement = 0;
    delete m_setIconIDForPageURLStatement;
    m_setIconIDForPageURLStatement = 0;
    delete m_getIconIDForIconURLStatement;
    m_getIconIDForIconURLStatement = 0;
    delete m_addIconForIconURLStatement;
    m_addIconForIconURLStatement = 0;
    delete m_imageDataForIconURLStatement;
    m_imageDataForIconURLStatement = 0;
    delete m_importedStatement;
    m_importedStatement = 0;
    delete m_setImportedStatement;
    m_setImportedStatement = 0;
}

bool IconDatabase::isEmpty()
{
    if (m_privateBrowsingEnabled)
        if (!pageURLTableIsEmptyQuery(m_privateBrowsingDB))
            return false;
            
    return pageURLTableIsEmptyQuery(m_mainDB);
}

bool IconDatabase::isValidDatabase(SQLDatabase& db)
{
    // These two tables should always exist in a valid db
    if (!db.tableExists("Icon") || !db.tableExists("PageURL") || !db.tableExists("IconDatabaseInfo"))
        return false;
    
    if (SQLStatement(db, "SELECT value FROM IconDatabaseInfo WHERE key = 'Version';").getColumnInt(0) < currentDatabaseVersion) {
        LOG(IconDatabase, "DB version is not found or below expected valid version");
        return false;
    }
    
    return true;
}

void IconDatabase::createDatabaseTables(SQLDatabase& db)
{
    if (!db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE Icon (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, stamp INTEGER, data BLOB);")) {
        LOG_ERROR("Could not create Icon table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
}    

PassRefPtr<SharedBuffer> IconDatabase::imageDataForIconURL(const String& iconURL)
{      
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {    
        RefPtr<SharedBuffer> result = imageDataForIconURLQuery(m_privateBrowsingDB, iconURL);
        if (result && !result->isEmpty())
            return result.release();
    } 
    
    // It wasn't found there, so lets check the main tables
    return imageDataForIconURLQuery(m_mainDB, iconURL);
}

void IconDatabase::setPrivateBrowsingEnabled(bool flag)
{
    if (!isOpen())
        return;
    if (m_privateBrowsingEnabled == flag)
        return;
    
    // Sync any deferred DB changes before we change the active DB
    syncDatabase();
    
    m_privateBrowsingEnabled = flag;
    
    if (m_privateBrowsingEnabled) {
        createDatabaseTables(m_privateBrowsingDB);
        m_currentDB = &m_privateBrowsingDB;
    } else {
        m_privateBrowsingDB.clearAllTables();
        m_currentDB = &m_mainDB;
    }
}

bool IconDatabase::isPrivateBrowsingEnabled() const
{
    return m_privateBrowsingEnabled;
}

Image* IconDatabase::iconForPageURL(const String& pageURL, const IntSize& size, bool cache)
{   
    if (!isOpen())
        return defaultIcon(size);
        
    // See if we even have an IconURL for this PageURL...
    String iconURL = iconURLForPageURL(pageURL);
    if (iconURL.isEmpty())
        return 0;
    
    // If we do, maybe we have a IconDataCache for this IconURL
    IconDataCache* icon = getOrCreateIconDataCache(iconURL);
    
    // If it's a new IconDataCache object that doesn't have its imageData set yet,
    // we'll read in that image data now
    if (icon->imageDataStatus() == ImageDataStatusUnknown) {
        RefPtr<SharedBuffer> data = imageDataForIconURL(iconURL);
        icon->setImageData(data.get());
    }
        
    return icon->getImage(size);
}

// FIXME 4667425 - this check needs to see if the icon's data is empty or not and apply
// iconExpirationTime to present icons, and missingIconExpirationTime for missing icons
bool IconDatabase::isIconExpiredForIconURL(const String& iconURL)
{
    // If we're closed and someone is making this call, it is likely a return value of 
    // false will discourage them to take any further action, which is our goal in this case
    // Same notion for an empty iconURL - which is now defined as "never expires"
    if (!isOpen() || iconURL.isEmpty())
        return false;
    
    // If we have a IconDataCache, then it definitely has the Timestamp in it
    IconDataCache* icon = m_iconURLToIconDataCacheMap.get(iconURL);
    if (icon) 
        return (int)currentTime() - icon->getTimestamp() > iconExpirationTime;
            
    // Otherwise, we'll get the timestamp from the DB and use it
    int stamp;
    if (m_privateBrowsingEnabled) {
        stamp = timeStampForIconURLQuery(m_privateBrowsingDB, iconURL);
        if (stamp)
            return ((int)currentTime() - stamp) > iconExpirationTime;
    }
    
    stamp = timeStampForIconURLQuery(m_mainDB, iconURL);
    if (stamp)
        return ((int)currentTime() - stamp) > iconExpirationTime;
    
    return false;
}
    
String IconDatabase::iconURLForPageURL(const String& pageURL)
{    
    if (!isOpen() || pageURL.isEmpty())
        return String();
        
    if (m_pageURLToIconURLMap.contains(pageURL))
        return m_pageURLToIconURLMap.get(pageURL);
    
    // Try the private browsing database because if any PageURL's IconURL was updated during privated browsing, 
    // the most up-to-date iconURL would be there
    if (m_privateBrowsingEnabled) {
        String iconURL = iconURLForPageURLQuery(m_privateBrowsingDB, pageURL);
        if (!iconURL.isEmpty()) {
            m_pageURLToIconURLMap.set(pageURL, iconURL);
            return iconURL;
        }
    }
    
    String iconURL = iconURLForPageURLQuery(m_mainDB, pageURL);
    if (!iconURL.isEmpty())
        m_pageURLToIconURLMap.set(pageURL, iconURL);
    return iconURL;
}

Image* IconDatabase::defaultIcon(const IntSize& size)
{
    if (!m_defaultIconDataCache) {
        m_defaultIconDataCache = new IconDataCache("urlIcon");
        m_defaultIconDataCache->loadImageFromResource("urlIcon");
    }
    
    return m_defaultIconDataCache->getImage(size);
}

void IconDatabase::retainIconForPageURL(const String& pageURL)
{
    if (!isOpen() || pageURL.isEmpty())
        return;
    
    // If we don't have the retain count for this page, we need to setup records of its retain
    // Otherwise, get the count and increment it
    int retainCount;
    if (!(retainCount = m_pageURLToRetainCount.get(pageURL))) {
        m_pageURLToRetainCount.set(pageURL, 1);   

        // If we haven't done pruning yet, we want to avoid any pageURL->iconURL lookups and the pageURLsPendingDeletion is moot, 
        // so we bail here and skip those steps
        if (!m_initialPruningComplete)
            return;
        
        // If this pageURL is marked for deletion, bring it back from the brink
        m_pageURLsPendingDeletion.remove(pageURL);
        
        // If we have an iconURL for this pageURL, we'll now retain the iconURL
        String iconURL = iconURLForPageURL(pageURL);
        if (!iconURL.isEmpty())
            retainIconURL(iconURL);

    } else
        m_pageURLToRetainCount.set(pageURL, retainCount + 1);   
}

void IconDatabase::releaseIconForPageURL(const String& pageURL)
{
    if (!isOpen() || pageURL.isEmpty())
        return;
        
    // Check if this pageURL is actually retained
    if(!m_pageURLToRetainCount.contains(pageURL)) {
        LOG_ERROR("Attempting to release icon for URL %s which is not retained", pageURL.ascii().data());
        return;
    }
    
    // Get its retain count
    int retainCount = m_pageURLToRetainCount.get(pageURL);
    ASSERT(retainCount > 0);
    
    // If it still has a positive retain count, store the new count and bail
    if (--retainCount) {
        m_pageURLToRetainCount.set(pageURL, retainCount);
        return;
    }
    
    LOG(IconDatabase, "No more retainers for PageURL %s", pageURL.ascii().data());
    
    // Otherwise, remove all record of the retain count
    m_pageURLToRetainCount.remove(pageURL);   
    
    // If we haven't done pruning yet, we want to avoid any pageURL->iconURL lookups and the pageURLsPendingDeletion is moot, 
    // so we bail here and skip those steps
    if (!m_initialPruningComplete)
        return;

    
    // Then mark this pageURL for deletion
    m_pageURLsPendingDeletion.add(pageURL);
    
    // Grab the iconURL and release it
    String iconURL = iconURLForPageURL(pageURL);
    if (!iconURL.isEmpty())
        releaseIconURL(iconURL);
}

void IconDatabase::retainIconURL(const String& iconURL)
{
    ASSERT(!iconURL.isEmpty());
    
    if (int retainCount = m_iconURLToRetainCount.get(iconURL)) {
        ASSERT(retainCount > 0);
        m_iconURLToRetainCount.set(iconURL, retainCount + 1);
    } else {
        m_iconURLToRetainCount.set(iconURL, 1);
        if (m_iconURLsPendingDeletion.contains(iconURL))
            m_iconURLsPendingDeletion.remove(iconURL);
    }   
}

void IconDatabase::releaseIconURL(const String& iconURL)
{
    ASSERT(!iconURL.isEmpty());
        
    // If the iconURL has no retain count, we can bail
    if (!m_iconURLToRetainCount.contains(iconURL))
        return;
    
    // Otherwise, decrement it
    int retainCount = m_iconURLToRetainCount.get(iconURL) - 1;
    ASSERT(retainCount > -1);
    
    // If the icon is still retained, store the count and bail
    if (retainCount) {
        m_iconURLToRetainCount.set(iconURL, retainCount);
        return;
    }
    
    LOG(IconDatabase, "No more retainers for IconURL %s", iconURL.ascii().data());
    
    // Otherwise, this icon is toast.  Remove all traces of its retain count...
    m_iconURLToRetainCount.remove(iconURL);
    
    // And since we delay the actual deletion of icons, so lets add it to that queue
    m_iconURLsPendingDeletion.add(iconURL);
}

void IconDatabase::forgetPageURL(const String& pageURL)
{
    // Remove the PageURL->IconURL mapping
    m_pageURLToIconURLMap.remove(pageURL);
    
    // And remove this pageURL from the DB
    forgetPageURLQuery(*m_currentDB, pageURL);
}
    
bool IconDatabase::isIconURLRetained(const String& iconURL)
{
    if (iconURL.isEmpty())
        return false;
        
    return m_iconURLToRetainCount.contains(iconURL);
}

void IconDatabase::forgetIconForIconURLFromDatabase(const String& iconURL)
{
    if (iconURL.isEmpty())
        return;

    // For private browsing safety, since this alters the database, we only forget from the current database
    // If we're in private browsing and the Icon also exists in the main database, it will be pruned on the next startup
    int64_t iconID = establishIconIDForIconURL(*m_currentDB, iconURL, false);
    
    // If we didn't actually have an icon for this iconURL... well, thats a screwy condition we should track down, but also 
    // something we could move on from
    ASSERT(iconID);
    if (!iconID) {
        LOG_ERROR("Attempting to forget icon for IconURL %s, though we don't have it in the database", iconURL.ascii().data());
        return;
    }
    
    if (!m_currentDB->executeCommand(String::format("DELETE FROM Icon WHERE Icon.iconID = %lli;", iconID)))
        LOG_ERROR("Unable to drop Icon for IconURL", iconURL.ascii().data()); 
    if (!m_currentDB->executeCommand(String::format("DELETE FROM PageURL WHERE PageURL.iconID = %lli", iconID)))
        LOG_ERROR("Unable to drop all PageURL for IconURL", iconURL.ascii().data()); 
}

IconDataCache* IconDatabase::getOrCreateIconDataCache(const String& iconURL)
{
    IconDataCache* icon;
    if ((icon = m_iconURLToIconDataCacheMap.get(iconURL)))
        return icon;
        
    icon = new IconDataCache(iconURL);
    m_iconURLToIconDataCacheMap.set(iconURL, icon);
    
    // Get the most current time stamp for this IconURL
    int timestamp = 0;
    if (m_privateBrowsingEnabled)
        timestamp = timeStampForIconURLQuery(m_privateBrowsingDB, iconURL);
    if (!timestamp)
        timestamp = timeStampForIconURLQuery(m_mainDB, iconURL);
        
    // If we can't get a timestamp for this URL, then it is a new icon and we initialize its timestamp now
    if (!timestamp) {
        icon->setTimestamp((int)currentTime());
        m_iconDataCachesPendingUpdate.add(icon);
    } else 
        icon->setTimestamp(timestamp);
        
    return icon;
}

void IconDatabase::setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String& iconURL)
{
    if (!isOpen() || iconURL.isEmpty())
        return;

    // Get the IconDataCache for this IconURL (note, IconDataCacheForIconURL will create it if necessary)
    IconDataCache* icon = getOrCreateIconDataCache(iconURL);
    
    // Set the data in the IconDataCache
    icon->setImageData(data);
    
    // Update the timestamp in the IconDataCache to NOW
    icon->setTimestamp((int)currentTime());

    // Mark the IconDataCache as requiring an update to the database
    m_iconDataCachesPendingUpdate.add(icon);
}

void IconDatabase::setHaveNoIconForIconURL(const String& iconURL)
{   
    setIconDataForIconURL(0, iconURL);
}

bool IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL)
{
    ASSERT(!iconURL.isEmpty());
    if (!isOpen() || pageURL.isEmpty())
        return false;
    
    // If the urls already map to each other, bail.
    // This happens surprisingly often, and seems to cream iBench performance
    if (m_pageURLToIconURLMap.get(pageURL) == iconURL)
        return false;

    // If this pageURL is retained, we have some work to do on the IconURL retain counts
    if (m_pageURLToRetainCount.contains(pageURL)) {
        String oldIconURL = m_pageURLToIconURLMap.get(pageURL);
        if (!oldIconURL.isEmpty())
            releaseIconURL(oldIconURL);
        retainIconURL(iconURL);
    } else {
        // If this pageURL is *not* retained, then we may be marking it for deletion, as well!
        // As counterintuitive as it seems to mark it for addition and for deletion at the same time,
        // it's valid because when we do a new pageURL->iconURL mapping we *have* to mark it for addition,
        // no matter what, as there is no efficient was to determine if the mapping is in the DB already.
        // But, if the iconURL is marked for deletion, we'll also mark this pageURL for deletion - if a 
        // client comes along and retains it before the timer fires, the "pendingDeletion" lists will
        // be manipulated appopriately and new pageURL will be brought back from the brink
        if (m_iconURLsPendingDeletion.contains(iconURL))
            m_pageURLsPendingDeletion.add(pageURL);
    }
    
    // Cache the pageURL->iconURL map
    m_pageURLToIconURLMap.set(pageURL, iconURL);
    
    // And mark this mapping to be added to the database
    m_pageURLsPendingAddition.add(pageURL);
    
    // Then start the timer to commit this change - or further delay the timer if it
    // was already started
    m_updateTimer.startOneShot(updateTimerDelay);
    
    return true;
}

void IconDatabase::setIconURLForPageURLInDatabase(const String& iconURL, const String& pageURL)
{
    int64_t iconID = establishIconIDForIconURL(*m_currentDB, iconURL);
    if (!iconID) {
        LOG_ERROR("Failed to establish an ID for iconURL %s", iconURL.ascii().data());
        return;
    }
    setIconIDForPageURLQuery(*m_currentDB, iconID, pageURL);
}

int64_t IconDatabase::establishIconIDForIconURL(SQLDatabase& db, const String& iconURL, bool createIfNecessary)
{    
    // Get the iconID thats already in this database and return it - or return 0 if we're read-only
    int64_t iconID = getIconIDForIconURLQuery(db, iconURL);
    if (iconID || !createIfNecessary)
        return iconID;
        
    // Create the icon table entry for the iconURL
    return addIconForIconURLQuery(db, iconURL);
}

void IconDatabase::pruneUnretainedIconsOnStartup(Timer<IconDatabase>*)
{
    if (!isOpen())
        return;
        
    // This function should only be called once per run, and ideally only via the timer
    // on program startup
    ASSERT(!m_initialPruningComplete);

#ifndef NDEBUG
    double timestamp = currentTime();
#endif
    
    // rdar://4690949 - Need to prune unretained iconURLs here, then prune out all pageURLs that reference
    // nonexistent icons
    
    SQLTransaction pruningTransaction(m_mainDB);
    pruningTransaction.begin();
    
    // Wipe all PageURLs that aren't retained
    // Temporary tables in sqlite seem to lose memory permanently so do this by hand instead. This is faster too.
    
    HashMap<String, int64_t> pageUrlsToDelete; 
    
    // First get the known PageURLs from the db
    int result;
    SQLStatement pageSQL(m_mainDB, "SELECT url, iconID FROM PageURL");
    pageSQL.prepare();
    while((result = pageSQL.step()) == SQLResultRow)
        pageUrlsToDelete.set(pageSQL.getColumnText16(0), pageSQL.getColumnInt64(1));
    pageSQL.finalize();
    if (result != SQLResultDone)
        LOG_ERROR("Error reading PageURL table from on-disk DB");
    
    // Remove all urls we actually want to keep from the hash
    HashMap<String, int>::iterator endit = m_pageURLToRetainCount.end();
    for (HashMap<String, int>::iterator it = m_pageURLToRetainCount.begin(); it != endit; ++it)
        pageUrlsToDelete.remove(it->first);
    
    // Delete the rest, if any
    if (pageUrlsToDelete.size()) {
        SQLStatement pageDeleteSQL(m_mainDB, "DELETE FROM PageURL WHERE iconID = (?)");
        pageDeleteSQL.prepare();
        HashMap<String, int64_t>::iterator endit = pageUrlsToDelete.end();
        for (HashMap<String, int64_t>::iterator it = pageUrlsToDelete.begin(); it != endit; ++it) {
            LOG(IconDatabase, "Deleting %s from PageURL table\n", it->first.latin1().data());
            pageDeleteSQL.bindInt64(1, it->second);
            if (pageDeleteSQL.step() != SQLResultDone)
                LOG_ERROR("Unable to delete %s from PageURL table", it->first.latin1().data());
            pageDeleteSQL.reset();
        }
        pageDeleteSQL.finalize();
    }
    
    // Wipe Icons that aren't retained
    if (!m_mainDB.executeCommand("DELETE FROM Icon WHERE Icon.iconID NOT IN (SELECT iconID FROM PageURL);"))
        LOG_ERROR("Failed to execute SQL to prune unretained icons from the on-disk tables");    
    
    // Since we lazily retained the pageURLs without getting the iconURLs or retaining the iconURLs, 
    // we need to do that now
    SQLStatement sql(m_mainDB, "SELECT PageURL.url, Icon.url FROM PageURL INNER JOIN Icon ON PageURL.iconID=Icon.iconID");
    sql.prepare();
    while((result = sql.step()) == SQLResultRow) {
        String iconURL = sql.getColumnText16(1);
        retainIconURL(iconURL);
        LOG(IconDatabase, "Found a PageURL that mapped to %s", iconURL.ascii().data());
    }
    if (result != SQLResultDone)
        LOG_ERROR("Error reading PageURL->IconURL mappings from on-disk DB");
    sql.finalize();
    
    pruningTransaction.commit();
    m_initialPruningComplete = true;
    
    // Handle dangling PageURLs, if any
    checkForDanglingPageURLs(true);

#ifndef NDEBUG
    timestamp = currentTime() - timestamp;
    if (timestamp <= 1.0)
        LOG(IconDatabase, "Pruning unretained icons took %.4f seconds", timestamp);
    else
        LOG(IconDatabase, "Pruning unretained icons took %.4f seconds - this is much too long!", timestamp);

#endif
}

void IconDatabase::updateDatabase(Timer<IconDatabase>*)
{
    syncDatabase();
}

void IconDatabase::syncDatabase()
{
#ifndef NDEBUG
    double timestamp = currentTime();
#endif

    // First we'll do the pending additions
    // Starting with the IconDataCaches that need updating/insertion
    for (HashSet<IconDataCache*>::iterator i = m_iconDataCachesPendingUpdate.begin(), end = m_iconDataCachesPendingUpdate.end(); i != end; ++i) {
        (*i)->writeToDatabase(*m_currentDB);
        LOG(IconDatabase, "Wrote IconDataCache for IconURL %s with timestamp of %li to the DB", (*i)->getIconURL().ascii().data(), (*i)->getTimestamp());
    }
    m_iconDataCachesPendingUpdate.clear();
    
    HashSet<String>::iterator i = m_pageURLsPendingAddition.begin(), end = m_pageURLsPendingAddition.end();
    for (; i != end; ++i) {
        setIconURLForPageURLInDatabase(m_pageURLToIconURLMap.get(*i), *i);
        LOG(IconDatabase, "Committed IconURL for PageURL %s to database", (*i).ascii().data());
    }
    m_pageURLsPendingAddition.clear();
    
    // Then we'll do the pending deletions
    // First lets wipe all the pageURLs
    for (i = m_pageURLsPendingDeletion.begin(), end = m_pageURLsPendingDeletion.end(); i != end; ++i) {    
        forgetPageURL(*i);
        LOG(IconDatabase, "Deleted PageURL %s", (*i).ascii().data());
    }
    m_pageURLsPendingDeletion.clear();

    // Then get rid of all traces of the icons and IconURLs
    IconDataCache* icon;    
    for (i = m_iconURLsPendingDeletion.begin(), end = m_iconURLsPendingDeletion.end(); i != end; ++i) {
        // Forget the IconDataCache
        icon = m_iconURLToIconDataCacheMap.get(*i);
        if (icon)
            m_iconURLToIconDataCacheMap.remove(*i);
        delete icon;
        
        // Forget the IconURL from the database
        forgetIconForIconURLFromDatabase(*i);
        LOG(IconDatabase, "Deleted icon %s", (*i).ascii().data());   
    }
    m_iconURLsPendingDeletion.clear();
    
    // If the timer was running to cause this update, we can kill the timer as its firing would be redundant
    m_updateTimer.stop();
    
#ifndef NDEBUG
    timestamp = currentTime() - timestamp;
    if (timestamp <= 1.0)
        LOG(IconDatabase, "Updating the database took %.4f seconds", timestamp);
    else 
        LOG(IconDatabase, "Updating the database took %.4f seconds - this is much too long!", timestamp);
    
    // Check to make sure there are no dangling PageURLs - If there are, we want to output one log message but not spam the console potentially every few seconds
    checkForDanglingPageURLs(false);
#endif
}

void IconDatabase::checkForDanglingPageURLs(bool pruneIfFound)
{
    // We don't want to keep performing this check and reporting this error if it has already found danglers so we keep track
    static bool danglersFound = false;
    
    // However, if the caller wants us to prune the danglers, we will reset this flag and prune every time
    if (pruneIfFound)
        danglersFound = false;
        
    if (!danglersFound && SQLStatement(*m_currentDB, "SELECT url FROM PageURL WHERE PageURL.iconID NOT IN (SELECT iconID FROM Icon) LIMIT 1;").returnsAtLeastOneResult()) {
        danglersFound = true;
        LOG_ERROR("Dangling PageURL entries found");
        if (pruneIfFound && !m_currentDB->executeCommand("DELETE FROM PageURL WHERE iconID NOT IN (SELECT iconID FROM Icon);"))
            LOG_ERROR("Unable to prune dangling PageURLs");
    }
}

bool IconDatabase::hasEntryForIconURL(const String& iconURL)
{
    if (!isOpen() || iconURL.isEmpty())
        return false;
        
    // First check the in memory mapped icons...
    if (m_iconURLToIconDataCacheMap.contains(iconURL))
        return true;

    // Then we'll check the main database
    if (hasIconForIconURLQuery(m_mainDB, iconURL))
        return true;
        
    // Finally, the last resort - check the private browsing database
    if (m_privateBrowsingEnabled)  
        if (hasIconForIconURLQuery(m_privateBrowsingDB, iconURL))
            return true;    

    // We must not have this iconURL!
    return false;
}

void IconDatabase::setEnabled(bool enabled)
{
    if (!enabled && isOpen())
        close();
    m_isEnabled = enabled;
}

bool IconDatabase::enabled() const
{
     return m_isEnabled;
}

bool IconDatabase::imported()
{
    if (!m_isImportedSet) {
        m_imported = importedQuery(m_mainDB);
        m_isImportedSet = true;
    }
    return m_imported;
}

void IconDatabase::setImported(bool import)
{
    m_imported = import;
    m_isImportedSet = true;
    setImportedQuery(m_mainDB, import);
}

IconDatabase::~IconDatabase()
{
    ASSERT_NOT_REACHED();
}

// readySQLStatement() handles two things
// 1 - If the SQLDatabase& argument is different, the statement must be destroyed and remade.  This happens when the user
//     switches to and from private browsing
// 2 - Lazy construction of the Statement in the first place, in case we've never made this query before
inline void readySQLStatement(SQLStatement*& statement, SQLDatabase& db, const String& str)
{
    if (statement && (statement->database() != &db || statement->isExpired())) {
        if (statement->isExpired())
            LOG(IconDatabase, "SQLStatement associated with %s is expired", str.ascii().data());
        delete statement;
        statement = 0;
    }
    if (!statement) {
        statement = new SQLStatement(db, str);
        int result;
        result = statement->prepare();
        ASSERT(result == SQLResultOk);
    }
}

// Any common IconDatabase query should be seperated into a fooQuery() and a *m_fooStatement.  
// This way we can lazily construct the SQLStatment for a query on its first use, then reuse the Statement binding
// the new parameter as needed
// The statement must be deleted in IconDatabase::close() before the actual SQLDatabase::close() call
// Also, m_fooStatement must be reset() before fooQuery() returns otherwise we will constantly get "database file locked" 
// errors in various combinations of queries

bool IconDatabase::pageURLTableIsEmptyQuery(SQLDatabase& db)
{  
    // We won't make this use a m_fooStatement because its not really a "common" query
    return !SQLStatement(db, "SELECT iconID FROM PageURL LIMIT 1;").returnsAtLeastOneResult();
}

PassRefPtr<SharedBuffer> IconDatabase::imageDataForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    RefPtr<SharedBuffer> imageData;
    
    readySQLStatement(m_imageDataForIconURLStatement, db, "SELECT Icon.data FROM Icon WHERE Icon.url = (?);");
    m_imageDataForIconURLStatement->bindText16(1, iconURL, false);
    
    int result = m_imageDataForIconURLStatement->step();
    if (result == SQLResultRow) {
        Vector<char> data;
        m_imageDataForIconURLStatement->getColumnBlobAsVector(0, data);
        imageData = new SharedBuffer;
        imageData->append(data.data(), data.size());
    } else if (result != SQLResultDone)
        LOG_ERROR("imageDataForIconURLQuery failed");

    m_imageDataForIconURLStatement->reset();
    
    return imageData.release();
}

int IconDatabase::timeStampForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    readySQLStatement(m_timeStampForIconURLStatement, db, "SELECT Icon.stamp FROM Icon WHERE Icon.url = (?);");
    m_timeStampForIconURLStatement->bindText16(1, iconURL, false);

    int result = m_timeStampForIconURLStatement->step();
    if (result == SQLResultRow)
        result = m_timeStampForIconURLStatement->getColumnInt(0);
    else {
        if (result != SQLResultDone)
            LOG_ERROR("timeStampForIconURLQuery failed");
        result = 0;
    }

    m_timeStampForIconURLStatement->reset();
    return result;
}

String IconDatabase::iconURLForPageURLQuery(SQLDatabase& db, const String& pageURL)
{
    readySQLStatement(m_iconURLForPageURLStatement, db, "SELECT Icon.url FROM Icon, PageURL WHERE PageURL.url = (?) AND Icon.iconID = PageURL.iconID;");
    m_iconURLForPageURLStatement->bindText16(1, pageURL, false);
    
    int result = m_iconURLForPageURLStatement->step();
    String iconURL;
    if (result == SQLResultRow)
        iconURL = m_iconURLForPageURLStatement->getColumnText16(0);
    else if (result != SQLResultDone)
        LOG_ERROR("iconURLForPageURLQuery failed");
    
    m_iconURLForPageURLStatement->reset();
    return iconURL;
}

void IconDatabase::forgetPageURLQuery(SQLDatabase& db, const String& pageURL)
{
    readySQLStatement(m_forgetPageURLStatement, db, "DELETE FROM PageURL WHERE url = (?);");
    m_forgetPageURLStatement->bindText16(1, pageURL, false);

    if (m_forgetPageURLStatement->step() != SQLResultDone)
        LOG_ERROR("forgetPageURLQuery failed");
    
    m_forgetPageURLStatement->reset();
}

void IconDatabase::setIconIDForPageURLQuery(SQLDatabase& db, int64_t iconID, const String& pageURL)
{
    readySQLStatement(m_setIconIDForPageURLStatement, db, "INSERT INTO PageURL (url, iconID) VALUES ((?), ?);");
    m_setIconIDForPageURLStatement->bindText16(1, pageURL, false);
    m_setIconIDForPageURLStatement->bindInt64(2, iconID);

    if (m_setIconIDForPageURLStatement->step() != SQLResultDone)
        LOG_ERROR("setIconIDForPageURLQuery failed");

    m_setIconIDForPageURLStatement->reset();
}

int64_t IconDatabase::getIconIDForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    readySQLStatement(m_getIconIDForIconURLStatement, db, "SELECT Icon.iconID FROM Icon WHERE Icon.url = (?);");
    m_getIconIDForIconURLStatement->bindText16(1, iconURL, false);
    
    int64_t result = m_getIconIDForIconURLStatement->step();
    if (result == SQLResultRow)
        result = m_getIconIDForIconURLStatement->getColumnInt64(0);
    else {
        if (result != SQLResultDone)
            LOG_ERROR("getIconIDForIconURLQuery failed");
        result = 0;
    }

    m_getIconIDForIconURLStatement->reset();
    return result;
}

int64_t IconDatabase::addIconForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    readySQLStatement(m_addIconForIconURLStatement, db, "INSERT INTO Icon (url) VALUES ((?));");
    m_addIconForIconURLStatement->bindText16(1, iconURL, false);
    
    int64_t result = m_addIconForIconURLStatement->step();
    if (result == SQLResultDone)
        result = db.lastInsertRowID();
    else {
        LOG_ERROR("addIconForIconURLQuery failed");
        result = 0;
    }

    m_addIconForIconURLStatement->reset();
    return result;
}

bool IconDatabase::hasIconForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    readySQLStatement(m_hasIconForIconURLStatement, db, "SELECT Icon.iconID FROM Icon WHERE Icon.url = (?);");
    m_hasIconForIconURLStatement->bindText16(1, iconURL, false);

    int result = m_hasIconForIconURLStatement->step();

    if (result != SQLResultRow && result != SQLResultDone)
        LOG_ERROR("hasIconForIconURLQuery failed");

    m_hasIconForIconURLStatement->reset();
    return result == SQLResultRow;
}

bool IconDatabase::importedQuery(SQLDatabase& db)
{
    readySQLStatement(m_importedStatement, db, "SELECT IconDatabaseInfo.value FROM IconDatabaseInfo WHERE IconDatabaseInfo.key = \"ImportedSafari2Icons\";");
    
    int result = m_importedStatement->step();

    if (result == SQLResultRow)
        result = m_importedStatement->getColumnInt(0);
    else {
        if (result != SQLResultDone)
            LOG_ERROR("importedQuery failed");
        result = 0;
    }

    m_importedStatement->reset();
    return result;
}

void IconDatabase::setImportedQuery(SQLDatabase& db, bool imported)
{
    if (imported)
        readySQLStatement(m_setImportedStatement, db, "INSERT INTO IconDatabaseInfo (key, value) VALUES (\"ImportedSafari2Icons\", 1);");
    else
        readySQLStatement(m_setImportedStatement, db, "INSERT INTO IconDatabaseInfo (key, value) VALUES (\"ImportedSafari2Icons\", 0);");

    int result = m_setImportedStatement->step();

    if (result != SQLResultDone)
        LOG_ERROR("setImportedQuery failed");

    m_setImportedStatement->reset();
}

} // namespace WebCore
