/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IconDatabase.h"

#include "Logging.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/Image.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>

// For methods that are meant to support API from the main thread - should not be called internally
#define ASSERT_NOT_SYNC_THREAD() ASSERT(!m_syncThreadRunning || !IS_ICON_SYNC_THREAD())

// For methods that are meant to support the sync thread ONLY
#define IS_ICON_SYNC_THREAD() (m_syncThread == &Thread::current())
#define ASSERT_ICON_SYNC_THREAD() ASSERT(IS_ICON_SYNC_THREAD())

namespace WebKit {
using namespace WebCore;

static int databaseCleanupCounter = 0;

// This version number is in the DB and marks the current generation of the schema
// Currently, a mismatched schema causes the DB to be wiped and reset. This isn't
// so bad during development but in the future, we would need to write a conversion
// function to advance older released schemas to "current"
static const int currentDatabaseVersion = 6;

// Icons expire once every 4 days
static const int iconExpirationTime = 60*60*24*4;

static const Seconds updateTimerDelay { 5_s };

static bool checkIntegrityOnOpen = false;

// We are not interested in icons that have been unused for more than
// 30 days, delete them even if they have not been explicitly released.
static const Seconds notUsedIconExpirationTime { 60*60*24*30 };

#if !LOG_DISABLED || !ERROR_DISABLED
static String urlForLogging(const String& url)
{
    static const unsigned urlTruncationLength = 120;

    if (url.length() < urlTruncationLength)
        return url;
    return url.substring(0, urlTruncationLength) + "...";
}
#endif

class DefaultIconDatabaseClient final : public IconDatabaseClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void didImportIconURLForPageURL(const String&) override { }
    void didImportIconDataForPageURL(const String&) override { }
    void didChangeIconForPageURL(const String&) override { }
    void didFinishURLImport() override { }
};

static IconDatabaseClient* defaultClient()
{
    static IconDatabaseClient* defaultClient = new DefaultIconDatabaseClient;
    return defaultClient;
}

IconDatabase::IconRecord::IconRecord(const String& url)
    : m_iconURL(url)
{
}

IconDatabase::IconRecord::~IconRecord()
{
    LOG(IconDatabase, "Destroying IconRecord for icon url %s", m_iconURL.ascii().data());
}

NativeImagePtr IconDatabase::IconRecord::image(const IntSize&)
{
    // FIXME rdar://4680377 - For size right now, we are returning our one and only image and the Bridge
    // is resizing it in place. We need to actually store all the original representations here and return a native
    // one, or resize the best one to the requested size and cache that result.
    return m_image;
}

void IconDatabase::IconRecord::setImageData(RefPtr<SharedBuffer>&& data)
{
    m_dataSet = true;
    m_imageData = WTFMove(data);
    m_image = nullptr;

    if (!m_imageData || !m_imageData->size()) {
        m_imageData = nullptr;
        return;
    }

    auto image = BitmapImage::create();
    if (image->setData(RefPtr<SharedBuffer> { m_imageData }, true) < EncodedDataStatus::SizeAvailable) {
        LOG(IconDatabase, "Manual image data for iconURL '%s' FAILED - it was probably invalid image data", m_iconURL.ascii().data());
        m_imageData = nullptr;
        return;
    }

    m_image = image->nativeImageForCurrentFrame();
    if (!m_image) {
        LOG(IconDatabase, "Manual image data for iconURL '%s' FAILED - it was probably invalid image data", m_iconURL.ascii().data());
        m_imageData = nullptr;
    }
}

IconDatabase::IconRecord::ImageDataStatus IconDatabase::IconRecord::imageDataStatus()
{
    if (!m_dataSet)
        return ImageDataStatus::Unknown;
    if (!m_image)
        return ImageDataStatus::Missing;
    return ImageDataStatus::Present;
}

IconDatabase::IconSnapshot IconDatabase::IconRecord::snapshot(bool forDeletion) const
{
    if (forDeletion)
        return IconSnapshot(m_iconURL, 0, nullptr);

    return IconSnapshot(m_iconURL, m_stamp, m_imageData ? m_imageData.get() : nullptr);
}

IconDatabase::PageURLRecord::PageURLRecord(const String& pageURL)
    : m_pageURL(pageURL)
{
}

IconDatabase::PageURLRecord::~PageURLRecord()
{
    if (m_iconRecord)
        m_iconRecord->retainingPageURLs().remove(m_pageURL);
}

void IconDatabase::PageURLRecord::setIconRecord(RefPtr<IconRecord>&& icon)
{
    if (m_iconRecord)
        m_iconRecord->retainingPageURLs().remove(m_pageURL);

    m_iconRecord = WTFMove(icon);

    if (m_iconRecord)
        m_iconRecord->retainingPageURLs().add(m_pageURL);
}

IconDatabase::PageURLSnapshot IconDatabase::PageURLRecord::snapshot(bool forDeletion) const
{
    return { m_pageURL, (m_iconRecord && !forDeletion) ? m_iconRecord->iconURL() : String() };
}

// ************************
// *** Main Thread Only ***
// ************************

void IconDatabase::setClient(std::unique_ptr<IconDatabaseClient>&& client)
{
    // Don't allow a client change after the thread has already began
    // (setting the client should occur before the database is opened)
    ASSERT(!m_syncThreadRunning);
    if (!client || m_syncThreadRunning)
        return;

    m_client = WTFMove(client);
}

bool IconDatabase::open(const String& directory, const String& filename)
{
    ASSERT_NOT_SYNC_THREAD();

    if (!m_isEnabled)
        return false;

    if (isOpen()) {
        LOG_ERROR("Attempt to reopen the IconDatabase which is already open. Must close it first.");
        return false;
    }

    m_mainThreadNotifier.setActive(true);

    m_databaseDirectory = directory.isolatedCopy();

    // Formulate the full path for the database file
    m_completeDatabasePath = FileSystem::pathByAppendingComponent(m_databaseDirectory, filename);

    // Lock here as well as first thing in the thread so the thread doesn't actually commence until the createThread() call
    // completes and m_syncThreadRunning is properly set
    m_syncLock.lock();
    m_syncThread = Thread::create("WebCore: IconDatabase", [this] {
        iconDatabaseSyncThread();
    });
    m_syncThreadRunning = true;
    m_syncLock.unlock();
    return true;
}

void IconDatabase::close()
{
    ASSERT_NOT_SYNC_THREAD();

    m_mainThreadNotifier.stop();

    if (m_syncThreadRunning) {
        // Set the flag to tell the sync thread to wrap it up
        m_threadTerminationRequested = true;

        // Wake up the sync thread if it's waiting
        wakeSyncThread();

        // Wait for the sync thread to terminate
        m_syncThread->waitForCompletion();
    }

    m_syncThreadRunning = false;
    m_threadTerminationRequested = false;
    m_removeIconsRequested = false;

    m_syncDB.close();

    m_client = nullptr;
}

void IconDatabase::removeAllIcons()
{
    ASSERT_NOT_SYNC_THREAD();

    if (!isOpen())
        return;

    LOG(IconDatabase, "Requesting background thread to remove all icons");

    // Clear the in-memory record of every IconRecord, anything waiting to be read from disk, and anything waiting to be written to disk
    {
        LockHolder locker(m_urlAndIconLock);

        // Clear the IconRecords for every page URL - RefCounting will cause the IconRecords themselves to be deleted
        // We don't delete the actual PageRecords because we have the "retain icon for url" count to keep track of
        for (auto& pageURL : m_pageURLToRecordMap.values())
            pageURL->setIconRecord(nullptr);

        // Clear the iconURL -> IconRecord map
        m_iconURLToRecordMap.clear();

        // Clear all in-memory records of things that need to be synced out to disk
        {
            LockHolder locker(m_pendingSyncLock);
            m_pageURLsPendingSync.clear();
            m_iconsPendingSync.clear();
        }

        // Clear all in-memory records of things that need to be read in from disk
        {
            LockHolder locker(m_pendingReadingLock);
            m_pageURLsPendingImport.clear();
            m_pageURLsInterestedInIcons.clear();
            m_iconsPendingReading.clear();
        }
    }

    m_removeIconsRequested = true;
    wakeSyncThread();
}

static bool documentCanHaveIcon(const String& documentURL)
{
    return !documentURL.isEmpty() && !protocolIs(documentURL, "about");
}

