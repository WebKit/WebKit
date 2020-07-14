/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include "StorageAccessStatus.h"
#include "WebPageProxyIdentifier.h"
#include "WebsiteDataType.h"
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceLoadObserver.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class ResourceRequest;
struct ResourceLoadStatistics;
enum class ShouldSample : bool;
enum class IncludeHttpOnlyCookies : bool;
enum class ThirdPartyCookieBlockingMode : uint8_t;
}

namespace WebKit {

class NetworkSession;
class ResourceLoadStatisticsStore;
class ResourceLoadStatisticsPersistentStorage;
class WebFrameProxy;
class WebProcessProxy;
enum class ShouldGrandfatherStatistics : bool;
enum class ShouldIncludeLocalhost : bool { No, Yes };
enum class EnableResourceLoadStatisticsDebugMode : bool { No, Yes };
struct RegistrableDomainsToBlockCookiesFor {
    Vector<WebCore::RegistrableDomain> domainsToBlockAndDeleteCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToBlockButKeepCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsWithUserInteractionAsFirstParty;
    RegistrableDomainsToBlockCookiesFor isolatedCopy() const { return { domainsToBlockAndDeleteCookiesFor.isolatedCopy(), domainsToBlockButKeepCookiesFor.isolatedCopy(), domainsWithUserInteractionAsFirstParty.isolatedCopy() }; }
};
struct RegistrableDomainsToDeleteOrRestrictWebsiteDataFor {
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllButHttpOnlyCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllNonCookieWebsiteDataFor;
    Vector<WebCore::RegistrableDomain> domainsToEnforceSameSiteStrictFor;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor isolatedCopy() const { return { domainsToDeleteAllCookiesFor.isolatedCopy(), domainsToDeleteAllButHttpOnlyCookiesFor.isolatedCopy(), domainsToDeleteAllNonCookieWebsiteDataFor.isolatedCopy(), domainsToEnforceSameSiteStrictFor.isolatedCopy() }; }
    bool isEmpty() const { return domainsToDeleteAllCookiesFor.isEmpty() && domainsToDeleteAllButHttpOnlyCookiesFor.isEmpty() && domainsToDeleteAllNonCookieWebsiteDataFor.isEmpty() && domainsToEnforceSameSiteStrictFor.isEmpty(); }
};

class WebResourceLoadStatisticsStore final : public ThreadSafeRefCounted<WebResourceLoadStatisticsStore, WTF::DestructionThread::Main> {
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
    using StorageAccessWasGranted = WebCore::StorageAccessWasGranted;
    using StorageAccessPromptWasShown = WebCore::StorageAccessPromptWasShown;
    using StorageAccessScope = WebCore::StorageAccessScope;
    using RequestStorageAccessResult = WebCore::RequestStorageAccessResult;

    static Ref<WebResourceLoadStatisticsStore> create(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost, WebCore::ResourceLoadStatistics::IsEphemeral isEphemeral)
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(networkSession, resourceLoadStatisticsDirectory, shouldIncludeLocalhost, isEphemeral));
    }

    ~WebResourceLoadStatisticsStore();

struct ThirdPartyDataForSpecificFirstParty {
    WebCore::RegistrableDomain firstPartyDomain;
    bool storageAccessGranted;
    Seconds timeLastUpdated;

    String toString() const
    {
        return makeString("Has been granted storage access under ", firstPartyDomain.string(), ": ", storageAccessGranted ? '1' : '0', "; Has been seen under ", firstPartyDomain.string(), " in the last 24 hours: ", WallTime::now().secondsSinceEpoch() - timeLastUpdated < 24_h ? '1' : '0');
    }

    void encode(IPC::Encoder& encoder) const
    {
        encoder << firstPartyDomain;
        encoder << storageAccessGranted;
        encoder << timeLastUpdated;
    }

