/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "Database.h"

#include "ChangeVersionData.h"
#include "ChangeVersionWrapper.h"
#include "DOMWindow.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseCallback.h"
#include "DatabaseContext.h"
#include "DatabaseManager.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "JSDOMWindow.h"
#include "Logging.h"
#include "SQLError.h"
#include "SQLTransaction.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionErrorCallback.h"
#include "SQLiteDatabaseTracker.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "VoidCallback.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

// Registering "opened" databases with the DatabaseTracker
// =======================================================
// The DatabaseTracker maintains a list of databases that have been
// "opened" so that the client can call interrupt or delete on every database
// associated with a DatabaseContext.
//
// We will only call DatabaseTracker::addOpenDatabase() to add the database
// to the tracker as opened when we've succeeded in opening the database,
// and will set m_opened to true. Similarly, we only call
// DatabaseTracker::removeOpenDatabase() to remove the database from the
// tracker when we set m_opened to false in closeDatabase(). This sets up
// a simple symmetry between open and close operations, and a direct
// correlation to adding and removing databases from the tracker's list,
// thus ensuring that we have a correct list for the interrupt and
// delete operations to work on.
//
// The only databases instances not tracked by the tracker's open database
// list are the ones that have not been added yet, or the ones that we
// attempted an open on but failed to. Such instances only exist in the
// factory functions for creating database backends.
//
// The factory functions will either call openAndVerifyVersion() or
// performOpenAndVerify(). These methods will add the newly instantiated
// database backend if they succeed in opening the requested database.
// In the case of failure to open the database, the factory methods will
// simply discard the newly instantiated database backend when they return.
// The ref counting mechanims will automatically destruct the un-added
// (and un-returned) databases instances.

static const char versionKey[] = "WebKitDatabaseVersionKey";
static const char unqualifiedInfoTableName[] = "__WebKitDatabaseInfoTable__";
const unsigned long long quotaIncreaseSize = 5 * 1024 * 1024;

static const char* fullyQualifiedInfoTableName()
{
    static const char qualifier[] = "main.";
    static char qualifiedName[sizeof(qualifier) + sizeof(unqualifiedInfoTableName) - 1];

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s%s", qualifier, unqualifiedInfoTableName);
    });

    return qualifiedName;
}

static String formatErrorMessage(const char* message, int sqliteErrorCode, const char* sqliteErrorMessage)
{
    return String::format("%s (%d %s)", message, sqliteErrorCode, sqliteErrorMessage);
}

static bool setTextValueInDatabase(SQLiteDatabase& db, const String& query, const String& value)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement to set value in database (%s)", query.ascii().data());
        return false;
    }

    statement.bindText(1, value);

    result = statement.step();
    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to step statement to set value in database (%s)", query.ascii().data());
        return false;
    }

    return true;
}

static bool retrieveTextResultFromDatabase(SQLiteDatabase& db, const String& query, String& resultString)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLITE_OK) {
        LOG_ERROR("Error (%i) preparing statement to read text result from database (%s)", result, query.ascii().data());
        return false;
    }

    result = statement.step();
    if (result == SQLITE_ROW) {
        resultString = statement.getColumnText(0);
        return true;
    }
    if (result == SQLITE_DONE) {
        resultString = String();
        return true;
    }

    LOG_ERROR("Error (%i) reading text result from database (%s)", result, query.ascii().data());
    return false;
}

// FIXME: move all guid-related functions to a DatabaseVersionTracker class.
static Lock guidMutex;

static HashMap<DatabaseGUID, String>& guidToVersionMap()
{
    static NeverDestroyed<HashMap<DatabaseGUID, String>> map;
    return map;
}

// NOTE: Caller must lock guidMutex().
static inline void updateGUIDVersionMap(DatabaseGUID guid, const String& newVersion)
{
    // Note: It is not safe to put an empty string into the guidToVersionMap() map.
    // That's because the map is cross-thread, but empty strings are per-thread.
    // The copy() function makes a version of the string you can use on the current
    // thread, but we need a string we can keep in a cross-thread data structure.
    // FIXME: This is a quite-awkward restriction to have to program with.

    // Map empty string to null string (see comment above).
    guidToVersionMap().set(guid, newVersion.isEmpty() ? String() : newVersion.isolatedCopy());
}

