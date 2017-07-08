/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WebResourceLoadStatisticsStore.h"

#include "Logging.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataType.h"
#include <WebCore/FileMonitor.h>
#include <WebCore/FileSystem.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/WallTime.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#define ENABLE_MULTIPROCESS_ACCESS_TO_STATISTICS_STORE 1
#endif

using namespace WebCore;

namespace WebKit {

template<typename T> static inline String primaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value);
}

constexpr Seconds minimumStatisticsFileWriteInterval { 5_min };

static bool notifyPagesWhenDataRecordsWereScanned = false;
static bool shouldClassifyResourcesBeforeDataRecordsRemoval = true;
static auto shouldSubmitTelemetry = true;

static const OptionSet<WebsiteDataType>& dataTypesToRemove()
{
    static NeverDestroyed<OptionSet<WebsiteDataType>> dataTypes(std::initializer_list<WebsiteDataType>({
        WebsiteDataType::Cookies,
        WebsiteDataType::IndexedDBDatabases,
        WebsiteDataType::LocalStorage,
#if ENABLE(MEDIA_STREAM)
        WebsiteDataType::MediaDeviceIdentifier,
#endif
        WebsiteDataType::MediaKeys,
        WebsiteDataType::OfflineWebApplicationCache,
#if ENABLE(NETSCAPE_PLUGIN_API)
        WebsiteDataType::PlugInData,
#endif
        WebsiteDataType::SearchFieldRecentSearches,
        WebsiteDataType::SessionStorage,
        WebsiteDataType::WebSQLDatabases,
    }));

    ASSERT(RunLoop::isMain());

    return dataTypes;
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory, UpdatePartitionCookiesForDomainsHandler&& updatePartitionCookiesForDomainsHandler)
    : m_resourceLoadStatisticsStore(ResourceLoadStatisticsStore::create())
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_statisticsStoragePath(resourceLoadStatisticsDirectory)
    , m_telemetryOneShotTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::submitTelemetryIfNecessary)
    , m_telemetryRepeatedTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
{
    ASSERT(RunLoop::isMain());

#if PLATFORM(COCOA)
    registerUserDefaultsIfNeeded();
#endif

    m_resourceLoadStatisticsStore->setNotificationCallback([this, protectedThis = makeRef(*this)] {
        if (m_resourceLoadStatisticsStore->isEmpty())
            return;
        processStatisticsAndDataRecords();
    });
    m_resourceLoadStatisticsStore->setGrandfatherExistingWebsiteDataCallback([this, protectedThis = makeRef(*this)] {
        grandfatherExistingWebsiteData();
    });
    m_resourceLoadStatisticsStore->setDeletePersistentStoreCallback([this, protectedThis = makeRef(*this)] {
        m_statisticsQueue->dispatch([this, protectedThis = protectedThis.copyRef()] {
            deleteStoreFromDisk();
        });
    });
    m_resourceLoadStatisticsStore->setFireTelemetryCallback([this, protectedThis = makeRef(*this)] {
        submitTelemetry();
    });

    if (updatePartitionCookiesForDomainsHandler) {
        m_resourceLoadStatisticsStore->setShouldPartitionCookiesCallback([updatePartitionCookiesForDomainsHandler = WTFMove(updatePartitionCookiesForDomainsHandler)] (const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool shouldClearFirst) {
            updatePartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, shouldClearFirst);
        });
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        readDataFromDiskIfNeeded();
        startMonitoringStatisticsStorage();
    });

    m_telemetryOneShotTimer.startOneShot(5_s);
    m_telemetryRepeatedTimer.startRepeating(24_h);
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool always)
{
    notifyPagesWhenDataRecordsWereScanned = always;
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    shouldSubmitTelemetry = value;
}
    
void WebResourceLoadStatisticsStore::classifyResource(ResourceLoadStatistics& resourceStatistic)
{
    if (!resourceStatistic.isPrevalentResource && m_resourceLoadStatisticsClassifier.hasPrevalentResourceCharacteristics(resourceStatistic))
        resourceStatistic.isPrevalentResource = true;
}
    
