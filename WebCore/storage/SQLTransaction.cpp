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
#include "SQLTransaction.h"

#include "ChromeClient.h"
#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseDetails.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "OriginQuotaManager.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SQLError.h"
#include "SQLiteTransaction.h"
#include "SQLResultSet.h"
#include "SQLStatement.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLValue.h"

// There's no way of knowing exactly how much more space will be required when a statement hits the quota limit.  
// For now, we'll arbitrarily choose currentQuota + 1mb.
// In the future we decide to track if a size increase wasn't enough, and ask for larger-and-larger increases until its enough.
static const int DefaultQuotaSizeIncrease = 1048576;

namespace WebCore {

SQLTransaction::SQLTransaction(Database* db, PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<VoidCallback> successCallback, 
                               PassRefPtr<SQLTransactionWrapper> wrapper)
    : m_nextStep(&SQLTransaction::openTransactionAndPreflight)
    , m_executeSqlAllowed(false)
    , m_database(db)
    , m_wrapper(wrapper)
    , m_callback(callback)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_shouldRetryCurrentStatement(false)
    , m_shouldCommitAfterErrorCallback(true)
    , m_modifiedDatabase(false)
{
    ASSERT(m_database);
}

SQLTransaction::~SQLTransaction()
{
}

void SQLTransaction::executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLStatementCallback> callback, PassRefPtr<SQLStatementErrorCallback> callbackError, ExceptionCode& e)
{
    if (!m_executeSqlAllowed || m_database->stopped()) {
        e = INVALID_STATE_ERR;
        return;
    }
    
    RefPtr<SQLStatement> statement = new SQLStatement(sqlStatement.copy(), arguments, callback, callbackError);

    if (m_database->deleted())
        statement->setDatabaseDeletedError();

    if (!m_database->versionMatchesExpected())
        statement->setVersionMismatchedError();
        
    enqueueStatement(statement);
}

void SQLTransaction::enqueueStatement(PassRefPtr<SQLStatement> statement)
{
    MutexLocker locker(m_statementMutex);
    m_statementQueue.append(statement);
}

#ifndef NDEBUG
const char* SQLTransaction::debugStepName(SQLTransaction::TransactionStepMethod step)
{
    if (step == &SQLTransaction::openTransactionAndPreflight)
        return "openTransactionAndPreflight";
    else if (step == &SQLTransaction::runStatements)
        return "runStatements";
    else if (step == &SQLTransaction::postflightAndCommit)
        return "postflightAndCommit";
    else if (step == &SQLTransaction::cleanupAfterTransactionErrorCallback)
        return "cleanupAfterTransactionErrorCallback";
    else if (step == &SQLTransaction::deliverTransactionCallback)
        return "deliverTransactionCallback";
    else if (step == &SQLTransaction::deliverTransactionErrorCallback)
        return "deliverTransactionErrorCallback";
    else if (step == &SQLTransaction::deliverStatementCallback)
        return "deliverStatementCallback";
    else if (step == &SQLTransaction::deliverQuotaIncreaseCallback)
        return "deliverQuotaIncreaseCallback";
    else if (step == &SQLTransaction::deliverSuccessCallback)
        return "deliverSuccessCallback";
    else if (step == &SQLTransaction::cleanupAfterSuccessCallback)
        return "cleanupAfterSuccessCallback";
    else
        return "UNKNOWN";
}
#endif

void SQLTransaction::checkAndHandleClosedDatabase()
{
    if (!m_database->stopped())
        return;
        
    // If the database was stopped, don't do anything and cancel queued work
    LOG(StorageAPI, "Database was stopped - cancelling work for this transaction");
    MutexLocker locker(m_statementMutex);
    m_statementQueue.clear();
    m_nextStep = 0;
    
    // The current SQLite transaction should be stopped, as well
    if (m_sqliteTransaction) {
        m_sqliteTransaction->stop();
        m_sqliteTransaction.clear();
    }
}


bool SQLTransaction::performNextStep()
{
    LOG(StorageAPI, "Step %s\n", debugStepName(m_nextStep));

    ASSERT(m_nextStep == &SQLTransaction::openTransactionAndPreflight ||
           m_nextStep == &SQLTransaction::runStatements ||
           m_nextStep == &SQLTransaction::postflightAndCommit ||
           m_nextStep == &SQLTransaction::cleanupAfterSuccessCallback ||
           m_nextStep == &SQLTransaction::cleanupAfterTransactionErrorCallback);
    
    checkAndHandleClosedDatabase();
    
    if (m_nextStep)
        (this->*m_nextStep)();

    // If there is no nextStep after performing the above step, the transaction is complete
    return !m_nextStep;
}

