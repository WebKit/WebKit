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
#include "InspectorDatabaseAgent.h"

#include "Database.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "InspectorDatabaseResource.h"
#include "InstrumentingAgents.h"
#include "SQLError.h"
#include "SQLResultSet.h"
#include "SQLResultSetRowList.h"
#include "SQLStatementCallback.h"
#include "SQLStatementErrorCallback.h"
#include "SQLTransaction.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionErrorCallback.h"
#include "SQLValue.h"
#include "VoidCallback.h"
#include <inspector/InspectorValues.h>
#include <wtf/Vector.h>

typedef Inspector::DatabaseBackendDispatcherHandler::ExecuteSQLCallback ExecuteSQLCallback;

using namespace Inspector;

namespace WebCore {

namespace {

void reportTransactionFailed(ExecuteSQLCallback& requestCallback, SQLError* error)
{
    auto errorObject = Inspector::Protocol::Database::Error::create()
        .setMessage(error->message())
        .setCode(error->code())
        .release();
    requestCallback.sendSuccess(nullptr, nullptr, WTF::move(errorObject));
}

class StatementCallback : public SQLStatementCallback {
public:
    static Ref<StatementCallback> create(Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new StatementCallback(WTF::move(requestCallback)));
    }

    virtual ~StatementCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLResultSet* resultSet) override
    {
        SQLResultSetRowList* rowList = resultSet->rows();

        auto columnNames = Inspector::Protocol::Array<String>::create();
        const Vector<String>& columns = rowList->columnNames();
        for (size_t i = 0; i < columns.size(); ++i)
            columnNames->addItem(columns[i]);

        auto values = Inspector::Protocol::Array<InspectorValue>::create();
        const Vector<SQLValue>& data = rowList->values();
        for (size_t i = 0; i < data.size(); ++i) {
            const SQLValue& value = rowList->values()[i];
            RefPtr<InspectorValue> inspectorValue;
            switch (value.type()) {
            case SQLValue::StringValue: inspectorValue = InspectorString::create(value.string()); break;
            case SQLValue::NumberValue: inspectorValue = InspectorBasicValue::create(value.number()); break;
            case SQLValue::NullValue: inspectorValue = InspectorValue::null(); break;
            }
            
            values->addItem(WTF::move(inspectorValue));
        }
        m_requestCallback->sendSuccess(WTF::move(columnNames), WTF::move(values), nullptr);
        return true;
    }

private:
    StatementCallback(Ref<ExecuteSQLCallback>&& requestCallback)
        : m_requestCallback(WTF::move(requestCallback)) { }
    Ref<ExecuteSQLCallback> m_requestCallback;
};

class StatementErrorCallback : public SQLStatementErrorCallback {
public:
    static Ref<StatementErrorCallback> create(Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new StatementErrorCallback(WTF::move(requestCallback)));
    }

    virtual ~StatementErrorCallback() { }

    virtual bool handleEvent(SQLTransaction*, SQLError* error) override
    {
        reportTransactionFailed(m_requestCallback.copyRef(), error);
        return true;  
    }

private:
    StatementErrorCallback(Ref<ExecuteSQLCallback>&& requestCallback)
        : m_requestCallback(WTF::move(requestCallback)) { }
    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionCallback : public SQLTransactionCallback {
public:
    static Ref<TransactionCallback> create(const String& sqlStatement, Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new TransactionCallback(sqlStatement, WTF::move(requestCallback)));
    }

    virtual ~TransactionCallback() { }

    virtual bool handleEvent(SQLTransaction* transaction) override
    {
        if (!m_requestCallback->isActive())
            return true;

        Vector<SQLValue> sqlValues;
        Ref<SQLStatementCallback> callback(StatementCallback::create(m_requestCallback.copyRef()));
        Ref<SQLStatementErrorCallback> errorCallback(StatementErrorCallback::create(m_requestCallback.copyRef()));
        transaction->executeSQL(m_sqlStatement, sqlValues, WTF::move(callback), WTF::move(errorCallback), IGNORE_EXCEPTION);
        return true;
    }
private:
    TransactionCallback(const String& sqlStatement, Ref<ExecuteSQLCallback>&& requestCallback)
        : m_sqlStatement(sqlStatement)
        , m_requestCallback(WTF::move(requestCallback)) { }
    String m_sqlStatement;
    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionErrorCallback : public SQLTransactionErrorCallback {
public:
    static Ref<TransactionErrorCallback> create(Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new TransactionErrorCallback(WTF::move(requestCallback)));
    }

    virtual ~TransactionErrorCallback() { }

    virtual bool handleEvent(SQLError* error) override
    {
        reportTransactionFailed(m_requestCallback.get(), error);
        return true;
    }
