/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

class ResourceLoadStatisticsPersistentStorage;

class OperatingDate {
public:
    OperatingDate() = default;
    
    static OperatingDate fromWallTime(WallTime);
    static OperatingDate today();
    Seconds secondsSinceEpoch() const;
    bool operator==(const OperatingDate& other) const;
    bool operator<(const OperatingDate& other) const;
    bool operator<=(const OperatingDate& other) const;
    
private:
    OperatingDate(int year, int month, int monthDay)
        : m_year(year)
        , m_month(month)
        , m_monthDay(monthDay)
    { }

    int m_year { 0 };
    int m_month { 0 }; // [0, 11].
    int m_monthDay { 0 }; // [1, 31].
};

// This is always constructed / used / destroyed on the WebResourceLoadStatisticsStore's statistics queue.
class ResourceLoadStatisticsStore : public CanMakeWeakPtr<ResourceLoadStatisticsStore> {
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
    
    virtual ~ResourceLoadStatisticsStore();

    virtual void clear(CompletionHandler<void()>&&) = 0;
    virtual bool isEmpty() const = 0;

    virtual void updateCookieBlocking(CompletionHandler<void()>&&) = 0;
    void updateCookieBlockingForDomains(const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&&);
    void clearBlockingStateForDomains(const Vector<RegistrableDomain>& domains, CompletionHandler<void()>&&);

    void includeTodayAsOperatingDateIfNecessary();
    void processStatisticsAndDataRecords();

    virtual void classifyPrevalentResources() = 0;
    virtual void syncStorageIfNeeded() = 0;
    virtual void syncStorageImmediately() = 0;

    virtual void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, OpenerPageID, OpenerDomain&&) = 0;
    void removeAllStorageAccess(CompletionHandler<void()>&&);

    void grandfatherExistingWebsiteData(CompletionHandler<void()>&&);
    void cancelPendingStatisticsProcessingRequest();

    virtual bool isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&) const = 0;
    virtual bool isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&) const = 0;
    virtual bool isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&) const = 0;

    virtual void clearPrevalentResource(const RegistrableDomain&) = 0;
    virtual String dumpResourceLoadStatistics() const = 0;
    virtual bool isPrevalentResource(const RegistrableDomain&) const = 0;
    virtual bool isVeryPrevalentResource(const RegistrableDomain&) const = 0;
    virtual void setPrevalentResource(const RegistrableDomain&) = 0;
    virtual void setVeryPrevalentResource(const RegistrableDomain&) = 0;

    virtual void setGrandfathered(const RegistrableDomain&, bool value) = 0;
    virtual bool isGrandfathered(const RegistrableDomain&) const = 0;

    virtual void incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&&) = 0;
    virtual void grandfatherDataForDomains(const HashSet<RegistrableDomain>&) = 0;

    virtual void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&) = 0;
    virtual void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&) = 0;
    virtual void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectDomain&) = 0;
    virtual void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectDomain&) = 0;
    virtual void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectDomain&) = 0;
    virtual void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectDomain&) = 0;

    void logTestingEvent(const String&);

    void setMaxStatisticsEntries(size_t maximumEntryCount);
    void setPruneEntriesDownTo(size_t pruneTargetCount);
    void resetParametersToDefaultValues();
    Optional<Seconds> statisticsEpirationTime() const;

    virtual void calculateAndSubmitTelemetry() const = 0;

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setShouldSubmitTelemetry(bool);
    void setTimeToLiveUserInteraction(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setResourceLoadStatisticsDebugMode(bool);
    bool isDebugModeEnabled() const { return m_debugModeEnabled; };
    void setPrevalentResourceForDebugMode(const RegistrableDomain&);

    virtual void hasStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(bool)>&&) = 0;
    virtual void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&) = 0;
    virtual void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&) = 0;

    virtual void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame) = 0;
    virtual void logUserInteraction(const TopFrameDomain&) = 0;
    virtual void logSubresourceLoading(const SubResourceDomain&, const TopFrameDomain&, WallTime lastSeen) = 0;
    virtual void logSubresourceRedirect(const RedirectedFromDomain&, const RedirectedToDomain&) = 0;
    virtual void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&) = 0;

    virtual void clearUserInteraction(const RegistrableDomain&) = 0;
    virtual bool hasHadUserInteraction(const RegistrableDomain&) = 0;

    virtual void setLastSeen(const RegistrableDomain& primaryDomain, Seconds) = 0;

    void didCreateNetworkProcess();

    const WebResourceLoadStatisticsStore& store() const { return m_store; }

    static constexpr unsigned maxImportance { 3 };

    virtual bool isMemoryStore() const { return false; }
    virtual bool isDatabaseStore()const { return false; }

