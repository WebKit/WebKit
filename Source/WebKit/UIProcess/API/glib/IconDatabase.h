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

#pragma once

#include <WebCore/NativeImage.h>
#include <WebCore/SQLiteDatabase.h>
#include <wtf/Condition.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class IconDatabaseClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~IconDatabaseClient() = default;

    virtual void didImportIconURLForPageURL(const String&) { }
    virtual void didImportIconDataForPageURL(const String&) { }
    virtual void didChangeIconForPageURL(const String&) { }
    virtual void didFinishURLImport() { }
};

class IconDatabase {
    WTF_MAKE_FAST_ALLOCATED;

private:
    class IconSnapshot {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        IconSnapshot() = default;

        IconSnapshot(const String& iconURL, int timestamp, WebCore::SharedBuffer* data)
            : m_iconURL(iconURL)
            , m_timestamp(timestamp)
            , m_data(data)
        { }

        const String& iconURL() const { return m_iconURL; }
        int timestamp() const { return m_timestamp; }
        WebCore::SharedBuffer* data() const { return m_data.get(); }

    private:
        String m_iconURL;
        int m_timestamp { 0 };
        RefPtr<WebCore::SharedBuffer> m_data;
    };

    class IconRecord : public RefCounted<IconRecord> {
    public:
        static Ref<IconRecord> create(const String& url)
        {
            return adoptRef(*new IconRecord(url));
        }
        ~IconRecord();

        time_t getTimestamp() { return m_stamp; }
        void setTimestamp(time_t stamp) { m_stamp = stamp; }

        void setImageData(RefPtr<WebCore::SharedBuffer>&&);
        WebCore::NativeImagePtr image(const WebCore::IntSize&);

        String iconURL() { return m_iconURL; }

        enum class ImageDataStatus { Present, Missing, Unknown };
        ImageDataStatus imageDataStatus();

        HashSet<String>& retainingPageURLs() { return m_retainingPageURLs; }

        IconSnapshot snapshot(bool forDeletion = false) const;

    private:
        IconRecord(const String& url);

        String m_iconURL;
        time_t m_stamp { 0 };
        RefPtr<WebCore::SharedBuffer> m_imageData;
        WebCore::NativeImagePtr m_image;

        HashSet<String> m_retainingPageURLs;

        // This allows us to cache whether or not a SiteIcon has had its data set yet
        // This helps the IconDatabase know if it has to set the data on a new object or not,
        // and also to determine if the icon is missing data or if it just hasn't been brought
        // in from the DB yet
        bool m_dataSet { false };
    };

    class PageURLSnapshot {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        PageURLSnapshot() = default;

        PageURLSnapshot(const String& pageURL, const String& iconURL)
            : m_pageURL(pageURL)
            , m_iconURL(iconURL)
        { }

        const String& pageURL() const { return m_pageURL; }
        const String& iconURL() const { return m_iconURL; }

    private:
        String m_pageURL;
        String m_iconURL;
    };

    class PageURLRecord {
        WTF_MAKE_NONCOPYABLE(PageURLRecord); WTF_MAKE_FAST_ALLOCATED;
    public:
        PageURLRecord(const String& pageURL);
        ~PageURLRecord();

        inline String url() const { return m_pageURL; }

        void setIconRecord(RefPtr<IconRecord>&&);
        IconRecord* iconRecord() { return m_iconRecord.get(); }

        PageURLSnapshot snapshot(bool forDeletion = false) const;

        // Returns false if the page wasn't retained beforehand, true if the retain count was already 1 or higher.
        bool retain(int count)
        {
            bool wasRetained = m_retainCount > 0;
            m_retainCount += count;
            return wasRetained;
        }

        // Returns true if the page is still retained after the call. False if the retain count just dropped to 0.
        bool release(int count)
        {
            ASSERT(m_retainCount >= count);
            m_retainCount -= count;
            return m_retainCount > 0;
        }

        int retainCount() const { return m_retainCount; }

    private:
        String m_pageURL;
        RefPtr<IconRecord> m_iconRecord;
        int m_retainCount { 0 };
    };

    class MainThreadNotifier {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        MainThreadNotifier()
            : m_timer(RunLoop::main(), this, &MainThreadNotifier::timerFired)
        {
            m_timer.setPriority(RunLoopSourcePriority::MainThreadDispatcherTimer);
        }

        void setActive(bool active)
        {
            m_isActive.store(active);
        }

        void notify(Function<void()>&& notification)
        {
            if (!m_isActive.load())
                return;

            {
                LockHolder locker(m_notificationQueueLock);
                m_notificationQueue.append(WTFMove(notification));
            }

            if (!m_timer.isActive())
                m_timer.startOneShot(0_s);
        }

        void stop()
        {
            setActive(false);
            m_timer.stop();
            LockHolder locker(m_notificationQueueLock);
            m_notificationQueue.clear();
        }

