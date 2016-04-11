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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Database.h"
#include "DatabaseContext.h"
#include "DatabaseManager.h"
#include "DatabaseManagerClient.h"
#include "DatabaseThread.h"
#include "FileSystem.h"
#include "Logging.h"
#include "OriginLock.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "UUID.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS)
#include "WebCoreThread.h"
#endif

namespace WebCore {

std::unique_ptr<DatabaseTracker> DatabaseTracker::trackerWithDatabasePath(const String& databasePath)
{
    return std::unique_ptr<DatabaseTracker>(new DatabaseTracker(databasePath));
}

static DatabaseTracker* staticTracker = nullptr;

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
    : m_client(nullptr)
{
    setDatabaseDirectoryPath(databasePath);
}

void DatabaseTracker::setDatabaseDirectoryPath(const String& path)
{
    LockHolder lockDatabase(m_databaseGuard);
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
    ASSERT(!m_databaseGuard.tryLock());
    unsigned long long usage = usageForOrigin(origin);

    // If the database will fit, allow its creation.
    unsigned long long requirement = usage + std::max<unsigned long long>(1, estimatedSize);
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

bool DatabaseTracker::canEstablishDatabase(DatabaseContext* context, const String& name, unsigned long estimatedSize, DatabaseError& error)
{
    error = DatabaseError::None;

    LockHolder lockDatabase(m_databaseGuard);
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
bool DatabaseTracker::retryCanEstablishDatabase(DatabaseContext* context, const String& name, unsigned long estimatedSize, DatabaseError& error)
{
    error = DatabaseError::None;

    LockHolder lockDatabase(m_databaseGuard);
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
    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT origin FROM Origins where origin=?;");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement.");
        return false;
    }

    statement.bindText(1, origin->databaseIdentifier());

    return statement.step() == SQLITE_ROW;
}

bool DatabaseTracker::hasEntryForOrigin(SecurityOrigin* origin)
{
    LockHolder lockDatabase(m_databaseGuard);
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

    if (statement.prepare() != SQLITE_OK)
        return false;

    statement.bindText(1, origin->databaseIdentifier());
    statement.bindText(2, databaseIdentifier);

    return statement.step() == SQLITE_ROW;
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const Database* database)
{
    // The maximum size for a database is the full quota for its origin, minus the current usage within the origin,
    // plus the current usage of the given database
    LockHolder lockDatabase(m_databaseGuard);
    SecurityOrigin* origin = database->securityOrigin();

    unsigned long long quota = quotaForOriginNoLock(origin);
    unsigned long long diskUsage = usageForOrigin(origin);
    unsigned long long databaseFileSize = SQLiteFileSystem::getDatabaseFileSize(database->fileName());
    ASSERT(databaseFileSize <= diskUsage);

    if (diskUsage > quota)
        return databaseFileSize;

    // A previous error may have allowed the origin to exceed its quota, or may
    // have allowed this database to exceed our cached estimate of the origin
    // disk usage. Don't multiply that error through integer underflow, or the
    // effective quota will permanently become 2^64.
    unsigned long long maxSize = quota - diskUsage + databaseFileSize;
    if (maxSize > quota)
        maxSize = databaseFileSize;
    return maxSize;
}

void DatabaseTracker::closeAllDatabases()
{
    Vector<Ref<Database>> openDatabases;
    {
        LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);
        if (!m_openDatabaseMap)
            return;
        for (auto& nameMap : m_openDatabaseMap->values()) {
            for (auto& set : nameMap->values()) {
                for (auto& database : *set)
                    openDatabases.append(*database);
            }
        }
    }
    for (auto& database : openDatabases)
        database->close();
}

String DatabaseTracker::originPath(SecurityOrigin* origin) const
{
    return SQLiteFileSystem::appendDatabaseFileNameToPath(m_databaseDirectoryPath.isolatedCopy(), origin->databaseIdentifier());
}

static String generateDatabaseFileName()
{
    StringBuilder stringBuilder;

    stringBuilder.append(createCanonicalUUIDString());
    stringBuilder.appendLiteral(".db");

    return stringBuilder.toString();
}

