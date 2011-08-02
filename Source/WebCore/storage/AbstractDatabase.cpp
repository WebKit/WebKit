/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "AbstractDatabase.h"

#if ENABLE(DATABASE)
#include "DatabaseAuthorizer.h"
#include "DatabaseTracker.h"
#include "Logging.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static bool retrieveTextResultFromDatabase(SQLiteDatabase& db, const String& query, String& resultString)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLResultOk) {
        LOG_ERROR("Error (%i) preparing statement to read text result from database (%s)", result, query.ascii().data());
        return false;
    }

    result = statement.step();
    if (result == SQLResultRow) {
        resultString = statement.getColumnText(0);
        return true;
    }
    if (result == SQLResultDone) {
        resultString = String();
        return true;
    }

    LOG_ERROR("Error (%i) reading text result from database (%s)", result, query.ascii().data());
    return false;
}

static bool setTextValueInDatabase(SQLiteDatabase& db, const String& query, const String& value)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLResultOk) {
        LOG_ERROR("Failed to prepare statement to set value in database (%s)", query.ascii().data());
        return false;
    }

    statement.bindText(1, value);

    result = statement.step();
    if (result != SQLResultDone) {
        LOG_ERROR("Failed to step statement to set value in database (%s)", query.ascii().data());
        return false;
    }

    return true;
}

