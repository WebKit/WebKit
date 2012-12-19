/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DatabaseManager.h"

#if ENABLE(SQL_DATABASE)

#include "AbstractDatabase.h"
#include "Database.h"
#include "DatabaseCallback.h"
#include "DatabaseContext.h"
#include "DatabaseSync.h"
#include "InspectorDatabaseInstrumentation.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

#if USE(PLATFORM_STRATEGIES)
#include "DatabaseStrategy.h"
#include "PlatformStrategies.h"
#else
#include "DBBackendServer.h"
#endif

namespace WebCore {

DatabaseManager& DatabaseManager::manager()
{
    static DatabaseManager* dbManager = 0;
    if (!dbManager)
        dbManager = new DatabaseManager();

    return *dbManager;
}

DatabaseManager::DatabaseManager()
    : m_client(0)
    , m_databaseIsAvailable(true)
{
#if USE(PLATFORM_STRATEGIES)
    m_server = platformStrategies()->databaseStrategy()->getDatabaseServer();
#else
    m_server = new DBBackend::Server;
#endif
    ASSERT(m_server); // We should always have a server to work with.
}

void DatabaseManager::initialize(const String& databasePath)
{
    m_server->initialize(databasePath);
}

void DatabaseManager::setClient(DatabaseManagerClient* client)
{
    m_client = client;
    m_server->setClient(client);
}

String DatabaseManager::databaseDirectoryPath() const
{
    return m_server->databaseDirectoryPath();
}

void DatabaseManager::setDatabaseDirectoryPath(const String& path)
{
    m_server->setDatabaseDirectoryPath(path);
}

bool DatabaseManager::isAvailable()
{
    return m_databaseIsAvailable;
}

void DatabaseManager::setIsAvailable(bool available)
{
    m_databaseIsAvailable = available;
}

class DatabaseCreationCallbackTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DatabaseCreationCallbackTask> create(PassRefPtr<Database> database, PassRefPtr<DatabaseCallback> creationCallback)
    {
        return adoptPtr(new DatabaseCreationCallbackTask(database, creationCallback));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_creationCallback->handleEvent(m_database.get());
    }

private:
    DatabaseCreationCallbackTask(PassRefPtr<Database> database, PassRefPtr<DatabaseCallback> callback)
        : m_database(database)
        , m_creationCallback(callback)
    {
    }

    RefPtr<Database> m_database;
    RefPtr<DatabaseCallback> m_creationCallback;
};

PassRefPtr<Database> DatabaseManager::openDatabase(ScriptExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& e)
{
    if (!canEstablishDatabase(context, name, displayName, estimatedSize)) {
        LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(), context->securityOrigin()->toString().ascii().data());
        return 0;
    }

    RefPtr<Database> database = adoptRef(new Database(context, name, expectedVersion, displayName, estimatedSize));

    String errorMessage;
    if (!database->openAndVerifyVersion(!creationCallback, e, errorMessage)) {
        database->logErrorMessage(errorMessage);
        DatabaseManager::manager().removeOpenDatabase(database.get());
        return 0;
    }

    DatabaseManager::manager().setDatabaseDetails(context->securityOrigin(), name, displayName, estimatedSize);
    DatabaseManager::manager().setHasOpenDatabases(context);

    InspectorInstrumentation::didOpenDatabase(context, database, context->securityOrigin()->host(), name, expectedVersion);

    if (database->isNew() && creationCallback.get()) {
        LOG(StorageAPI, "Scheduling DatabaseCreationCallbackTask for database %p\n", database.get());
        database->m_scriptExecutionContext->postTask(DatabaseCreationCallbackTask::create(database, creationCallback));
    }

    return database;
}

