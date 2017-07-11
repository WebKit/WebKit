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

#pragma once

#include "Connection.h"
#include "ResourceLoadStatisticsClassifier.h"
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if HAVE(CORE_PREDICTION)
#include "ResourceLoadStatisticsClassifierCocoa.h"
#endif

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class FileMonitor;
class KeyedDecoder;
class KeyedEncoder;
class URL;
struct ResourceLoadStatistics;
}

namespace WebKit {

class WebProcessProxy;

enum class ShouldClearFirst;

// FIXME: We should consider moving FileSystem I/O to a separate class.
class WebResourceLoadStatisticsStore final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    using UpdateCookiePartitioningForDomainsHandler = WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst)>;
    static Ref<WebResourceLoadStatisticsStore> create(const String& resourceLoadStatisticsDirectory, UpdateCookiePartitioningForDomainsHandler&& updateCookiePartitioningForDomainsHandler = { })
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory, WTFMove(updateCookiePartitioningForDomainsHandler)));
    }

    ~WebResourceLoadStatisticsStore();

    void setNotifyPagesWhenDataRecordsWereScanned(bool value) { m_shouldNotifyPagesWhenDataRecordsWereScanned = value; }
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value) { m_shouldClassifyResourcesBeforeDataRecordsRemoval = value; }
    void setShouldSubmitTelemetry(bool value) { m_shouldSubmitTelemetry = value; }

    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void logUserInteraction(const WebCore::URL&);
    void clearUserInteraction(const WebCore::URL&);
    void hasHadUserInteraction(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void setLastSeen(const WebCore::URL&, Seconds);
    void setPrevalentResource(const WebCore::URL&);
    void isPrevalentResource(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void clearPrevalentResource(const WebCore::URL&);
    void setGrandfathered(const WebCore::URL&, bool);
    void isGrandfathered(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void setSubframeUnderTopFrameOrigin(const WebCore::URL& subframe, const WebCore::URL& topFrame);
    void setSubresourceUnderTopFrameOrigin(const WebCore::URL& subresource, const WebCore::URL& topFrame);
    void setSubresourceUniqueRedirectTo(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedTo);
    void scheduleCookiePartitioningUpdate();
    void scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst);
    void processStatisticsAndDataRecords();
    void submitTelemetry();

    void scheduleClearInMemory();
    void scheduleClearInMemoryAndPersistent();
    void scheduleClearInMemoryAndPersistent(std::chrono::system_clock::time_point modifiedSince);

    void setTimeToLiveUserInteraction(std::optional<Seconds>);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setMaxStatisticsEntries(size_t);
    void setPruneEntriesDownTo(size_t);
    
    void processStatistics(const WTF::Function<void (const WebCore::ResourceLoadStatistics&)>&) const;
    void pruneStatisticsIfNeeded();
    
private:
    WebResourceLoadStatisticsStore(const String&, UpdateCookiePartitioningForDomainsHandler&&);

    void readDataFromDiskIfNeeded();

    void removeDataRecords();
    void startMonitoringStatisticsStorage();
    void stopMonitoringStatisticsStorage();

    String statisticsStoragePath() const;
    String resourceLogFilePath() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void grandfatherExistingWebsiteData();

    void writeStoreToDisk();
    void scheduleOrWriteStoreToDisk();
    std::unique_ptr<WebCore::KeyedDecoder> createDecoderFromDisk(const String& path) const;
    WallTime statisticsFileModificationTime(const String& label) const;
    void platformExcludeFromBackup() const;
    void deleteStoreFromDisk();
    void syncWithExistingStatisticsStorageIfNeeded();
    void refreshFromDisk();
    bool hasStatisticsFileChangedSinceLastSync(const String& path) const;
    void performDailyTasks();
    bool shouldRemoveDataRecords() const;
    void setDataRecordsBeingRemoved(bool);

    bool shouldPartitionCookies(const WebCore::ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(const WebCore::ResourceLoadStatistics&) const;
    bool hasHadUnexpiredRecentUserInteraction(WebCore::ResourceLoadStatistics&) const;
    void includeTodayAsOperatingDateIfNecessary();
    Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    void updateCookiePartitioning();
    void updateCookiePartitioningForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst);
    void mergeStatistics(Vector<WebCore::ResourceLoadStatistics>&&);
    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);
    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void populateFromDecoder(WebCore::KeyedDecoder&);
    void clearInMemory();

#if PLATFORM(COCOA)
    void registerUserDefaultsIfNeeded();
#endif

    HashMap<String, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa m_resourceLoadStatisticsClassifier;
#else
    ResourceLoadStatisticsClassifier m_resourceLoadStatisticsClassifier;
#endif
    Ref<WTF::WorkQueue> m_statisticsQueue;
    std::unique_ptr<WebCore::FileMonitor> m_statisticsStorageMonitor;
    Deque<WTF::WallTime> m_operatingDates;

    UpdateCookiePartitioningForDomainsHandler m_updateCookiePartitioningForDomainsHandler;

    WallTime m_endOfGrandfatheringTimestamp;
    const String m_statisticsStoragePath;
    WallTime m_lastStatisticsFileSyncTime;
    MonotonicTime m_lastStatisticsWriteTime;
    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;
    MonotonicTime m_lastTimeDataRecordsWereRemoved;
    Seconds m_minimumTimeBetweenDataRecordsRemoval { 1_h };
    std::optional<Seconds> m_timeToLiveUserInteraction;
    Seconds m_timeToLiveCookiePartitionFree { 24_h };
    Seconds m_grandfatheringTime { 1_h };
    size_t m_maxStatisticsEntries { 1000 };
    size_t m_pruneEntriesDownTo { 800 };
    bool m_dataRecordsBeingRemoved { false };
    bool m_didScheduleWrite { false };
    bool m_shouldNotifyPagesWhenDataRecordsWereScanned { false };
    bool m_shouldClassifyResourcesBeforeDataRecordsRemoval { true };
    bool m_shouldSubmitTelemetry { true };
};

} // namespace WebKit
