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
#include "SQLTransaction.h"

#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "PlatformString.h"
#include "SQLError.h"
#include "SQLiteTransaction.h"
#include "SQLResultSet.h"
#include "SQLStatement.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLValue.h"

namespace WebCore {

SQLTransaction::SQLTransaction(Database* db, PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<SQLTransactionWrapper> wrapper)
    : m_nextStep(&SQLTransaction::openTransactionAndPreflight)
    , m_executeSqlAllowed(false)
    , m_database(db)
    , m_wrapper(wrapper)
    , m_callback(callback)
    , m_errorCallback(errorCallback)
    , m_shouldCommitAfterErrorCallback(true)
{
    ASSERT(m_database);
}

void SQLTransaction::executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments, PassRefPtr<SQLStatementCallback> callback, PassRefPtr<SQLStatementErrorCallback> callbackError, ExceptionCode& e)
{
    if (!m_executeSqlAllowed) {
        e = INVALID_STATE_ERR;
        return;
    }
    
    RefPtr<SQLStatement> statement = new SQLStatement(sqlStatement.copy(), arguments, callback, callbackError);
    
    if (!m_database->versionMatchesExpected())
        statement->setVersionMismatchedError();
        
    enqueueStatement(statement);
}

void SQLTransaction::enqueueStatement(PassRefPtr<SQLStatement> statement)
{
    MutexLocker locker(m_statementMutex);
    m_statementQueue.append(statement);
}

bool SQLTransaction::performNextStep()
{
    ASSERT(m_nextStep == &SQLTransaction::openTransactionAndPreflight ||
           m_nextStep == &SQLTransaction::runStatements ||
           m_nextStep == &SQLTransaction::postflightAndCommit ||
           m_nextStep == &SQLTransaction::cleanupAfterTransactionErrorCallback);
               
    (this->*m_nextStep)();

    // If there is no nextStep after performing the above step, the transaction is complete
    return !m_nextStep;
}

void SQLTransaction::performPendingCallback()
{
    ASSERT(m_nextStep == &SQLTransaction::deliverTransactionCallback ||
           m_nextStep == &SQLTransaction::deliverTransactionErrorCallback ||
           m_nextStep == &SQLTransaction::deliverStatementCallback);
           
    (this->*m_nextStep)();
}

void SQLTransaction::openTransactionAndPreflight()
{
    LOG(StorageAPI, "Opening and preflighting transaction %p", this);
    
    ASSERT(!m_sqliteTransaction);
    m_sqliteTransaction.set(new SQLiteTransaction(m_database->m_sqliteDatabase));
    
    m_database->m_databaseAuthorizer->disable();
    m_sqliteTransaction->begin();
    m_database->m_databaseAuthorizer->enable();    
    
    // Transaction Steps 1+2 - Open a transaction to the database, jumping to the error callback if that fails
    if (!m_sqliteTransaction->inProgress()) {
        m_sqliteTransaction.clear();
        m_transactionError = new SQLError(0, "unable to open a transaction to the database");
        handleTransactionError(false);
        return;
    }
    
    // Transaction Steps 3 - Peform preflight steps, jumping to the error callback if they fail
    if (m_wrapper && !m_wrapper->performPreflight(this)) {
        m_sqliteTransaction.clear();
        m_transactionError = m_wrapper->sqlError();
        if (!m_transactionError)
            m_transactionError = new SQLError(0, "unknown error occured setting up transaction");

        handleTransactionError(false);
        return;
    }
    
    // Transaction Step 4 - Invoke the transaction callback with the new SQLTransaction object
    m_nextStep = &SQLTransaction::deliverTransactionCallback;
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
    m_currentStatement = 0;
    m_nextStep = &SQLTransaction::runStatements;
    m_database->scheduleTransactionStep();
}

void SQLTransaction::runStatements()
{
    ASSERT(!m_currentStatement);

    // If there is a series of statements queued up that are all successful and have no associated
    // SQLStatementCallback objects, then we can burn through the queue
    do {
        getNextStatement();
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
        if (m_currentStatement->hasStatementCallback()) {
            m_nextStep = &SQLTransaction::deliverStatementCallback;
            m_database->scheduleTransactionCallback(this);
            return false;
        }
        return true;
    }
    
    // Transaction Steps 6.error - Call the statement's error callback, but if there was no error callback,
    // jump to the transaction error callback
    if (m_currentStatement->hasStatementErrorCallback()) {
        m_nextStep = &SQLTransaction::deliverStatementCallback;
        m_database->scheduleTransactionCallback(this);
    } else {
        m_transactionError = m_currentStatement->sqlError();
        if (!m_transactionError)
            m_transactionError = new SQLError(1, "the statement failed to execute");
        handleTransactionError(false);
    }
    
    return false;
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
    
    // Transaction Step 10 - End transaction steps
    // There is no next step
    m_nextStep = 0;
}

void SQLTransaction::handleTransactionError(bool inCallback)
{
    if (m_errorCallback) {
        if (inCallback)
            deliverTransactionErrorCallback();
        else {
            m_nextStep = &SQLTransaction::deliverTransactionErrorCallback;
            m_database->scheduleTransactionCallback(this);
        }
        return;
    }
    
    // Transaction Step 11 - If the callback couldn't be called, then rollback the transaction.
    m_shouldCommitAfterErrorCallback = false;
    if (inCallback) {
        m_nextStep = &SQLTransaction::cleanupAfterTransactionErrorCallback;
        m_database->scheduleTransactionStep();
    } else {
        cleanupAfterTransactionErrorCallback();
    }
}

void SQLTransaction::deliverTransactionErrorCallback()
{
    ASSERT(m_transactionError);
    
    // Transaction Step 11 - If the callback didn't return false, then rollback the transaction.
    // This includes the callback not existing, returning true, or throwing an exception
    if (!m_errorCallback || m_errorCallback->handleEvent(m_transactionError.get()))
        m_shouldCommitAfterErrorCallback = false;

    m_nextStep = &SQLTransaction::cleanupAfterTransactionErrorCallback;
    m_database->scheduleTransactionStep();
}

void SQLTransaction::cleanupAfterTransactionErrorCallback()
{
    m_database->m_databaseAuthorizer->disable();
    if (m_sqliteTransaction) {
        // Transaction Step 11 -If the error callback returned false, and the last error wasn't itself a 
        // failure when committing the transaction, then try to commit the transaction
        if (m_shouldCommitAfterErrorCallback)
            m_sqliteTransaction->commit();
        
        // Transaction Step 11 - If that fails, or if the callback couldn't be called 
        // or if it didn't return false, then rollback the transaction.
        if (m_sqliteTransaction->inProgress())
            m_sqliteTransaction->rollback();

        m_sqliteTransaction.clear();
    }
    m_database->m_databaseAuthorizer->enable();
    
    // Transaction Step 11 - Any still-pending statements in the transaction are discarded.
    {
        MutexLocker locker(m_statementMutex);
        m_statementQueue.clear();
    }
    
    // Transaction is complete!  There is no next step
    m_nextStep = 0;
}

} // namespace WebCore


