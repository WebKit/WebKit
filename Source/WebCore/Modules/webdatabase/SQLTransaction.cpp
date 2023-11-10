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
#include "SQLTransaction.h"

#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseContext.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "Logging.h"
#include "OriginLock.h"
#include "SQLError.h"
#include "SQLStatement.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLTransactionBackend.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionCoordinator.h"
#include "SQLTransactionErrorCallback.h"
#include "SQLiteTransaction.h"
#include "VoidCallback.h"
#include "WindowEventLoop.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

Ref<SQLTransaction> SQLTransaction::create(Ref<Database>&& database, RefPtr<SQLTransactionCallback>&& callback, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<SQLTransactionWrapper>&& wrapper, bool readOnly)
{
    return adoptRef(*new SQLTransaction(WTFMove(database), WTFMove(callback), WTFMove(successCallback), WTFMove(errorCallback), WTFMove(wrapper), readOnly));
}

SQLTransaction::SQLTransaction(Ref<Database>&& database, RefPtr<SQLTransactionCallback>&& callback, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&& errorCallback, RefPtr<SQLTransactionWrapper>&& wrapper, bool readOnly)
    : m_database(WTFMove(database))
    , m_callbackWrapper(WTFMove(callback), &m_database->document())
    , m_successCallbackWrapper(WTFMove(successCallback), &m_database->document())
    , m_errorCallbackWrapper(WTFMove(errorCallback), &m_database->document())
    , m_wrapper(WTFMove(wrapper))
    , m_nextStep(&SQLTransaction::acquireLock)
    , m_readOnly(readOnly)
    , m_backend(*this)
{
}

SQLTransaction::~SQLTransaction() = default;

ExceptionOr<void> SQLTransaction::executeSql(const String& sqlStatement, std::optional<Vector<SQLValue>>&& arguments, RefPtr<SQLStatementCallback>&& callback, RefPtr<SQLStatementErrorCallback>&& callbackError)
{
    if (!m_executeSqlAllowed || !m_database->opened())
        return Exception { ExceptionCode::InvalidStateError };

    int permissions = DatabaseAuthorizer::ReadWriteMask;
    if (!m_database->databaseContext().allowDatabaseAccess())
        permissions |= DatabaseAuthorizer::NoAccessMask;
    else if (m_readOnly)
        permissions |= DatabaseAuthorizer::ReadOnlyMask;

    auto statement = makeUnique<SQLStatement>(m_database, sqlStatement, valueOrDefault(arguments), WTFMove(callback), WTFMove(callbackError), permissions);

    if (m_database->deleted())
        statement->setDatabaseDeletedError();

    enqueueStatement(WTFMove(statement));

    return { };
}

void SQLTransaction::lockAcquired()
{
    m_lockAcquired = true;

    m_backend.m_requestedState = SQLTransactionState::OpenTransactionAndPreflight;
    m_database->scheduleTransactionStep(*this);
}

void SQLTransaction::performNextStep()
{
    m_backend.computeNextStateAndCleanupIfNeeded();
    m_backend.runStateMachine();
}

void SQLTransaction::performPendingCallback()
{
    ASSERT(isMainThread());
    LOG(StorageAPI, "Callback %s\n", debugStepName(m_nextStep));

    ASSERT(m_nextStep == &SQLTransaction::deliverTransactionCallback
           || m_nextStep == &SQLTransaction::deliverTransactionErrorCallback
           || m_nextStep == &SQLTransaction::deliverStatementCallback
           || m_nextStep == &SQLTransaction::deliverQuotaIncreaseCallback
           || m_nextStep == &SQLTransaction::deliverSuccessCallback);

    checkAndHandleClosedDatabase();

    if (m_nextStep)
        (this->*m_nextStep)();
}

void SQLTransaction::notifyDatabaseThreadIsShuttingDown()
{
    m_backend.notifyDatabaseThreadIsShuttingDown();
}

