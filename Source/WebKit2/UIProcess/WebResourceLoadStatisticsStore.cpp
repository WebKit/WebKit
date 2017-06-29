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
#include "WebResourceLoadStatisticsManager.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataType.h"
#include <WebCore/FileMonitor.h>
#include <WebCore/FileSystem.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#define ENABLE_MULTIPROCESS_ACCESS_TO_STATISTICS_STORE 1
#endif

using namespace WebCore;

namespace WebKit {

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

Ref<WebResourceLoadStatisticsStore> WebResourceLoadStatisticsStore::create(const String& resourceLoadStatisticsDirectory)
{
    return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory));
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory)
    : m_resourceLoadStatisticsStore(ResourceLoadStatisticsStore::create())
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue"))
    , m_statisticsStoragePath(resourceLoadStatisticsDirectory)
    , m_telemetryOneShotTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::telemetryTimerFired)
    , m_telemetryRepeatedTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::telemetryTimerFired)
{
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
        auto locker = holdLock(coreStore().statisticsLock());
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

        stopMonitoringStatisticsStorage();

        writeStoreToDisk();

        startMonitoringStatisticsStorage();
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

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    ASSERT(RunLoop::isMain());

    if (enabled == m_resourceLoadStatisticsEnabled)
        return;

    m_resourceLoadStatisticsEnabled = enabled;

    if (m_resourceLoadStatisticsEnabled) {
        readDataFromDiskIfNeeded();
        m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
            startMonitoringStatisticsStorage();
        });
    } else
        m_statisticsQueue->dispatch([statisticsStorageMonitor = WTFMove(m_statisticsStorageMonitor)]  { });
}

bool WebResourceLoadStatisticsStore::resourceLoadStatisticsEnabled() const
{
    return m_resourceLoadStatisticsEnabled;
}

void WebResourceLoadStatisticsStore::registerSharedResourceLoadObserver()
{
    ASSERT(RunLoop::isMain());
    
    ResourceLoadObserver::sharedObserver().setStatisticsStore(m_resourceLoadStatisticsStore.copyRef());
    ResourceLoadObserver::sharedObserver().setStatisticsQueue(m_statisticsQueue.copyRef());
    m_resourceLoadStatisticsStore->setNotificationCallback([this, protectedThis = makeRef(*this)] {
        if (m_resourceLoadStatisticsStore->isEmpty())
            return;
        processStatisticsAndDataRecords();
    });
    m_resourceLoadStatisticsStore->setWritePersistentStoreCallback([this, protectedThis = makeRef(*this)] {
        m_statisticsQueue->dispatch([this, protectedThis = protectedThis.copyRef()] {
            writeStoreToDisk();
        });
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
#if PLATFORM(COCOA)
    WebResourceLoadStatisticsManager::registerUserDefaultsIfNeeded();
#endif
}
    
void WebResourceLoadStatisticsStore::registerSharedResourceLoadObserver(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&& shouldPartitionCookiesForDomainsHandler)
{
    ASSERT(RunLoop::isMain());
    
    registerSharedResourceLoadObserver();
    m_resourceLoadStatisticsStore->setShouldPartitionCookiesCallback([shouldPartitionCookiesForDomainsHandler = WTFMove(shouldPartitionCookiesForDomainsHandler)] (const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst) {
        shouldPartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
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

void WebResourceLoadStatisticsStore::readDataFromDiskIfNeeded()
{
    if (!m_resourceLoadStatisticsEnabled)
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        auto decoder = createDecoderFromDisk(ASCIILiteral("full_browsing_session"));
        if (!decoder) {
            grandfatherExistingWebsiteData();
            return;
        }
        
        auto locker = holdLock(coreStore().statisticsLock());
        coreStore().clearInMemory();
        coreStore().readDataFromDecoder(*decoder);

        if (coreStore().isEmpty())
            grandfatherExistingWebsiteData();
    });
}
    
void WebResourceLoadStatisticsStore::refreshFromDisk()
{
    ASSERT(!RunLoop::isMain());
    auto decoder = createDecoderFromDisk(ASCIILiteral("full_browsing_session"));
    if (!decoder)
        return;

    auto locker = holdLock(coreStore().statisticsLock());
    coreStore().readDataFromDecoder(*decoder);
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
    m_statisticsQueue->dispatch([&semaphore] {
        // Make sure any ongoing work in our queue is finished before we terminate.
        semaphore.signal();
    });
    semaphore.wait(WallTime::infinity());
}

String WebResourceLoadStatisticsStore::persistentStoragePath(const String& label) const
{
    if (m_statisticsStoragePath.isEmpty())
        return emptyString();

    // TODO Decide what to call this file
    return pathByAppendingComponent(m_statisticsStoragePath, label + "_resourceLog.plist");
}

void WebResourceLoadStatisticsStore::writeStoreToDisk()
{
    ASSERT(!RunLoop::isMain());
    
    syncWithExistingStatisticsStorageIfNeeded();

    auto encoder = coreStore().createEncoderFromData();
    writeEncoderToDisk(*encoder.get(), "full_browsing_session");
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

void WebResourceLoadStatisticsStore::writeEncoderToDisk(KeyedEncoder& encoder, const String& label) const
{
    ASSERT(!RunLoop::isMain());
    
    RefPtr<SharedBuffer> rawData = encoder.finishEncoding();
    if (!rawData)
        return;

    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return;

    if (!m_statisticsStoragePath.isEmpty()) {
        makeAllDirectories(m_statisticsStoragePath);
        platformExcludeFromBackup();
    }

    auto handle = openAndLockFile(resourceLog, OpenForWrite);
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
    String resourceLogPath = persistentStoragePath(ASCIILiteral("full_browsing_session"));
    if (resourceLogPath.isEmpty())
        return;

    stopMonitoringStatisticsStorage();

    if (!deleteFile(resourceLogPath))
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to delete statistics file: %s", resourceLogPath.utf8().data());
}

void WebResourceLoadStatisticsStore::clearInMemoryData()
{
    auto locker = holdLock(coreStore().statisticsLock());
    coreStore().clearInMemory();
}

void WebResourceLoadStatisticsStore::startMonitoringStatisticsStorage()
{
    ASSERT(!RunLoop::isMain());
    if (m_statisticsStorageMonitor)
        return;
    
    String resourceLogPath = persistentStoragePath(ASCIILiteral("full_browsing_session"));
    if (resourceLogPath.isEmpty())
        return;
    
    m_statisticsStorageMonitor = FileMonitor::create(resourceLogPath, m_statisticsQueue.copyRef(), [this] (FileMonitor::FileChangeType type) {
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

    m_statisticsStorageMonitor->startMonitoring();
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
    
    refreshFromDisk();
    
    startMonitoringStatisticsStorage();
}
    
    
#if !PLATFORM(COCOA)
void WebResourceLoadStatisticsStore::platformExcludeFromBackup() const
{
    // Do nothing
}
#endif

std::unique_ptr<KeyedDecoder> WebResourceLoadStatisticsStore::createDecoderFromDisk(const String& label) const
{
    ASSERT(!RunLoop::isMain());
    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return nullptr;

    auto handle = openAndLockFile(resourceLog, OpenForRead);
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

void WebResourceLoadStatisticsStore::telemetryTimerFired()
{
    ASSERT(RunLoop::isMain());
    
    if (!shouldSubmitTelemetry)
        return;
    
    submitTelemetry();
}

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        auto locker = holdLock(coreStore().statisticsLock());
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(coreStore());
    });
}
    
} // namespace WebKit