void WebResourceLoadStatisticsStore::removeDataRecords()
{
    ASSERT(!RunLoop::isMain());
    
    if (!coreStore().shouldRemoveDataRecords())
        return;

    auto prevalentResourceDomains = coreStore().topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    if (prevalentResourceDomains.isEmpty())
        return;
    
    coreStore().dataRecordsBeingRemoved();

    // Switch to the main thread to get the default website data store
    RunLoop::main().dispatch([prevalentResourceDomains = CrossThreadCopier<Vector<String>>::copy(prevalentResourceDomains), this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(dataTypesToRemove(), WTFMove(prevalentResourceDomains), notifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)](const HashSet<String>& domainsWithDeletedWebsiteData) mutable {
            // But always touch the ResourceLoadStatistics store on the worker queue.
            m_statisticsQueue->dispatch([protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(domainsWithDeletedWebsiteData)] () mutable {
                protectedThis->coreStore().updateStatisticsForRemovedDataRecords(topDomains);
                protectedThis->coreStore().dataRecordsWereRemoved();
            });
        });
    });
}

void WebResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] () {
        if (shouldClassifyResourcesBeforeDataRecordsRemoval) {
            coreStore().processStatistics([this] (ResourceLoadStatistics& resourceStatistic) {
                classifyResource(resourceStatistic);
            });
        }
        removeDataRecords();
        
        if (notifyPagesWhenDataRecordsWereScanned) {
            RunLoop::main().dispatch([] () mutable {
                WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
            });
        }

        scheduleOrWriteStoreToDisk();
    });
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(const Vector<WebCore::ResourceLoadStatistics>& origins)
{
    coreStore().mergeStatistics(origins);
    // Fire before processing statistics to propagate user
    // interaction as fast as possible to the network process.
    coreStore().fireShouldPartitionCookiesHandler();
    processStatisticsAndDataRecords();
}

void WebResourceLoadStatisticsStore::grandfatherExistingWebsiteData()
{
    // Switch to the main thread to get the default website data store
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::topPrivatelyControlledDomainsWithWebsiteData(dataTypesToRemove(), notifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)] (HashSet<String>&& topPrivatelyControlledDomainsWithWebsiteData) mutable {
            // But always touch the ResourceLoadStatistics store on the worker queue
            m_statisticsQueue->dispatch([protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(topPrivatelyControlledDomainsWithWebsiteData)] () mutable {
                protectedThis->coreStore().handleFreshStartWithEmptyOrNoStore(WTFMove(topDomains));
            });
        });
    });
}

WallTime WebResourceLoadStatisticsStore::statisticsFileModificationTime(const String& path) const
{
    ASSERT(!RunLoop::isMain());
    time_t modificationTime;
    if (!getFileModificationTime(path, modificationTime))
        return { };

    return WallTime::fromRawSeconds(modificationTime);
}

bool WebResourceLoadStatisticsStore::hasStatisticsFileChangedSinceLastSync(const String& path)
{
    return statisticsFileModificationTime(path) > m_lastStatisticsFileSyncTime;
}

void WebResourceLoadStatisticsStore::readDataFromDiskIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    String resourceLog = resourceLogFilePath();
    if (resourceLog.isEmpty() || !fileExists(resourceLog)) {
        grandfatherExistingWebsiteData();
        return;
    }

    if (!hasStatisticsFileChangedSinceLastSync(resourceLog)) {
        // No need to grandfather in this case.
        return;
    }

    WallTime readTime = WallTime::now();

    auto decoder = createDecoderFromDisk(resourceLog);
    if (!decoder) {
        grandfatherExistingWebsiteData();
        return;
    }

    coreStore().clearInMemory();
    coreStore().readDataFromDecoder(*decoder);

    m_lastStatisticsFileSyncTime = readTime;

    if (coreStore().isEmpty())
        grandfatherExistingWebsiteData();

    coreStore().includeTodayAsOperatingDateIfNecessary();
}
    
