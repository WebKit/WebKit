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

const char* DefaultIconDatabaseFilename = "/icon.db";

namespace WebCore {

IconDatabase* IconDatabase::m_sharedInstance = 0;

IconDatabase* IconDatabase::sharedIconDatabase()
{
    if (!m_sharedInstance) {
        m_sharedInstance = new IconDatabase();
    }
    return m_sharedInstance;
}

IconDatabase::IconDatabase() : m_db(0)
{

}

bool IconDatabase::open(const String& databasePath)
{
    close();
    
    //Make sure directory exists
    DeprecatedString ds = databasePath.deprecatedString();
    const char* path = ds.ascii();
    
    struct stat dirInfo;
    if (stat(path, &dirInfo)) {
        LOG_ERROR("Unable to stat icon database path %s (%i)", path, errno);
        return false;
    }
    if (dirInfo.st_mode & S_IRWXU != S_IRWXU) {
        LOG_ERROR("Unable to access icon database path %s", path);
        return false;
    }
        
    String dbFilename = databasePath + DefaultIconDatabaseFilename;
    //specifically include the null-terminator as sqlite3 expects it on unicode-16 strings
    dbFilename.append('\0');
    
    int result = sqlite3_open16(dbFilename.characters(), &m_db);
    if (result != SQLITE_OK) {
        LOG_ERROR("SQLite database failed to load from %s\nCause - %s", dbFilename.ascii(),
            sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = 0;
    }
    return m_db;
}

void IconDatabase::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = 0;
    }
}

IconDatabase::~IconDatabase()
{
    if (!m_db)
        return;
        
    int result = sqlite3_close(m_db);
    if (result != SQLITE_OK)
        LOG_ERROR("IconDatabase SQLite database failed to close\n%s", sqlite3_errmsg(m_db));
}

} //namespace WebCore

