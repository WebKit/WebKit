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

#include "ChangeVersionWrapper.h"
#include "CString.h"
#include "DatabaseAuthorizer.h"
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
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLResultSet.h"

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

PassRefPtr<Database> Database::openDatabase(Document* document, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, ExceptionCode& e)
{
    if (!DatabaseTracker::tracker().canEstablishDatabase(document, name, displayName, estimatedSize)) {
        // There should be an exception raised here in addition to returning a null Database object.  The question has been raised with the WHATWG
        LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(), document->securityOrigin()->toString().ascii().data());
        return 0;
    }
    
    RefPtr<Database> database = new Database(document, name, expectedVersion);

    if (!database->openAndVerifyVersion(e)) {
       LOG(StorageAPI, "Failed to open and verify version (expected %s) of database %s", expectedVersion.ascii().data(), database->databaseDebugName().ascii().data());
       return 0;
    }
    
    DatabaseTracker::tracker().setDatabaseDetails(document->securityOrigin(), name, displayName, estimatedSize);

    document->setHasOpenDatabases();

    if (Page* page = document->frame()->page())
        page->inspectorController()->didOpenDatabase(database.get(), document->domain(), name, expectedVersion);

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
    m_securityOrigin = document->securityOrigin();

    if (m_name.isNull())
        m_name = "";

    initializeThreading();

    m_guid = guidForOriginAndName(m_securityOrigin->toString(), name);

    {
        MutexLocker locker(guidMutex());

        HashSet<Database*>* hashSet = guidToDatabaseMap().get(m_guid);
        if (!hashSet) {
            hashSet = new HashSet<Database*>;
            guidToDatabaseMap().set(m_guid, hashSet);
        }

        hashSet->add(this);
    }

    m_databaseThread = document->databaseThread();
    ASSERT(m_databaseThread);

    m_filename = DatabaseTracker::tracker().fullPathForDatabase(m_securityOrigin.get(), m_name);
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
    m_databaseAuthorizer = new DatabaseAuthorizer();

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
    static String getVersionQuery = "SELECT value FROM " + databaseInfoTableName() + " WHERE key = '" + databaseVersionKey() + "';";

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_sqliteDatabase, getVersionQuery.copy(), version);
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
    static String setVersionQuery = "INSERT INTO " + databaseInfoTableName() + " (key, value) VALUES ('" + databaseVersionKey() + "', ?);";

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_sqliteDatabase, setVersionQuery.copy(), version);
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
    if (!m_sqliteDatabase.open(m_filename)) {
        LOG_ERROR("Unable to open database at path %s", m_filename.ascii().data());
        e = INVALID_STATE_ERR;
        return false;
    }

    ASSERT(m_databaseAuthorizer);
    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer);

    if (!m_sqliteDatabase.tableExists(databaseInfoTableName())) {
        if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + databaseInfoTableName() + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
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

void Database::performTransactionStep()
{
    // Do not perform a transaction if a global callback is scheduled.
    MutexLocker locker(globalCallbackMutex());
    if (s_globalCallbackScheduled)
        return;

    {
        MutexLocker locker(m_transactionMutex);

        if (!m_currentTransaction) {
            ASSERT(m_transactionQueue.size());
            m_currentTransaction = m_transactionQueue.first();
            m_transactionQueue.removeFirst();
        }
    }

    // If this step completes the entire transaction, clear the working transaction
    // and schedule the next one if necessary
    if (!m_currentTransaction->performNextStep())
        return;

    {
        MutexLocker locker(m_transactionMutex);
        m_currentTransaction = 0;

        if (m_transactionQueue.size())
            m_databaseThread->scheduleTask(this, new DatabaseTransactionTask);
    }
}

void Database::changeVersion(const String& oldVersion, const String& newVersion, 
                             PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                             PassRefPtr<VoidCallback> successCallback)
{
    scheduleTransaction(new SQLTransaction(this, callback, errorCallback, new ChangeVersionWrapper(oldVersion, newVersion)));
}

void Database::transaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback,
                           PassRefPtr<VoidCallback> successCallback)
{
    scheduleTransaction(new SQLTransaction(this, callback, errorCallback, 0));
}

void Database::scheduleTransaction(PassRefPtr<SQLTransaction> transaction)
{   
    MutexLocker locker(m_transactionMutex);
    m_transactionQueue.append(transaction);
    if (!m_currentTransaction)
        m_databaseThread->scheduleTask(this, new DatabaseTransactionTask);
}

void Database::scheduleTransactionStep()
{
    MutexLocker locker(m_transactionMutex);
    m_databaseThread->scheduleTask(this, new DatabaseTransactionTask);
}

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{    
    MutexLocker locker(globalCallbackMutex());
    {
        MutexLocker locker(m_callbackMutex);
        
        ASSERT(!m_transactionPendingCallback);
        m_transactionPendingCallback = transaction;
        globalCallbackSet().add(this);
        if (!s_globalCallbackScheduled) {
            callOnMainThread(deliverAllPendingCallbacks);
            s_globalCallbackScheduled = true;
        }
    }
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

String Database::version() const
{
    MutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).copy();
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

    LOG(StorageAPI, "Having %zu databases deliver their pending callbacks", databases.size());
    for (unsigned i = 0; i < databases.size(); ++i)
        databases[i]->deliverPendingCallback();
}

void Database::deliverPendingCallback()
{
    RefPtr<SQLTransaction> transaction;
    {
        MutexLocker locker(m_callbackMutex);
        
        ASSERT(m_transactionPendingCallback);
        transaction = m_transactionPendingCallback.release();
    }

    transaction->performPendingCallback();
}

Vector<String> Database::tableNames()
{
    RefPtr<DatabaseTableNamesTask> task = new DatabaseTableNamesTask();

    task->lockForSynchronousScheduling();
    m_databaseThread->scheduleImmediateTask(this, task.get());
    task->waitForSynchronousCompletion();

    return task->tableNames();
}

void Database::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.copy();
}

PassRefPtr<SecurityOrigin> Database::securityOriginCopy() const
{
    return m_securityOrigin->copy();
}

String Database::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.copy();
}

}