void SQLTransaction::callErrorCallbackDueToInterruption()
{
    ASSERT(isMainThread());
    auto errorCallback = m_errorCallbackWrapper.unwrap();
    if (!errorCallback)
        return;

    m_database->document().eventLoop().queueTask(TaskSource::Networking, [errorCallback = WTFMove(errorCallback)]() mutable {
        errorCallback->handleEvent(SQLError::create(SQLError::DATABASE_ERR, "the database was closed"_s));
    });
}

void SQLTransaction::enqueueStatement(std::unique_ptr<SQLStatement> statement)
{
    Locker locker { m_statementLock };
    m_statementQueue.append(WTFMove(statement));
}

SQLTransaction::StateFunction SQLTransaction::stateFunctionFor(SQLTransactionState state)
{
    static const StateFunction stateFunctions[] = {
        &SQLTransaction::unreachableState,                // 0. illegal
        &SQLTransaction::unreachableState,                // 1. idle
        &SQLTransaction::unreachableState,                // 2. acquireLock
        &SQLTransaction::unreachableState,                // 3. openTransactionAndPreflight
        &SQLTransaction::unreachableState,                // 4. runStatements
        &SQLTransaction::unreachableState,                // 5. postflightAndCommit
        &SQLTransaction::unreachableState,                // 6. cleanupAndTerminate
        &SQLTransaction::unreachableState,                // 7. cleanupAfterTransactionErrorCallback
        &SQLTransaction::deliverTransactionCallback,      // 8.
        &SQLTransaction::deliverTransactionErrorCallback, // 9.
        &SQLTransaction::deliverStatementCallback,        // 10.
        &SQLTransaction::deliverQuotaIncreaseCallback,    // 11.
        &SQLTransaction::deliverSuccessCallback           // 12.
    };

    ASSERT(std::size(stateFunctions) == static_cast<int>(SQLTransactionState::NumberOfStates));
    ASSERT(state < SQLTransactionState::NumberOfStates);

    return stateFunctions[static_cast<int>(state)];
}

// requestTransitToState() can be called from the backend. Hence, it should
// NOT be modifying SQLTransactionBackend in general. The only safe field to
// modify is m_requestedState which is meant for this purpose.
void SQLTransaction::requestTransitToState(SQLTransactionState nextState)
{
    LOG(StorageAPI, "Scheduling %s for transaction %p\n", nameForSQLTransactionState(nextState), this);
    m_requestedState = nextState;
    m_database->scheduleTransactionCallback(this);
}

void SQLTransaction::checkAndHandleClosedDatabase()
{
    if (m_database->opened())
        return;

    // If the database was stopped, don't do anything and cancel queued work
    LOG(StorageAPI, "Database was stopped or interrupted - cancelling work for this transaction");

    Locker locker { m_statementLock };
    m_statementQueue.clear();
    m_nextStep = nullptr;

    callErrorCallbackDueToInterruption();

    // Release the unneeded callbacks, to break reference cycles.
    m_callbackWrapper.clear();
    m_successCallbackWrapper.clear();
    m_errorCallbackWrapper.clear();

    // The next steps should be executed only if we're on the DB thread.
    if (m_database->databaseThread().getThread() != &Thread::current())
        return;

    // The current SQLite transaction should be stopped, as well
    if (m_sqliteTransaction) {
        m_sqliteTransaction->stop();
        m_sqliteTransaction = nullptr;
    }

    if (m_lockAcquired)
        m_database->transactionCoordinator()->releaseLock(*this);
}

void SQLTransaction::scheduleCallback(void (SQLTransaction::*step)())
{
    m_nextStep = step;

    LOG(StorageAPI, "Scheduling %s for transaction %p\n", debugStepName(step), this);
    m_database->scheduleTransactionCallback(this);
}

void SQLTransaction::acquireLock()
{
    m_database->transactionCoordinator()->acquireLock(*this);
}

