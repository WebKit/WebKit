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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>



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
    if (!m_db.executeCommand("CREATE TABLE Icon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL UNIQUE ON CONFLICT FAIL, expires INTEGER);")) {
        LOG_ERROR("Could not create Icon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE IconResource (iconID integer NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data BLOB, touch INTEGER);")) {
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
    if (!m_db.executeCommand("CREATE TEMP TABLE TempPageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) 
        LOG_ERROR("Could not create TempPageURL table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIcon (iconID INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL UNIQUE ON CONFLICT FAIL, expires INTEGER);")) 
        LOG_ERROR("Could not create TempIcon table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TABLE TempIconResource (iconID INTERGER NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data BLOB, touch INTEGER);")) 
        LOG_ERROR("Could not create TempIconResource table in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());

    if (!m_db.executeCommand("CREATE TEMP TRIGGER temp_create_icon_resource AFTER INSERT ON TempIcon BEGIN INSERT INTO TempIconResource (iconID, data) VALUES (new.iconID, NULL); END;")) 
        LOG_ERROR("Unable to create temp_create_icon_resource trigger in icon.db (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
}

void IconDatabase::deletePrivateTables()
{
    if (!m_db.executeCommand("DROP TABLE TempPageURL;"))
        LOG_ERROR("Could not drop TempPageURL table - %s", m_db.lastErrorMsg());
    if (!m_db.executeCommand("DROP TABLE TempIcon;"))
        LOG_ERROR("Could not drop TempIcon table - %s", m_db.lastErrorMsg());
    if (!m_db.executeCommand("DROP TABLE TempIconResource;"))
        LOG_ERROR("Could not drop TempIconResource table - %s", m_db.lastErrorMsg());
}

// FIXME - This is a DIRTY, dirty workaround for a problem that we're seeing where certain blobs are having a corrupt buffer
// returned when we get the result as a const void* blob.  Getting the blob as a textual representation is 100% accurate so this hack
// does an in place hex-to-character from the textual representation of the icon data.  After I manage to follow up with Adam Swift, the OSX sqlite maintainer,
// who is too busy to help me until after 7-4-06, this NEEEEEEEEEEEEEEDS to be changed. 
// *SIGH*
unsigned char hexToUnsignedChar(UChar h, UChar l)
{
    unsigned char c;
    if (h >= '0' && h <= '9')
        c = h - '0';
    else if (h >= 'A' && h <= 'F')
        c = h - 'A' + 10;
    else {
        LOG_ERROR("Failed to parse TEXT result from SQL BLOB query");
        return 0;
    }
    c *= 16;
    if (l >= '0' && l <= '9')
        c += l - '0';
    else if (l >= 'A' && l <= 'F')
        c += l - 'A' + 10;
    else {
        LOG_ERROR("Failed to parse TEXT result from SQL BLOB query");
        return 0;
    }    
    return c;
}

Vector<unsigned char> hexStringToVector(const String& s)
{
    LOG(IconDatabase, "hexStringToVector() - s.length is %i", s.length());
    if (s[0] != 'X' || s[1] != '\'') {
        LOG(IconDatabase, "hexStringToVector() - string is invalid SQL HEX-string result - %s", s.ascii().data());
        return Vector<unsigned char>();
    }

    Vector<unsigned char> result;
    result.reserveCapacity(s.length() / 2);
    const UChar* data = s.characters() + 2;
    int count = 0;
    while (data[0] != '\'') {
        if (data[1] == '\'') {
            LOG_ERROR("Invalid HEX TEXT data for BLOB query");
            return Vector<unsigned char>();
        }
        result.append(hexToUnsignedChar(data[0], data[1]));
        data++;
        data++;
        count++;
    }
    
    LOG(IconDatabase, "Finished atoi() - %i iterations, result size %i", count, result.size());
    return result;
}

Vector<unsigned char> IconDatabase::imageDataForIconID(int id)
{
    String blob = SQLStatement(m_db, String::sprintf("SELECT data FROM IconResource WHERE iconid = %i", id)).getColumnText(0);
    if (blob.isEmpty())
        return Vector<unsigned char>();
    return hexStringToVector(blob);
}

Vector<unsigned char> IconDatabase::imageDataForIconURL(const String& _iconURL)
{
    //Escape single quotes for SQL 
    String iconURL = _iconURL;
    iconURL.replace('\'', "''");
    String blob;
    
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {
        blob = SQLStatement(m_db, "SELECT quote(TempIconResource.data) FROM TempIconResource, TempIcon WHERE TempIcon.url = '" + iconURL + "' AND TempIconResource.iconID = TempIcon.iconID;").getColumnText(0);
        if (!blob.isEmpty()) {
            LOG(IconDatabase, "Icon data pulled from temp tables");
            return hexStringToVector(blob);
        }
    } 
    
    // It wasn't found there, so lets check the main tables
    blob = SQLStatement(m_db, "SELECT quote(IconResource.data) FROM IconResource, Icon WHERE Icon.url = '" + iconURL + "' AND IconResource.iconID = Icon.iconID;").getColumnText(0);
    if (blob.isEmpty())
        return Vector<unsigned char>();
    
    return hexStringToVector(blob);
}

Vector<unsigned char> IconDatabase::imageDataForPageURL(const String& _pageURL)
{
    //Escape single quotes for SQL 
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    String blob;
    
    // If private browsing is enabled, we'll check there first as the most up-to-date data for an icon will be there
    if (m_privateBrowsingEnabled) {
        blob = SQLStatement(m_db, "SELECT TempIconResource.data FROM TempIconResource, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIconResource.iconID = TempPageURL.iconID;").getColumnText(0);
        if (!blob.isEmpty()) {
            LOG(IconDatabase, "Icon data pulled from temp tables");
            return hexStringToVector(blob);
        }
    } 
    
    // It wasn't found there, so lets check the main tables
    blob = SQLStatement(m_db, "SELECT quote(IconResource.data) FROM IconResource, PageURL WHERE PageURL.url = '" + pageURL + "' AND IconResource.iconID = PageURL.iconID;").getColumnText(0);
    if (blob.isEmpty())
        return Vector<unsigned char>();
    
    return hexStringToVector(blob);
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
    // We may have a SiteIcon for this specific PageURL...
    if (m_pageURLToSiteIcons.contains(url))
        return m_pageURLToSiteIcons.get(url)->getImage(size);
    
    // Otherwise see if we even have an IconURL for this PageURL
    String iconURL = iconURLForPageURL(url);
    if (iconURL.isEmpty())
        return 0;
    
    // If we do, maybe we have an image for this IconURL
    if (m_iconURLToSiteIcons.contains(iconURL))
        return m_iconURLToSiteIcons.get(iconURL)->getImage(size);
        
    // If we don't have either, we have to create the SiteIcon
    SiteIcon* icon = new SiteIcon(iconURL);
    m_pageURLToSiteIcons.set(url, icon);
    m_iconURLToSiteIcons.set(iconURL, icon);
    return icon->getImage(size);
}

String IconDatabase::iconURLForPageURL(const String& _pageURL)
{
    if (_pageURL.isEmpty()) 
        return String();
        
    String pageURL = _pageURL;
    pageURL.replace('\'', "''");
    
    // Try the private browsing tables because if any PageURL's IconURL was updated during privated browsing, it would be here
    if (m_privateBrowsingEnabled) {
        String iconURL = SQLStatement(m_db, "SELECT TempIcon.url FROM TempIcon, TempPageURL WHERE TempPageURL.url = '" + pageURL + "' AND TempIcon.iconID = TempPageURL.iconID").getColumnText16(0);
        if (!iconURL.isEmpty())
            return iconURL;
    }
    
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
        
    if (_iconURL.isEmpty()) {
        LOG_ERROR("Attempt to set icon for blank url");
        return;
    }
    
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

int IconDatabase::establishTemporaryIconIDForEscapedIconURL(const String& iconURL)
{
    // We either lookup the iconURL and return its ID, or we create a new one for it
    int64_t iconID = 0;
    SQLStatement sql(m_db, "SELECT iconID FROM TempIcon WHERE url = '" + iconURL + "';");
    sql.prepare();    
    if (sql.step() == SQLITE_ROW) {
        iconID = sql.getColumnInt64(0);
    } else {
        sql.finalize();
        if (m_db.executeCommand("INSERT INTO TempIcon (url) VALUES ('" + iconURL + "');"))
            iconID = m_db.lastInsertRowID();
    }
    return iconID;
}

int IconDatabase::establishIconIDForEscapedIconURL(const String& iconURL)
{
    // We either lookup the iconURL and return its ID, or we create a new one for it
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
    setIconDataForIconURL(0, 0, _iconURL);
}

void IconDatabase::setIconURLForPageURL(const String& _iconURL, const String& _pageURL)
{
    ASSERT(!_iconURL.isEmpty());
    ASSERT(!_pageURL.isEmpty());
    
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