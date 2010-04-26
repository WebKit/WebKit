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
#include "Database.h"

#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "ChangeVersionWrapper.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseCallback.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "Page.h"
#include "OriginQuotaManager.h"
#include "ScriptController.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "SQLResultSet.h"
#include "SQLTransactionClient.h"
#include "SQLTransactionCoordinator.h"

#endif // ENABLE(DATABASE)

#if USE(JSC)
#include "JSDOMWindow.h"
#endif

namespace WebCore {

// If we sleep for more the 30 seconds while blocked on SQLITE_BUSY, give up.
static const int maxSqliteBusyWaitTime = 30000;

const String& Database::databaseInfoTableName()
{
    DEFINE_STATIC_LOCAL(String, name, ("__WebKitDatabaseInfoTable__"));
    return name;
}

#if ENABLE(DATABASE)

static bool isDatabaseAvailable = true;

void Database::setIsAvailable(bool available)
{
    isDatabaseAvailable = available;
}

bool Database::isAvailable()
{
    return isDatabaseAvailable;
}

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

typedef HashMap<int, HashSet<Database*>*> GuidDatabaseMap;
static GuidDatabaseMap& guidToDatabaseMap()
{
    DEFINE_STATIC_LOCAL(GuidDatabaseMap, map, ());
    return map;
}

static const String& databaseVersionKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebKitDatabaseVersionKey"));
    return key;
}

static int guidForOriginAndName(const String& origin, const String& name);

class DatabaseCreationCallbackTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DatabaseCreationCallbackTask> create(PassRefPtr<Database> database)
    {
        return new DatabaseCreationCallbackTask(database);
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_database->performCreationCallback();
    }

private:
    DatabaseCreationCallbackTask(PassRefPtr<Database> database)
        : m_database(database)
    {
    }

    RefPtr<Database> m_database;
};

PassRefPtr<Database> Database::openDatabase(ScriptExecutionContext* context, const String& name,
                                            const String& expectedVersion, const String& displayName,
                                            unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback,
                                            ExceptionCode& e)
{
    if (!DatabaseTracker::tracker().canEstablishDatabase(context, name, displayName, estimatedSize)) {
        // FIXME: There should be an exception raised here in addition to returning a null Database object.  The question has been raised with the WHATWG.
        LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(), context->securityOrigin()->toString().ascii().data());
        return 0;
    }

    RefPtr<Database> database = adoptRef(new Database(context, name, expectedVersion, displayName, estimatedSize, creationCallback));

    if (!database->openAndVerifyVersion(e)) {
        LOG(StorageAPI, "Failed to open and verify version (expected %s) of database %s", expectedVersion.ascii().data(), database->databaseDebugName().ascii().data());
        context->removeOpenDatabase(database.get());
        DatabaseTracker::tracker().removeOpenDatabase(database.get());
        return 0;
    }

    DatabaseTracker::tracker().setDatabaseDetails(context->securityOrigin(), name, displayName, estimatedSize);

    context->setHasOpenDatabases();
#if ENABLE(INSPECTOR)
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        if (Page* page = document->page())
            page->inspectorController()->didOpenDatabase(database.get(), context->securityOrigin()->host(), name, expectedVersion);
    }
#endif

    // If it's a new database and a creation callback was provided, reset the expected
    // version to "" and schedule the creation callback. Because of some subtle String
    // implementation issues, we have to reset m_expectedVersion here instead of doing
    // it inside performOpenAndVerify() which is run on the DB thread.
    if (database->isNew() && database->m_creationCallback.get()) {
        database->m_expectedVersion = "";
        LOG(StorageAPI, "Scheduling DatabaseCreationCallbackTask for database %p\n", database.get());
        database->m_scriptExecutionContext->postTask(DatabaseCreationCallbackTask::create(database));
    }

    return database;
}