void SQLTransaction::performPendingCallback()
{
    LOG(StorageAPI, "Callback %s\n", debugStepName(m_nextStep));

    ASSERT(m_nextStep == &SQLTransaction::deliverTransactionCallback ||
           m_nextStep == &SQLTransaction::deliverTransactionErrorCallback ||
           m_nextStep == &SQLTransaction::deliverStatementCallback ||
           m_nextStep == &SQLTransaction::deliverQuotaIncreaseCallback ||
           m_nextStep == &SQLTransaction::deliverSuccessCallback);

    checkAndHandleClosedDatabase();
    
    if (m_nextStep)
        (this->*m_nextStep)();
}

void SQLTransaction::openTransactionAndPreflight()
{
    ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());

    LOG(StorageAPI, "Opening and preflighting transaction %p", this);

    // If the database was deleted, jump to the error callback
    if (m_database->deleted()) {
        m_transactionError = new SQLError(0, "unable to open a transaction, because the user deleted the database");
        handleTransactionError(false);
        return;
    }

    // Set the maximum usage for this transaction
    m_database->m_sqliteDatabase.setMaximumSize(m_database->maximumSize());
    
    ASSERT(!m_sqliteTransaction);
    m_sqliteTransaction.set(new SQLiteTransaction(m_database->m_sqliteDatabase));
    
    m_database->m_databaseAuthorizer->disable();
    m_sqliteTransaction->begin();
    m_database->m_databaseAuthorizer->enable();    
    
    // Transaction Steps 1+2 - Open a transaction to the database, jumping to the error callback if that fails
    if (!m_sqliteTransaction->inProgress()) {
        ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());
        m_sqliteTransaction.clear();
        m_transactionError = new SQLError(0, "unable to open a transaction to the database");
        handleTransactionError(false);
        return;
    }
    
    // Transaction Steps 3 - Peform preflight steps, jumping to the error callback if they fail
    if (m_wrapper && !m_wrapper->performPreflight(this)) {
        ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());
        m_sqliteTransaction.clear();
        m_transactionError = m_wrapper->sqlError();
        if (!m_transactionError)
            m_transactionError = new SQLError(0, "unknown error occured setting up transaction");

        handleTransactionError(false);
        return;
    }
    
    // Transaction Step 4 - Invoke the transaction callback with the new SQLTransaction object
    m_nextStep = &SQLTransaction::deliverTransactionCallback;
    LOG(StorageAPI, "Scheduling deliverTransactionCallback for transaction %p\n", this);
    m_database->scheduleTransactionCallback(this);
}

void SQLTransaction::deliverTransactionCallback()
{
    bool shouldDeliverErrorCallback = false;

    if (m_callback) {
        m_executeSqlAllowed = true;
        m_callback->handleEvent(this, shouldDeliverErrorCallback);
        m_executeSqlAllowed = false;
    } else
        shouldDeliverErrorCallback = true;

    // Transaction Step 5 - If the transaction callback was null or raised an exception, jump to the error callback
    if (shouldDeliverErrorCallback) {
        m_transactionError = new SQLError(0, "the SQLTransactionCallback was null or threw an exception");
        deliverTransactionErrorCallback();
    } else
        scheduleToRunStatements();
}

void SQLTransaction::scheduleToRunStatements()
{
    m_nextStep = &SQLTransaction::runStatements;
    LOG(StorageAPI, "Scheduling runStatements for transaction %p\n", this);
    m_database->scheduleTransactionStep(this);
}

void SQLTransaction::runStatements()
{
    // If there is a series of statements queued up that are all successful and have no associated
    // SQLStatementCallback objects, then we can burn through the queue
    do {
        if (m_shouldRetryCurrentStatement) {
            m_shouldRetryCurrentStatement = false;
            // FIXME - Another place that needs fixing up after <rdar://problem/5628468> is addressed.
            // See ::openTransactionAndPreflight() for discussion
            
            // Reset the maximum size here, as it was increased to allow us to retry this statement
            m_database->m_sqliteDatabase.setMaximumSize(m_database->maximumSize());
        } else {
            // If the current statement has already been run, failed due to quota constraints, and we're not retrying it,
            // that means it ended in an error.  Handle it now
            if (m_currentStatement && m_currentStatement->lastExecutionFailedDueToQuota()) {
                handleCurrentStatementError();
                break;
            }
            
            // Otherwise, advance to the next statement
            getNextStatement();
        }
    } while (runCurrentStatement());
    
    // If runCurrentStatement() returned false, that means either there was no current statement to run,
    // or the current statement requires a callback to complete.  In the later case, it also scheduled 
    // the callback or performed any other additional work so we can return
    if (!m_currentStatement)
        postflightAndCommit();
}

void SQLTransaction::getNextStatement()
{
    m_currentStatement = 0;
    
    MutexLocker locker(m_statementMutex);
    if (!m_statementQueue.isEmpty()) {
        m_currentStatement = m_statementQueue.first();
        m_statementQueue.removeFirst();
    }
}

