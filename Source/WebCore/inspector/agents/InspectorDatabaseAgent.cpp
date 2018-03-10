/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <wtf/JSONValues.h>
#include <wtf/Vector.h>


namespace WebCore {

using namespace Inspector;

using ExecuteSQLCallback = Inspector::DatabaseBackendDispatcherHandler::ExecuteSQLCallback;

namespace {

void reportTransactionFailed(ExecuteSQLCallback& requestCallback, SQLError& error)
{
    auto errorObject = Inspector::Protocol::Database::Error::create()
        .setMessage(error.message())
        .setCode(error.code())
        .release();
    requestCallback.sendSuccess(nullptr, nullptr, WTFMove(errorObject));
}

class StatementCallback final : public SQLStatementCallback {
public:
    static Ref<StatementCallback> create(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new StatementCallback(context, WTFMove(requestCallback)));
    }

private:
    StatementCallback(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
        : SQLStatementCallback(context)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    CallbackResult<void> handleEvent(SQLTransaction&, SQLResultSet& resultSet) final
    {
        auto& rowList = resultSet.rows();

        auto columnNames = JSON::ArrayOf<String>::create();
        for (auto& column : rowList.columnNames())
            columnNames->addItem(column);

        auto values = JSON::ArrayOf<JSON::Value>::create();
        for (auto& value : rowList.values()) {
            auto inspectorValue = WTF::switchOn(value,
                [] (const std::nullptr_t&) { return JSON::Value::null(); },
                [] (const String& string) { return JSON::Value::create(string); },
                [] (double number) { return JSON::Value::create(number); }
            );
            values->addItem(WTFMove(inspectorValue));
        }
        m_requestCallback->sendSuccess(WTFMove(columnNames), WTFMove(values), nullptr);
        return { };
    }

    Ref<ExecuteSQLCallback> m_requestCallback;
};

class StatementErrorCallback final : public SQLStatementErrorCallback {
public:
    static Ref<StatementErrorCallback> create(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new StatementErrorCallback(context, WTFMove(requestCallback)));
    }

private:
    StatementErrorCallback(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
        : SQLStatementErrorCallback(context)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    CallbackResult<bool> handleEvent(SQLTransaction&, SQLError& error) final
    {
        reportTransactionFailed(m_requestCallback.copyRef(), error);
        return true;
    }

    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionCallback final : public SQLTransactionCallback {
public:
    static Ref<TransactionCallback> create(ScriptExecutionContext* context, const String& sqlStatement, Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new TransactionCallback(context, sqlStatement, WTFMove(requestCallback)));
    }

private:
    TransactionCallback(ScriptExecutionContext* context, const String& sqlStatement, Ref<ExecuteSQLCallback>&& requestCallback)
        : SQLTransactionCallback(context)
        , m_sqlStatement(sqlStatement)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    CallbackResult<void> handleEvent(SQLTransaction& transaction) final
    {
        if (!m_requestCallback->isActive())
            return { };

        Ref<SQLStatementCallback> callback(StatementCallback::create(scriptExecutionContext(), m_requestCallback.copyRef()));
        Ref<SQLStatementErrorCallback> errorCallback(StatementErrorCallback::create(scriptExecutionContext(), m_requestCallback.copyRef()));
        transaction.executeSql(m_sqlStatement, { }, WTFMove(callback), WTFMove(errorCallback));
        return { };
    }

    String m_sqlStatement;
    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionErrorCallback final : public SQLTransactionErrorCallback {
public:
    static Ref<TransactionErrorCallback> create(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
    {
        return adoptRef(*new TransactionErrorCallback(context, WTFMove(requestCallback)));
    }

private:
    TransactionErrorCallback(ScriptExecutionContext* context, Ref<ExecuteSQLCallback>&& requestCallback)
        : SQLTransactionErrorCallback(context)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    CallbackResult<void> handleEvent(SQLError& error) final
    {
        reportTransactionFailed(m_requestCallback.get(), error);
        return { };
    }

    Ref<ExecuteSQLCallback> m_requestCallback;
};

class TransactionSuccessCallback final : public VoidCallback {
public:
    static Ref<TransactionSuccessCallback> create(ScriptExecutionContext* context)
    {
        return adoptRef(*new TransactionSuccessCallback(context));
    }

    CallbackResult<void> handleEvent() final { return { }; }

private:
    TransactionSuccessCallback(ScriptExecutionContext* context)
        : VoidCallback(context)
    {
    }
};

} // namespace

void InspectorDatabaseAgent::didOpenDatabase(RefPtr<Database>&& database, const String& domain, const String& name, const String& version)
{
    if (auto* resource = findByFileName(database->fileName())) {
        resource->setDatabase(WTFMove(database));
        return;
    }

    auto resource = InspectorDatabaseResource::create(WTFMove(database), domain, name, version);
    m_resources.add(resource->id(), resource.ptr());
    // Resources are only bound while visible.
    if (m_enabled)
        resource->bind(m_frontendDispatcher.get());
}

void InspectorDatabaseAgent::clearResources()
{
    m_resources.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(WebAgentContext& context)
    : InspectorAgentBase(ASCIILiteral("Database"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::DatabaseFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DatabaseBackendDispatcher::create(context.backendDispatcher, this))
{
    m_instrumentingAgents.setInspectorDatabaseAgent(this);
}

InspectorDatabaseAgent::~InspectorDatabaseAgent()
{
    m_instrumentingAgents.setInspectorDatabaseAgent(nullptr);
}

void InspectorDatabaseAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorDatabaseAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString unused;
    disable(unused);
}

void InspectorDatabaseAgent::enable(ErrorString&)
{
    if (m_enabled)
        return;
    m_enabled = true;

    for (auto& resource : m_resources.values())
        resource->bind(m_frontendDispatcher.get());
}

void InspectorDatabaseAgent::disable(ErrorString&)
{
    if (!m_enabled)
        return;
    m_enabled = false;
}

void InspectorDatabaseAgent::getDatabaseTableNames(ErrorString& error, const String& databaseId, RefPtr<JSON::ArrayOf<String>>& names)
{
    if (!m_enabled) {
        error = ASCIILiteral("Database agent is not enabled");
        return;
    }

    names = JSON::ArrayOf<String>::create();

    if (auto* database = databaseForId(databaseId)) {
        for (auto& tableName : database->tableNames())
            names->addItem(tableName);
    }
}

void InspectorDatabaseAgent::executeSQL(const String& databaseId, const String& query, Ref<ExecuteSQLCallback>&& requestCallback)
{
    if (!m_enabled) {
        requestCallback->sendFailure("Database agent is not enabled");
        return;
    }

    auto* database = databaseForId(databaseId);
    if (!database) {
        requestCallback->sendFailure("Database not found");
        return;
    }

    database->transaction(TransactionCallback::create(&database->scriptExecutionContext(), query, requestCallback.copyRef()),
        TransactionErrorCallback::create(&database->scriptExecutionContext(), requestCallback.copyRef()),
        TransactionSuccessCallback::create(&database->scriptExecutionContext()));
}

String InspectorDatabaseAgent::databaseId(Database& database)
{
    for (auto& resource : m_resources) {
        if (resource.value->database() == &database)
            return resource.key;
    }
    return String();
}

InspectorDatabaseResource* InspectorDatabaseAgent::findByFileName(const String& fileName)
{
    for (auto& resource : m_resources.values()) {
        if (resource->database()->fileName() == fileName)
            return resource.get();
    }
    return nullptr;
}

Database* InspectorDatabaseAgent::databaseForId(const String& databaseId)
{
    auto* resource = m_resources.get(databaseId);
    if (!resource)
        return nullptr;
    return resource->database();
}

} // namespace WebCore