Database::Database(ScriptExecutionContext* context, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback)
    : m_transactionInProgress(false)
    , m_isTransactionQueueEnabled(true)
    , m_scriptExecutionContext(context)
    , m_name(name.crossThreadString())
    , m_guid(0)
    , m_expectedVersion(expectedVersion.crossThreadString())
    , m_displayName(displayName.crossThreadString())
    , m_estimatedSize(estimatedSize)
    , m_deleted(false)
    , m_stopped(false)
    , m_opened(false)
    , m_new(false)
    , m_creationCallback(creationCallback)
{
    ASSERT(m_scriptExecutionContext.get());
    m_mainThreadSecurityOrigin = m_scriptExecutionContext->securityOrigin();
    m_databaseThreadSecurityOrigin = m_mainThreadSecurityOrigin->threadsafeCopy();
    if (m_name.isNull())
        m_name = "";

    ScriptController::initializeThreading();

    m_guid = guidForOriginAndName(securityOrigin()->toString(), name);

    {
        MutexLocker locker(guidMutex());

        HashSet<Database*>* hashSet = guidToDatabaseMap().get(m_guid);
        if (!hashSet) {
            hashSet = new HashSet<Database*>;
            guidToDatabaseMap().set(m_guid, hashSet);
        }

        hashSet->add(this);
    }

    ASSERT(m_scriptExecutionContext->databaseThread());
    m_filename = DatabaseTracker::tracker().fullPathForDatabase(securityOrigin(), m_name);

    DatabaseTracker::tracker().addOpenDatabase(this);
    context->addOpenDatabase(this);
}

class DerefContextTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DerefContextTask> create()
    {
        return new DerefContextTask();
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        context->deref();
    }

    virtual bool isCleanupTask() const { return true; }
};

Database::~Database()
{
    // The reference to the ScriptExecutionContext needs to be cleared on the JavaScript thread.  If we're on that thread already, we can just let the RefPtr's destruction do the dereffing.
    if (!m_scriptExecutionContext->isContextThread()) {
        m_scriptExecutionContext->postTask(DerefContextTask::create());
        m_scriptExecutionContext.release().releaseRef();
    }
}

bool Database::openAndVerifyVersion(ExceptionCode& e)
{
    if (!m_scriptExecutionContext->databaseThread())
        return false;
    m_databaseAuthorizer = DatabaseAuthorizer::create();

    bool success = false;
    DatabaseTaskSynchronizer synchronizer;
    OwnPtr<DatabaseOpenTask> task = DatabaseOpenTask::create(this, &synchronizer, e, success);

    m_scriptExecutionContext->databaseThread()->scheduleImmediateTask(task.release());
    synchronizer.waitForTaskCompletion();

    return success;
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
    } else if (result == SQLResultDone) {
        resultString = String();
        return true;
    } else {
        LOG_ERROR("Error (%i) reading text result from database (%s)", result, query.ascii().data());
        return false;
    }
}

bool Database::getVersionFromDatabase(String& version)
{
    DEFINE_STATIC_LOCAL(String, getVersionQuery, ("SELECT value FROM " + databaseInfoTableName() + " WHERE key = '" + databaseVersionKey() + "';"));

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_sqliteDatabase, getVersionQuery.threadsafeCopy(), version);
    if (!result)
        LOG_ERROR("Failed to retrieve version from database %s", databaseDebugName().ascii().data());

    m_databaseAuthorizer->enable();

    return result;
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

bool Database::setVersionInDatabase(const String& version)
{
    // The INSERT will replace an existing entry for the database with the new version number, due to the UNIQUE ON CONFLICT REPLACE
    // clause in the CREATE statement (see Database::performOpenAndVerify()).
    DEFINE_STATIC_LOCAL(String, setVersionQuery, ("INSERT INTO " + databaseInfoTableName() + " (key, value) VALUES ('" + databaseVersionKey() + "', ?);"));

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_sqliteDatabase, setVersionQuery.threadsafeCopy(), version);
    if (!result)
        LOG_ERROR("Failed to set version %s in database (%s)", version.ascii().data(), setVersionQuery.ascii().data());

    m_databaseAuthorizer->enable();

    return result;
}

bool Database::versionMatchesExpected() const
{
    if (!m_expectedVersion.isEmpty()) {
        MutexLocker locker(guidMutex());
        return m_expectedVersion == guidToVersionMap().get(m_guid);
    }

    return true;
}