void WebResourceLoadStatisticsStore::refreshFromDisk()
{
    ASSERT(!RunLoop::isMain());

    String resourceLog = resourceLogFilePath();
    if (resourceLog.isEmpty())
        return;

    // We sometimes see file changed events from before our load completed (we start
    // reading at the first change event, but we might receive a series of events related
    // to the same file operation). Catch this case to avoid reading overly often.
    if (!hasStatisticsFileChangedSinceLastSync(resourceLog))
        return;

    WallTime readTime = WallTime::now();

    auto decoder = createDecoderFromDisk(resourceLog);
    if (!decoder)
        return;

    coreStore().readDataFromDecoder(*decoder);
    m_lastStatisticsFileSyncTime = readTime;
}
    
void WebResourceLoadStatisticsStore::processWillOpenConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), m_statisticsQueue.get(), this);
}

void WebResourceLoadStatisticsStore::processDidCloseConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    BinarySemaphore semaphore;
    m_statisticsQueue->dispatch([&semaphore, this, protectedThis = makeRef(*this)] {
        // Write final file state to disk.
        if (m_didScheduleWrite)
            writeStoreToDisk();

        // Make sure any ongoing work in our queue is finished before we terminate.
        semaphore.signal();
    });
    semaphore.wait(WallTime::infinity());
}

String WebResourceLoadStatisticsStore::statisticsStoragePath() const
{
    return m_statisticsStoragePath.isolatedCopy();
}

String WebResourceLoadStatisticsStore::resourceLogFilePath() const
{
    String statisticsStoragePath = this->statisticsStoragePath();
    if (statisticsStoragePath.isEmpty())
        return emptyString();

    // TODO Decide what to call this file
    return pathByAppendingComponent(statisticsStoragePath, "full_browsing_session_resourceLog.plist");
}

void WebResourceLoadStatisticsStore::writeStoreToDisk()
{
    ASSERT(!RunLoop::isMain());
    
    stopMonitoringStatisticsStorage();

    syncWithExistingStatisticsStorageIfNeeded();

    auto encoder = coreStore().createEncoderFromData();

    String resourceLog = resourceLogFilePath();
    writeEncoderToDisk(*encoder.get(), resourceLog);

    m_lastStatisticsFileSyncTime = WallTime::now();
    m_lastStatisticsWriteTime = MonotonicTime::now();

    startMonitoringStatisticsStorage();
    m_didScheduleWrite = false;
}

void WebResourceLoadStatisticsStore::scheduleOrWriteStoreToDisk()
{
    ASSERT(!RunLoop::isMain());

    auto timeSinceLastWrite = MonotonicTime::now() - m_lastStatisticsWriteTime;
    if (timeSinceLastWrite < minimumStatisticsFileWriteInterval) {
        if (!m_didScheduleWrite) {
            m_didScheduleWrite = true;
            Seconds delayUntil = minimumStatisticsFileWriteInterval - timeSinceLastWrite + 1_s;
            m_statisticsQueue->dispatchAfter(delayUntil, [this, protectedThis = makeRef(*this)] {
                writeStoreToDisk();
            });
        }
        return;
    }

    writeStoreToDisk();
}

static PlatformFileHandle openAndLockFile(const String& path, FileOpenMode mode)
{
    ASSERT(!RunLoop::isMain());
    auto handle = openFile(path, mode);
    if (handle == invalidPlatformFileHandle)
        return invalidPlatformFileHandle;

#if ENABLE(MULTIPROCESS_ACCESS_TO_STATISTICS_STORE)
    bool locked = lockFile(handle, WebCore::LockExclusive);
    ASSERT_UNUSED(locked, locked);
#endif

    return handle;
}

