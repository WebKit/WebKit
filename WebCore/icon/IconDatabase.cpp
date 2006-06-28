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
#include "IconDatabase.h"

#include "Image.h"
#include "Logging.h"
#include "PlatformString.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

// FIXME - Make sure we put a private browsing consideration in that uses the temporary tables anytime private browsing would be an issue.

const char* DefaultIconDatabaseFilename = "/icon.db";

namespace WebCore {

IconDatabase* IconDatabase::m_sharedInstance = 0;
const int IconDatabase::currentDatabaseVersion = 3;

IconDatabase* IconDatabase::sharedIconDatabase()
{
    if (!m_sharedInstance) {
        m_sharedInstance = new IconDatabase();
    }
    return m_sharedInstance;
}

IconDatabase::IconDatabase()
    : m_privateBrowsingEnabled(false)
{
    close();
}

bool IconDatabase::open(const String& databasePath)
{
    close();
    String dbFilename = databasePath + DefaultIconDatabaseFilename;
    if (!m_db.open(dbFilename)) {
        LOG(IconDatabase, "Unable to open icon database at path %s", dbFilename.ascii().data());
        return false;
    }
    
    if (!isValidDatabase()) {
        LOG(IconDatabase, "%s is missing or in an invalid state - reconstructing", dbFilename.ascii().data());
        clearDatabase();
        recreateDatabase();
    }
    
    m_db.setFullsync(false);
    return isOpen();
}

void IconDatabase::close()
{
    //TODO - sync any cached info before m_db.close();
    m_db.close();
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
}

void IconDatabase::recreateDatabase()
{
    if (!m_db.executeCommand("CREATE TABLE IconDatabaseInfo (key varchar NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE PageURL (url varchar NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID integer NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE Icon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url NOT NULL UNIQUE ON CONFLICT FAIL, expires INTEGER);")) {
        LOG_ERROR("Could not create Icon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE IconResource (iconID integer NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data blob, touch);")) {
        LOG_ERROR("Could not create IconResource table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TRIGGER create_icon_resource AFTER INSERT ON Icon BEGIN INSERT INTO IconResource (iconID, data) VALUES (new.iconID, NULL); END;")) {
        LOG_ERROR("Unable to create create_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
}    

void IconDatabase::createPrivateTables()
{
    if (!m_db.executeCommand("CREATE TEMP TABLE TempPageURL (url varchar NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID integer NOT NULL ON CONFLICT FAIL);")) 
        LOG_ERROR("Could not create TempPageURL table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIcon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url NOT NULL UNIQUE ON CONFLICT FAIL, expires INTEGER);")) 
        LOG_ERROR("Could not create TempIcon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIconResource (iconID integer NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data blob, touch);")) 
        LOG_ERROR("Could not create TempIconResource table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TRIGGER temp_create_icon_resource AFTER INSERT ON TempIcon BEGIN INSERT INTO TempIconResource (iconID, data) VALUES (new.iconID, NULL); END;")) 
        LOG_ERROR("Unable to create temp_create_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
}

void IconDatabase::deletePrivateTables()
{
    if (!m_db.executeCommand("DROP TEMP TABLE TempPageURL;"))
        LOG_ERROR("Could not drop TempPageURL table");
    if (!m_db.executeCommand("DROP TEMP TABLE TempIcon;"))
        LOG_ERROR("Could not drop TempIcon table");
    if (!m_db.executeCommand("DROP TEMP TABLE TempIconResource;"))
        LOG_ERROR("Could not drop TempIconResource table");
}

const void* IconDatabase::imageDataForIconID(int id, int& size)
{
    char* query = sqlite3_mprintf("SELECT data FROM IconResource WHERE iconid = %i", id);
    SQLStatement sql(m_db, String(query));
    sql.prepare();
    sqlite3_free(query);
    if (sql.step() != SQLITE_ROW) {
        size = 0;
        return 0;
    }
    const void* blob = sql.getColumnBlob(0, size);
    if (!blob) {
        size = 0;
        return 0;
    }
    return blob;
}

const void* IconDatabase::imageDataForIconURL(const String& _iconURL, int& size)
{
    //Escape single quotes for SQL 
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    const void* blob = 0;
    
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {
        blob = SQLStatement(m_db, "SELECT TempIconResource.data FROM TempIconResource, TempIcon WHERE TempIcon.url = '" + iconURL + "' AND TempIconResource.iconID = TempIcon.iconID;").getColumnBlob(0,size);
        if (blob)
            return blob;
    }
    
    // It wasn't found there, so lets check the main tables
    blob = SQLStatement(m_db, "SELECT IconResource.data FROM IconResource, Icon WHERE Icon.url = '" + iconURL + "' AND IconResource.iconID = Icon.iconID;").getColumnBlob(0, size);
    if (blob) 
        return blob;
    size = 0;
    return 0;
}

const void* IconDatabase::imageDataForPageURL(const String& _pageURL, int& size)
{
    //Escape single quotes for SQL 
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    
    const void* blob = 0;
    
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {
        blob = SQLStatement(m_db, "SELECT TempIconResource.data FROM TempIconResource, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIconResource.iconID = TempPageURL.iconID;").getColumnBlob(0,size);
        if (blob)
            return blob;
    }
    
    // It wasn't found there, so lets check the main tables
    blob = SQLStatement(m_db, "SELECT IconResource.data FROM IconResource, PageURL WHERE PageURL.url = '" + pageURL + "' AND IconResource.iconID = PageURL.iconID;").getColumnBlob(0, size);
    if (blob) 
        return blob;
    size = 0;
    return 0;
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
    // FIXME - Private browsing

    // We may have a SiteIcon for this specific URL...
    if (m_pageURLToSiteIcons.contains(url))
        return m_pageURLToSiteIcons.get(url)->getImage(size);
    
    // ...and we may have one for the IconURL this URL maps to
    String iconURL = iconURLForURL(url);
    if (m_iconURLToSiteIcons.contains(iconURL))
        return m_iconURLToSiteIcons.get(iconURL)->getImage(size);
        
    // If we don't have either, we have to create the SiteIcon
    if (!iconURL.isEmpty()) {
        SiteIcon* icon = new SiteIcon(iconURL);
        m_pageURLToSiteIcons.set(url, icon);
        m_iconURLToSiteIcons.set(iconURL, icon);
        return icon->getImage(size);
    }
    
    return 0;
}

String IconDatabase::iconURLForURL(const String& _url)
{
    String pageURL = _url;
    pageURL.replace('\'', "''");

    return SQLStatement(m_db, "SELECT Icon.url FROM Icon, PageURL WHERE PageURL.url = '" + pageURL + "' AND Icon.iconID = PageURL.iconID").getColumnText16(0);
}

Image* IconDatabase::defaultIcon(const IntSize& size)
{
    return 0;
}

void IconDatabase::retainIconForURL(const String& url)
{

}

void IconDatabase::releaseIconForURL(const String& url)
{

}

void IconDatabase::setIconDataForIconURL(const void* data, int size, const String& _iconURL)
{
    ASSERT(size > -1);
    if (size)
        ASSERT(data);
    else
        data = 0;
        
    if (_iconURL.length() < 1) {
        LOG_ERROR("Attempt to set icon for blank url");
        return;
    }
    
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");

    // If we're in private browsing, we'll keep a record in cache instead of in the DB
    if (m_privateBrowsingEnabled) {
        // FIXME - set data in the temporary tables
        return;
    }
    
    int64_t iconID = establishIconIDForEscapedIconURL(iconURL);
    if (iconID) {
        // The following statement works to set the icon data to NULL because sqlite defaults unbound ? parameters to null
        SQLStatement sql(m_db, "UPDATE IconResource SET data = ? WHERE iconID = ?;");
        sql.prepare();
        if (data)
            sql.bindBlob(1, data, size);
        sql.bindInt64(2, iconID);
        if (sql.step() != SQLITE_DONE)
            LOG_ERROR("Unable to set icon resource data for iconID %i", iconID);
    }

}

int IconDatabase::establishIconIDForEscapedIconURL(const String& iconURL)
{
    int64_t iconID = 0;
    SQLStatement sql(m_db, "SELECT iconID FROM Icon WHERE url = '" + iconURL + "';");
    sql.prepare();    
    if (sql.step() == SQLITE_ROW) {
        iconID = sql.getColumnInt64(0);
    } else {
        sql.finalize();
        if (m_db.executeCommand("INSERT INTO Icon (url) VALUES ('" + iconURL + "');"))
            iconID = m_db.lastInsertRowID();
    }
    return iconID;
}

void IconDatabase::setHaveNoIconForIconURL(const String& _iconURL)
{    
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    
    // If we're in private browsing, we'll keep a record in cache instead of in the DB
    if (m_privateBrowsingEnabled) {
        // FIXME - set data in the temporary tables
        return;
    }

    int64_t iconID = establishIconIDForEscapedIconURL(iconURL);
    if (iconID) {
        // The following statement works to set the icon data to NULL because sqlite defaults unbound ? parameters to null
        SQLStatement sql(m_db, "UPDATE IconResource SET data = ? WHERE iconID = ?;");
        sql.prepare();
        sql.bindInt64(2, iconID);
        if (sql.step() != SQLITE_DONE)
            LOG_ERROR("Unable to set icon resource data for iconID %i", iconID);
    }
}

void IconDatabase::setIconURLForPageURL(const String& _iconURL, const String& _pageURL)
{
    String iconURL = _iconURL;
    iconURL.replace('\'',"''");
    String pageURL = _pageURL;
    pageURL.replace('\'',"''");
     
    // If we're in private browsing, we'll keep a record in a temporary table
    if (m_privateBrowsingEnabled) {
        // FIXME - set data in the temporary tables
        return;
    }
     
    // Check if an entry in the Icon table already exists for this iconURL
    // Get this iconID, or create a new one
    int64_t iconID = 0;
    SQLStatement sql(m_db, "SELECT iconID FROM Icon WHERE url = '" + iconURL + "';");
    sql.prepare();    
    if (sql.step() == SQLITE_ROW) {
        iconID = sql.getColumnInt64(0);
        sql.finalize();
    } else {
        sql.finalize();
        if (m_db.executeCommand("INSERT INTO Icon (url) VALUES ('" + iconURL + "');"))
            iconID = m_db.lastInsertRowID();
    }
    
    if (!iconID) {
        LOG_ERROR("Unable to establish iconID for iconURL %s - %s", iconURL.ascii().data(), m_db.lastErrorMsg());
        return;
    }
    
    if (m_db.returnsAtLeastOneResult("SELECT url FROM PageURL WHERE url = '" + pageURL + "';")) {
        if (!m_db.executeCommand("UPDATE PageURL SET iconID = " + String::number(iconID) + " WHERE url = '" + pageURL + "';"))
            LOG_ERROR("Failed to update record in PageURL table - %s", m_db.lastErrorMsg());
    } else {
        if( !m_db.executeCommand("INSERT INTO PageURL (url, iconID) VALUES ('" + pageURL + "', " + String::number(iconID) + ");"))
            LOG_ERROR("Failed to insert record into PageURL - %s", m_db.lastErrorMsg());
    }
}

bool IconDatabase::hasIconForIconURL(const String& _url)
{
    // Places to check -
    // -iconURLToSiteIcon map
    // -On disk SQL
    // -Temporary SQL if private browsing enabled

    String url = _url;
    url.replace('\'', "''");
    
    // If we're in private browsing, we'll check the temporary table as a backup
    if (m_privateBrowsingEnabled) {
        // FIXME - make this AFTER the primary checks as we can probably safely consider private browsing to be less common
        // than regular
        return false;
    }
    
    String query = "SELECT IconResource.data FROM IconResource, Icon WHERE Icon.url = '" + url + "' AND IconResource.iconID = Icon.iconID;";
    int size;
    const void* data = SQLStatement(m_db, query).getColumnBlob(0, size);
    return (data && size);
}

IconDatabase::~IconDatabase()
{
    m_db.close();
}

} //namespace WebCore