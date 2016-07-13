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

#include "AbstractDatabaseServer.h"
#include "Database.h"
#include "DatabaseCallback.h"
#include "DatabaseContext.h"
#include "DatabaseServer.h"
#include "DatabaseTask.h"
#include "ExceptionCode.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "PlatformStrategies.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

DatabaseManager::ProposedDatabase::ProposedDatabase(DatabaseManager& manager, SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
    : m_manager(manager)
    , m_origin(origin->isolatedCopy())
    , m_details(name.isolatedCopy(), displayName.isolatedCopy(), estimatedSize, 0, 0, 0)
{
    m_manager.addProposedDatabase(this);
}

DatabaseManager::ProposedDatabase::~ProposedDatabase()
{
    m_manager.removeProposedDatabase(this);
}

DatabaseManager& DatabaseManager::singleton()
{
    static NeverDestroyed<DatabaseManager> instance;
    return instance;
}

DatabaseManager::DatabaseManager()
    : m_server(new DatabaseServer)
    , m_client(nullptr)
    , m_databaseIsAvailable(true)
#if !ASSERT_DISABLED
    , m_databaseContextRegisteredCount(0)
    , m_databaseContextInstanceCount(0)
#endif
{
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

RefPtr<DatabaseContext> DatabaseManager::existingDatabaseContextFor(ScriptExecutionContext* context)
{
    std::lock_guard<Lock> lock(m_mutex);

    ASSERT(m_databaseContextRegisteredCount >= 0);
    ASSERT(m_databaseContextInstanceCount >= 0);
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);

    RefPtr<DatabaseContext> databaseContext = adoptRef(m_contextMap.get(context));
    if (databaseContext) {
        // If we're instantiating a new DatabaseContext, the new instance would
        // carry a new refCount of 1. The client expects this and will simply
        // adoptRef the databaseContext without ref'ing it.
        //     However, instead of instantiating a new instance, we're reusing
        // an existing one that corresponds to the specified ScriptExecutionContext.
        // Hence, that new refCount need to be attributed to the reused instance
        // to ensure that the refCount is accurate when the client adopts the ref.
        // We do this by ref'ing the reused databaseContext before returning it.
        databaseContext->ref();
    }
    return databaseContext;
}

RefPtr<DatabaseContext> DatabaseManager::databaseContextFor(ScriptExecutionContext* context)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext)
        databaseContext = adoptRef(*new DatabaseContext(context));
    return databaseContext;
}

void DatabaseManager::registerDatabaseContext(DatabaseContext* databaseContext)
{
    std::lock_guard<Lock> lock(m_mutex);

    ScriptExecutionContext* context = databaseContext->scriptExecutionContext();
    m_contextMap.set(context, databaseContext);
#if !ASSERT_DISABLED
    m_databaseContextRegisteredCount++;
#endif
}

void DatabaseManager::unregisterDatabaseContext(DatabaseContext* databaseContext)
{
    std::lock_guard<Lock> lock(m_mutex);

    ScriptExecutionContext* context = databaseContext->scriptExecutionContext();
    ASSERT(m_contextMap.get(context));
#if !ASSERT_DISABLED
    m_databaseContextRegisteredCount--;
#endif
    m_contextMap.remove(context);
}

#if !ASSERT_DISABLED
void DatabaseManager::didConstructDatabaseContext()
{
    std::lock_guard<Lock> lock(m_mutex);

    m_databaseContextInstanceCount++;
}

void DatabaseManager::didDestructDatabaseContext()
{
    std::lock_guard<Lock> lock(m_mutex);

    m_databaseContextInstanceCount--;
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
}
#endif

ExceptionCode DatabaseManager::exceptionCodeForDatabaseError(DatabaseError error)
{
    switch (error) {
    case DatabaseError::None:
        return 0;
    case DatabaseError::DatabaseIsBeingDeleted:
    case DatabaseError::DatabaseSizeExceededQuota:
    case DatabaseError::DatabaseSizeOverflowed:
    case DatabaseError::GenericSecurityError:
        return SECURITY_ERR;
    case DatabaseError::InvalidDatabaseState:
        return INVALID_STATE_ERR;
    }
    ASSERT_NOT_REACHED();
    return 0; // Make some older compilers happy.
}

static void logOpenDatabaseError(ScriptExecutionContext* context, const String& name)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(name);
    LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(),
        context->securityOrigin()->toString().ascii().data());
}

