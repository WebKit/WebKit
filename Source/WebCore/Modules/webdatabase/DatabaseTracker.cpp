/*
 * Copyright (C) 2007, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(SQL_DATABASE)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DatabaseBackendBase.h"
#include "DatabaseBackendContext.h"
#include "DatabaseManager.h"
#include "DatabaseManagerClient.h"
#include "DatabaseThread.h"
#include "Logging.h"
#include "OriginQuotaManager.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

using namespace std;

static WebCore::OriginQuotaManager& originQuotaManager()
{
    DEFINE_STATIC_LOCAL(WebCore::OriginQuotaManager, quotaManager, ());
    return quotaManager;
}

namespace WebCore {

static DatabaseTracker* staticTracker = 0;

void DatabaseTracker::initializeTracker(const String& databasePath)
{
    ASSERT(!staticTracker);
    if (staticTracker)
        return;

    staticTracker = new DatabaseTracker(databasePath);
}

DatabaseTracker& DatabaseTracker::tracker()
{
    if (!staticTracker)
        staticTracker = new DatabaseTracker("");

    return *staticTracker;
}

DatabaseTracker::DatabaseTracker(const String& databasePath)
    : m_client(0)
{
    setDatabaseDirectoryPath(databasePath);
    
    SQLiteFileSystem::registerSQLiteVFS();
}

void DatabaseTracker::setDatabaseDirectoryPath(const String& path)
{
    MutexLocker lockDatabase(m_databaseGuard);
    ASSERT(!m_database.isOpen());
    m_databaseDirectoryPath = path.isolatedCopy();
}

String DatabaseTracker::databaseDirectoryPath() const
{
    return m_databaseDirectoryPath.isolatedCopy();
}

String DatabaseTracker::trackerDatabasePath() const
{
    return SQLiteFileSystem::appendDatabaseFileNameToPath(m_databaseDirectoryPath.isolatedCopy(), "Databases.db");
}

void DatabaseTracker::openTrackerDatabase(TrackerCreationAction createAction)
{
    ASSERT(!m_databaseGuard.tryLock());

    if (m_database.isOpen())
        return;

    // If createIfDoesNotExist is false, SQLiteFileSystem::ensureDatabaseFileExists()
    // will return false if the database file does not exist.
    // If createIfDoesNotExist is true, SQLiteFileSystem::ensureDatabaseFileExists()
    // will attempt to create the path to the database file if it does not
    // exists yet. It'll return true if the path already exists, or if it
    // successfully creates the path. Else, it will return false.
    String databasePath = trackerDatabasePath();
    if (!SQLiteFileSystem::ensureDatabaseFileExists(databasePath, createAction == CreateIfDoesNotExist))
        return;

    if (!m_database.open(databasePath)) {
        // FIXME: What do do here?
        LOG_ERROR("Failed to open databasePath %s.", databasePath.ascii().data());
        return;
    }
    m_database.disableThreadingChecks();

    if (!m_database.tableExists("Origins")) {
        if (!m_database.executeCommand("CREATE TABLE Origins (origin TEXT UNIQUE ON CONFLICT REPLACE, quota INTEGER NOT NULL ON CONFLICT FAIL);")) {
            // FIXME: and here
            LOG_ERROR("Failed to create Origins table");
        }
    }

    if (!m_database.tableExists("Databases")) {
        if (!m_database.executeCommand("CREATE TABLE Databases (guid INTEGER PRIMARY KEY AUTOINCREMENT, origin TEXT, name TEXT, displayName TEXT, estimatedSize INTEGER, path TEXT);")) {
            // FIXME: and here
            LOG_ERROR("Failed to create Databases table");
        }
    }
}

bool DatabaseTracker::hasAdequateQuotaForOrigin(SecurityOrigin* origin, unsigned long estimatedSize, DatabaseError& err)
{
    // Since we're imminently opening a database within this context's origin,
    // make sure this origin is being tracked by the OriginQuotaManager
    // by fetching its current usage now. Calling usageForOrigin() has the side
    // effect of initiating tracking by the OriginQuotaManager if the origin is
    // not already tracked.
    unsigned long long usage = usageForOriginNoLock(origin);

    // If the database will fit, allow its creation.
    unsigned long long requirement = usage + max(1UL, estimatedSize);
    if (requirement < usage) {
        // The estimated size is so big it causes an overflow; don't allow creation.
        err = DatabaseError::DatabaseSizeOverflowed;
        return false;
    }
    if (requirement <= quotaForOriginNoLock(origin))
        return true;

    err = DatabaseError::DatabaseSizeExceededQuota;
    return false;
}

bool DatabaseTracker::canEstablishDatabase(DatabaseBackendContext* context, const String& name, const String& displayName, unsigned long estimatedSize, DatabaseError& error)
{
    UNUSED_PARAM(displayName); // Chromium needs the displayName but we don't.
    error = DatabaseError::None;

    MutexLocker lockDatabase(m_databaseGuard);
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
    SecurityOrigin* origin = context->securityOrigin();

    if (isDeletingDatabaseOrOriginFor(origin, name)) {
        error = DatabaseError::DatabaseIsBeingDeleted;
        return false;
    }

    recordCreatingDatabase(origin, name);

    // If a database already exists, ignore the passed-in estimated size and say it's OK.
    if (hasEntryForDatabase(origin, name))
        return true;

    if (hasAdequateQuotaForOrigin(origin, estimatedSize, error)) {
        ASSERT(error == DatabaseError::None);
        return true;
    }

    // If we get here, then we do not have enough quota for one of the
    // following reasons as indicated by the set error:
    //
    // If the error is DatabaseSizeOverflowed, then this means the requested
    // estimatedSize if so unreasonably large that it can cause an overflow in
    // the usage budget computation. In that case, there's nothing more we can
    // do, and there's no need for a retry. Hence, we should indicate that
    // we're done with our attempt to create the database.
    //
    // If the error is DatabaseSizeExceededQuota, then we'll give the client
    // a chance to update the quota and call retryCanEstablishDatabase() to try
    // again. Hence, we don't call doneCreatingDatabase() yet in that case.

    if (error == DatabaseError::DatabaseSizeOverflowed)
        doneCreatingDatabase(origin, name);
    else
        ASSERT(error == DatabaseError::DatabaseSizeExceededQuota);

    return false;
}

// Note: a thought about performance: hasAdequateQuotaForOrigin() was also
// called in canEstablishDatabase(), and hence, we're repeating some work within
// hasAdequateQuotaForOrigin(). However, retryCanEstablishDatabase() should only
// be called in the rare even if canEstablishDatabase() fails. Since it is rare,
// we should not bother optimizing it. It is more beneficial to keep
// hasAdequateQuotaForOrigin() simple and correct (i.e. bug free), and just
// re-use it. Also note that the path for opening a database involves IO, and
// hence should not be a performance critical path anyway. 
bool DatabaseTracker::retryCanEstablishDatabase(DatabaseBackendContext* context, const String& name, const String& displayName, unsigned long estimatedSize, DatabaseError& error)
{
    // Chromium needs the displayName in canEstablishDatabase(), but we don't.
    // Though Chromium does not use retryCanEstablishDatabase(), we should
    // keep the prototypes for canEstablishDatabase() and its retry function
    // the same. Hence, we also have an unneeded displayName arg here.
    UNUSED_PARAM(displayName);
    error = DatabaseError::None;

    MutexLocker lockDatabase(m_databaseGuard);
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
    SecurityOrigin* origin = context->securityOrigin();

    // We have already eliminated other types of errors in canEstablishDatabase().
    // The only reason we're in retryCanEstablishDatabase() is because we gave
    // the client a chance to update the quota and are rechecking it here.
    // If we fail this check, the only possible reason this time should be due
    // to inadequate quota.
    if (hasAdequateQuotaForOrigin(origin, estimatedSize, error)) {
        ASSERT(error == DatabaseError::None);
        return true;
    }

    ASSERT(error == DatabaseError::DatabaseSizeExceededQuota);
    doneCreatingDatabase(origin, name);

    return false;
}

bool DatabaseTracker::hasEntryForOriginNoLock(SecurityOrigin* origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    populateOriginsIfNeeded();
    ASSERT(m_quotaMap);
    return m_quotaMap->contains(origin);
}

bool DatabaseTracker::hasEntryForOrigin(SecurityOrigin* origin)
{
    MutexLocker lockDatabase(m_databaseGuard);
    return hasEntryForOriginNoLock(origin);
}

bool DatabaseTracker::hasEntryForDatabase(SecurityOrigin* origin, const String& databaseIdentifier)
{
    ASSERT(!m_databaseGuard.tryLock());
    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen()) {
        // No "tracker database". Hence, no entry for the database of interest.
        return false;
    }

    // We've got a tracker database. Set up a query to ask for the db of interest:
    SQLiteStatement statement(m_database, "SELECT guid FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, databaseIdentifier);

    return statement.step() == SQLResultRow;
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const DatabaseBackendBase* database)
{
    // The maximum size for a database is the full quota for its origin, minus the current usage within the origin,
    // plus the current usage of the given database
    MutexLocker lockDatabase(m_databaseGuard);
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
    SecurityOrigin* origin = database->securityOrigin();

    unsigned long long quota = quotaForOriginNoLock(origin);
    unsigned long long diskUsage = originQuotaManager().diskUsage(origin);
    unsigned long long databaseFileSize = SQLiteFileSystem::getDatabaseFileSize(database->fileName());

    // A previous error may have allowed the origin to exceed its quota, or may
    // have allowed this database to exceed our cached estimate of the origin
    // disk usage. Don't multiply that error through integer underflow, or the
    // effective quota will permanently become 2^64.
    unsigned long long maxSize = quota - diskUsage + databaseFileSize;
    if (maxSize > quota)
        maxSize = 0;
    return maxSize;
}

void DatabaseTracker::databaseChanged(DatabaseBackendBase* database)
{
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
    originQuotaManager().markDatabase(database);
}

void DatabaseTracker::interruptAllDatabasesForContext(const DatabaseBackendContext* context)
{
    Vector<RefPtr<DatabaseBackendBase> > openDatabases;
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

        if (!m_openDatabaseMap)
            return;

        DatabaseNameMap* nameMap = m_openDatabaseMap->get(context->securityOrigin());
        if (!nameMap)
            return;

        DatabaseNameMap::const_iterator dbNameMapEndIt = nameMap->end();
        for (DatabaseNameMap::const_iterator dbNameMapIt = nameMap->begin(); dbNameMapIt != dbNameMapEndIt; ++dbNameMapIt) {
            DatabaseSet* databaseSet = dbNameMapIt->value;
            DatabaseSet::const_iterator dbSetEndIt = databaseSet->end();
            for (DatabaseSet::const_iterator dbSetIt = databaseSet->begin(); dbSetIt != dbSetEndIt; ++dbSetIt) {
                if ((*dbSetIt)->databaseContext() == context)
                    openDatabases.append(*dbSetIt);
            }
        }
    }

    Vector<RefPtr<DatabaseBackendBase> >::const_iterator openDatabasesEndIt = openDatabases.end();
    for (Vector<RefPtr<DatabaseBackendBase> >::const_iterator openDatabasesIt = openDatabases.begin(); openDatabasesIt != openDatabasesEndIt; ++openDatabasesIt)
        (*openDatabasesIt)->interrupt();
}

String DatabaseTracker::originPath(SecurityOrigin* origin) const
{
    return SQLiteFileSystem::appendDatabaseFileNameToPath(m_databaseDirectoryPath.isolatedCopy(), origin->databaseIdentifier());
}

String DatabaseTracker::fullPathForDatabaseNoLock(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    ASSERT(!m_databaseGuard.tryLock());
    ASSERT(!originQuotaManager().tryLock());

    String originIdentifier = origin->databaseIdentifier();
    String originPath = this->originPath(origin);

    // Make sure the path for this SecurityOrigin exists
    if (createIfNotExists && !SQLiteFileSystem::ensureDatabaseDirectoryExists(originPath))
        return String();

    // See if we have a path for this database yet
    if (!m_database.isOpen())
        return String();
    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLResultOk)
        return String();

    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);

    int result = statement.step();

    if (result == SQLResultRow)
        return SQLiteFileSystem::appendDatabaseFileNameToPath(originPath, statement.getColumnText(0));
    if (!createIfNotExists)
        return String();

    if (result != SQLResultDone) {
        LOG_ERROR("Failed to retrieve filename from Database Tracker for origin %s, name %s", originIdentifier.ascii().data(), name.ascii().data());
        return String();
    }
    statement.finalize();

    String fileName = SQLiteFileSystem::getFileNameForNewDatabase(originPath, name, originIdentifier, &m_database);
    if (!addDatabase(origin, name, fileName))
        return String();

    // If this origin's quota is being tracked (open handle to a database in this origin), add this new database
    // to the quota manager now
    String fullFilePath = SQLiteFileSystem::appendDatabaseFileNameToPath(originPath, fileName);
    if (originQuotaManager().tracksOrigin(origin))
        originQuotaManager().addDatabase(origin, name, fullFilePath);

    return fullFilePath;
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    MutexLocker lockDatabase(m_databaseGuard);
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());

    return fullPathForDatabaseNoLock(origin, name, createIfNotExists).isolatedCopy();
}

void DatabaseTracker::populateOriginsIfNeeded()
{
    ASSERT(!m_databaseGuard.tryLock());
    if (m_quotaMap)
        return;

    m_quotaMap = adoptPtr(new QuotaMap);

    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT origin, quota FROM Origins");

    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare statement.");
        return;
    }

    int result;
    while ((result = statement.step()) == SQLResultRow) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(statement.getColumnText(0));
        m_quotaMap->set(origin.get()->isolatedCopy(), statement.getColumnInt64(1));
    }

    if (result != SQLResultDone)
        LOG_ERROR("Failed to read in all origins from the database.");
}

void DatabaseTracker::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    MutexLocker lockDatabase(m_databaseGuard);
    populateOriginsIfNeeded();
    ASSERT(m_quotaMap);
    copyKeysToVector(*m_quotaMap, result);
}

bool DatabaseTracker::databaseNamesForOriginNoLock(SecurityOrigin* origin, Vector<String>& resultVector)
{
    ASSERT(!m_databaseGuard.tryLock());
    openTrackerDatabase(DontCreateIfDoesNotExist);
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

bool DatabaseTracker::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& resultVector)
{
    Vector<String> temp;
    {
        MutexLocker lockDatabase(m_databaseGuard);
        if (!databaseNamesForOriginNoLock(origin, temp))
          return false;
    }

    for (Vector<String>::iterator iter = temp.begin(); iter != temp.end(); ++iter)
        resultVector.append(iter->isolatedCopy());
    return true;
}

DatabaseDetails DatabaseTracker::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    String originIdentifier = origin->databaseIdentifier();
    String displayName;
    int64_t expectedUsage;

    {
        MutexLocker lockDatabase(m_databaseGuard);

        openTrackerDatabase(DontCreateIfDoesNotExist);
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
        displayName = statement.getColumnText(0);
        expectedUsage = statement.getColumnInt64(1);
    }

    return DatabaseDetails(name, displayName, expectedUsage, usageForDatabase(name, origin));
}

void DatabaseTracker::setDatabaseDetails(SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
{
    String originIdentifier = origin->databaseIdentifier();
    int64_t guid = 0;

    MutexLocker lockDatabase(m_databaseGuard);

    openTrackerDatabase(CreateIfDoesNotExist);
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
    String path = fullPathForDatabase(origin, name, false);
    if (path.isEmpty())
        return 0;

    return SQLiteFileSystem::getDatabaseFileSize(path);
}

void DatabaseTracker::doneCreatingDatabase(DatabaseBackendBase* database)
{
    MutexLocker lockDatabase(m_databaseGuard);
    doneCreatingDatabase(database->securityOrigin(), database->stringIdentifier());
}

void DatabaseTracker::addOpenDatabase(DatabaseBackendBase* database)
{
    if (!database)
        return;

    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

        if (!m_openDatabaseMap)
            m_openDatabaseMap = adoptPtr(new DatabaseOriginMap);

        String name(database->stringIdentifier());
        DatabaseNameMap* nameMap = m_openDatabaseMap->get(database->securityOrigin());
        if (!nameMap) {
            nameMap = new DatabaseNameMap;
            m_openDatabaseMap->set(database->securityOrigin()->isolatedCopy(), nameMap);
        }

        DatabaseSet* databaseSet = nameMap->get(name);
        if (!databaseSet) {
            databaseSet = new DatabaseSet;
            nameMap->set(name.isolatedCopy(), databaseSet);
        }

        databaseSet->add(database);

        LOG(StorageAPI, "Added open Database %s (%p)\n", database->stringIdentifier().ascii().data(), database);

        Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
        if (!originQuotaManager().tracksOrigin(database->securityOrigin())) {
            originQuotaManager().trackOrigin(database->securityOrigin());
            originQuotaManager().addDatabase(database->securityOrigin(), database->stringIdentifier(), database->fileName());
        }
    }
}

void DatabaseTracker::removeOpenDatabase(DatabaseBackendBase* database)
{
    if (!database)
        return;

    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

        if (!m_openDatabaseMap) {
            ASSERT_NOT_REACHED();
            return;
        }

        String name(database->stringIdentifier());
        DatabaseNameMap* nameMap = m_openDatabaseMap->get(database->securityOrigin());
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

        m_openDatabaseMap->remove(database->securityOrigin());
        delete nameMap;

        Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
        originQuotaManager().removeOrigin(database->securityOrigin());
    }
}

void DatabaseTracker::getOpenDatabases(SecurityOrigin* origin, const String& name, HashSet<RefPtr<DatabaseBackendBase> >* databases)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
    if (!nameMap)
        return;

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    for (DatabaseSet::iterator it = databaseSet->begin(); it != databaseSet->end(); ++it)
        databases->add(*it);
}

unsigned long long DatabaseTracker::usageForOriginNoLock(SecurityOrigin* origin)
{
    ASSERT(!originQuotaManager().tryLock());

    // Use the OriginQuotaManager mechanism to calculate the usage
    if (originQuotaManager().tracksOrigin(origin))
        return originQuotaManager().diskUsage(origin);

    // If the OriginQuotaManager doesn't track this origin already, prime it to do so
    originQuotaManager().trackOrigin(origin);

    Vector<String> names;
    databaseNamesForOriginNoLock(origin, names);

    for (unsigned i = 0; i < names.size(); ++i)
        originQuotaManager().addDatabase(origin, names[i], fullPathForDatabaseNoLock(origin, names[i], false));

    if (!originQuotaManager().tracksOrigin(origin))
        return 0;

    // OriginQuotaManager::diskUsage() may result in a disk scan to compute the
    // sum of disk usage of all databases from this specified origin if its
    // cached usage value is determined to be outdated.
    return originQuotaManager().diskUsage(origin);
}

unsigned long long DatabaseTracker::usageForOrigin(SecurityOrigin* origin)
{
    MutexLocker lockDatabase(m_databaseGuard);
    Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
    return usageForOriginNoLock(origin);
}

unsigned long long DatabaseTracker::quotaForOriginNoLock(SecurityOrigin* origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    populateOriginsIfNeeded();
    ASSERT(m_quotaMap);
    return m_quotaMap->get(origin);
}

unsigned long long DatabaseTracker::quotaForOrigin(SecurityOrigin* origin)
{
    MutexLocker lockDatabase(m_databaseGuard);
    return quotaForOriginNoLock(origin);
}

void DatabaseTracker::setQuota(SecurityOrigin* origin, unsigned long long quota)
{
    MutexLocker lockDatabase(m_databaseGuard);

    if (quotaForOriginNoLock(origin) == quota)
        return;

    openTrackerDatabase(CreateIfDoesNotExist);
    if (!m_database.isOpen())
        return;

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
#if OS(WINDOWS)
            LOG_ERROR("Failed to set quota %I64u in tracker database for origin %s", quota, origin->databaseIdentifier().ascii().data());
#else
            LOG_ERROR("Failed to set quota %llu in tracker database for origin %s", quota, origin->databaseIdentifier().ascii().data());
#endif
    }

    // FIXME: Is it really OK to update the quota in memory if we failed to update it on disk?
    m_quotaMap->set(origin->isolatedCopy(), quota);

    if (m_client)
        m_client->dispatchDidModifyOrigin(origin);
}

bool DatabaseTracker::addDatabase(SecurityOrigin* origin, const String& name, const String& path)
{
    ASSERT(!m_databaseGuard.tryLock());
    openTrackerDatabase(CreateIfDoesNotExist);
    if (!m_database.isOpen())
        return false;
    populateOriginsIfNeeded();
    ASSERT(m_quotaMap);

    // New database should never be added until the origin has been established
    ASSERT(hasEntryForOriginNoLock(origin));

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
    Vector<RefPtr<SecurityOrigin> > originsCopy;
    origins(originsCopy);

    for (unsigned i = 0; i < originsCopy.size(); ++i)
        deleteOrigin(originsCopy[i].get());
}

// It is the caller's responsibility to make sure that nobody is trying to create, delete, open, or close databases in this origin while the deletion is
// taking place.
bool DatabaseTracker::deleteOrigin(SecurityOrigin* origin)
{
    Vector<String> databaseNames;
    {
        MutexLocker lockDatabase(m_databaseGuard);
        openTrackerDatabase(DontCreateIfDoesNotExist);
        if (!m_database.isOpen())
            return false;

        if (!databaseNamesForOriginNoLock(origin, databaseNames)) {
            LOG_ERROR("Unable to retrieve list of database names for origin %s", origin->databaseIdentifier().ascii().data());
            return false;
        }
        if (!canDeleteOrigin(origin)) {
            LOG_ERROR("Tried to delete an origin (%s) while either creating database in it or already deleting it", origin->databaseIdentifier().ascii().data());
            ASSERT_NOT_REACHED();
            return false;
        }
        recordDeletingOrigin(origin);
    }

    // We drop the lock here because holding locks during a call to deleteDatabaseFile will deadlock.
    for (unsigned i = 0; i < databaseNames.size(); ++i) {
        if (!deleteDatabaseFile(origin, databaseNames[i])) {
            // Even if the file can't be deleted, we want to try and delete the rest, don't return early here.
            LOG_ERROR("Unable to delete file for database %s in origin %s", databaseNames[i].ascii().data(), origin->databaseIdentifier().ascii().data());
        }
    }

    {
        MutexLocker lockDatabase(m_databaseGuard);
        doneDeletingOrigin(origin);

        SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=?");
        if (statement.prepare() != SQLResultOk) {
            LOG_ERROR("Unable to prepare deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        statement.bindText(1, origin->databaseIdentifier());

        if (!statement.executeCommand()) {
            LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        SQLiteStatement originStatement(m_database, "DELETE FROM Origins WHERE origin=?");
        if (originStatement.prepare() != SQLResultOk) {
            LOG_ERROR("Unable to prepare deletion of origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        originStatement.bindText(1, origin->databaseIdentifier());

        if (!originStatement.executeCommand()) {
            LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        SQLiteFileSystem::deleteEmptyDatabaseDirectory(originPath(origin));

        populateOriginsIfNeeded();
        RefPtr<SecurityOrigin> originPossiblyLastReference = origin;
        m_quotaMap->remove(origin);

        {
            Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
            originQuotaManager().removeOrigin(origin);
        }

        // If we removed the last origin, do some additional deletion.
        if (m_quotaMap->isEmpty()) {
            if (m_database.isOpen())
                m_database.close();
           SQLiteFileSystem::deleteDatabaseFile(trackerDatabasePath());
           SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_databaseDirectoryPath);
        }

        if (m_client) {
            m_client->dispatchDidModifyOrigin(origin);
            for (unsigned i = 0; i < databaseNames.size(); ++i)
                m_client->dispatchDidModifyDatabase(origin, databaseNames[i]);
        }
    }
    return true;
}

bool DatabaseTracker::isDeletingDatabaseOrOriginFor(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    // Can't create a database while someone else is deleting it; there's a risk of leaving untracked database debris on the disk.
    return isDeletingDatabase(origin, name) || isDeletingOrigin(origin);
}

void DatabaseTracker::recordCreatingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    NameCountMap* nameMap = m_beingCreated.get(origin);
    if (!nameMap) {
        nameMap = new NameCountMap();
        m_beingCreated.set(origin->isolatedCopy(), nameMap);
    }
    long count = nameMap->get(name);
    nameMap->set(name.isolatedCopy(), count + 1);
}

void DatabaseTracker::doneCreatingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    NameCountMap* nameMap = m_beingCreated.get(origin);
    ASSERT(nameMap);
    if (!nameMap)
        return;

    long count = nameMap->get(name);
    ASSERT(count > 0);
    if (count <= 1) {
        nameMap->remove(name);
        if (nameMap->isEmpty()) {
            m_beingCreated.remove(origin);
            delete nameMap;
        }
    } else
        nameMap->set(name, count - 1);
}

bool DatabaseTracker::creatingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    NameCountMap* nameMap = m_beingCreated.get(origin);
    return nameMap && nameMap->get(name);
}

bool DatabaseTracker::canDeleteDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    return !creatingDatabase(origin, name) && !isDeletingDatabase(origin, name);
}

void DatabaseTracker::recordDeletingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    ASSERT(canDeleteDatabase(origin, name));
    NameSet* nameSet = m_beingDeleted.get(origin);
    if (!nameSet) {
        nameSet = new NameSet();
        m_beingDeleted.set(origin->isolatedCopy(), nameSet);
    }
    ASSERT(!nameSet->contains(name));
    nameSet->add(name.isolatedCopy());
}

void DatabaseTracker::doneDeletingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    NameSet* nameSet = m_beingDeleted.get(origin);
    ASSERT(nameSet);
    if (!nameSet)
        return;

    ASSERT(nameSet->contains(name));
    nameSet->remove(name);
    if (nameSet->isEmpty()) {
        m_beingDeleted.remove(origin);
        delete nameSet;
    }
}

bool DatabaseTracker::isDeletingDatabase(SecurityOrigin *origin, const String& name)
{
    ASSERT(!m_databaseGuard.tryLock());
    NameSet* nameSet = m_beingDeleted.get(origin);
    return nameSet && nameSet->contains(name);
}

bool DatabaseTracker::canDeleteOrigin(SecurityOrigin *origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    return !(isDeletingOrigin(origin) || m_beingCreated.get(origin));
}

bool DatabaseTracker::isDeletingOrigin(SecurityOrigin *origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    return m_originsBeingDeleted.contains(origin);
}

void DatabaseTracker::recordDeletingOrigin(SecurityOrigin *origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    ASSERT(!isDeletingOrigin(origin));
    m_originsBeingDeleted.add(origin->isolatedCopy());
}

void DatabaseTracker::doneDeletingOrigin(SecurityOrigin *origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    ASSERT(isDeletingOrigin(origin));
    m_originsBeingDeleted.remove(origin);
}

bool DatabaseTracker::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    {
        MutexLocker lockDatabase(m_databaseGuard);
        openTrackerDatabase(DontCreateIfDoesNotExist);
        if (!m_database.isOpen())
            return false;

        if (!canDeleteDatabase(origin, name)) {
            ASSERT_NOT_REACHED();
            return false;
        }
        recordDeletingDatabase(origin, name);
    }

    // We drop the lock here because holding locks during a call to deleteDatabaseFile will deadlock.
    if (!deleteDatabaseFile(origin, name)) {
        LOG_ERROR("Unable to delete file for database %s in origin %s", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        MutexLocker lockDatabase(m_databaseGuard);
        doneDeletingDatabase(origin, name);
        return false;
    }

    MutexLocker lockDatabase(m_databaseGuard);

    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to prepare deletion of database %s from origin %s from tracker", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        doneDeletingDatabase(origin, name);
        return false;
    }

    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, name);

    if (!statement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of database %s from origin %s from tracker", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        doneDeletingDatabase(origin, name);
        return false;
    }

    {
        Locker<OriginQuotaManager> quotaManagerLocker(originQuotaManager());
        originQuotaManager().removeDatabase(origin, name);
    }

    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        m_client->dispatchDidModifyDatabase(origin, name);
    }
    doneDeletingDatabase(origin, name);
    
    return true;
}

// deleteDatabaseFile has to release locks between looking up the list of databases to close and closing them.  While this is in progress, the caller
// is responsible for making sure no new databases are opened in the file to be deleted.
bool DatabaseTracker::deleteDatabaseFile(SecurityOrigin* origin, const String& name)
{
    String fullPath = fullPathForDatabase(origin, name, false);
    if (fullPath.isEmpty())
        return true;

#ifndef NDEBUG
    {
        MutexLocker lockDatabase(m_databaseGuard);
        ASSERT(isDeletingDatabaseOrOriginFor(origin, name));
    }
#endif

    Vector<RefPtr<DatabaseBackendBase> > deletedDatabases;

    // Make sure not to hold the any locks when calling
    // Database::markAsDeletedAndClose(), since that can cause a deadlock
    // during the synchronous DatabaseThread call it triggers.
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {
            // There are some open databases, lets check if they are for this origin.
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
            if (nameMap && nameMap->size()) {
                // There are some open databases for this origin, let's check
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

    return SQLiteFileSystem::deleteDatabaseFile(fullPath);
}

void DatabaseTracker::setClient(DatabaseManagerClient* client)
{
    m_client = client;
}

static Mutex& notificationMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

typedef Vector<pair<RefPtr<SecurityOrigin>, String> > NotificationQueue;

static NotificationQueue& notificationQueue()
{
    DEFINE_STATIC_LOCAL(NotificationQueue, queue, ());
    return queue;
}

void DatabaseTracker::scheduleNotifyDatabaseChanged(SecurityOrigin* origin, const String& name)
{
    MutexLocker locker(notificationMutex());

    notificationQueue().append(pair<RefPtr<SecurityOrigin>, String>(origin->isolatedCopy(), name.isolatedCopy()));
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
        theTracker.m_client->dispatchDidModifyDatabase(notifications[i].first.get(), notifications[i].second);
}


} // namespace WebCore
#endif