    static Optional<ThirdPartyDataForSpecificFirstParty> decode(IPC::Decoder& decoder)
    {
        Optional<WebCore::RegistrableDomain> decodedDomain;
        decoder >> decodedDomain;
        if (!decodedDomain)
            return WTF::nullopt;

        Optional<bool> decodedStorageAccess;
        decoder >> decodedStorageAccess;
        if (!decodedStorageAccess)
            return WTF::nullopt;
        
        Optional<Seconds> decodedTimeLastUpdated;
        decoder >> decodedTimeLastUpdated;
        if (!decodedTimeLastUpdated)
            return WTF::nullopt;
        
        return {{ WTFMove(*decodedDomain), WTFMove(*decodedStorageAccess), WTFMove(*decodedTimeLastUpdated) }};
    }

    bool operator==(ThirdPartyDataForSpecificFirstParty const other) const
    {
        return firstPartyDomain == other.firstPartyDomain && storageAccessGranted == other.storageAccessGranted;
    }
};

struct ThirdPartyData {
    WebCore::RegistrableDomain thirdPartyDomain;
    Vector<ThirdPartyDataForSpecificFirstParty> underFirstParties;

    String toString() const
    {
        StringBuilder stringBuilder;
        stringBuilder.append("Third Party Registrable Domain: ", thirdPartyDomain.string(), "\n");
        stringBuilder.appendLiteral("    {");

        for (auto firstParty : underFirstParties) {
            stringBuilder.appendLiteral("{ ");
            stringBuilder.append(firstParty.toString());
            stringBuilder.appendLiteral(" },");
        }
        stringBuilder.appendLiteral("}");
        return stringBuilder.toString();
    }

    void encode(IPC::Encoder& encoder) const
    {
        encoder << thirdPartyDomain;
        encoder << underFirstParties;
    }

    static Optional<ThirdPartyData> decode(IPC::Decoder& decoder)
    {
        Optional<WebCore::RegistrableDomain> decodedDomain;
        decoder >> decodedDomain;
        if (!decodedDomain)
            return WTF::nullopt;

        Optional<Vector<ThirdPartyDataForSpecificFirstParty>> decodedFirstParties;
        decoder >> decodedFirstParties;
        if (!decodedFirstParties)
            return WTF::nullopt;

        return {{ WTFMove(*decodedDomain), WTFMove(*decodedFirstParties) }};
    }