void SQLTransaction::openTransactionAndPreflight()
{
    ASSERT(!m_database->sqliteDatabase().transactionInProgress());
    ASSERT(m_lockAcquired);

    LOG(StorageAPI, "Opening and preflighting transaction %p", this);

    // If the database was deleted, jump to the error callback
    if (m_database->deleted()) {
        m_transactionError = SQLError::create(SQLError::UNKNOWN_ERR, "unable to open a transaction, because the user deleted the database"_s);

        handleTransactionError();
        return;
    }

    // Set the maximum usage for this transaction if this transactions is not read-only
    if (!m_readOnly) {
        acquireOriginLock();
        m_database->sqliteDatabase().setMaximumSize(m_database->maximumSize());
    }

    ASSERT(!m_sqliteTransaction);
    m_sqliteTransaction = makeUnique<SQLiteTransaction>(m_database->sqliteDatabase(), m_readOnly);

    m_database->resetDeletes();
    m_database->disableAuthorizer();
    m_sqliteTransaction->begin();
    m_database->enableAuthorizer();

    // Spec 4.3.2.1+2: Open a transaction to the database, jumping to the error callback if that fails
    if (!m_sqliteTransaction->inProgress()) {
        ASSERT(!m_database->sqliteDatabase().transactionInProgress());
        m_transactionError = SQLError::create(SQLError::DATABASE_ERR, "unable to begin transaction", m_database->sqliteDatabase().lastError(), m_database->sqliteDatabase().lastErrorMsg());
        m_sqliteTransaction = nullptr;

        handleTransactionError();
        return;
    }

    // Note: We intentionally retrieve the actual version even with an empty expected version.
    // In multi-process browsers, we take this opportinutiy to update the cached value for
    // the actual version. In single-process browsers, this is just a map lookup.
    String actualVersion;
    if (!m_database->getActualVersionForTransaction(actualVersion)) {
        m_transactionError = SQLError::create(SQLError::DATABASE_ERR, "unable to read version", m_database->sqliteDatabase().lastError(), m_database->sqliteDatabase().lastErrorMsg());
        m_database->disableAuthorizer();
        m_sqliteTransaction = nullptr;
        m_database->enableAuthorizer();

        handleTransactionError();
        return;
    }

    auto expectedVersion = m_database->expectedVersionIsolatedCopy();
    m_hasVersionMismatch = !expectedVersion.isEmpty() && expectedVersion != actualVersion;

    // Spec 4.3.2.3: Perform preflight steps, jumping to the error callback if they fail
    if (m_wrapper && !m_wrapper->performPreflight(*this)) {
        m_database->disableAuthorizer();
        m_sqliteTransaction = nullptr;
        m_database->enableAuthorizer();
        m_transactionError = m_wrapper->sqlError();
        if (!m_transactionError)
            m_transactionError = SQLError::create(SQLError::UNKNOWN_ERR, "unknown error occurred during transaction preflight"_s);

        handleTransactionError();
        return;
    }

    // Spec 4.3.2.4: Invoke the transaction callback with the new SQLTransaction object
    if (m_callbackWrapper.hasCallback()) {
        scheduleCallback(&SQLTransaction::deliverTransactionCallback);
        return;
    }

    // If we have no callback to make, skip pass to the state after:
    runStatements();
}

void SQLTransaction::runStatements()
{
    ASSERT(m_lockAcquired);

    // If there is a series of statements queued up that are all successful and have no associated
    // SQLStatementCallback objects, then we can burn through the queue
    do {
        if (m_shouldRetryCurrentStatement && !m_sqliteTransaction->wasRolledBackBySqlite()) {
            m_shouldRetryCurrentStatement = false;
            // FIXME - Another place that needs fixing up after <rdar://problem/5628468> is addressed.
            // See ::openTransactionAndPreflight() for discussion

            // Reset the maximum size here, as it was increased to allow us to retry this statement.
            // m_shouldRetryCurrentStatement is set to true only when a statement exceeds
            // the quota, which can happen only in a read-write transaction. Therefore, there
            // is no need to check here if the transaction is read-write.
            m_database->sqliteDatabase().setMaximumSize(m_database->maximumSize());
        } else {
            // If the current statement has already been run, failed due to quota constraints, and we're not retrying it,
            // that means it ended in an error. Handle it now
            if (m_currentStatement && m_currentStatement->lastExecutionFailedDueToQuota()) {
                handleCurrentStatementError();
                break;
            }

            // Otherwise, advance to the next statement
            getNextStatement();
        }
    } while (runCurrentStatement());

    // If runCurrentStatement() returned false, that means either there was no current statement to run,
    // or the current statement requires a callback to complete. In the later case, it also scheduled
    // the callback or performed any other additional work so we can return.
    if (!m_currentStatement)
        postflightAndCommit();
}

