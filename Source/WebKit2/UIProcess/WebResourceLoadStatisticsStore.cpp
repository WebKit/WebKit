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
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataType.h"
#include <WebCore/FileMonitor.h>
#include <WebCore/FileSystem.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WebCore;

namespace WebKit {

constexpr Seconds minimumStatisticsFileWriteInterval { 5_min };
constexpr unsigned operatingDatesWindow { 30 };
constexpr unsigned statisticsModelVersion { 7 };
constexpr unsigned maxImportance { 3 };

template<typename T> static inline String isolatedPrimaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value).isolatedCopy();
}

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

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory, UpdateCookiePartitioningForDomainsHandler&& updateCookiePartitioningForDomainsHandler)
    : m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_updateCookiePartitioningForDomainsHandler(WTFMove(updateCookiePartitioningForDomainsHandler))
    , m_statisticsStoragePath(resourceLoadStatisticsDirectory)
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
{
    ASSERT(RunLoop::isMain());

#if PLATFORM(COCOA)
    registerUserDefaultsIfNeeded();
#endif

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        readDataFromDiskIfNeeded();
        startMonitoringStatisticsStorage();
    });
    m_statisticsQueue->dispatchAfter(5_s, [this, protectedThis = makeRef(*this)] {
        if (m_shouldSubmitTelemetry)
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
    });

    m_dailyTasksTimer.startRepeating(24_h);
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
}
    
void WebResourceLoadStatisticsStore::removeDataRecords()
{
    ASSERT(!RunLoop::isMain());
    
    if (!shouldRemoveDataRecords())
        return;

    auto prevalentResourceDomains = topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    if (prevalentResourceDomains.isEmpty())
        return;
    
    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([prevalentResourceDomains = CrossThreadCopier<Vector<String>>::copy(prevalentResourceDomains), this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(dataTypesToRemove(), WTFMove(prevalentResourceDomains), m_shouldNotifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)](const HashSet<String>& domainsWithDeletedWebsiteData) mutable {
            m_statisticsQueue->dispatch([this, protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(domainsWithDeletedWebsiteData)] () mutable {
                for (auto& prevalentResourceDomain : topDomains) {
                    auto& statistic = ensureResourceStatisticsForPrimaryDomain(prevalentResourceDomain);
                    ++statistic.dataRecordsRemoved;
                }
                setDataRecordsBeingRemoved(false);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] () {
        if (m_shouldClassifyResourcesBeforeDataRecordsRemoval) {
            for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
                if (!resourceStatistic.isPrevalentResource && m_resourceLoadStatisticsClassifier.hasPrevalentResourceCharacteristics(resourceStatistic))
                    resourceStatistic.isPrevalentResource = true;
            }
        }
        removeDataRecords();
        
        pruneStatisticsIfNeeded();

        if (m_shouldNotifyPagesWhenDataRecordsWereScanned) {
            RunLoop::main().dispatch([] {
                WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
            });
        }

        scheduleOrWriteStoreToDisk();
    });
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins)
{
    ASSERT(!RunLoop::isMain());

    mergeStatistics(WTFMove(origins));
    // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
    updateCookiePartitioning();
    processStatisticsAndDataRecords();
}

void WebResourceLoadStatisticsStore::grandfatherExistingWebsiteData()
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::topPrivatelyControlledDomainsWithWebsiteData(dataTypesToRemove(), m_shouldNotifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)] (HashSet<String>&& topPrivatelyControlledDomainsWithWebsiteData) mutable {
            m_statisticsQueue->dispatch([this, protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(topPrivatelyControlledDomainsWithWebsiteData)] () mutable {
                for (auto& topPrivatelyControlledDomain : topDomains) {
                    auto& statistic = ensureResourceStatisticsForPrimaryDomain(topPrivatelyControlledDomain);
                    statistic.grandfathered = true;
                }
                m_endOfGrandfatheringTimestamp = WallTime::now() + m_grandfatheringTime;
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

bool WebResourceLoadStatisticsStore::hasStatisticsFileChangedSinceLastSync(const String& path) const
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

    clearInMemory();
    populateFromDecoder(*decoder);

    m_lastStatisticsFileSyncTime = readTime;

    if (m_resourceStatisticsMap.isEmpty())
        grandfatherExistingWebsiteData();

    includeTodayAsOperatingDateIfNecessary();
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

    populateFromDecoder(*decoder);
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
    // Make sure any pending work in our queue is finished before we terminate.
    m_statisticsQueue->dispatch([&semaphore, this, protectedThis = makeRef(*this)] {
        // Write final file state to disk.
        if (m_didScheduleWrite)
            writeStoreToDisk();

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

    return pathByAppendingComponent(statisticsStoragePath, "full_browsing_session_resourceLog.plist");
}

void WebResourceLoadStatisticsStore::writeStoreToDisk()
{
    ASSERT(!RunLoop::isMain());
    
    stopMonitoringStatisticsStorage();

    syncWithExistingStatisticsStorageIfNeeded();

    auto encoder = createEncoderFromData();
    RefPtr<SharedBuffer> rawData = encoder->finishEncoding();
    if (!rawData)
        return;

    auto statisticsStoragePath = this->statisticsStoragePath();
    if (!statisticsStoragePath.isEmpty()) {
        makeAllDirectories(statisticsStoragePath);
        platformExcludeFromBackup();
    }

    auto handle = openAndLockFile(resourceLogFilePath(), OpenForWrite);
    if (handle == invalidPlatformFileHandle)
        return;

    int64_t writtenBytes = writeToFile(handle, rawData->data(), rawData->size());
    unlockAndCloseFile(handle);

    if (writtenBytes != static_cast<int64_t>(rawData->size()))
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "WebResourceLoadStatisticsStore: We only wrote %d out of %zu bytes to disk", static_cast<unsigned>(writtenBytes), rawData->size());

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
            Seconds delay = minimumStatisticsFileWriteInterval - timeSinceLastWrite + 1_s;
            m_statisticsQueue->dispatchAfter(delay, [this, protectedThis = makeRef(*this)] {
                writeStoreToDisk();
            });
        }
        return;
    }

    writeStoreToDisk();
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
            clearInMemory();
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
        unlockAndCloseFile(handle);
        return nullptr;
    }
    
    size_t bytesToRead;
    if (!WTF::convertSafely(fileSize, bytesToRead)) {
        unlockAndCloseFile(handle);
        return nullptr;
    }

    Vector<char> buffer(bytesToRead);
    size_t totalBytesRead = readFromFile(handle, buffer.data(), buffer.size());

    unlockAndCloseFile(handle);

    if (totalBytesRead != bytesToRead)
        return nullptr;

    return KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    includeTodayAsOperatingDateIfNecessary();
    if (m_shouldSubmitTelemetry)
        submitTelemetry();
}

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteractionTime = WallTime::now();

        updateCookiePartitioningForDomains({ primaryDomain }, { }, ShouldClearFirst::No);
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
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

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool hadUserInteraction = mapEntry == m_resourceStatisticsMap.end() ? false: hasHadUnexpiredRecentUserInteraction(mapEntry->value);
        RunLoop::main().dispatch([hadUserInteraction, completionHandler = WTFMove(completionHandler)] {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const URL& url, Seconds seconds)
{
    if (url.isBlankURL() || url.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), seconds] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.lastSeen = WallTime::fromRawSeconds(seconds.seconds());
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.isPrevalentResource = true;
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool isPrevalentResource = mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource;
        RunLoop::main().dispatch([isPrevalentResource, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.isPrevalentResource = false;
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const URL& url, bool value)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), value] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.grandfathered = value;
    });
}

