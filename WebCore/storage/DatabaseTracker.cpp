/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#if ENABLE(DATABASE)

#include "ChromeClient.h"
#include "Database.h"
#include "DatabaseTrackerClient.h"
#include "Document.h"
#include "FileSystem.h"
#include "Logging.h"
#include "OriginQuotaManager.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SQLiteStatement.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

OriginQuotaManager& DatabaseTracker::originQuotaManager()
{
    populateOrigins();
    ASSERT(m_quotaManager);
    return *m_quotaManager;
}

DatabaseTracker& DatabaseTracker::tracker()
{
    DEFINE_STATIC_LOCAL(DatabaseTracker, tracker, ());
    return tracker;
}

DatabaseTracker::DatabaseTracker()
    : m_client(0)
    , m_proposedDatabase(0)
#ifndef NDEBUG
    , m_thread(currentThread())
#endif
{
}

void DatabaseTracker::setDatabaseDirectoryPath(const String& path)
{
    ASSERT(currentThread() == m_thread);
    ASSERT(!m_database.isOpen());
    m_databaseDirectoryPath = path;
}

const String& DatabaseTracker::databaseDirectoryPath() const
{
    ASSERT(currentThread() == m_thread);
    return m_databaseDirectoryPath;
}

String DatabaseTracker::trackerDatabasePath() const
{
    ASSERT(currentThread() == m_thread);
    if (m_databaseDirectoryPath.isEmpty())
        return String();
    return pathByAppendingComponent(m_databaseDirectoryPath, "Databases.db");
}

void DatabaseTracker::openTrackerDatabase(bool createIfDoesNotExist)
{
    ASSERT(currentThread() == m_thread);

    if (m_database.isOpen())
        return;

    String databasePath = trackerDatabasePath();
    if (databasePath.isEmpty())
        return;

    if (!createIfDoesNotExist && !fileExists(databasePath))
        return;

    makeAllDirectories(m_databaseDirectoryPath);
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
    ASSERT(currentThread() == m_thread);

    // Populate the origins before we establish a database; this guarantees that quotaForOrigin
    // can run on the database thread later.
    populateOrigins();

    SecurityOrigin* origin = document->securityOrigin();

    // Since we're imminently opening a database within this Document's origin, make sure this origin is being tracked by the QuotaTracker
    // by fetching it's current usage now
    unsigned long long usage = usageForOrigin(origin);
    
    // If a database already exists, ignore the passed-in estimated size and say it's OK.
    if (hasEntryForDatabase(origin, name))
        return true;

    // If the database will fit, allow its creation.
    unsigned long long requirement = usage + max(1UL, estimatedSize);
    if (requirement < usage)
        return false; // If the estimated size is so big it causes an overflow, don't allow creation.
    if (requirement <= quotaForOrigin(origin))
        return true;

    // Give the chrome client a chance to increase the quota.
    // Temporarily make the details of the proposed database available, so the client can get at them.
    Page* page = document->page();
    if (!page)
        return false;
    pair<SecurityOrigin*, DatabaseDetails> details(origin, DatabaseDetails(name, displayName, estimatedSize, 0));
    m_proposedDatabase = &details;
    page->chrome()->client()->exceededDatabaseQuota(document->frame(), name);
    m_proposedDatabase = 0;

    // If the database will fit now, allow its creation.
    return requirement <= quotaForOrigin(origin);
}

bool DatabaseTracker::hasEntryForOrigin(SecurityOrigin* origin)
{
    ASSERT(currentThread() == m_thread);
    populateOrigins();
    MutexLocker lockQuotaMap(m_quotaMapGuard);
    return m_quotaMap->contains(origin);
}