void Database::markAsDeletedAndClose()
{
    if (m_deleted || !m_scriptExecutionContext->databaseThread())
        return;

    LOG(StorageAPI, "Marking %s (%p) as deleted", stringIdentifier().ascii().data(), this);
    m_deleted = true;

    if (m_scriptExecutionContext->databaseThread()->terminationRequested()) {
        LOG(StorageAPI, "Database handle %p is on a terminated DatabaseThread, cannot be marked for normal closure\n", this);
        return;
    }

    m_scriptExecutionContext->databaseThread()->unscheduleDatabaseTasks(this);

    DatabaseTaskSynchronizer synchronizer;
    OwnPtr<DatabaseCloseTask> task = DatabaseCloseTask::create(this, DoNotRemoveDatabaseFromContext, &synchronizer);

    m_scriptExecutionContext->databaseThread()->scheduleImmediateTask(task.release());
    synchronizer.waitForTaskCompletion();

    // DatabaseCloseTask tells Database::close not to do these two removals so that we can do them here synchronously.
    m_scriptExecutionContext->removeOpenDatabase(this);
    DatabaseTracker::tracker().removeOpenDatabase(this);
}

class ContextRemoveOpenDatabaseTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<ContextRemoveOpenDatabaseTask> create(PassRefPtr<Database> database)
    {
        return new ContextRemoveOpenDatabaseTask(database);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        context->removeOpenDatabase(m_database.get());
        DatabaseTracker::tracker().removeOpenDatabase(m_database.get());
    }

    virtual bool isCleanupTask() const { return true; }

private:
    ContextRemoveOpenDatabaseTask(PassRefPtr<Database> database)
        : m_database(database)
    {
    }

    RefPtr<Database> m_database;
};

void Database::close(ClosePolicy policy)
{
    RefPtr<Database> protect = this;

    if (!m_opened)
        return;

    ASSERT(m_scriptExecutionContext->databaseThread());
    ASSERT(currentThread() == m_scriptExecutionContext->databaseThread()->getThreadID());
    m_sqliteDatabase.close();
    // Must ref() before calling databaseThread()->recordDatabaseClosed().
    m_scriptExecutionContext->databaseThread()->recordDatabaseClosed(this);
    m_opened = false;

    {
        MutexLocker locker(guidMutex());

        HashSet<Database*>* hashSet = guidToDatabaseMap().get(m_guid);
        ASSERT(hashSet);
        ASSERT(hashSet->contains(this));
        hashSet->remove(this);
        if (hashSet->isEmpty()) {
            guidToDatabaseMap().remove(m_guid);
            delete hashSet;
            guidToVersionMap().remove(m_guid);
        }
    }

    m_scriptExecutionContext->databaseThread()->unscheduleDatabaseTasks(this);
    if (policy == RemoveDatabaseFromContext)
        m_scriptExecutionContext->postTask(ContextRemoveOpenDatabaseTask::create(this));
}

void Database::stop()
{
    // FIXME: The net effect of the following code is to remove all pending transactions and statements, but allow the current statement
    // to run to completion.  In the future we can use the sqlite3_progress_handler or sqlite3_interrupt interfaces to cancel the current
    // statement in response to close(), as well.

    // This method is meant to be used as an analog to cancelling a loader, and is used when a document is shut down as the result of
    // a page load or closing the page
    m_stopped = true;

    {
        MutexLocker locker(m_transactionInProgressMutex);
        m_isTransactionQueueEnabled = false;
        m_transactionInProgress = false;
    }
}

unsigned long long Database::maximumSize() const
{
    return DatabaseTracker::tracker().getMaxSizeForDatabase(this);
}

void Database::disableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->disable();
}

void Database::enableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->enable();
}

void Database::setAuthorizerReadOnly()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->setReadOnly();
}