String DatabaseTracker::fullPathForDatabaseNoLock(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    ASSERT(!m_databaseGuard.tryLock());

    String originIdentifier = origin->databaseIdentifier();
    String originPath = this->originPath(origin);

    // Make sure the path for this SecurityOrigin exists
    if (createIfNotExists && !SQLiteFileSystem::ensureDatabaseDirectoryExists(originPath))
        return String();

    // See if we have a path for this database yet
    if (!m_database.isOpen())
        return String();
    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");

    if (statement.prepare() != SQLITE_OK)
        return String();

    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);

    int result = statement.step();

    if (result == SQLITE_ROW)
        return SQLiteFileSystem::appendDatabaseFileNameToPath(originPath, statement.getColumnText(0));
    if (!createIfNotExists)
        return String();

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to retrieve filename from Database Tracker for origin %s, name %s", originIdentifier.ascii().data(), name.ascii().data());
        return String();
    }
    statement.finalize();

    String fileName = generateDatabaseFileName();

    if (!addDatabase(origin, name, fileName))
        return String();

    // If this origin's quota is being tracked (open handle to a database in this origin), add this new database
    // to the quota manager now
    String fullFilePath = SQLiteFileSystem::appendDatabaseFileNameToPath(originPath, fileName);

    return fullFilePath;
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfNotExists)
{
    LockHolder lockDatabase(m_databaseGuard);
    return fullPathForDatabaseNoLock(origin, name, createIfNotExists).isolatedCopy();
}

void DatabaseTracker::origins(Vector<RefPtr<SecurityOrigin>>& originsResult)
{
    LockHolder lockDatabase(m_databaseGuard);

    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT origin FROM Origins");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement.");
        return;
    }

    int result;
    while ((result = statement.step()) == SQLITE_ROW) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(statement.getColumnText(0));
        originsResult.append(origin->isolatedCopy());
    }
    originsResult.shrinkToFit();

    if (result != SQLITE_DONE)
        LOG_ERROR("Failed to read in all origins from the database.");
}

bool DatabaseTracker::databaseNamesForOriginNoLock(SecurityOrigin* origin, Vector<String>& resultVector)
{
    ASSERT(!m_databaseGuard.tryLock());
    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT name FROM Databases where origin=?;");

    if (statement.prepare() != SQLITE_OK)
        return false;

    statement.bindText(1, origin->databaseIdentifier());

    int result;
    while ((result = statement.step()) == SQLITE_ROW)
        resultVector.append(statement.getColumnText(0));

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to retrieve all database names for origin %s", origin->databaseIdentifier().ascii().data());
        return false;
    }

    return true;
}

bool DatabaseTracker::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& resultVector)
{
    Vector<String> temp;
    {
        LockHolder lockDatabase(m_databaseGuard);
        if (!databaseNamesForOriginNoLock(origin, temp))
          return false;
    }

    for (auto& databaseName : temp)
        resultVector.append(databaseName.isolatedCopy());
    return true;
}

DatabaseDetails DatabaseTracker::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    String originIdentifier = origin->databaseIdentifier();
    String displayName;
    int64_t expectedUsage;

    {
        LockHolder lockDatabase(m_databaseGuard);

        openTrackerDatabase(DontCreateIfDoesNotExist);
        if (!m_database.isOpen())
            return DatabaseDetails();
        SQLiteStatement statement(m_database, "SELECT displayName, estimatedSize FROM Databases WHERE origin=? AND name=?");
        if (statement.prepare() != SQLITE_OK)
            return DatabaseDetails();

        statement.bindText(1, originIdentifier);
        statement.bindText(2, name);

        int result = statement.step();
        if (result == SQLITE_DONE)
            return DatabaseDetails();

        if (result != SQLITE_ROW) {
            LOG_ERROR("Error retrieving details for database %s in origin %s from tracker database", name.ascii().data(), originIdentifier.ascii().data());
            return DatabaseDetails();
        }
        displayName = statement.getColumnText(0);
        expectedUsage = statement.getColumnInt64(1);
    }

    String path = fullPathForDatabase(origin, name, false);
    if (path.isEmpty())
        return DatabaseDetails(name, displayName, expectedUsage, 0, 0, 0);
    return DatabaseDetails(name, displayName, expectedUsage, SQLiteFileSystem::getDatabaseFileSize(path), SQLiteFileSystem::databaseCreationTime(path), SQLiteFileSystem::databaseModificationTime(path));
}

