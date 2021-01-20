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

#include <WebCore/SQLiteDatabase.h>
#include <wtf/HashSet.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class StorageThread;
class SecurityOrigin;
class StorageTrackerClient;
struct SecurityOriginData;
}

namespace WebKit {

class StorageTracker {
    WTF_MAKE_NONCOPYABLE(StorageTracker);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void initializeTracker(const String& storagePath, WebCore::StorageTrackerClient*);
    static StorageTracker& tracker();

    void setOriginDetails(const String& originIdentifier, const String& databaseFile);
    
    void deleteAllOrigins();
    void deleteOrigin(const WebCore::SecurityOriginData&);
    void deleteOriginWithIdentifier(const String& originIdentifier);
    Vector<WebCore::SecurityOriginData> origins();
    long long diskUsageForOrigin(WebCore::SecurityOrigin*);
    
    void cancelDeletingOrigin(const String& originIdentifier);
    
    bool isActive();

    Seconds storageDatabaseIdleInterval() { return m_StorageDatabaseIdleInterval; }
    void setStorageDatabaseIdleInterval(Seconds interval) { m_StorageDatabaseIdleInterval = interval; }

    void syncFileSystemAndTrackerDatabase();

private:
    explicit StorageTracker(const String& storagePath);

    void internalInitialize();

    String trackerDatabasePath();
    void openTrackerDatabase(bool createIfDoesNotExist);

    void importOriginIdentifiers();
    void finishedImportingOriginIdentifiers();
    
    void deleteTrackerFiles();
    String databasePathForOrigin(const String& originIdentifier);

    bool canDeleteOrigin(const String& originIdentifier);
    void willDeleteOrigin(const String& originIdentifier);
    void willDeleteAllOrigins();

    void originFilePaths(Vector<String>& paths);
    
    void setIsActive(bool);

    // Sync to disk on background thread.
    void syncDeleteAllOrigins();
    void syncDeleteOrigin(const String& originIdentifier);
    void syncSetOriginDetails(const String& originIdentifier, const String& databaseFile);
    void syncImportOriginIdentifiers();

    // Mutex for m_database and m_storageDirectoryPath.
    Lock m_databaseMutex;
    WebCore::SQLiteDatabase m_database;
    String m_storageDirectoryPath;

    Lock m_clientMutex;
    WebCore::StorageTrackerClient* m_client;

    // Guard for m_originSet and m_originsBeingDeleted.
    Lock m_originSetMutex;
    typedef HashSet<String> OriginSet;
    OriginSet m_originSet;
    OriginSet m_originsBeingDeleted;

    std::unique_ptr<WebCore::StorageThread> m_thread;
    
    bool m_isActive;
    bool m_needsInitialization;
    Seconds m_StorageDatabaseIdleInterval;
};
    
} // namespace WebCore
