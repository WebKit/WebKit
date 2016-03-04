/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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
 
#ifndef IconDatabase_h
#define IconDatabase_h

#include "IconDatabaseBase.h"
#include <wtf/text/WTFString.h>

#if ENABLE(ICONDATABASE)
#include "SQLiteDatabase.h"
#include "Timer.h"
#include <wtf/Condition.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#endif

namespace WebCore { 

#if !ENABLE(ICONDATABASE)

// Dummy version of IconDatabase that does nothing.
class IconDatabase final : public IconDatabaseBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void delayDatabaseCleanup() { }
    static void allowDatabaseCleanup() { }
    static void checkIntegrityBeforeOpening() { }

    // FIXME: Is it really helpful to return a filename here rather than just the null string?
    static String defaultDatabaseFilename() { return ASCIILiteral("WebpageIcons.db"); }
};

#else

class IconRecord;
class IconSnapshot;
class PageURLRecord;
class PageURLSnapshot;
class SuddenTerminationDisabler;

class IconDatabase final : public IconDatabaseBase {
    WTF_MAKE_FAST_ALLOCATED;
    
// *** Main Thread Only ***
public:
    WEBCORE_EXPORT IconDatabase();
    ~IconDatabase();

    WEBCORE_EXPORT void setClient(IconDatabaseClient*) override;

    WEBCORE_EXPORT bool open(const String& directory, const String& filename) override;
    WEBCORE_EXPORT void close() override;
            
    WEBCORE_EXPORT void removeAllIcons() override;

    void readIconForPageURLFromDisk(const String&);

    WEBCORE_EXPORT Image* defaultIcon(const IntSize&) override;

    WEBCORE_EXPORT void retainIconForPageURL(const String&) override;
    WEBCORE_EXPORT void releaseIconForPageURL(const String&) override;
    WEBCORE_EXPORT void setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String&) override;
    WEBCORE_EXPORT void setIconURLForPageURL(const String& iconURL, const String& pageURL) override;

    WEBCORE_EXPORT Image* synchronousIconForPageURL(const String&, const IntSize&) override;
    PassNativeImagePtr synchronousNativeIconForPageURL(const String& pageURLOriginal, const IntSize&) override;
    WEBCORE_EXPORT String synchronousIconURLForPageURL(const String&) override;
    bool synchronousIconDataKnownForIconURL(const String&) override;
    WEBCORE_EXPORT IconLoadDecision synchronousLoadDecisionForIconURL(const String&, DocumentLoader*) override;

    WEBCORE_EXPORT void setEnabled(bool) override;
    WEBCORE_EXPORT bool isEnabled() const override;

    WEBCORE_EXPORT void setPrivateBrowsingEnabled(bool flag) override;
    bool isPrivateBrowsingEnabled() const;

    WEBCORE_EXPORT static void delayDatabaseCleanup();
    WEBCORE_EXPORT static void allowDatabaseCleanup();
    WEBCORE_EXPORT static void checkIntegrityBeforeOpening();

    // Support for WebCoreStatistics in WebKit
    WEBCORE_EXPORT size_t pageURLMappingCount() override;
    WEBCORE_EXPORT size_t retainedPageURLCount() override;
    WEBCORE_EXPORT size_t iconRecordCount() override;
    WEBCORE_EXPORT size_t iconRecordCountWithData() override;

private:
    friend IconDatabaseBase& iconDatabase();

    void notifyPendingLoadDecisions();

    void wakeSyncThread();
    void scheduleOrDeferSyncTimer();
    void syncTimerFired();
    
    Timer m_syncTimer;
    ThreadIdentifier m_syncThread;
    bool m_syncThreadRunning;
    
    HashSet<RefPtr<DocumentLoader>> m_loadersPendingDecision;

    RefPtr<IconRecord> m_defaultIconRecord;

    bool m_scheduleOrDeferSyncTimerRequested;
    std::unique_ptr<SuddenTerminationDisabler> m_disableSuddenTerminationWhileSyncTimerScheduled;

// *** Any Thread ***
public:
    WEBCORE_EXPORT bool isOpen() const override;
    WEBCORE_EXPORT String databasePath() const override;
    WEBCORE_EXPORT static String defaultDatabaseFilename();

