/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if ENABLE(ICONDATABASE)
#include "SQLiteDatabase.h"
#endif

#include "StringHash.h"
#if ENABLE(ICONDATABASE)
#include "Threading.h"
#endif
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore { 

class DocumentLoader;
class Image;
class IntSize;
class IconDatabaseClient;
class IconRecord;
class IconSnapshot;
class KURL;
class PageURLRecord;
class PageURLSnapshot;
class SharedBuffer;

#if ENABLE(ICONDATABASE)
class SQLTransaction;
#endif

enum IconLoadDecision {
    IconLoadYes,
    IconLoadNo,
    IconLoadUnknown
};

class IconDatabase : Noncopyable {

// *** Main Thread Only ***
public:
    void setClient(IconDatabaseClient*);

    bool open(const String& path);
    void close();
            
    void removeAllIcons();

    Image* iconForPageURL(const String&, const IntSize&, bool cache = true);
    void readIconForPageURLFromDisk(const String&);
    String iconURLForPageURL(const String&);
    Image* defaultIcon(const IntSize&);

    void retainIconForPageURL(const String&);
    void releaseIconForPageURL(const String&);

    void setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String&);
    void setIconURLForPageURL(const String& iconURL, const String& pageURL);

    IconLoadDecision loadDecisionForIconURL(const String&, DocumentLoader*);
    bool iconDataKnownForIconURL(const String&);
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPrivateBrowsingEnabled(bool flag);
    bool isPrivateBrowsingEnabled() const;
    
    static void delayDatabaseCleanup();
    static void allowDatabaseCleanup();
    static void checkIntegrityBeforeOpening();
        
    // Support for WebCoreStatistics in WebKit
    size_t pageURLMappingCount();
    size_t retainedPageURLCount();
    size_t iconRecordCount();
    size_t iconRecordCountWithData();

private:
    IconDatabase();
    ~IconDatabase();
    friend IconDatabase* iconDatabase();

#if ENABLE(ICONDATABASE)
    static void notifyPendingLoadDecisionsOnMainThread(void*);
    void notifyPendingLoadDecisions();

    void wakeSyncThread();
    void scheduleOrDeferSyncTimer();
    OwnPtr<Timer<IconDatabase> > m_syncTimer;
    void syncTimerFired(Timer<IconDatabase>*);
    
    ThreadIdentifier m_syncThread;
    bool m_syncThreadRunning;
    
    HashSet<RefPtr<DocumentLoader> > m_loadersPendingDecision;

    IconRecord* m_defaultIconRecord;
#endif // ENABLE(ICONDATABASE)

// *** Any Thread ***
public:
    bool isOpen() const;
    String databasePath() const;
    static String defaultDatabaseFilename();

#if ENABLE(ICONDATABASE)
private:
    IconRecord* getOrCreateIconRecord(const String& iconURL);
    PageURLRecord* getOrCreatePageURLRecord(const String& pageURL);
    
    bool m_isEnabled;
    bool m_privateBrowsingEnabled;
    
    mutable Mutex m_syncLock;
    ThreadCondition m_syncCondition;
    String m_databaseDirectory;
    // Holding m_syncLock is required when accessing m_completeDatabasePath
    String m_completeDatabasePath;
    
    bool m_threadTerminationRequested;
    bool m_removeIconsRequested;
    bool m_iconURLImportComplete;
    
    Mutex m_urlAndIconLock;
    // Holding m_urlAndIconLock is required when accessing any of the following data structures or the objects they contain
    HashMap<String, IconRecord*> m_iconURLToRecordMap;
    HashMap<String, PageURLRecord*> m_pageURLToRecordMap;
    HashSet<String> m_retainedPageURLs;

    Mutex m_pendingSyncLock;
    // Holding m_pendingSyncLock is required when accessing any of the following data structures
    HashMap<String, PageURLSnapshot> m_pageURLsPendingSync;
    HashMap<String, IconSnapshot> m_iconsPendingSync;
    
    Mutex m_pendingReadingLock;    
    // Holding m_pendingSyncLock is required when accessing any of the following data structures - when dealing with IconRecord*s, holding m_urlAndIconLock is also required
    HashSet<String> m_pageURLsPendingImport;
    HashSet<String> m_pageURLsInterestedInIcons;
    HashSet<IconRecord*> m_iconsPendingReading;

// *** Sync Thread Only ***
public:
    // Should be used only on the sync thread and only by the Safari 2 Icons import procedure
    void importIconURLForPageURL(const String& iconURL, const String& pageURL);
    void importIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String& iconURL);
    
    bool shouldStopThreadActivity() const;

private:    
    static void* iconDatabaseSyncThreadStart(void *);
    void* iconDatabaseSyncThread();
    
    // The following block of methods are called exclusively by the sync thread to manage i/o to and from the database
    // Each method should periodically monitor m_threadTerminationRequested when it makes sense to return early on shutdown
    void performOpenInitialization();
    bool checkIntegrity();
    void performURLImport();
    void* syncThreadMainLoop();
    bool readFromDatabase();
    bool writeToDatabase();
    void pruneUnretainedIcons();
    void checkForDanglingPageURLs(bool pruneIfFound);
    void removeAllIconsOnThread();
    void deleteAllPreparedStatements();
    void* cleanupSyncThread();

    // Record (on disk) whether or not Safari 2-style icons were imported (once per dataabse)
    bool imported();
    void setImported(bool);
    
    bool m_initialPruningComplete;
        
    void setIconURLForPageURLInSQLDatabase(const String&, const String&);
    void setIconIDForPageURLInSQLDatabase(int64_t, const String&);
    void removePageURLFromSQLDatabase(const String& pageURL);
    int64_t getIconIDForIconURLFromSQLDatabase(const String& iconURL);
    int64_t addIconURLToSQLDatabase(const String&);
    PassRefPtr<SharedBuffer> getImageDataForIconURLFromSQLDatabase(const String& iconURL);
    void removeIconFromSQLDatabase(const String& iconURL);
    void writeIconSnapshotToSQLDatabase(const IconSnapshot&);    
    
    // The client is set by the main thread before the thread starts, and from then on is only used by the sync thread
    IconDatabaseClient* m_client;
    
    SQLiteDatabase m_syncDB;
    
    // Track whether the "Safari 2" import is complete and/or set in the database
    bool m_imported;
    bool m_isImportedSet;
    
    OwnPtr<SQLiteStatement> m_setIconIDForPageURLStatement;
    OwnPtr<SQLiteStatement> m_removePageURLStatement;
    OwnPtr<SQLiteStatement> m_getIconIDForIconURLStatement;
    OwnPtr<SQLiteStatement> m_getImageDataForIconURLStatement;
    OwnPtr<SQLiteStatement> m_addIconToIconInfoStatement;
    OwnPtr<SQLiteStatement> m_addIconToIconDataStatement;
    OwnPtr<SQLiteStatement> m_getImageDataStatement;
    OwnPtr<SQLiteStatement> m_deletePageURLsForIconURLStatement;
    OwnPtr<SQLiteStatement> m_deleteIconFromIconInfoStatement;
    OwnPtr<SQLiteStatement> m_deleteIconFromIconDataStatement;
    OwnPtr<SQLiteStatement> m_updateIconInfoStatement;
    OwnPtr<SQLiteStatement> m_updateIconDataStatement;
    OwnPtr<SQLiteStatement> m_setIconInfoStatement;
    OwnPtr<SQLiteStatement> m_setIconDataStatement;
#endif // ENABLE(ICONDATABASE)
};

// Function to obtain the global icon database.
IconDatabase* iconDatabase();

} // namespace WebCore

#endif