static void closeAndUnlockFile(PlatformFileHandle handle)
{
    ASSERT(!RunLoop::isMain());
#if ENABLE(MULTIPROCESS_ACCESS_TO_STATISTICS_STORE)
    bool unlocked = unlockFile(handle);
    ASSERT_UNUSED(unlocked, unlocked);
#endif
    closeFile(handle);
}

void WebResourceLoadStatisticsStore::writeEncoderToDisk(KeyedEncoder& encoder, const String& path) const
{
    ASSERT(!RunLoop::isMain());
    
    RefPtr<SharedBuffer> rawData = encoder.finishEncoding();
    if (!rawData)
        return;

    auto statisticsStoragePath = this->statisticsStoragePath();
    if (!statisticsStoragePath.isEmpty()) {
        makeAllDirectories(statisticsStoragePath);
        platformExcludeFromBackup();
    }

    auto handle = openAndLockFile(path, OpenForWrite);
    if (handle == invalidPlatformFileHandle)
        return;
    
    int64_t writtenBytes = writeToFile(handle, rawData->data(), rawData->size());
    closeAndUnlockFile(handle);

    if (writtenBytes != static_cast<int64_t>(rawData->size()))
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "WebResourceLoadStatisticsStore: We only wrote %d out of %d bytes to disk", static_cast<unsigned>(writtenBytes), rawData->size());
}

void WebResourceLoadStatisticsStore::deleteStoreFromDisk()
{
    ASSERT(!RunLoop::isMain());
    String resourceLogPath = resourceLogFilePath();
    if (resourceLogPath.isEmpty())
        return;

    stopMonitoringStatisticsStorage();

    if (!deleteFile(resourceLogPath))
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to delete statistics file: %s", resourceLogPath.utf8().data());
}

void WebResourceLoadStatisticsStore::clearInMemoryData()
{
    ASSERT(!RunLoop::isMain());
    coreStore().clearInMemory();
}

void WebResourceLoadStatisticsStore::startMonitoringStatisticsStorage()
{
    ASSERT(!RunLoop::isMain());
    if (m_statisticsStorageMonitor)
        return;
    
    String resourceLogPath = resourceLogFilePath();
    if (resourceLogPath.isEmpty())
        return;
    
    m_statisticsStorageMonitor = std::make_unique<FileMonitor>(resourceLogPath, m_statisticsQueue.copyRef(), [this] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
        case FileMonitor::FileChangeType::Modification:
            refreshFromDisk();
            break;
        case FileMonitor::FileChangeType::Removal:
            clearInMemoryData();
            m_statisticsStorageMonitor = nullptr;
            break;
        }
    });
}

void WebResourceLoadStatisticsStore::stopMonitoringStatisticsStorage()
{
    ASSERT(!RunLoop::isMain());
    m_statisticsStorageMonitor = nullptr;
}

void WebResourceLoadStatisticsStore::syncWithExistingStatisticsStorageIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    if (m_statisticsStorageMonitor)
        return;

    String resourceLog = resourceLogFilePath();
    if (resourceLog.isEmpty() || !fileExists(resourceLog))
        return;

    refreshFromDisk();
}
    
    
#if !PLATFORM(COCOA)
void WebResourceLoadStatisticsStore::platformExcludeFromBackup() const
{
    // Do nothing
}
#endif

std::unique_ptr<KeyedDecoder> WebResourceLoadStatisticsStore::createDecoderFromDisk(const String& path) const
{
    ASSERT(!RunLoop::isMain());
    auto handle = openAndLockFile(path, OpenForRead);
    if (handle == invalidPlatformFileHandle)
        return nullptr;
    
    long long fileSize = 0;
    if (!getFileSize(handle, fileSize)) {
        closeAndUnlockFile(handle);
        return nullptr;
    }
    
    size_t bytesToRead;
    if (!WTF::convertSafely(fileSize, bytesToRead)) {
        closeAndUnlockFile(handle);
        return nullptr;
    }

    Vector<char> buffer(bytesToRead);
    size_t totalBytesRead = readFromFile(handle, buffer.data(), buffer.size());

    closeAndUnlockFile(handle);

    if (totalBytesRead != bytesToRead)
        return nullptr;

    return KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    coreStore().includeTodayAsOperatingDateIfNecessary();

    submitTelemetryIfNecessary();
}

