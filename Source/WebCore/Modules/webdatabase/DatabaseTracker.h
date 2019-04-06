/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DatabaseDetails.h"
#include "ExceptionOr.h"
#include "SQLiteDatabase.h"
#include "SecurityOriginData.h"
#include "SecurityOriginHash.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Database;
class DatabaseContext;
class DatabaseManagerClient;
class OriginLock;
class SecurityOrigin;
struct SecurityOriginData;

enum class CurrentQueryBehavior { Interrupt, RunToCompletion };

class DatabaseTracker {
    WTF_MAKE_NONCOPYABLE(DatabaseTracker); WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: This is a hack so we can easily delete databases from the UI process in WebKit2.
    WEBCORE_EXPORT static std::unique_ptr<DatabaseTracker> trackerWithDatabasePath(const String& databasePath);

    static void initializeTracker(const String& databasePath);

    WEBCORE_EXPORT static DatabaseTracker& singleton();
    WEBCORE_EXPORT static bool isInitialized();
    // This singleton will potentially be used from multiple worker threads and the page's context thread simultaneously.  To keep this safe, it's
    // currently using 4 locks.  In order to avoid deadlock when taking multiple locks, you must take them in the correct order:
    // m_databaseGuard before quotaManager if both locks are needed.
    // m_openDatabaseMapGuard before quotaManager if both locks are needed.
    // m_databaseGuard and m_openDatabaseMapGuard currently don't overlap.
    // notificationMutex() is currently independent of the other locks.

    ExceptionOr<void> canEstablishDatabase(DatabaseContext&, const String& name, unsigned long long estimatedSize);
    ExceptionOr<void> retryCanEstablishDatabase(DatabaseContext&, const String& name, unsigned long long estimatedSize);

    void setDatabaseDetails(const SecurityOriginData&, const String& name, const String& displayName, unsigned long long estimatedSize);
    WEBCORE_EXPORT String fullPathForDatabase(const SecurityOriginData&, const String& name, bool createIfDoesNotExist);

    Vector<Ref<Database>> openDatabases();
    void addOpenDatabase(Database&);
    void removeOpenDatabase(Database&);

    unsigned long long maximumSize(Database&);

    WEBCORE_EXPORT void closeAllDatabases(CurrentQueryBehavior = CurrentQueryBehavior::RunToCompletion);

    WEBCORE_EXPORT Vector<SecurityOriginData> origins();
    WEBCORE_EXPORT Vector<String> databaseNames(const SecurityOriginData&);

    DatabaseDetails detailsForNameAndOrigin(const String&, const SecurityOriginData&);

    WEBCORE_EXPORT unsigned long long usage(const SecurityOriginData&);
    WEBCORE_EXPORT unsigned long long quota(const SecurityOriginData&);
    WEBCORE_EXPORT void setQuota(const SecurityOriginData&, unsigned long long);
    RefPtr<OriginLock> originLockFor(const SecurityOriginData&);

    WEBCORE_EXPORT void deleteAllDatabasesImmediately();
    WEBCORE_EXPORT void deleteDatabasesModifiedSince(WallTime);
    WEBCORE_EXPORT bool deleteOrigin(const SecurityOriginData&);
    WEBCORE_EXPORT bool deleteDatabase(const SecurityOriginData&, const String& name);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void removeDeletedOpenedDatabases();
    WEBCORE_EXPORT static bool deleteDatabaseFileIfEmpty(const String&);

    // MobileSafari will grab this mutex on the main thread before dispatching the task to 
    // clean up zero byte database files.  Any operations to open new database will have to
    // wait for that task to finish by waiting on this mutex.
    static Lock& openDatabaseMutex();
    
    WEBCORE_EXPORT static void emptyDatabaseFilesRemovalTaskWillBeScheduled();
    WEBCORE_EXPORT static void emptyDatabaseFilesRemovalTaskDidFinish();
#endif
    
    void setClient(DatabaseManagerClient*);

    // From a secondary thread, must be thread safe with its data
    void scheduleNotifyDatabaseChanged(const SecurityOriginData&, const String& name);

    void doneCreatingDatabase(Database&);

private:
    explicit DatabaseTracker(const String& databasePath);

    ExceptionOr<void> hasAdequateQuotaForOrigin(const SecurityOriginData&, unsigned long long estimatedSize);

    bool hasEntryForOriginNoLock(const SecurityOriginData&);
    String fullPathForDatabaseNoLock(const SecurityOriginData&, const String& name, bool createIfDoesNotExist);
    Vector<String> databaseNamesNoLock(const SecurityOriginData&);
    unsigned long long quotaNoLock(const SecurityOriginData&);

    String trackerDatabasePath() const;

    enum TrackerCreationAction {
        DontCreateIfDoesNotExist,
        CreateIfDoesNotExist
    };
    void openTrackerDatabase(TrackerCreationAction);

    String originPath(const SecurityOriginData&) const;

    bool hasEntryForDatabase(const SecurityOriginData&, const String& databaseIdentifier);

    bool addDatabase(const SecurityOriginData&, const String& name, const String& path);

    enum class DeletionMode {
        Immediate,
#if PLATFORM(IOS_FAMILY)
        // Deferred deletion is currently only supported on iOS
        // (see removeDeletedOpenedDatabases etc, above).
        Deferred,
        Default = Deferred
#else
        Default = Immediate
#endif
    };

    bool deleteOrigin(const SecurityOriginData&, DeletionMode);
    bool deleteDatabaseFile(const SecurityOriginData&, const String& name, DeletionMode);

    void deleteOriginLockFor(const SecurityOriginData&);

    using DatabaseSet = HashSet<Database*>;
    using DatabaseNameMap = HashMap<String, DatabaseSet*>;
    using DatabaseOriginMap = HashMap<SecurityOriginData, DatabaseNameMap*>;

    Lock m_openDatabaseMapGuard;
    mutable std::unique_ptr<DatabaseOriginMap> m_openDatabaseMap;

    // This lock protects m_database, m_originLockMap, m_databaseDirectoryPath, m_originsBeingDeleted, m_beingCreated, and m_beingDeleted.
    Lock m_databaseGuard;
    SQLiteDatabase m_database;

    using OriginLockMap = HashMap<String, RefPtr<OriginLock>>;
    OriginLockMap m_originLockMap;

    String m_databaseDirectoryPath;

    DatabaseManagerClient* m_client { nullptr };

    HashMap<SecurityOriginData, std::unique_ptr<HashCountedSet<String>>> m_beingCreated;
    HashMap<SecurityOriginData, std::unique_ptr<HashSet<String>>> m_beingDeleted;
    HashSet<SecurityOriginData> m_originsBeingDeleted;
    bool isDeletingDatabaseOrOriginFor(const SecurityOriginData&, const String& name);
    void recordCreatingDatabase(const SecurityOriginData&, const String& name);
    void doneCreatingDatabase(const SecurityOriginData&, const String& name);
    bool creatingDatabase(const SecurityOriginData&, const String& name);
    bool canDeleteDatabase(const SecurityOriginData&, const String& name);
    void recordDeletingDatabase(const SecurityOriginData&, const String& name);
    void doneDeletingDatabase(const SecurityOriginData&, const String& name);
    bool isDeletingDatabase(const SecurityOriginData&, const String& name);
    bool canDeleteOrigin(const SecurityOriginData&);
    bool isDeletingOrigin(const SecurityOriginData&);
    void recordDeletingOrigin(const SecurityOriginData&);
    void doneDeletingOrigin(const SecurityOriginData&);

    static void scheduleForNotification();
    static void notifyDatabasesChanged();
};

} // namespace WebCore
