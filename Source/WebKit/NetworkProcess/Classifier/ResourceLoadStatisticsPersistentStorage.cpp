/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsPersistentStorage.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Logging.h"
#include "PersistencyUtils.h"
#include "ResourceLoadStatisticsMemoryStore.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/FileMonitor.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

constexpr Seconds minimumWriteInterval { 5_min };

using namespace WebCore;

static bool hasFileChangedSince(const String& path, WallTime since)
{
    ASSERT(!RunLoop::isMain());
    auto modificationTime = FileSystem::getFileModificationTime(path);
    if (!modificationTime)
        return true;

    return modificationTime.value() > since;
}

ResourceLoadStatisticsPersistentStorage::ResourceLoadStatisticsPersistentStorage(ResourceLoadStatisticsMemoryStore& memoryStore, WorkQueue& workQueue, const String& storageDirectoryPath)
    : m_memoryStore(memoryStore)
    , m_workQueue(workQueue)
    , m_storageDirectoryPath(storageDirectoryPath)
{
    RELEASE_ASSERT(!RunLoop::isMain());

    m_memoryStore.setPersistentStorage(*this);

    startMonitoringDisk();
}

ResourceLoadStatisticsPersistentStorage::~ResourceLoadStatisticsPersistentStorage()
{
    RELEASE_ASSERT(!RunLoop::isMain());

    if (m_hasPendingWrite)
        writeMemoryStoreToDisk();
}

String ResourceLoadStatisticsPersistentStorage::storageDirectoryPathIsolatedCopy() const
{
    return m_storageDirectoryPath.isolatedCopy();
}

String ResourceLoadStatisticsPersistentStorage::resourceLogFilePath() const
{
    String storagePath = storageDirectoryPathIsolatedCopy();
    if (storagePath.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storagePath, "full_browsing_session_resourceLog.plist");
}

void ResourceLoadStatisticsPersistentStorage::startMonitoringDisk()
{
    ASSERT(!RunLoop::isMain());
    if (m_fileMonitor)
        return;

    String resourceLogPath = resourceLogFilePath();
    if (resourceLogPath.isEmpty())
        return;

    m_fileMonitor = makeUnique<FileMonitor>(resourceLogPath, m_workQueue.copyRef(), [this, weakThis = makeWeakPtr(*this)] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        if (!weakThis)
            return;

        switch (type) {
        case FileMonitor::FileChangeType::Modification:
            refreshMemoryStoreFromDisk();
            break;
        case FileMonitor::FileChangeType::Removal:
            m_memoryStore.clear([] { });
            m_fileMonitor = nullptr;
            monitorDirectoryForNewStatistics();
            break;
        }
    });
}

void ResourceLoadStatisticsPersistentStorage::monitorDirectoryForNewStatistics()
{
    ASSERT(!RunLoop::isMain());

    String storagePath = storageDirectoryPathIsolatedCopy();
    ASSERT(!storagePath.isEmpty());

    if (!FileSystem::fileExists(storagePath)) {
        if (!FileSystem::makeAllDirectories(storagePath)) {
            RELEASE_LOG_ERROR(ResourceLoadStatistics, "ResourceLoadStatisticsPersistentStorage: Failed to create directory path %s", storagePath.utf8().data());
            return;
        }
    }

    m_fileMonitor = makeUnique<FileMonitor>(storagePath, m_workQueue.copyRef(), [this] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        if (type == FileMonitor::FileChangeType::Removal) {
            // Directory was removed!
            m_fileMonitor = nullptr;
            return;
        }

        String resourceLogPath = resourceLogFilePath();
        ASSERT(!resourceLogPath.isEmpty());

        if (!FileSystem::fileExists(resourceLogPath))
            return;

        m_fileMonitor = nullptr;

        refreshMemoryStoreFromDisk();
        startMonitoringDisk();
    });
}

void ResourceLoadStatisticsPersistentStorage::stopMonitoringDisk()
{
    ASSERT(!RunLoop::isMain());
    m_fileMonitor = nullptr;
}