std::pair<NativeImagePtr, IconDatabase::IsKnownIcon> IconDatabase::synchronousIconForPageURL(const String& pageURLOriginal, const IntSize& size)
{
    ASSERT_NOT_SYNC_THREAD();

    // pageURLOriginal cannot be stored without being deep copied first.
    // We should go our of our way to only copy it if we have to store it

    if (!isOpen() || !documentCanHaveIcon(pageURLOriginal))
        return { nullptr, IsKnownIcon::No };

    LockHolder locker(m_urlAndIconLock);

    performPendingRetainAndReleaseOperations();

    String pageURLCopy; // Creates a null string for easy testing

    PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURLOriginal);
    if (!pageRecord) {
        pageURLCopy = pageURLOriginal.isolatedCopy();
        pageRecord = getOrCreatePageURLRecord(pageURLCopy);
    }

    // If pageRecord is nullptr, one of two things is true -
    // 1 - The initial url import is incomplete and this pageURL was marked to be notified once it is complete if an iconURL exists
    // 2 - The initial url import IS complete and this pageURL has no icon
    if (!pageRecord) {
        LockHolder locker(m_pendingReadingLock);

        // Import is ongoing, there might be an icon. In this case, register to be notified when the icon comes in
        // If we ever reach this condition, we know we've already made the pageURL copy
        if (!m_iconURLImportComplete)
            m_pageURLsInterestedInIcons.add(pageURLCopy);

        return { nullptr, IsKnownIcon::No };
    }

    IconRecord* iconRecord = pageRecord->iconRecord();

    // If the initial URL import isn't complete, it's possible to have a PageURL record without an associated icon
    // In this case, the pageURL is already in the set to alert the client when the iconURL mapping is complete so
    // we can just bail now
    if (!m_iconURLImportComplete && !iconRecord)
        return { nullptr, IsKnownIcon::No };

    // Assuming we're done initializing and cleanup is allowed,
    // the only way we should *not* have an icon record is if this pageURL is retained but has no icon yet.
    ASSERT(iconRecord || databaseCleanupCounter || m_retainedPageURLs.contains(pageURLOriginal));

    if (!iconRecord)
        return { nullptr, IsKnownIcon::No };

    // If it's a new IconRecord object that doesn't have its imageData set yet,
    // mark it to be read by the background thread
    if (iconRecord->imageDataStatus() == IconRecord::ImageDataStatus::Unknown) {
        if (pageURLCopy.isNull())
            pageURLCopy = pageURLOriginal.isolatedCopy();

        LockHolder locker(m_pendingReadingLock);
        m_pageURLsInterestedInIcons.add(pageURLCopy);
        m_iconsPendingReading.add(iconRecord);
        wakeSyncThread();
        return { nullptr, IsKnownIcon::No };
    }

    // If the size parameter was (0, 0) that means the caller of this method just wanted the read from disk to be kicked off
    // and isn't actually interested in the image return value
    if (size == IntSize(0, 0))
        return { nullptr, IsKnownIcon::Yes };

    return { iconRecord->image(size), IsKnownIcon::Yes };
}

String IconDatabase::synchronousIconURLForPageURL(const String& pageURLOriginal)
{
    ASSERT_NOT_SYNC_THREAD();

    // Cannot do anything with pageURLOriginal that would end up storing it without deep copying first
    // Also, in the case we have a real answer for the caller, we must deep copy that as well

    if (!isOpen() || !documentCanHaveIcon(pageURLOriginal))
        return String();

    LockHolder locker(m_urlAndIconLock);

    PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURLOriginal);
    if (!pageRecord)
        pageRecord = getOrCreatePageURLRecord(pageURLOriginal.isolatedCopy());

    // If pageRecord is nullptr, one of two things is true -
    // 1 - The initial url import is incomplete and this pageURL has already been marked to be notified once it is complete if an iconURL exists
    // 2 - The initial url import IS complete and this pageURL has no icon
    if (!pageRecord)
        return String();

    // Possible the pageRecord is around because it's a retained pageURL with no iconURL, so we have to check
    return pageRecord->iconRecord() ? pageRecord->iconRecord()->iconURL().isolatedCopy() : String();
}

void IconDatabase::retainIconForPageURL(const String& pageURL)
{
    ASSERT_NOT_SYNC_THREAD();

    if (!isEnabled() || !documentCanHaveIcon(pageURL))
        return;

    {
        LockHolder locker(m_urlsToRetainOrReleaseLock);
        m_urlsToRetain.add(pageURL.isolatedCopy());
        m_retainOrReleaseIconRequested = true;
    }

    scheduleOrDeferSyncTimer();
}

void IconDatabase::performRetainIconForPageURL(const String& pageURLOriginal, int retainCount)
{
    PageURLRecord* record = m_pageURLToRecordMap.get(pageURLOriginal);

    String pageURL;

    if (!record) {
        pageURL = pageURLOriginal.isolatedCopy();

        record = new PageURLRecord(pageURL);
        m_pageURLToRecordMap.set(pageURL, record);
    }

    if (!record->retain(retainCount)) {
        if (pageURL.isNull())
            pageURL = pageURLOriginal.isolatedCopy();

        // This page just had its retain count bumped from 0 to 1 - Record that fact
        m_retainedPageURLs.add(pageURL);

        // If we read the iconURLs yet, we want to avoid any pageURL->iconURL lookups and the pageURLsPendingDeletion is moot,
        // so we bail here and skip those steps
        if (!m_iconURLImportComplete)
            return;

        LockHolder locker(m_pendingSyncLock);
        // If this pageURL waiting to be sync'ed, update the sync record
        // This saves us in the case where a page was ready to be deleted from the database but was just retained - so theres no need to delete it!
        if (!m_privateBrowsingEnabled && m_pageURLsPendingSync.contains(pageURL)) {
            LOG(IconDatabase, "Bringing %s back from the brink", pageURL.ascii().data());
            m_pageURLsPendingSync.set(pageURL, record->snapshot());
        }
    }
}

void IconDatabase::releaseIconForPageURL(const String& pageURL)
{
    ASSERT_NOT_SYNC_THREAD();

    // Cannot do anything with pageURLOriginal that would end up storing it without deep copying first

    if (!isEnabled() || !documentCanHaveIcon(pageURL))
        return;

    {
        LockHolder locker(m_urlsToRetainOrReleaseLock);
        m_urlsToRelease.add(pageURL.isolatedCopy());
        m_retainOrReleaseIconRequested = true;
    }
    scheduleOrDeferSyncTimer();
}

void IconDatabase::performReleaseIconForPageURL(const String& pageURLOriginal, int releaseCount)
{
    // Check if this pageURL is actually retained
    if (!m_retainedPageURLs.contains(pageURLOriginal)) {
        LOG_ERROR("Attempting to release icon for URL %s which is not retained", urlForLogging(pageURLOriginal).ascii().data());
        return;
    }

    // Get its retain count - if it's retained, we'd better have a PageURLRecord for it
    PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURLOriginal);
    ASSERT(pageRecord);
    LOG(IconDatabase, "Releasing pageURL %s to a retain count of %i", urlForLogging(pageURLOriginal).ascii().data(), pageRecord->retainCount() - 1);
    ASSERT(pageRecord->retainCount() > 0);

    // If it still has a positive retain count, store the new count and bail
    if (pageRecord->release(releaseCount))
        return;

    // This pageRecord has now been fully released. Do the appropriate cleanup
    LOG(IconDatabase, "No more retainers for PageURL %s", urlForLogging(pageURLOriginal).ascii().data());
    m_pageURLToRecordMap.remove(pageURLOriginal);
    m_retainedPageURLs.remove(pageURLOriginal);

    // Grab the iconRecord for later use (and do a sanity check on it for kicks)
    IconRecord* iconRecord = pageRecord->iconRecord();

    ASSERT(!iconRecord || (iconRecord && m_iconURLToRecordMap.get(iconRecord->iconURL()) == iconRecord));

    {
        LockHolder locker(m_pendingReadingLock);

        // Since this pageURL is going away, there's no reason anyone would ever be interested in its read results
        if (!m_iconURLImportComplete)
            m_pageURLsPendingImport.remove(pageURLOriginal);
        m_pageURLsInterestedInIcons.remove(pageURLOriginal);

        // If this icon is down to it's last retainer, we don't care about reading it in from disk anymore
        if (iconRecord && iconRecord->hasOneRef()) {
            m_iconURLToRecordMap.remove(iconRecord->iconURL());
            m_iconsPendingReading.remove(iconRecord);
        }
    }

    // Mark stuff for deletion from the database only if we're not in private browsing
    if (!m_privateBrowsingEnabled) {
        LockHolder locker(m_pendingSyncLock);
        m_pageURLsPendingSync.set(pageURLOriginal.isolatedCopy(), pageRecord->snapshot(true));

        // If this page is the last page to refer to a particular IconRecord, that IconRecord needs to
        // be marked for deletion
        if (iconRecord && iconRecord->hasOneRef())
            m_iconsPendingSync.set(iconRecord->iconURL(), iconRecord->snapshot(true));
    }

    delete pageRecord;
}

