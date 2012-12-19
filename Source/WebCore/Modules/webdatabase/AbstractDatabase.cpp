/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(SQL_DATABASE)

#include "DatabaseAuthorizer.h"
#include "DatabaseContext.h"
#include "DatabaseManager.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(CHROMIUM)
#include "DatabaseObserver.h" // For error reporting.
#endif

namespace WebCore {

static const char versionKey[] = "WebKitDatabaseVersionKey";
static const char infoTableName[] = "__WebKitDatabaseInfoTable__";

static String formatErrorMessage(const char* message, int sqliteErrorCode, const char* sqliteErrorMessage)
{
    return String::format("%s (%d %s)", message, sqliteErrorCode, sqliteErrorMessage);
}

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
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

typedef HashMap<DatabaseGuid, String> GuidVersionMap;
static GuidVersionMap& guidToVersionMap()
{
    // Ensure the the mutex is locked.
    ASSERT(!guidMutex().tryLock());
    DEFINE_STATIC_LOCAL(GuidVersionMap, map, ());
    return map;
}

// NOTE: Caller must lock guidMutex().
static inline void updateGuidVersionMap(DatabaseGuid guid, String newVersion)
{
    // Ensure the the mutex is locked.
    ASSERT(!guidMutex().tryLock());

    // Note: It is not safe to put an empty string into the guidToVersionMap() map.
    // That's because the map is cross-thread, but empty strings are per-thread.
    // The copy() function makes a version of the string you can use on the current
    // thread, but we need a string we can keep in a cross-thread data structure.
    // FIXME: This is a quite-awkward restriction to have to program with.

    // Map null string to empty string (see comment above).
    guidToVersionMap().set(guid, newVersion.isEmpty() ? String() : newVersion.isolatedCopy());
}

typedef HashMap<DatabaseGuid, HashSet<AbstractDatabase*>*> GuidDatabaseMap;
static GuidDatabaseMap& guidToDatabaseMap()
{
    // Ensure the the mutex is locked.
    ASSERT(!guidMutex().tryLock());
    DEFINE_STATIC_LOCAL(GuidDatabaseMap, map, ());
    return map;
}

static DatabaseGuid guidForOriginAndName(const String& origin, const String& name)
{
    // Ensure the the mutex is locked.
    ASSERT(!guidMutex().tryLock());

    String stringID = origin + "/" + name;

    typedef HashMap<String, int> IDGuidMap;
    DEFINE_STATIC_LOCAL(IDGuidMap, stringIdentifierToGUIDMap, ());
    DatabaseGuid guid = stringIdentifierToGUIDMap.get(stringID);
    if (!guid) {
        static int currentNewGUID = 1;
        guid = currentNewGUID++;
        stringIdentifierToGUIDMap.set(stringID, guid);
    }

    return guid;
}

// static
const char* AbstractDatabase::databaseInfoTableName()
{
    return infoTableName;
}

AbstractDatabase::AbstractDatabase(ScriptExecutionContext* context, const String& name, const String& expectedVersion,
                                   const String& displayName, unsigned long estimatedSize, DatabaseType databaseType)
    : m_scriptExecutionContext(context)
    , m_databaseContext(DatabaseContext::from(context))
    , m_name(name.isolatedCopy())
    , m_expectedVersion(expectedVersion.isolatedCopy())
    , m_displayName(displayName.isolatedCopy())
    , m_estimatedSize(estimatedSize)
    , m_guid(0)
    , m_opened(false)
    , m_new(false)
    , m_isSyncDatabase(databaseType == SyncDatabase)
{
    ASSERT(context->isContextThread());
    m_contextThreadSecurityOrigin = m_scriptExecutionContext->securityOrigin();

    m_databaseAuthorizer = DatabaseAuthorizer::create(infoTableName);

    if (m_name.isNull())
        m_name = "";

    {
        MutexLocker locker(guidMutex());
        m_guid = guidForOriginAndName(securityOrigin()->toString(), name);
        HashSet<AbstractDatabase*>* hashSet = guidToDatabaseMap().get(m_guid);
        if (!hashSet) {
            hashSet = new HashSet<AbstractDatabase*>;
            guidToDatabaseMap().set(m_guid, hashSet);
        }

        hashSet->add(this);
    }

    m_filename = DatabaseManager::manager().fullPathForDatabase(securityOrigin(), m_name);
    DatabaseManager::manager().addOpenDatabase(this);
}

AbstractDatabase::~AbstractDatabase()
{
    ASSERT(!m_opened);
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

bool AbstractDatabase::performOpenAndVerify(bool shouldSetVersionInNewDatabase, ExceptionCode& ec, String& errorMessage)
{
    ASSERT(errorMessage.isEmpty());

    const int maxSqliteBusyWaitTime = 30000;

    if (!m_sqliteDatabase.open(m_filename, true)) {
        reportOpenDatabaseResult(1, INVALID_STATE_ERR, m_sqliteDatabase.lastError());
        errorMessage = formatErrorMessage("unable to open database", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
        ec = INVALID_STATE_ERR;
        return false;
    }
    if (!m_sqliteDatabase.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum (%d %s)", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());

    m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);

    String currentVersion;
    {
        MutexLocker locker(guidMutex());

        GuidVersionMap::iterator entry = guidToVersionMap().find(m_guid);
        if (entry != guidToVersionMap().end()) {
            // Map null string to empty string (see updateGuidVersionMap()).
            currentVersion = entry->value.isNull() ? emptyString() : entry->value.isolatedCopy();
            LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());

#if PLATFORM(CHROMIUM)
            // Note: In multi-process browsers the cached value may be inaccurate, but
            // we cannot read the actual version from the database without potentially
            // inducing a form of deadlock, a busytimeout error when trying to
            // access the database. So we'll use the cached value if we're unable to read
            // the value from the database file without waiting.
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
                reportOpenDatabaseResult(2, INVALID_STATE_ERR, m_sqliteDatabase.lastError());
                errorMessage = formatErrorMessage("unable to open database, failed to start transaction", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                ec = INVALID_STATE_ERR;
                m_sqliteDatabase.close();
                return false;
            }

            String tableName(infoTableName);
            if (!m_sqliteDatabase.tableExists(tableName)) {
                m_new = true;

                if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + tableName + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
                    reportOpenDatabaseResult(3, INVALID_STATE_ERR, m_sqliteDatabase.lastError());
                    errorMessage = formatErrorMessage("unable to open database, failed to create 'info' table", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                    ec = INVALID_STATE_ERR;
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return false;
                }
            } else if (!getVersionFromDatabase(currentVersion, false)) {
                reportOpenDatabaseResult(4, INVALID_STATE_ERR, m_sqliteDatabase.lastError());
                errorMessage = formatErrorMessage("unable to open database, failed to read current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
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
                    reportOpenDatabaseResult(5, INVALID_STATE_ERR, m_sqliteDatabase.lastError());
                    errorMessage = formatErrorMessage("unable to open database, failed to write current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
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
        reportOpenDatabaseResult(6, INVALID_STATE_ERR, 0);
        errorMessage = "unable to open database, version mismatch, '" + m_expectedVersion + "' does not match the currentVersion of '" + currentVersion + "'";
        ec = INVALID_STATE_ERR;
        m_sqliteDatabase.close();
        return false;
    }

    ASSERT(m_databaseAuthorizer);
    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer);

    m_opened = true;

    if (m_new && !shouldSetVersionInNewDatabase)
        m_expectedVersion = ""; // The caller provided a creationCallback which will set the expected version.

    reportOpenDatabaseResult(0, -1, 0); // OK
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
    return m_name.isolatedCopy();
}

String AbstractDatabase::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.isolatedCopy();
}

unsigned long AbstractDatabase::estimatedSize() const
{
    return m_estimatedSize;
}

String AbstractDatabase::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.isolatedCopy();
}

bool AbstractDatabase::getVersionFromDatabase(String& version, bool shouldCacheVersion)
{
    String query(String("SELECT value FROM ") + infoTableName +  " WHERE key = '" + versionKey + "';");

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_sqliteDatabase, query, version);
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
    String query(String("INSERT INTO ") + infoTableName +  " (key, value) VALUES ('" + versionKey + "', ?);");

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_sqliteDatabase, query, version);
    if (result) {
        if (shouldCacheVersion)
            setCachedVersion(version);
    } else
        LOG_ERROR("Failed to set version %s in database (%s)", version.ascii().data(), query.ascii().data());

    m_databaseAuthorizer->enable();

    return result;
}

void AbstractDatabase::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.isolatedCopy();
}