static HashMap<DatabaseGUID, HashSet<Database*>>& guidToDatabaseMap()
{
    static NeverDestroyed<HashMap<DatabaseGUID, HashSet<Database*>>> map;
    return map;
}

static inline DatabaseGUID guidForOriginAndName(const String& origin, const String& name)
{
    static NeverDestroyed<HashMap<String, DatabaseGUID>> map;
    return map.get().ensure(makeString(origin, '/', name), [] {
        static DatabaseGUID lastUsedGUID;
        return ++lastUsedGUID;
    }).iterator->value;
}

Database::Database(DatabaseContext& context, const String& name, const String& expectedVersion, const String& displayName, unsigned long long estimatedSize)
    : m_scriptExecutionContext(*context.scriptExecutionContext())
    , m_contextThreadSecurityOrigin(m_scriptExecutionContext->securityOrigin()->isolatedCopy())
    , m_databaseThreadSecurityOrigin(m_scriptExecutionContext->securityOrigin()->isolatedCopy())
    , m_databaseContext(context)
    , m_name((name.isNull() ? emptyString() : name).isolatedCopy())
    , m_expectedVersion(expectedVersion.isolatedCopy())
    , m_displayName(displayName.isolatedCopy())
    , m_estimatedSize(estimatedSize)
    , m_filename(DatabaseManager::singleton().fullPathForDatabase(*m_scriptExecutionContext->securityOrigin(), m_name))
    , m_databaseAuthorizer(DatabaseAuthorizer::create(unqualifiedInfoTableName))
{
    {
        std::lock_guard<Lock> locker(guidMutex);

        m_guid = guidForOriginAndName(securityOrigin().securityOrigin()->toString(), name);
        guidToDatabaseMap().ensure(m_guid, [] {
            return HashSet<Database*>();
        }).iterator->value.add(this);
    }

    m_databaseContext->databaseThread();

    ASSERT(m_databaseContext->existingDatabaseThread());
}

DatabaseThread& Database::databaseThread()
{
    ASSERT(m_databaseContext->existingDatabaseThread());
    return *m_databaseContext->existingDatabaseThread();
}

Database::~Database()
{
    // The reference to the ScriptExecutionContext needs to be cleared on the JavaScript thread.  If we're on that thread already, we can just let the RefPtr's destruction do the dereffing.
    if (!m_scriptExecutionContext->isContextThread()) {
        auto passedContext = WTFMove(m_scriptExecutionContext);
        auto& contextRef = passedContext.get();
        contextRef.postTask({ScriptExecutionContext::Task::CleanupTask, [passedContext = WTFMove(passedContext), databaseContext = WTFMove(m_databaseContext)] (ScriptExecutionContext& context) {
            ASSERT_UNUSED(context, &context == passedContext.ptr());
        }});
    }

    // SQLite is "multi-thread safe", but each database handle can only be used
    // on a single thread at a time.
    //
    // For DatabaseBackend, we open the SQLite database on the DatabaseThread,
    // and hence we should also close it on that same thread. This means that the
    // SQLite database need to be closed by another mechanism (see
    // DatabaseContext::stopDatabases()). By the time we get here, the SQLite
    // database should have already been closed.

    ASSERT(!m_opened);
}

ExceptionOr<void> Database::openAndVerifyVersion(bool setVersionInNewDatabase)
{
    DatabaseTaskSynchronizer synchronizer;
    auto& thread = databaseThread();
    if (thread.terminationRequested(&synchronizer))
        return Exception { InvalidStateError };

    ExceptionOr<void> result;
    auto task = std::make_unique<DatabaseOpenTask>(*this, setVersionInNewDatabase, synchronizer, result);
    thread.scheduleImmediateTask(WTFMove(task));
    synchronizer.waitForTaskCompletion();

    return result;
}

void Database::interrupt()
{
    // It is safe to call this from any thread for an opened or closed database.
    m_sqliteDatabase.interrupt();
}