void WebResourceLoadStatisticsStore::submitTelemetryIfNecessary()
{
    ASSERT(RunLoop::isMain());
    
    if (!shouldSubmitTelemetry)
        return;
    
    submitTelemetry();
}

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(coreStore());
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomainString = primaryDomain(url).isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomainString);
        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteractionTime = WallTime::now();

        coreStore().fireShouldPartitionCookiesHandler({ primaryDomainString }, { }, false);
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
        statistics.hadUserInteraction = false;
        statistics.mostRecentUserInteractionTime = { };
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        // FIXME: Why is this potentially creating a store entry just for querying?
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
        RunLoop::main().dispatch([hasHadUserInteraction = coreStore().hasHadRecentUserInteraction(statistics), completionHandler = WTFMove(completionHandler)] {
            completionHandler(hasHadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
        statistics.isPrevalentResource = true;
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        bool prevalentResource = coreStore().isPrevalentResource(primaryDomain(url));
        RunLoop::main().dispatch([prevalentResource, completionHandler = WTFMove(completionHandler)] {
            completionHandler(prevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
        statistics.isPrevalentResource = false;
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const URL& url, bool value)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), url = url.isolatedCopy(), value] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
        statistics.grandfathered = value;
    });
}

void WebResourceLoadStatisticsStore::isGrandfathered(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), url = url.isolatedCopy()] () mutable {
        bool grandFathered = coreStore().isGrandFathered(primaryDomain(url));
        RunLoop::main().dispatch([grandFathered, completionHandler = WTFMove(completionHandler)] {
            completionHandler(grandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    if (subframe.isBlankURL() || subframe.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomainString = primaryDomain(topFrame).isolatedCopy(), primarySubFrameDomainString = primaryDomain(subframe).isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primarySubFrameDomainString);
        statistics.subframeUnderTopFrameOrigins.add(primaryTopFrameDomainString);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomainString = primaryDomain(topFrame).isolatedCopy(), primarySubresourceDomainString = primaryDomain(subresource).isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomainString);
        statistics.subresourceUnderTopFrameOrigins.add(primaryTopFrameDomainString);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomainString = primaryDomain(hostNameRedirectedTo).isolatedCopy(), primarySubresourceDomainString = primaryDomain(subresource).isolatedCopy()] {
        auto& statistics = coreStore().ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomainString);
        statistics.subresourceUniqueRedirectsTo.add(primaryRedirectDomainString);
    });
}

void WebResourceLoadStatisticsStore::fireDataModificationHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        coreStore().fireDataModificationHandler();
    });
}

void WebResourceLoadStatisticsStore::fireShouldPartitionCookiesHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        coreStore().fireShouldPartitionCookiesHandler();
    });
}

void WebResourceLoadStatisticsStore::fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd), clearFirst] {
        coreStore().fireShouldPartitionCookiesHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
}

void WebResourceLoadStatisticsStore::fireTelemetryHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    coreStore().fireTelemetryHandler();
}

void WebResourceLoadStatisticsStore::clearInMemory()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        coreStore().clearInMemory();
    });
}

void WebResourceLoadStatisticsStore::clearInMemoryAndPersistent()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        coreStore().clearInMemoryAndPersistent();
    });
}

void WebResourceLoadStatisticsStore::clearInMemoryAndPersistent(std::chrono::system_clock::time_point modifiedSince)
{
    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    clearInMemoryAndPersistent();
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    coreStore().setTimeToLiveUserInteraction(seconds);
}

void WebResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    coreStore().setTimeToLiveCookiePartitionFree(seconds);
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    coreStore().setMinimumTimeBetweenDataRecordsRemoval(seconds);
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    coreStore().setGrandfatheringTime(seconds);
}
    
} // namespace WebKit