String AbstractDatabase::getCachedVersion() const
{
    MutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).isolatedCopy();
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
    return DatabaseManager::manager().getMaxSizeForDatabase(this);
}

void AbstractDatabase::incrementalVacuumIfNeeded()
{
    int64_t freeSpaceSize = m_sqliteDatabase.freeSpaceSize();
    int64_t totalSize = m_sqliteDatabase.totalSize();
    if (totalSize <= 10 * freeSpaceSize) {
        int result = m_sqliteDatabase.runIncrementalVacuumCommand();
        reportVacuumDatabaseResult(result);
        if (result != SQLResultOk)
            logErrorMessage(formatErrorMessage("error vacuuming database", result, m_sqliteDatabase.lastErrorMsg()));
    }
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

void AbstractDatabase::logErrorMessage(const String& message)
{
    m_scriptExecutionContext->addConsoleMessage(OtherMessageSource, ErrorMessageLevel, message);
}

#if PLATFORM(CHROMIUM)
// These are used to generate histograms of errors seen with websql.
// See about:histograms in chromium.
void AbstractDatabase::reportOpenDatabaseResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    DatabaseObserver::reportOpenDatabaseResult(this, errorSite, webSqlErrorCode, sqliteErrorCode);
}

void AbstractDatabase::reportChangeVersionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    DatabaseObserver::reportChangeVersionResult(this, errorSite, webSqlErrorCode, sqliteErrorCode);
}

void AbstractDatabase::reportStartTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    DatabaseObserver::reportStartTransactionResult(this, errorSite, webSqlErrorCode, sqliteErrorCode);
}

void AbstractDatabase::reportCommitTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    DatabaseObserver::reportCommitTransactionResult(this, errorSite, webSqlErrorCode, sqliteErrorCode);
}

void AbstractDatabase::reportExecuteStatementResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    DatabaseObserver::reportExecuteStatementResult(this, errorSite, webSqlErrorCode, sqliteErrorCode);
}

void AbstractDatabase::reportVacuumDatabaseResult(int sqliteErrorCode)
{
    DatabaseObserver::reportVacuumDatabaseResult(this, sqliteErrorCode);
}

#else
void AbstractDatabase::reportOpenDatabaseResult(int, int, int) { }
void AbstractDatabase::reportChangeVersionResult(int, int, int) { }
void AbstractDatabase::reportStartTransactionResult(int, int, int) { }
void AbstractDatabase::reportCommitTransactionResult(int, int, int) { }
void AbstractDatabase::reportExecuteStatementResult(int, int, int) { }
void AbstractDatabase::reportVacuumDatabaseResult(int) { }
#endif // PLATFORM(CHROMIUM)

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