void Database::close()
{
    auto& thread = databaseThread();

    DatabaseTaskSynchronizer synchronizer;
    if (thread.terminationRequested(&synchronizer)) {
        LOG(StorageAPI, "Database handle %p is on a terminated DatabaseThread, cannot be marked for normal closure\n", this);
        return;
    }

    thread.scheduleImmediateTask(std::make_unique<DatabaseCloseTask>(*this, synchronizer));

    // FIXME: iOS depends on this function blocking until the database is closed as part
    // of closing all open databases from a process assertion expiration handler.
    // See <https://bugs.webkit.org/show_bug.cgi?id=157184>.
    synchronizer.waitForTaskCompletion();
}

void Database::performClose()
{
    ASSERT(databaseThread().getThread() == &Thread::current());

    {
        LockHolder locker(m_transactionInProgressMutex);

        // Clean up transactions that have not been scheduled yet:
        // Transaction phase 1 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        while (!m_transactionQueue.isEmpty())
            m_transactionQueue.takeFirst()->notifyDatabaseThreadIsShuttingDown();

        m_isTransactionQueueEnabled = false;
        m_transactionInProgress = false;
    }

    closeDatabase();

    // DatabaseThread keeps databases alive by referencing them in its
    // m_openDatabaseSet. DatabaseThread::recordDatabaseClose() will remove
    // this database from that set (which effectively deref's it). We hold on
    // to it with a local pointer here for a liitle longer, so that we can
    // unschedule any DatabaseTasks that refer to it before the database gets
    // deleted.
    Ref<Database> protectedThis(*this);
    auto& thread = databaseThread();
    thread.recordDatabaseClosed(*this);
    thread.unscheduleDatabaseTasks(*this);
}

class DoneCreatingDatabaseOnExitCaller {
public:
    DoneCreatingDatabaseOnExitCaller(Database& database)
        : m_database(database)
    {
    }

    ~DoneCreatingDatabaseOnExitCaller()
    {
        DatabaseTracker::singleton().doneCreatingDatabase(m_database);
    }

private:
    Database& m_database;
};