    bool operator<(const ThirdPartyData &other) const
    {
        return underFirstParties.size() < other.underFirstParties.size();
    }
};

    void didDestroyNetworkSession(CompletionHandler<void()>&&);

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    WTF::WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void populateMemoryStoreFromDisk(CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool, CompletionHandler<void()>&&);
    void setShouldSubmitTelemetry(bool);

    void grantStorageAccess(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, StorageAccessPromptWasShown, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);

    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser);
    void logUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&);
    void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&, CompletionHandler<void()>&&);
    void clearUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&);
    void removeDataForDomain(const RegistrableDomain, CompletionHandler<void()>&&);
    void deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&&);
    void registrableDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&);
    StorageAccessWasGranted grantStorageAccessInStorageSession(const SubFrameDomain&, const TopFrameDomain&, Optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, StorageAccessScope);
    void hasHadUserInteraction(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void hasStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    bool hasStorageAccessForFrame(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier);
    void requestStorageAccess(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, StorageAccessScope,  CompletionHandler<void(RequestStorageAccessResult)>&&);
    void setLastSeen(const RegistrableDomain&, Seconds, CompletionHandler<void()>&&);
    void mergeStatisticForTesting(const RegistrableDomain&, const TopFrameDomain& topFrameDomain1, const TopFrameDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&&);
    void isRelationshipOnlyInDatabaseOnce(const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&&);
    void setPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(String)>&&);
    void isPrevalentResource(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&, CompletionHandler<void(bool)>&&);
    void clearPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void setGrandfathered(const RegistrableDomain&, bool, CompletionHandler<void()>&&);
    void isGrandfathered(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool, CompletionHandler<void()>&&);
    void setIsRunningTest(bool, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdateForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&&);
    void statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&&);
    void submitTelemetry(CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void clearInMemoryEphemeral(CompletionHandler<void()>&&);
    void domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&&);

    void setTimeToLiveUserInteraction(Seconds, CompletionHandler<void()>&&);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds, CompletionHandler<void()>&&);
    void setGrandfatheringTime(Seconds, CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(Seconds, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(size_t, CompletionHandler<void()>&&);
    void setPruneEntriesDownTo(size_t, CompletionHandler<void()>&&);

    void resetParametersToDefaultValues(CompletionHandler<void()>&&);

    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(const RegistrableDomain&, CompletionHandler<void()>&&);

    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const SubFrameDomain&, const TopFrameDomain&, Optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, StorageAccessScope, CompletionHandler<void(StorageAccessWasGranted)>&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);
    bool needsUserInteractionQuirk(const RegistrableDomain&) const;
    void callUpdatePrevalentDomainsToBlockCookiesForHandler(const RegistrableDomainsToBlockCookiesFor&, CompletionHandler<void()>&&);
    void callHasStorageAccessForFrameHandler(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);

    void hasCookies(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode);
    void setSameSiteStrictEnforcementEnabled(WebCore::SameSiteStrictEnforcementEnabled);
    void setFirstPartyWebsiteDataRemovalMode(WebCore::FirstPartyWebsiteDataRemovalMode, CompletionHandler<void()>&&);
    void setStandaloneApplicationDomain(const RegistrableDomain&, CompletionHandler<void()>&&);
    void setAppBoundDomains(HashSet<RegistrableDomain>&&, CompletionHandler<void()>&&);
    void didCreateNetworkProcess();

    void notifyResourceLoadStatisticsProcessed();

    NetworkSession* networkSession();
    void invalidateAndCancel();

    void sendDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned sigDigits, WebCore::ShouldSample) const;
    void notifyPageStatisticsTelemetryFinished(unsigned numberOfPrevalentResources, unsigned numberOfPrevalentResourcesWithUserInteraction, unsigned numberOfPrevalentResourcesWithoutUserInteraction, unsigned topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, unsigned medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, unsigned top3NumberOfPrevalentResourcesWithUI, unsigned top3MedianSubFrameWithoutUI, unsigned top3MedianSubResourceWithoutUI, unsigned top3MedianUniqueRedirectsWithoutUI, unsigned top3MedianDataRecordsRemovedWithoutUI) const;

    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&&);
    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&);
    void aggregatedThirdPartyData(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&&);
    static void suspend(CompletionHandler<void()>&&);
    static void resume();
    
    bool isEphemeral() const { return m_isEphemeral == WebCore::ResourceLoadStatistics::IsEphemeral::Yes; };
    void insertExpiredStatisticForTesting(const RegistrableDomain&, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);
    void flushAndDestroyPersistentStore(CompletionHandler<void()>&&);

private:
    explicit WebResourceLoadStatisticsStore(NetworkSession&, const String&, ShouldIncludeLocalhost, WebCore::ResourceLoadStatistics::IsEphemeral);

    void postTask(WTF::Function<void()>&&);
    static void postTaskReply(WTF::Function<void()>&&);

    void performDailyTasks();

    void hasStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, Optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    void requestStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);
    void requestStorageAccessUnderOpenerEphemeral(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&);
    void grantStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, StorageAccessPromptWasShown, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);

    void logUserInteractionEphemeral(const TopFrameDomain&, CompletionHandler<void()>&&);
    void clearUserInteractionEphemeral(const RegistrableDomain&, CompletionHandler<void()>&&);
    void hasHadUserInteractionEphemeral(const RegistrableDomain&, CompletionHandler<void(bool)>&&);

    StorageAccessStatus storageAccessStatus(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain);

    void destroyResourceLoadStatisticsStore(CompletionHandler<void()>&&);

    WeakPtr<NetworkSession> m_networkSession;
    Ref<WTF::WorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsStore> m_statisticsStore;
    std::unique_ptr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;

    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;

    WebCore::ResourceLoadStatistics::IsEphemeral m_isEphemeral { WebCore::ResourceLoadStatistics::IsEphemeral::No };
    HashSet<RegistrableDomain> m_domainsWithEphemeralUserInteraction;

    HashSet<RegistrableDomain> m_domainsWithUserInteractionQuirk;

    bool m_hasScheduledProcessStats { false };

    bool m_firstNetworkProcessCreated { false };
    
    enum class State {
        Running,
        WillSuspend,
        Suspended
    };
    static State suspendedState;
    static Lock suspendedStateLock;
    static Condition suspendedStateChangeCondition;
};

} // namespace WebKit

#endif