RefPtr<Database> DatabaseManager::openDatabaseBackend(ScriptExecutionContext* context, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    ASSERT(error == DatabaseError::None);

    RefPtr<DatabaseContext> databaseContext = databaseContextFor(context);

    RefPtr<Database> backend = m_server->openDatabase(databaseContext, name, expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);

    if (!backend) {
        ASSERT(error != DatabaseError::None);

        switch (error) {
        case DatabaseError::DatabaseIsBeingDeleted:
        case DatabaseError::DatabaseSizeOverflowed:
        case DatabaseError::GenericSecurityError:
            logOpenDatabaseError(context, name);
            return nullptr;

        case DatabaseError::InvalidDatabaseState:
            logErrorMessage(context, errorMessage);
            return nullptr;

        case DatabaseError::DatabaseSizeExceededQuota:
            // Notify the client that we've exceeded the database quota.
            // The client may want to increase the quota, and we'll give it
            // one more try after if that is the case.
            {
                ProposedDatabase proposedDb(*this, context->securityOrigin(), name, displayName, estimatedSize);
                databaseContext->databaseExceededQuota(name, proposedDb.details());
            }
            error = DatabaseError::None;

            backend = m_server->openDatabase(databaseContext, name, expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage, AbstractDatabaseServer::RetryOpenDatabase);
            break;

        default:
            ASSERT_NOT_REACHED();
        }

        if (!backend) {
            ASSERT(error != DatabaseError::None);

            if (error == DatabaseError::InvalidDatabaseState) {
                logErrorMessage(context, errorMessage);
                return nullptr;
            }

            logOpenDatabaseError(context, name);
            return nullptr;
        }
    }

    return backend;
}

void DatabaseManager::addProposedDatabase(ProposedDatabase* proposedDb)
{
    std::lock_guard<Lock> lock(m_mutex);

    m_proposedDatabases.add(proposedDb);
}

void DatabaseManager::removeProposedDatabase(ProposedDatabase* proposedDb)
{
    std::lock_guard<Lock> lock(m_mutex);

    m_proposedDatabases.remove(proposedDb);
}

RefPtr<Database> DatabaseManager::openDatabase(ScriptExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, RefPtr<DatabaseCallback>&& creationCallback,
    DatabaseError& error)
{
    ScriptController::initializeThreading();
    ASSERT(error == DatabaseError::None);

    bool setVersionInNewDatabase = !creationCallback;
    String errorMessage;
    RefPtr<Database> database = openDatabaseBackend(context, name, expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    if (!database)
        return nullptr;

    RefPtr<DatabaseContext> databaseContext = databaseContextFor(context);
    databaseContext->setHasOpenDatabases();
    InspectorInstrumentation::didOpenDatabase(context, database.copyRef(), context->securityOrigin()->host(), name, expectedVersion);

    if (database->isNew() && creationCallback.get()) {
        LOG(StorageAPI, "Scheduling DatabaseCreationCallbackTask for database %p\n", database.get());
        database->setHasPendingCreationEvent(true);
        database->m_scriptExecutionContext->postTask([creationCallback, database] (ScriptExecutionContext&) {
            creationCallback->handleEvent(database.get());
            database->setHasPendingCreationEvent(false);
        });
    }

    ASSERT(database);
    return database;
}

bool DatabaseManager::hasOpenDatabases(ScriptExecutionContext* context)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext)
        return false;
    return databaseContext->hasOpenDatabases();
}

void DatabaseManager::stopDatabases(ScriptExecutionContext* context, DatabaseTaskSynchronizer* synchronizer)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext || !databaseContext->stopDatabases(synchronizer))
        if (synchronizer)
            synchronizer->taskCompleted();
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    {
        std::lock_guard<Lock> lock(m_mutex);

        for (auto* proposedDatabase : m_proposedDatabases) {
            if (proposedDatabase->details().name() == name && proposedDatabase->origin()->equal(origin))
                return String();
        }
    }

    return m_server->fullPathForDatabase(origin, name, createIfDoesNotExist);
}

bool DatabaseManager::hasEntryForOrigin(SecurityOrigin* origin)
{
    return m_server->hasEntryForOrigin(origin);
}

void DatabaseManager::origins(Vector<RefPtr<SecurityOrigin>>& result)
{
    m_server->origins(result);
}

bool DatabaseManager::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& result)
{
    return m_server->databaseNamesForOrigin(origin, result);
}

DatabaseDetails DatabaseManager::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    {
        std::lock_guard<Lock> lock(m_mutex);

        for (auto* proposedDatabase : m_proposedDatabases) {
            if (proposedDatabase->details().name() == name && proposedDatabase->origin()->equal(origin)) {
                ASSERT(proposedDatabase->details().threadID() == std::this_thread::get_id() || isMainThread());

                return proposedDatabase->details();
            }
        }
    }

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

void DatabaseManager::deleteAllDatabasesImmediately()
{
    m_server->deleteAllDatabasesImmediately();
}

bool DatabaseManager::deleteOrigin(SecurityOrigin* origin)
{
    return m_server->deleteOrigin(origin);
}

bool DatabaseManager::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    return m_server->deleteDatabase(origin, name);
}

void DatabaseManager::closeAllDatabases()
{
    m_server->closeAllDatabases();
}

void DatabaseManager::logErrorMessage(ScriptExecutionContext* context, const String& message)
{
    context->addConsoleMessage(MessageSource::Storage, MessageLevel::Error, message);
}

} // namespace WebCore