static int guidForOriginAndName(const String& origin, const String& name)
{
    String stringID;
    if (origin.endsWith("/"))
        stringID = origin + name;
    else
        stringID = origin + "/" + name;

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

void Database::resetAuthorizer()
{
    if (m_databaseAuthorizer)
        m_databaseAuthorizer->reset();
}

void Database::performPolicyChecks()
{
    // FIXME: Code similar to the following will need to be run to enforce the per-origin size limit the spec mandates.
    // Additionally, we might need a way to pause the database thread while the UA prompts the user for permission to
    // increase the size limit

    /*
    if (m_databaseAuthorizer->lastActionIncreasedSize())
        DatabaseTracker::scheduleFileSizeCheckOnMainThread(this);
    */

    notImplemented();
}

bool Database::performOpenAndVerify(ExceptionCode& e)
{
    if (!m_sqliteDatabase.open(m_filename, true)) {
        LOG_ERROR("Unable to open database at path %s", m_filename.ascii().data());
        e = INVALID_STATE_ERR;
        return false;
    }
    if (!m_sqliteDatabase.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum for database %s", m_filename.ascii().data());

    ASSERT(m_databaseAuthorizer);
    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer);
    m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);

    String currentVersion;
    {
        MutexLocker locker(guidMutex());

        GuidVersionMap::iterator entry = guidToVersionMap().find(m_guid);
        if (entry != guidToVersionMap().end()) {
            // Map null string to empty string (see updateGuidVersionMap()).
            currentVersion = entry->second.isNull() ? String("") : entry->second;
            LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());
        } else {
            LOG(StorageAPI, "No cached version for guid %i", m_guid);

            if (!m_sqliteDatabase.tableExists(databaseInfoTableName())) {
                m_new = true;

                if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + databaseInfoTableName() + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
                    LOG_ERROR("Unable to create table %s in database %s", databaseInfoTableName().ascii().data(), databaseDebugName().ascii().data());
                    e = INVALID_STATE_ERR;
                    // Close the handle to the database file.
                    m_sqliteDatabase.close();
                    return false;
                }
            }

            if (!getVersionFromDatabase(currentVersion)) {
                LOG_ERROR("Failed to get current version from database %s", databaseDebugName().ascii().data());
                e = INVALID_STATE_ERR;
                // Close the handle to the database file.
                m_sqliteDatabase.close();
                return false;
            }
            if (currentVersion.length()) {
                LOG(StorageAPI, "Retrieved current version %s from database %s", currentVersion.ascii().data(), databaseDebugName().ascii().data());
            } else if (!m_new || !m_creationCallback) {
                LOG(StorageAPI, "Setting version %s in database %s that was just created", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                if (!setVersionInDatabase(m_expectedVersion)) {
                    LOG_ERROR("Failed to set version %s in database %s", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                    e = INVALID_STATE_ERR;
                    // Close the handle to the database file.
                    m_sqliteDatabase.close();
                    return false;
                }
                currentVersion = m_expectedVersion;
            }

            updateGuidVersionMap(m_guid, currentVersion);
        }
    }

    if (currentVersion.isNull()) {
        LOG(StorageAPI, "Database %s does not have its version set", databaseDebugName().ascii().data());
        currentVersion = "";
    }

    // If the expected version isn't the empty string, ensure that the current database version we have matches that version. Otherwise, set an exception.
    // If the expected version is the empty string, then we always return with whatever version of the database we have.
    if ((!m_new || !m_creationCallback) && m_expectedVersion.length() && m_expectedVersion != currentVersion) {
        LOG(StorageAPI, "page expects version %s from database %s, which actually has version name %s - openDatabase() call will fail", m_expectedVersion.ascii().data(),
            databaseDebugName().ascii().data(), currentVersion.ascii().data());
        e = INVALID_STATE_ERR;
        // Close the handle to the database file.
        m_sqliteDatabase.close();
        return false;
    }

    // All checks passed and we still have a handle to this database file.
    // Make sure DatabaseThread closes it when DatabaseThread goes away.
    m_opened = true;
    if (m_scriptExecutionContext->databaseThread())
        m_scriptExecutionContext->databaseThread()->recordDatabaseOpen(this);

    return true;
}

void Database::changeVersion(const String& oldVersion, const String& newVersion,
                             PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                             PassRefPtr<VoidCallback> successCallback)
{
    m_transactionQueue.append(SQLTransaction::create(this, callback, errorCallback, successCallback, ChangeVersionWrapper::create(oldVersion, newVersion)));
    MutexLocker locker(m_transactionInProgressMutex);
    if (!m_transactionInProgress)
        scheduleTransaction();
}

void Database::transaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                           PassRefPtr<VoidCallback> successCallback, bool readOnly)
{
    m_transactionQueue.append(SQLTransaction::create(this, callback, errorCallback, successCallback, 0, readOnly));
    MutexLocker locker(m_transactionInProgressMutex);
    if (!m_transactionInProgress)
        scheduleTransaction();
}

