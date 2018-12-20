/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "SQLCallbackWrapper.h"
#include "SQLTransactionBackend.h"
#include "SQLTransactionStateMachine.h"
#include "SQLValue.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Optional.h>

namespace WebCore {

class Database;
class SQLError;
class SQLStatementCallback;
class SQLStatementErrorCallback;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class VoidCallback;

class SQLTransactionWrapper : public ThreadSafeRefCounted<SQLTransactionWrapper> {
public:
    virtual ~SQLTransactionWrapper() = default;
    virtual bool performPreflight(SQLTransaction&) = 0;
    virtual bool performPostflight(SQLTransaction&) = 0;
    virtual SQLError* sqlError() const = 0;
    virtual void handleCommitFailedAfterPostflight(SQLTransaction&) = 0;
};

class SQLTransaction : public ThreadSafeRefCounted<SQLTransaction>, public SQLTransactionStateMachine<SQLTransaction> {
public:
    static Ref<SQLTransaction> create(Ref<Database>&&, RefPtr<SQLTransactionCallback>&&, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<SQLTransactionWrapper>&&, bool readOnly);
    ~SQLTransaction();

    ExceptionOr<void> executeSql(const String& sqlStatement, Optional<Vector<SQLValue>>&& arguments, RefPtr<SQLStatementCallback>&&, RefPtr<SQLStatementErrorCallback>&&);

    void lockAcquired();
    void performNextStep();
    void performPendingCallback();

    Database& database() { return m_database; }
    bool isReadOnly() const { return m_readOnly; }
    void notifyDatabaseThreadIsShuttingDown();

    // APIs called from the backend published via SQLTransaction:
    void requestTransitToState(SQLTransactionState);

private:
    friend class SQLTransactionBackend;

    SQLTransaction(Ref<Database>&&, RefPtr<SQLTransactionCallback>&&, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&&, RefPtr<SQLTransactionWrapper>&&, bool readOnly);

    void enqueueStatement(std::unique_ptr<SQLStatement>);

    void checkAndHandleClosedDatabase();

    void clearCallbackWrappers();

    void scheduleCallback(void (SQLTransaction::*)());

    // State Machine functions:
    StateFunction stateFunctionFor(SQLTransactionState) override;
    void computeNextStateAndCleanupIfNeeded();

    // State functions:
    void acquireLock();
    void openTransactionAndPreflight();
    void runStatements();
    void cleanupAndTerminate();
    void cleanupAfterTransactionErrorCallback();
    void deliverTransactionCallback();
    void deliverTransactionErrorCallback();
    void deliverStatementCallback();
    void deliverQuotaIncreaseCallback();
    void deliverSuccessCallback();

    NO_RETURN_DUE_TO_ASSERT void unreachableState();

    void getNextStatement();
    bool runCurrentStatement();
    void handleCurrentStatementError();
    void handleTransactionError();
    void postflightAndCommit();

    void acquireOriginLock();
    void releaseOriginLockIfNeeded();

#if !LOG_DISABLED
    static const char* debugStepName(void (SQLTransaction::*)());
#endif

    Ref<Database> m_database;
    SQLCallbackWrapper<SQLTransactionCallback> m_callbackWrapper;
    SQLCallbackWrapper<VoidCallback> m_successCallbackWrapper;
    SQLCallbackWrapper<SQLTransactionErrorCallback> m_errorCallbackWrapper;

    RefPtr<SQLTransactionWrapper> m_wrapper;

    void (SQLTransaction::*m_nextStep)();

    bool m_executeSqlAllowed { false };
    RefPtr<SQLError> m_transactionError;

    bool m_shouldRetryCurrentStatement { false };
    bool m_modifiedDatabase { false };
    bool m_lockAcquired { false };
    bool m_readOnly { false };
    bool m_hasVersionMismatch { false };

    Lock m_statementMutex;
    Deque<std::unique_ptr<SQLStatement>> m_statementQueue;

    std::unique_ptr<SQLStatement> m_currentStatement;

    std::unique_ptr<SQLiteTransaction> m_sqliteTransaction;
    RefPtr<OriginLock> m_originLock;

    SQLTransactionBackend m_backend;
};

} // namespace WebCore
