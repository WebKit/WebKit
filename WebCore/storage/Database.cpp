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
#include "Database.h"

#include "CString.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseCallback.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FileSystem.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "Page.h"
#include "SQLCallback.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLResultSet.h"
#include "VersionChangeCallback.h"

namespace WebCore {

Mutex& Database::globalCallbackMutex()
{
    static Mutex mutex;
    return mutex;
}

HashSet<RefPtr<Database> >& Database::globalCallbackSet()
{
    static HashSet<RefPtr<Database> > set;
    return set;
}

bool Database::s_globalCallbackScheduled = false;

Mutex& Database::guidMutex()
{
    static Mutex mutex;
    return mutex;
}

HashMap<int, String>& Database::guidToVersionMap()
{
    static HashMap<int, String> map;
    return map;
}

HashMap<int, HashSet<Database*>*>& Database::guidToDatabaseMap()
{
    static HashMap<int, HashSet<Database*>*> map;
    return map;
}

const String& Database::databaseInfoTableName()
{
    static String name = "__WebKitDatabaseInfoTable__";
    return name;
}

static const String& databaseVersionKey()
{
    static String key = "WebKitDatabaseVersionKey";
    return key;
}

PassRefPtr<Database> Database::openDatabase(Document* document, const String& name, const String& expectedVersion, ExceptionCode& e)
{
    RefPtr<Database> database = new Database(document, name, expectedVersion);

    if (!database->openAndVerifyVersion(e)) {
       LOG(StorageAPI, "Failed to open and verify version (expected %s) of database %s", expectedVersion.ascii().data(), database->databaseDebugName().ascii().data());
       return 0;
    }

    if (Page* page = document->frame()->page())
        page->inspectorController()->didOpenDatabase(database.get(), document->securityOrigin().toString(), name, expectedVersion);

    return database;
}

Database::Database(Document* document, const String& name, const String& expectedVersion)
    : m_document(document)
    , m_name(name.copy())
    , m_guid(0)
    , m_expectedVersion(expectedVersion)
    , m_databaseThread(0)
{
    ASSERT(document);

    // FIXME: Right now, this is the entire domain of the document, when it really needs to be scheme/host/port only
    m_securityOrigin = document->securityOrigin();

    if (m_name.isNull())
        m_name = "";

    initializeThreading();

    m_guid = guidForOriginAndName(m_securityOrigin.toString(), name);

    {
        MutexLocker locker(guidMutex());

        HashSet<Database*>* hashSet = guidToDatabaseMap().get(m_guid);
        if (!hashSet) {
            hashSet = new HashSet<Database*>;
            guidToDatabaseMap().set(m_guid, hashSet);
        }

        hashSet->add(this);
    }

    {
        MutexLocker locker(m_databaseThreadMutex);
        m_databaseThread = document->databaseThread();
        ASSERT(m_databaseThread);
    }

    m_filename = DatabaseTracker::tracker().fullPathForDatabase(m_securityOrigin.toString(), m_name);
}

Database::~Database()
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

bool Database::openAndVerifyVersion(ExceptionCode& e)
{
    // Open the main thread connection to the database
    if (!m_mainSQLDatabase.open(m_filename)) {
        LOG_ERROR("Unable to open database at path %s", m_filename.ascii().data());
        e = INVALID_STATE_ERR;
        return false;
    }

    m_databaseAuthorizer = new DatabaseAuthorizer();
    m_mainSQLDatabase.setAuthorizer(m_databaseAuthorizer);

    // Open the background thread connection
    RefPtr<DatabaseOpenTask> task = new DatabaseOpenTask();

    task->lockForSynchronousScheduling();
    m_databaseThread->scheduleImmediateTask(this, task.get());
    task->waitForSynchronousCompletion();

    ASSERT(task->isComplete());
    e = task->exceptionCode();
    return task->openSuccessful();
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
        resultString = statement.getColumnText16(0);
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
    static String getVersionQuery = "SELECT value FROM " + databaseInfoTableName() + " WHERE key = '" + databaseVersionKey() + "';";

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_threadSQLDatabase, getVersionQuery.copy(), version);
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

    statement.bindText16(1, value);

    result = statement.step();
    if (result != SQLResultDone) {
        LOG_ERROR("Failed to step statement to set value in database (%s)", query.ascii().data());
        return false;
    }

    return true;
}

bool Database::setVersionInDatabase(const String& version)
{
    static String setVersionQuery = "INSERT INTO " + databaseInfoTableName() + " (key, value) VALUES ('" + databaseVersionKey() + "', ?);";

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_threadSQLDatabase, setVersionQuery.copy(), version);
    if (!result)
        LOG_ERROR("Failed to set version %s in database (%s)", version.ascii().data(), setVersionQuery.ascii().data());

    m_databaseAuthorizer->enable();

    return result;
}

