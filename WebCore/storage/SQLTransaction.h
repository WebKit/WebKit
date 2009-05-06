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
#ifndef SQLTransaction_h
#define SQLTransaction_h

#if ENABLE(DATABASE)

#include <wtf/Threading.h>

#include "SQLiteTransaction.h"
#include "SQLStatement.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionErrorCallback.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class Database;
class SQLError;
class SQLStatementCallback;
class SQLStatementErrorCallback;
class SQLTransaction;
class SQLValue;
class String;
class VoidCallback;

class SQLTransactionWrapper : public ThreadSafeShared<SQLTransactionWrapper> {
public:
    virtual ~SQLTransactionWrapper() { }
    virtual bool performPreflight(SQLTransaction*) = 0;
    virtual bool performPostflight(SQLTransaction*) = 0;
    
    virtual SQLError* sqlError() const = 0;
};

class SQLTransaction : public ThreadSafeShared<SQLTransaction> {
public:
    static PassRefPtr<SQLTransaction> create(Database*, PassRefPtr<SQLTransactionCallback>, PassRefPtr<SQLTransactionErrorCallback>, PassRefPtr<VoidCallback>, PassRefPtr<SQLTransactionWrapper>);

    ~SQLTransaction();
    
    void executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments, 
                    PassRefPtr<SQLStatementCallback> callback, PassRefPtr<SQLStatementErrorCallback> callbackError, ExceptionCode& e);
                                        
    bool performNextStep();
    void performPendingCallback();
    
    Database* database() { return m_database.get(); }

private:
    SQLTransaction(Database*, PassRefPtr<SQLTransactionCallback>, PassRefPtr<SQLTransactionErrorCallback>, PassRefPtr<VoidCallback>, PassRefPtr<SQLTransactionWrapper>);

    typedef void (SQLTransaction::*TransactionStepMethod)();
    TransactionStepMethod m_nextStep;
    
    void enqueueStatement(PassRefPtr<SQLStatement>);
    
    void checkAndHandleClosedDatabase();
    
    void openTransactionAndPreflight();
    void deliverTransactionCallback();
    void scheduleToRunStatements();
    void runStatements();
    void getNextStatement();
    bool runCurrentStatement();
    void handleCurrentStatementError();
    void deliverStatementCallback();
    void deliverQuotaIncreaseCallback();
    void postflightAndCommit();
    void deliverSuccessCallback();
    void cleanupAfterSuccessCallback();
    void handleTransactionError(bool inCallback);
    void deliverTransactionErrorCallback();
    void cleanupAfterTransactionErrorCallback();

#ifndef NDEBUG
    static const char* debugStepName(TransactionStepMethod);
#endif

    RefPtr<SQLStatement> m_currentStatement;
    
    bool m_executeSqlAllowed;
    
    RefPtr<Database> m_database;
    RefPtr<SQLTransactionWrapper> m_wrapper;
    RefPtr<SQLTransactionCallback> m_callback;
    RefPtr<VoidCallback> m_successCallback;
    RefPtr<SQLTransactionErrorCallback> m_errorCallback;
    RefPtr<SQLError> m_transactionError;
    bool m_shouldRetryCurrentStatement;
    bool m_shouldCommitAfterErrorCallback;
    bool m_modifiedDatabase;
    
    Mutex m_statementMutex;
    Deque<RefPtr<SQLStatement> > m_statementQueue;

    OwnPtr<SQLiteTransaction> m_sqliteTransaction;
};
    
} // namespace WebCore

#endif

#endif // SQLTransaction_h