void IconDatabase::setIconDataForIconURL(RefPtr<SharedBuffer>&& data, const String& iconURLOriginal)
{
    ASSERT_NOT_SYNC_THREAD();

    // Cannot do anything with iconURLOriginal that would end up storing them without deep copying first

    if (!isOpen() || iconURLOriginal.isEmpty())
        return;

    String iconURL = iconURLOriginal.isolatedCopy();
    Vector<String> pageURLs;
    {
        LockHolder locker(m_urlAndIconLock);

        // If this icon was pending a read, remove it from that set because this new data should override what is on disk
        RefPtr<IconRecord> icon = m_iconURLToRecordMap.get(iconURL);
        if (icon) {
            LockHolder locker(m_pendingReadingLock);
            m_iconsPendingReading.remove(icon.get());
        } else
            icon = getOrCreateIconRecord(iconURL);

        // Update the data and set the time stamp
        icon->setImageData(WTFMove(data));
        icon->setTimestamp((int)WallTime::now().secondsSinceEpoch().seconds());

        // Copy the current retaining pageURLs - if any - to notify them of the change
        pageURLs.appendRange(icon->retainingPageURLs().begin(), icon->retainingPageURLs().end());

        // Mark the IconRecord as requiring an update to the database only if private browsing is disabled
        if (!m_privateBrowsingEnabled) {
            LockHolder locker(m_pendingSyncLock);
            m_iconsPendingSync.set(iconURL, icon->snapshot());
        }

        if (icon->hasOneRef()) {
            ASSERT(icon->retainingPageURLs().isEmpty());
            LOG(IconDatabase, "Icon for icon url %s is about to be destroyed - removing mapping for it", urlForLogging(icon->iconURL()).ascii().data());
            m_iconURLToRecordMap.remove(icon->iconURL());
        }
    }

    // Send notification out regarding all PageURLs that retain this icon
    // Start the timer to commit this change - or further delay the timer if it was already started
    scheduleOrDeferSyncTimer();

    for (auto& pageURL : pageURLs) {
        LOG(IconDatabase, "Dispatching notification that retaining pageURL %s has a new icon", urlForLogging(pageURL).ascii().data());
        if (m_client)
            m_client->didChangeIconForPageURL(pageURL);
    }
}

void IconDatabase::setIconURLForPageURL(const String& iconURLOriginal, const String& pageURLOriginal)
{
    ASSERT_NOT_SYNC_THREAD();

    // Cannot do anything with iconURLOriginal or pageURLOriginal that would end up storing them without deep copying first

    ASSERT(!iconURLOriginal.isEmpty());

    if (!isOpen() || !documentCanHaveIcon(pageURLOriginal))
        return;

    String iconURL, pageURL;
    {
        LockHolder locker(m_urlAndIconLock);

        PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURLOriginal);

        // If the urls already map to each other, bail.
        // This happens surprisingly often, and seems to cream iBench performance
        if (pageRecord && pageRecord->iconRecord() && pageRecord->iconRecord()->iconURL() == iconURLOriginal)
            return;

        pageURL = pageURLOriginal.isolatedCopy();
        iconURL = iconURLOriginal.isolatedCopy();

        if (!pageRecord) {
            pageRecord = new PageURLRecord(pageURL);
            m_pageURLToRecordMap.set(pageURL, pageRecord);
        }

        RefPtr<IconRecord> iconRecord = pageRecord->iconRecord();

        // Otherwise, set the new icon record for this page
        pageRecord->setIconRecord(getOrCreateIconRecord(iconURL));

        // If the current icon has only a single ref left, it is about to get wiped out.
        // Remove it from the in-memory records and don't bother reading it in from disk anymore
        if (iconRecord && iconRecord->hasOneRef()) {
            ASSERT(!iconRecord->retainingPageURLs().size());
            LOG(IconDatabase, "Icon for icon url %s is about to be destroyed - removing mapping for it", urlForLogging(iconRecord->iconURL()).ascii().data());
            m_iconURLToRecordMap.remove(iconRecord->iconURL());
            LockHolder locker(m_pendingReadingLock);
            m_iconsPendingReading.remove(iconRecord.get());
        }

        // And mark this mapping to be added to the database
        if (!m_privateBrowsingEnabled) {
            LockHolder locker(m_pendingSyncLock);
            m_pageURLsPendingSync.set(pageURL, pageRecord->snapshot());

            // If the icon is on its last ref, mark it for deletion
            if (iconRecord && iconRecord->hasOneRef())
                m_iconsPendingSync.set(iconRecord->iconURL(), iconRecord->snapshot(true));
        }
    }

    // Since this mapping is new, send the notification out - but not if we're on the sync thread because that implies this mapping
    // comes from the initial import which we don't want notifications for
    if (!IS_ICON_SYNC_THREAD()) {
        // Start the timer to commit this change - or further delay the timer if it was already started
        scheduleOrDeferSyncTimer();

        LOG(IconDatabase, "Dispatching notification that we changed an icon mapping for url %s", urlForLogging(pageURL).ascii().data());
        if (m_client)
            m_client->didChangeIconForPageURL(pageURL);
    }
}

IconDatabase::IconLoadDecision IconDatabase::synchronousLoadDecisionForIconURL(const String& iconURL)
{
    ASSERT_NOT_SYNC_THREAD();

    if (!isOpen() || iconURL.isEmpty())
        return IconLoadDecision::No;

    // If we have a IconRecord, it should also have its timeStamp marked because there is only two times when we create the IconRecord:
    // 1 - When we read the icon urls from disk, getting the timeStamp at the same time
    // 2 - When we get a new icon from the loader, in which case the timestamp is set at that time
    {
        LockHolder locker(m_urlAndIconLock);
        if (IconRecord* icon = m_iconURLToRecordMap.get(iconURL)) {
            LOG(IconDatabase, "Found expiration time on a present icon based on existing IconRecord");
            return static_cast<int>(WallTime::now().secondsSinceEpoch().seconds()) - static_cast<int>(icon->getTimestamp()) > iconExpirationTime ? IconLoadDecision::Yes : IconLoadDecision::No;
        }
    }

    // If we don't have a record for it, but we *have* imported all iconURLs from disk, then we should load it now
    LockHolder readingLocker(m_pendingReadingLock);
    if (m_iconURLImportComplete)
        return IconLoadDecision::Yes;

    // Otherwise - since we refuse to perform I/O on the main thread to find out for sure - we return the answer that says
    // "You might be asked to load this later, so flag that"
    return IconLoadDecision::Unknown;
}

bool IconDatabase::synchronousIconDataKnownForIconURL(const String& iconURL)
{
    ASSERT_NOT_SYNC_THREAD();

    LockHolder locker(m_urlAndIconLock);
    if (IconRecord* icon = m_iconURLToRecordMap.get(iconURL))
        return icon->imageDataStatus() != IconRecord::ImageDataStatus::Unknown;

    return false;
}

void IconDatabase::setEnabled(bool enabled)
{
    ASSERT_NOT_SYNC_THREAD();

    if (!enabled && isOpen())
        close();
    m_isEnabled = enabled;
}

bool IconDatabase::isEnabled() const
{
    ASSERT_NOT_SYNC_THREAD();

    return m_isEnabled;
}

void IconDatabase::setPrivateBrowsingEnabled(bool flag)
{
    m_privateBrowsingEnabled = flag;
}

bool IconDatabase::isPrivateBrowsingEnabled() const
{
    return m_privateBrowsingEnabled;
}

void IconDatabase::delayDatabaseCleanup()
{
    ++databaseCleanupCounter;
    if (databaseCleanupCounter == 1)
        LOG(IconDatabase, "Database cleanup is now DISABLED");
}

void IconDatabase::allowDatabaseCleanup()
{
    if (--databaseCleanupCounter < 0)
        databaseCleanupCounter = 0;
    if (!databaseCleanupCounter)
        LOG(IconDatabase, "Database cleanup is now ENABLED");
}

void IconDatabase::checkIntegrityBeforeOpening()
{
    checkIntegrityOnOpen = true;
}

IconDatabase::IconDatabase()
    : m_syncTimer(RunLoop::main(), this, &IconDatabase::syncTimerFired)
    , m_client(defaultClient())
{
    LOG(IconDatabase, "Creating IconDatabase %p", this);
    ASSERT(isMainThread());
}

IconDatabase::~IconDatabase()
{
    ASSERT(!isOpen());
}

void IconDatabase::wakeSyncThread()
{
    LockHolder locker(m_syncLock);

    m_syncThreadHasWorkToDo = true;
    m_syncCondition.notifyOne();
}

void IconDatabase::scheduleOrDeferSyncTimer()
{
    ASSERT_NOT_SYNC_THREAD();

    if (m_scheduleOrDeferSyncTimerRequested)
        return;

    m_scheduleOrDeferSyncTimerRequested = true;
    callOnMainThread([this] {
        m_syncTimer.startOneShot(updateTimerDelay);
        m_scheduleOrDeferSyncTimerRequested = false;
    });
}

void IconDatabase::syncTimerFired()
{
    ASSERT_NOT_SYNC_THREAD();
    wakeSyncThread();
}

// ******************
// *** Any Thread ***
// ******************

bool IconDatabase::isOpen() const
{
    LockHolder locker(m_syncLock);
    return m_syncThreadRunning || m_syncDB.isOpen();
}

String IconDatabase::databasePath() const
{
    LockHolder locker(m_syncLock);
    return m_completeDatabasePath.isolatedCopy();
}

String IconDatabase::defaultDatabaseFilename()
{
    static NeverDestroyed<String> defaultDatabaseFilename(MAKE_STATIC_STRING_IMPL("WebpageIcons.db"));
    return defaultDatabaseFilename.get().isolatedCopy();
}

