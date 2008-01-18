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
#include "DatabaseTrackerClient.h"
#include "Document.h"
#include "FileSystem.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SQLiteStatement.h"

namespace WebCore {

// HTML5 SQL Storage spec suggests 5MB as the default quota per origin
static const unsigned DefaultOriginQuota = 5242880;

struct SecurityOriginHash {
    static unsigned hash(RefPtr<SecurityOrigin> origin)
    {
        unsigned hashCodes[3] = {
            origin->protocol().impl() ? origin->protocol().impl()->hash() : 0,
            origin->host().impl() ? origin->host().impl()->hash() : 0,
            origin->port()
        };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 3 * sizeof(unsigned) / sizeof(UChar));
    }
         
    static bool equal(RefPtr<SecurityOrigin> a, RefPtr<SecurityOrigin> b)
    {
        if (a == 0 || b == 0)
            return a == b;
        return a->equal(b.get());
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};


struct SecurityOriginTraits : WTF::GenericHashTraits<RefPtr<SecurityOrigin> > {
    static const bool emptyValueIsZero = true;
    static const RefPtr<SecurityOrigin>& deletedValue() 
    { 
        // Okay deleted value because file: protocols should always have port 0
        static const RefPtr<SecurityOrigin> securityOriginDeletedValue = SecurityOrigin::create("file", "", 1, 0);    
        return securityOriginDeletedValue; 
    }

    static SecurityOrigin* emptyValue() 
    { 
        return 0;
    }
};

DatabaseTracker& DatabaseTracker::tracker()
{
    static DatabaseTracker tracker;

    return tracker;
}

DatabaseTracker::DatabaseTracker()
    : m_defaultQuota(DefaultOriginQuota)
    , m_client(0)
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
        if (!m_database.executeCommand("CREATE TABLE Origins (origin TEXT UNIQUE ON CONFLICT REPLACE, quota INTEGER NOT NULL ON CONFLICT FAIL);")) {
            // FIXME: and here
        }
    }

    if (!m_database.tableExists("Databases")) {
        if (!m_database.executeCommand("CREATE TABLE Databases (guid INTEGER PRIMARY KEY AUTOINCREMENT, origin TEXT, name TEXT, displayName TEXT, estimatedSize INTEGER, path TEXT);")) {
            // FIXME: and here
        }
    }
}

bool DatabaseTracker::canEstablishDatabase(Document* document, const String& name, const String& displayName, unsigned long estimatedSize)
{
    SecurityOrigin* origin = document->securityOrigin();
    
    // If this origin has no databases yet, establish an entry in the tracker database with the default quota
    if (!hasEntryForOrigin(origin))
        establishEntryForOrigin(origin);
    
    // If a database already exists, you can always establish a handle to it
    if (hasEntryForDatabase(origin, name))
        return true;
        
    // If the new database will fit as-is, allow its creation
    unsigned long long usage = usageForOrigin(origin);
    if (usage + estimatedSize < quotaForOrigin(origin))
        return true;
    
    // Otherwise, ask the UI Delegate for a new quota
    Page* page;
    if (!(page = document->page()))
        return false;
    
    // If no displayName was specified, pass the standard (required) name instead
    unsigned long long newQuota = page->chrome()->requestQuotaIncreaseForNewDatabase(document->frame(), origin, displayName.length() > 0 ? displayName : name, estimatedSize);
    setQuota(origin, newQuota);
    
    return usage + estimatedSize <= newQuota;
}

bool DatabaseTracker::hasEntryForOrigin(SecurityOrigin* origin)
{
    populateOrigins();
    return m_originQuotaMap->contains(origin);
}

