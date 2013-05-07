/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalStorageDatabaseTracker.h"

#include "WorkQueue.h"
#include <WebCore/FileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LocalStorageDatabaseTracker> LocalStorageDatabaseTracker::create(PassRefPtr<WorkQueue> queue)
{
    return adoptRef(new LocalStorageDatabaseTracker(queue));
}

LocalStorageDatabaseTracker::LocalStorageDatabaseTracker(PassRefPtr<WorkQueue> queue)
    : m_queue(queue)
{
}

LocalStorageDatabaseTracker::~LocalStorageDatabaseTracker()
{
}

void LocalStorageDatabaseTracker::setLocalStorageDirectory(const String& localStorageDirectory)
{
    m_queue->dispatch(bind(&LocalStorageDatabaseTracker::setLocalStorageDirectoryInternal, this, localStorageDirectory.isolatedCopy()));
}

String LocalStorageDatabaseTracker::databaseFilename(SecurityOrigin* securityOrigin) const
{
    return databaseFilename(securityOrigin->databaseIdentifier() + ".localstorage");
}

void LocalStorageDatabaseTracker::didOpenDatabaseWithOrigin(WebCore::SecurityOrigin*)
{
}

void LocalStorageDatabaseTracker::deleteEmptyDatabaseWithOrigin(WebCore::SecurityOrigin*)
{
}

void LocalStorageDatabaseTracker::setLocalStorageDirectoryInternal(const String& localStorageDirectory)
{
    if (m_database.isOpen())
        m_database.close();

    m_localStorageDirectory = localStorageDirectory;
    m_origins.clear();

    m_queue->dispatch(bind(&LocalStorageDatabaseTracker::importOriginIdentifiers, this));
}

String LocalStorageDatabaseTracker::databaseFilename(const String& filename) const
{
    if (!makeAllDirectories(m_localStorageDirectory)) {
        LOG_ERROR("Unabled to create LocalStorage database path %s", m_localStorageDirectory.utf8().data());
        return String();
    }

    return pathByAppendingComponent(m_localStorageDirectory, filename);
}

String LocalStorageDatabaseTracker::trackerDatabasePath() const
{
    return databaseFilename("StorageTracker.db");
}

void LocalStorageDatabaseTracker::openTrackerDatabase(DatabaseOpeningStrategy openingStrategy)
{
    String databasePath = trackerDatabasePath();

    if (!fileExists(databasePath) && openingStrategy == SkipIfNonExistent)
        return;

    if (!m_database.open(databasePath)) {
        LOG_ERROR("Failed to open databasePath %s.", databasePath.ascii().data());
        return;
    }

    // Since a WorkQueue isn't bound to a specific thread, we have to disable threading checks
    // even though we never access the database from different threads simultaneously.
    m_database.disableThreadingChecks();

    if (m_database.tableExists("Origins"))
        return;

    if (!m_database.executeCommand("CREATE TABLE Origins (origin TEXT UNIQUE ON CONFLICT REPLACE, path TEXT);"))
        LOG_ERROR("Failed to create Origins table.");
}

void LocalStorageDatabaseTracker::importOriginIdentifiers()
{
    openTrackerDatabase(SkipIfNonExistent);

    if (m_database.isOpen()) {
        SQLiteStatement statement(m_database, "SELECT origin FROM Origins");
        if (statement.prepare() != SQLResultOk) {
            LOG_ERROR("Failed to prepare statement.");
            return;
        }

        int result;

        while ((result = statement.step()) == SQLResultRow)
            m_origins.add(statement.getColumnText(0));

        if (result != SQLResultDone) {
            LOG_ERROR("Failed to read in all origins from the database.");
            return;
        }
    }

    updateTrackerDatabaseFromLocalStorageDatabaseFiles();
}

void LocalStorageDatabaseTracker::updateTrackerDatabaseFromLocalStorageDatabaseFiles()
{
    Vector<String> paths = listDirectory(m_localStorageDirectory, "*.localstorage");

    HashSet<String> origins(m_origins);
    HashSet<String> originsFromLocalStorageDatabaseFiles;

    for (size_t i = 0; i < paths.size(); ++i) {
        const String& path = paths[i];

        if (!path.endsWith(".localstorage"))
            continue;

        String filename = pathGetFileName(path);

        String originIdentifier = filename.substring(0, filename.length() - strlen(".localstorage"));

        // FIXME: Insert the origin and database pair.
        originsFromLocalStorageDatabaseFiles.add(originIdentifier);
    }

    for (auto it = origins.begin(), end = origins.end(); it != end; ++it) {
        const String& originIdentifier = *it;
        if (origins.contains(originIdentifier))
            continue;

        // This origin doesn't have a database file, delete it from the database.
        // FIXME: Do this.
    }
}

} // namespace WebKit