// Unlike getOrCreatePageURLRecord(), getOrCreateIconRecord() does not mark the icon as "interested in import"
Ref<IconDatabase::IconRecord> IconDatabase::getOrCreateIconRecord(const String& iconURL)
{
    // Clients of getOrCreateIconRecord() are required to acquire the m_urlAndIconLock before calling this method
    ASSERT(!m_urlAndIconLock.tryLock());

    if (auto* icon = m_iconURLToRecordMap.get(iconURL))
        return *icon;

    auto newIcon = IconRecord::create(iconURL);
    m_iconURLToRecordMap.set(iconURL, newIcon.ptr());
    return newIcon;
}

// This method retrieves the existing PageURLRecord, or creates a new one and marks it as "interested in the import" for later notification
IconDatabase::PageURLRecord* IconDatabase::getOrCreatePageURLRecord(const String& pageURL)
{
    // Clients of getOrCreatePageURLRecord() are required to acquire the m_urlAndIconLock before calling this method
    ASSERT(!m_urlAndIconLock.tryLock());

    if (!documentCanHaveIcon(pageURL))
        return nullptr;

    PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURL);

    LockHolder locker(m_pendingReadingLock);
    if (!m_iconURLImportComplete) {
        // If the initial import of all URLs hasn't completed and we have no page record, we assume we *might* know about this later and create a record for it
        if (!pageRecord) {
            LOG(IconDatabase, "Creating new PageURLRecord for pageURL %s", urlForLogging(pageURL).ascii().data());
            pageRecord = new PageURLRecord(pageURL);
            m_pageURLToRecordMap.set(pageURL, pageRecord);
        }

        // If the pageRecord for this page does not have an iconRecord attached to it, then it is a new pageRecord still awaiting the initial import
        // Mark the URL as "interested in the result of the import" then bail
        if (!pageRecord->iconRecord()) {
            m_pageURLsPendingImport.add(pageURL);
            return nullptr;
        }
    }

    // We've done the initial import of all URLs known in the database. If this record doesn't exist now, it never will
    return pageRecord;
}


// ************************
// *** Sync Thread Only ***
// ************************

bool IconDatabase::shouldStopThreadActivity() const
{
    ASSERT_ICON_SYNC_THREAD();

    return m_threadTerminationRequested || m_removeIconsRequested;
}

void IconDatabase::iconDatabaseSyncThread()
{
    // The call to create this thread might not complete before the thread actually starts, so we might fail this ASSERT_ICON_SYNC_THREAD() because the pointer
    // to our thread structure hasn't been filled in yet.
    // To fix this, the main thread acquires this lock before creating us, then releases the lock after creation is complete. A quick lock/unlock cycle here will
    // prevent us from running before that call completes
    m_syncLock.lock();
    m_syncLock.unlock();

    ASSERT_ICON_SYNC_THREAD();

    LOG(IconDatabase, "(THREAD) IconDatabase sync thread started");

#if !LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif

    // Need to create the database path if it doesn't already exist
    FileSystem::makeAllDirectories(m_databaseDirectory);

    // Existence of a journal file is evidence of a previous crash/force quit and automatically qualifies
    // us to do an integrity check
    String journalFilename = m_completeDatabasePath + "-journal";
    if (!checkIntegrityOnOpen)
        checkIntegrityOnOpen = FileSystem::fileExists(journalFilename);

    {
        LockHolder locker(m_syncLock);
        if (!m_syncDB.open(m_completeDatabasePath)) {
            LOG_ERROR("Unable to open icon database at path %s - %s", m_completeDatabasePath.ascii().data(), m_syncDB.lastErrorMsg());
            return;
        }
    }

    if (shouldStopThreadActivity()) {
        syncThreadMainLoop();
        return;
    }

#if !LOG_DISABLED
    MonotonicTime timeStamp = MonotonicTime::now();
    Seconds delta = timeStamp - startTime;
    LOG(IconDatabase, "(THREAD) Open took %.4f seconds", delta.value());
#endif

    performOpenInitialization();
    if (shouldStopThreadActivity()) {
        syncThreadMainLoop();
        return;
    }

#if !LOG_DISABLED
    MonotonicTime newStamp = MonotonicTime::now();
    Seconds totalDelta = newStamp - startTime;
    delta = newStamp - timeStamp;
    LOG(IconDatabase, "(THREAD) performOpenInitialization() took %.4f seconds, now %.4f seconds from thread start", delta.value(), totalDelta.value());
    timeStamp = newStamp;
#endif

    // Uncomment the following line to simulate a long lasting URL import (*HUGE* icon databases, or network home directories)
    // while (MonotonicTime::now() - timeStamp < 10_s);

    // Read in URL mappings from the database
    LOG(IconDatabase, "(THREAD) Starting iconURL import");
    performURLImport();

    if (shouldStopThreadActivity()) {
        syncThreadMainLoop();
        return;
    }

#if !LOG_DISABLED
    newStamp = MonotonicTime::now();
    totalDelta = newStamp - startTime;
    delta = newStamp - timeStamp;
    LOG(IconDatabase, "(THREAD) performURLImport() took %.4f seconds. Entering main loop %.4f seconds from thread start", delta.value(), totalDelta.value());
#endif

    LOG(IconDatabase, "(THREAD) Beginning sync");
    syncThreadMainLoop();
}

static int databaseVersionNumber(SQLiteDatabase& db)
{
    return SQLiteStatement(db, "SELECT value FROM IconDatabaseInfo WHERE key = 'Version';").getColumnInt(0);
}

static bool isValidDatabase(SQLiteDatabase& db)
{
    // These four tables should always exist in a valid db
    if (!db.tableExists("IconInfo") || !db.tableExists("IconData") || !db.tableExists("PageURL") || !db.tableExists("IconDatabaseInfo"))
        return false;

    if (databaseVersionNumber(db) < currentDatabaseVersion) {
        LOG(IconDatabase, "DB version is not found or below expected valid version");
        return false;
    }

    return true;
}

