/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "WebsiteDataType.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
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
class KeyedDecoder;
class KeyedEncoder;
class URL;
struct ResourceLoadStatistics;
}

namespace WebKit {

class OperatingDate;
class WebProcessProxy;

enum class ShouldClearFirst;

class WebResourceLoadStatisticsStore final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    using UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler = WTF::Function<void(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst)>;
    using HasStorageAccessForFrameHandler = WTF::Function<void(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::Function<void(bool hasAccess)>&& callback)>;
    using GrantStorageAccessHandler = WTF::Function<void(const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID, WTF::Function<void(bool wasGranted)>&& callback)>;
    using RemoveAllStorageAccessHandler = WTF::Function<void()>;
    using RemovePrevalentDomainsHandler = WTF::Function<void (const Vector<String>&)>;
    static Ref<WebResourceLoadStatisticsStore> create(const String& resourceLoadStatisticsDirectory, Function<void (const String&)>&& testingCallback, bool isEphemeral, UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler&& updatePrevalentDomainsToPartitionOrBlockCookiesHandler = [](const WTF::Vector<String>&, const WTF::Vector<String>&, const WTF::Vector<String>&, ShouldClearFirst) { }, HasStorageAccessForFrameHandler&& hasStorageAccessForFrameHandler = [](const String&, const String&, uint64_t, uint64_t, WTF::Function<void(bool)>&&) { }, GrantStorageAccessHandler&& grantStorageAccessHandler = [](const String&, const String&, std::optional<uint64_t>, uint64_t, WTF::Function<void(bool)>&&) { }, RemoveAllStorageAccessHandler&& removeAllStorageAccessHandler = []() { }, RemovePrevalentDomainsHandler&& removeDomainsHandler = [] (const WTF::Vector<String>&) { })
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory, WTFMove(testingCallback), isEphemeral, WTFMove(updatePrevalentDomainsToPartitionOrBlockCookiesHandler), WTFMove(hasStorageAccessForFrameHandler), WTFMove(grantStorageAccessHandler), WTFMove(removeAllStorageAccessHandler), WTFMove(removeDomainsHandler)));
    }

    ~WebResourceLoadStatisticsStore();

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }
    WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void setNotifyPagesWhenDataRecordsWereScanned(bool value) { m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned = value; }
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value) { m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval = value; }
    void setShouldSubmitTelemetry(bool value) { m_parameters.shouldSubmitTelemetry = value; }

    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins);

    void hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void (bool)>&& callback);
    void requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void (bool)>&& callback);
    void requestStorageAccessUnderOpener(String&& domainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerDomain, bool isTriggeredByUserGesture);
    void requestStorageAccessCallback(bool wasGranted, uint64_t contextId);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void logUserInteraction(const WebCore::URL&);
    void logNonRecentUserInteraction(const WebCore::URL&);
    void clearUserInteraction(const WebCore::URL&);
    void hasHadUserInteraction(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void setLastSeen(const WebCore::URL&, Seconds);
    void setPrevalentResource(const WebCore::URL&);
    void setVeryPrevalentResource(const WebCore::URL&);
    void isPrevalentResource(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void isVeryPrevalentResource(const WebCore::URL&, WTF::Function<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const WebCore::URL& subFrame, const WebCore::URL& topFrame, WTF::Function<void (bool)>&&);
    void isRegisteredAsRedirectingTo(const WebCore::URL& hostRedirectedFrom, const WebCore::URL& hostRedirectedTo, WTF::Function<void (bool)>&&);
    void clearPrevalentResource(const WebCore::URL&);
    void setGrandfathered(const WebCore::URL&, bool);
    void isGrandfathered(const WebCore::URL&, WTF::Function<void (bool)>&&);
    void setSubframeUnderTopFrameOrigin(const WebCore::URL& subframe, const WebCore::URL& topFrame);
    void setSubresourceUnderTopFrameOrigin(const WebCore::URL& subresource, const WebCore::URL& topFrame);
    void setSubresourceUniqueRedirectTo(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedTo);
    void setSubresourceUniqueRedirectFrom(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedFrom);
    void setTopFrameUniqueRedirectTo(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedTo);
    void setTopFrameUniqueRedirectFrom(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedFrom);
    void scheduleCookiePartitioningUpdate(CompletionHandler<void()>&&);
    void scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst, CompletionHandler<void()>&&);
    void scheduleClearPartitioningStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing();
    void submitTelemetry();
    void scheduleCookiePartitioningStateReset();

    void scheduleClearInMemory();
    
    enum class ShouldGrandfather {
        No,
        Yes,
    };
    void scheduleClearInMemoryAndPersistent(ShouldGrandfather, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfather, CompletionHandler<void()>&&);

    void setTimeToLiveUserInteraction(Seconds);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setMaxStatisticsEntries(size_t);
    void setPruneEntriesDownTo(size_t);
    
    void processStatistics(const WTF::Function<void (const WebCore::ResourceLoadStatistics&)>&) const;
    void pruneStatisticsIfNeeded();

    void resetParametersToDefaultValues();

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void mergeWithDataFromDecoder(WebCore::KeyedDecoder&);
    void clearInMemory();
    void grandfatherExistingWebsiteData(CompletionHandler<void()>&&);

    void setResourceLoadStatisticsDebugMode(bool);

    void setStatisticsTestingCallback(Function<void (const String&)>&& callback) { m_statisticsTestingCallback = WTFMove(callback); }
    void logTestingEvent(const String&);

private:
    WebResourceLoadStatisticsStore(const String&, Function<void(const String&)>&& testingCallback, bool isEphemeral, UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler&&, HasStorageAccessForFrameHandler&&, GrantStorageAccessHandler&&, RemoveAllStorageAccessHandler&&, RemovePrevalentDomainsHandler&&);

    void removeDataRecords(CompletionHandler<void()>&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void performDailyTasks();
    bool shouldRemoveDataRecords() const;
    void setDataRecordsBeingRemoved(bool);

    bool shouldPartitionCookies(const WebCore::ResourceLoadStatistics&) const;
    bool shouldBlockCookies(const WebCore::ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(const WebCore::ResourceLoadStatistics&) const;
    bool hasHadUnexpiredRecentUserInteraction(WebCore::ResourceLoadStatistics&) const;
    void includeTodayAsOperatingDateIfNecessary();
    Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    void updateCookiePartitioning(CompletionHandler<void()>&&);
    void updateCookiePartitioningForDomains(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst, CompletionHandler<void()>&&);
    void clearPartitioningStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);
    void mergeStatistics(Vector<WebCore::ResourceLoadStatistics>&&);
    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);
    unsigned recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const WebCore::ResourceLoadStatistics&, HashSet<String>& domainsThatHaveRedirectedTo, unsigned numberOfRecursiveCalls);
    void setPrevalentResource(WebCore::ResourceLoadStatistics&, ResourceLoadPrevalence);
    void processStatisticsAndDataRecords();

    void resetCookiePartitioningState();
    void removeAllStorageAccess();

    void setDebugLogggingEnabled(bool enabled) { m_debugLoggingEnabled  = enabled; }