bool DatabaseTracker::hasEntryForDatabase(SecurityOrigin* origin, const String& databaseIdentifier)
{
    ASSERT(currentThread() == m_thread);
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return false;
    SQLiteStatement statement(m_database, "SELECT guid FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, databaseIdentifier);

    return statement.step() == SQLResultRow;
}

String DatabaseTracker::originPath(SecurityOrigin* origin) const
{
    ASSERT(currentThread() == m_thread);
    if (m_databaseDirectoryPath.isEmpty())
        return String();
    return pathByAppendingComponent(m_databaseDirectoryPath, origin->databaseIdentifier());
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    ASSERT(currentThread() == m_thread);

    if (m_proposedDatabase && m_proposedDatabase->first == origin && m_proposedDatabase->second.name() == name)
        return String();

    String originIdentifier = origin->databaseIdentifier();
    String originPath = this->originPath(origin);
    
    // Make sure the path for this SecurityOrigin exists
    if (createIfNotExists && !makeAllDirectories(originPath))
        return String();
    
    // See if we have a path for this database yet
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return String();
    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return String();

    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);

    int result = statement.step();

    if (result == SQLResultRow)
        return pathByAppendingComponent(originPath, statement.getColumnText(0));
    if (!createIfNotExists)
        return String();
        
    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve filename from Database Tracker for origin %s, name %s", origin->databaseIdentifier().ascii().data(), name.ascii().data());
        return String();
    }
    statement.finalize();
    
    SQLiteStatement sequenceStatement(m_database, "SELECT seq FROM sqlite_sequence WHERE name='Databases';");

    // FIXME: More informative error handling here, even though these steps should never fail
    if (sequenceStatement.prepare() != SQLResultOk)
        return String();
    result = sequenceStatement.step();

    // This has a range of 2^63 and starts at 0 for every time a user resets Safari -
    // I can't imagine it'd over overflow
    int64_t seq = 0;
    if (result == SQLResultRow) {
        seq = sequenceStatement.getColumnInt64(0);
    } else if (result != SQLResultDone)
        return String();
    sequenceStatement.finalize();

    String filename;
    do {
        ++seq;
        filename = pathByAppendingComponent(originPath, String::format("%016llx.db", seq));
    } while (fileExists(filename));

    if (!addDatabase(origin, name, String::format("%016llx.db", seq)))
        return String();

    // If this origin's quota is being tracked (open handle to a database in this origin), add this new database
    // to the quota manager now
    {
        Locker<OriginQuotaManager> locker(originQuotaManager());
        if (originQuotaManager().tracksOrigin(origin))
            originQuotaManager().addDatabase(origin, name, filename);
    }
    
    return filename;
}

void DatabaseTracker::populateOrigins()
{
    if (m_quotaMap)
        return;

    ASSERT(currentThread() == m_thread);

    m_quotaMap.set(new QuotaMap);
    m_quotaManager.set(new OriginQuotaManager);

    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT origin, quota FROM Origins");

    if (statement.prepare() != SQLResultOk)
        return;

    int result;
    while ((result = statement.step()) == SQLResultRow) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(statement.getColumnText(0));
        m_quotaMap->set(origin.get(), statement.getColumnInt64(1));
    }

    if (result != SQLResultDone)
        LOG_ERROR("Failed to read in all origins from the database");
}

void DatabaseTracker::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    ASSERT(currentThread() == m_thread);
    populateOrigins();
    MutexLocker lockQuotaMap(m_quotaMapGuard);
    copyKeysToVector(*m_quotaMap, result);
}

bool DatabaseTracker::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& resultVector)
{
    ASSERT(currentThread() == m_thread);
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT name FROM Databases where origin=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->databaseIdentifier());

    int result;
    while ((result = statement.step()) == SQLResultRow)
        resultVector.append(statement.getColumnText(0));

    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve all database names for origin %s", origin->databaseIdentifier().ascii().data());
        return false;
    }

    return true;
}

DatabaseDetails DatabaseTracker::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    ASSERT(currentThread() == m_thread);

    if (m_proposedDatabase && m_proposedDatabase->first == origin && m_proposedDatabase->second.name() == name)
        return m_proposedDatabase->second;

    String originIdentifier = origin->databaseIdentifier();

    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return DatabaseDetails();
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
    ASSERT(currentThread() == m_thread);

    String originIdentifier = origin->databaseIdentifier();
    int64_t guid = 0;
    
    openTrackerDatabase(true);
    if (!m_database.isOpen())
        return;
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
    ASSERT(currentThread() == m_thread);
    String path = fullPathForDatabase(origin, name, false);
    if (path.isEmpty())
        return 0;
        
    long long size;
    return getFileSize(path, size) ? size : 0;
}

void DatabaseTracker::addOpenDatabase(Database* database)
{
    if (!database)
        return;

    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

    if (!m_openDatabaseMap)
        m_openDatabaseMap.set(new DatabaseOriginMap);

    RefPtr<SecurityOrigin> origin(database->securityOriginCopy());
    String name(database->stringIdentifier());

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
    if (!nameMap) {
        nameMap = new DatabaseNameMap;
        m_openDatabaseMap->set(origin, nameMap);
    }

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet) {
        databaseSet = new DatabaseSet;
        nameMap->set(name, databaseSet);
    }

    databaseSet->add(database);

    LOG(StorageAPI, "Added open Database %s (%p)\n", database->stringIdentifier().ascii().data(), database);
}