ExceptionOr<void> Database::performOpenAndVerify(bool shouldSetVersionInNewDatabase)
{
    DoneCreatingDatabaseOnExitCaller onExitCaller(*this);

    const int maxSqliteBusyWaitTime = 30000;

#if PLATFORM(IOS_FAMILY)
    {
        // Make sure we wait till the background removal of the empty database files finished before trying to open any database.
        auto locker = holdLock(DatabaseTracker::openDatabaseMutex());
    }
#endif

    SQLiteTransactionInProgressAutoCounter transactionCounter;

    if (!m_sqliteDatabase.open(m_filename, true))
        return Exception { InvalidStateError, formatErrorMessage("unable to open database", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg()) };
    if (!m_sqliteDatabase.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum (%d %s)", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());

    m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);

    String currentVersion;
    {
        std::lock_guard<Lock> locker(guidMutex);

        auto entry = guidToVersionMap().find(m_guid);
        if (entry != guidToVersionMap().end()) {
            // Map null string to empty string (see updateGUIDVersionMap()).
            currentVersion = entry->value.isNull() ? emptyString() : entry->value.isolatedCopy();
            LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());
        } else {
            LOG(StorageAPI, "No cached version for guid %i", m_guid);

            SQLiteTransaction transaction(m_sqliteDatabase);
            transaction.begin();
            if (!transaction.inProgress()) {
                String message = formatErrorMessage("unable to open database, failed to start transaction", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                m_sqliteDatabase.close();
                return Exception { InvalidStateError, WTFMove(message) };
            }

            String tableName(unqualifiedInfoTableName);
            if (!m_sqliteDatabase.tableExists(tableName)) {
                m_new = true;

                if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + tableName + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
                    String message = formatErrorMessage("unable to open database, failed to create 'info' table", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                    transaction.rollback();
                    m_sqliteDatabase.close();
                return Exception { InvalidStateError, WTFMove(message) };
                }
            } else if (!getVersionFromDatabase(currentVersion, false)) {
                String message = formatErrorMessage("unable to open database, failed to read current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                transaction.rollback();
                m_sqliteDatabase.close();
                return Exception { InvalidStateError, WTFMove(message) };
            }

            if (currentVersion.length()) {
                LOG(StorageAPI, "Retrieved current version %s from database %s", currentVersion.ascii().data(), databaseDebugName().ascii().data());
            } else if (!m_new || shouldSetVersionInNewDatabase) {
                LOG(StorageAPI, "Setting version %s in database %s that was just created", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                if (!setVersionInDatabase(m_expectedVersion, false)) {
                    String message = formatErrorMessage("unable to open database, failed to write current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return Exception { InvalidStateError, WTFMove(message) };
                }
                currentVersion = m_expectedVersion;
            }
            updateGUIDVersionMap(m_guid, currentVersion);
            transaction.commit();
        }
    }

    if (currentVersion.isNull()) {
        LOG(StorageAPI, "Database %s does not have its version set", databaseDebugName().ascii().data());
        currentVersion = emptyString();
    }

    // If the expected version isn't the empty string, ensure that the current database version we have matches that version. Otherwise, set an exception.
    // If the expected version is the empty string, then we always return with whatever version of the database we have.
    if ((!m_new || shouldSetVersionInNewDatabase) && m_expectedVersion.length() && m_expectedVersion != currentVersion) {
        m_sqliteDatabase.close();
        return Exception { InvalidStateError, "unable to open database, version mismatch, '" + m_expectedVersion + "' does not match the currentVersion of '" + currentVersion + "'" };
    }

    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer.get());

    DatabaseTracker::singleton().addOpenDatabase(*this);
    m_opened = true;

    if (m_new && !shouldSetVersionInNewDatabase)
        m_expectedVersion = emptyString(); // The caller provided a creationCallback which will set the expected version.

    databaseThread().recordDatabaseOpen(*this);

    return { };
}

void Database::closeDatabase()
{
    if (!m_opened)
        return;

    m_sqliteDatabase.close();
    m_opened = false;

    // See comment at the top this file regarding calling removeOpenDatabase().
    DatabaseTracker::singleton().removeOpenDatabase(*this);

    {
        std::lock_guard<Lock> locker(guidMutex);

        auto it = guidToDatabaseMap().find(m_guid);
        ASSERT(it != guidToDatabaseMap().end());
        ASSERT(it->value.contains(this));
        it->value.remove(this);
        if (it->value.isEmpty()) {
            guidToDatabaseMap().remove(it);
            guidToVersionMap().remove(m_guid);
        }
    }
}

bool Database::getVersionFromDatabase(String& version, bool shouldCacheVersion)
{
    String query(String("SELECT value FROM ") + fullyQualifiedInfoTableName() +  " WHERE key = '" + versionKey + "';");

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

bool Database::setVersionInDatabase(const String& version, bool shouldCacheVersion)
{
    // The INSERT will replace an existing entry for the database with the new version number, due to the UNIQUE ON CONFLICT REPLACE
    // clause in the CREATE statement (see Database::performOpenAndVerify()).
    String query(String("INSERT INTO ") + fullyQualifiedInfoTableName() +  " (key, value) VALUES ('" + versionKey + "', ?);");

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

void Database::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.isolatedCopy();
}

String Database::getCachedVersion() const
{
    std::lock_guard<Lock> locker(guidMutex);

    return guidToVersionMap().get(m_guid).isolatedCopy();
}

void Database::setCachedVersion(const String& actualVersion)
{
    std::lock_guard<Lock> locker(guidMutex);

    updateGUIDVersionMap(m_guid, actualVersion);
}

bool Database::getActualVersionForTransaction(String &actualVersion)
{
    ASSERT(m_sqliteDatabase.transactionInProgress());

    // Note: In multi-process browsers the cached value may be inaccurate.
    // So we retrieve the value from the database and update the cached value here.
    return getVersionFromDatabase(actualVersion, true);
}

void Database::scheduleTransaction()
{
    ASSERT(!m_transactionInProgressMutex.tryLock()); // Locked by caller.

    if (!m_isTransactionQueueEnabled || m_transactionQueue.isEmpty()) {
        m_transactionInProgress = false;
        return;
    }

    m_transactionInProgress = true;

    auto transaction = m_transactionQueue.takeFirst();
    auto task = std::make_unique<DatabaseTransactionTask>(WTFMove(transaction));
    LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for transaction %p\n", task.get(), task->transaction());
    databaseThread().scheduleTask(WTFMove(task));
}

void Database::scheduleTransactionStep(SQLTransaction& transaction)
{
    auto& thread = databaseThread();

    auto task = std::make_unique<DatabaseTransactionTask>(&transaction);
    LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for the transaction step\n", task.get());
    thread.scheduleTask(WTFMove(task));
}

void Database::inProgressTransactionCompleted()
{
    LockHolder locker(m_transactionInProgressMutex);
    m_transactionInProgress = false;
    scheduleTransaction();
}

bool Database::hasPendingTransaction()
{
    LockHolder locker(m_transactionInProgressMutex);
    return m_transactionInProgress || !m_transactionQueue.isEmpty();
}

SQLTransactionCoordinator* Database::transactionCoordinator()
{
    return databaseThread().transactionCoordinator();
}

String Database::version() const
{
    if (m_deleted)
        return String();

    // Note: In multi-process browsers the cached value may be accurate, but we cannot read the
    // actual version from the database without potentially inducing a deadlock.
    // FIXME: Add an async version getter to the DatabaseAPI.
    return getCachedVersion();
}

void Database::markAsDeletedAndClose()
{
    if (m_deleted)
        return;

    LOG(StorageAPI, "Marking %s (%p) as deleted", stringIdentifier().ascii().data(), this);
    m_deleted = true;

    close();
}

void Database::changeVersion(const String& oldVersion, const String& newVersion, RefPtr<SQLTransactionCallback>&& callback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<VoidCallback>&& successCallback)
{
    runTransaction(WTFMove(callback), WTFMove(errorCallback), WTFMove(successCallback), ChangeVersionWrapper::create(oldVersion, newVersion), false);
}

void Database::transaction(RefPtr<SQLTransactionCallback>&& callback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<VoidCallback>&& successCallback)
{
    runTransaction(WTFMove(callback), WTFMove(errorCallback), WTFMove(successCallback), nullptr, false);
}

void Database::readTransaction(RefPtr<SQLTransactionCallback>&& callback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<VoidCallback>&& successCallback)
{
    runTransaction(WTFMove(callback), WTFMove(errorCallback), WTFMove(successCallback), nullptr, true);
}

String Database::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.isolatedCopy();
}

String Database::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.isolatedCopy();
}

unsigned long long Database::estimatedSize() const
{
    return m_estimatedSize;
}

void Database::setEstimatedSize(unsigned long long estimatedSize)
{
    m_estimatedSize = estimatedSize;
    DatabaseTracker::singleton().setDatabaseDetails(securityOrigin(), m_name, m_displayName, m_estimatedSize);
}

String Database::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.isolatedCopy();
}