#if PLATFORM(COCOA)
    void registerUserDefaultsIfNeeded();
#endif

    bool wasAccessedAsFirstPartyDueToUserInteraction(const WebCore::ResourceLoadStatistics& current, const WebCore::ResourceLoadStatistics& updated);

    struct Parameters {
        size_t pruneEntriesDownTo { 800 };
        size_t maxStatisticsEntries { 1000 };
        std::optional<Seconds> timeToLiveUserInteraction;
        Seconds timeToLiveCookiePartitionFree { 24_h };
        Seconds minimumTimeBetweenDataRecordsRemoval { 1_h };
        Seconds grandfatheringTime { 24_h * 7 };
        bool shouldNotifyPagesWhenDataRecordsWereScanned { false };
        bool shouldClassifyResourcesBeforeDataRecordsRemoval { true };
        bool shouldSubmitTelemetry { true };
    };

    HashMap<String, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa m_resourceLoadStatisticsClassifier;
#else
    ResourceLoadStatisticsClassifier m_resourceLoadStatisticsClassifier;
#endif
    Ref<WTF::WorkQueue> m_statisticsQueue;
    ResourceLoadStatisticsPersistentStorage m_persistentStorage;
    Vector<OperatingDate> m_operatingDates;

    UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler m_updatePrevalentDomainsToPartitionOrBlockCookiesHandler;
    HasStorageAccessForFrameHandler m_hasStorageAccessForFrameHandler;
    GrantStorageAccessHandler m_grantStorageAccessHandler;
    RemoveAllStorageAccessHandler m_removeAllStorageAccessHandler;
    RemovePrevalentDomainsHandler m_removeDomainsHandler;

    WallTime m_endOfGrandfatheringTimestamp;
    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;
    MonotonicTime m_lastTimeDataRecordsWereRemoved;

    Parameters m_parameters;

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashSet<uint64_t> m_activePluginTokens;
#endif
    bool m_dataRecordsBeingRemoved { false };

    bool m_debugModeEnabled { false };
    bool m_debugLoggingEnabled { false };

    Function<void (const String&)> m_statisticsTestingCallback;
};

} // namespace WebKit