void Database::databaseThreadGoingAway()
{
    // FIXME: We might not need this anymore

    LOG(StorageAPI, "Database thread is going away for database %p\n", this);
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

int Database::guidForOriginAndName(const String& origin, const String& name)
{
    static int currentNewGUID = 1;
    static Mutex stringIdentifierMutex;
    static HashMap<String, int> stringIdentifierToGUIDMap;

    String stringID;
    if (origin.endsWith("/"))
        stringID = origin + name;
    else
        stringID = origin + "/" + name;

    MutexLocker locker(stringIdentifierMutex);
    int guid = stringIdentifierToGUIDMap.get(stringID);
    if (!guid) {
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
    if (!m_threadSQLDatabase.open(m_filename)) {
        LOG_ERROR("Unable to open database at path %s", m_filename.ascii().data());
        e = INVALID_STATE_ERR;
        return false;
    }

    ASSERT(m_databaseAuthorizer);
    m_threadSQLDatabase.setAuthorizer(m_databaseAuthorizer);

    if (!m_threadSQLDatabase.tableExists(databaseInfoTableName())) {
        if (!m_threadSQLDatabase.executeCommand("CREATE TABLE " + databaseInfoTableName() + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
            LOG_ERROR("Unable to create table %s in database %s", databaseInfoTableName().ascii().data(), databaseDebugName().ascii().data());
            e = INVALID_STATE_ERR;
            return false;
        }
    }


    String currentVersion;
    {
        MutexLocker locker(guidMutex());
        currentVersion = guidToVersionMap().get(m_guid);


        if (currentVersion.isNull())
            LOG(StorageAPI, "Current cached version for guid %i is null", m_guid);
        else
            LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());

        if (currentVersion.isNull()) {
            if (!getVersionFromDatabase(currentVersion)) {
                LOG_ERROR("Failed to get current version from database %s", databaseDebugName().ascii().data());
                e = INVALID_STATE_ERR;
                return false;
            }
            if (currentVersion.length()) {
                LOG(StorageAPI, "Retrieved current version %s from database %s", currentVersion.ascii().data(), databaseDebugName().ascii().data());
            } else {
                LOG(StorageAPI, "Setting version %s in database %s that was just created", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                if (!setVersionInDatabase(m_expectedVersion)) {
                    LOG_ERROR("Failed to set version %s in database %s", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                    e = INVALID_STATE_ERR;
                    return false;
                }

                currentVersion = m_expectedVersion;
            }

            guidToVersionMap().set(m_guid, currentVersion.copy());
        }
    }

    if (currentVersion.isNull()) {
        LOG(StorageAPI, "Database %s does not have its version set", databaseDebugName().ascii().data());
        currentVersion = "";
    }

    // FIXME: For now, the spec says that if the database has no version, it is valid for any "Expected version" string.  That seems silly and I think it should be
    // changed, and here's where we would change it
    if (m_expectedVersion.length()) {
        if (currentVersion.length() && m_expectedVersion != currentVersion) {
            LOG(StorageAPI, "page expects version %s from database %s, which actually has version name %s - openDatabase() call will fail", m_expectedVersion.ascii().data(),
                databaseDebugName().ascii().data(), currentVersion.ascii().data());
            e = INVALID_STATE_ERR;
            return false;
        }
    }

    return true;
}

void Database::scheduleDatabaseCallback(DatabaseCallback* callback)
{
    ASSERT(callback);
    MutexLocker locker(globalCallbackMutex());
    {
        MutexLocker locker(m_callbackMutex);
        m_pendingCallbacks.append(callback);
        globalCallbackSet().add(this);
        if (!s_globalCallbackScheduled) {
            callOnMainThread(deliverAllPendingCallbacks);
            s_globalCallbackScheduled = true;
        }
    }
}

void Database::performChangeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<VersionChangeCallback> callback)
{
    // FIXME: "user agent must obtain a full lock of the database (waiting for all open transactions to be closed)"
    // Currently, not having implemented the implicit thread transaction, we're not all too worried about this.
    // Once the transaction is implemented, we'll have to find a way to reschedule the changeVersion over and over as long as
    // transactions are active.  Keep on bumping it to second in line, or something!

    bool result = true;

    MutexLocker locker(guidMutex());
    {
        // Run this code in a block to guarantee that actualVersion goes out of scope before the mutex
        // is released, so we can avoid copying the string when inserting but still avoid
        // string thread-safety issues
        String actualVersion;
        if (!getVersionFromDatabase(actualVersion)) {
            LOG_ERROR("Unable to retrieve actual current version from database");
            result = false;
        }

        if (result && actualVersion != oldVersion) {
            LOG_ERROR("Old version doesn't match actual version");
            result = false;
        }

        if (result && !setVersionInDatabase(newVersion)) {
            LOG_ERROR("Unable to set new version in database");
            result = false;
        }

        if (result)
            guidToVersionMap().set(m_guid, actualVersion);
    }

    scheduleDatabaseCallback(new DatabaseChangeVersionCallback(callback, result));
}

void Database::performExecuteSql(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLCallback> callback)
{
    ASSERT(callback);

    RefPtr<SQLResultSet> resultSet = new SQLResultSet;
    bool succeeded = true;

    m_databaseAuthorizer->reset();

    SQLiteStatement statement(m_threadSQLDatabase, sqlStatement);
    int result = statement.prepare();
    if (result != SQLResultOk) {
        LOG(StorageAPI, "Failed to prepare sql query '%s' - error was: %s", sqlStatement.ascii().data(), statement.lastErrorMsg());
        succeeded = false;
        resultSet->setErrorCode(1);
        resultSet->setErrorMessage(m_threadSQLDatabase.lastErrorMsg());
    }

    if (succeeded) {
        for (unsigned i = 0; i < arguments.size(); ++i) {
            if (statement.bindValue(i + 1, arguments[i]) != SQLResultOk) {
                LOG(StorageAPI, "Failed to bind value index %i to statement for query '%s'", i + 1, sqlStatement.ascii().data());
                // FIXME: Mark the transaction invalid here once we implement the transaction part of the spec
                succeeded = false;
                resultSet->setErrorCode(1);
                resultSet->setErrorMessage(m_threadSQLDatabase.lastErrorMsg());
                break;
            }
        }
    }

    // Step so we can fetch the column names.
    result = statement.step();
    if (result == SQLResultRow && succeeded) {
        int columnCount = statement.columnCount();
        SQLResultSetRowList* rows = resultSet->rows();

        for (int i = 0; i < columnCount; i++)
            rows->addColumn(statement.getColumnName16(i));

        do {
            for (int i = 0; i < columnCount; i++) {
                // FIXME: Look at the column type
                rows->addResult(statement.getColumnText16(i));
            }

            result = statement.step();
        } while (result == SQLResultRow);

        if (result != SQLResultDone) {
            resultSet->setErrorCode(1);
            resultSet->setErrorMessage(m_threadSQLDatabase.lastErrorMsg());
            succeeded = false;
        }
    } else if (result == SQLResultDone && succeeded) {
        // Didn't find anything, or was an insert
        if (m_databaseAuthorizer->lastActionWasInsert())
            resultSet->setInsertId(m_threadSQLDatabase.lastInsertRowID());
    } else if (succeeded) {
        // A new error occured on the first step of the statement - record it here
        resultSet->setErrorCode(1);
        resultSet->setErrorMessage(m_threadSQLDatabase.lastErrorMsg());
        succeeded = false;
    }

    // FIXME: If the spec allows triggers, and we want to be "accurate" in a different way, we'd use
    // sqlite3_total_changes() here instead of sqlite3_changed, because that includes rows modified from within a trigger
    // For now, this seems sufficient
    if (succeeded)
        resultSet->setRowsAffected(m_threadSQLDatabase.lastChanges());

    scheduleDatabaseCallback(new DatabaseExecuteSqlCallback(callback, resultSet.release()));
}

void Database::performCloseTransaction()
{
    notImplemented();
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(m_threadSQLDatabase, "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLResultOk) {
        LOG_ERROR("Unable to retrieve list of tables for database %s", databaseDebugName().ascii().data());
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLResultRow) {
        String name = statement.getColumnText16(0);
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

String Database::version() const
{
    MutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).copy();
}

void Database::changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<VersionChangeCallback> callback)
{
    ASSERT(m_databaseThread);
    m_databaseThread->scheduleTask(this, new DatabaseChangeVersionTask(oldVersion, newVersion, callback));
}

void Database::executeSql(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLCallback> callback, ExceptionCode& e)
{
    // 4.11.3 Step 1 - Statement and argument validation
    // FIXME: Currently the best way we have to validate the statement is to create an actual SQLiteStatement and prepare it.  We can't prepare on
    // the main thread and run on the worker thread, because the worker thread might be in the middle of database activity.  So we have two options
    // 1 - Break up the executeSql task into two steps.  Step 1 is prepare the statement on the background thread while the main thread waits.  If
    //     the statement is valid, the main thread can return and step 2 would continue asynchronously, actually running the statement and generating results
    //     The synchronous step 1 could "cut in line", but the main thread could still block waiting for any currently running query to complete
    // 2 - Keep a main thread connection around, and keep a worker thread connection around.  The main thread connection will be used solely to prepare
    //     statements, checking them for validity.  After a statement is clear,  the work can be scheduled on the background thread.  This way, if there is a
    //     read-only query running on the background thread, the main thread can simultaneously do the prepare on the main thread and there will be no
    //     contention.  This is an advantage over #1.  A disadvantage is that between the prepare on the main thread and actually performing the query on the
    //     background thread, there is a time window where the schema could change and the statement is no longer valid.
    // 3 - A much more complex, less tested, but possibly premium solution is to do #1, but combine it with the use of the sqlite_busy_handler.  This would allow
    //     any long-running queries to periodically check and see if there's anything they need to do.  The sqlite3 docs claim sqlite is theoretically reentrant,
    //     and therefore a statement could be prepared inside a busy handler, if just to check for validation.  But it is also likely untested, and is not recc.
    //     Testing must be done on this possibility before going forward with it
    //
    //     For now, I'm going with solution #2, as it is easiest to implement and the true badness of its "con" is dubious.

    SQLiteStatement statement(m_mainSQLDatabase, sqlStatement);
    int result = statement.prepare();

    // Workaround for <rdar://problem/5537019> - a prepare on 1 connection immediately after a statement that changes the schema on the second connection
    // can fail with a SQLResultSchema error.  Trying to prepare it a second time will succeed, assuming it is actually a valid statement
    if (result == SQLResultSchema) {
        statement.finalize();
        result = statement.prepare();
    }

    if (result != SQLResultOk) {
        LOG(StorageAPI, "Unable to verify correctness of statement %s - error %i (%s)", sqlStatement.ascii().data(), result, statement.lastErrorMsg());
        e = SYNTAX_ERR;
        return;
    }

    // FIXME:  If the statement uses the ?### syntax supported by sqlite, the bind parameter count is very likely off from the number of question marks.
    // If this is the case, they might be trying to do something fishy or malicious
    if (statement.bindParameterCount() != arguments.size()) {
        LOG(StorageAPI, "Bind parameter count doesn't match number of question marks - someone's trying to use the ?### format and this might be bad");
        e = SYNTAX_ERR;
        return;
    }

    statement.finalize();

    // FIXME: There is one remaining error with the above procedure.  The statement might have ?### style binds in it that will still escape the above checks
    // For example, a statement with "?2 ?2" in it will have 2 question marks, a bind parameter count of 2, but bind number 1 will not be valid.
    // To catch this, we either need to do a dry run of bindings here (which might be expensive), or do some manual parsing to make sure ?### isn't used at all

    // FIXME: Haven't yet implemented the thread global transaction due to ambiguity/churn in the spec
    // 4.11.3 Step 2 - Assign existing transaction or begin a new one to "transaction" associated with this statement

    // FIXME: Haven't yet implemented the thread global transaction due to ambiguity/churn in the spec
    // 4.11.3 Step 3 - If "transaction" is marked as bad, throw "INVALID_STATE_ERR" exception

    ASSERT(m_databaseThread);
    m_databaseThread->scheduleTask(this, new DatabaseExecuteSqlTask(sqlStatement, arguments, callback));
}

void Database::closeTransaction()
{
    // FIXME: Haven't yet implemented the thread global transaction due to ambiguity/churn in the spec
    notImplemented();
}

void Database::deliverAllPendingCallbacks()
{
    Vector<RefPtr<Database> > databases;
    {
        MutexLocker locker(globalCallbackMutex());
        copyToVector(globalCallbackSet(), databases);
        globalCallbackSet().clear();
        s_globalCallbackScheduled = false;
    }

    LOG(StorageAPI, "Having %u databases deliver their pending callbacks", databases.size());
    for (unsigned i = 0; i < databases.size(); ++i)
        databases[i]->deliverPendingCallbacks();
}

void Database::deliverPendingCallbacks()
{
    Vector<RefPtr<DatabaseCallback> > callbacks;
    {
        MutexLocker locker(m_callbackMutex);
        callbacks.swap(m_pendingCallbacks);
    }

    LOG(StorageAPI, "Delivering %u callbacks for database %p", callbacks.size(), this);
    for (unsigned i = 0; i < callbacks.size(); ++i)
        callbacks[i]->performCallback();
}

Vector<String> Database::tableNames()
{
    RefPtr<DatabaseTableNamesTask> task = new DatabaseTableNamesTask();

    task->lockForSynchronousScheduling();
    m_databaseThread->scheduleImmediateTask(this, task.get());
    task->waitForSynchronousCompletion();

    return task->tableNames();
}

}
