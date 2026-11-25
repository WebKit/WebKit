/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "StorageTrackerClient.h"
#include <WebCore/SQLiteDatabase.h>
#include <wtf/HashSet.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class StorageThread;
class SecurityOrigin;
class SecurityOriginData;
}

namespace WebKit {

class StorageTracker {
    WTF_MAKE_NONCOPYABLE(StorageTracker);
    WTF_MAKE_TZONE_ALLOCATED(StorageTracker);
public:
    NODELETE static void initializeTracker(StringImpl* storagePath, WebCore::StorageTrackerClient*);
    static StorageTracker& tracker();

    void setOriginDetails(StringImpl* originIdentifier, StringImpl* databaseFile);
    
    void deleteAllOrigins();
    void deleteOrigin(const WebCore::SecurityOriginData&);
    void deleteOriginWithIdentifier(StringImpl* originIdentifier);
    Vector<WebCore::SecurityOriginData> origins();
    uint64_t diskUsageForOrigin(WebCore::SecurityOrigin*);
    
    void cancelDeletingOrigin(StringImpl* originIdentifier);
    
    bool isActive() const { return m_isActive; }

    Seconds storageDatabaseIdleInterval() { return m_StorageDatabaseIdleInterval; }
    void setStorageDatabaseIdleInterval(Seconds interval) { m_StorageDatabaseIdleInterval = interval; }

    void syncFileSystemAndTrackerDatabase();

private:
    explicit StorageTracker(StringImpl* storagePath);

    void internalInitialize();

    RefPtr<StringImpl> trackerDatabasePath();
    void openTrackerDatabase(bool createIfDoesNotExist) WTF_REQUIRES_LOCK(m_databaseMutex);

    void importOriginIdentifiers();
    void finishedImportingOriginIdentifiers();
    
    void deleteTrackerFiles();
    RefPtr<StringImpl> databasePathForOrigin(StringImpl* originIdentifier) WTF_REQUIRES_LOCK(m_databaseMutex);

    bool canDeleteOrigin(StringImpl* originIdentifier);
    void willDeleteOrigin(StringImpl* originIdentifier) WTF_REQUIRES_LOCK(m_originSetMutex);
    void willDeleteAllOrigins() WTF_REQUIRES_LOCK(m_originSetMutex);

    void originFilePaths(Vector<RefPtr<StringImpl>>& paths);
    
    NODELETE void setIsActive(bool);

    // Sync to disk on background thread.
    void syncDeleteAllOrigins();
    void syncDeleteOrigin(StringImpl* originIdentifier);
    void syncSetOriginDetails(StringImpl* originIdentifier, StringImpl* databaseFile);
    void syncImportOriginIdentifiers();

    // Mutex for m_database. m_storageDirectoryPath is set in the constructor and never changes.
    Lock m_databaseMutex;
    WebCore::SQLiteDatabase m_database WTF_GUARDED_BY_LOCK(m_databaseMutex);
    const RefPtr<StringImpl> m_storageDirectoryPath;

    Lock m_clientMutex;
    WebCore::StorageTrackerClient* m_client;

    // Guard for m_originSet and m_originsBeingDeleted.
    Lock m_originSetMutex;
    typedef HashSet<RefPtr<StringImpl>> OriginSet;
    OriginSet m_originSet WTF_GUARDED_BY_LOCK(m_originSetMutex);
    OriginSet m_originsBeingDeleted WTF_GUARDED_BY_LOCK(m_originSetMutex);

    const UniqueRef<WebCore::StorageThread> m_thread;
    
    bool m_isActive;
    bool m_needsInitialization;
    Seconds m_StorageDatabaseIdleInterval;
};
    
} // namespace WebCore