void WebResourceLoadStatisticsStore::isGrandfathered(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), primaryDomain = isolatedPrimaryDomain(url)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool isGrandFathered = mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.grandfathered;
        RunLoop::main().dispatch([isGrandFathered, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    if (subframe.isBlankURL() || subframe.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubFrameDomain = isolatedPrimaryDomain(subframe)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubFrameDomain);
        statistics.subframeUnderTopFrameOrigins.add(primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
        statistics.subresourceUnderTopFrameOrigins.add(primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
        statistics.subresourceUniqueRedirectsTo.add(primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdate()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        updateCookiePartitioning();
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst shouldClearFirst)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd), shouldClearFirst] {
        updateCookiePartitioningForDomains(domainsToRemove, domainsToAdd, shouldClearFirst);
    });
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void WebResourceLoadStatisticsStore::scheduleCookiePartitioningStateReset()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        resetCookiePartitioningState();
    });
}
#endif

void WebResourceLoadStatisticsStore::scheduleClearInMemory()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        clearInMemory();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        clearInMemory();
        deleteStoreFromDisk();
        grandfatherExistingWebsiteData();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(std::chrono::system_clock::time_point modifiedSince)
{
    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    scheduleClearInMemoryAndPersistent();
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(std::optional<Seconds> seconds)
{
    ASSERT(!seconds || seconds.value() >= 0_s);
    m_timeToLiveUserInteraction = seconds;
}

void WebResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_timeToLiveCookiePartitionFree = seconds;
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_minimumTimeBetweenDataRecordsRemoval = seconds;
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_grandfatheringTime = seconds;
}

bool WebResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());
    if (m_dataRecordsBeingRemoved)
        return false;

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_minimumTimeBetweenDataRecordsRemoval);
}

void WebResourceLoadStatisticsStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

ResourceLoadStatistics& WebResourceLoadStatisticsStore::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());
    return m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    }).iterator->value;
}

std::unique_ptr<KeyedEncoder> WebResourceLoadStatisticsStore::createEncoderFromData() const
{
    ASSERT(!RunLoop::isMain());
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("version", statisticsModelVersion);
    encoder->encodeDouble("endOfGrandfatheringTimestamp", m_endOfGrandfatheringTimestamp.secondsSinceEpoch().value());

    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [](KeyedEncoder& encoderInner, const auto& origin) {
        origin.value.encode(encoderInner);
    });

    encoder->encodeObjects("operatingDates", m_operatingDates.begin(), m_operatingDates.end(), [](KeyedEncoder& encoderInner, WallTime date) {
        encoderInner.encodeDouble("date", date.secondsSinceEpoch().value());
    });

    return encoder;
}

void WebResourceLoadStatisticsStore::populateFromDecoder(KeyedDecoder& decoder)
{
    ASSERT(!RunLoop::isMain());
    if (!m_resourceStatisticsMap.isEmpty())
        return;

    unsigned versionOnDisk;
    if (!decoder.decodeUInt32("version", versionOnDisk))
        return;

    if (versionOnDisk != statisticsModelVersion)
        return;

    double endOfGrandfatheringTimestamp;
    if (decoder.decodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp))
        m_endOfGrandfatheringTimestamp = WallTime::fromRawSeconds(endOfGrandfatheringTimestamp);
    else
        m_endOfGrandfatheringTimestamp = { };

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder.decodeObjects("browsingStatistics", loadedStatistics, [](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner);
    });

    if (!succeeded)
        return;

    Vector<String> prevalentResourceDomainsWithoutUserInteraction;
    prevalentResourceDomainsWithoutUserInteraction.reserveInitialCapacity(loadedStatistics.size());
    for (auto& statistics : loadedStatistics) {
        if (statistics.isPrevalentResource && !statistics.hadUserInteraction) {
            prevalentResourceDomainsWithoutUserInteraction.uncheckedAppend(statistics.highLevelDomain);
            statistics.isMarkedForCookiePartitioning = true;
        }
        m_resourceStatisticsMap.add(statistics.highLevelDomain, WTFMove(statistics));
    }

    succeeded = decoder.decodeObjects("operatingDates", m_operatingDates, [](KeyedDecoder& decoder, WallTime& wallTime) {
        double value;
        if (!decoder.decodeDouble("date", value))
            return false;

        wallTime = WallTime::fromRawSeconds(value);
        return true;
    });

    if (!succeeded)
        return;

    updateCookiePartitioningForDomains({ }, prevalentResourceDomainsWithoutUserInteraction, ShouldClearFirst::Yes);
}

void WebResourceLoadStatisticsStore::clearInMemory()
{
    ASSERT(!RunLoop::isMain());
    m_resourceStatisticsMap.clear();
    m_operatingDates.clear();

    updateCookiePartitioningForDomains({ }, { }, ShouldClearFirst::Yes);
}

void WebResourceLoadStatisticsStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());
    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return WTFMove(statistic);
        });
        if (!result.isNewEntry)
            result.iterator->value.merge(statistic);
    }
}

inline bool WebResourceLoadStatisticsStore::shouldPartitionCookies(const ResourceLoadStatistics& statistic) const
{
    return statistic.isPrevalentResource && (!statistic.hadUserInteraction || WallTime::now() > statistic.mostRecentUserInteractionTime + m_timeToLiveCookiePartitionFree);
}

void WebResourceLoadStatisticsStore::updateCookiePartitioning()
{
    ASSERT(!RunLoop::isMain());

    Vector<String> domainsToRemove;
    Vector<String> domainsToAdd;
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        bool shouldPartition = shouldPartitionCookies(resourceStatistic);
        if (resourceStatistic.isMarkedForCookiePartitioning && !shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = false;
            domainsToRemove.append(resourceStatistic.highLevelDomain);
        } else if (!resourceStatistic.isMarkedForCookiePartitioning && shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = true;
            domainsToAdd.append(resourceStatistic.highLevelDomain);
        }
    }

    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        m_updateCookiePartitioningForDomainsHandler(domainsToRemove, domainsToAdd, ShouldClearFirst::No);
    });
}

void WebResourceLoadStatisticsStore::updateCookiePartitioningForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst shouldClearFirst)
{
    ASSERT(!RunLoop::isMain());
    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;

    RunLoop::main().dispatch([this, shouldClearFirst, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        m_updateCookiePartitioningForDomainsHandler(domainsToRemove, domainsToAdd, shouldClearFirst);
    });

    if (shouldClearFirst == ShouldClearFirst::Yes)
        resetCookiePartitioningState();
    else {
        for (auto& domain : domainsToRemove)
            ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = false;
    }

    for (auto& domain : domainsToAdd)
        ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = true;
}

