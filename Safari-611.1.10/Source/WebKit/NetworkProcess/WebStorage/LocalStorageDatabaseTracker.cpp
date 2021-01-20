/*
 * Copyright (C) 2011, 2013, 2019 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/TextEncoding.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

namespace WebKit {
using namespace WebCore;

Ref<LocalStorageDatabaseTracker> LocalStorageDatabaseTracker::create(String&& localStorageDirectory)
{
    return adoptRef(*new LocalStorageDatabaseTracker(WTFMove(localStorageDirectory)));
}

LocalStorageDatabaseTracker::LocalStorageDatabaseTracker(String&& localStorageDirectory)
    : m_localStorageDirectory(WTFMove(localStorageDirectory))
{
    ASSERT(!RunLoop::isMain());

    SQLiteFileSystem::deleteDatabaseFile(databasePath("StorageTracker.db"));
}

String LocalStorageDatabaseTracker::localStorageDirectory() const
{
    return m_localStorageDirectory.isolatedCopy();
}

LocalStorageDatabaseTracker::~LocalStorageDatabaseTracker()
{
    ASSERT(!RunLoop::isMain());
}

String LocalStorageDatabaseTracker::databasePath(const SecurityOriginData& securityOrigin) const
{
    return databasePath(securityOrigin.databaseIdentifier() + ".localstorage");
}

void LocalStorageDatabaseTracker::didOpenDatabaseWithOrigin(const SecurityOriginData& securityOrigin)
{
    // FIXME: Tell clients that the origin was added.
}

void LocalStorageDatabaseTracker::deleteDatabaseWithOrigin(const SecurityOriginData& securityOrigin)
{
    auto path = databasePath(securityOrigin);
    if (!path.isEmpty())
        SQLiteFileSystem::deleteDatabaseFile(path);

    // FIXME: Tell clients that the origin was removed.
}

void LocalStorageDatabaseTracker::deleteAllDatabases()
{
    auto localStorageDirectory = this->localStorageDirectory();
    auto paths = FileSystem::listDirectory(localStorageDirectory, "*.localstorage");
    for (const auto& path : paths) {
        SQLiteFileSystem::deleteDatabaseFile(path);

        // FIXME: Call out to the client.
    }

    SQLiteFileSystem::deleteEmptyDatabaseDirectory(localStorageDirectory);
}

Vector<SecurityOriginData> LocalStorageDatabaseTracker::databasesModifiedSince(WallTime time)
{
    Vector<SecurityOriginData> databaseOriginsModified;
    auto databaseOrigins = origins();
    
    for (auto origin : databaseOrigins) {
        auto path = databasePath(origin);
        
        auto modificationTime = SQLiteFileSystem::databaseModificationTime(path);
        if (!modificationTime)
            continue;

        if (modificationTime.value() >= time)
            databaseOriginsModified.append(origin);
    }

    return databaseOriginsModified;
}

Vector<SecurityOriginData> LocalStorageDatabaseTracker::origins() const
{
    Vector<SecurityOriginData> databaseOrigins;
    auto paths = FileSystem::listDirectory(localStorageDirectory(), "*.localstorage");
    
    for (const auto& path : paths) {
        auto filename = FileSystem::pathGetFileName(path);
        auto originIdentifier = filename.substring(0, filename.length() - strlen(".localstorage"));
        auto origin = SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
        if (origin)
            databaseOrigins.append(origin.value());
        else
            RELEASE_LOG_ERROR(LocalStorageDatabaseTracker, "Unable to extract origin from path %s", path.utf8().data());
    }

    return databaseOrigins;
}

Vector<LocalStorageDatabaseTracker::OriginDetails> LocalStorageDatabaseTracker::originDetailsCrossThreadCopy()
{
    Vector<OriginDetails> result;
    auto databaseOrigins = origins();
    result.reserveInitialCapacity(databaseOrigins.size());

    for (const auto& origin : databaseOrigins) {
        String path = databasePath(origin);

        OriginDetails details;
        details.originIdentifier = crossThreadCopy(origin.databaseIdentifier());
        details.creationTime = SQLiteFileSystem::databaseCreationTime(path);
        details.modificationTime = SQLiteFileSystem::databaseModificationTime(path);
        result.uncheckedAppend(WTFMove(details));
    }

    return result;
}

String LocalStorageDatabaseTracker::databasePath(const String& filename) const
{
    auto localStorageDirectory = this->localStorageDirectory();
    if (!SQLiteFileSystem::ensureDatabaseDirectoryExists(localStorageDirectory)) {
        if (!m_localStorageDirectory.isNull())
            LOG_ERROR("Unable to create LocalStorage database path %s", m_localStorageDirectory.utf8().data());
        return String();
    }

#if PLATFORM(IOS_FAMILY)
    platformMaybeExcludeFromBackup();
#endif

    return SQLiteFileSystem::appendDatabaseFileNameToPath(localStorageDirectory, filename);
}

} // namespace WebKit
