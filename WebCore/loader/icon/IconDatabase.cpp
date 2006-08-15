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

#include "Image.h"
#include "Logging.h"
#include "PlatformString.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>


// FIXME - Make sure we put a private browsing consideration in that uses the temporary tables anytime private browsing would be an issue.

// FIXME - One optimization to be made when this is no longer in flux is to make query construction smarter - that is queries that are created from
// multiple strings and numbers should be handled differently than with String + String + String + etc.

namespace WebCore {

IconDatabase* IconDatabase::m_sharedInstance = 0;
const int IconDatabase::currentDatabaseVersion = 3;

// Icons expire once a day
const int IconDatabase::iconExpirationTime = 60*60*24; 
// Absent icons are rechecked once a week
const int IconDatabase::missingIconExpirationTime = 60*60*24*7; 

const String& IconDatabase::defaultDatabaseFilename()
{
    static String defaultDatabaseFilename = "icon.db";
    return defaultDatabaseFilename;
}


IconDatabase* IconDatabase::sharedIconDatabase()
{
    if (!m_sharedInstance) {
        m_sharedInstance = new IconDatabase();
    }
    return m_sharedInstance;
}

IconDatabase::IconDatabase()
    : m_privateBrowsingEnabled(false)
    , m_startupTimer(this, &IconDatabase::pruneUnretainedIcons)
{
    close();
}

bool IconDatabase::open(const String& databasePath)
{
    close();
    String dbFilename;
    
    if (databasePath[databasePath.length()] == '/')
        dbFilename = databasePath + defaultDatabaseFilename();
    else
        dbFilename = databasePath + "/" + defaultDatabaseFilename();

    if (!m_db.open(dbFilename)) {
        LOG(IconDatabase, "Unable to open icon database at path %s", dbFilename.ascii().data());
        return false;
    }
    
    if (!isValidDatabase()) {
        LOG(IconDatabase, "%s is missing or in an invalid state - reconstructing", dbFilename.ascii().data());
        clearDatabase();
        recreateDatabase();
    }

    // We're going to track an icon's retain count in a temp table in memory so we can cross reference it to to the on disk tables
    bool result;
    result = m_db.executeCommand("CREATE TEMP TABLE PageRetain (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,count INTEGER NOT NULL ON CONFLICT FAIL);");
    // Creating an in-memory temp table should never, ever, ever fail
    ASSERT(result);
    
    // These are actually two different SQLite config options - not my fault they are named confusingly  ;)
    m_db.setSynchronous(SQLDatabase::SyncOff);    
    m_db.setFullsync(false);
    
    // Only if we successfully remained open will we setup our "initial purge timer"
    if (isOpen())
        m_startupTimer.startOneShot(0);
    
    return isOpen();
}

void IconDatabase::close()
{
    //TODO - sync any cached info before m_db.close();
    m_db.close();
}

bool IconDatabase::isEmpty()
{
    return !(SQLStatement(m_db, "SELECT iconID FROM PageURL LIMIT 1;").getColumnInt(0));
}


bool IconDatabase::isValidDatabase()
{
    if (!m_db.tableExists("Icon") || !m_db.tableExists("PageURL") || !m_db.tableExists("IconResource") || !m_db.tableExists("IconDatabaseInfo")) {
        return false;
    }
    
    if (SQLStatement(m_db, "SELECT value FROM IconDatabaseInfo WHERE key = 'Version';").getColumnInt(0) < currentDatabaseVersion) {
        LOG(IconDatabase, "DB version is not found or below expected valid version");
        return false;
    }
    
    return true;
}

void IconDatabase::clearDatabase()
{
    String query = "SELECT name FROM sqlite_master WHERE type='table';";
    Vector<String> tables;
    if (!SQLStatement(m_db, query).returnTextResults16(0, tables)) {
        LOG(IconDatabase, "Unable to retrieve list of tables from database");
        return;
    }
    
    for (Vector<String>::iterator table = tables.begin(); table != tables.end(); ++table ) {
        if (!m_db.executeCommand("DROP TABLE " + *table)) {
            LOG(IconDatabase, "Unable to drop table %s", (*table).ascii().data());
        }
    }
    
    deletePrivateTables();
}

void IconDatabase::recreateDatabase()
{
    if (!m_db.executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE Icon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL UNIQUE ON CONFLICT FAIL, stamp INTEGER);")) {
        LOG_ERROR("Could not create Icon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE IconResource (iconID integer NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, data BLOB);")) {
        LOG_ERROR("Could not create IconResource table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TRIGGER create_icon_resource AFTER INSERT ON Icon BEGIN INSERT INTO IconResource (iconID, data) VALUES (new.iconID, NULL); END;")) {
        LOG_ERROR("Unable to create create_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TRIGGER delete_icon_resource AFTER DELETE ON Icon BEGIN DELETE FROM IconResource WHERE IconResource.iconID =  old.iconID; END;")) {
        LOG_ERROR("Unable to create delete_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TRIGGER update_icon_timestamp AFTER UPDATE ON IconResource BEGIN UPDATE Icon SET stamp = strftime('%s','now') WHERE iconID = new.iconID; END;")) {
        LOG_ERROR("Unable to create update_icon_timestamp trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
}    

void IconDatabase::createPrivateTables()
{
    if (!m_db.executeCommand("CREATE TEMP TABLE TempPageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) 
        LOG_ERROR("Could not create TempPageURL table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIcon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL UNIQUE ON CONFLICT FAIL, stamp INTEGER);")) 
        LOG_ERROR("Could not create TempIcon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIconResource (iconID INTERGER NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data BLOB);")) 
        LOG_ERROR("Could not create TempIconResource table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TRIGGER temp_create_icon_resource AFTER INSERT ON TempIcon BEGIN INSERT INTO TempIconResource (iconID, data) VALUES (new.iconID, NULL); END;")) 
        LOG_ERROR("Unable to create temp_create_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
}

void IconDatabase::deletePrivateTables()
{
    m_db.executeCommand("DROP TABLE TempPageURL;");
    m_db.executeCommand("DROP TABLE TempIcon;");
    m_db.executeCommand("DROP TABLE TempIconResource;");
}

Vector<unsigned char> IconDatabase::imageDataForIconID(int id)
{
    return SQLStatement(m_db, String::sprintf("SELECT data FROM IconResource WHERE iconid = %i", id)).getColumnBlobAsVector(0);
}

Vector<unsigned char> IconDatabase::imageDataForIconURL(const String& _iconURL)
{
    //Escape single quotes for SQL 
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
      
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {    
        Vector<unsigned char> blob = SQLStatement(m_db, "SELECT TempIconResource.data FROM TempIconResource, TempIcon WHERE TempIcon.url = '" + iconURL + "' AND TempIconResource.iconID = TempIcon.iconID;").getColumnBlobAsVector(0);
        if (!blob.isEmpty()) {
            LOG(IconDatabase, "Icon data pulled from temp tables");
            return blob;
        }
    } 
    
    // It wasn't found there, so lets check the main tables
    return SQLStatement(m_db, "SELECT IconResource.data FROM IconResource, Icon WHERE Icon.url = '" + iconURL + "' AND IconResource.iconID = Icon.iconID;").getColumnBlobAsVector(0);
}

Vector<unsigned char> IconDatabase::imageDataForPageURL(const String& _pageURL)
{
    //Escape single quotes for SQL 
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    
    LOG(IconDatabase, "imageDataForIconURL called using preferred method");
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {    
        Vector<unsigned char> blob = SQLStatement(m_db, "SELECT TempIconResource.data FROM TempIconResource, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIconResource.iconID = TempPageURL.iconID;").getColumnBlobAsVector(0);
        if (!blob.isEmpty()) {
            LOG(IconDatabase, "Icon data pulled from temp tables");
            return blob;
        }
    } 
    
    // It wasn't found there, so lets check the main tables
    return SQLStatement(m_db, "SELECT IconResource.data FROM IconResource, PageURL WHERE PageURL.url = '" + pageURL + "' AND IconResource.iconID = PageURL.iconID;").getColumnBlobAsVector(0);
}

void IconDatabase::setPrivateBrowsingEnabled(bool flag)
{
    if (m_privateBrowsingEnabled == flag)
        return;
    
    m_privateBrowsingEnabled = flag;
    
    if (!m_privateBrowsingEnabled)
        deletePrivateTables();
    else
        createPrivateTables();
}

Image* IconDatabase::iconForPageURL(const String& url, const IntSize& size, bool cache)
{   
    String iconURL;

    // We may have a SiteIcon for this specific PageURL...
    if (m_pageURLToSiteIcons.contains(url))
        return m_pageURLToSiteIcons.get(url)->getImage(size);
    
    // Otherwise see if we even have an IconURL for this PageURL...
    // The weird flow here is because we declare the iconURL variable up above, but MAY not have retrieved the string yet
    // Trying to keep out excessive SQLite calls, which the pageURL->iconURL mapping incur
    if (iconURL.isEmpty())
        iconURL = iconURLForPageURL(url);
    if (iconURL.isEmpty())
        return 0;
    
    // If we do, maybe we have an image for this IconURL
    if (m_iconURLToSiteIcons.contains(iconURL)) {
        SiteIcon* icon = m_iconURLToSiteIcons.get(iconURL);
        // Assign this SiteIcon to this PageURL for faster lookup in the future
        m_pageURLToSiteIcons.set(url, icon);
        return icon->getImage(size);
    }
        
    // If we don't have either, we have to create the SiteIcon
    SiteIcon* icon = new SiteIcon(iconURL);
    m_pageURLToSiteIcons.set(url, icon);
    m_iconURLToSiteIcons.set(iconURL, icon);
    return icon->getImage(size);
}

// FIXME 4667425 - this check needs to see if the icon's data is empty or not and apply
// iconExpirationTime to present icons, and missingIconExpirationTime for missing icons
bool IconDatabase::isIconExpiredForIconURL(const String& _iconURL)
{
    if (_iconURL.isEmpty()) 
        return true;
        
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    int stamp;
    if (m_privateBrowsingEnabled) {
        stamp = SQLStatement(m_db, "SELECT TempIcon.stamp FROM TempIcon WHERE TempIcon.url = '" + iconURL + "';").getColumnInt(0);
        if (stamp)
            return (time(NULL) - stamp) > iconExpirationTime;
    }
    stamp = SQLStatement(m_db, "SELECT Icon.stamp FROM Icon WHERE Icon.url = '" + iconURL + "';").getColumnInt(0);
    
    if (stamp)
        return (time(NULL) - stamp) > iconExpirationTime;
    return false;
}

bool IconDatabase::isIconExpiredForPageURL(const String& _pageURL)
{
    // We don't want to encourage kicking off any loads for an empty url, so this
    // case will always return false
    if (_pageURL.isEmpty()) 
        return false;
        
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    
    int stamp;
    if (m_privateBrowsingEnabled) {
        stamp = SQLStatement(m_db, "SELECT TempIcon.stamp FROM TempIcon, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIcon.iconID = TempPageURL.iconID").getColumnInt(0);
        if (stamp)
            return (time(NULL) - stamp) > iconExpirationTime;
    }
    stamp = SQLStatement(m_db, "SELECT Icon.stamp FROM Icon, PageURL WHERE PageURL.url = '" + pageURL + "' AND Icon.iconID = PageURL.iconID").getColumnInt(0);
    if (stamp)
        return (time(NULL) - stamp) > iconExpirationTime;
    return false;
}
    
String IconDatabase::iconURLForPageURL(const String& _pageURL)
{
    if (_pageURL.isEmpty()) 
        return String();
        
    if (m_pageURLToIconURLMap.contains(_pageURL))
        return m_pageURLToIconURLMap.get(_pageURL);
        
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    
    // Try the private browsing tables because if any PageURL's IconURL was updated during privated browsing, it would be here
    if (m_privateBrowsingEnabled) {
        String iconURL = SQLStatement(m_db, "SELECT TempIcon.url FROM TempIcon, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIcon.iconID = TempPageURL.iconID").getColumnText16(0);
        if (!iconURL.isEmpty())
            return iconURL;
    }
    
    String iconURL = SQLStatement(m_db, "SELECT Icon.url FROM Icon, PageURL WHERE PageURL.url = '" + pageURL + "' AND Icon.iconID = PageURL.iconID").getColumnText16(0);
    if (!iconURL.isEmpty())
        m_pageURLToIconURLMap.set(_pageURL, iconURL);
    return iconURL;
}

Image* IconDatabase::defaultIcon(const IntSize& size)
{
    return 0;
}

void IconDatabase::retainIconForPageURL(const String& _url)
{
    if (_url.isEmpty())
        return;
        
    String url = _url;
    url.replace('\'', "''");
    
    int retainCount = SQLStatement(m_db, "SELECT count FROM PageRetain WHERE url = '" + url + "';").getColumnInt(0);
    ASSERT(retainCount > -1);
    
    if (!m_db.executeCommand("INSERT INTO PageRetain VALUES ('" + url + "', " + String::number(retainCount + 1) + ");"))
        LOG_ERROR("Failed to increment retain count for url %s", _url.ascii().data());
        
}

void IconDatabase::releaseIconForPageURL(const String& _url)
{
    if (_url.isEmpty())
        return;
        
    String url = _url;
    url.replace('\'', "''");
    
    SQLStatement sql(m_db, "SELECT count FROM PageRetain WHERE url = '" + url + "';");
    switch (sql.prepareAndStep()) {
        case SQLITE_ROW:
            break;
        case SQLITE_DONE:
            LOG_ERROR("Released icon for url %s that had not been retained", _url.ascii().data());
            return;
        default:
            LOG_ERROR("Error retrieving retain count for url %s", _url.ascii().data());
            return;
    }
    
    int retainCount = sql.getColumnInt(0);
    sql.finalize();
    
    // If the retain count SOMEHOW gets to zero or less, we need to bail right here   
    if (retainCount < 1) {
        LOG_ERROR("Attempting to release icon for URL %s - already fully released", _url.ascii().data());
        return;
    }
    
    --retainCount;
    if (!m_db.executeCommand("INSERT INTO PageRetain VALUES ('" + url + "', " + String::number(retainCount) + ");"))
        LOG_ERROR("Failed to decrement retain count for url %s", _url.ascii().data());
        
    // If we still have a positve retain count, we're done - lets bail
    if (retainCount)
        return;
    
    // Grab the iconURL for later use...
    String iconURL = iconURLForPageURL(_url);
    
    // The retain count is zilch so we can wipe this PageURL
    if (!m_db.executeCommand("DELETE FROM PageRetain WHERE url = '" + url + "';"))
        LOG_ERROR("Failed to delete retain record for url %s", _url.ascii().data());
    if (m_privateBrowsingEnabled)
        if (!m_db.executeCommand("DELETE FROM TempPageURL WHERE url = '" + url + "';"))
            LOG_ERROR("Failed to delete record of PageURL %s from private browsing tables", _url.ascii().data());
    if (!m_db.executeCommand("DELETE FROM PageURL WHERE url = '" + url + "';"))
        LOG_ERROR("Failed to delete record of PageURL %s from on-disk tables", _url.ascii().data());            
            
    // And now see if we can wipe the icon itself
    if (iconURL.isEmpty())
        return;
        
    // If the icon has other retainers, we're all done - bail
    if (isIconURLRetained(iconURL))
        return;
        
    LOG(IconDatabase, "No retainers for Icon URL %s - forgetting icon altogether", iconURL.ascii().data());

    // Wipe it from the database...
    forgetIconForIconURLFromDatabase(iconURL);

    // And then from the SiteIcons
    SiteIcon* icon1;
    SiteIcon* icon2;
    if ((icon1 = m_pageURLToSiteIcons.get(_url)))
        m_pageURLToSiteIcons.remove(_url);
    if ((icon2 = m_iconURLToSiteIcons.get(iconURL)))
        m_iconURLToSiteIcons.remove(iconURL);
    
    if (icon1 && icon2) {
        ASSERT(icon1 == icon2);
        delete icon1;
        icon1 = icon2 = 0;
    } 
    if (icon1)
        delete icon1;
    else if (icon2)
        delete icon2;
}

bool IconDatabase::isIconURLRetained(const String& _iconURL)
{
    if (_iconURL.isEmpty())
        return false;
        
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    return SQLStatement(m_db, "SELECT count FROM PageRetain WHERE url IN(SELECT PageURL.url FROM PageURL, Icon WHERE PageURL.iconID = Icon.iconID AND Icon.url = '" + iconURL + "') LIMIT 1;").returnsAtLeastOneResult();
}

int IconDatabase::totalRetainCountForIconURL(const String& _iconURL)
{
    if (_iconURL.isEmpty())
        return 0;
        
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    int retainCount = SQLStatement(m_db, "SELECT sum(count) FROM PageRetain WHERE url IN(SELECT PageURL.url FROM PageURL, Icon WHERE PageURL.iconID = Icon.iconID AND Icon.url = '" + iconURL + "');").getColumnInt(0);
    LOG(IconDatabase, "The total retain count for URL %s is %i", _iconURL.ascii().data(), retainCount);
    return retainCount;
}

void IconDatabase::forgetIconForIconURLFromDatabase(const String& _iconURL)
{
    if (_iconURL.isEmpty())
        return;
        
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    // Lets start with the icon from the temporary private tables...
    int64_t iconID;
    if (m_privateBrowsingEnabled) {
        iconID = establishTemporaryIconIDForIconURL(iconURL, false);
        if (iconID) {
            if (!m_db.executeCommand(String::sprintf("DELETE FROM TempIcon WHERE iconID = %lli", iconID)))
                LOG_ERROR("Unable to drop Icon at URL %s from table TemporaryIcon", iconURL.ascii().data()); 
            if (!m_db.executeCommand(String::sprintf("DELETE FROM TempPageURL WHERE iconID = %lli", iconID)))
                LOG_ERROR("Unable to drop temporary PageURLs pointing at Icon URL %s", iconURL.ascii().data());
        }
    }
    
    // And then from the on-disk tables
    iconID = establishIconIDForIconURL(iconURL, false);
    if (iconID) {
        if (!m_db.executeCommand(String::sprintf("DELETE FROM Icon WHERE iconID = %lli", iconID)))
            LOG_ERROR("Unable to drop Icon at URL %s from table Icon", iconURL.ascii().data()); 
        if (!m_db.executeCommand(String::sprintf("DELETE FROM PageURL WHERE iconID = %lli", iconID)))
            LOG_ERROR("Unable to drop PageURLs pointing at Icon URL %s", iconURL.ascii().data());
    }

}

void IconDatabase::setIconDataForIconURL(const void* data, int size, const String& _iconURL)
{
    ASSERT(size > -1);
    if (size)
        ASSERT(data);
    else
        data = 0;
        
    if (_iconURL.isEmpty()) {
        LOG_ERROR("Attempt to set icon for blank url");
        return;
    }
    
    // First, if we already have a SiteIcon in memory, let's update its image data
    if (m_iconURLToSiteIcons.contains(_iconURL))
        m_iconURLToSiteIcons.get(_iconURL)->manuallySetImageData((unsigned char*)data, size);

    // Next, we actually commit the image data to the database
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");

    int64_t iconID;
    String resourceTable;

    // If we're in private browsing, we'll keep a record in the temporary tables instead of in the ondisk table
    if (m_privateBrowsingEnabled) {
        iconID = establishTemporaryIconIDForEscapedIconURL(iconURL);
        if (!iconID) {
            LOG(IconDatabase, "Failed to establish an iconID for URL %s in the private browsing table", _iconURL.ascii().data());
            return;
        }
        resourceTable = "TempIconResource";
    } else {
        iconID = establishIconIDForEscapedIconURL(iconURL);
        if (!iconID) {
            LOG(IconDatabase, "Failed to establish an iconID for URL %s in the on-disk table", _iconURL.ascii().data());
            return;
        }
        resourceTable = "IconResource";
    }
    
    performSetIconDataForIconID(iconID, resourceTable, data, size);
}

void IconDatabase::performSetIconDataForIconID(int64_t iconID, const String& resourceTable, const void* data, int size)
{
    ASSERT(iconID);
    ASSERT(!resourceTable.isEmpty());
    if (data)
        ASSERT(size > 0);
        
    // First we create and prepare the SQLStatement
    // The following statement also works to set the icon data to NULL because sqlite defaults unbound ? parameters to NULL
    SQLStatement sql(m_db, "UPDATE " + resourceTable + " SET data = ? WHERE iconID = ?;");
    sql.prepare();
        
    // Then we bind the icondata and the iconID to the SQLStatement
    if (data)
        sql.bindBlob(1, data, size);
    sql.bindInt64(2, iconID);
        
    // Finally we step and make sure the step was successful
    if (sql.step() != SQLITE_DONE)
        LOG_ERROR("Unable to set icon resource data in table %s for iconID %lli", resourceTable.ascii().data(), iconID);
    LOG(IconDatabase, "Icon data set in table %s for iconID %lli", resourceTable.ascii().data(), iconID);
    return;
}

int64_t IconDatabase::establishTemporaryIconIDForIconURL(const String& _iconURL, bool create)
{
    if (_iconURL.isEmpty())
        return 0;
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    return establishTemporaryIconIDForEscapedIconURL(iconURL, create);
}

int64_t IconDatabase::establishTemporaryIconIDForEscapedIconURL(const String& iconURL, bool create)
{
    // We either lookup the iconURL and return its ID, or we create a new one for it
    int64_t iconID = 0;
    SQLStatement sql(m_db, "SELECT iconID FROM TempIcon WHERE url = '" + iconURL + "';");
    sql.prepare();    
    if (sql.step() == SQLITE_ROW) {
        iconID = sql.getColumnInt64(0);
    } else {
        sql.finalize();
        if (create)
            if (m_db.executeCommand("INSERT INTO TempIcon (url) VALUES ('" + iconURL + "');"))
                iconID = m_db.lastInsertRowID();
    }
    return iconID;
}

int64_t IconDatabase::establishIconIDForIconURL(const String& _iconURL, bool create)
{
    if (_iconURL.isEmpty())
        return 0;
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    return establishIconIDForEscapedIconURL(iconURL, create);
}

int64_t IconDatabase::establishIconIDForEscapedIconURL(const String& iconURL, bool create)
{
    // We either lookup the iconURL and return its ID, or we create a new one for it
    int64_t iconID = 0;
    SQLStatement sql(m_db, "SELECT iconID FROM Icon WHERE url = '" + iconURL + "';");
    sql.prepare();    
    if (sql.step() == SQLITE_ROW) {
        iconID = sql.getColumnInt64(0);
    } else {
        sql.finalize();
        if (create)
            if (m_db.executeCommand("INSERT INTO Icon (url) VALUES ('" + iconURL + "');"))
                iconID = m_db.lastInsertRowID();
    }
    return iconID;
}

void IconDatabase::setHaveNoIconForIconURL(const String& _iconURL)
{   
    setIconDataForIconURL(0, 0, _iconURL);
}

void IconDatabase::setIconURLForPageURL(const String& _iconURL, const String& _pageURL)
{
    ASSERT(!_iconURL.isEmpty());
    ASSERT(!_pageURL.isEmpty());
    
    // If the urls already map to each other, bail.
    // This happens surprisingly often, and seems to cream iBench performance
    if (m_pageURLToIconURLMap.get(_pageURL) == _iconURL)
        return;
    
    String iconURL = _iconURL;
    iconURL.replace('\'',"''");
    String pageURL = _pageURL;
    pageURL.replace('\'',"''");

    int64_t iconID;
    String pageTable;
    if (m_privateBrowsingEnabled) {
        iconID = establishTemporaryIconIDForEscapedIconURL(iconURL);
        pageTable = "TempPageURL";
    } else {
        iconID = establishIconIDForEscapedIconURL(iconURL);
        pageTable = "PageURL";
    }
    
    if (!iconID) {
        LOG_ERROR("Failed to establish an ID for iconURL %s", iconURL.ascii().data());
        return;
    }
    
    // Cache the mapping...
    m_pageURLToIconURLMap.set(_pageURL, _iconURL);
    // Change the cached pageURL->SiteIcon mapping based on the new iconURL
    if (m_iconURLToSiteIcons.contains(_iconURL))
        m_pageURLToSiteIcons.set(_pageURL, m_iconURLToSiteIcons.get(_iconURL));
    else
        m_pageURLToSiteIcons.remove(_pageURL);
    // Update the DB
    performSetIconURLForPageURL(iconID, pageTable, pageURL);
}

void IconDatabase::performSetIconURLForPageURL(int64_t iconID, const String& pageTable, const String& pageURL)
{
    ASSERT(iconID);
    if (m_db.returnsAtLeastOneResult("SELECT url FROM " + pageTable + " WHERE url = '" + pageURL + "';")) {
        if (!m_db.executeCommand("UPDATE " + pageTable + " SET iconID = " + String::number(iconID) + " WHERE url = '" + pageURL + "';"))
            LOG_ERROR("Failed to update record in %s - %s", pageTable.ascii().data(), m_db.lastErrorMsg());
    } else
        if (!m_db.executeCommand("INSERT INTO " + pageTable + " (url, iconID) VALUES ('" + pageURL + "', " + String::number(iconID) + ");"))
            LOG_ERROR("Failed to insert record into %s - %s", pageTable.ascii().data(), m_db.lastErrorMsg());
}

void IconDatabase::pruneUnreferencedIcons(int numberToPrune)
{
    if (!numberToPrune || !isOpen())
        return;
    
    if (numberToPrune > 0) {
        if (!m_db.executeCommand(String::sprintf("DELETE FROM Icon WHERE Icon.iconID IN (SELECT Icon.iconID FROM Icon WHERE Icon.iconID NOT IN(SELECT PageURL.iconID FROM PageURL) LIMIT %i);", numberToPrune)))
            LOG_ERROR("Failed to prune %i unreferenced icons from the DB - %s", numberToPrune, m_db.lastErrorMsg());
    } else {
        if (!m_db.executeCommand("DELETE FROM Icon WHERE Icon.iconID IN (SELECT Icon.iconID FROM Icon WHERE Icon.iconID NOT IN(SELECT PageURL.iconID FROM PageURL));"))
            LOG_ERROR("Failed to prune all unreferenced icons from the DB - %s", m_db.lastErrorMsg());
    }
}

void IconDatabase::pruneUnretainedIcons(Timer<IconDatabase>* timer)
{
    if (!isOpen())
        return;
        
// FIXME - The PageURL delete and the pruneunreferenced icons need to be in an atomic transaction
#ifndef NDEBUG
    double start = CFAbsoluteTimeGetCurrent();
#endif
    if (!m_db.executeCommand("DELETE FROM PageURL WHERE PageURL.url NOT IN(SELECT url FROM PageRetain WHERE count > 0);"))
        LOG_ERROR("Failed to delete unretained PageURLs from DB - %s", m_db.lastErrorMsg());
    pruneUnreferencedIcons(-1);
#ifndef NDEBUG
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(IconDatabase, "Pruning unretained icons took %d seconds", duration);
    if (duration > 1.0) 
        LOG_ERROR("Pruning unretained icons took %d seconds - this is much too long!", duration);
#endif
}


bool IconDatabase::hasIconForIconURL(const String& _url)
{
    // First check the in memory mapped icons...
    if (m_iconURLToSiteIcons.contains(_url))
        return true;

    // Check the on-disk database as we're more likely to have more icons there
    String url = _url;
    url.replace('\'', "''");
    
    String query = "SELECT IconResource.data FROM IconResource, Icon WHERE Icon.url = '" + url + "' AND IconResource.iconID = Icon.iconID;";
    int size = 0;
    const void* data = SQLStatement(m_db, query).getColumnBlob(0, size);
    if (data && size)
        return true;
    
    // Finally, check the temporary tables for private browsing if enabled
    if (m_privateBrowsingEnabled) {
        query = "SELECT TempIconResource.data FROM TempIconResource, TempIcon WHERE TempIcon.url = '" + url + "' AND TempIconResource.iconID = TempIcon.iconID;";
        size = 0;
        data = SQLStatement(m_db, query).getColumnBlob(0, size);
        LOG(IconDatabase, "Checking for icon for IconURL %s in temporary tables", _url.ascii().data());
        return data && size;
    }
    return false;
}

IconDatabase::~IconDatabase()
{
    m_db.close();
}

} //namespace WebCore
