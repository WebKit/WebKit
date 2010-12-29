/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "InspectorStorageAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(DATABASE)

#include "Database.h"
#include "ExceptionCode.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "SQLError.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLResultSetRowList.h"
#include "SQLTransaction.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionErrorCallback.h"
#include "SQLValue.h"
#include "VoidCallback.h"

#include <wtf/Vector.h>

namespace WebCore {

namespace {

long lastTransactionId = 0;

void reportTransactionFailed(InspectorStorageAgent* agent, long transactionId, SQLError* error)
{
    if (!agent->frontend())
        return;
    RefPtr<InspectorObject> errorObject = InspectorObject::create();
    errorObject->setString("message", error->message());
    errorObject->setNumber("code", error->code());
    agent->frontend()->sqlTransactionFailed(transactionId, errorObject);
}

class StatementCallback : public SQLStatementCallback {
public:
    static PassRefPtr<StatementCallback> create(long transactionId, PassRefPtr<InspectorStorageAgent> agent)
    {
        return adoptRef(new StatementCallback(transactionId, agent));
    }

    virtual ~StatementCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLResultSet* resultSet)
    {
        if (!m_agent->frontend())
            return true;

        SQLResultSetRowList* rowList = resultSet->rows();

        RefPtr<InspectorArray> columnNames = InspectorArray::create();
        const Vector<String>& columns = rowList->columnNames();
        for (size_t i = 0; i < columns.size(); ++i)
            columnNames->pushString(columns[i]);

        RefPtr<InspectorArray> values = InspectorArray::create();
        const Vector<SQLValue>& data = rowList->values();
        for (size_t i = 0; i < data.size(); ++i) {
            const SQLValue& value = rowList->values()[i];
            switch (value.type()) {
                case SQLValue::StringValue: values->pushString(value.string()); break;
                case SQLValue::NumberValue: values->pushNumber(value.number()); break;
                case SQLValue::NullValue: values->pushValue(InspectorValue::null()); break;
            }
        }
        m_agent->frontend()->sqlTransactionSucceeded(m_transactionId, columnNames, values);
        return true;
    }

private:
    StatementCallback(long transactionId, PassRefPtr<InspectorStorageAgent> agent)
        : m_transactionId(transactionId)
        , m_agent(agent) { }
    long m_transactionId;
    RefPtr<InspectorStorageAgent> m_agent;
};

class StatementErrorCallback : public SQLStatementErrorCallback {
public:
    static PassRefPtr<StatementErrorCallback> create(long transactionId, PassRefPtr<InspectorStorageAgent> agent)
    {
        return adoptRef(new StatementErrorCallback(transactionId, agent));
    }

    virtual ~StatementErrorCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLError* error)
    {
        reportTransactionFailed(m_agent.get(), m_transactionId, error);
        return true;  
    }

private:
    StatementErrorCallback(long transactionId, RefPtr<InspectorStorageAgent> agent)
        : m_transactionId(transactionId)
        , m_agent(agent) { }
    long m_transactionId;
    RefPtr<InspectorStorageAgent> m_agent;
};

class TransactionCallback : public SQLTransactionCallback {
public:
    static PassRefPtr<TransactionCallback> create(const String& sqlStatement, long transactionId, PassRefPtr<InspectorStorageAgent> agent)
    {
        return adoptRef(new TransactionCallback(sqlStatement, transactionId, agent));
    }

    virtual ~TransactionCallback() { }

    virtual bool handleEvent(SQLTransaction* transaction)
    {
        if (!m_agent->frontend())
            return true;

        Vector<SQLValue> sqlValues;
        RefPtr<SQLStatementCallback> callback(StatementCallback::create(m_transactionId, m_agent));
        RefPtr<SQLStatementErrorCallback> errorCallback(StatementErrorCallback::create(m_transactionId, m_agent));
        ExceptionCode ec = 0;
        transaction->executeSQL(m_sqlStatement, sqlValues, callback.release(), errorCallback.release(), ec);
        return true;
    }
private:
    TransactionCallback(const String& sqlStatement, long transactionId, PassRefPtr<InspectorStorageAgent> agent)
        : m_sqlStatement(sqlStatement)
        , m_transactionId(transactionId)
        , m_agent(agent) { }
    String m_sqlStatement;
    long m_transactionId;
    RefPtr<InspectorStorageAgent> m_agent;
};

class TransactionErrorCallback : public SQLTransactionErrorCallback {
public:
    static PassRefPtr<TransactionErrorCallback> create(long transactionId, PassRefPtr<InspectorStorageAgent> agent)
    {
        return adoptRef(new TransactionErrorCallback(transactionId, agent));
    }

    virtual ~TransactionErrorCallback() { }

    virtual bool handleEvent(SQLError* error)
    {
        reportTransactionFailed(m_agent.get(), m_transactionId, error);
        return true;
    }
private:
    TransactionErrorCallback(long transactionId, PassRefPtr<InspectorStorageAgent> agent)
        : m_transactionId(transactionId)
        , m_agent(agent) { }
    long m_transactionId;
    RefPtr<InspectorStorageAgent> m_agent;
};

class TransactionSuccessCallback : public VoidCallback {
public:
    static PassRefPtr<TransactionSuccessCallback> create()
    {
        return adoptRef(new TransactionSuccessCallback());
    }

    virtual ~TransactionSuccessCallback() { }

    virtual void handleEvent() { }

private:
    TransactionSuccessCallback() { }
};

} // namespace

InspectorStorageAgent::InspectorStorageAgent(InspectorFrontend* frontend)
    : m_frontend(frontend)
{
}

InspectorStorageAgent::~InspectorStorageAgent()
{
}

long InspectorStorageAgent::executeSQL(Database* database, const String& query)
{
    long transactionId = ++lastTransactionId;
    RefPtr<SQLTransactionCallback> callback(TransactionCallback::create(query, transactionId, this));
    RefPtr<SQLTransactionErrorCallback> errorCallback(TransactionErrorCallback::create(transactionId, this));
    RefPtr<VoidCallback> successCallback(TransactionSuccessCallback::create());
    database->transaction(callback.release(), errorCallback.release(), successCallback.release());
    return transactionId;
}

void InspectorStorageAgent::clearFrontend()
{
    m_frontend = 0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(DATABASE)