protected:
    static unsigned computeImportance(const WebCore::ResourceLoadStatistics&);
    static Vector<OperatingDate> mergeOperatingDates(const Vector<OperatingDate>& existingDates, Vector<OperatingDate>&& newDates);
    static void debugLogDomainsInBatches(const char* action, const Vector<RegistrableDomain>& domains);

    ResourceLoadStatisticsStore(WebResourceLoadStatisticsStore&, WorkQueue&);

    bool hasStatisticsExpired(const ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(WallTime mostRecentUserInteractionTime) const;
#if PLATFORM(COCOA)
    void registerUserDefaultsIfNeeded();
#endif
    void scheduleStatisticsProcessingRequestIfNecessary();
    void mergeOperatingDates(Vector<OperatingDate>&&);
    virtual Vector<RegistrableDomain> ensurePrevalentResourcesForDebugMode() = 0;
    virtual Vector<RegistrableDomain> registrableDomainsToRemoveWebsiteDataFor() = 0;
    virtual void pruneStatisticsIfNeeded() = 0;

    WebResourceLoadStatisticsStore& store() { return m_store; }
    Ref<WorkQueue>& workQueue() { return m_workQueue; }
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa& classifier() { return m_resourceLoadStatisticsClassifier; }
#else
    ResourceLoadStatisticsClassifier& classifier() { return m_resourceLoadStatisticsClassifier; }
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
    const Parameters& parameters() const { return m_parameters; }
    const Vector<OperatingDate>& operatingDates() const { return m_operatingDates; }
    void clearOperatingDates() { m_operatingDates.clear(); }
    WallTime& endOfGrandfatheringTimestamp() { return m_endOfGrandfatheringTimestamp; }
    const WallTime& endOfGrandfatheringTimestamp() const { return m_endOfGrandfatheringTimestamp; }
    void setEndOfGrandfatheringTimestamp(const WallTime& grandfatheringTime) { m_endOfGrandfatheringTimestamp = grandfatheringTime; }
    void clearEndOfGrandfatheringTimeStamp() { m_endOfGrandfatheringTimestamp = { }; }
    const RegistrableDomain& debugManualPrevalentResource() const { return m_debugManualPrevalentResource; }
    const RegistrableDomain& debugStaticPrevalentResource() const { return m_debugStaticPrevalentResource; }
    bool debugLoggingEnabled() const { return m_debugLoggingEnabled; };
    bool debugModeEnabled() const { return m_debugModeEnabled; }
    bool storageAccessPromptsEnabled() const { return m_storageAccessPromptsEnabled; }

    static constexpr unsigned maxNumberOfRecursiveCallsInRedirectTraceBack { 50 };

private:
    void setStorageAccessPromptsEnabled(bool enabled) { m_storageAccessPromptsEnabled  = enabled; }
    bool shouldRemoveDataRecords() const;
    void setDebugLogggingEnabled(bool enabled) { m_debugLoggingEnabled  = enabled; }
    void setDataRecordsBeingRemoved(bool);
    void removeDataRecords(CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(Seconds);
    void updateCacheMaxAgeCap();
    void setAgeCapForClientSideCookies(Seconds);
    void updateClientSideCookiesAgeCap();

    WebResourceLoadStatisticsStore& m_store;
    Ref<WorkQueue> m_workQueue;
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
    RegistrableDomain m_debugManualPrevalentResource;
    MonotonicTime m_lastTimeDataRecordsWereRemoved;
    uint64_t m_lastStatisticsProcessingRequestIdentifier { 0 };
    Optional<uint64_t> m_pendingStatisticsProcessingRequestIdentifier;
    const RegistrableDomain m_debugStaticPrevalentResource { URL { URL(), "https://3rdpartytestwebkit.org"_s } };
    bool m_debugLoggingEnabled { false };
    bool m_debugModeEnabled { false };
    bool m_storageAccessPromptsEnabled { false };
    bool m_dataRecordsBeingRemoved { false };
};

} // namespace WebKit

#endif