DatabaseDetails Database::details() const
{
    // This code path is only used for database quota delegate calls, so file dates are irrelevant and left uninitialized.
    return DatabaseDetails(stringIdentifier(), displayName(), estimatedSize(), 0, 0, 0);
}

void Database::disableAuthorizer()
{
    m_databaseAuthorizer->disable();
}

void Database::enableAuthorizer()
{
    m_databaseAuthorizer->enable();
}

void Database::setAuthorizerPermissions(int permissions)
{
    m_databaseAuthorizer->setPermissions(permissions);
}

bool Database::lastActionChangedDatabase()
{
    return m_databaseAuthorizer->lastActionChangedDatabase();
}

bool Database::lastActionWasInsert()
{
    return m_databaseAuthorizer->lastActionWasInsert();
}

void Database::resetDeletes()
{
    m_databaseAuthorizer->resetDeletes();
}

bool Database::hadDeletes()
{
    return m_databaseAuthorizer->hadDeletes();
}

void Database::resetAuthorizer()
{
    m_databaseAuthorizer->reset();
}

void Database::runTransaction(RefPtr<SQLTransactionCallback>&& callback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionWrapper>&& wrapper, bool readOnly)
{
    LockHolder locker(m_transactionInProgressMutex);
    if (!m_isTransactionQueueEnabled) {
        if (errorCallback) {
            RefPtr<SQLTransactionErrorCallback> errorCallbackProtector = WTFMove(errorCallback);
            m_scriptExecutionContext->postTask([errorCallbackProtector](ScriptExecutionContext&) {
                errorCallbackProtector->handleEvent(SQLError::create(SQLError::UNKNOWN_ERR, "database has been closed"));
            });
        }
        return;
    }

    m_transactionQueue.append(SQLTransaction::create(*this, WTFMove(callback), WTFMove(successCallback), errorCallback.copyRef(), WTFMove(wrapper), readOnly));
    if (!m_transactionInProgress)
        scheduleTransaction();
}

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{
    RefPtr<SQLTransaction> transactionProtector(transaction);
    m_scriptExecutionContext->postTask([transactionProtector] (ScriptExecutionContext&) {
        transactionProtector->performPendingCallback();
    });
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(sqliteDatabase(), "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to retrieve list of tables for database %s", databaseDebugName().ascii().data());
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLITE_ROW) {
        String name = statement.getColumnText(0);
        if (name != unqualifiedInfoTableName)
            tableNames.append(name);
    }

    enableAuthorizer();

    if (result != SQLITE_DONE) {
        LOG_ERROR("Error getting tables for database %s", databaseDebugName().ascii().data());
        return Vector<String>();
    }

    return tableNames;
}

