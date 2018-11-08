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

#pragma once

#include "ResourceLoadStatisticsClassifier.h"
#include "WebResourceLoadStatisticsStore.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

#if HAVE(CORE_PREDICTION)
#include "ResourceLoadStatisticsClassifierCocoa.h"
#endif

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
struct ResourceLoadStatistics;
}

namespace WebKit {

class OperatingDate;
class ResourceLoadStatisticsPersistentStorage;

// This is always constructed / used / destroyed on the WebResourceLoadStatisticsStore's statistics queue.
class ResourceLoadStatisticsMemoryStore : public CanMakeWeakPtr<ResourceLoadStatisticsMemoryStore> {
public:
    ResourceLoadStatisticsMemoryStore(WebResourceLoadStatisticsStore&, WorkQueue&);
    ~ResourceLoadStatisticsMemoryStore();

    void setPersistentStorage(ResourceLoadStatisticsPersistentStorage&);

    void clear(CompletionHandler<void()>&&);
    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void mergeWithDataFromDecoder(WebCore::KeyedDecoder&);

    void mergeStatistics(Vector<WebCore::ResourceLoadStatistics>&&);
    void processStatistics(const Function<void(const WebCore::ResourceLoadStatistics&)>&) const;

    void updateCookieBlocking(CompletionHandler<void()>&&);
    void updateCookieBlockingForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void clearBlockingStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);

    void includeTodayAsOperatingDateIfNecessary();
    void processStatisticsAndDataRecords();

