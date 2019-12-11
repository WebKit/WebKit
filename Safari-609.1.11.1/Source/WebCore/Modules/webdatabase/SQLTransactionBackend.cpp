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
#include "SQLTransactionBackend.h"

#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseContext.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Logging.h"
#include "OriginLock.h"
#include "SQLError.h"
#include "SQLStatement.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLTransaction.h"
#include "SQLTransactionCoordinator.h"
#include "SQLiteTransaction.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>


// How does a SQLTransaction work?
// ==============================
// The SQLTransaction is a state machine that executes a series of states / steps.
//
// The work of the transaction states are defined in section of 4.3.2 of the
// webdatabase spec: http://dev.w3.org/html5/webdatabase/#processing-model
//
// the State Transition Graph at a glance:
// ======================================
//
//     Backend                          .   Frontend
//     (works with SQLiteDatabase)      .   (works with Script)
//     ===========================      .   ===================
//                                      .
//     1. Idle                          .
//         v                            .
//     2. AcquireLock                   .
//         v                            .
//     3. OpenTransactionAndPreflight ------------------------------------------.
//         |                            .                                       |
//         `-------------------------------> 8. DeliverTransactionCallback --.  |
//                                      .        |                           v  v
//         ,-------------------------------------'   9. DeliverTransactionErrorCallback +
//         |                            .                                    ^  ^  ^    |
//         v                            .                                    |  |  |    |
//     4. RunStatements -----------------------------------------------------'  |  |    |
//         |        ^  ^ |  ^ |         .                                       |  |    |
//         |--------'  | |  | `------------> 10. DeliverStatementCallback +-----'  |    |
//         |           | |  `---------------------------------------------'        |    |
//         |           | `-----------------> 11. DeliverQuotaIncreaseCallback +    |    |
//         |            `-----------------------------------------------------'    |    |
//         v                            .                                          |    |
//     5. PostflightAndCommit --+--------------------------------------------------'    |
//                              |----------> 12. DeliverSuccessCallback +               |
//         ,--------------------'       .                               |               |
//         v                            .                               |               |
//     6. CleanupAndTerminate <-----------------------------------------'               |
//         v           ^                .                                               |
//     0. End          |                .                                               |
//                     |                .                                               |
//                7: CleanupAfterTransactionErrorCallback <----------------------------'
//                                      .
//
// the States and State Transitions:
// ================================
//     0. SQLTransactionState::End
//         - the end state.
//
//     1. SQLTransactionState::Idle
//         - placeholder state while waiting on frontend/backend, etc. See comment on
//           "State transitions between SQLTransaction and SQLTransactionBackend"
//           below.
//
//     2. SQLTransactionState::AcquireLock (runs in backend)
//         - this is the start state.
//         - acquire the "lock".
//         - on "lock" acquisition, goto SQLTransactionState::OpenTransactionAndPreflight.
//
//     3. SQLTransactionState::openTransactionAndPreflight (runs in backend)
//         - Sets up an SQLiteTransaction.
//         - begin the SQLiteTransaction.
//         - call the SQLTransactionWrapper preflight if available.
//         - schedule script callback.
//         - on error, goto SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::DeliverTransactionCallback.
//
//     4. SQLTransactionState::DeliverTransactionCallback (runs in frontend)
//         - invoke the script function callback() if available.
//         - on error, goto SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::RunStatements.
//
//     5. SQLTransactionState::DeliverTransactionErrorCallback (runs in frontend)
//         - invoke the script function errorCallback if available.
//         - goto SQLTransactionState::CleanupAfterTransactionErrorCallback.
//
//     6. SQLTransactionState::RunStatements (runs in backend)
//         - while there are statements {
//             - run a statement.
//             - if statementCallback is available, goto SQLTransactionState::DeliverStatementCallback.
//             - on error,
//               goto SQLTransactionState::DeliverQuotaIncreaseCallback, or
//               goto SQLTransactionState::DeliverStatementCallback, or
//               goto SQLTransactionState::deliverTransactionErrorCallback.
//           }
//         - goto SQLTransactionState::PostflightAndCommit.
//
//     7. SQLTransactionState::DeliverStatementCallback (runs in frontend)
//         - invoke script statement callback (assume available).
//         - on error, goto SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::RunStatements.
//
//     8. SQLTransactionState::DeliverQuotaIncreaseCallback (runs in frontend)
//         - give client a chance to increase the quota.
//         - goto SQLTransactionState::RunStatements.
//
//     9. SQLTransactionState::PostflightAndCommit (runs in backend)
//         - call the SQLTransactionWrapper postflight if available.
//         - commit the SQLiteTansaction.
//         - on error, goto SQLTransactionState::DeliverTransactionErrorCallback.
//         - if successCallback is available, goto SQLTransactionState::DeliverSuccessCallback.
//           else goto SQLTransactionState::CleanupAndTerminate.
//
//     10. SQLTransactionState::DeliverSuccessCallback (runs in frontend)
//         - invoke the script function successCallback() if available.
//         - goto SQLTransactionState::CleanupAndTerminate.
//
//     11. SQLTransactionState::CleanupAndTerminate (runs in backend)
//         - stop and clear the SQLiteTransaction.
//         - release the "lock".
//         - goto SQLTransactionState::End.
//
//     12. SQLTransactionState::CleanupAfterTransactionErrorCallback (runs in backend)
//         - rollback the SQLiteTransaction.
//         - goto SQLTransactionState::CleanupAndTerminate.
//
// State transitions between SQLTransaction and SQLTransactionBackend
// ==================================================================
// As shown above, there are state transitions that crosses the boundary between
// the frontend and backend. For example,
//
//     OpenTransactionAndPreflight (state 3 in the backend)
//     transitions to DeliverTransactionCallback (state 8 in the frontend),
//     which in turn transitions to RunStatements (state 4 in the backend).
//
// This cross boundary transition is done by posting transition requests to the
// other side and letting the other side's state machine execute the state
// transition in the appropriate thread (i.e. the script thread for the frontend,
// and the database thread for the backend).
//
// Logically, the state transitions work as shown in the graph above. But
// physically, the transition mechanism uses the Idle state (both in the frontend
// and backend) as a waiting state for further activity. For example, taking a
// closer look at the 3 state transition example above, what actually happens
// is as follows:
//
//     Step 1:
//     ======
//     In the frontend thread:
//     - waiting quietly is Idle. Not doing any work.
//
//     In the backend:
//     - is in OpenTransactionAndPreflight, and doing its work.
//     - when done, it transits to the backend DeliverTransactionCallback.
//     - the backend DeliverTransactionCallback sends a request to the frontend
//       to transit to DeliverTransactionCallback, and then itself transits to
//       Idle.
//
//     Step 2:
//     ======
//     In the frontend thread:
//     - transits to DeliverTransactionCallback and does its work.
//     - when done, it transits to the frontend RunStatements.
//     - the frontend RunStatements sends a request to the backend to transit
//       to RunStatements, and then itself transits to Idle.
//
//     In the backend:
//     - waiting quietly in Idle.
//
//     Step 3:
//     ======
//     In the frontend thread:
//     - waiting quietly is Idle. Not doing any work.
//
//     In the backend:
//     - transits to RunStatements, and does its work.
//        ...
//
// So, when the frontend or backend are not active, they will park themselves in
// their Idle states. This means their m_nextState is set to Idle, but they never
// actually run the corresponding state function. Note: for both the frontend and
// backend, the state function for Idle is unreachableState().
//
// The states that send a request to their peer across the front/back boundary
// are implemented with just 2 functions: SQLTransaction::sendToBackendState()
// and SQLTransactionBackend::sendToFrontendState(). These state functions do
// nothing but sends a request to the other side to transit to the current
// state (indicated by m_nextState), and then transits itself to the Idle state
// to wait for further action.