// FIXME: move all guid-related functions to a DatabaseVersionTracker class.
static Mutex& guidMutex()
{
    // Note: We don't have to use AtomicallyInitializedStatic here because
    // this function is called once in the constructor on the main thread
    // before any other threads that call this function are used.
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

typedef HashMap<int, String> GuidVersionMap;
static GuidVersionMap& guidToVersionMap()
{
    DEFINE_STATIC_LOCAL(GuidVersionMap, map, ());
    return map;
}

// NOTE: Caller must lock guidMutex().
static inline void updateGuidVersionMap(int guid, String newVersion)
{
    // Ensure the the mutex is locked.
    ASSERT(!guidMutex().tryLock());

    // Note: It is not safe to put an empty string into the guidToVersionMap() map.
    // That's because the map is cross-thread, but empty strings are per-thread.
    // The copy() function makes a version of the string you can use on the current
    // thread, but we need a string we can keep in a cross-thread data structure.
    // FIXME: This is a quite-awkward restriction to have to program with.

    // Map null string to empty string (see comment above).
    guidToVersionMap().set(guid, newVersion.isEmpty() ? String() : newVersion.threadsafeCopy());
}

typedef HashMap<int, HashSet<AbstractDatabase*>*> GuidDatabaseMap;
static GuidDatabaseMap& guidToDatabaseMap()
{
    DEFINE_STATIC_LOCAL(GuidDatabaseMap, map, ());
    return map;
}

static int guidForOriginAndName(const String& origin, const String& name)
{
    String stringID = origin + "/" + name;

    // Note: We don't have to use AtomicallyInitializedStatic here because
    // this function is called once in the constructor on the main thread
    // before any other threads that call this function are used.
    DEFINE_STATIC_LOCAL(Mutex, stringIdentifierMutex, ());
    MutexLocker locker(stringIdentifierMutex);
    typedef HashMap<String, int> IDGuidMap;
    DEFINE_STATIC_LOCAL(IDGuidMap, stringIdentifierToGUIDMap, ());
    int guid = stringIdentifierToGUIDMap.get(stringID);
    if (!guid) {
        static int currentNewGUID = 1;
        guid = currentNewGUID++;
        stringIdentifierToGUIDMap.set(stringID, guid);
    }

    return guid;
}

static bool isDatabaseAvailable = true;

bool AbstractDatabase::isAvailable()
{
    return isDatabaseAvailable;
}

void AbstractDatabase::setIsAvailable(bool available)
{
    isDatabaseAvailable = available;
}

// static
const String& AbstractDatabase::databaseInfoTableName()
{
    DEFINE_STATIC_LOCAL(String, name, ("__WebKitDatabaseInfoTable__"));
    return name;
}

AbstractDatabase::AbstractDatabase(ScriptExecutionContext* context, const String& name, const String& expectedVersion,
                                   const String& displayName, unsigned long estimatedSize)
    : m_scriptExecutionContext(context)
    , m_name(name.crossThreadString())
    , m_expectedVersion(expectedVersion.crossThreadString())
    , m_displayName(displayName.crossThreadString())
    , m_estimatedSize(estimatedSize)
    , m_guid(0)
    , m_opened(false)
    , m_new(false)
{
    ASSERT(context->isContextThread());
    m_contextThreadSecurityOrigin = m_scriptExecutionContext->securityOrigin();

    m_databaseAuthorizer = DatabaseAuthorizer::create(databaseInfoTableName());

    if (m_name.isNull())
        m_name = "";

    m_guid = guidForOriginAndName(securityOrigin()->toString(), name);
    {
        MutexLocker locker(guidMutex());

        HashSet<AbstractDatabase*>* hashSet = guidToDatabaseMap().get(m_guid);
        if (!hashSet) {
            hashSet = new HashSet<AbstractDatabase*>;
            guidToDatabaseMap().set(m_guid, hashSet);
        }

        hashSet->add(this);
    }

    m_filename = DatabaseTracker::tracker().fullPathForDatabase(securityOrigin(), m_name);
    DatabaseTracker::tracker().addOpenDatabase(this);
}

AbstractDatabase::~AbstractDatabase()
{
}

void AbstractDatabase::closeDatabase()
{
    if (!m_opened)
        return;

    m_sqliteDatabase.close();
    m_opened = false;
    {
        MutexLocker locker(guidMutex());

        HashSet<AbstractDatabase*>* hashSet = guidToDatabaseMap().get(m_guid);
        ASSERT(hashSet);
        ASSERT(hashSet->contains(this));
        hashSet->remove(this);
        if (hashSet->isEmpty()) {
            guidToDatabaseMap().remove(m_guid);
            delete hashSet;
            guidToVersionMap().remove(m_guid);
        }
    }
}

String AbstractDatabase::version() const
{
    // Note: In multi-process browsers the cached value may be accurate, but we cannot read the
    // actual version from the database without potentially inducing a deadlock.
    // FIXME: Add an async version getter to the DatabaseAPI.
    return getCachedVersion();
}

bool AbstractDatabase::performOpenAndVerify(bool shouldSetVersionInNewDatabase, ExceptionCode& ec)
{
    const int maxSqliteBusyWaitTime = 30000;

    if (!m_sqliteDatabase.open(m_filename, true)) {
        LOG_ERROR("Unable to open database at path %s", m_filename.ascii().data());
        ec = INVALID_STATE_ERR;
        return false;
    }
    if (!m_sqliteDatabase.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum for database %s", m_filename.ascii().data());

    m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);

    String currentVersion;
    {
        MutexLocker locker(guidMutex());

        GuidVersionMap::iterator entry = guidToVersionMap().find(m_guid);
        if (entry != guidToVersionMap().end()) {
            // Map null string to empty string (see updateGuidVersionMap()).
            currentVersion = entry->second.isNull() ? String("") : entry->second;
            LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());

#if PLATFORM(CHROMIUM)
            // Note: In multi-process browsers the cached value may be inaccurate, but
            // we cannot read the actual version from the database without potentially
            // inducing a form of deadlock, a busytimeout error when trying to
            // access the database. So we'll use the cached value if we're able to read
            // the value without waiting, and otherwise use the cached value (which may be off).
            // FIXME: Add an async openDatabase method to the DatabaseAPI.
            const int noSqliteBusyWaitTime = 0;
            m_sqliteDatabase.setBusyTimeout(noSqliteBusyWaitTime);
            String versionFromDatabase;
            if (getVersionFromDatabase(versionFromDatabase, false)) {
                currentVersion = versionFromDatabase;
                updateGuidVersionMap(m_guid, currentVersion);
            }
            m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);
#endif
        } else {
            LOG(StorageAPI, "No cached version for guid %i", m_guid);

            SQLiteTransaction transaction(m_sqliteDatabase);
            transaction.begin();
            if (!transaction.inProgress()) {
                LOG_ERROR("Unable to begin transaction while opening %s", databaseDebugName().ascii().data());
                ec = INVALID_STATE_ERR;
                m_sqliteDatabase.close();
                return false;
            }

            if (!m_sqliteDatabase.tableExists(databaseInfoTableName())) {
                m_new = true;

                if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + databaseInfoTableName() + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
                    LOG_ERROR("Unable to create table %s in database %s", databaseInfoTableName().ascii().data(), databaseDebugName().ascii().data());
                    ec = INVALID_STATE_ERR;
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return false;
                }
            } else if (!getVersionFromDatabase(currentVersion, false)) {
                LOG_ERROR("Failed to get current version from database %s", databaseDebugName().ascii().data());
                ec = INVALID_STATE_ERR;
                transaction.rollback();
                m_sqliteDatabase.close();
                return false;
            }

            if (currentVersion.length()) {
                LOG(StorageAPI, "Retrieved current version %s from database %s", currentVersion.ascii().data(), databaseDebugName().ascii().data());
            } else if (!m_new || shouldSetVersionInNewDatabase) {
                LOG(StorageAPI, "Setting version %s in database %s that was just created", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                if (!setVersionInDatabase(m_expectedVersion, false)) {
                    LOG_ERROR("Failed to set version %s in database %s", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                    ec = INVALID_STATE_ERR;
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return false;
                }
                currentVersion = m_expectedVersion;
            }
            updateGuidVersionMap(m_guid, currentVersion);
            transaction.commit();
        }
    }

    if (currentVersion.isNull()) {
        LOG(StorageAPI, "Database %s does not have its version set", databaseDebugName().ascii().data());
        currentVersion = "";
    }

    // If the expected version isn't the empty string, ensure that the current database version we have matches that version. Otherwise, set an exception.
    // If the expected version is the empty string, then we always return with whatever version of the database we have.
    if ((!m_new || shouldSetVersionInNewDatabase) && m_expectedVersion.length() && m_expectedVersion != currentVersion) {
        LOG(StorageAPI, "page expects version %s from database %s, which actually has version name %s - openDatabase() call will fail", m_expectedVersion.ascii().data(),
            databaseDebugName().ascii().data(), currentVersion.ascii().data());
        ec = INVALID_STATE_ERR;
        m_sqliteDatabase.close();
        return false;
    }

    ASSERT(m_databaseAuthorizer);
    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer);

    m_opened = true;

    if (m_new && !shouldSetVersionInNewDatabase)
        m_expectedVersion = ""; // The caller provided a creationCallback which will set the expected version.

    return true;
}