    private:
        void timerFired()
        {
            Deque<Function<void()>> notificationQueue;
            {
                LockHolder locker(m_notificationQueueLock);
                notificationQueue = WTFMove(m_notificationQueue);
            }

            if (!m_isActive.load())
                return;

            while (!notificationQueue.isEmpty()) {
                auto function = notificationQueue.takeFirst();
                function();
            }
        }

        Deque<Function<void()>> m_notificationQueue;
        Lock m_notificationQueueLock;
        Atomic<bool> m_isActive;
        RunLoop::Timer<MainThreadNotifier> m_timer;
    };

// *** Main Thread Only ***
public:
    IconDatabase();
    ~IconDatabase();

    enum class IconLoadDecision { Yes, No, Unknown };

    void setClient(std::unique_ptr<IconDatabaseClient>&&);

    bool open(const String& directory, const String& filename);
    void close();

    void removeAllIcons();

    void retainIconForPageURL(const String&);
    void releaseIconForPageURL(const String&);
    void setIconDataForIconURL(RefPtr<WebCore::SharedBuffer>&&, const String& iconURL);
    void setIconURLForPageURL(const String& iconURL, const String& pageURL);

    enum class IsKnownIcon { No, Yes };
    std::pair<WebCore::NativeImagePtr, IsKnownIcon> synchronousIconForPageURL(const String&, const WebCore::IntSize&);
    String synchronousIconURLForPageURL(const String&);
    bool synchronousIconDataKnownForIconURL(const String&);
    IconLoadDecision synchronousLoadDecisionForIconURL(const String&);

    void setEnabled(bool);
    bool isEnabled() const;

    void setPrivateBrowsingEnabled(bool flag);
    bool isPrivateBrowsingEnabled() const;

    static void delayDatabaseCleanup();
    static void allowDatabaseCleanup();
    static void checkIntegrityBeforeOpening();

private:
    void wakeSyncThread();
    void scheduleOrDeferSyncTimer();
    void syncTimerFired();

    RunLoop::Timer<IconDatabase> m_syncTimer;
    RefPtr<Thread> m_syncThread;
    bool m_syncThreadRunning { false };
    bool m_scheduleOrDeferSyncTimerRequested { false };

// *** Any Thread ***
public:
    bool isOpen() const;
    String databasePath() const;
    static String defaultDatabaseFilename();

private:
    Ref<IconRecord> getOrCreateIconRecord(const String& iconURL);
    PageURLRecord* getOrCreatePageURLRecord(const String& pageURL);

    bool m_isEnabled { false };
    bool m_privateBrowsingEnabled { false };

    mutable Lock m_syncLock;
    Condition m_syncCondition;
    String m_databaseDirectory;
    // Holding m_syncLock is required when accessing m_completeDatabasePath
    String m_completeDatabasePath;

    bool m_threadTerminationRequested { false };
    bool m_removeIconsRequested { false };
    bool m_iconURLImportComplete { false };
    bool m_syncThreadHasWorkToDo { false };

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
    bool m_retainOrReleaseIconRequested { false };

// *** Sync Thread Only ***
public:
    bool shouldStopThreadActivity() const;

private:
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

    bool m_initialPruningComplete { false };

    void setIconURLForPageURLInSQLDatabase(const String&, const String&);
    void setIconIDForPageURLInSQLDatabase(int64_t, const String&);
    void removePageURLFromSQLDatabase(const String& pageURL);
    int64_t getIconIDForIconURLFromSQLDatabase(const String& iconURL);
    int64_t addIconURLToSQLDatabase(const String&);
    RefPtr<WebCore::SharedBuffer> getImageDataForIconURLFromSQLDatabase(const String& iconURL);
    void removeIconFromSQLDatabase(const String& iconURL);
    void writeIconSnapshotToSQLDatabase(const IconSnapshot&);

    void performPendingRetainAndReleaseOperations();

    // Methods to dispatch client callbacks on the main thread
    void dispatchDidImportIconURLForPageURLOnMainThread(const String&);
    void dispatchDidImportIconDataForPageURLOnMainThread(const String&);
    void dispatchDidFinishURLImportOnMainThread();

    // The client is set by the main thread before the thread starts, and from then on is only used by the sync thread
    std::unique_ptr<IconDatabaseClient> m_client;

    WebCore::SQLiteDatabase m_syncDB;

    std::unique_ptr<WebCore::SQLiteStatement> m_setIconIDForPageURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_removePageURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_getIconIDForIconURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_getImageDataForIconURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_addIconToIconInfoStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_addIconToIconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_getImageDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deletePageURLsForIconURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deleteIconFromIconInfoStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deleteIconFromIconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateIconInfoStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateIconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_setIconInfoStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_setIconDataStatement;

    MainThreadNotifier m_mainThreadNotifier;
};

} // namespace WebKit