bool DatabaseTracker::hasEntryForDatabase(SecurityOrigin* origin, const String& databaseIdentifier)
{
    SQLiteStatement statement(m_database, "SELECT guid FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->stringIdentifier());
    statement.bindText(2, databaseIdentifier);

    return statement.step() == SQLResultRow;
}

void DatabaseTracker::establishEntryForOrigin(SecurityOrigin* origin)
{
    ASSERT(!hasEntryForOrigin(origin));
    
    SQLiteStatement statement(m_database, "INSERT INTO Origins VALUES (?, ?)");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to establish origin %s in the tracker", origin->stringIdentifier().ascii().data());
        return;
    }
        
    statement.bindText(1, origin->stringIdentifier());
    statement.bindInt64(2, m_defaultQuota);
    
    if (statement.step() != SQLResultDone) {
        LOG_ERROR("Unable to establish origin %s in the tracker", origin->stringIdentifier().ascii().data());
        return;
    }

    populateOrigins();
    m_originQuotaMap->set(origin, m_defaultQuota);
    
    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    String originIdentifier = origin->stringIdentifier();
    String originPath = pathByAppendingComponent(m_databasePath, originIdentifier);
    
    // Make sure the path for this SecurityOrigin exists
    if (createIfNotExists && !makeAllDirectories(originPath))
        return "";
    
    // See if we have a path for this database yet
    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return "";

    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);

    int result = statement.step();

    if (result == SQLResultRow)
        return pathByAppendingComponent(originPath, statement.getColumnText16(0));
    if (!createIfNotExists)
        return "";
        
    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve filename from Database Tracker for origin %s, name %s", origin->stringIdentifier().ascii().data(), name.ascii().data());
        return "";
    }
    statement.finalize();
    
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
    sequenceStatement.finalize();

    String filename;
    do {
        ++seq;
        filename = pathByAppendingComponent(originPath, String::format("%016llx.db", seq));
    } while (fileExists(filename));

    if (!addDatabase(origin, name, String::format("%016llx.db", seq)))
        return "";

    return filename;
}

void DatabaseTracker::populateOrigins()
{
    if (m_originQuotaMap)
        return;

    m_originQuotaMap.set(new HashMap<RefPtr<SecurityOrigin>, unsigned long long, SecurityOriginHash, SecurityOriginTraits>);

    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT origin, quota FROM Origins");

    if (statement.prepare() != SQLResultOk)
        return;

    int result;
    while ((result = statement.step()) == SQLResultRow) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromIdentifier(statement.getColumnText16(0));
        m_originQuotaMap->set(origin.get(), statement.getColumnInt64(1));
    }

    if (result != SQLResultDone)
        LOG_ERROR("Failed to read in all origins from the database");

    return;
}

void DatabaseTracker::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    if (!m_originQuotaMap)
        populateOrigins();

    copyKeysToVector(*(m_originQuotaMap.get()), result);
}

bool DatabaseTracker::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& resultVector)
{
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT name FROM Databases where origin=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->stringIdentifier());

    int result;
    while ((result = statement.step()) == SQLResultRow)
        resultVector.append(statement.getColumnText16(0));

    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve all database names for origin %s", origin->stringIdentifier().ascii().data());
        return false;
    }

    return true;
}

DatabaseDetails DatabaseTracker::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    String originIdentifier = origin->stringIdentifier();
        
    SQLiteStatement statement(m_database, "SELECT displayName, estimatedSize FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLResultOk)
        return DatabaseDetails();
   
    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);
    
    int result = statement.step();
    if (result == SQLResultDone)
        return DatabaseDetails();
    
    if (result != SQLResultRow) {
        LOG_ERROR("Error retrieving details for database %s in origin %s from tracker database", name.ascii().data(), originIdentifier.ascii().data());
        return DatabaseDetails();
    }
    
    return DatabaseDetails(name, statement.getColumnText(0), statement.getColumnInt64(1), usageForDatabase(name, origin));
}