void Database::incrementalVacuumIfNeeded()
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    int64_t freeSpaceSize = m_sqliteDatabase.freeSpaceSize();
    int64_t totalSize = m_sqliteDatabase.totalSize();
    if (totalSize <= 10 * freeSpaceSize) {
        int result = m_sqliteDatabase.runIncrementalVacuumCommand();
        if (result != SQLITE_OK)
            logErrorMessage(formatErrorMessage("error vacuuming database", result, m_sqliteDatabase.lastErrorMsg()));
    }
}

void Database::logErrorMessage(const String& message)
{
    m_scriptExecutionContext->addConsoleMessage(MessageSource::Storage, MessageLevel::Error, message);
}

Vector<String> Database::tableNames()
{
    // FIXME: Not using isolatedCopy on these strings looks ok since threads take strict turns
    // in dealing with them. However, if the code changes, this may not be true anymore.
    Vector<String> result;
    DatabaseTaskSynchronizer synchronizer;
    auto& thread = databaseThread();
    if (thread.terminationRequested(&synchronizer))
        return result;

    auto task = std::make_unique<DatabaseTableNamesTask>(*this, synchronizer, result);
    thread.scheduleImmediateTask(WTFMove(task));
    synchronizer.waitForTaskCompletion();

    return result;
}

SecurityOriginData Database::securityOrigin()
{
    if (m_scriptExecutionContext->isContextThread())
        return m_contextThreadSecurityOrigin->data();
    if (databaseThread().getThread() == &Thread::current())
        return m_databaseThreadSecurityOrigin->data();
    RELEASE_ASSERT_NOT_REACHED();
}

unsigned long long Database::maximumSize()
{
    return DatabaseTracker::singleton().maximumSize(*this);
}

void Database::didCommitWriteTransaction()
{
    DatabaseTracker::singleton().scheduleNotifyDatabaseChanged(securityOrigin(), stringIdentifier());
}

bool Database::didExceedQuota()
{
    ASSERT(databaseContext().scriptExecutionContext()->isContextThread());
    auto& tracker = DatabaseTracker::singleton();
    auto oldQuota = tracker.quota(securityOrigin());
    if (estimatedSize() <= oldQuota) {
        // The expected usage provided by the page is now smaller than the actual database size so we bump the expected usage to
        // oldQuota + 5MB so that the client actually increases the quota.
        setEstimatedSize(oldQuota + quotaIncreaseSize);
    }
    databaseContext().databaseExceededQuota(stringIdentifier(), details());
    return tracker.quota(securityOrigin()) > oldQuota;
}

#if !LOG_DISABLED || !ERROR_DISABLED

String Database::databaseDebugName() const
{
    return m_contextThreadSecurityOrigin->toString() + "::" + m_name;
}

#endif

} // namespace WebCore
