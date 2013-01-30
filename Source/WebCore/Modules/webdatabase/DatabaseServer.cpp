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
#include "DatabaseServer.h"

#if ENABLE(SQL_DATABASE)

#include "DatabaseTracker.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

void DatabaseServer::initialize(const String& databasePath)
{
#if !PLATFORM(CHROMIUM)
    DatabaseTracker::initializeTracker(databasePath);
#else
    UNUSED_PARAM(databasePath);
#endif
}

void DatabaseServer::setClient(DatabaseManagerClient* client)
{
#if !PLATFORM(CHROMIUM)
    DatabaseTracker::tracker().setClient(client);
#else
    UNUSED_PARAM(client);
#endif
}

String DatabaseServer::databaseDirectoryPath() const
{
#if !PLATFORM(CHROMIUM)
    return DatabaseTracker::tracker().databaseDirectoryPath();
#else
    return "";
#endif
}

void DatabaseServer::setDatabaseDirectoryPath(const String& path)
{
#if !PLATFORM(CHROMIUM)
    DatabaseTracker::tracker().setDatabaseDirectoryPath(path);
#endif
}

String DatabaseServer::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return DatabaseTracker::tracker().fullPathForDatabase(origin, name, createIfDoesNotExist);
}

#if !PLATFORM(CHROMIUM)
bool DatabaseServer::hasEntryForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().hasEntryForOrigin(origin);
}

void DatabaseServer::origins(Vector<RefPtr<SecurityOrigin> >& result)
{
    DatabaseTracker::tracker().origins(result);
}

bool DatabaseServer::databaseNamesForOrigin(SecurityOrigin* origin, Vector<String>& result)
{
    return DatabaseTracker::tracker().databaseNamesForOrigin(origin, result);
}

DatabaseDetails DatabaseServer::detailsForNameAndOrigin(const String& name, SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().detailsForNameAndOrigin(name, origin);
}

unsigned long long DatabaseServer::usageForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().usageForOrigin(origin);
}

unsigned long long DatabaseServer::quotaForOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().quotaForOrigin(origin);
}

void DatabaseServer::setQuota(SecurityOrigin* origin, unsigned long long quotaSize)
{
    DatabaseTracker::tracker().setQuota(origin, quotaSize);
}

void DatabaseServer::deleteAllDatabases()
{
    DatabaseTracker::tracker().deleteAllDatabases();
}

bool DatabaseServer::deleteOrigin(SecurityOrigin* origin)
{
    return DatabaseTracker::tracker().deleteOrigin(origin);
}

bool DatabaseServer::deleteDatabase(SecurityOrigin* origin, const String& name)
{
    return DatabaseTracker::tracker().deleteDatabase(origin, name);
}

// From a secondary thread, must be thread safe with its data
void DatabaseServer::scheduleNotifyDatabaseChanged(SecurityOrigin* origin, const String& name)
{
    DatabaseTracker::tracker().scheduleNotifyDatabaseChanged(origin, name);
}

void DatabaseServer::databaseChanged(DatabaseBackend* database)
{
    DatabaseTracker::tracker().databaseChanged(database);
}

#else // PLATFORM(CHROMIUM)
void DatabaseServer::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    DatabaseTracker::tracker().closeDatabasesImmediately(originIdentifier, name);
}
#endif // PLATFORM(CHROMIUM)

void DatabaseServer::interruptAllDatabasesForContext(const ScriptExecutionContext* context)
{
    DatabaseTracker::tracker().interruptAllDatabasesForContext(context);
}

bool DatabaseServer::canEstablishDatabase(ScriptExecutionContext* context, const String& name, const String& displayName, unsigned long estimatedSize)
{
    return DatabaseTracker::tracker().canEstablishDatabase(context, name, displayName, estimatedSize);
}

void DatabaseServer::addOpenDatabase(DatabaseBackend* database)
{
    DatabaseTracker::tracker().addOpenDatabase(database);
}

void DatabaseServer::removeOpenDatabase(DatabaseBackend* database)
{
    DatabaseTracker::tracker().removeOpenDatabase(database);
}

void DatabaseServer::setDatabaseDetails(SecurityOrigin* origin, const String& name, const String& displayName, unsigned long estimatedSize)
{
    DatabaseTracker::tracker().setDatabaseDetails(origin, name, displayName, estimatedSize);
}

unsigned long long DatabaseServer::getMaxSizeForDatabase(const DatabaseBackend* database)
{
    return DatabaseTracker::tracker().getMaxSizeForDatabase(database);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