void WebResourceLoadStatisticsStore::resetCookiePartitioningState()
{
    ASSERT(!RunLoop::isMain());
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        resourceStatistic.isMarkedForCookiePartitioning = false;
}

void WebResourceLoadStatisticsStore::processStatistics(const WTF::Function<void (const ResourceLoadStatistics&)>& processFunction) const
{
    ASSERT(!RunLoop::isMain());
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool WebResourceLoadStatisticsStore::hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics& resourceStatistic) const
{
    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteractionTime = { };
        resourceStatistic.hadUserInteraction = false;
    }

    return resourceStatistic.hadUserInteraction;
}

Vector<String> WebResourceLoadStatisticsStore::topPrivatelyControlledDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = m_endOfGrandfatheringTimestamp > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && m_endOfGrandfatheringTimestamp;

    if (shouldClearGrandfathering)
        m_endOfGrandfatheringTimestamp = { };

    Vector<String> prevalentResources;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (statistic.isPrevalentResource && !hasHadUnexpiredRecentUserInteraction(statistic) && (!shouldCheckForGrandfathering || !statistic.grandfathered))
            prevalentResources.append(statistic.highLevelDomain);

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    return prevalentResources;
}

void WebResourceLoadStatisticsStore::includeTodayAsOperatingDateIfNecessary()
{
    if (!m_operatingDates.isEmpty() && (WallTime::now() - m_operatingDates.last() < 24_h))
        return;

    while (m_operatingDates.size() >= operatingDatesWindow)
        m_operatingDates.removeFirst();

    m_operatingDates.append(WallTime::now());
}

bool WebResourceLoadStatisticsStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic) const
{
    if (m_operatingDates.size() >= operatingDatesWindow) {
        if (resourceStatistic.mostRecentUserInteractionTime < m_operatingDates.first())
            return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user
    // setting for a tighter restriction (mainly for testing).
    if (m_timeToLiveUserInteraction) {
        if (WallTime::now() > resourceStatistic.mostRecentUserInteractionTime + m_timeToLiveUserInteraction.value())
            return true;
    }

    return false;
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    m_maxStatisticsEntries = maximumEntryCount;
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    m_pruneEntriesDownTo = pruneTargetCount;
}
    
struct StatisticsLastSeen {
    String topPrivatelyOwnedDomain;
    WallTime lastSeen;
};
    
static void pruneResources(HashMap<String, WebCore::ResourceLoadStatistics>& statisticsMap, Vector<StatisticsLastSeen>& statisticsToPrune, size_t& numberOfEntriesToPrune)
{
    if (statisticsToPrune.size() > numberOfEntriesToPrune) {
        std::sort(statisticsToPrune.begin(), statisticsToPrune.end(), [](const StatisticsLastSeen& a, const StatisticsLastSeen& b) {
            return a.lastSeen < b.lastSeen;
        });
    }

    for (size_t i = 0, end = std::min(numberOfEntriesToPrune, statisticsToPrune.size()); i != end; ++i, --numberOfEntriesToPrune)
        statisticsMap.remove(statisticsToPrune[i].topPrivatelyOwnedDomain);
}
    
static unsigned computeImportance(const ResourceLoadStatistics& resourceStatistic)
{
    unsigned importance = maxImportance;
    if (!resourceStatistic.isPrevalentResource)
        importance -= 1;
    if (!resourceStatistic.hadUserInteraction)
        importance -= 2;
    return importance;
}
    
void WebResourceLoadStatisticsStore::pruneStatisticsIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    if (m_resourceStatisticsMap.size() <= m_maxStatisticsEntries)
        return;

    ASSERT(m_pruneEntriesDownTo <= m_maxStatisticsEntries);

    size_t numberOfEntriesLeftToPrune = m_resourceStatisticsMap.size() - m_pruneEntriesDownTo;
    ASSERT(numberOfEntriesLeftToPrune);
    
    Vector<StatisticsLastSeen> resourcesToPrunePerImportance[maxImportance + 1];
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        resourcesToPrunePerImportance[computeImportance(resourceStatistic)].append({ resourceStatistic.highLevelDomain, resourceStatistic.lastSeen });
    
    for (unsigned importance = 0; numberOfEntriesLeftToPrune && importance <= maxImportance; ++importance)
        pruneResources(m_resourceStatisticsMap, resourcesToPrunePerImportance[importance], numberOfEntriesLeftToPrune);

    ASSERT(!numberOfEntriesLeftToPrune);
}
    
} // namespace WebKit