bool SQLTransaction::runCurrentStatement()
{
    if (!m_currentStatement)
        return false;
        
    m_database->m_databaseAuthorizer->reset();
    
    if (m_currentStatement->execute(m_database)) {
        if (m_database->m_databaseAuthorizer->lastActionChangedDatabase()) {
            // Flag this transaction as having changed the database for later delegate notification
            m_modifiedDatabase = true;
            // Also dirty the size of this database file for calculating quota usage
            OriginQuotaManager& manager(DatabaseTracker::tracker().originQuotaManager());
            Locker<OriginQuotaManager> locker(manager);
            
            manager.markDatabase(m_database);
        }
            
        if (m_currentStatement->hasStatementCallback()) {
            m_nextStep = &SQLTransaction::deliverStatementCallback;
            LOG(StorageAPI, "Scheduling deliverStatementCallback for transaction %p\n", this);
            m_database->scheduleTransactionCallback(this);
            return false;
        }
        return true;
    }
    
    if (m_currentStatement->lastExecutionFailedDueToQuota()) {
        m_nextStep = &SQLTransaction::deliverQuotaIncreaseCallback;
        LOG(StorageAPI, "Scheduling deliverQuotaIncreaseCallback for transaction %p\n", this);
        m_database->scheduleTransactionCallback(this);
        return false;
    }
    
    handleCurrentStatementError();
    
    return false;
}

void SQLTransaction::handleCurrentStatementError()
{
    // Transaction Steps 6.error - Call the statement's error callback, but if there was no error callback,
    // jump to the transaction error callback
    if (m_currentStatement->hasStatementErrorCallback()) {
        m_nextStep = &SQLTransaction::deliverStatementCallback;
        LOG(StorageAPI, "Scheduling deliverStatementCallback for transaction %p\n", this);
        m_database->scheduleTransactionCallback(this);
    } else {
        m_transactionError = m_currentStatement->sqlError();
        if (!m_transactionError)
            m_transactionError = new SQLError(1, "the statement failed to execute");
        handleTransactionError(false);
    }
}

void SQLTransaction::deliverStatementCallback()
{
    ASSERT(m_currentStatement);
    
    // Transaction Step 6.6 and 6.3(error) - If the statement callback went wrong, jump to the transaction error callback
    // Otherwise, continue to loop through the statement queue
    m_executeSqlAllowed = true;
    bool result = m_currentStatement->performCallback(this);
    m_executeSqlAllowed = false;

    if (result) {
        m_transactionError = new SQLError(0, "the statement callback raised an exception or statement error callback did not return false");
        handleTransactionError(true);
    } else
        scheduleToRunStatements();
}

void SQLTransaction::deliverQuotaIncreaseCallback()
{
    ASSERT(m_currentStatement);
    ASSERT(!m_shouldRetryCurrentStatement);
    
    Page* page = m_database->document()->page();
    ASSERT(page);
    
    RefPtr<SecurityOrigin> origin = m_database->securityOriginCopy();
    
    unsigned long long currentQuota = DatabaseTracker::tracker().quotaForOrigin(origin.get());
    page->chrome()->client()->exceededDatabaseQuota(m_database->document()->frame(), m_database->stringIdentifier());
    unsigned long long newQuota = DatabaseTracker::tracker().quotaForOrigin(origin.get());
    
    // If the new quota ended up being larger than the old quota, we will retry the statement.
    if (newQuota > currentQuota)
        m_shouldRetryCurrentStatement = true;
        
    m_nextStep = &SQLTransaction::runStatements;
    LOG(StorageAPI, "Scheduling runStatements for transaction %p\n", this);
    m_database->scheduleTransactionStep(this);
}

void SQLTransaction::postflightAndCommit()
{    
    // Transaction Step 7 - Peform postflight steps, jumping to the error callback if they fail
    if (m_wrapper && !m_wrapper->performPostflight(this)) {
        m_transactionError = m_wrapper->sqlError();
        if (!m_transactionError)
            m_transactionError = new SQLError(0, "unknown error occured setting up transaction");
        handleTransactionError(false);
        return;
    }
    
    // Transacton Step 8+9 - Commit the transaction, jumping to the error callback if that fails
    ASSERT(m_sqliteTransaction);
    
    m_database->m_databaseAuthorizer->disable();
    m_sqliteTransaction->commit();
    m_database->m_databaseAuthorizer->enable();

    // If the commit failed, the transaction will still be marked as "in progress"
    if (m_sqliteTransaction->inProgress()) {
        m_shouldCommitAfterErrorCallback = false;
        m_transactionError = new SQLError(0, "failed to commit the transaction");
        handleTransactionError(false);
        return;
    }
    
    // The commit was successful, notify the delegates if the transaction modified this database
    if (m_modifiedDatabase)
        DatabaseTracker::tracker().scheduleNotifyDatabaseChanged(m_database->m_securityOrigin.get(), m_database->m_name);
    
    // Now release our unneeded callbacks, to break reference cycles.
    m_callback = 0;
    m_errorCallback = 0;
    
    // Transaction Step 10 - Deliver success callback, if there is one
    if (m_successCallback) {
        m_nextStep = &SQLTransaction::deliverSuccessCallback;
        LOG(StorageAPI, "Scheduling deliverSuccessCallback for transaction %p\n", this);
        m_database->scheduleTransactionCallback(this);
    } else 
        cleanupAfterSuccessCallback();
}