void SQLTransaction::cleanupAndTerminate()
{
    ASSERT(m_lockAcquired);

    // Spec 4.3.2.9: End transaction steps. There is no next step.
    LOG(StorageAPI, "Transaction %p is complete\n", this);
    ASSERT(!m_database->sqliteDatabase().transactionInProgress());

    // Phase 5 cleanup. See comment on the SQLTransaction life-cycle above.
    m_backend.doCleanup();
    m_database->inProgressTransactionCompleted();
}

void SQLTransaction::cleanupAfterTransactionErrorCallback()
{
    ASSERT(m_lockAcquired);

    LOG(StorageAPI, "Transaction %p is complete with an error\n", this);
    m_database->disableAuthorizer();
    if (m_sqliteTransaction) {
        // Spec 4.3.2.10: Rollback the transaction.
        m_sqliteTransaction->rollback();

        ASSERT(!m_database->sqliteDatabase().transactionInProgress());
        m_sqliteTransaction = nullptr;
    }
    m_database->enableAuthorizer();

    releaseOriginLockIfNeeded();

    ASSERT(!m_database->sqliteDatabase().transactionInProgress());

    cleanupAndTerminate();
}

void SQLTransaction::deliverTransactionCallback()
{
    bool shouldDeliverErrorCallback = false;

    // Spec 4.3.2 4: Invoke the transaction callback with the new SQLTransaction object
    RefPtr<SQLTransactionCallback> callback = m_callbackWrapper.unwrap();
    if (callback) {
        m_executeSqlAllowed = true;

        auto result = callback->handleEvent(*this);
        shouldDeliverErrorCallback = result.type() == CallbackResultType::ExceptionThrown;

        m_executeSqlAllowed = false;
    }

    // Spec 4.3.2 5: If the transaction callback was null or raised an exception, jump to the error callback
    if (shouldDeliverErrorCallback) {
        m_transactionError = SQLError::create(SQLError::UNKNOWN_ERR, "the SQLTransactionCallback was null or threw an exception"_s);
        return deliverTransactionErrorCallback();
    }

    m_backend.requestTransitToState(SQLTransactionState::RunStatements);
}

void SQLTransaction::deliverTransactionErrorCallback()
{
    ASSERT(m_transactionError);

    // Spec 4.3.2.10: If exists, invoke error callback with the last
    // error to have occurred in this transaction.
    RefPtr<SQLTransactionErrorCallback> errorCallback = m_errorCallbackWrapper.unwrap();
    if (errorCallback) {
        m_database->document().eventLoop().queueTask(TaskSource::Networking, [errorCallback = WTFMove(errorCallback), transactionError = m_transactionError]() mutable {
            errorCallback->handleEvent(*transactionError);
        });
    }

    clearCallbackWrappers();

    // Spec 4.3.2.10: Rollback the transaction.
    m_backend.requestTransitToState(SQLTransactionState::CleanupAfterTransactionErrorCallback);
}

void SQLTransaction::deliverStatementCallback()
{
    ASSERT(m_currentStatement);

    // Spec 4.3.2.6.6 and 4.3.2.6.3: If the statement callback went wrong, jump to the transaction error callback
    // Otherwise, continue to loop through the statement queue
    m_executeSqlAllowed = true;
    bool result = m_currentStatement->performCallback(*this);
    m_executeSqlAllowed = false;

    if (result) {
        m_transactionError = SQLError::create(SQLError::UNKNOWN_ERR, "the statement callback raised an exception or statement error callback did not return false"_s);

        if (m_errorCallbackWrapper.hasCallback())
            return deliverTransactionErrorCallback();

        // No error callback, so fast-forward to:
        // Transaction Step 11 - Rollback the transaction.
        m_backend.requestTransitToState(SQLTransactionState::CleanupAfterTransactionErrorCallback);
        return;
    }

    m_backend.requestTransitToState(SQLTransactionState::RunStatements);
}