void DatabaseTracker::removeOpenDatabase(Database* database)
{
    if (!database)
        return;

    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

    if (!m_openDatabaseMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr<SecurityOrigin> origin(database->securityOriginCopy());
    String name(database->stringIdentifier());

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
    if (!nameMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet) {
        ASSERT_NOT_REACHED();
        return;
    }

    databaseSet->remove(database);

    LOG(StorageAPI, "Removed open Database %s (%p)\n", database->stringIdentifier().ascii().data(), database);

    if (!databaseSet->isEmpty())
        return;

    nameMap->remove(name);
    delete databaseSet;

    if (!nameMap->isEmpty())
        return;

    m_openDatabaseMap->remove(origin);
    delete nameMap;
}

unsigned long long DatabaseTracker::usageForOrigin(SecurityOrigin* origin)
{
    ASSERT(currentThread() == m_thread);
    Locker<OriginQuotaManager> locker(originQuotaManager());

    // Use the OriginQuotaManager mechanism to calculate the usage
    if (originQuotaManager().tracksOrigin(origin))
        return originQuotaManager().diskUsage(origin);
    
    // If the OriginQuotaManager doesn't track this origin already, prime it to do so
    originQuotaManager().trackOrigin(origin);
    
    Vector<String> names;
    databaseNamesForOrigin(origin, names);

    for (unsigned i = 0; i < names.size(); ++i)
        originQuotaManager().addDatabase(origin, names[i], fullPathForDatabase(origin, names[i], false));
    
    if (!originQuotaManager().tracksOrigin(origin))
        return 0;
    return originQuotaManager().diskUsage(origin);
}

unsigned long long DatabaseTracker::quotaForOrigin(SecurityOrigin* origin)
{
    ASSERT(currentThread() == m_thread || m_quotaMap);
    populateOrigins();
    MutexLocker lockQuotaMap(m_quotaMapGuard);
    return m_quotaMap->get(origin);
}

void DatabaseTracker::setQuota(SecurityOrigin* origin, unsigned long long quota)
{
    ASSERT(currentThread() == m_thread);
    if (quotaForOrigin(origin) == quota)
        return;

    openTrackerDatabase(true);
    if (!m_database.isOpen())
        return;

    {
        MutexLocker lockQuotaMap(m_quotaMapGuard);

        if (!m_quotaMap->contains(origin)) {
            SQLiteStatement statement(m_database, "INSERT INTO Origins VALUES (?, ?)");
            if (statement.prepare() != SQLResultOk) {
                LOG_ERROR("Unable to establish origin %s in the tracker", origin->databaseIdentifier().ascii().data());
            } else {
                statement.bindText(1, origin->databaseIdentifier());
                statement.bindInt64(2, quota);

                if (statement.step() != SQLResultDone)
                    LOG_ERROR("Unable to establish origin %s in the tracker", origin->databaseIdentifier().ascii().data());
            }
        } else {
            SQLiteStatement statement(m_database, "UPDATE Origins SET quota=? WHERE origin=?");        
            bool error = statement.prepare() != SQLResultOk;
            if (!error) {
                statement.bindInt64(1, quota);
                statement.bindText(2, origin->databaseIdentifier());

                error = !statement.executeCommand();
            }

            if (error)
                LOG_ERROR("Failed to set quota %llu in tracker database for origin %s", quota, origin->databaseIdentifier().ascii().data());
        }

        // FIXME: Is it really OK to update the quota in memory if we failed to update it on disk?
        m_quotaMap->set(origin, quota);
    }

    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
}

bool DatabaseTracker::addDatabase(SecurityOrigin* origin, const String& name, const String& path)
{
    ASSERT(currentThread() == m_thread);
    openTrackerDatabase(true);
    if (!m_database.isOpen())
        return false;
        
    // New database should never be added until the origin has been established
    ASSERT(hasEntryForOrigin(origin));

    SQLiteStatement statement(m_database, "INSERT INTO Databases (origin, name, path) VALUES (?, ?, ?);");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, name);
    statement.bindText(3, path);

    if (!statement.executeCommand()) {
        LOG_ERROR("Failed to add database %s to origin %s: %s\n", name.ascii().data(), origin->databaseIdentifier().ascii().data(), m_database.lastErrorMsg());
        return false;
    }
    
    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
    
    return true;
}

void DatabaseTracker::deleteAllDatabases()
{
    ASSERT(currentThread() == m_thread);

    Vector<RefPtr<SecurityOrigin> > originsCopy;
    origins(originsCopy);

    for (unsigned i = 0; i < originsCopy.size(); ++i)
        deleteOrigin(originsCopy[i].get());
}

