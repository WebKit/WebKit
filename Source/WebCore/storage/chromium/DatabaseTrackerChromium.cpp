/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DatabaseTracker.h"

#if ENABLE(DATABASE)

#include "AbstractDatabase.h"
#include "DatabaseObserver.h"
#include "QuotaTracker.h"
#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SQLiteFileSystem.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

DatabaseTracker& DatabaseTracker::tracker()
{
    DEFINE_STATIC_LOCAL(DatabaseTracker, tracker, (""));
    return tracker;
}

DatabaseTracker::DatabaseTracker(const String&)
{
    SQLiteFileSystem::registerSQLiteVFS();
}

bool DatabaseTracker::canEstablishDatabase(ScriptExecutionContext* scriptExecutionContext, const String& name, const String& displayName, unsigned long estimatedSize)
{
    return DatabaseObserver::canEstablishDatabase(scriptExecutionContext, name, displayName, estimatedSize);
}

void DatabaseTracker::setDatabaseDetails(SecurityOrigin*, const String&, const String&, unsigned long)
{
    // Chromium sets the database details when the database is opened
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool)
{
    return origin->databaseIdentifier() + "/" + name + "#";
}

void DatabaseTracker::addOpenDatabase(AbstractDatabase* database)
{
    ASSERT(database->scriptExecutionContext()->isContextThread());
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        m_openDatabaseMap.set(new DatabaseOriginMap());

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(database->securityOrigin());
    if (!nameMap) {
        nameMap = new DatabaseNameMap();
        m_openDatabaseMap->set(database->securityOrigin(), nameMap);
    }

    String name(database->stringIdentifier());
    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet) {
        databaseSet = new DatabaseSet();
        nameMap->set(name, databaseSet);
    }

    databaseSet->add(database);

    DatabaseObserver::databaseOpened(database);
}

class TrackerRemoveOpenDatabaseTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<TrackerRemoveOpenDatabaseTask> create(PassRefPtr<AbstractDatabase> database)
    {
        return new TrackerRemoveOpenDatabaseTask(database);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        DatabaseTracker::tracker().removeOpenDatabase(m_database.get());
    }

private:
    TrackerRemoveOpenDatabaseTask(PassRefPtr<AbstractDatabase> database)
        : m_database(database)
    {
    }

    RefPtr<AbstractDatabase> m_database;
};

void DatabaseTracker::removeOpenDatabase(AbstractDatabase* database)
{
    if (!database->scriptExecutionContext()->isContextThread()) {
        database->scriptExecutionContext()->postTask(TrackerRemoveOpenDatabaseTask::create(database));
        return;
    }

    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    ASSERT(m_openDatabaseMap);
    DatabaseNameMap* nameMap = m_openDatabaseMap->get(database->securityOrigin());
    ASSERT(nameMap);
    String name(database->stringIdentifier());
    DatabaseSet* databaseSet = nameMap->get(name);
    ASSERT(databaseSet);
    databaseSet->remove(database);

    if (databaseSet->isEmpty()) {
        nameMap->remove(name);
        delete databaseSet;
        if (nameMap->isEmpty()) {
            m_openDatabaseMap->remove(database->securityOrigin());
            delete nameMap;
        }
    }

    DatabaseObserver::databaseClosed(database);
}


void DatabaseTracker::getOpenDatabases(SecurityOrigin* origin, const String& name, HashSet<RefPtr<AbstractDatabase> >* databases)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);
    if (!nameMap)
        return;

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    for (DatabaseSet::iterator it = databaseSet->begin(); it != databaseSet->end(); ++it)
        databases->add(*it);
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const AbstractDatabase* database)
{
    unsigned long long spaceAvailable = 0;
    unsigned long long databaseSize = 0;
    QuotaTracker::instance().getDatabaseSizeAndSpaceAvailableToOrigin(
        database->securityOrigin()->databaseIdentifier(),
        database->stringIdentifier(), &databaseSize, &spaceAvailable);
    return databaseSize + spaceAvailable;
}

void DatabaseTracker::interruptAllDatabasesForContext(const ScriptExecutionContext* context)
{
    Vector<RefPtr<AbstractDatabase> > openDatabases;
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

        if (!m_openDatabaseMap)
            return;

        DatabaseNameMap* nameMap = m_openDatabaseMap->get(context->securityOrigin());
        if (!nameMap)
            return;

        DatabaseNameMap::const_iterator dbNameMapEndIt = nameMap->end();
        for (DatabaseNameMap::const_iterator dbNameMapIt = nameMap->begin(); dbNameMapIt != dbNameMapEndIt; ++dbNameMapIt) {
            DatabaseSet* databaseSet = dbNameMapIt->second;
            DatabaseSet::const_iterator dbSetEndIt = databaseSet->end();
            for (DatabaseSet::const_iterator dbSetIt = databaseSet->begin(); dbSetIt != dbSetEndIt; ++dbSetIt) {
                if ((*dbSetIt)->scriptExecutionContext() == context)
                    openDatabases.append(*dbSetIt);
            }
        }
    }

    Vector<RefPtr<AbstractDatabase> >::const_iterator openDatabasesEndIt = openDatabases.end();
    for (Vector<RefPtr<AbstractDatabase> >::const_iterator openDatabasesIt = openDatabases.begin(); openDatabasesIt != openDatabasesEndIt; ++openDatabasesIt)
        (*openDatabasesIt)->interrupt();
}

}

#endif // ENABLE(DATABASE)
