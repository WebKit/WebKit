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

#include "InspectorDatabaseAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(DATABASE)

#include "Database.h"
#include "ExceptionCode.h"
#include "InspectorDatabaseResource.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
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

class InspectorDatabaseAgent::FrontendProvider : public RefCounted<InspectorDatabaseAgent::FrontendProvider> {
public:
    static PassRefPtr<FrontendProvider> create(InspectorFrontend* inspectorFrontend)
    {
        return adoptRef(new FrontendProvider(inspectorFrontend));
    }

    virtual ~FrontendProvider() { }

    InspectorFrontend::Database* frontend() { return m_inspectorFrontend; }
    void clearFrontend() { m_inspectorFrontend = 0; }
private:
    FrontendProvider(InspectorFrontend* inspectorFrontend) : m_inspectorFrontend(inspectorFrontend->database()) { }
    InspectorFrontend::Database* m_inspectorFrontend;
};

namespace {

long lastTransactionId = 0;

void reportTransactionFailed(InspectorFrontend::Database* frontend, long transactionId, SQLError* error)
{
    if (!frontend)
        return;
    RefPtr<InspectorObject> errorObject = InspectorObject::create();
    errorObject->setString("message", error->message());
    errorObject->setNumber("code", error->code());
    frontend->sqlTransactionFailed(transactionId, errorObject);
}

class StatementCallback : public SQLStatementCallback {
public:
    static PassRefPtr<StatementCallback> create(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
    {
        return adoptRef(new StatementCallback(transactionId, frontendProvider));
    }

    virtual ~StatementCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLResultSet* resultSet)
    {
        if (!m_frontendProvider->frontend())
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
        m_frontendProvider->frontend()->sqlTransactionSucceeded(m_transactionId, columnNames, values);
        return true;
    }

private:
    StatementCallback(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
        : m_transactionId(transactionId)
        , m_frontendProvider(frontendProvider) { }
    long m_transactionId;
    RefPtr<InspectorDatabaseAgent::FrontendProvider> m_frontendProvider;
};

class StatementErrorCallback : public SQLStatementErrorCallback {
public:
    static PassRefPtr<StatementErrorCallback> create(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
    {
        return adoptRef(new StatementErrorCallback(transactionId, frontendProvider));
    }

    virtual ~StatementErrorCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLError* error)
    {
        reportTransactionFailed(m_frontendProvider->frontend(), m_transactionId, error);
        return true;  
    }

private:
    StatementErrorCallback(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
        : m_transactionId(transactionId)
        , m_frontendProvider(frontendProvider) { }
    long m_transactionId;
    RefPtr<InspectorDatabaseAgent::FrontendProvider> m_frontendProvider;
};

class TransactionCallback : public SQLTransactionCallback {
public:
    static PassRefPtr<TransactionCallback> create(const String& sqlStatement, long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
    {
        return adoptRef(new TransactionCallback(sqlStatement, transactionId, frontendProvider));
    }

    virtual ~TransactionCallback() { }

    virtual bool handleEvent(SQLTransaction* transaction)
    {
        if (!m_frontendProvider->frontend())
            return true;

        Vector<SQLValue> sqlValues;
        RefPtr<SQLStatementCallback> callback(StatementCallback::create(m_transactionId, m_frontendProvider));
        RefPtr<SQLStatementErrorCallback> errorCallback(StatementErrorCallback::create(m_transactionId, m_frontendProvider));
        ExceptionCode ec = 0;
        transaction->executeSQL(m_sqlStatement, sqlValues, callback.release(), errorCallback.release(), ec);
        return true;
    }
private:
    TransactionCallback(const String& sqlStatement, long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
        : m_sqlStatement(sqlStatement)
        , m_transactionId(transactionId)
        , m_frontendProvider(frontendProvider) { }
    String m_sqlStatement;
    long m_transactionId;
    RefPtr<InspectorDatabaseAgent::FrontendProvider> m_frontendProvider;
};

class TransactionErrorCallback : public SQLTransactionErrorCallback {
public:
    static PassRefPtr<TransactionErrorCallback> create(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
    {
        return adoptRef(new TransactionErrorCallback(transactionId, frontendProvider));
    }

    virtual ~TransactionErrorCallback() { }

    virtual bool handleEvent(SQLError* error)
    {
        reportTransactionFailed(m_frontendProvider->frontend(), m_transactionId, error);
        return true;
    }
private:
    TransactionErrorCallback(long transactionId, PassRefPtr<InspectorDatabaseAgent::FrontendProvider> frontendProvider)
        : m_transactionId(transactionId)
        , m_frontendProvider(frontendProvider) { }
    long m_transactionId;
    RefPtr<InspectorDatabaseAgent::FrontendProvider> m_frontendProvider;
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

void InspectorDatabaseAgent::didOpenDatabase(PassRefPtr<Database> database, const String& domain, const String& name, const String& version)
{
    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);
    m_resources.set(resource->id(), resource);
    // Resources are only bound while visible.
    if (m_frontendProvider)
        resource->bind(m_frontendProvider->frontend());
}

void InspectorDatabaseAgent::clearResources()
{
    m_resources.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(InstrumentingAgents* instrumentingAgents)
    : m_instrumentingAgents(instrumentingAgents)
{
    m_instrumentingAgents->setInspectorDatabaseAgent(this);
}

InspectorDatabaseAgent::~InspectorDatabaseAgent()
{
    m_instrumentingAgents->setInspectorDatabaseAgent(0);
}

void InspectorDatabaseAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontendProvider = FrontendProvider::create(frontend);
    DatabaseResourcesMap::iterator databasesEnd = m_resources.end();
    for (DatabaseResourcesMap::iterator it = m_resources.begin(); it != databasesEnd; ++it)
        it->second->bind(m_frontendProvider->frontend());
}

void InspectorDatabaseAgent::clearFrontend()
{
    m_frontendProvider->clearFrontend();
    m_frontendProvider.clear();
}

void InspectorDatabaseAgent::getDatabaseTableNames(ErrorString*, long databaseId, RefPtr<InspectorArray>* names)
{
    Database* database = databaseForId(databaseId);
    if (database) {
        Vector<String> tableNames = database->tableNames();
        unsigned length = tableNames.size();
        for (unsigned i = 0; i < length; ++i)
            (*names)->pushString(tableNames[i]);
    }
}

void InspectorDatabaseAgent::executeSQL(ErrorString*, long databaseId, const String& query, bool* success, long* transactionId)
{
    Database* database = databaseForId(databaseId);
    if (!database) {
        *success = false;
        return;
    }

    *transactionId = ++lastTransactionId;
    RefPtr<SQLTransactionCallback> callback(TransactionCallback::create(query, *transactionId, m_frontendProvider));
    RefPtr<SQLTransactionErrorCallback> errorCallback(TransactionErrorCallback::create(*transactionId, m_frontendProvider));
    RefPtr<VoidCallback> successCallback(TransactionSuccessCallback::create());
    database->transaction(callback.release(), errorCallback.release(), successCallback.release());
    *success = true;
}

long InspectorDatabaseAgent::databaseId(Database* database)
{
    for (DatabaseResourcesMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->second->database() == database)
            return it->first;
    }
    return 0;
}

Database* InspectorDatabaseAgent::databaseForId(long databaseId)
{
    DatabaseResourcesMap::iterator it = m_resources.find(databaseId);
    if (it == m_resources.end())
        return 0;
    return it->second->database();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(DATABASE)