void DatabaseTracker::setDatabaseDetails(SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
{
    String originIdentifier = origin->stringIdentifier();
    int64_t guid = 0;
    
    SQLiteStatement statement(m_database, "SELECT guid FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLResultOk)
        return;
        
    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);
    
    int result = statement.step();
    if (result == SQLResultRow)
        guid = statement.getColumnInt64(0);
    statement.finalize();

    if (guid == 0) {
        if (result != SQLResultDone)
            LOG_ERROR("Error to determing existence of database %s in origin %s in tracker database", name.ascii().data(), originIdentifier.ascii().data());
        else {
            // This case should never occur - we should never be setting database details for a database that doesn't already exist in the tracker
            // But since the tracker file is an external resource not under complete control of our code, it's somewhat invalid to make this an ASSERT case
            // So we'll print an error instead
            LOG_ERROR("Could not retrieve guid for database %s in origin %s from the tracker database - it is invalid to set database details on a database that doesn't already exist in the tracker",
                       name.ascii().data(), originIdentifier.ascii().data());
        }
        return;
    }
    
    SQLiteStatement updateStatement(m_database, "UPDATE Databases SET displayName=?, estimatedSize=? WHERE guid=?");
    if (updateStatement.prepare() != SQLResultOk)
        return;
    
    updateStatement.bindText(1, displayName);
    updateStatement.bindInt64(2, estimatedSize);
    updateStatement.bindInt64(3, guid);
    
    if (updateStatement.step() != SQLResultDone) {
        LOG_ERROR("Failed to update details for database %s in origin %s", name.ascii().data(), originIdentifier.ascii().data());
        return;  
    }
    
    if (m_client)
        m_client->dispatchDidModifyDatabase(origin, name);
}

unsigned long long DatabaseTracker::usageForDatabase(const String& name, SecurityOrigin* origin)
{
    String path = fullPathForDatabase(origin, name, false);
    if (path.isEmpty())
        return 0;
        
    long long size;
    return fileSize(path, size) ? size : 0;
}

unsigned long long DatabaseTracker::usageForOrigin(SecurityOrigin* origin)
{
    Vector<String> names;
    databaseNamesForOrigin(origin, names);
    
    unsigned long long result = 0;
    for (unsigned i = 0; i < names.size(); ++i)
        result += usageForDatabase(names[i], origin);
        
    return result;
}

unsigned long long DatabaseTracker::quotaForOrigin(SecurityOrigin* origin)
{
    populateOrigins();
    return m_originQuotaMap->get(origin);
}

void DatabaseTracker::setQuota(SecurityOrigin* origin, unsigned long long quota)
{
    populateOrigins();
    if (!m_originQuotaMap->contains(origin))
        establishEntryForOrigin(origin);
    
    m_originQuotaMap->set(origin, quota);
    
    SQLiteStatement statement(m_database, "UPDATE Origins SET quota=? WHERE origin=?");
    
    bool error = statement.prepare() != SQLResultOk;
    if (!error) {
        statement.bindInt64(1, quota);
        statement.bindText(2, origin->stringIdentifier());
        
        error = !statement.executeCommand();
    }
        
    if (error)
        LOG_ERROR("Failed to set quota %llu in tracker database for origin %s", quota, origin->stringIdentifier().ascii().data());
    
    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
}
    
bool DatabaseTracker::addDatabase(SecurityOrigin* origin, const String& name, const String& path)
{
    if (!m_database.isOpen())
        return false;
        
    // New database should never be added until the origin has been established
    ASSERT(hasEntryForOrigin(origin));

    SQLiteStatement statement(m_database, "INSERT INTO Databases (origin, name, path) VALUES (?, ?, ?);");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->stringIdentifier());
    statement.bindText(2, name);
    statement.bindText(3, path);

    if (!statement.executeCommand()) {
        LOG_ERROR("Failed to add database %s to origin %s: %s\n", name.ascii().data(), origin->stringIdentifier().ascii().data(), statement.lastErrorMsg());
        return false;
    }
    
    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
    
    return true;
}

void DatabaseTracker::deleteAllDatabases()
{
    populateOrigins();
    
    HashMap<RefPtr<SecurityOrigin>, unsigned long long, SecurityOriginHash, SecurityOriginTraits>::const_iterator iter = m_originQuotaMap->begin();
    HashMap<RefPtr<SecurityOrigin>, unsigned long long, SecurityOriginHash, SecurityOriginTraits>::const_iterator end = m_originQuotaMap->end();

    for (; iter != end; ++iter)
        deleteDatabasesWithOrigin(iter->first.get());
}