// This is called when the file changes on disk.
void ResourceLoadStatisticsPersistentStorage::refreshMemoryStoreFromDisk()
{
    ASSERT(!RunLoop::isMain());

    String filePath = resourceLogFilePath();
    if (filePath.isEmpty())
        return;

    // We sometimes see file changed events from before our load completed (we start
    // reading at the first change event, but we might receive a series of events related
    // to the same file operation). Catch this case to avoid reading overly often.
    if (!hasFileChangedSince(filePath, m_lastStatisticsFileSyncTime))
        return;

    WallTime readTime = WallTime::now();

    auto decoder = createForFile(filePath);
    if (!decoder)
        return;

    m_memoryStore.mergeWithDataFromDecoder(*decoder);
    m_lastStatisticsFileSyncTime = readTime;
}

void ResourceLoadStatisticsPersistentStorage::populateMemoryStoreFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    String filePath = resourceLogFilePath();
    if (filePath.isEmpty() || !FileSystem::fileExists(filePath)) {
        m_memoryStore.grandfatherExistingWebsiteData(WTFMove(completionHandler));
        monitorDirectoryForNewStatistics();
        return;
    }

    if (!hasFileChangedSince(filePath, m_lastStatisticsFileSyncTime)) {
        // No need to grandfather in this case.
        completionHandler();
        return;
    }

    WallTime readTime = WallTime::now();

    auto decoder = createForFile(filePath);
    if (!decoder) {
        m_memoryStore.grandfatherExistingWebsiteData(WTFMove(completionHandler));
        return;
    }

    // Debug mode has a prepoulated memory store.
    ASSERT_WITH_MESSAGE(m_memoryStore.isEmpty() || m_memoryStore.isDebugModeEnabled(), "This is the initial import so the store should be empty");
    m_memoryStore.mergeWithDataFromDecoder(*decoder);

    m_lastStatisticsFileSyncTime = readTime;

    m_memoryStore.logTestingEvent("PopulatedWithoutGrandfathering"_s);
    completionHandler();
}

void ResourceLoadStatisticsPersistentStorage::writeMemoryStoreToDisk()
{
    ASSERT(!RunLoop::isMain());

    m_hasPendingWrite = false;
    stopMonitoringDisk();

    writeToDisk(m_memoryStore.createEncoderFromData(), resourceLogFilePath());

    m_lastStatisticsFileSyncTime = WallTime::now();
    m_lastStatisticsWriteTime = MonotonicTime::now();

    startMonitoringDisk();
}

void ResourceLoadStatisticsPersistentStorage::scheduleOrWriteMemoryStore(ForceImmediateWrite forceImmediateWrite)
{
    ASSERT(!RunLoop::isMain());

    auto timeSinceLastWrite = MonotonicTime::now() - m_lastStatisticsWriteTime;
    if (forceImmediateWrite != ForceImmediateWrite::Yes && timeSinceLastWrite < minimumWriteInterval) {
        if (!m_hasPendingWrite) {
            m_hasPendingWrite = true;
            Seconds delay = minimumWriteInterval - timeSinceLastWrite + 1_s;
            m_workQueue->dispatchAfter(delay, [weakThis = makeWeakPtr(*this)] {
                if (weakThis)
                    weakThis->writeMemoryStoreToDisk();
            });
        }
        return;
    }

    writeMemoryStoreToDisk();
}

void ResourceLoadStatisticsPersistentStorage::clear()
{
    ASSERT(!RunLoop::isMain());
    String filePath = resourceLogFilePath();
    if (filePath.isEmpty())
        return;

    stopMonitoringDisk();

    if (!FileSystem::deleteFile(filePath) && FileSystem::fileExists(filePath))
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "ResourceLoadStatisticsPersistentStorage: Unable to delete statistics file: %s", filePath.utf8().data());
}

#if !PLATFORM(IOS_FAMILY)
void ResourceLoadStatisticsPersistentStorage::excludeFromBackup() const
{
}
#endif

} // namespace WebKit

#endif