PassRefPtr<DatabaseSync> DatabaseManager::openDatabaseSync(ScriptExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& ec)
{
    ASSERT(context->isContextThread());

    if (!canEstablishDatabase(context, name, displayName, estimatedSize)) {
        LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(), context->securityOrigin()->toString().ascii().data());
        return 0;
    }

    RefPtr<DatabaseSync> database = adoptRef(new DatabaseSync(context, name, expectedVersion, displayName, estimatedSize));

    String errorMessage;
    if (!database->performOpenAndVerify(!creationCallback, ec, errorMessage)) {
        database->logErrorMessage(errorMessage);
        DatabaseManager::manager().removeOpenDatabase(database.get());
        return 0;
    }

    DatabaseManager::manager().setDatabaseDetails(context->securityOrigin(), name, displayName, estimatedSize);

    if (database->isNew() && creationCallback.get()) {
        LOG(StorageAPI, "Invoking the creation callback for database %p\n", database.get());
        creationCallback->handleEvent(database.get());
    }

    return database;
}

bool DatabaseManager::hasOpenDatabases(ScriptExecutionContext* context)
{
    return DatabaseContext::hasOpenDatabases(context);
}

void DatabaseManager::setHasOpenDatabases(ScriptExecutionContext* context)
{
    DatabaseContext::from(context)->setHasOpenDatabases();
}

void DatabaseManager::stopDatabases(ScriptExecutionContext* context, DatabaseTaskSynchronizer* synchronizer)
{
    DatabaseContext::stopDatabases(context, synchronizer);
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return m_server->fullPathForDatabase(origin, name, createIfDoesNotExist);
}

#if !PLATFORM(CHROMIUM)
bool DatabaseManager::hasEntryForOrigin(SecurityOrigin* origin)
{
    return m_server->hasEntryForOrigin(origin);
}

void DatabaseManager::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    m_server->origins(result);
}

bool DatabaseManager::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& result)
{
    return m_server->databaseNamesForOrigin(origin, result);
}

DatabaseDetails DatabaseManager::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    return m_server->detailsForNameAndOrigin(name, origin);
}

unsigned long long DatabaseManager::usageForOrigin(SecurityOrigin* origin)
{
    return m_server->usageForOrigin(origin);
}

unsigned long long DatabaseManager::quotaForOrigin(SecurityOrigin* origin)
{
    return m_server->quotaForOrigin(origin);
}

void DatabaseManager::setQuota(SecurityOrigin* origin, unsigned long long quotaSize)
{
    m_server->setQuota(origin, quotaSize);
}

void DatabaseManager::deleteAllDatabases()
{
    m_server->deleteAllDatabases();
}

bool DatabaseManager::deleteOrigin(SecurityOrigin* origin)
{
    return m_server->deleteOrigin(origin);
}

bool DatabaseManager::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    return m_server->deleteDatabase(origin, name);
}

// From a secondary thread, must be thread safe with its data
void DatabaseManager::scheduleNotifyDatabaseChanged(SecurityOrigin* origin, const String& name)
{
    m_server->scheduleNotifyDatabaseChanged(origin, name);
}

void DatabaseManager::databaseChanged(AbstractDatabase* database)
{
    m_server->databaseChanged(database);
}

#else // PLATFORM(CHROMIUM)
void DatabaseManager::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    m_server->closeDatabasesImmediately(originIdentifier, name);
}
#endif // PLATFORM(CHROMIUM)

void DatabaseManager::interruptAllDatabasesForContext(const ScriptExecutionContext* context)
{
    m_server->interruptAllDatabasesForContext(context);
}

bool DatabaseManager::canEstablishDatabase(ScriptExecutionContext* context, const String& name, const String& displayName, unsigned long estimatedSize)
{
    return m_server->canEstablishDatabase(context, name, displayName, estimatedSize);
}

void DatabaseManager::addOpenDatabase(AbstractDatabase* database)
{
    m_server->addOpenDatabase(database);
}

void DatabaseManager::removeOpenDatabase(AbstractDatabase* database)
{
    m_server->removeOpenDatabase(database);
}

void DatabaseManager::setDatabaseDetails(SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
{
    m_server->setDatabaseDetails(origin, name, displayName, estimatedSize);
}

unsigned long long DatabaseManager::getMaxSizeForDatabase(const AbstractDatabase* database)
{
    return m_server->getMaxSizeForDatabase(database);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