void SQLTransaction::deliverSuccessCallback()
{
    // Transaction Step 10 - Deliver success callback
    ASSERT(m_successCallback);
    m_successCallback->handleEvent();
    
    // Release the last callback to break reference cycle
    m_successCallback = 0;

    // Schedule a "post-success callback" step to return control to the database thread in case there
    // are further transactions queued up for this Database
    m_nextStep = &SQLTransaction::cleanupAfterSuccessCallback;
    LOG(StorageAPI, "Scheduling cleanupAfterSuccessCallback for transaction %p\n", this);
    m_database->scheduleTransactionStep(this);
}

void SQLTransaction::cleanupAfterSuccessCallback()
{
    // Transaction Step 11 - End transaction steps
    // There is no next step
    LOG(StorageAPI, "Transaction %p is complete\n", this);
    ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());
    m_nextStep = 0;
}

void SQLTransaction::handleTransactionError(bool inCallback)
{
    if (m_errorCallback) {
        if (inCallback)
            deliverTransactionErrorCallback();
        else {
            m_nextStep = &SQLTransaction::deliverTransactionErrorCallback;
            LOG(StorageAPI, "Scheduling deliverTransactionErrorCallback for transaction %p\n", this);
            m_database->scheduleTransactionCallback(this);
        }
        return;
    }
    
    // Transaction Step 12 - If the callback couldn't be called, then rollback the transaction.
    m_shouldCommitAfterErrorCallback = false;
    if (inCallback) {
        m_nextStep = &SQLTransaction::cleanupAfterTransactionErrorCallback;
        LOG(StorageAPI, "Scheduling cleanupAfterTransactionErrorCallback for transaction %p\n", this);
        m_database->scheduleTransactionStep(this);
    } else {
        cleanupAfterTransactionErrorCallback();
    }
}

void SQLTransaction::deliverTransactionErrorCallback()
{
    ASSERT(m_transactionError);
    
    // Transaction Step 12 - If the callback didn't return false, then rollback the transaction.
    // This includes the callback not existing, returning true, or throwing an exception
    if (!m_errorCallback || m_errorCallback->handleEvent(m_transactionError.get()))
        m_shouldCommitAfterErrorCallback = false;

    m_nextStep = &SQLTransaction::cleanupAfterTransactionErrorCallback;
    LOG(StorageAPI, "Scheduling cleanupAfterTransactionErrorCallback for transaction %p\n", this);
    m_database->scheduleTransactionStep(this);
}

void SQLTransaction::cleanupAfterTransactionErrorCallback()
{
    m_database->m_databaseAuthorizer->disable();
    if (m_sqliteTransaction) {
        // Transaction Step 12 -If the error callback returned false, and the last error wasn't itself a 
        // failure when committing the transaction, then try to commit the transaction
        if (m_shouldCommitAfterErrorCallback)
            m_sqliteTransaction->commit();
        
        if (m_sqliteTransaction->inProgress()) {
            // Transaction Step 12 - If that fails, or if the callback couldn't be called 
            // or if it didn't return false, then rollback the transaction.
            m_sqliteTransaction->rollback();
        } else if (m_modifiedDatabase) {
            // But if the commit was successful, notify the delegates if the transaction modified this database
            DatabaseTracker::tracker().scheduleNotifyDatabaseChanged(m_database->m_securityOrigin.get(), m_database->m_name);
        }
        
        ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());
        m_sqliteTransaction.clear();
    }
    m_database->m_databaseAuthorizer->enable();
    
    // Transaction Step 12 - Any still-pending statements in the transaction are discarded.
    {
        MutexLocker locker(m_statementMutex);
        m_statementQueue.clear();
    }
    
    // Transaction is complete!  There is no next step
    LOG(StorageAPI, "Transaction %p is complete with an error\n", this);
    ASSERT(!m_database->m_sqliteDatabase.transactionInProgress());
    m_nextStep = 0;

    // Now release our callbacks, to break reference cycles.
    m_callback = 0;
    m_errorCallback = 0;
}

} // namespace WebCore