// The Life-Cycle of a SQLTransaction i.e. Who's keeping the SQLTransaction alive? 
// ==============================================================================
// The RefPtr chain goes something like this:
//
//     At birth (in DatabaseBackend::runTransaction()):
//     ====================================================
//     DatabaseBackend                    // Deque<RefPtr<SQLTransactionBackend>> m_transactionQueue points to ...
//     --> SQLTransactionBackend          // RefPtr<SQLTransaction> m_frontend points to ...
//         --> SQLTransaction             // RefPtr<SQLTransactionBackend> m_backend points to ...
//             --> SQLTransactionBackend  // which is a circular reference.
//
//     Note: there's a circular reference between the SQLTransaction front-end and
//     back-end. This circular reference is established in the constructor of the
//     SQLTransactionBackend. The circular reference will be broken by calling
//     doCleanup() to nullify m_frontend. This is done at the end of the transaction's
//     clean up state (i.e. when the transaction should no longer be in use thereafter),
//     or if the database was interrupted. See comments on "What happens if a transaction
//     is interrupted?" below for details.
//
//     After scheduling the transaction with the DatabaseThread (DatabaseBackend::scheduleTransaction()):
//     ======================================================================================================
//     DatabaseThread                         // MessageQueue<DatabaseTask> m_queue points to ...
//     --> DatabaseTransactionTask            // RefPtr<SQLTransactionBackend> m_transaction points to ...
//         --> SQLTransactionBackend          // RefPtr<SQLTransaction> m_frontend points to ...
//             --> SQLTransaction             // RefPtr<SQLTransactionBackend> m_backend points to ...
//                 --> SQLTransactionBackend  // which is a circular reference.
//
//     When executing the transaction (in DatabaseThread::databaseThread()):
//     ====================================================================
//     std::unique_ptr<DatabaseTask> task;    // points to ...
//     --> DatabaseTransactionTask            // RefPtr<SQLTransactionBackend> m_transaction points to ...
//         --> SQLTransactionBackend          // RefPtr<SQLTransaction> m_frontend;
//             --> SQLTransaction             // RefPtr<SQLTransactionBackend> m_backend points to ...
//                 --> SQLTransactionBackend  // which is a circular reference.
//
//     At the end of cleanupAndTerminate():
//     ===================================
//     At the end of the cleanup state, the SQLTransactionBackend::m_frontend is nullified.
//     If by then, a JSObject wrapper is referring to the SQLTransaction, then the reference
//     chain looks like this:
//
//     JSObjectWrapper
//     --> SQLTransaction             // in RefPtr<SQLTransactionBackend> m_backend points to ...
//         --> SQLTransactionBackend  // which no longer points back to its SQLTransaction.
//
//     When the GC collects the corresponding JSObject, the above chain will be cleaned up
//     and deleted.
//
//     If there is no JSObject wrapper referring to the SQLTransaction when the cleanup
//     states nullify SQLTransactionBackend::m_frontend, the SQLTransaction will deleted then.
//     However, there will still be a DatabaseTask pointing to the SQLTransactionBackend (see
//     the "When executing the transaction" chain above). This will keep the
//     SQLTransactionBackend alive until DatabaseThread::databaseThread() releases its
//     task std::unique_ptr.
//
//     What happens if a transaction is interrupted?
//     ============================================
//     If the transaction is interrupted half way, it won't get to run to state
//     CleanupAndTerminate, and hence, would not have called SQLTransactionBackend's
//     doCleanup(). doCleanup() is where we nullify SQLTransactionBackend::m_frontend
//     to break the reference cycle between the frontend and backend. Hence, we need
//     to cleanup the transaction by other means.
//
//     Note: calling SQLTransactionBackend::notifyDatabaseThreadIsShuttingDown()
//     is effectively the same as calling SQLTransactionBackend::doClean().
//
//     In terms of who needs to call doCleanup(), there are 5 phases in the
//     SQLTransactionBackend life-cycle. These are the phases and how the clean
//     up is done:
//
//     Phase 1. After Birth, before scheduling
//
//     - To clean up, DatabaseThread::databaseThread() will call
//       DatabaseBackend::close() during its shutdown.
//     - DatabaseBackend::close() will iterate
//       DatabaseBackend::m_transactionQueue and call
//       notifyDatabaseThreadIsShuttingDown() on each transaction there.
//        
//     Phase 2. After scheduling, before state AcquireLock
//
//     - If the interruption occures before the DatabaseTransactionTask is
//       scheduled in DatabaseThread::m_queue but hasn't gotten to execute
//       (i.e. DatabaseTransactionTask::performTask() has not been called),
//       then the DatabaseTransactionTask may get destructed before it ever
//       gets to execute.
//     - To clean up, the destructor will check if the task's m_wasExecuted is
//       set. If not, it will call notifyDatabaseThreadIsShuttingDown() on
//       the task's transaction.
//
//     Phase 3. After state AcquireLock, before "lockAcquired"
//
//     - In this phase, the transaction would have been added to the
//       SQLTransactionCoordinator's CoordinationInfo's pendingTransactions.
//     - To clean up, during shutdown, DatabaseThread::databaseThread() calls
//       SQLTransactionCoordinator::shutdown(), which calls
//       notifyDatabaseThreadIsShuttingDown().
//
//     Phase 4: After "lockAcquired", before state CleanupAndTerminate
//
//     - In this phase, the transaction would have been added either to the
//       SQLTransactionCoordinator's CoordinationInfo's activeWriteTransaction
//       or activeReadTransactions.
//     - To clean up, during shutdown, DatabaseThread::databaseThread() calls
//       SQLTransactionCoordinator::shutdown(), which calls
//       notifyDatabaseThreadIsShuttingDown().
//
//     Phase 5: After state CleanupAndTerminate
//
//     - This is how a transaction ends normally.
//     - state CleanupAndTerminate calls doCleanup().