void SQLTransaction::deliverQuotaIncreaseCallback()
{
    ASSERT(m_currentStatement);
    ASSERT(!m_shouldRetryCurrentStatement);

    m_shouldRetryCurrentStatement = m_database->didExceedQuota();

    m_backend.requestTransitToState(SQLTransactionState::RunStatements);
}

void SQLTransaction::deliverSuccessCallback()
{
    // Spec 4.3.2.8: Deliver success callback.
    RefPtr<VoidCallback> successCallback = m_successCallbackWrapper.unwrap();
    if (successCallback) {
        m_database->document().eventLoop().queueTask(TaskSource::Networking, [successCallback = WTFMove(successCallback)]() mutable {
            successCallback->handleEvent();
        });
    }

    clearCallbackWrappers();

    // Schedule a "post-success callback" step to return control to the database thread in case there
    // are further transactions queued up for this Database
    m_backend.requestTransitToState(SQLTransactionState::CleanupAndTerminate);
}

// This state function is used as a stub function to plug unimplemented states
// in the state dispatch table. They are unimplemented because they should
// never be reached in the course of correct execution.
void SQLTransaction::unreachableState()
{
    ASSERT_NOT_REACHED();
}

void SQLTransaction::computeNextStateAndCleanupIfNeeded()
{
    // Only honor the requested state transition if we're not supposed to be
    // cleaning up and shutting down:
    if (m_database->opened()) {
        setStateToRequestedState();
        ASSERT(m_nextState == SQLTransactionState::End
            || m_nextState == SQLTransactionState::DeliverTransactionCallback
            || m_nextState == SQLTransactionState::DeliverTransactionErrorCallback
            || m_nextState == SQLTransactionState::DeliverStatementCallback
            || m_nextState == SQLTransactionState::DeliverQuotaIncreaseCallback
            || m_nextState == SQLTransactionState::DeliverSuccessCallback);

        LOG(StorageAPI, "Callback %s\n", nameForSQLTransactionState(m_nextState));
        return;
    } else
        callErrorCallbackDueToInterruption();

    clearCallbackWrappers();
    m_backend.requestTransitToState(SQLTransactionState::CleanupAndTerminate);
}

void SQLTransaction::clearCallbackWrappers()
{
    // Release the unneeded callbacks, to break reference cycles.
    m_callbackWrapper.clear();
    m_successCallbackWrapper.clear();
    m_errorCallbackWrapper.clear();
}

void SQLTransaction::getNextStatement()
{
    m_currentStatement = nullptr;

    Locker locker { m_statementLock };
    if (!m_statementQueue.isEmpty())
        m_currentStatement = m_statementQueue.takeFirst();
}

bool SQLTransaction::runCurrentStatement()
{
    if (!m_currentStatement) {
        // No more statements to run. So move on to the next state.
        return false;
    }

    m_database->resetAuthorizer();

    if (m_hasVersionMismatch)
        m_currentStatement->setVersionMismatchedError();

    if (m_currentStatement->execute(m_database)) {
        if (m_database->lastActionChangedDatabase()) {
            // Flag this transaction as having changed the database for later delegate notification
            m_modifiedDatabase = true;
        }

        if (m_currentStatement->hasStatementCallback()) {
            scheduleCallback(&SQLTransaction::deliverStatementCallback);
            return false;
        }

        // If we get here, then the statement doesn't have a callback to invoke.
        // We can move on to the next statement. Hence, stay in this state.
        return true;
    }

    if (m_currentStatement->lastExecutionFailedDueToQuota()) {
        scheduleCallback(&SQLTransaction::deliverQuotaIncreaseCallback);
        return false;
    }

    handleCurrentStatementError();
    return false;
}

void SQLTransaction::handleCurrentStatementError()
{
    // Spec 4.3.2.6.6: error - Call the statement's error callback, but if there was no error callback,
    // or the transaction was rolled back, jump to the transaction error callback
    if (m_currentStatement->hasStatementErrorCallback() && !m_sqliteTransaction->wasRolledBackBySqlite()) {
        scheduleCallback(&SQLTransaction::deliverStatementCallback);
        return;
    }

    m_transactionError = m_currentStatement->sqlError();
    if (!m_transactionError)
        m_transactionError = SQLError::create(SQLError::DATABASE_ERR, "the statement failed to execute"_s);

    handleTransactionError();
}

