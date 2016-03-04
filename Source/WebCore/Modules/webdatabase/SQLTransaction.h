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

#ifndef SQLTransaction_h
#define SQLTransaction_h

#include "EventTarget.h"
#include "SQLCallbackWrapper.h"
#include "SQLTransactionStateMachine.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Database;
class SQLError;
class SQLStatementCallback;
class SQLStatementErrorCallback;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class SQLValue;
class VoidCallback;

class SQLTransaction : public ThreadSafeRefCounted<SQLTransaction>, public SQLTransactionStateMachine<SQLTransaction> {
public:
    static Ref<SQLTransaction> create(Ref<Database>&&, RefPtr<SQLTransactionCallback>&&, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&&, bool readOnly);
    ~SQLTransaction();

    void performPendingCallback();

    void executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments, RefPtr<SQLStatementCallback>&&, RefPtr<SQLStatementErrorCallback>&&, ExceptionCode&);

    Database& database() { return m_database; }

    // APIs called from the backend published via SQLTransaction:
    void requestTransitToState(SQLTransactionState);
    bool hasCallback() const;
    bool hasSuccessCallback() const;
    bool hasErrorCallback() const;
    void setBackend(SQLTransactionBackend*);

private:
    SQLTransaction(Ref<Database>&&, RefPtr<SQLTransactionCallback>&&, RefPtr<VoidCallback>&& successCallback, RefPtr<SQLTransactionErrorCallback>&&, bool readOnly);

    void clearCallbackWrappers();

    // State Machine functions:
    StateFunction stateFunctionFor(SQLTransactionState) override;
    void computeNextStateAndCleanupIfNeeded();

    // State functions:
    void deliverTransactionCallback();
    void deliverTransactionErrorCallback();
    void deliverStatementCallback();
    void deliverQuotaIncreaseCallback();
    void deliverSuccessCallback();

    NO_RETURN_DUE_TO_ASSERT void unreachableState();

    Ref<Database> m_database;
    RefPtr<SQLTransactionBackend> m_backend;
    SQLCallbackWrapper<SQLTransactionCallback> m_callbackWrapper;
    SQLCallbackWrapper<VoidCallback> m_successCallbackWrapper;
    SQLCallbackWrapper<SQLTransactionErrorCallback> m_errorCallbackWrapper;

    bool m_executeSqlAllowed;
    RefPtr<SQLError> m_transactionError;

    bool m_readOnly;
};

} // namespace WebCore

#endif // SQLTransaction_h
