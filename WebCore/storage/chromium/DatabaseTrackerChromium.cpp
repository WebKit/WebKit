/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "CString.h"
#include "Database.h"
#include "DatabaseObserver.h"
#include "DatabaseThread.h"
#include "Document.h"
#include "QuotaTracker.h"
#include "SecurityOrigin.h"
#include "SQLiteFileSystem.h"
#include <wtf/HashSet.h>

namespace WebCore {

DatabaseTracker& DatabaseTracker::tracker()
{
    DEFINE_STATIC_LOCAL(DatabaseTracker, tracker, ());
    return tracker;
}

DatabaseTracker::DatabaseTracker()
{
    SQLiteFileSystem::registerSQLiteVFS();
}

bool DatabaseTracker::canEstablishDatabase(Document*, const String&, const String&, unsigned long)
{
    // In Chromium, a database can always be established (even though we might not
    // be able to write anything to it if the quota for this origin was exceeded)
    return true;
}

void DatabaseTracker::setDatabaseDetails(SecurityOrigin*, const String&, const String&, unsigned long)
{
    // Chromium sets the database details when the database is opened
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool)
{
    return origin->databaseIdentifier() + "/" + name;
}

void DatabaseTracker::addOpenDatabase(Database* database)
{
    ASSERT(isMainThread());
    DatabaseObserver::databaseOpened(database);
}

void DatabaseTracker::removeOpenDatabase(Database* database)
{
    ASSERT(isMainThread());
    DatabaseObserver::databaseClosed(database);
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const Database* database) const
{
    ASSERT(currentThread() == database->document()->databaseThread()->getThreadID());
    unsigned long long spaceAvailable = 0;
    unsigned long long databaseSize = 0;
    QuotaTracker::instance().getDatabaseSizeAndSpaceAvailableToOrigin(
        database->securityOrigin()->databaseIdentifier(),
        database->stringIdentifier(), &databaseSize, &spaceAvailable);
    return databaseSize + spaceAvailable;
}

}