static void createDatabaseTables(SQLiteDatabase& db)
{
    if (!db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE INDEX PageURLIndex ON PageURL (url);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE IconInfo (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, stamp INTEGER);")) {
        LOG_ERROR("Could not create IconInfo table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE INDEX IconInfoIndex ON IconInfo (url, iconID);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE IconData (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, data BLOB);")) {
        LOG_ERROR("Could not create IconData table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE INDEX IconDataIndex ON IconData (iconID);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in database (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
    if (!db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", db.lastError(), db.lastErrorMsg());
        db.close();
        return;
    }
}

void IconDatabase::performOpenInitialization()
{
    ASSERT_ICON_SYNC_THREAD();

    if (!isOpen())
        return;

    if (checkIntegrityOnOpen) {
        checkIntegrityOnOpen = false;
        if (!checkIntegrity()) {
            LOG(IconDatabase, "Integrity check was bad - dumping IconDatabase");

            m_syncDB.close();

            {
                LockHolder locker(m_syncLock);
                // Should've been consumed by SQLite, delete just to make sure we don't see it again in the future;
                FileSystem::deleteFile(m_completeDatabasePath + "-journal");
                FileSystem::deleteFile(m_completeDatabasePath);
            }

            // Reopen the write database, creating it from scratch
            if (!m_syncDB.open(m_completeDatabasePath)) {
                LOG_ERROR("Unable to open icon database at path %s - %s", m_completeDatabasePath.ascii().data(), m_syncDB.lastErrorMsg());
                return;
            }
        }
    }

    int version = databaseVersionNumber(m_syncDB);

    if (version > currentDatabaseVersion) {
        LOG(IconDatabase, "Database version number %i is greater than our current version number %i - closing the database to prevent overwriting newer versions", version, currentDatabaseVersion);
        m_syncDB.close();
        m_threadTerminationRequested = true;
        return;
    }

    if (!isValidDatabase(m_syncDB)) {
        LOG(IconDatabase, "%s is missing or in an invalid state - reconstructing", m_completeDatabasePath.ascii().data());
        m_syncDB.clearAllTables();
        createDatabaseTables(m_syncDB);
    }

    // Reduce sqlite RAM cache size from default 2000 pages (~1.5kB per page). 3MB of cache for icon database is overkill
    if (!SQLiteStatement(m_syncDB, "PRAGMA cache_size = 200;").executeCommand())
        LOG_ERROR("SQLite database could not set cache_size");
}

bool IconDatabase::checkIntegrity()
{
    ASSERT_ICON_SYNC_THREAD();

    SQLiteStatement integrity(m_syncDB, "PRAGMA integrity_check;");
    if (integrity.prepare() != SQLITE_OK) {
        LOG_ERROR("checkIntegrity failed to execute");
        return false;
    }

    int resultCode = integrity.step();
    if (resultCode == SQLITE_OK)
        return true;

    if (resultCode != SQLITE_ROW)
        return false;

    int columns = integrity.columnCount();
    if (columns != 1) {
        LOG_ERROR("Received %i columns performing integrity check, should be 1", columns);
        return false;
    }

    String resultText = integrity.getColumnText(0);

    // A successful, no-error integrity check will be "ok" - all other strings imply failure
    if (resultText == "ok")
        return true;

    LOG_ERROR("Icon database integrity check failed - \n%s", resultText.ascii().data());
    return false;
}

void IconDatabase::performURLImport()
{
    ASSERT_ICON_SYNC_THREAD();

    // Do not import icons not used in the last 30 days. They will be automatically pruned later if nobody retains them.
    // Note that IconInfo.stamp is only set when the icon data is retrieved from the server (and thus is not updated whether
    // we use it or not). This code works anyway because the IconDatabase downloads icons again if they are older than 4 days,
    // so if the timestamp goes back in time more than those 30 days we can be sure that the icon was not used at all.
    String importQuery = String::format("SELECT PageURL.url, IconInfo.url, IconInfo.stamp FROM PageURL INNER JOIN IconInfo ON PageURL.iconID=IconInfo.iconID WHERE IconInfo.stamp > %.0f;", floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds()));

    SQLiteStatement query(m_syncDB, importQuery);

    if (query.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to prepare icon url import query");
        return;
    }

    int result = query.step();
    while (result == SQLITE_ROW) {
        String pageURL = query.getColumnText(0);
        String iconURL = query.getColumnText(1);

        {
            LockHolder locker(m_urlAndIconLock);

            PageURLRecord* pageRecord = m_pageURLToRecordMap.get(pageURL);

            // If the pageRecord doesn't exist in this map, then no one has retained this pageURL
            // If the s_databaseCleanupCounter count is non-zero, then we're not supposed to be pruning the database in any manner,
            // so actually create a pageURLRecord for this url even though it's not retained.
            // If database cleanup *is* allowed, we don't want to bother pulling in a page url from disk that noone is actually interested
            // in - we'll prune it later instead!
            if (!pageRecord && databaseCleanupCounter && documentCanHaveIcon(pageURL)) {
                pageRecord = new PageURLRecord(pageURL);
                m_pageURLToRecordMap.set(pageURL, pageRecord);
            }

            if (pageRecord) {
                IconRecord* currentIcon = pageRecord->iconRecord();

                if (!currentIcon || currentIcon->iconURL() != iconURL) {
                    pageRecord->setIconRecord(getOrCreateIconRecord(iconURL));
                    currentIcon = pageRecord->iconRecord();
                }

                // Regardless, the time stamp from disk still takes precedence. Until we read this icon from disk, we didn't think we'd seen it before
                // so we marked the timestamp as "now", but it's really much older
                currentIcon->setTimestamp(query.getColumnInt(2));
            }
        }

        // FIXME: Currently the WebKit API supports 1 type of notification that is sent whenever we get an Icon URL for a Page URL. We might want to re-purpose it to work for
        // getting the actually icon itself also (so each pageurl would get this notification twice) or we might want to add a second type of notification -
        // one for the URL and one for the Image itself
        // Note that WebIconDatabase is not neccessarily API so we might be able to make this change
        {
            LockHolder locker(m_pendingReadingLock);
            if (m_pageURLsPendingImport.contains(pageURL)) {
                dispatchDidImportIconURLForPageURLOnMainThread(pageURL);
                m_pageURLsPendingImport.remove(pageURL);
            }
        }

        // Stop the import at any time of the thread has been asked to shutdown
        if (shouldStopThreadActivity()) {
            LOG(IconDatabase, "IconDatabase asked to terminate during performURLImport()");
            return;
        }

        result = query.step();
    }

    if (result != SQLITE_DONE)
        LOG(IconDatabase, "Error reading page->icon url mappings from database");

    // Clear the m_pageURLsPendingImport set - either the page URLs ended up with an iconURL (that we'll notify about) or not,
    // but after m_iconURLImportComplete is set to true, we don't care about this set anymore
    Vector<String> urls;
    {
        LockHolder locker(m_pendingReadingLock);

        urls.appendRange(m_pageURLsPendingImport.begin(), m_pageURLsPendingImport.end());
        m_pageURLsPendingImport.clear();
        m_iconURLImportComplete = true;
    }

    Vector<String> urlsToNotify;

    // Loop through the urls pending import
    // Remove unretained ones if database cleanup is allowed
    // Keep a set of ones that are retained and pending notification
    {
        LockHolder locker(m_urlAndIconLock);

        performPendingRetainAndReleaseOperations();

        for (auto& url : urls) {
            if (!m_retainedPageURLs.contains(url)) {
                PageURLRecord* record = m_pageURLToRecordMap.get(url);
                if (record && !databaseCleanupCounter) {
                    m_pageURLToRecordMap.remove(url);
                    IconRecord* iconRecord = record->iconRecord();

                    // If this page is the only remaining retainer of its icon, mark that icon for deletion and don't bother
                    // reading anything related to it
                    if (iconRecord && iconRecord->hasOneRef()) {
                        m_iconURLToRecordMap.remove(iconRecord->iconURL());

                        {
                            LockHolder locker(m_pendingReadingLock);
                            m_pageURLsInterestedInIcons.remove(url);
                            m_iconsPendingReading.remove(iconRecord);
                        }
                        {
                            LockHolder locker(m_pendingSyncLock);
                            m_iconsPendingSync.set(iconRecord->iconURL(), iconRecord->snapshot(true));
                        }
                    }

                    delete record;
                }
            } else
                urlsToNotify.append(url);
        }
    }

    LOG(IconDatabase, "Notifying %lu interested page URLs that their icon URL is known due to the import", static_cast<unsigned long>(urlsToNotify.size()));
    // Now that we don't hold any locks, perform the actual notifications
    for (auto& url : urlsToNotify) {
        LOG(IconDatabase, "Notifying icon info known for pageURL %s", url.ascii().data());
        dispatchDidImportIconURLForPageURLOnMainThread(url);
        if (shouldStopThreadActivity())
            return;
    }

    // Notify the client that the URL import is complete in case it's managing its own pending notifications.
    dispatchDidFinishURLImportOnMainThread();
}

void IconDatabase::syncThreadMainLoop()
{
    ASSERT_ICON_SYNC_THREAD();

    m_syncLock.lock();

    // We'll either do any pending work on our first pass through the loop, or we'll terminate
    // without doing any work. Either way we're dealing with any currently-pending work.
    m_syncThreadHasWorkToDo = false;

    // It's possible thread termination is requested before the main loop even starts - in that case, just skip straight to cleanup
    while (!m_threadTerminationRequested) {
        m_syncLock.unlock();

#if !LOG_DISABLED
        MonotonicTime timeStamp = MonotonicTime::now();
#endif
        LOG(IconDatabase, "(THREAD) Main work loop starting");

        // If we should remove all icons, do it now. This is an uninteruptible procedure that we will always do before quitting if it is requested
        if (m_removeIconsRequested) {
            removeAllIconsOnThread();
            m_removeIconsRequested = false;
        }

        // Then, if the thread should be quitting, quit now!
        if (m_threadTerminationRequested) {
            cleanupSyncThread();
            return;
        }

        {
            LockHolder locker(m_urlAndIconLock);
            performPendingRetainAndReleaseOperations();
        }

        bool didAnyWork = true;
        while (didAnyWork) {
            bool didWrite = writeToDatabase();
            if (shouldStopThreadActivity())
                break;

            didAnyWork = readFromDatabase();
            if (shouldStopThreadActivity())
                break;

            // Prune unretained icons after the first time we sync anything out to the database
            // This way, pruning won't be the only operation we perform to the database by itself
            // We also don't want to bother doing this if the thread should be terminating (the user is quitting)
            // or if private browsing is enabled
            // We also don't want to prune if the m_databaseCleanupCounter count is non-zero - that means someone
            // has asked to delay pruning
            static bool prunedUnretainedIcons = false;
            if (didWrite && !m_privateBrowsingEnabled && !prunedUnretainedIcons && !databaseCleanupCounter) {
#if !LOG_DISABLED
                MonotonicTime time = MonotonicTime::now();
#endif
                LOG(IconDatabase, "(THREAD) Starting pruneUnretainedIcons()");

                pruneUnretainedIcons();

#if !LOG_DISABLED
                Seconds delta = MonotonicTime::now() - time;
#endif
                LOG(IconDatabase, "(THREAD) pruneUnretainedIcons() took %.4f seconds", delta.value());

                // If pruneUnretainedIcons() returned early due to requested thread termination, its still okay
                // to mark prunedUnretainedIcons true because we're about to terminate anyway
                prunedUnretainedIcons = true;
            }

            didAnyWork = didAnyWork || didWrite;
            if (shouldStopThreadActivity())
                break;
        }

#if !LOG_DISABLED
        MonotonicTime newstamp = MonotonicTime::now();
        Seconds delta = newstamp - timeStamp;
        LOG(IconDatabase, "(THREAD) Main work loop ran for %.4f seconds, %s requested to terminate", delta.value(), shouldStopThreadActivity() ? "was" : "was not");
#endif

        m_syncLock.lock();

        // There is some condition that is asking us to stop what we're doing now and handle a special case
        // This is either removing all icons, or shutting down the thread to quit the app
        // We handle those at the top of this main loop so continue to jump back up there
        if (shouldStopThreadActivity())
            continue;

        while (!m_syncThreadHasWorkToDo)
            m_syncCondition.wait(m_syncLock);

        m_syncThreadHasWorkToDo = false;
    }

    m_syncLock.unlock();

    // Thread is terminating at this point
    cleanupSyncThread();
}

void IconDatabase::performPendingRetainAndReleaseOperations()
{
    // NOTE: The caller is assumed to hold m_urlAndIconLock.
    ASSERT(!m_urlAndIconLock.tryLock());

    HashCountedSet<String> toRetain;
    HashCountedSet<String> toRelease;

    {
        LockHolder pendingWorkLocker(m_urlsToRetainOrReleaseLock);
        if (!m_retainOrReleaseIconRequested)
            return;

        // Make a copy of the URLs to retain and/or release so we can release m_urlsToRetainOrReleaseLock ASAP.
        // Holding m_urlAndIconLock protects this function from being re-entered.

        toRetain.swap(m_urlsToRetain);
        toRelease.swap(m_urlsToRelease);
        m_retainOrReleaseIconRequested = false;
    }

    for (auto& entry : toRetain) {
        ASSERT(!entry.key.impl() || entry.key.impl()->hasOneRef());
        performRetainIconForPageURL(entry.key, entry.value);
    }

    for (auto& entry : toRelease) {
        ASSERT(!entry.key.impl() || entry.key.impl()->hasOneRef());
        performReleaseIconForPageURL(entry.key, entry.value);
    }
}

bool IconDatabase::readFromDatabase()
{
    ASSERT_ICON_SYNC_THREAD();

#if !LOG_DISABLED
    MonotonicTime timeStamp = MonotonicTime::now();
#endif

    bool didAnyWork = false;

    // We'll make a copy of the sets of things that need to be read. Then we'll verify at the time of updating the record that it still wants to be updated
    // This way we won't hold the lock for a long period of time
    Vector<IconRecord*> icons;
    {
        LockHolder locker(m_pendingReadingLock);
        icons.appendRange(m_iconsPendingReading.begin(), m_iconsPendingReading.end());
    }

    // Keep track of icons we actually read to notify them of the new icon
    HashSet<String> urlsToNotify;

    for (unsigned i = 0; i < icons.size(); ++i) {
        didAnyWork = true;
        auto imageData = getImageDataForIconURLFromSQLDatabase(icons[i]->iconURL());

        // Verify this icon still wants to be read from disk
        {
            LockHolder urlLocker(m_urlAndIconLock);
            {
                LockHolder readLocker(m_pendingReadingLock);

                if (m_iconsPendingReading.contains(icons[i])) {
                    // Set the new data
                    icons[i]->setImageData(WTFMove(imageData));

                    // Remove this icon from the set that needs to be read
                    m_iconsPendingReading.remove(icons[i]);

                    // We have a set of all Page URLs that retain this icon as well as all PageURLs waiting for an icon
                    // We want to find the intersection of these two sets to notify them
                    // Check the sizes of these two sets to minimize the number of iterations
                    const HashSet<String>* outerHash;
                    const HashSet<String>* innerHash;

                    if (icons[i]->retainingPageURLs().size() > m_pageURLsInterestedInIcons.size()) {
                        outerHash = &m_pageURLsInterestedInIcons;
                        innerHash = &(icons[i]->retainingPageURLs());
                    } else {
                        innerHash = &m_pageURLsInterestedInIcons;
                        outerHash = &(icons[i]->retainingPageURLs());
                    }

                    for (auto& outer : *outerHash) {
                        if (innerHash->contains(outer)) {
                            LOG(IconDatabase, "%s is interested in the icon we just read. Adding it to the notification list and removing it from the interested set", urlForLogging(outer).ascii().data());
                            urlsToNotify.add(outer);
                        }

                        // If we ever get to the point were we've seen every url interested in this icon, break early
                        if (urlsToNotify.size() == m_pageURLsInterestedInIcons.size())
                            break;
                    }

                    // We don't need to notify a PageURL twice, so all the ones we're about to notify can be removed from the interested set
                    if (urlsToNotify.size() == m_pageURLsInterestedInIcons.size())
                        m_pageURLsInterestedInIcons.clear();
                    else {
                        for (auto& url : urlsToNotify)
                            m_pageURLsInterestedInIcons.remove(url);
                    }
                }
            }
        }

        if (shouldStopThreadActivity())
            return didAnyWork;

        // Now that we don't hold any locks, perform the actual notifications
        for (HashSet<String>::const_iterator it = urlsToNotify.begin(), end = urlsToNotify.end(); it != end; ++it) {
            LOG(IconDatabase, "Notifying icon received for pageURL %s", urlForLogging(*it).ascii().data());
            dispatchDidImportIconDataForPageURLOnMainThread(*it);
            if (shouldStopThreadActivity())
                return didAnyWork;
        }

        LOG(IconDatabase, "Done notifying %i pageURLs who just received their icons", urlsToNotify.size());
        urlsToNotify.clear();

        if (shouldStopThreadActivity())
            return didAnyWork;
    }

#if !LOG_DISABLED
    Seconds delta = MonotonicTime::now() - timeStamp;
    LOG(IconDatabase, "Reading from database took %.4f seconds", delta.value());
#endif

    return didAnyWork;
}

bool IconDatabase::writeToDatabase()
{
    ASSERT_ICON_SYNC_THREAD();

#if !LOG_DISABLED
    MonotonicTime timeStamp = MonotonicTime::now();
#endif

    bool didAnyWork = false;

    // We can copy the current work queue then clear it out - If any new work comes in while we're writing out,
    // we'll pick it up on the next pass. This greatly simplifies the locking strategy for this method and remains cohesive with changes
    // asked for by the database on the main thread
    {
        LockHolder locker(m_urlAndIconLock);
        Vector<IconSnapshot> iconSnapshots;
        Vector<PageURLSnapshot> pageSnapshots;
        {
            LockHolder locker(m_pendingSyncLock);

            iconSnapshots.appendRange(m_iconsPendingSync.begin().values(), m_iconsPendingSync.end().values());
            m_iconsPendingSync.clear();

            pageSnapshots.appendRange(m_pageURLsPendingSync.begin().values(), m_pageURLsPendingSync.end().values());
            m_pageURLsPendingSync.clear();
        }

        if (iconSnapshots.size() || pageSnapshots.size())
            didAnyWork = true;

        SQLiteTransaction syncTransaction(m_syncDB);
        syncTransaction.begin();

        for (auto& snapshot : iconSnapshots) {
            writeIconSnapshotToSQLDatabase(snapshot);
            LOG(IconDatabase, "Wrote IconRecord for IconURL %s with timeStamp of %i to the DB", urlForLogging(snapshot.iconURL()).ascii().data(), snapshot.timestamp());
        }

        for (auto& snapshot : pageSnapshots) {
            // If the icon URL is empty, this page is meant to be deleted
            // ASSERTs are sanity checks to make sure the mappings exist if they should and don't if they shouldn't
            if (snapshot.iconURL().isEmpty())
                removePageURLFromSQLDatabase(snapshot.pageURL());
            else
                setIconURLForPageURLInSQLDatabase(snapshot.iconURL(), snapshot.pageURL());
            LOG(IconDatabase, "Committed IconURL for PageURL %s to database", urlForLogging(snapshot.pageURL()).ascii().data());
        }

        syncTransaction.commit();
    }

    // Check to make sure there are no dangling PageURLs - If there are, we want to output one log message but not spam the console potentially every few seconds
    if (didAnyWork)
        checkForDanglingPageURLs(false);

#if !LOG_DISABLED
    Seconds delta = MonotonicTime::now() - timeStamp;
    LOG(IconDatabase, "Updating the database took %.4f seconds", delta.value());
#endif

    return didAnyWork;
}

void IconDatabase::pruneUnretainedIcons()
{
    ASSERT_ICON_SYNC_THREAD();

    if (!isOpen())
        return;

    // This method should only be called once per run
    ASSERT(!m_initialPruningComplete);

    // This method relies on having read in all page URLs from the database earlier.
    ASSERT(m_iconURLImportComplete);

    // Get the known PageURLs from the db, and record the ID of any that are not in the retain count set.
    Vector<int64_t> pageIDsToDelete;

    SQLiteStatement pageSQL(m_syncDB, "SELECT rowid, url FROM PageURL;");
    pageSQL.prepare();

    int result;
    while ((result = pageSQL.step()) == SQLITE_ROW) {
        LockHolder locker(m_urlAndIconLock);
        if (!m_pageURLToRecordMap.contains(pageSQL.getColumnText(1)))
            pageIDsToDelete.append(pageSQL.getColumnInt64(0));
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Error reading PageURL table from on-disk DB");
    pageSQL.finalize();

    // Delete page URLs that were in the table, but not in our retain count set.
    size_t numToDelete = pageIDsToDelete.size();
    if (numToDelete) {
        SQLiteTransaction pruningTransaction(m_syncDB);
        pruningTransaction.begin();

        SQLiteStatement pageDeleteSQL(m_syncDB, "DELETE FROM PageURL WHERE rowid = (?);");
        pageDeleteSQL.prepare();
        for (size_t i = 0; i < numToDelete; ++i) {
            LOG(IconDatabase, "Pruning page with rowid %lli from disk", static_cast<long long>(pageIDsToDelete[i]));
            pageDeleteSQL.bindInt64(1, pageIDsToDelete[i]);
            int result = pageDeleteSQL.step();
            if (result != SQLITE_DONE)
                LOG_ERROR("Unabled to delete page with id %lli from disk", static_cast<long long>(pageIDsToDelete[i]));
            pageDeleteSQL.reset();

            // If the thread was asked to terminate, we should commit what pruning we've done so far, figuring we can
            // finish the rest later (hopefully)
            if (shouldStopThreadActivity()) {
                pruningTransaction.commit();
                return;
            }
        }
        pruningTransaction.commit();
        pageDeleteSQL.finalize();
    }

    // Deleting unreferenced icons from the Icon tables has to be atomic -
    // If the user quits while these are taking place, they might have to wait. Thankfully this will rarely be an issue
    // A user on a network home directory with a wildly inconsistent database might see quite a pause...

    SQLiteTransaction pruningTransaction(m_syncDB);
    pruningTransaction.begin();

    // Wipe Icons that aren't retained
    if (!m_syncDB.executeCommand("DELETE FROM IconData WHERE iconID NOT IN (SELECT iconID FROM PageURL);"))
        LOG_ERROR("Failed to execute SQL to prune unretained icons from the on-disk IconData table");
    if (!m_syncDB.executeCommand("DELETE FROM IconInfo WHERE iconID NOT IN (SELECT iconID FROM PageURL);"))
        LOG_ERROR("Failed to execute SQL to prune unretained icons from the on-disk IconInfo table");

    pruningTransaction.commit();

    checkForDanglingPageURLs(true);

    m_initialPruningComplete = true;
}

void IconDatabase::checkForDanglingPageURLs(bool pruneIfFound)
{
    ASSERT_ICON_SYNC_THREAD();

    // This check can be relatively expensive so we don't do it in a release build unless the caller has asked us to prune any dangling
    // entries. We also don't want to keep performing this check and reporting this error if it has already found danglers before so we
    // keep track of whether we've found any. We skip the check in the release build pretending to have already found danglers already.
#ifndef NDEBUG
    static bool danglersFound = true;
#else
    static bool danglersFound = false;
#endif

    if ((pruneIfFound || !danglersFound) && SQLiteStatement(m_syncDB, "SELECT url FROM PageURL WHERE PageURL.iconID NOT IN (SELECT iconID FROM IconInfo) LIMIT 1;").returnsAtLeastOneResult()) {
        danglersFound = true;
        LOG(IconDatabase, "Dangling PageURL entries found");
        if (pruneIfFound && !m_syncDB.executeCommand("DELETE FROM PageURL WHERE iconID NOT IN (SELECT iconID FROM IconInfo);"))
            LOG(IconDatabase, "Unable to prune dangling PageURLs");
    }
}

void IconDatabase::removeAllIconsOnThread()
{
    ASSERT_ICON_SYNC_THREAD();

    LOG(IconDatabase, "Removing all icons on the sync thread");

    // Delete all the prepared statements so they can start over
    deleteAllPreparedStatements();

    // To reset the on-disk database, we'll wipe all its tables then vacuum it
    // This is easier and safer than closing it, deleting the file, and recreating from scratch
    m_syncDB.clearAllTables();
    m_syncDB.runVacuumCommand();
    createDatabaseTables(m_syncDB);
}

void IconDatabase::deleteAllPreparedStatements()
{
    ASSERT_ICON_SYNC_THREAD();

    m_setIconIDForPageURLStatement = nullptr;
    m_removePageURLStatement = nullptr;
    m_getIconIDForIconURLStatement = nullptr;
    m_getImageDataForIconURLStatement = nullptr;
    m_addIconToIconInfoStatement = nullptr;
    m_addIconToIconDataStatement = nullptr;
    m_getImageDataStatement = nullptr;
    m_deletePageURLsForIconURLStatement = nullptr;
    m_deleteIconFromIconInfoStatement = nullptr;
    m_deleteIconFromIconDataStatement = nullptr;
    m_updateIconInfoStatement = nullptr;
    m_updateIconDataStatement  = nullptr;
    m_setIconInfoStatement = nullptr;
    m_setIconDataStatement = nullptr;
}

void* IconDatabase::cleanupSyncThread()
{
    ASSERT_ICON_SYNC_THREAD();

#if !LOG_DISABLED
    MonotonicTime timeStamp = MonotonicTime::now();
#endif

    // If the removeIcons flag is set, remove all icons from the db.
    if (m_removeIconsRequested)
        removeAllIconsOnThread();

    // Sync remaining icons out
    LOG(IconDatabase, "(THREAD) Doing final writeout and closure of sync thread");
    writeToDatabase();

    // Close the database
    LockHolder locker(m_syncLock);

    m_databaseDirectory = String();
    m_completeDatabasePath = String();
    deleteAllPreparedStatements();
    m_syncDB.close();

#if !LOG_DISABLED
    Seconds delta = MonotonicTime::now() - timeStamp;
    LOG(IconDatabase, "(THREAD) Final closure took %.4f seconds", delta.value());
#endif

    m_syncThreadRunning = false;
    return nullptr;
}

// readySQLiteStatement() handles two things
// 1 - If the SQLDatabase& argument is different, the statement must be destroyed and remade. This happens when the user
//     switches to and from private browsing
// 2 - Lazy construction of the Statement in the first place, in case we've never made this query before
inline void readySQLiteStatement(std::unique_ptr<SQLiteStatement>& statement, SQLiteDatabase& db, const String& str)
{
    if (statement && (&statement->database() != &db || statement->isExpired())) {
        if (statement->isExpired())
            LOG(IconDatabase, "SQLiteStatement associated with %s is expired", str.ascii().data());
        statement = nullptr;
    }
    if (!statement) {
        statement = std::make_unique<SQLiteStatement>(db, str);
        if (statement->prepare() != SQLITE_OK)
            LOG_ERROR("Preparing statement %s failed", str.ascii().data());
    }
}

void IconDatabase::setIconURLForPageURLInSQLDatabase(const String& iconURL, const String& pageURL)
{
    ASSERT_ICON_SYNC_THREAD();

    int64_t iconID = getIconIDForIconURLFromSQLDatabase(iconURL);

    if (!iconID)
        iconID = addIconURLToSQLDatabase(iconURL);

    if (!iconID) {
        LOG_ERROR("Failed to establish an ID for iconURL %s", urlForLogging(iconURL).ascii().data());
        ASSERT_NOT_REACHED();
        return;
    }

    setIconIDForPageURLInSQLDatabase(iconID, pageURL);
}

void IconDatabase::setIconIDForPageURLInSQLDatabase(int64_t iconID, const String& pageURL)
{
    ASSERT_ICON_SYNC_THREAD();

    readySQLiteStatement(m_setIconIDForPageURLStatement, m_syncDB, "INSERT INTO PageURL (url, iconID) VALUES ((?), ?);");
    m_setIconIDForPageURLStatement->bindText(1, pageURL);
    m_setIconIDForPageURLStatement->bindInt64(2, iconID);

    int result = m_setIconIDForPageURLStatement->step();
    if (result != SQLITE_DONE) {
        ASSERT_NOT_REACHED();
        LOG_ERROR("setIconIDForPageURLQuery failed for url %s", urlForLogging(pageURL).ascii().data());
    }

    m_setIconIDForPageURLStatement->reset();
}

void IconDatabase::removePageURLFromSQLDatabase(const String& pageURL)
{
    ASSERT_ICON_SYNC_THREAD();

    readySQLiteStatement(m_removePageURLStatement, m_syncDB, "DELETE FROM PageURL WHERE url = (?);");
    m_removePageURLStatement->bindText(1, pageURL);

    if (m_removePageURLStatement->step() != SQLITE_DONE)
        LOG_ERROR("removePageURLFromSQLDatabase failed for url %s", urlForLogging(pageURL).ascii().data());

    m_removePageURLStatement->reset();
}


int64_t IconDatabase::getIconIDForIconURLFromSQLDatabase(const String& iconURL)
{
    ASSERT_ICON_SYNC_THREAD();

    readySQLiteStatement(m_getIconIDForIconURLStatement, m_syncDB, "SELECT IconInfo.iconID FROM IconInfo WHERE IconInfo.url = (?);");
    m_getIconIDForIconURLStatement->bindText(1, iconURL);

    int64_t result = m_getIconIDForIconURLStatement->step();
    if (result == SQLITE_ROW)
        result = m_getIconIDForIconURLStatement->getColumnInt64(0);
    else {
        if (result != SQLITE_DONE)
            LOG_ERROR("getIconIDForIconURLFromSQLDatabase failed for url %s", urlForLogging(iconURL).ascii().data());
        result = 0;
    }

    m_getIconIDForIconURLStatement->reset();
    return result;
}

int64_t IconDatabase::addIconURLToSQLDatabase(const String& iconURL)
{
    ASSERT_ICON_SYNC_THREAD();

    // There would be a transaction here to make sure these two inserts are atomic
    // In practice the only caller of this method is always wrapped in a transaction itself so placing another
    // here is unnecessary

    readySQLiteStatement(m_addIconToIconInfoStatement, m_syncDB, "INSERT INTO IconInfo (url, stamp) VALUES (?, 0);");
    m_addIconToIconInfoStatement->bindText(1, iconURL);

    int result = m_addIconToIconInfoStatement->step();
    m_addIconToIconInfoStatement->reset();
    if (result != SQLITE_DONE) {
        LOG_ERROR("addIconURLToSQLDatabase failed to insert %s into IconInfo", urlForLogging(iconURL).ascii().data());
        return 0;
    }
    int64_t iconID = m_syncDB.lastInsertRowID();

    readySQLiteStatement(m_addIconToIconDataStatement, m_syncDB, "INSERT INTO IconData (iconID, data) VALUES (?, ?);");
    m_addIconToIconDataStatement->bindInt64(1, iconID);

    result = m_addIconToIconDataStatement->step();
    m_addIconToIconDataStatement->reset();
    if (result != SQLITE_DONE) {
        LOG_ERROR("addIconURLToSQLDatabase failed to insert %s into IconData", urlForLogging(iconURL).ascii().data());
        return 0;
    }

    return iconID;
}

RefPtr<SharedBuffer> IconDatabase::getImageDataForIconURLFromSQLDatabase(const String& iconURL)
{
    ASSERT_ICON_SYNC_THREAD();

    RefPtr<SharedBuffer> imageData;

    readySQLiteStatement(m_getImageDataForIconURLStatement, m_syncDB, "SELECT IconData.data FROM IconData WHERE IconData.iconID IN (SELECT iconID FROM IconInfo WHERE IconInfo.url = (?));");
    m_getImageDataForIconURLStatement->bindText(1, iconURL);

    int result = m_getImageDataForIconURLStatement->step();
    if (result == SQLITE_ROW) {
        Vector<char> data;
        m_getImageDataForIconURLStatement->getColumnBlobAsVector(0, data);
        imageData = SharedBuffer::create(data.data(), data.size());
    } else if (result != SQLITE_DONE)
        LOG_ERROR("getImageDataForIconURLFromSQLDatabase failed for url %s", urlForLogging(iconURL).ascii().data());

    m_getImageDataForIconURLStatement->reset();

    return imageData;
}

void IconDatabase::removeIconFromSQLDatabase(const String& iconURL)
{
    ASSERT_ICON_SYNC_THREAD();

    if (iconURL.isEmpty())
        return;

    // There would be a transaction here to make sure these removals are atomic
    // In practice the only caller of this method is always wrapped in a transaction itself so placing another here is unnecessary

    // It's possible this icon is not in the database because of certain rapid browsing patterns (such as a stress test) where the
    // icon is marked to be added then marked for removal before it is ever written to disk. No big deal, early return
    int64_t iconID = getIconIDForIconURLFromSQLDatabase(iconURL);
    if (!iconID)
        return;

    readySQLiteStatement(m_deletePageURLsForIconURLStatement, m_syncDB, "DELETE FROM PageURL WHERE PageURL.iconID = (?);");
    m_deletePageURLsForIconURLStatement->bindInt64(1, iconID);

    if (m_deletePageURLsForIconURLStatement->step() != SQLITE_DONE)
        LOG_ERROR("m_deletePageURLsForIconURLStatement failed for url %s", urlForLogging(iconURL).ascii().data());

    readySQLiteStatement(m_deleteIconFromIconInfoStatement, m_syncDB, "DELETE FROM IconInfo WHERE IconInfo.iconID = (?);");
    m_deleteIconFromIconInfoStatement->bindInt64(1, iconID);

    if (m_deleteIconFromIconInfoStatement->step() != SQLITE_DONE)
        LOG_ERROR("m_deleteIconFromIconInfoStatement failed for url %s", urlForLogging(iconURL).ascii().data());

    readySQLiteStatement(m_deleteIconFromIconDataStatement, m_syncDB, "DELETE FROM IconData WHERE IconData.iconID = (?);");
    m_deleteIconFromIconDataStatement->bindInt64(1, iconID);

    if (m_deleteIconFromIconDataStatement->step() != SQLITE_DONE)
        LOG_ERROR("m_deleteIconFromIconDataStatement failed for url %s", urlForLogging(iconURL).ascii().data());

    m_deletePageURLsForIconURLStatement->reset();
    m_deleteIconFromIconInfoStatement->reset();
    m_deleteIconFromIconDataStatement->reset();
}

void IconDatabase::writeIconSnapshotToSQLDatabase(const IconSnapshot& snapshot)
{
    ASSERT_ICON_SYNC_THREAD();

    if (snapshot.iconURL().isEmpty())
        return;

    // A nulled out timestamp and data means this icon is destined to be deleted - do that instead of writing it out
    if (!snapshot.timestamp() && !snapshot.data()) {
        LOG(IconDatabase, "Removing %s from on-disk database", urlForLogging(snapshot.iconURL()).ascii().data());
        removeIconFromSQLDatabase(snapshot.iconURL());
        return;
    }

    // There would be a transaction here to make sure these removals are atomic
    // In practice the only caller of this method is always wrapped in a transaction itself so placing another here is unnecessary

    // Get the iconID for this url
    int64_t iconID = getIconIDForIconURLFromSQLDatabase(snapshot.iconURL());

    // If there is already an iconID in place, update the database.
    // Otherwise, insert new records
    if (iconID) {
        readySQLiteStatement(m_updateIconInfoStatement, m_syncDB, "UPDATE IconInfo SET stamp = ?, url = ? WHERE iconID = ?;");
        m_updateIconInfoStatement->bindInt64(1, snapshot.timestamp());
        m_updateIconInfoStatement->bindText(2, snapshot.iconURL());
        m_updateIconInfoStatement->bindInt64(3, iconID);

        if (m_updateIconInfoStatement->step() != SQLITE_DONE)
            LOG_ERROR("Failed to update icon info for url %s", urlForLogging(snapshot.iconURL()).ascii().data());

        m_updateIconInfoStatement->reset();

        readySQLiteStatement(m_updateIconDataStatement, m_syncDB, "UPDATE IconData SET data = ? WHERE iconID = ?;");
        m_updateIconDataStatement->bindInt64(2, iconID);

        // If we *have* image data, bind it to this statement - Otherwise bind "null" for the blob data,
        // signifying that this icon doesn't have any data
        if (snapshot.data() && snapshot.data()->size())
            m_updateIconDataStatement->bindBlob(1, snapshot.data()->data(), snapshot.data()->size());
        else
            m_updateIconDataStatement->bindNull(1);

        if (m_updateIconDataStatement->step() != SQLITE_DONE)
            LOG_ERROR("Failed to update icon data for url %s", urlForLogging(snapshot.iconURL()).ascii().data());

        m_updateIconDataStatement->reset();
    } else {
        readySQLiteStatement(m_setIconInfoStatement, m_syncDB, "INSERT INTO IconInfo (url,stamp) VALUES (?, ?);");
        m_setIconInfoStatement->bindText(1, snapshot.iconURL());
        m_setIconInfoStatement->bindInt64(2, snapshot.timestamp());

        if (m_setIconInfoStatement->step() != SQLITE_DONE)
            LOG_ERROR("Failed to set icon info for url %s", urlForLogging(snapshot.iconURL()).ascii().data());

        m_setIconInfoStatement->reset();

        int64_t iconID = m_syncDB.lastInsertRowID();

        readySQLiteStatement(m_setIconDataStatement, m_syncDB, "INSERT INTO IconData (iconID, data) VALUES (?, ?);");
        m_setIconDataStatement->bindInt64(1, iconID);

        // If we *have* image data, bind it to this statement - Otherwise bind "null" for the blob data,
        // signifying that this icon doesn't have any data
        if (snapshot.data() && snapshot.data()->size())
            m_setIconDataStatement->bindBlob(2, snapshot.data()->data(), snapshot.data()->size());
        else
            m_setIconDataStatement->bindNull(2);

        if (m_setIconDataStatement->step() != SQLITE_DONE)
            LOG_ERROR("Failed to set icon data for url %s", urlForLogging(snapshot.iconURL()).ascii().data());

        m_setIconDataStatement->reset();
    }
}

void IconDatabase::dispatchDidImportIconURLForPageURLOnMainThread(const String& pageURL)
{
    ASSERT_ICON_SYNC_THREAD();

    m_mainThreadNotifier.notify([this, pageURL = pageURL.isolatedCopy()] {
        if (m_client)
            m_client->didImportIconURLForPageURL(pageURL);
    });
}

void IconDatabase::dispatchDidImportIconDataForPageURLOnMainThread(const String& pageURL)
{
    ASSERT_ICON_SYNC_THREAD();

    m_mainThreadNotifier.notify([this, pageURL = pageURL.isolatedCopy()] {
        if (m_client)
            m_client->didImportIconDataForPageURL(pageURL);
    });
}

void IconDatabase::dispatchDidFinishURLImportOnMainThread()
{
    ASSERT_ICON_SYNC_THREAD();

    m_mainThreadNotifier.notify([this] {
        if (m_client)
            m_client->didFinishURLImport();
    });
}

} // namespace WebKit