void DatabaseTracker::setDatabaseDetails(SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
{
    String originIdentifier = origin->databaseIdentifier();
    int64_t guid = 0;

    LockHolder lockDatabase(m_databaseGuard);

    openTrackerDatabase(CreateIfDoesNotExist);
    if (!m_database.isOpen())
        return;
    SQLiteStatement statement(m_database, "SELECT guid FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLITE_OK)
        return;

    statement.bindText(1, originIdentifier);
    statement.bindText(2, name);

    int result = statement.step();
    if (result == SQLITE_ROW)
        guid = statement.getColumnInt64(0);
    statement.finalize();

    if (guid == 0) {
        if (result != SQLITE_DONE)
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
    if (updateStatement.prepare() != SQLITE_OK)
        return;

    updateStatement.bindText(1, displayName);
    updateStatement.bindInt64(2, estimatedSize);
    updateStatement.bindInt64(3, guid);

    if (updateStatement.step() != SQLITE_DONE) {
        LOG_ERROR("Failed to update details for database %s in origin %s", name.ascii().data(), originIdentifier.ascii().data());
        return;
    }

    if (m_client)
        m_client->dispatchDidModifyDatabase(origin, name);
}

void DatabaseTracker::doneCreatingDatabase(Database* database)
{
    LockHolder lockDatabase(m_databaseGuard);
    doneCreatingDatabase(database->securityOrigin(), database->stringIdentifier());
}

void DatabaseTracker::addOpenDatabase(Database* database)
{
    if (!database)
        return;

    {
        LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);

        if (!m_openDatabaseMap)
            m_openDatabaseMap = std::make_unique<DatabaseOriginMap>();

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
    }
}

void DatabaseTracker::removeOpenDatabase(Database* database)
{
    if (!database)
        return;

    {
        LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);

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
    }
}

void DatabaseTracker::getOpenDatabases(SecurityOrigin* origin, const String& name, HashSet<RefPtr<Database>>* databases)
{
    LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
    if (!nameMap)
        return;

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    for (auto& database : *databaseSet)
        databases->add(database);
}

RefPtr<OriginLock> DatabaseTracker::originLockFor(SecurityOrigin* origin)
{
    LockHolder lockDatabase(m_databaseGuard);
    String databaseIdentifier = origin->databaseIdentifier();

    // The originLockMap is accessed from multiple DatabaseThreads since
    // different script contexts can be writing to different databases from
    // the same origin. Hence, the databaseIdentifier key needs to be an
    // isolated copy. An isolated copy gives us a value whose refCounting is
    // thread-safe, since our copy is guarded by the m_databaseGuard mutex.
    databaseIdentifier = databaseIdentifier.isolatedCopy();

    OriginLockMap::AddResult addResult =
        m_originLockMap.add(databaseIdentifier, RefPtr<OriginLock>());
    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    String path = originPath(origin);
    RefPtr<OriginLock> lock = adoptRef(*new OriginLock(path));
    ASSERT(lock);
    addResult.iterator->value = lock;

    return lock;
}

void DatabaseTracker::deleteOriginLockFor(SecurityOrigin* origin)
{
    ASSERT(!m_databaseGuard.tryLock());

    // There is not always an instance of an OriginLock associated with an origin.
    // For example, if the OriginLock lock file was created by a previous run of
    // the browser which has now terminated, and the current browser process
    // has not executed any database transactions from this origin that would
    // have created the OriginLock instance in memory. In this case, we will have
    // a lock file but not an OriginLock instance in memory.

    // This function is only called if we are already deleting all the database
    // files in this origin. We'll give the OriginLock one chance to do an
    // orderly clean up first when we remove its ref from the m_originLockMap.
    // This may or may not be possible depending on whether other threads are
    // also using the OriginLock at the same time. After that, we will delete the lock file.

    m_originLockMap.remove(origin->databaseIdentifier());
    OriginLock::deleteLockFile(originPath(origin));
}

unsigned long long DatabaseTracker::usageForOrigin(SecurityOrigin* origin)
{
    String originPath = this->originPath(origin);
    unsigned long long diskUsage = 0;
    for (auto& fileName : listDirectory(originPath, ASCIILiteral("*.db"))) {
        long long size;
        getFileSize(fileName, size);
        diskUsage += size;
    }
    return diskUsage;
}

unsigned long long DatabaseTracker::quotaForOriginNoLock(SecurityOrigin* origin)
{
    ASSERT(!m_databaseGuard.tryLock());
    unsigned long long quota = 0;

    openTrackerDatabase(DontCreateIfDoesNotExist);
    if (!m_database.isOpen())
        return quota;

    SQLiteStatement statement(m_database, "SELECT quota FROM Origins where origin=?;");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement.");
        return quota;
    }
    statement.bindText(1, origin->databaseIdentifier());

    if (statement.step() == SQLITE_ROW)
        quota = statement.getColumnInt64(0);

    return quota;
}