void Database::scheduleTransaction()
{
    ASSERT(!m_transactionInProgressMutex.tryLock()); // Locked by caller.
    RefPtr<SQLTransaction> transaction;

    if (m_isTransactionQueueEnabled && !m_transactionQueue.isEmpty()) {
        transaction = m_transactionQueue.first();
        m_transactionQueue.removeFirst();
    }

    if (transaction && m_scriptExecutionContext->databaseThread()) {
        OwnPtr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
        LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for transaction %p\n", task.get(), task->transaction());
        m_transactionInProgress = true;
        m_scriptExecutionContext->databaseThread()->scheduleTask(task.release());
    } else
        m_transactionInProgress = false;
}

void Database::scheduleTransactionStep(SQLTransaction* transaction, bool immediately)
{
    if (!m_scriptExecutionContext->databaseThread())
        return;

    OwnPtr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
    LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for the transaction step\n", task.get());
    if (immediately)
        m_scriptExecutionContext->databaseThread()->scheduleImmediateTask(task.release());
    else
        m_scriptExecutionContext->databaseThread()->scheduleTask(task.release());
}

class DeliverPendingCallbackTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DeliverPendingCallbackTask> create(PassRefPtr<SQLTransaction> transaction)
    {
        return new DeliverPendingCallbackTask(transaction);
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_transaction->performPendingCallback();
    }

private:
    DeliverPendingCallbackTask(PassRefPtr<SQLTransaction> transaction)
        : m_transaction(transaction)
    {
    }

    RefPtr<SQLTransaction> m_transaction;
};

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{
    m_scriptExecutionContext->postTask(DeliverPendingCallbackTask::create(transaction));
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(m_sqliteDatabase, "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to retrieve list of tables for database %s", databaseDebugName().ascii().data());
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLResultRow) {
        String name = statement.getColumnText(0);
        if (name != databaseInfoTableName())
            tableNames.append(name);
    }

    enableAuthorizer();

    if (result != SQLResultDone) {
        LOG_ERROR("Error getting tables for database %s", databaseDebugName().ascii().data());
        return Vector<String>();
    }

    return tableNames;
}

void Database::performCreationCallback()
{
    m_creationCallback->handleEvent(m_scriptExecutionContext.get(), this);
}

SQLTransactionClient* Database::transactionClient() const
{
    return m_scriptExecutionContext->databaseThread()->transactionClient();
}

SQLTransactionCoordinator* Database::transactionCoordinator() const
{
    return m_scriptExecutionContext->databaseThread()->transactionCoordinator();
}

String Database::version() const
{
    if (m_deleted)
        return String();
    MutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).threadsafeCopy();
}

Vector<String> Database::tableNames()
{
    // FIXME: Not using threadsafeCopy on these strings looks ok since threads take strict turns
    // in dealing with them. However, if the code changes, this may not be true anymore.
    Vector<String> result;
    if (!m_scriptExecutionContext->databaseThread())
        return result;

    DatabaseTaskSynchronizer synchronizer;
    OwnPtr<DatabaseTableNamesTask> task = DatabaseTableNamesTask::create(this, &synchronizer, result);

    m_scriptExecutionContext->databaseThread()->scheduleImmediateTask(task.release());
    synchronizer.waitForTaskCompletion();

    return result;
}

void Database::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.threadsafeCopy();
    // Update the in memory database version map.
    MutexLocker locker(guidMutex());
    updateGuidVersionMap(m_guid, version);
}

SecurityOrigin* Database::securityOrigin() const
{
    if (scriptExecutionContext()->isContextThread())
        return m_mainThreadSecurityOrigin.get();
    if (currentThread() == m_scriptExecutionContext->databaseThread()->getThreadID())
        return m_databaseThreadSecurityOrigin.get();
    return 0;
}

String Database::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.threadsafeCopy();
}

String Database::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.threadsafeCopy();
}

unsigned long Database::estimatedSize() const
{
    return m_estimatedSize;
}

String Database::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.threadsafeCopy();
}

void Database::incrementalVacuumIfNeeded()
{
    int64_t freeSpaceSize = m_sqliteDatabase.freeSpaceSize();
    int64_t totalSize = m_sqliteDatabase.totalSize();
    if (totalSize <= 10 * freeSpaceSize)
        m_sqliteDatabase.runIncrementalVacuumCommand();
}

#endif // ENABLE(DATABASE)

} // namespace WebCore
