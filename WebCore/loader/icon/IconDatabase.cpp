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

#include "IconDataCache.h"
#include "Image.h"
#include "Logging.h"
#include "PlatformString.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "SQLStatement.h"
#include "SQLTransaction.h"


// FIXME - One optimization to be made when this is no longer in flux is to make query construction smarter - that is queries that are created from
// multiple strings and numbers should be handled differently than with String + String + String + etc.

namespace WebCore {

IconDatabase* IconDatabase::m_sharedInstance = 0;

// This version number is in the DB and marks the current generation of the schema
// Theoretically once the switch is flipped this should never change
// Currently, an out-of-date schema causes the DB to be wiped and reset.  This isn't 
// so bad during development but in the future, we would need to write a conversion
// function to advance older released schemas to "current"
const int IconDatabase::currentDatabaseVersion = 5;

// Icons expire once a day
const int IconDatabase::iconExpirationTime = 60*60*24; 
// Absent icons are rechecked once a week
const int IconDatabase::missingIconExpirationTime = 60*60*24*7; 

const int IconDatabase::updateTimerDelay = 5; 

const String& IconDatabase::defaultDatabaseFilename()
{
    static String defaultDatabaseFilename = "icon.db";
    return defaultDatabaseFilename;
}

// Query - Checks for at least 1 entry in the PageURL table
static bool pageURLTableIsEmptyQuery(SQLDatabase&);
// Query - Returns the time stamp for an Icon entry
static int timeStampForIconURLQuery(SQLDatabase&, const String& iconURL);    
// Query - Returns the IconURL for a PageURL
static String iconURLForPageURLQuery(SQLDatabase&, const String& pageURL);    
// Query - Checks for the existence of the given IconURL in the Icon table
static bool hasIconForIconURLQuery(SQLDatabase& db, const String& iconURL);
// Query - Deletes a PageURL from the PageURL table
static void forgetPageURLQuery(SQLDatabase& db, const String& pageURL);
// Query - Sets the Icon.iconID for a PageURL in the PageURL table
static void setIconIDForPageURLQuery(SQLDatabase& db, int64_t, const String&);
// Query - Returns the iconID for the given IconURL
static int64_t getIconIDForIconURLQuery(SQLDatabase& db, const String& iconURL);
// Query - Creates the Icon entry for the given IconURL and returns the resulting iconID
static int64_t addIconForIconURLQuery(SQLDatabase& db, const String& iconURL);
// Query - Returns the image data from the given database for the given IconURL
static void imageDataForIconURLQuery(SQLDatabase& db, const String& iconURL, Vector<unsigned char>& result);

IconDatabase* IconDatabase::sharedIconDatabase()
{
    if (!m_sharedInstance) {
        m_sharedInstance = new IconDatabase();
    }
    return m_sharedInstance;
}

IconDatabase::IconDatabase()
    : m_currentDB(&m_mainDB)
    , m_defaultIconDataCache(0)
    , m_privateBrowsingEnabled(false)
    , m_startupTimer(this, &IconDatabase::pruneUnretainedIconsOnStartup)
    , m_updateTimer(this, &IconDatabase::updateDatabase)
    , m_initialPruningComplete(false)
    , m_initialPruningTransaction(0)
    , m_preparedPageRetainInsertStatement(0)
{
    
}

bool IconDatabase::open(const String& databasePath)
{
    if (isOpen()) {
        LOG_ERROR("Attempt to reopen the IconDatabase which is already open.  Must close it first.");
        return false;
    }
    
    // First we'll formulate the full path for the database file
    String dbFilename;
    if (databasePath[databasePath.length()] == '/')
        dbFilename = databasePath + defaultDatabaseFilename();
    else
        dbFilename = databasePath + "/" + defaultDatabaseFilename();

    // Now, we'll see if we can open the on-disk table
    // If we can't, this ::open() failed and we should bail now
    if (!m_mainDB.open(dbFilename)) {
        LOG(IconDatabase, "Unable to open icon database at path %s", dbFilename.ascii().data());
        return false;
    }
    
    if (!isValidDatabase(m_mainDB)) {
        LOG(IconDatabase, "%s is missing or in an invalid state - reconstructing", dbFilename.ascii().data());
        clearDatabaseTables(m_mainDB);
        createDatabaseTables(m_mainDB);
    }

    m_initialPruningTransaction = new SQLTransaction(m_mainDB);
    // We're going to track an icon's retain count in a temp table in memory so we can cross reference it to to the on disk tables
    bool result;
    result = m_mainDB.executeCommand("CREATE TEMP TABLE PageRetain (url TEXT);");
    // Creating an in-memory temp table should never, ever, ever fail
    ASSERT(result);

    // These are actually two different SQLite config options - not my fault they are named confusingly  ;)
    m_mainDB.setSynchronous(SQLDatabase::SyncOff);
    m_mainDB.setFullsync(false);

    m_initialPruningTransaction->begin();
    
    // Open the in-memory table for private browsing
    if (!m_privateBrowsingDB.open(":memory:"))
        LOG_ERROR("Unabled to open in-memory database for private browsing - %s", m_privateBrowsingDB.lastErrorMsg());

    // Only if we successfully remained open will we start our "initial purge timer"
    // rdar://4690949 - when we have deferred reads and writes all the way in, the prunetimer
    // will become "deferredTimer" or something along those lines, and will be set only when
    // a deferred read/write is queued
    if (isOpen())
        m_startupTimer.startOneShot(0);
    
    return isOpen();
}

void IconDatabase::close()
{
    delete m_initialPruningTransaction;
    if (m_preparedPageRetainInsertStatement)
        m_preparedPageRetainInsertStatement->finalize();
    delete m_preparedPageRetainInsertStatement;
    syncDatabase();
    m_mainDB.close();
    m_privateBrowsingDB.close();
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

void IconDatabase::clearDatabaseTables(SQLDatabase& db)
{
    String query = "SELECT name FROM sqlite_master WHERE type='table';";
    Vector<String> tables;
    if (!SQLStatement(db, query).returnTextResults16(0, tables)) {
        LOG(IconDatabase, "Unable to retrieve list of tables from database");
        return;
    }
    
    for (Vector<String>::iterator table = tables.begin(); table != tables.end(); ++table ) {
        if (!db.executeCommand("DROP TABLE " + *table)) {
            LOG(IconDatabase, "Unable to drop table %s", (*table).ascii().data());
        }
    }
}

void IconDatabase::createDatabaseTables(SQLDatabase& db)
{
    if (!db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE Icon (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, stamp INTEGER, data BLOB);")) {
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

void IconDatabase::imageDataForIconURL(const String& iconURL, Vector<unsigned char>& result)
{      
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {    
        imageDataForIconURLQuery(m_privateBrowsingDB, iconURL, result);
        if (!result.isEmpty())
            return;
    } 
    
    // It wasn't found there, so lets check the main tables
    imageDataForIconURLQuery(m_mainDB, iconURL, result);
}

void IconDatabase::setPrivateBrowsingEnabled(bool flag)
{
    if (m_privateBrowsingEnabled == flag)
        return;
    
    // Sync any deferred DB changes before we change the active DB
    syncDatabase();
    
    m_privateBrowsingEnabled = flag;
    
    if (m_privateBrowsingEnabled) {
        createDatabaseTables(m_privateBrowsingDB);
        m_currentDB = &m_privateBrowsingDB;
    } else {
        clearDatabaseTables(m_privateBrowsingDB);
        m_currentDB = &m_mainDB;
    }
}

Image* IconDatabase::iconForPageURL(const String& pageURL, const IntSize& size, bool cache)
{   
    // See if we even have an IconURL for this PageURL...
    String iconURL = iconURLForPageURL(pageURL);
    if (iconURL.isEmpty())
        return 0;
    
    // If we do, maybe we have a IconDataCache for this IconURL
    IconDataCache* icon = getOrCreateIconDataCache(iconURL);
    
    // If it's a new IconDataCache object that doesn't have its imageData set yet,
    // we'll read in that image data now
    if (icon->imageDataStatus() == ImageDataStatusUnknown) {
        Vector<unsigned char> data;
        imageDataForIconURL(iconURL, data);
        icon->setImageData(data.data(), data.size());
    }
        
    return icon->getImage(size);
}

// FIXME 4667425 - this check needs to see if the icon's data is empty or not and apply
// iconExpirationTime to present icons, and missingIconExpirationTime for missing icons
bool IconDatabase::isIconExpiredForIconURL(const String& iconURL)
{
    if (iconURL.isEmpty()) 
        return true;
    
    // If we have a IconDataCache, then it definitely has the Timestamp in it
    IconDataCache* icon = m_iconURLToIconDataCacheMap.get(iconURL);
    if (icon) 
        return time(NULL) - icon->getTimestamp() > iconExpirationTime;
            
    // Otherwise, we'll get the timestamp from the DB and use it
    int stamp;
    if (m_privateBrowsingEnabled) {
        stamp = timeStampForIconURLQuery(m_privateBrowsingDB, iconURL);
        if (stamp)
            return (time(NULL) - stamp) > iconExpirationTime;
    }
    
    stamp = timeStampForIconURLQuery(m_mainDB, iconURL);
    if (stamp)
        return (time(NULL) - stamp) > iconExpirationTime;
    
    return false;
}
    
String IconDatabase::iconURLForPageURL(const String& pageURL)
{    
    if (pageURL.isEmpty()) 
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
    if (pageURL.isEmpty())
        return;
    
    // If we don't have the retain count for this page, we need to setup records of its retain
    // Otherwise, get the count and increment it
    int retainCount;
    if (!(retainCount = m_pageURLToRetainCount.get(pageURL))) {
        m_pageURLToRetainCount.set(pageURL, 1);   

        // If we haven't done initial pruning, we store this retain record in the temporary in-memory table
        // Note we only keep the URL in the temporary table, not the full retain count, because for pruning-considerations
        // we only care *if* a pageURL is retained - not the full count.  This call to retainIconForPageURL incremented the PageURL's
        // retain count from 0 to 1 therefore we may store it in the temporary table
        // Also, if we haven't done pruning yet, we want to avoid any pageURL->iconURL lookups and the pageURLsPendingDeletion is moot, 
        // so we bail here and skip those steps
        if (!m_initialPruningComplete) {
            String escapedPageURL = escapeSQLString(pageURL);
            if (!m_preparedPageRetainInsertStatement) {
                m_preparedPageRetainInsertStatement = new SQLStatement(m_mainDB, "INSERT INTO PageRetain VALUES (?);");
                m_preparedPageRetainInsertStatement->prepare();
            }
            m_preparedPageRetainInsertStatement->reset();
            m_preparedPageRetainInsertStatement->bindText16(1, pageURL);
            if (m_preparedPageRetainInsertStatement->step() != SQLITE_DONE)
                LOG_ERROR("Failed to record icon retention in temporary table for IconURL %s", pageURL.ascii().data());
            return;
        }
        
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
    if (pageURL.isEmpty())
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
    
    // If we haven't done initial pruning, we remove this retain record from the temporary in-memory table
    // Note we only keep the URL in the temporary table, not the full retain count, because for pruning-considerations
    // we only care *if* a pageURL is retained - not the full count.  This call to releaseIconForPageURL decremented the PageURL's
    // retain count from 1 to 0 therefore we may remove it from the temporary table
    // Also, if we haven't done pruning yet, we want to avoid any pageURL->iconURL lookups and the pageURLsPendingDeletion is moot, 
    // so we bail here and skip those steps
    if (!m_initialPruningComplete) {
        String escapedPageURL = escapeSQLString(pageURL);
        if (!m_mainDB.executeCommand("DELETE FROM PageRetain WHERE url='" + escapedPageURL + "';"))
            LOG_ERROR("Failed to delete record of icon retention from temporary table for IconURL %s", pageURL.ascii().data());
        return;
    }

    
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
    
    if (!m_currentDB->executeCommand(String::sprintf("DELETE FROM Icon WHERE Icon.iconID = %lli;", iconID)))
        LOG_ERROR("Unable to drop Icon for IconURL", iconURL.ascii().data()); 
    if (!m_currentDB->executeCommand(String::sprintf("DELETE FROM PageURL WHERE PageURL.iconID = %lli", iconID)))
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
        icon->setTimestamp(time(NULL));
        m_iconDataCachesPendingUpdate.add(icon);
    } else 
        icon->setTimestamp(timestamp);
        
    return icon;
}

void IconDatabase::setIconDataForIconURL(const void* data, int size, const String& iconURL)
{
    ASSERT(size > -1);
    if (size)
        ASSERT(data);
    else
        data = 0;
        
    if (iconURL.isEmpty())
        return;
    
    // Get the IconDataCache for this IconURL (note, IconDataCacheForIconURL will create it if necessary)
    IconDataCache* icon = getOrCreateIconDataCache(iconURL);
    
    // Set the data in the IconDataCache
    icon->setImageData((unsigned char*)data, size);
    
    // Update the timestamp in the IconDataCache to NOW
    icon->setTimestamp(time(NULL));

    // Mark the IconDataCache as requiring an update to the database
    m_iconDataCachesPendingUpdate.add(icon);
}

void IconDatabase::setHaveNoIconForIconURL(const String& iconURL)
{   
    setIconDataForIconURL(0, 0, iconURL);
}

void IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL)
{
    ASSERT(!iconURL.isEmpty());
    ASSERT(!pageURL.isEmpty());
    
    // If the urls already map to each other, bail.
    // This happens surprisingly often, and seems to cream iBench performance
    if (m_pageURLToIconURLMap.get(pageURL) == iconURL)
        return;

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
    String escapedIconURL = escapeSQLString(iconURL);
    
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
    CFTimeInterval start = CFAbsoluteTimeGetCurrent();
#endif
    
    // rdar://4690949 - Need to prune unretained iconURLs here, then prune out all pageURLs that reference
    // nonexistent icons
    
    // First wipe all PageURLs and Icons that aren't retained
    if (!m_mainDB.executeCommand("DELETE FROM PageURL WHERE PageURL.url NOT IN (SELECT url FROM PageRetain);") ||
        !m_mainDB.executeCommand("DELETE FROM Icon WHERE Icon.iconID NOT IN (SELECT iconID FROM PageURL);") ||
        !m_mainDB.executeCommand("DROP TABLE PageRetain;")) {
        LOG_ERROR("Failed to execute SQL to prune unretained pages and icons from the on-disk tables");
        m_initialPruningTransaction->rollback();
    } else
        m_initialPruningTransaction->commit();
    delete m_initialPruningTransaction;
    m_initialPruningTransaction = 0;
    
    // Since we lazily retained the pageURLs without getting the iconURLs or retaining the iconURLs, 
    // we need to do that now
    // We now should be in a spiffy situation where we know every single pageURL left in the DB is retained, so we are interested
    // in the iconURLs for all remaining pageURLs
    // So we can simply add all the remaining mappings, and retain each pageURL's icon once
    
    SQLStatement sql(m_mainDB, "SELECT PageURL.url, Icon.url FROM PageURL INNER JOIN Icon ON PageURL.iconID=Icon.iconID");
    sql.prepare();
    int result;
    while((result = sql.step()) == SQLITE_ROW) {
        String iconURL = sql.getColumnText16(1);
        m_pageURLToIconURLMap.set(sql.getColumnText16(0), iconURL);
        retainIconURL(iconURL);
        LOG(IconDatabase, "Found a PageURL that mapped to %s", iconURL.ascii().data());
    }
    if (result != SQLITE_DONE)
        LOG_ERROR("Error reading PageURL->IconURL mappings from on-disk DB");
    sql.finalize();

    if (m_preparedPageRetainInsertStatement) {
        m_preparedPageRetainInsertStatement->finalize();
        delete m_preparedPageRetainInsertStatement;
        m_preparedPageRetainInsertStatement = 0;
    }
    m_initialPruningComplete = true;
    
#ifndef NDEBUG
    CFTimeInterval duration = CFAbsoluteTimeGetCurrent() - start;
    if (duration <= 1.0)
        LOG(IconDatabase, "Pruning unretained icons took %.4f seconds", duration);
    else
        LOG(IconDatabase, "Pruning unretained icons took %.4f seconds - this is much too long!", duration);
#endif
}

void IconDatabase::updateDatabase(Timer<IconDatabase>*)
{
    syncDatabase();
}

void IconDatabase::syncDatabase()
{
#ifndef NDEBUG
    CFTimeInterval start = CFAbsoluteTimeGetCurrent();
#endif

    // First we'll do the pending additions
    // Starting with the IconDataCaches that need updating/insertion
    for (HashSet<IconDataCache*>::iterator i = m_iconDataCachesPendingUpdate.begin(), end = m_iconDataCachesPendingUpdate.end(); i != end; ++i) {
        (*i)->writeToDatabase(*m_currentDB);
        LOG(IconDatabase, "Wrote IconDataCache for IconURL %s with timestamp of %lli to the DB", (*i)->getIconURL().ascii().data(), (*i)->getTimestamp());
    }
    m_iconDataCachesPendingUpdate.clear();
    
    HashSet<String>::iterator i = m_pageURLsPendingAddition.begin(), end = m_pageURLsPendingAddition.end();
    for (; i != end; ++i) {
        setIconURLForPageURLInDatabase(m_pageURLToIconURLMap.get(*i), *i);
        LOG(IconDatabase, "Commited IconURL for PageURL %s to database", (*i).ascii().data());
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
    CFTimeInterval duration = CFAbsoluteTimeGetCurrent() - start;
    if (duration <= 1.0)
        LOG(IconDatabase, "Updating the database took %.4f seconds", duration);
    else 
        LOG(IconDatabase, "Updating the database took %.4f seconds - this is much too long!", duration);
#endif
}

bool IconDatabase::hasEntryForIconURL(const String& iconURL)
{
    if (iconURL.isEmpty())
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

IconDatabase::~IconDatabase()
{
    close();
}

// Query helper functions
static bool pageURLTableIsEmptyQuery(SQLDatabase& db)
{
    return !(SQLStatement(db, "SELECT iconID FROM PageURL LIMIT 1;").returnsAtLeastOneResult());
}

static void imageDataForIconURLQuery(SQLDatabase& db, const String& iconURL, Vector<unsigned char>& result)
{
    String escapedIconURL = escapeSQLString(iconURL);
    SQLStatement(db, "SELECT Icon.data FROM Icon WHERE Icon.url = '" + escapedIconURL + "';").getColumnBlobAsVector(0, result);
}

static int timeStampForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    String escapedIconURL = escapeSQLString(iconURL);
    return SQLStatement(db, "SELECT Icon.stamp FROM Icon WHERE Icon.url = '" + escapedIconURL + "';").getColumnInt(0);
}

static String iconURLForPageURLQuery(SQLDatabase& db, const String& pageURL)
{
    String escapedPageURL = escapeSQLString(pageURL);
    return SQLStatement(db, "SELECT Icon.url FROM Icon, PageURL WHERE PageURL.url = '" + escapedPageURL + "' AND Icon.iconID = PageURL.iconID").getColumnText16(0);
}

static void forgetPageURLQuery(SQLDatabase& db, const String& pageURL)
{
    String escapedPageURL = escapeSQLString(pageURL);
    db.executeCommand("DELETE FROM PageURL WHERE url = '" + escapedPageURL + "';");
}

static void setIconIDForPageURLQuery(SQLDatabase& db, int64_t iconID, const String& pageURL)
{
    String escapedPageURL = escapeSQLString(pageURL);
    if (!db.executeCommand("INSERT INTO PageURL (url, iconID) VALUES ('" + escapedPageURL + "', " + String::number(iconID) + ");"))
        LOG_ERROR("Failed to set iconid %lli for PageURL %s", iconID, pageURL.ascii().data());
}

static int64_t getIconIDForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    String escapedIconURL = escapeSQLString(iconURL);
    return SQLStatement(db, "SELECT Icon.iconID FROM Icon WHERE Icon.url = '" + escapedIconURL + "';").getColumnInt64(0);
}

static int64_t addIconForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    String escapedIconURL = escapeSQLString(iconURL);
    if (db.executeCommand("INSERT INTO Icon (url) VALUES ('" + escapedIconURL + "');"))
        return db.lastInsertRowID();
    return 0;
}

static bool hasIconForIconURLQuery(SQLDatabase& db, const String& iconURL)
{
    String escapedIconURL = escapeSQLString(iconURL);
    return SQLStatement(db, "SELECT Icon.iconID FROM Icon WHERE Icon.url = '" + escapedIconURL + "';").returnsAtLeastOneResult();
}

} //namespace WebCore
