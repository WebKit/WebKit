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

#if ENABLE(TRACKING_PREVENTION)

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
#include <wtf/Condition.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

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
class WebFrameProxy;
class WebProcessProxy;
enum class ShouldGrandfatherStatistics : bool;
enum class ShouldIncludeLocalhost : bool { No, Yes };
enum class EnableResourceLoadStatisticsDebugMode : bool { No, Yes };

using TopFrameDomain = WebCore::RegistrableDomain;
using SubResourceDomain = WebCore::RegistrableDomain;

struct RegistrableDomainsToBlockCookiesFor {
    Vector<WebCore::RegistrableDomain> domainsToBlockAndDeleteCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToBlockButKeepCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsWithUserInteractionAsFirstParty;
    HashMap<TopFrameDomain, SubResourceDomain> domainsWithStorageAccess;
    RegistrableDomainsToBlockCookiesFor isolatedCopy() const & { return { crossThreadCopy(domainsToBlockAndDeleteCookiesFor), crossThreadCopy(domainsToBlockButKeepCookiesFor), crossThreadCopy(domainsWithUserInteractionAsFirstParty), crossThreadCopy(domainsWithStorageAccess) }; }
    RegistrableDomainsToBlockCookiesFor isolatedCopy() && { return { crossThreadCopy(WTFMove(domainsToBlockAndDeleteCookiesFor)), crossThreadCopy(WTFMove(domainsToBlockButKeepCookiesFor)), crossThreadCopy(WTFMove(domainsWithUserInteractionAsFirstParty)), crossThreadCopy(WTFMove(domainsWithStorageAccess)) }; }
};
struct RegistrableDomainsToDeleteOrRestrictWebsiteDataFor {
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllButHttpOnlyCookiesFor;
    Vector<WebCore::RegistrableDomain> domainsToDeleteAllScriptWrittenStorageFor;
    Vector<WebCore::RegistrableDomain> domainsToEnforceSameSiteStrictFor;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor isolatedCopy() const & { return { crossThreadCopy(domainsToDeleteAllCookiesFor), crossThreadCopy(domainsToDeleteAllButHttpOnlyCookiesFor), crossThreadCopy(domainsToDeleteAllScriptWrittenStorageFor), crossThreadCopy(domainsToEnforceSameSiteStrictFor) }; }
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor isolatedCopy() && { return { crossThreadCopy(WTFMove(domainsToDeleteAllCookiesFor)), crossThreadCopy(WTFMove(domainsToDeleteAllButHttpOnlyCookiesFor)), crossThreadCopy(WTFMove(domainsToDeleteAllScriptWrittenStorageFor)), crossThreadCopy(WTFMove(domainsToEnforceSameSiteStrictFor)) }; }
    bool isEmpty() const { return domainsToDeleteAllCookiesFor.isEmpty() && domainsToDeleteAllButHttpOnlyCookiesFor.isEmpty() && domainsToDeleteAllScriptWrittenStorageFor.isEmpty() && domainsToEnforceSameSiteStrictFor.isEmpty(); }
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

    static Ref<WebResourceLoadStatisticsStore> create(NetworkSession&, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost, ResourceLoadStatistics::IsEphemeral);

    ~WebResourceLoadStatisticsStore();

    struct ThirdPartyDataForSpecificFirstParty {
        WebCore::RegistrableDomain firstPartyDomain;
        bool storageAccessGranted;
        Seconds timeLastUpdated;

        String toString() const;
        void encode(IPC::Encoder&) const;
        static std::optional<ThirdPartyDataForSpecificFirstParty> decode(IPC::Decoder&);

        // FIXME: Since this ignores differences in decodedTimeLastUpdated it probably should be a named function, not operator==.
        bool operator==(const ThirdPartyDataForSpecificFirstParty&) const;
    };

    struct ThirdPartyData {
        WebCore::RegistrableDomain thirdPartyDomain;
        Vector<ThirdPartyDataForSpecificFirstParty> underFirstParties;

        String toString() const;
        void encode(IPC::Encoder&) const;
        static std::optional<ThirdPartyData> decode(IPC::Decoder&);

        // FIXME: This sorts by number of underFirstParties, so it probably should be a named function, not operator<.
        bool operator<(const ThirdPartyData&) const;
    };

    void didDestroyNetworkSession(CompletionHandler<void()>&&);

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    SuspendableWorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void populateMemoryStoreFromDisk(CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool, CompletionHandler<void()>&&);