unsigned long long DatabaseTracker::quotaForOrigin(SecurityOrigin* origin)
{
    LockHolder lockDatabase(m_databaseGuard);
    return quotaForOriginNoLock(origin);
}

void DatabaseTracker::setQuota(SecurityOrigin* origin, unsigned long long quota)
{
    LockHolder lockDatabase(m_databaseGuard);

    if (quotaForOriginNoLock(origin) == quota)
        return;

    openTrackerDatabase(CreateIfDoesNotExist);
    if (!m_database.isOpen())
        return;
    
#if PLATFORM(IOS)
    bool insertedNewOrigin = false;
#endif

    bool originEntryExists = hasEntryForOriginNoLock(origin);
    if (!originEntryExists) {
        SQLiteStatement statement(m_database, "INSERT INTO Origins VALUES (?, ?)");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to establish origin %s in the tracker", origin->databaseIdentifier().ascii().data());
        } else {
            statement.bindText(1, origin->databaseIdentifier());
            statement.bindInt64(2, quota);

            if (statement.step() != SQLITE_DONE)
                LOG_ERROR("Unable to establish origin %s in the tracker", origin->databaseIdentifier().ascii().data());
#if PLATFORM(IOS)
            else
                insertedNewOrigin = true;
#endif
        }
    } else {
        SQLiteStatement statement(m_database, "UPDATE Origins SET quota=? WHERE origin=?");
        bool error = statement.prepare() != SQLITE_OK;
        if (!error) {
            statement.bindInt64(1, quota);
            statement.bindText(2, origin->databaseIdentifier());

            error = !statement.executeCommand();
        }

        if (error)
            LOG_ERROR("Failed to set quota %llu in tracker database for origin %s", quota, origin->databaseIdentifier().ascii().data());
    }

    if (m_client) {
#if PLATFORM(IOS)
        if (insertedNewOrigin)
            m_client->dispatchDidAddNewOrigin(origin);
#endif
        m_client->dispatchDidModifyOrigin(origin);
    }
}