void DatabaseTracker::deleteDatabasesWithOrigin(SecurityOrigin* origin)
{
    Vector<String> databaseNames;
    if (!databaseNamesForOrigin(origin, databaseNames)) {
        LOG_ERROR("Unable to retrieve list of database names for origin %s", origin->stringIdentifier().ascii().data());
        return;
    }
    
    for (unsigned i = 0; i < databaseNames.size(); ++i) {
        if (!deleteDatabaseFile(origin, databaseNames[i])) {
            LOG_ERROR("Unable to delete file for database %s in origin %s", databaseNames[i].ascii().data(), origin->stringIdentifier().ascii().data());
            return;
        }
    }
    
    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=?");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of databases from origin %s from tracker", origin->stringIdentifier().ascii().data());
        return;
    }
        
    statement.bindText(1, origin->stringIdentifier());
    
    if (!statement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->stringIdentifier().ascii().data());
        return;
    }
    
    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        for (unsigned i = 0; i < databaseNames.size(); ++i)
            m_client->dispatchDidModifyDatabase(origin, databaseNames[i]);
    }
}

void DatabaseTracker::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    if (!deleteDatabaseFile(origin, name)) {
        LOG_ERROR("Unable to delete file for database %s in origin %s", name.ascii().data(), origin->stringIdentifier().ascii().data());
        return;
    }
    
    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of database %s from origin %s from tracker", name.ascii().data(), origin->stringIdentifier().ascii().data());
        return;
    }
        
    statement.bindText(1, origin->stringIdentifier());
    statement.bindText(2, name);
    
    if (!statement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of database %s from origin %s from tracker", name.ascii().data(), origin->stringIdentifier().ascii().data());
        return;
    }
    
    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        m_client->dispatchDidModifyDatabase(origin, name);
    }
}

bool DatabaseTracker::deleteDatabaseFile(SecurityOrigin* origin, const String& name)
{
    String fullPath = fullPathForDatabase(origin, name, false);
    if (fullPath.isEmpty())
        return true;
        
    return deleteFile(fullPath);
}

void DatabaseTracker::setClient(DatabaseTrackerClient* client)
{
    m_client = client;
}

void DatabaseTracker::setDefaultOriginQuota(unsigned long long quota)
{
    m_defaultQuota = quota;
}

unsigned long long DatabaseTracker::defaultOriginQuota() const
{
    return m_defaultQuota;
}

static Mutex& notificationMutex()
{
    static Mutex mutex;
    return mutex;
}

static Vector<pair<SecurityOrigin*, String> >& notificationQueue()
{
    static Vector<pair<SecurityOrigin*, String> > queue;
    return queue;
}

void DatabaseTracker::scheduleNotifyDatabaseChanged(SecurityOrigin* origin, const String& name)
{
    MutexLocker locker(notificationMutex());

    notificationQueue().append(pair<SecurityOrigin*, String>(origin, name.copy()));
    scheduleForNotification();
}

static bool notificationScheduled = false;

void DatabaseTracker::scheduleForNotification()
{
    ASSERT(!notificationMutex().tryLock());

    if (!notificationScheduled) {
        callOnMainThread(DatabaseTracker::notifyDatabasesChanged);
        notificationScheduled = true;
    }
}

void DatabaseTracker::notifyDatabasesChanged()
{
    // Note that if DatabaseTracker ever becomes non-singleton, we'll have to ammend this notification
    // mechanism to inclue which tracker the notification goes out on, as well
    DatabaseTracker& theTracker(tracker());

    Vector<pair<SecurityOrigin*, String> > notifications;
    {
        MutexLocker locker(notificationMutex());

        notifications.swap(notificationQueue());

        notificationScheduled = false;
    }

    if (!theTracker.m_client)
        return;

    for (unsigned i = 0; i < notifications.size(); ++i)
        theTracker.m_client->dispatchDidModifyDatabase(notifications[i].first, notifications[i].second);
}


} // namespace WebCore