namespace WebCore {

SQLTransactionBackend::SQLTransactionBackend(SQLTransaction& frontend)
    : m_frontend(frontend)
{
    m_requestedState = SQLTransactionState::AcquireLock;
}

SQLTransactionBackend::~SQLTransactionBackend()
{
    ASSERT(!m_frontend.m_sqliteTransaction);
}

void SQLTransactionBackend::doCleanup()
{
    ASSERT(m_frontend.database().databaseThread().getThread() == &Thread::current());

    m_frontend.releaseOriginLockIfNeeded();

    LockHolder locker(m_frontend.m_statementMutex);
    m_frontend.m_statementQueue.clear();

    if (m_frontend.m_sqliteTransaction) {
        // In the event we got here because of an interruption or error (i.e. if
        // the transaction is in progress), we should roll it back here. Clearing
        // m_sqliteTransaction invokes SQLiteTransaction's destructor which does
        // just that. We might as well do this unconditionally and free up its
        // resources because we're already terminating.
        m_frontend.m_sqliteTransaction = nullptr;
    }

    // Release the lock on this database
    if (m_frontend.m_lockAcquired)
        m_frontend.m_database->transactionCoordinator()->releaseLock(m_frontend);

    // Do some aggresive clean up here except for m_database.
    //
    // We can't clear m_database here because the frontend may asynchronously
    // invoke SQLTransactionBackend::requestTransitToState(), and that function
    // uses m_database to schedule a state transition. This may occur because
    // the frontend (being in another thread) may already be on the way to
    // requesting our next state before it detects an interruption.
    //
    // There is no harm in letting it finish making the request. It'll set
    // m_requestedState, but we won't execute a transition to that state because
    // we've already shut down the transaction.
    //
    // We also can't clear m_currentStatementBackend and m_transactionError.
    // m_currentStatementBackend may be accessed asynchronously by the
    // frontend's deliverStatementCallback() state. Similarly,
    // m_transactionError may be accessed by deliverTransactionErrorCallback().
    // This occurs if requests for transition to those states have already been
    // registered with the frontend just prior to a clean up request arriving.
    //
    // So instead, let our destructor handle their clean up since this
    // SQLTransactionBackend is guaranteed to not destruct until the frontend
    // is also destructing.

    m_frontend.m_wrapper = nullptr;
}

SQLTransactionBackend::StateFunction SQLTransactionBackend::stateFunctionFor(SQLTransactionState state)
{
    static const StateFunction stateFunctions[] = {
        &SQLTransactionBackend::unreachableState,            // 0. end
        &SQLTransactionBackend::unreachableState,            // 1. idle
        &SQLTransactionBackend::acquireLock,                 // 2.
        &SQLTransactionBackend::openTransactionAndPreflight, // 3.
        &SQLTransactionBackend::runStatements,               // 4.
        &SQLTransactionBackend::unreachableState,            // 5. postflightAndCommit
        &SQLTransactionBackend::cleanupAndTerminate,         // 6.
        &SQLTransactionBackend::cleanupAfterTransactionErrorCallback, // 7.
        &SQLTransactionBackend::unreachableState,            // 8. deliverTransactionCallback
        &SQLTransactionBackend::unreachableState,            // 9. deliverTransactionErrorCallback
        &SQLTransactionBackend::unreachableState,            // 10. deliverStatementCallback
        &SQLTransactionBackend::unreachableState,            // 11. deliverQuotaIncreaseCallback
        &SQLTransactionBackend::unreachableState             // 12. deliverSuccessCallback
    };

    ASSERT(WTF_ARRAY_LENGTH(stateFunctions) == static_cast<int>(SQLTransactionState::NumberOfStates));
    ASSERT(state < SQLTransactionState::NumberOfStates);

    return stateFunctions[static_cast<int>(state)];
}

void SQLTransactionBackend::computeNextStateAndCleanupIfNeeded()
{
    // Only honor the requested state transition if we're not supposed to be
    // cleaning up and shutting down:
    if (m_frontend.m_database->opened()) {
        setStateToRequestedState();
        ASSERT(m_nextState == SQLTransactionState::AcquireLock
            || m_nextState == SQLTransactionState::OpenTransactionAndPreflight
            || m_nextState == SQLTransactionState::RunStatements
            || m_nextState == SQLTransactionState::PostflightAndCommit
            || m_nextState == SQLTransactionState::CleanupAndTerminate
            || m_nextState == SQLTransactionState::CleanupAfterTransactionErrorCallback);

        LOG(StorageAPI, "State %s\n", nameForSQLTransactionState(m_nextState));
        return;
    }

    // If we get here, then we should be shutting down. Do clean up if needed:
    if (m_nextState == SQLTransactionState::End)
        return;
    m_nextState = SQLTransactionState::End;

    // If the database was stopped, don't do anything and cancel queued work
    LOG(StorageAPI, "Database was stopped or interrupted - cancelling work for this transaction");

    // The current SQLite transaction should be stopped, as well
    if (m_frontend.m_sqliteTransaction) {
        m_frontend.m_sqliteTransaction->stop();
        m_frontend.m_sqliteTransaction = nullptr;
    }

    // Terminate the frontend state machine. This also gets the frontend to
    // call computeNextStateAndCleanupIfNeeded() and clear its wrappers
    // if needed.
    m_frontend.requestTransitToState(SQLTransactionState::End);

    // Redirect to the end state to abort, clean up, and end the transaction.
    doCleanup();
}

void SQLTransactionBackend::notifyDatabaseThreadIsShuttingDown()
{
    ASSERT(m_frontend.database().databaseThread().getThread() == &Thread::current());

    // If the transaction is in progress, we should roll it back here, since this
    // is our last opportunity to do something related to this transaction on the
    // DB thread. Amongst other work, doCleanup() will clear m_sqliteTransaction
    // which invokes SQLiteTransaction's destructor, which will do the roll back
    // if necessary.
    doCleanup();
}

void SQLTransactionBackend::acquireLock()
{
    m_frontend.acquireLock();
}

void SQLTransactionBackend::openTransactionAndPreflight()
{
    m_frontend.openTransactionAndPreflight();
}

void SQLTransactionBackend::runStatements()
{
    m_frontend.runStatements();
}

void SQLTransactionBackend::cleanupAndTerminate()
{
    m_frontend.cleanupAndTerminate();
}

void SQLTransactionBackend::cleanupAfterTransactionErrorCallback()
{
    m_frontend.cleanupAfterTransactionErrorCallback();
}

// requestTransitToState() can be called from the frontend. Hence, it should
// NOT be modifying SQLTransactionBackend in general. The only safe field to
// modify is m_requestedState which is meant for this purpose.
void SQLTransactionBackend::requestTransitToState(SQLTransactionState nextState)
{
    LOG(StorageAPI, "Scheduling %s for transaction %p\n", nameForSQLTransactionState(nextState), this);
    m_requestedState = nextState;
    ASSERT(m_requestedState != SQLTransactionState::End);
    m_frontend.m_database->scheduleTransactionStep(m_frontend);
}

// This state function is used as a stub function to plug unimplemented states
// in the state dispatch table. They are unimplemented because they should
// never be reached in the course of correct execution.
void SQLTransactionBackend::unreachableState()
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