void DatabaseTracker::deleteOrigin(SecurityOrigin* origin)
{
    ASSERT(currentThread() == m_thread);
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return;

    Vector<String> databaseNames;
    if (!databaseNamesForOrigin(origin, databaseNames)) {
        LOG_ERROR("Unable to retrieve list of database names for origin %s", origin->databaseIdentifier().ascii().data());
        return;
    }
    
    for (unsigned i = 0; i < databaseNames.size(); ++i) {
        if (!deleteDatabaseFile(origin, databaseNames[i])) {
            LOG_ERROR("Unable to delete file for database %s in origin %s", databaseNames[i].ascii().data(), origin->databaseIdentifier().ascii().data());
            return;
        }
    }
    
    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=?");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
        return;
    }
        
    statement.bindText(1, origin->databaseIdentifier());
    
    if (!statement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
        return;
    }
    
    SQLiteStatement originStatement(m_database, "DELETE FROM Origins WHERE origin=?");
    if (originStatement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of origin %s from tracker", origin->databaseIdentifier().ascii().data());
        return;
    }

    originStatement.bindText(1, origin->databaseIdentifier());
    
    if (!originStatement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
        return;
    }

    deleteEmptyDirectory(originPath(origin));

    RefPtr<SecurityOrigin> originPossiblyLastReference = origin;
    {
        MutexLocker lockQuotaMap(m_quotaMapGuard);
        m_quotaMap->remove(origin);

        Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
        originQuotaManager().removeOrigin(origin);

        // If we removed the last origin, do some additional deletion.
        if (m_quotaMap->isEmpty()) {
            if (m_database.isOpen())
                m_database.close();
            deleteFile(trackerDatabasePath());
            deleteEmptyDirectory(m_databaseDirectoryPath);
        }
    }

    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        for (unsigned i = 0; i < databaseNames.size(); ++i)
            m_client->dispatchDidModifyDatabase(origin, databaseNames[i]);
    }
}

void DatabaseTracker::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    ASSERT(currentThread() == m_thread);
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return;

    if (!deleteDatabaseFile(origin, name)) {
        LOG_ERROR("Unable to delete file for database %s in origin %s", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        return;
    }
    
    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of database %s from origin %s from tracker", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        return;
    }
        
    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, name);
    
    if (!statement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of database %s from origin %s from tracker", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        return;
    }
    
    {
        Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
        originQuotaManager().removeDatabase(origin, name);
    }
    
    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        m_client->dispatchDidModifyDatabase(origin, name);
    }
}

bool DatabaseTracker::deleteDatabaseFile(SecurityOrigin* origin, const String& name)
{
    ASSERT(currentThread() == m_thread);
    String fullPath = fullPathForDatabase(origin, name, false);
    if (fullPath.isEmpty())
        return true;

    Vector<RefPtr<Database> > deletedDatabases;

    // Make sure not to hold the m_openDatabaseMapGuard mutex when calling
    // Database::markAsDeletedAndClose(), since that can cause a deadlock
    // during the synchronous DatabaseThread call it triggers.

    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {
            // There are some open databases, lets check if they are for this origin.
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
            if (nameMap && nameMap->size()) {
                // There are some open databases for this origin, lets check
                // if they are this database by name.
                DatabaseSet* databaseSet = nameMap->get(name);
                if (databaseSet && databaseSet->size()) {
                    // We have some database open with this name. Mark them as deleted.
                    DatabaseSet::const_iterator end = databaseSet->end();
                    for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it)
                        deletedDatabases.append(*it);
                }
            }
        }
    }

    for (unsigned i = 0; i < deletedDatabases.size(); ++i)
        deletedDatabases[i]->markAsDeletedAndClose();

    return deleteFile(fullPath);
}

void DatabaseTracker::setClient(DatabaseTrackerClient* client)
{
    ASSERT(currentThread() == m_thread);
    m_client = client;
}

static Mutex& notificationMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

typedef Vector<pair<SecurityOrigin*, String> > NotificationQueue;

static NotificationQueue& notificationQueue()
{
    DEFINE_STATIC_LOCAL(NotificationQueue, queue, ());
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
        callOnMainThread(DatabaseTracker::notifyDatabasesChanged, 0);
        notificationScheduled = true;
    }
}

void DatabaseTracker::notifyDatabasesChanged(void*)
{
    // Note that if DatabaseTracker ever becomes non-singleton, we'll have to amend this notification
    // mechanism to include which tracker the notification goes out on as well.
    DatabaseTracker& theTracker(tracker());

    NotificationQueue notifications;
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
#endif