ScriptExecutionContext* AbstractDatabase::scriptExecutionContext() const
{
    return m_scriptExecutionContext.get();
}

SecurityOrigin* AbstractDatabase::securityOrigin() const
{
    return m_contextThreadSecurityOrigin.get();
}

String AbstractDatabase::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.threadsafeCopy();
}

String AbstractDatabase::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.threadsafeCopy();
}

unsigned long AbstractDatabase::estimatedSize() const
{
    return m_estimatedSize;
}

String AbstractDatabase::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.threadsafeCopy();
}

// static
const String& AbstractDatabase::databaseVersionKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebKitDatabaseVersionKey"));
    return key;
}

bool AbstractDatabase::getVersionFromDatabase(String& version, bool shouldCacheVersion)
{
    DEFINE_STATIC_LOCAL(String, getVersionQuery, ("SELECT value FROM " + databaseInfoTableName() + " WHERE key = '" + databaseVersionKey() + "';"));

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_sqliteDatabase, getVersionQuery.threadsafeCopy(), version);
    if (result) {
        if (shouldCacheVersion)
            setCachedVersion(version);
    } else
        LOG_ERROR("Failed to retrieve version from database %s", databaseDebugName().ascii().data());

    m_databaseAuthorizer->enable();

    return result;
}

bool AbstractDatabase::setVersionInDatabase(const String& version, bool shouldCacheVersion)
{
    // The INSERT will replace an existing entry for the database with the new version number, due to the UNIQUE ON CONFLICT REPLACE
    // clause in the CREATE statement (see Database::performOpenAndVerify()).
    DEFINE_STATIC_LOCAL(String, setVersionQuery, ("INSERT INTO " + databaseInfoTableName() + " (key, value) VALUES ('" + databaseVersionKey() + "', ?);"));

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_sqliteDatabase, setVersionQuery.threadsafeCopy(), version);
    if (result) {
        if (shouldCacheVersion)
            setCachedVersion(version);
    } else
        LOG_ERROR("Failed to set version %s in database (%s)", version.ascii().data(), setVersionQuery.ascii().data());

    m_databaseAuthorizer->enable();

    return result;
}

void AbstractDatabase::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.threadsafeCopy();
}

String AbstractDatabase::getCachedVersion() const
{
    MutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).threadsafeCopy();
}

void AbstractDatabase::setCachedVersion(const String& actualVersion)
{
    // Update the in memory database version map.
    MutexLocker locker(guidMutex());
    updateGuidVersionMap(m_guid, actualVersion);
}

bool AbstractDatabase::getActualVersionForTransaction(String &actualVersion)
{
    ASSERT(m_sqliteDatabase.transactionInProgress());
#if PLATFORM(CHROMIUM)
    // Note: In multi-process browsers the cached value may be inaccurate.
    // So we retrieve the value from the database and update the cached value here.
    return getVersionFromDatabase(actualVersion, true);
#else
    actualVersion = getCachedVersion();
    return true;
#endif
}

void AbstractDatabase::disableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->disable();
}

void AbstractDatabase::enableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->enable();
}

void AbstractDatabase::setAuthorizerReadOnly()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->setReadOnly();
}

void AbstractDatabase::setAuthorizerPermissions(int permissions)
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->setPermissions(permissions);
}

bool AbstractDatabase::lastActionChangedDatabase()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->lastActionChangedDatabase();
}

bool AbstractDatabase::lastActionWasInsert()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->lastActionWasInsert();
}

void AbstractDatabase::resetDeletes()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->resetDeletes();
}

bool AbstractDatabase::hadDeletes()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->hadDeletes();
}

void AbstractDatabase::resetAuthorizer()
{
    if (m_databaseAuthorizer)
        m_databaseAuthorizer->reset();
}

unsigned long long AbstractDatabase::maximumSize() const
{
    return DatabaseTracker::tracker().getMaxSizeForDatabase(this);
}

void AbstractDatabase::incrementalVacuumIfNeeded()
{
    int64_t freeSpaceSize = m_sqliteDatabase.freeSpaceSize();
    int64_t totalSize = m_sqliteDatabase.totalSize();
    if (totalSize <= 10 * freeSpaceSize)
        m_sqliteDatabase.runIncrementalVacuumCommand();
}

void AbstractDatabase::interrupt()
{
    m_sqliteDatabase.interrupt();
}

bool AbstractDatabase::isInterrupted()
{
    MutexLocker locker(m_sqliteDatabase.databaseMutex());
    return m_sqliteDatabase.isInterrupted();
}

} // namespace WebCore

#endif // ENABLE(DATABASE)
