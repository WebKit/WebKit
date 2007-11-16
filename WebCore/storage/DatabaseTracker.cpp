/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "DatabaseTracker.h"

#include "Database.h"
#include "FileSystem.h"
#include "NotImplemented.h"
#include "SecurityOriginData.h"
#include "SQLiteStatement.h"

namespace WebCore {

struct SecurityOriginDataHash {
    static unsigned hash(const SecurityOriginData& data)
    {
        unsigned hashCodes[3] = {
            data.protocol().impl()->hash(),
            data.host().impl()->hash(),
            data.port()
        };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 3 * sizeof(unsigned) / sizeof(UChar));
    }
         
    static bool equal(const SecurityOriginData& a, const SecurityOriginData& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct SecurityOriginDataTraits : WTF::GenericHashTraits<SecurityOriginData> {
    static const SecurityOriginData& deletedValue()
    {
        // Okay deleted value because file: protocols should always have port 0
        static SecurityOriginData key("file", "", 1);
        return key;
    }
    static const SecurityOriginData& emptyValue()
    {
        // Okay empty value because file: protocols should always have port 0
        static SecurityOriginData key("file", "", 2);
        return key;
    }
};

DatabaseTracker& DatabaseTracker::tracker()
{
    static DatabaseTracker tracker;

    return tracker;
}

DatabaseTracker::DatabaseTracker()
{
}

void DatabaseTracker::setDatabasePath(const String& path)
{        
    m_databasePath = path;
    openTrackerDatabase();
}

const String& DatabaseTracker::databasePath()
{
    return m_databasePath;
}

void DatabaseTracker::openTrackerDatabase()
{
    ASSERT(!m_database.isOpen());
    
    makeAllDirectories(m_databasePath);
    String databasePath = pathByAppendingComponent(m_databasePath, "Databases.db");

    if (!m_database.open(databasePath)) {
        // FIXME: What do do here?
        return;
    }
    if (!m_database.tableExists("Origins")) {
        if (!m_database.executeCommand("CREATE TABLE Origins (origin TEXT UNIQUE ON CONFLICT REPLACE, creationPolicy INTEGER NOT NULL ON CONFLICT FAIL, sizePolicy INTEGER NOT NULL ON CONFLICT FAIL);")) {
            // FIXME: and here
        }
    }

    if (!m_database.tableExists("Databases")) {
        if (!m_database.executeCommand("CREATE TABLE Databases (guid INTEGER PRIMARY KEY AUTOINCREMENT, origin TEXT UNIQUE ON CONFLICT REPLACE, name TEXT UNIQUE ON CONFLICT REPLACE, path TEXT NOT NULL ON CONFLICT FAIL);")) {
            // FIXME: and here
        }
    }
}
    
String DatabaseTracker::fullPathForDatabase(const SecurityOriginData& origin, const String& name)
{
    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return "";

    statement.bindText(1, origin.stringIdentifier());
    statement.bindText(2, name);

    int result = statement.step();

    if (result == SQLResultRow)
        return pathByAppendingComponent(m_databasePath, statement.getColumnText16(0));
    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve filename from Database Tracker for origin %s, name %s", origin.stringIdentifier().ascii().data(), name.ascii().data());
        return "";
    }

    SQLiteStatement sequenceStatement(m_database, "SELECT seq FROM sqlite_sequence WHERE name='Databases';");

    // FIXME: More informative error handling here, even though these steps should never fail
    if (sequenceStatement.prepare() != SQLResultOk)
        return "";
    result = sequenceStatement.step();

    // This has a range of 2^63 and starts at 0 for every time a user resets Safari -
    // I can't imagine it'd over overflow
    int64_t seq = 0;
    if (result == SQLResultRow) {
        seq = sequenceStatement.getColumnInt64(0);
    } else if (result != SQLResultDone)
        return "";

    String filename;
    do {
        ++seq;
        filename = pathByAppendingComponent(m_databasePath, String::format("%016llx.db", seq));
    } while (fileExists(filename));

    sequenceStatement.finalize();

    if (!addDatabase(origin, name, String::format("%016llx.db", seq)))
        return "";

    return filename;
}

void DatabaseTracker::populateOrigins()
{
    if (m_origins)
        return;

    m_origins.set(new HashSet<SecurityOriginData, SecurityOriginDataHash, SecurityOriginDataTraits>);

    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT DISTINCT origin FROM Databases;");

    if (statement.prepare() != SQLResultOk)
        return;

    int result;
    while ((result = statement.step()) == SQLResultRow)
        m_origins->add(statement.getColumnText16(0));

    if (result != SQLResultDone)
        LOG_ERROR("Failed to read in all origins from the database");

    return;
}

void DatabaseTracker::origins(Vector<SecurityOriginData>& result)
{
    if (!m_origins)
        populateOrigins();

    copyToVector(*(m_origins.get()), result);
}

bool DatabaseTracker::databaseNamesForOrigin(const SecurityOriginData& origin, Vector<String>& resultVector)
{
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT name FROM Databases where origin=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin.stringIdentifier());

    int result;
    while ((result = statement.step()) == SQLResultRow)
        resultVector.append(statement.getColumnText16(0));

    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve all database names for origin %s", origin.stringIdentifier().ascii().data());
        return false;
    }

    return true;
}

bool DatabaseTracker::addDatabase(const SecurityOriginData& origin, const String& name, const String& path)
{
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "INSERT INTO Databases (origin, name, path) VALUES (?, ?, ?);");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin.stringIdentifier());
    statement.bindText(2, name);
    statement.bindText(3, path);

    if (!statement.executeCommand()) {
        LOG_ERROR("Failed to add database %s to origin %s: %s\n", name.ascii().data(), origin.stringIdentifier().ascii().data(), statement.lastErrorMsg());
        return false;
    }

    populateOrigins();
    m_origins->add(origin);
    return true;
}

void DatabaseTracker::deleteAllDatabases()
{
    notImplemented();
}

void DatabaseTracker::deleteDatabasesWithOrigin(const SecurityOriginData& origin)
{
    notImplemented();
}

void DatabaseTracker::deleteDatabase(const SecurityOriginData& origin, const String& name)
{
    notImplemented();
}

void DatabaseTracker::setClient(DatabaseTrackerClient* client)
{
    m_client = client;
}

} // namespace WebCore