private:
    TransactionErrorCallback(Ref<ExecuteSQLCallback>&& requestCallback)
        : m_requestCallback(WTF::move(requestCallback)) { }
    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionSuccessCallback : public VoidCallback {
public:
    static Ref<TransactionSuccessCallback> create()
    {
        return adoptRef(*new TransactionSuccessCallback());
    }

    virtual ~TransactionSuccessCallback() { }

    virtual bool handleEvent() override { return false; }

private:
    TransactionSuccessCallback() { }
};

} // namespace

void InspectorDatabaseAgent::didOpenDatabase(PassRefPtr<Database> database, const String& domain, const String& name, const String& version)
{
    if (InspectorDatabaseResource* resource = findByFileName(database->fileName())) {
        resource->setDatabase(database);
        return;
    }

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);
    m_resources.set(resource->id(), resource);
    // Resources are only bound while visible.
    if (m_frontendDispatcher && m_enabled)
        resource->bind(m_frontendDispatcher.get());
}

void InspectorDatabaseAgent::clearResources()
{
    m_resources.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(InstrumentingAgents* instrumentingAgents)
    : InspectorAgentBase(ASCIILiteral("Database"), instrumentingAgents)
    , m_enabled(false)
{
    m_instrumentingAgents->setInspectorDatabaseAgent(this);
}

InspectorDatabaseAgent::~InspectorDatabaseAgent()
{
    m_instrumentingAgents->setInspectorDatabaseAgent(nullptr);
}

void InspectorDatabaseAgent::didCreateFrontendAndBackend(Inspector::FrontendChannel* frontendChannel, Inspector::BackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<Inspector::DatabaseFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = Inspector::DatabaseBackendDispatcher::create(backendDispatcher, this);
}

void InspectorDatabaseAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher = nullptr;

    ErrorString unused;
    disable(unused);
}

void InspectorDatabaseAgent::enable(ErrorString&)
{
    if (m_enabled)
        return;
    m_enabled = true;

    DatabaseResourcesMap::iterator databasesEnd = m_resources.end();
    for (DatabaseResourcesMap::iterator it = m_resources.begin(); it != databasesEnd; ++it)
        it->value->bind(m_frontendDispatcher.get());
}

void InspectorDatabaseAgent::disable(ErrorString&)
{
    if (!m_enabled)
        return;
    m_enabled = false;
}

void InspectorDatabaseAgent::getDatabaseTableNames(ErrorString& error, const String& databaseId, RefPtr<Inspector::Protocol::Array<String>>& names)
{
    if (!m_enabled) {
        error = ASCIILiteral("Database agent is not enabled");
        return;
    }

    names = Inspector::Protocol::Array<String>::create();

    Database* database = databaseForId(databaseId);
    if (database) {
        Vector<String> tableNames = database->tableNames();
        unsigned length = tableNames.size();
        for (unsigned i = 0; i < length; ++i)
            names->addItem(tableNames[i]);
    }
}

void InspectorDatabaseAgent::executeSQL(ErrorString&, const String& databaseId, const String& query, Ref<ExecuteSQLCallback>&& requestCallback)
{
    if (!m_enabled) {
        requestCallback->sendFailure("Database agent is not enabled");
        return;
    }

    Database* database = databaseForId(databaseId);
    if (!database) {
        requestCallback->sendFailure("Database not found");
        return;
    }

    Ref<SQLTransactionCallback> callback(TransactionCallback::create(query, requestCallback.get()));
    Ref<SQLTransactionErrorCallback> errorCallback(TransactionErrorCallback::create(requestCallback.get()));
    Ref<VoidCallback> successCallback(TransactionSuccessCallback::create());
    database->transaction(WTF::move(callback), WTF::move(errorCallback), WTF::move(successCallback));
}

String InspectorDatabaseAgent::databaseId(Database* database)
{
    for (DatabaseResourcesMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->value->database() == database)
            return it->key;
    }
    return String();
}

InspectorDatabaseResource* InspectorDatabaseAgent::findByFileName(const String& fileName)
{
    for (DatabaseResourcesMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->value->database()->fileName() == fileName)
            return it->value.get();
    }
    return nullptr;
}

Database* InspectorDatabaseAgent::databaseForId(const String& databaseId)
{
    DatabaseResourcesMap::iterator it = m_resources.find(databaseId);
    if (it == m_resources.end())
        return nullptr;
    return it->value->database();
}

} // namespace WebCore
