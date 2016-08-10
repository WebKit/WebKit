/*
 * Copyright (C) 2007, 2013, 2015 Apple Inc. All rights reserved.
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
#ifndef SQLTransactionBackend_h
#define SQLTransactionBackend_h

#include "DatabaseBasicTypes.h"
#include "SQLTransactionStateMachine.h"
#include <memory>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Database;
class OriginLock;
class SQLError;
class SQLiteTransaction;
class SQLStatement;
class SQLTransaction;
class SQLTransactionWrapper;
class SQLValue;

class SQLTransactionBackend : public SQLTransactionStateMachine<SQLTransactionBackend> {
public:
    SQLTransactionBackend(Database*, SQLTransaction&, RefPtr<SQLTransactionWrapper>&&, bool readOnly);
    ~SQLTransactionBackend();

    void lockAcquired();
    void performNextStep();

    Database* database() { return m_database.get(); }
    bool isReadOnly() { return m_readOnly; }
    void notifyDatabaseThreadIsShuttingDown();

    // APIs called from the frontend published via SQLTransactionBackend:
    void requestTransitToState(SQLTransactionState);
    SQLError* transactionError();
    SQLStatement* currentStatement();
    void setShouldRetryCurrentStatement(bool);
    void executeSQL(std::unique_ptr<SQLStatement>);
    
private:

    void doCleanup();

    void enqueueStatementBackend(std::unique_ptr<SQLStatement>);

    // State Machine functions:
    StateFunction stateFunctionFor(SQLTransactionState) override;
    void computeNextStateAndCleanupIfNeeded();

    // State functions:
    void acquireLock();
    void openTransactionAndPreflight();
    void runStatements();
    void cleanupAndTerminate();
    void cleanupAfterTransactionErrorCallback();

    NO_RETURN_DUE_TO_ASSERT void unreachableState();

    void getNextStatement();
    bool runCurrentStatement();
    void handleCurrentStatementError();
    void handleTransactionError();
    void postflightAndCommit();

    void acquireOriginLock();
    void releaseOriginLockIfNeeded();

    SQLTransaction& m_frontend;
    std::unique_ptr<SQLStatement> m_currentStatementBackend;

    RefPtr<Database> m_database;
    RefPtr<SQLTransactionWrapper> m_wrapper;
    RefPtr<SQLError> m_transactionError;

    bool m_hasCallback;
    bool m_hasSuccessCallback;
    bool m_hasErrorCallback;
    bool m_shouldRetryCurrentStatement;
    bool m_modifiedDatabase;
    bool m_lockAcquired;
    bool m_readOnly;
    bool m_hasVersionMismatch;

    Lock m_statementMutex;
    Deque<std::unique_ptr<SQLStatement>> m_statementQueue;

    std::unique_ptr<SQLiteTransaction> m_sqliteTransaction;
    RefPtr<OriginLock> m_originLock;
};

} // namespace WebCore

#endif // SQLTransactionBackend_h