    void requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain);
    void removeAllStorageAccess(CompletionHandler<void()>&&);

    void grandfatherExistingWebsiteData(CompletionHandler<void()>&&);
    void cancelPendingStatisticsProcessingRequest();

    bool isRegisteredAsSubresourceUnder(const String& subresourcePrimaryDomain, const String& topFramePrimaryDomain) const;
    bool isRegisteredAsSubFrameUnder(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain) const;
    bool isRegisteredAsRedirectingTo(const String& hostRedirectedFromPrimaryDomain, const String& hostRedirectedToPrimaryDomain) const;

    void clearPrevalentResource(const String& primaryDomain);
    String dumpResourceLoadStatistics() const;
    bool isPrevalentResource(const String& primaryDomain) const;
    bool isVeryPrevalentResource(const String& primaryDomain) const;
    void setPrevalentResource(const String& primaryDomain);
    void setVeryPrevalentResource(const String& primaryDomain);

    void setGrandfathered(const String& primaryDomain, bool value);
    bool isGrandfathered(const String& primaryDomain) const;

    void setSubframeUnderTopFrameOrigin(const String& primarySubFrameDomain, const String& primaryTopFrameDomain);
    void setSubresourceUnderTopFrameOrigin(const String& primarySubresourceDomain, const String& primaryTopFrameDomain);
    void setSubresourceUniqueRedirectTo(const String& primarySubresourceDomain, const String& primaryRedirectDomain);
    void setSubresourceUniqueRedirectFrom(const String& primarySubresourceDomain, const String& primaryRedirectDomain);
    void setTopFrameUniqueRedirectTo(const String& topFramePrimaryDomain, const String& primaryRedirectDomain);
    void setTopFrameUniqueRedirectFrom(const String& topFramePrimaryDomain, const String& primaryRedirectDomain);

    void logTestingEvent(const String&);

    void setMaxStatisticsEntries(size_t maximumEntryCount);
    void setPruneEntriesDownTo(size_t pruneTargetCount);
    void resetParametersToDefaultValues();

    void calculateAndSubmitTelemetry() const;

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setShouldSubmitTelemetry(bool);
    void setTimeToLiveUserInteraction(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setCacheMaxAgeCap(Seconds);
    void updateCacheMaxAgeCap();
    void setResourceLoadStatisticsDebugMode(bool);
    bool isDebugModeEnabled() const { return m_debugModeEnabled; };
    void setPrevalentResourceForDebugMode(const String& domain);

    void hasStorageAccess(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(String&& subFramePrimaryDomain, String&& topFramePrimaryDomain, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);

    void logFrameNavigation(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, const String& sourcePrimaryDomain, const String& targetHost, const String& mainFrameHost, bool isRedirect, bool isMainFrame);
    void logUserInteraction(const String& primaryDomain);

    void clearUserInteraction(const String& primaryDomain);
    bool hasHadUserInteraction(const String& primaryDomain);

    void setLastSeen(const String& primaryDomain, Seconds);

private:
    static bool shouldBlockAndKeepCookies(const WebCore::ResourceLoadStatistics&);
    static bool shouldBlockAndPurgeCookies(const WebCore::ResourceLoadStatistics&);
    static bool hasUserGrantedStorageAccessThroughPrompt(const WebCore::ResourceLoadStatistics&, const String& firstPartyPrimaryDomain);
    bool hasHadUnexpiredRecentUserInteraction(WebCore::ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(const WebCore::ResourceLoadStatistics&) const;
    bool wasAccessedAsFirstPartyDueToUserInteraction(const WebCore::ResourceLoadStatistics& current, const WebCore::ResourceLoadStatistics& updated) const;
    void setPrevalentResource(WebCore::ResourceLoadStatistics&, ResourceLoadPrevalence);
    unsigned recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const WebCore::ResourceLoadStatistics&, HashSet<String>& domainsThatHaveRedirectedTo, unsigned numberOfRecursiveCalls) const;
    void setStorageAccessPromptsEnabled(bool enabled) { m_storageAccessPromptsEnabled  = enabled; }
    bool shouldRemoveDataRecords() const;
    void setDebugLogggingEnabled(bool enabled) { m_debugLoggingEnabled  = enabled; }
    void setDataRecordsBeingRemoved(bool);
    void scheduleStatisticsProcessingRequestIfNecessary();
    void grantStorageAccessInternal(String&& subFrameHost, String&& topFrameHost, std::optional<uint64_t> frameID, uint64_t pageID, bool userWasPromptedNowOrEarlier, CompletionHandler<void(bool)>&&);
    void markAsPrevalentIfHasRedirectedToPrevalent(WebCore::ResourceLoadStatistics&);
    bool isPrevalentDueToDebugMode(WebCore::ResourceLoadStatistics&);
    Vector<String> ensurePrevalentResourcesForDebugMode();
    void removeDataRecords(CompletionHandler<void()>&&);
    void pruneStatisticsIfNeeded();
    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);
    Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();

#if PLATFORM(COCOA)
    void registerUserDefaultsIfNeeded();
#endif

    struct Parameters {
        size_t pruneEntriesDownTo { 800 };
        size_t maxStatisticsEntries { 1000 };
        std::optional<Seconds> timeToLiveUserInteraction;
        Seconds minimumTimeBetweenDataRecordsRemoval { 1_h };
        Seconds grandfatheringTime { 24_h * 7 };
        Seconds cacheMaxAgeCapTime { 24_h * 7 };
        bool shouldNotifyPagesWhenDataRecordsWereScanned { false };
        bool shouldClassifyResourcesBeforeDataRecordsRemoval { true };
        bool shouldSubmitTelemetry { true };
    };

    WebResourceLoadStatisticsStore& m_store;
    Ref<WorkQueue> m_workQueue;
    WeakPtr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;
    HashMap<String, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa m_resourceLoadStatisticsClassifier;
#else
    ResourceLoadStatisticsClassifier m_resourceLoadStatisticsClassifier;
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
    HashSet<uint64_t> m_activePluginTokens;
#endif
    Parameters m_parameters;
    Vector<OperatingDate> m_operatingDates;
    WallTime m_endOfGrandfatheringTimestamp;
    bool m_debugLoggingEnabled { false };
    bool m_debugModeEnabled { false };
    String m_debugManualPrevalentResource;
    bool m_storageAccessPromptsEnabled { false };
    bool m_dataRecordsBeingRemoved { false };
    MonotonicTime m_lastTimeDataRecordsWereRemoved;

    uint64_t m_lastStatisticsProcessingRequestIdentifier { 0 };
    std::optional<uint64_t> m_pendingStatisticsProcessingRequestIdentifier;
};

} // namespace WebKit