bool DatabaseTracker::addDatabase(SecurityOrigin* origin, const String& name, const String& path)
{
    ASSERT(!m_databaseGuard.tryLock());
    openTrackerDatabase(CreateIfDoesNotExist);
    if (!m_database.isOpen())
        return false;

    // New database should never be added until the origin has been established
    ASSERT(hasEntryForOriginNoLock(origin));

    SQLiteStatement statement(m_database, "INSERT INTO Databases (origin, name, path) VALUES (?, ?, ?);");

    if (statement.prepare() != SQLITE_OK)
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

void DatabaseTracker::deleteAllDatabasesImmediately()
{
    Vector<RefPtr<SecurityOrigin>> originsCopy;
    origins(originsCopy);

    // This method is only intended for use by DumpRenderTree / WebKitTestRunner.
    // Actually deleting the databases is necessary to reset to a known state before running
    // each test case, but may be unsafe in deployment use cases (where multiple applications
    // may be accessing the same databases concurrently).
    for (auto& origin : originsCopy)
        deleteOrigin(origin.get(), DeletionMode::Immediate);
}

void DatabaseTracker::deleteDatabasesModifiedSince(std::chrono::system_clock::time_point time)
{
    Vector<RefPtr<SecurityOrigin>> originsCopy;
    origins(originsCopy);

    for (auto& origin : originsCopy) {
        Vector<String> databaseNames;
        if (!databaseNamesForOrigin(origin.get(), databaseNames))
            continue;

        size_t deletedDatabases = 0;

        for (auto& databaseName : databaseNames) {
            auto fullPath = fullPathForDatabase(origin.get(), databaseName, false);

            time_t modificationTime;
            if (!getFileModificationTime(fullPath, modificationTime))
                continue;

            if (modificationTime < std::chrono::system_clock::to_time_t(time))
                continue;

            deleteDatabase(origin.get(), databaseName);
            ++deletedDatabases;
        }

        if (deletedDatabases == databaseNames.size())
            deleteOrigin(origin.get());
    }
}

// It is the caller's responsibility to make sure that nobody is trying to create, delete, open, or close databases in this origin while the deletion is
// taking place.
bool DatabaseTracker::deleteOrigin(SecurityOrigin* origin)
{
    return deleteOrigin(origin, DeletionMode::Default);
}

bool DatabaseTracker::deleteOrigin(SecurityOrigin* origin, DeletionMode deletionMode)
{
    Vector<String> databaseNames;
    {
        LockHolder lockDatabase(m_databaseGuard);
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
    for (auto& name : databaseNames) {
        if (!deleteDatabaseFile(origin, name, deletionMode)) {
            // Even if the file can't be deleted, we want to try and delete the rest, don't return early here.
            LOG_ERROR("Unable to delete file for database %s in origin %s", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        }
    }

    {
        LockHolder lockDatabase(m_databaseGuard);
        deleteOriginLockFor(origin);
        doneDeletingOrigin(origin);

        SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=?");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        statement.bindText(1, origin->databaseIdentifier());

        if (!statement.executeCommand()) {
            LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        SQLiteStatement originStatement(m_database, "DELETE FROM Origins WHERE origin=?");
        if (originStatement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare deletion of origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        originStatement.bindText(1, origin->databaseIdentifier());

        if (!originStatement.executeCommand()) {
            LOG_ERROR("Unable to execute deletion of databases from origin %s from tracker", origin->databaseIdentifier().ascii().data());
            return false;
        }

        SQLiteFileSystem::deleteEmptyDatabaseDirectory(originPath(origin));

        RefPtr<SecurityOrigin> originPossiblyLastReference = origin;
        bool isEmpty = true;

        openTrackerDatabase(DontCreateIfDoesNotExist);
        if (m_database.isOpen()) {
            SQLiteStatement statement(m_database, "SELECT origin FROM Origins");
            if (statement.prepare() != SQLITE_OK)
                LOG_ERROR("Failed to prepare statement.");
            else if (statement.step() == SQLITE_ROW)
                isEmpty = false;
        }

        // If we removed the last origin, do some additional deletion.
        if (isEmpty) {
            if (m_database.isOpen())
                m_database.close();
           SQLiteFileSystem::deleteDatabaseFile(trackerDatabasePath());
           SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_databaseDirectoryPath);
        }

        if (m_client) {
            m_client->dispatchDidModifyOrigin(origin);
#if PLATFORM(IOS)
            m_client->dispatchDidDeleteDatabaseOrigin();
#endif
            for (auto& name : databaseNames)
                m_client->dispatchDidModifyDatabase(origin, name);
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
        LockHolder lockDatabase(m_databaseGuard);
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
    if (!deleteDatabaseFile(origin, name, DeletionMode::Default)) {
        LOG_ERROR("Unable to delete file for database %s in origin %s", name.ascii().data(), origin->databaseIdentifier().ascii().data());
        LockHolder lockDatabase(m_databaseGuard);
        doneDeletingDatabase(origin, name);
        return false;
    }

    LockHolder lockDatabase(m_databaseGuard);

    SQLiteStatement statement(m_database, "DELETE FROM Databases WHERE origin=? AND name=?");
    if (statement.prepare() != SQLITE_OK) {
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

    if (m_client) {
        m_client->dispatchDidModifyOrigin(origin);
        m_client->dispatchDidModifyDatabase(origin, name);
#if PLATFORM(IOS)
        m_client->dispatchDidDeleteDatabase();
#endif
    }
    doneDeletingDatabase(origin, name);
    
    return true;
}

// deleteDatabaseFile has to release locks between looking up the list of databases to close and closing them.  While this is in progress, the caller
// is responsible for making sure no new databases are opened in the file to be deleted.
bool DatabaseTracker::deleteDatabaseFile(SecurityOrigin* origin, const String& name, DeletionMode deletionMode)
{
    String fullPath = fullPathForDatabase(origin, name, false);
    if (fullPath.isEmpty())
        return true;

#ifndef NDEBUG
    {
        LockHolder lockDatabase(m_databaseGuard);
        ASSERT(isDeletingDatabaseOrOriginFor(origin, name));
    }
#endif

    Vector<RefPtr<Database>> deletedDatabases;

    // Make sure not to hold the any locks when calling
    // Database::markAsDeletedAndClose(), since that can cause a deadlock
    // during the synchronous DatabaseThread call it triggers.
    {
        LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {
            // There are some open databases, lets check if they are for this origin.
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
            if (nameMap && nameMap->size()) {
                // There are some open databases for this origin, let's check
                // if they are this database by name.
                DatabaseSet* databaseSet = nameMap->get(name);
                if (databaseSet && databaseSet->size()) {
                    // We have some database open with this name. Mark them as deleted.
                    for (auto& database : *databaseSet)
                        deletedDatabases.append(database);
                }
            }
        }
    }

    for (auto& database : deletedDatabases)
        database->markAsDeletedAndClose();

#if PLATFORM(IOS)
    if (deletionMode == DeletionMode::Deferred) {
        // On the phone, other background processes may still be accessing this database. Deleting the database directly
        // would nuke the POSIX file locks, potentially causing Safari/WebApp to corrupt the new db if it's running in the background.
        // We'll instead truncate the database file to 0 bytes. If another process is operating on this same database file after
        // the truncation, it should get an error since the database file is no longer valid. When Safari is launched
        // next time, it'll go through the database files and clean up any zero-bytes ones.
        SQLiteDatabase database;
        if (!database.open(fullPath))
            return false;
        return SQLiteFileSystem::truncateDatabaseFile(database.sqlite3Handle());
    }
#else
    UNUSED_PARAM(deletionMode);
#endif

    return SQLiteFileSystem::deleteDatabaseFile(fullPath);
}
    
#if PLATFORM(IOS)
void DatabaseTracker::removeDeletedOpenedDatabases()
{
    // This is called when another app has deleted a database.  Go through all opened databases in this
    // tracker and close any that's no longer being tracked in the database.
    
    {
        // Acquire the lock before calling openTrackerDatabase.
        LockHolder lockDatabase(m_databaseGuard);
        openTrackerDatabase(DontCreateIfDoesNotExist);
    }

    if (!m_database.isOpen())
        return;
    
    // Keep track of which opened databases have been deleted.
    Vector<RefPtr<Database> > deletedDatabases;
    typedef HashMap<RefPtr<SecurityOrigin>, Vector<String> > DeletedDatabaseMap;
    DeletedDatabaseMap deletedDatabaseMap;
    
    // Make sure not to hold the m_openDatabaseMapGuard mutex when calling
    // Database::markAsDeletedAndClose(), since that can cause a deadlock
    // during the synchronous DatabaseThread call it triggers.
    {
        LockHolder openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {
            for (auto& openDatabase : *m_openDatabaseMap) {
                auto& origin = openDatabase.key;
                DatabaseNameMap* databaseNameMap = openDatabase.value;
                Vector<String> deletedDatabaseNamesForThisOrigin;

                // Loop through all opened databases in this origin.  Get the current database file path of each database and see if
                // it still matches the path stored in the opened database object.
                for (auto& databases : *databaseNameMap) {
                    String databaseName = databases.key;
                    String databaseFileName;
                    SQLiteStatement statement(m_database, "SELECT path FROM Databases WHERE origin=? AND name=?;");
                    if (statement.prepare() == SQLITE_OK) {
                        statement.bindText(1, origin->databaseIdentifier());
                        statement.bindText(2, databaseName);
                        if (statement.step() == SQLITE_ROW)
                            databaseFileName = statement.getColumnText(0);
                        statement.finalize();
                    }
                    
                    bool foundDeletedDatabase = false;
                    for (auto& db : *databases.value) {
                        // We are done if this database has already been marked as deleted.
                        if (db->deleted())
                            continue;
                        
                        // If this database has been deleted or if its database file no longer matches the current version, this database is no longer valid and it should be marked as deleted.
                        if (databaseFileName.isNull() || databaseFileName != pathGetFileName(db->fileName())) {
                            deletedDatabases.append(db);
                            foundDeletedDatabase = true;
                        }
                    }
                    
                    // If the database no longer exists, we should remember to remove it from the OriginQuotaManager later.
                    if (foundDeletedDatabase && databaseFileName.isNull())
                        deletedDatabaseNamesForThisOrigin.append(databaseName);
                }
                
                if (!deletedDatabaseNamesForThisOrigin.isEmpty())
                    deletedDatabaseMap.set(origin, deletedDatabaseNamesForThisOrigin);
            }
        }
    }
    
    for (auto& deletedDatabase : deletedDatabases)
        deletedDatabase->markAsDeletedAndClose();

    for (auto& deletedDatabase : deletedDatabaseMap) {
        SecurityOrigin* origin = deletedDatabase.key.get();
        if (m_client)
            m_client->dispatchDidModifyOrigin(origin);
        
        const Vector<String>& databaseNames = deletedDatabase.value;
        for (auto& databaseName : databaseNames) {
            if (m_client)
                m_client->dispatchDidModifyDatabase(origin, databaseName);
        }        
    }
}
    
static bool isZeroByteFile(const String& path)
{
    long long size = 0;
    return getFileSize(path, size) && !size;
}
    
bool DatabaseTracker::deleteDatabaseFileIfEmpty(const String& path)
{
    if (!isZeroByteFile(path))
        return false;
    
    SQLiteDatabase database;
    if (!database.open(path))
        return false;
    
    // Specify that we want the exclusive locking mode, so after the next read,
    // we'll be holding the lock to this database file.
    SQLiteStatement lockStatement(database, "PRAGMA locking_mode=EXCLUSIVE;");
    if (lockStatement.prepare() != SQLITE_OK)
        return false;
    int result = lockStatement.step();
    if (result != SQLITE_ROW && result != SQLITE_DONE)
        return false;
    lockStatement.finalize();

    // Every sqlite database has a sqlite_master table that contains the schema for the database.
    // http://www.sqlite.org/faq.html#q7
    SQLiteStatement readStatement(database, "SELECT * FROM sqlite_master LIMIT 1;");    
    if (readStatement.prepare() != SQLITE_OK)
        return false;
    // We shouldn't expect any result.
    if (readStatement.step() != SQLITE_DONE)
        return false;
    readStatement.finalize();
    
    // At this point, we hold the exclusive lock to this file.  Double-check again to make sure
    // it's still zero bytes.
    if (!isZeroByteFile(path))
        return false;
    
    return SQLiteFileSystem::deleteDatabaseFile(path);
}

Lock& DatabaseTracker::openDatabaseMutex()
{
    static NeverDestroyed<Lock> mutex;
    return mutex;
}

void DatabaseTracker::emptyDatabaseFilesRemovalTaskWillBeScheduled()
{
    // Lock the database from opening any database until we are done with scanning the file system for
    // zero byte database files to remove.
    openDatabaseMutex().lock();
}

void DatabaseTracker::emptyDatabaseFilesRemovalTaskDidFinish()
{
    openDatabaseMutex().unlock();
}

#endif

void DatabaseTracker::setClient(DatabaseManagerClient* client)
{
    m_client = client;
}

static Lock& notificationMutex()
{
    static NeverDestroyed<Lock> mutex;
    return mutex;
}

typedef Vector<std::pair<RefPtr<SecurityOrigin>, String>> NotificationQueue;

static NotificationQueue& notificationQueue()
{
    static NeverDestroyed<NotificationQueue> queue;
    return queue;
}

void DatabaseTracker::scheduleNotifyDatabaseChanged(SecurityOrigin* origin, const String& name)
{
    LockHolder locker(notificationMutex());

    notificationQueue().append(std::pair<RefPtr<SecurityOrigin>, String>(origin->isolatedCopy(), name.isolatedCopy()));
    scheduleForNotification();
}

static bool notificationScheduled = false;

void DatabaseTracker::scheduleForNotification()
{
    ASSERT(!notificationMutex().tryLock());

    if (!notificationScheduled) {
        callOnMainThread([] {
            notifyDatabasesChanged();
        });
        notificationScheduled = true;
    }
}

void DatabaseTracker::notifyDatabasesChanged()
{
    // Note that if DatabaseTracker ever becomes non-singleton, we'll have to amend this notification
    // mechanism to include which tracker the notification goes out on as well.
    DatabaseTracker& theTracker(tracker());

    NotificationQueue notifications;
    {
        LockHolder locker(notificationMutex());

        notifications.swap(notificationQueue());

        notificationScheduled = false;
    }

    if (!theTracker.m_client)
        return;

    for (unsigned i = 0; i < notifications.size(); ++i)
        theTracker.m_client->dispatchDidModifyDatabase(notifications[i].first.get(), notifications[i].second);
}


} // namespace WebCore