private:
    PassRefPtr<IconRecord> getOrCreateIconRecord(const String& iconURL);
    PageURLRecord* getOrCreatePageURLRecord(const String& pageURL);
    
    bool m_isEnabled;
    bool m_privateBrowsingEnabled;

    mutable Lock m_syncLock;
    Condition m_syncCondition;
    String m_databaseDirectory;
    // Holding m_syncLock is required when accessing m_completeDatabasePath
    String m_completeDatabasePath;

    bool m_threadTerminationRequested;
    bool m_removeIconsRequested;
    bool m_iconURLImportComplete;
    bool m_syncThreadHasWorkToDo;
    std::unique_ptr<SuddenTerminationDisabler> m_disableSuddenTerminationWhileSyncThreadHasWorkToDo;

    Lock m_urlAndIconLock;
    // Holding m_urlAndIconLock is required when accessing any of the following data structures or the objects they contain
    HashMap<String, IconRecord*> m_iconURLToRecordMap;
    HashMap<String, PageURLRecord*> m_pageURLToRecordMap;
    HashSet<String> m_retainedPageURLs;

    Lock m_pendingSyncLock;
    // Holding m_pendingSyncLock is required when accessing any of the following data structures
    HashMap<String, PageURLSnapshot> m_pageURLsPendingSync;
    HashMap<String, IconSnapshot> m_iconsPendingSync;
    
    Lock m_pendingReadingLock;    
    // Holding m_pendingSyncLock is required when accessing any of the following data structures - when dealing with IconRecord*s, holding m_urlAndIconLock is also required
    HashSet<String> m_pageURLsPendingImport;
    HashSet<String> m_pageURLsInterestedInIcons;
    HashSet<IconRecord*> m_iconsPendingReading;

    Lock m_urlsToRetainOrReleaseLock;
    // Holding m_urlsToRetainOrReleaseLock is required when accessing any of the following data structures.
    HashCountedSet<String> m_urlsToRetain;
    HashCountedSet<String> m_urlsToRelease;
    bool m_retainOrReleaseIconRequested;

// *** Sync Thread Only ***
public:
    WEBCORE_EXPORT bool shouldStopThreadActivity() const override;

private:    
    static void iconDatabaseSyncThreadStart(void *);
    void iconDatabaseSyncThread();
    
    // The following block of methods are called exclusively by the sync thread to manage i/o to and from the database
    // Each method should periodically monitor m_threadTerminationRequested when it makes sense to return early on shutdown
    void performOpenInitialization();
    bool checkIntegrity();
    void performURLImport();
    void syncThreadMainLoop();
    bool readFromDatabase();
    bool writeToDatabase();
    void pruneUnretainedIcons();
    void checkForDanglingPageURLs(bool pruneIfFound);
    void removeAllIconsOnThread();
    void deleteAllPreparedStatements();
    void* cleanupSyncThread();
    void performRetainIconForPageURL(const String&, int retainCount);
    void performReleaseIconForPageURL(const String&, int releaseCount);
    
    bool wasExcludedFromBackup();
    void setWasExcludedFromBackup();

    bool isOpenBesidesMainThreadCallbacks() const;
    void checkClosedAfterMainThreadCallback();

    bool m_initialPruningComplete;
        
    void setIconURLForPageURLInSQLDatabase(const String&, const String&);
    void setIconIDForPageURLInSQLDatabase(int64_t, const String&);
    void removePageURLFromSQLDatabase(const String& pageURL);
    int64_t getIconIDForIconURLFromSQLDatabase(const String& iconURL);
    int64_t addIconURLToSQLDatabase(const String&);
    PassRefPtr<SharedBuffer> getImageDataForIconURLFromSQLDatabase(const String& iconURL);
    void removeIconFromSQLDatabase(const String& iconURL);
    void writeIconSnapshotToSQLDatabase(const IconSnapshot&);    

    void performPendingRetainAndReleaseOperations();

    // Methods to dispatch client callbacks on the main thread
    void dispatchDidImportIconURLForPageURLOnMainThread(const String&);
    void dispatchDidImportIconDataForPageURLOnMainThread(const String&);
    void dispatchDidRemoveAllIconsOnMainThread();
    void dispatchDidFinishURLImportOnMainThread();
    std::atomic<uint32_t> m_mainThreadCallbackCount;
    
    // The client is set by the main thread before the thread starts, and from then on is only used by the sync thread
    IconDatabaseClient* m_client;
    
    SQLiteDatabase m_syncDB;
    
    std::unique_ptr<SQLiteStatement> m_setIconIDForPageURLStatement;
    std::unique_ptr<SQLiteStatement> m_removePageURLStatement;
    std::unique_ptr<SQLiteStatement> m_getIconIDForIconURLStatement;
    std::unique_ptr<SQLiteStatement> m_getImageDataForIconURLStatement;
    std::unique_ptr<SQLiteStatement> m_addIconToIconInfoStatement;
    std::unique_ptr<SQLiteStatement> m_addIconToIconDataStatement;
    std::unique_ptr<SQLiteStatement> m_getImageDataStatement;
    std::unique_ptr<SQLiteStatement> m_deletePageURLsForIconURLStatement;
    std::unique_ptr<SQLiteStatement> m_deleteIconFromIconInfoStatement;
    std::unique_ptr<SQLiteStatement> m_deleteIconFromIconDataStatement;
    std::unique_ptr<SQLiteStatement> m_updateIconInfoStatement;
    std::unique_ptr<SQLiteStatement> m_updateIconDataStatement;
    std::unique_ptr<SQLiteStatement> m_setIconInfoStatement;
    std::unique_ptr<SQLiteStatement> m_setIconDataStatement;
};

#endif // !ENABLE(ICONDATABASE)

} // namespace WebCore

#endif // IconDatabase_h