void SQLTransaction::handleTransactionError()
{
    ASSERT(m_transactionError);
    if (m_errorCallbackWrapper.hasCallback()) {
        scheduleCallback(&SQLTransaction::deliverTransactionErrorCallback);
        return;
    }

    // No error callback, so fast-forward to the next state and rollback the
    // transaction.
    m_backend.cleanupAfterTransactionErrorCallback();
}

void SQLTransaction::postflightAndCommit()
{
    ASSERT(m_lockAcquired);

    // Spec 4.3.2.7: Perform postflight steps, jumping to the error callback if they fail.
    if (m_wrapper && !m_wrapper->performPostflight(*this)) {
        m_transactionError = m_wrapper->sqlError();
        if (!m_transactionError)
            m_transactionError = SQLError::create(SQLError::UNKNOWN_ERR, "unknown error occurred during transaction postflight"_s);

        handleTransactionError();
        return;
    }

    // Spec 4.3.2.7: Commit the transaction, jumping to the error callback if that fails.
    ASSERT(m_sqliteTransaction);

    m_database->disableAuthorizer();
    m_sqliteTransaction->commit();
    m_database->enableAuthorizer();

    releaseOriginLockIfNeeded();

    // If the commit failed, the transaction will still be marked as "in progress"
    if (m_sqliteTransaction->inProgress()) {
        if (m_wrapper)
            m_wrapper->handleCommitFailedAfterPostflight(*this);
        m_transactionError = SQLError::create(SQLError::DATABASE_ERR, "unable to commit transaction", m_database->sqliteDatabase().lastError(), m_database->sqliteDatabase().lastErrorMsg());

        handleTransactionError();
        return;
    }

    // Vacuum the database if anything was deleted.
    if (m_database->hadDeletes())
        m_database->incrementalVacuumIfNeeded();

    // The commit was successful. If the transaction modified this database, notify the delegates.
    if (m_modifiedDatabase)
        m_database->didCommitWriteTransaction();

    // Spec 4.3.2.8: Deliver success callback, if there is one.
    scheduleCallback(&SQLTransaction::deliverSuccessCallback);
}

void SQLTransaction::acquireOriginLock()
{
    ASSERT(!m_originLock);
    m_originLock = DatabaseTracker::singleton().originLockFor(m_database->securityOrigin());
    m_originLock->lock();
}

void SQLTransaction::releaseOriginLockIfNeeded()
{
    if (m_originLock) {
        m_originLock->unlock();
        m_originLock = nullptr;
    }
}

#if !LOG_DISABLED
const char* SQLTransaction::debugStepName(void (SQLTransaction::*step)())
{
    if (step == &SQLTransaction::acquireLock)
        return "acquireLock";
    if (step == &SQLTransaction::openTransactionAndPreflight)
        return "openTransactionAndPreflight";
    if (step == &SQLTransaction::runStatements)
        return "runStatements";
    if (step == &SQLTransaction::postflightAndCommit)
        return "postflightAndCommit";
    if (step == &SQLTransaction::cleanupAfterTransactionErrorCallback)
        return "cleanupAfterTransactionErrorCallback";
    if (step == &SQLTransaction::deliverTransactionCallback)
        return "deliverTransactionCallback";
    if (step == &SQLTransaction::deliverTransactionErrorCallback)
        return "deliverTransactionErrorCallback";
    if (step == &SQLTransaction::deliverStatementCallback)
        return "deliverStatementCallback";
    if (step == &SQLTransaction::deliverQuotaIncreaseCallback)
        return "deliverQuotaIncreaseCallback";
    if (step == &SQLTransaction::deliverSuccessCallback)
        return "deliverSuccessCallback";

    ASSERT_NOT_REACHED();
    return "UNKNOWN";
}
#endif

} // namespace WebCore
