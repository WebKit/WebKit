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

#if ENABLE(RESOURCE_LOAD_STATISTICS)

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
    using ResourceLoadStatistics = WebCore::ResourceLoadStatistics;
    using RegistrableDomain = WebCore::RegistrableDomain;
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;
    using RedirectDomain = WebCore::RegistrableDomain;
    using RedirectedFromDomain = WebCore::RegistrableDomain;
    using RedirectedToDomain = WebCore::RegistrableDomain;
    using NavigatedFromDomain = WebCore::RegistrableDomain;
    using NavigatedToDomain = WebCore::RegistrableDomain;
    using DomainInNeedOfStorageAccess = WebCore::RegistrableDomain;
    using OpenerDomain = WebCore::RegistrableDomain;
    using OpenerPageID = uint64_t;
    using PageID = uint64_t;
    using FrameID = uint64_t;

    ResourceLoadStatisticsMemoryStore(WebResourceLoadStatisticsStore&, WorkQueue&);
    ~ResourceLoadStatisticsMemoryStore();

    void setPersistentStorage(ResourceLoadStatisticsPersistentStorage&);

    void clear(CompletionHandler<void()>&&);
    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void mergeWithDataFromDecoder(WebCore::KeyedDecoder&);

    void mergeStatistics(Vector<ResourceLoadStatistics>&&);
    void processStatistics(const Function<void(const ResourceLoadStatistics&)>&) const;

    void updateCookieBlocking(CompletionHandler<void()>&&);
    void updateCookieBlockingForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void clearBlockingStateForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);

    void includeTodayAsOperatingDateIfNecessary();
    void processStatisticsAndDataRecords();

    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, OpenerPageID, OpenerDomain&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);

    void grandfatherExistingWebsiteData(CompletionHandler<void()>&&);
    void cancelPendingStatisticsProcessingRequest();

    bool isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&) const;
    bool isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&) const;
    bool isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&) const;

    void clearPrevalentResource(const RegistrableDomain&);
    String dumpResourceLoadStatistics() const;
    bool isPrevalentResource(const RegistrableDomain&) const;
    bool isVeryPrevalentResource(const RegistrableDomain&) const;
    void setPrevalentResource(const RegistrableDomain&);
    void setVeryPrevalentResource(const RegistrableDomain&);

    void setGrandfathered(const RegistrableDomain&, bool value);
    bool isGrandfathered(const RegistrableDomain&) const;

    void setSubframeUnderTopFrameOrigin(const SubFrameDomain&, const TopFrameDomain&);
    void setSubresourceUnderTopFrameOrigin(const SubResourceDomain&, const TopFrameDomain&);
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectDomain&);
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectDomain&);
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectDomain&);
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectDomain&);

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
    void setResourceLoadStatisticsDebugMode(bool);
    bool isDebugModeEnabled() const { return m_debugModeEnabled; };
    void setPrevalentResourceForDebugMode(const RegistrableDomain&);

    void hasStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);

    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame);
    void logUserInteraction(const TopFrameDomain&);
    void logSubresourceLoading(const SubResourceDomain&, const TopFrameDomain&, WallTime lastSeen);
    void logSubresourceRedirect(const RedirectedFromDomain&, const RedirectedToDomain&);

    void clearUserInteraction(const RegistrableDomain&);
    bool hasHadUserInteraction(const RegistrableDomain&);

    void setLastSeen(const RegistrableDomain&, Seconds);

    void didCreateNetworkProcess();

    const WebResourceLoadStatisticsStore& store() const { return m_store; }

private:
    static bool shouldBlockAndKeepCookies(const ResourceLoadStatistics&);
    static bool shouldBlockAndPurgeCookies(const ResourceLoadStatistics&);
    static bool hasUserGrantedStorageAccessThroughPrompt(const ResourceLoadStatistics&, const RegistrableDomain&);
    bool hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(const ResourceLoadStatistics&) const;
    bool wasAccessedAsFirstPartyDueToUserInteraction(const ResourceLoadStatistics& current, const ResourceLoadStatistics& updated) const;
    void setPrevalentResource(ResourceLoadStatistics&, ResourceLoadPrevalence);
    unsigned recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const ResourceLoadStatistics&, HashSet<RedirectedToDomain>&, unsigned numberOfRecursiveCalls) const;
    void setStorageAccessPromptsEnabled(bool enabled) { m_storageAccessPromptsEnabled  = enabled; }
    bool shouldRemoveDataRecords() const;
    void setDebugLogggingEnabled(bool enabled) { m_debugLoggingEnabled  = enabled; }
    void setDataRecordsBeingRemoved(bool);
    void scheduleStatisticsProcessingRequestIfNecessary();
    void grantStorageAccessInternal(SubFrameDomain&&, TopFrameDomain&&, Optional<FrameID>, PageID, bool userWasPromptedNowOrEarlier, CompletionHandler<void(bool)>&&);
    void markAsPrevalentIfHasRedirectedToPrevalent(ResourceLoadStatistics&);
    bool isPrevalentDueToDebugMode(ResourceLoadStatistics&);
    Vector<RegistrableDomain> ensurePrevalentResourcesForDebugMode();
    void removeDataRecords(CompletionHandler<void()>&&);
    void pruneStatisticsIfNeeded();
    ResourceLoadStatistics& ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain&);
    Vector<RegistrableDomain> registrableDomainsToRemoveWebsiteDataFor();
    void setCacheMaxAgeCap(Seconds);
    void updateCacheMaxAgeCap();
    void setAgeCapForClientSideCookies(Seconds);
    void updateClientSideCookiesAgeCap();

#if PLATFORM(COCOA)
    void registerUserDefaultsIfNeeded();
#endif

    struct Parameters {
        size_t pruneEntriesDownTo { 800 };
        size_t maxStatisticsEntries { 1000 };
        Optional<Seconds> timeToLiveUserInteraction;
        Seconds minimumTimeBetweenDataRecordsRemoval { 1_h };
        Seconds grandfatheringTime { 24_h * 7 };
        Seconds cacheMaxAgeCapTime { 24_h * 7 };
        Seconds clientSideCookiesAgeCapTime { 24_h * 7 };
        bool shouldNotifyPagesWhenDataRecordsWereScanned { false };
        bool shouldClassifyResourcesBeforeDataRecordsRemoval { true };
        bool shouldSubmitTelemetry { true };
    };

    WebResourceLoadStatisticsStore& m_store;
    Ref<WorkQueue> m_workQueue;
    WeakPtr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;
    HashMap<RegistrableDomain, ResourceLoadStatistics> m_resourceStatisticsMap;
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
    const RegistrableDomain m_debugStaticPrevalentResource { "3rdpartytestwebkit.org"_s };
    RegistrableDomain m_debugManualPrevalentResource;
    bool m_storageAccessPromptsEnabled { false };
    bool m_dataRecordsBeingRemoved { false };
    MonotonicTime m_lastTimeDataRecordsWereRemoved;

    uint64_t m_lastStatisticsProcessingRequestIdentifier { 0 };
    Optional<uint64_t> m_pendingStatisticsProcessingRequestIdentifier;
};

} // namespace WebKit

#endif
