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

#include "DeprecatedString.h"
#include "Logging.h"
#include "PlatformString.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

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
{

}

bool IconDatabase::open(const String& databasePath)
{
    close();
    String dbFilename = databasePath + DefaultIconDatabaseFilename;
    if (!m_db.open(dbFilename)) {
        LOG(IconDatabase, "Unable to open icon database at path %s", dbFilename.deprecatedString().ascii());
        return false;
    }
    
    if (!isValidDatabase()) {
        LOG(IconDatabase, "%s is in an invalid state - reconstructing", dbFilename.deprecatedString().ascii());
        clearDatabase();
        recreateDatabase();
    }
    
    return isOpen();
}

void IconDatabase::close()
{
    //TODO - sync any cached info before close();
    m_db.close();
}


bool IconDatabase::isValidDatabase()
{
    if (!m_db.tableExists("IconDatabaseInfo")) {
        return false;
    }
    
    String query = "SELECT value FROM IconDatabaseInfo WHERE key = 'Version';";
    SQLStatement sql(m_db, query);
    sql.prepare();
    sql.step();
    if (sql.getColumnInt(0) < currentDatabaseVersion) {
        LOG(IconDatabase, "DB version is not found or below expected valid version");
        return false;
    }
    
    if (!m_db.tableExists("Icon") || !m_db.tableExists("PageURL") || !m_db.tableExists("IconResource")) {
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
            LOG(IconDatabase, "Unable to drop table %s", (*table).deprecatedString().ascii());
        }
    }
}

void IconDatabase::recreateDatabase()
{
    if (!m_db.executeCommand("CREATE TABLE IconDatabaseInfo (key varchar NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value integer NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in icon.db (%i)\n%s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i)\n%s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE PageURL (url varchar NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID integer NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in icon.db (%i)\n%s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE Icon (id integer PRIMARY KEY ON CONFLICT FAIL,url varchar NOT NULL UNIQUE ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create Icon table in icon.db (%i)\n%s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    if (!m_db.executeCommand("CREATE TABLE IconResource (iconID integer NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,data blob NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconResource table in icon.db (%i)\n%s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return;
    }
    
    
}    

IconDatabase::~IconDatabase()
{
    m_db.close();
}

} //namespace WebCore