    void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, StorageAccessPromptWasShown, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);

    void logFrameNavigation(NavigatedToDomain&&, TopFrameDomain&&, NavigatedFromDomain&&, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser);
    void logUserInteraction(TopFrameDomain&&, CompletionHandler<void()>&&);
    void logCrossSiteLoadWithLinkDecoration(NavigatedFromDomain&&, NavigatedToDomain&&, CompletionHandler<void()>&&);
    void clearUserInteraction(TopFrameDomain&&, CompletionHandler<void()>&&);
    void setTimeAdvanceForTesting(Seconds, CompletionHandler<void()>&&);
    void removeDataForDomain(const RegistrableDomain, CompletionHandler<void()>&&);
    void deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&);
    void registrableDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&);
    StorageAccessWasGranted grantStorageAccessInStorageSession(const SubFrameDomain&, const TopFrameDomain&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, StorageAccessScope);
    void hasHadUserInteraction(RegistrableDomain&&, CompletionHandler<void(bool)>&&);
    void hasStorageAccess(SubFrameDomain&&, TopFrameDomain&&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    bool hasStorageAccessForFrame(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier);
    void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, StorageAccessScope,  CompletionHandler<void(RequestStorageAccessResult)>&&);
    void setLastSeen(RegistrableDomain&&, Seconds, CompletionHandler<void()>&&);
    void mergeStatisticForTesting(RegistrableDomain&&, TopFrameDomain&& topFrameDomain1, TopFrameDomain&& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&&);
    void isRelationshipOnlyInDatabaseOnce(RegistrableDomain&& subDomain, RegistrableDomain&& topDomain, CompletionHandler<void(bool)>&&);
    void setPrevalentResource(RegistrableDomain&&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(RegistrableDomain&&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&&);
    void setMostRecentWebPushInteractionTime(RegistrableDomain&&, CompletionHandler<void()>&&);
    void isPrevalentResource(RegistrableDomain&&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(RegistrableDomain&&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(SubResourceDomain&&, TopFrameDomain&&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(SubFrameDomain&&, TopFrameDomain&&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(RedirectedFromDomain&&, RedirectedToDomain&&, CompletionHandler<void(bool)>&&);
    void clearPrevalentResource(RegistrableDomain&&, CompletionHandler<void()>&&);
    void setGrandfathered(RegistrableDomain&&, bool, CompletionHandler<void()>&&);
    void isGrandfathered(RegistrableDomain&&, CompletionHandler<void(bool)>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool, CompletionHandler<void()>&&);
    void setIsRunningTest(bool, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(SubFrameDomain&&, TopFrameDomain&&, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(SubResourceDomain&&, TopFrameDomain&&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(SubResourceDomain&&, RedirectedToDomain&&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(SubResourceDomain&&, RedirectedFromDomain&&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(TopFrameDomain&&, RedirectedToDomain&&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(TopFrameDomain&&, RedirectedFromDomain&&, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdateForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&&);
    void statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&&);
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
    void setPrevalentResourceForDebugMode(RegistrableDomain&&, CompletionHandler<void()>&&);

    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const SubFrameDomain&, const TopFrameDomain&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, StorageAccessScope, CompletionHandler<void(StorageAccessWasGranted)>&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);
    bool needsUserInteractionQuirk(const RegistrableDomain&) const;
    void callUpdatePrevalentDomainsToBlockCookiesForHandler(const RegistrableDomainsToBlockCookiesFor&, CompletionHandler<void()>&&);
    void callHasStorageAccessForFrameHandler(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);

    void hasCookies(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode);
    void setSameSiteStrictEnforcementEnabled(WebCore::SameSiteStrictEnforcementEnabled);
    void setFirstPartyWebsiteDataRemovalMode(WebCore::FirstPartyWebsiteDataRemovalMode, CompletionHandler<void()>&&);
    void setStandaloneApplicationDomain(const RegistrableDomain&, CompletionHandler<void()>&&);
#if ENABLE(APP_BOUND_DOMAINS)
    void setAppBoundDomains(HashSet<RegistrableDomain>&&, CompletionHandler<void()>&&);
#endif
#if ENABLE(MANAGED_DOMAINS)
    void setManagedDomains(HashSet<RegistrableDomain>&&, CompletionHandler<void()>&&);
#endif
    void didCreateNetworkProcess();

    void notifyResourceLoadStatisticsProcessed();

    NetworkSession* networkSession();
    void invalidateAndCancel();

    void sendDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned sigDigits, WebCore::ShouldSample) const;

    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&&, CompletionHandler<void()>&&);
    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&);
    void aggregatedThirdPartyData(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&&);
    static void suspend(CompletionHandler<void()>&&);
    static void resume();
    
    bool isEphemeral() const { return m_isEphemeral == WebCore::ResourceLoadStatistics::IsEphemeral::Yes; };
    void insertExpiredStatisticForTesting(RegistrableDomain&&, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);

private:
    explicit WebResourceLoadStatisticsStore(NetworkSession&, const String&, ShouldIncludeLocalhost, WebCore::ResourceLoadStatistics::IsEphemeral);

    void postTask(WTF::Function<void()>&&);
    static void postTaskReply(WTF::Function<void()>&&);

    void performDailyTasks();

    void hasStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    void requestStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);
    void requestStorageAccessUnderOpenerEphemeral(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&);
    void grantStorageAccessEphemeral(const SubFrameDomain&, const TopFrameDomain&, WebCore::FrameIdentifier, WebCore::PageIdentifier, StorageAccessPromptWasShown, StorageAccessScope, CompletionHandler<void(RequestStorageAccessResult)>&&);

    void logUserInteractionEphemeral(const TopFrameDomain&, CompletionHandler<void()>&&);
    void clearUserInteractionEphemeral(const RegistrableDomain&, CompletionHandler<void()>&&);
    void hasHadUserInteractionEphemeral(const RegistrableDomain&, CompletionHandler<void(bool)>&&);

    StorageAccessStatus storageAccessStatus(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain);

    void destroyResourceLoadStatisticsStore(CompletionHandler<void()>&&);

    WeakPtr<NetworkSession> m_networkSession;
    Ref<SuspendableWorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsStore> m_statisticsStore;

    RunLoop::Timer m_dailyTasksTimer;

    WebCore::ResourceLoadStatistics::IsEphemeral m_isEphemeral { WebCore::ResourceLoadStatistics::IsEphemeral::No };
    HashSet<RegistrableDomain> m_domainsWithEphemeralUserInteraction;

    HashSet<RegistrableDomain> m_domainsWithUserInteractionQuirk;
    HashMap<TopFrameDomain, SubResourceDomain> m_domainsWithCrossPageStorageAccessQuirk;

    bool m_hasScheduledProcessStats { false };

    bool m_firstNetworkProcessCreated { false };
};

} // namespace WebKit

#endif
