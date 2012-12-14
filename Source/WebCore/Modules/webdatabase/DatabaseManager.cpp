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
#include "DatabaseContext.h"
#include "DatabaseTracker.h"
#include <wtf/UnusedParam.h>

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
#if !PLATFORM(CHROMIUM)
    DatabaseTracker::initializeTracker(databasePath);
#else
    UNUSED_PARAM(databasePath);
#endif
}

#if !PLATFORM(CHROMIUM)
void DatabaseManager::setClient(DatabaseManagerClient* client)
{
    DatabaseTracker::tracker().setClient(client);
}

String DatabaseManager::databaseDirectoryPath() const
{
    return DatabaseTracker::tracker().databaseDirectoryPath();
}

void DatabaseManager::setDatabaseDirectoryPath(const String& path)
{
    DatabaseTracker::tracker().setDatabaseDirectoryPath(path);
}
#endif

bool DatabaseManager::isAvailable()
{
    return AbstractDatabase::isAvailable();
}

void DatabaseManager::setIsAvailable(bool available)
{
    AbstractDatabase::setIsAvailable(available);
}

bool DatabaseManager::hasOpenDatabases(ScriptExecutionContext* context)
{
    return DatabaseContext::hasOpenDatabases(context);
}

void DatabaseManager::stopDatabases(ScriptExecutionContext* context, DatabaseTaskSynchronizer* synchronizer)
{
    DatabaseContext::stopDatabases(context, synchronizer);
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return DatabaseTracker::tracker().fullPathForDatabase(origin, name, createIfDoesNotExist);
}

#if !PLATFORM(CHROMIUM)
bool DatabaseManager::hasEntryForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().hasEntryForOrigin(origin);
}

void DatabaseManager::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    DatabaseTracker::tracker().origins(result);
}

bool DatabaseManager::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& result)
{
    return DatabaseTracker::tracker().databaseNamesForOrigin(origin, result);
}

DatabaseDetails DatabaseManager::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().detailsForNameAndOrigin(name, origin);
}

unsigned long long DatabaseManager::usageForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().usageForOrigin(origin);
}

unsigned long long DatabaseManager::quotaForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().quotaForOrigin(origin);
}

void DatabaseManager::setQuota(SecurityOrigin* origin, unsigned long long quotaSize)
{
    DatabaseTracker::tracker().setQuota(origin, quotaSize);
}

void DatabaseManager::deleteAllDatabases()
{
    DatabaseTracker::tracker().deleteAllDatabases();
}

bool DatabaseManager::deleteOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().deleteOrigin(origin);
}

bool DatabaseManager::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    return DatabaseTracker::tracker().deleteDatabase(origin, name);
}

#else // PLATFORM(CHROMIUM)
void DatabaseManager::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    DatabaseTracker::tracker().closeDatabasesImmediately(originIdentifier, name);
}
#endif // PLATFORM(CHROMIUM)

void DatabaseManager::interruptAllDatabasesForContext(const ScriptExecutionContext* context)
{
    DatabaseTracker::tracker().interruptAllDatabasesForContext(context);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
