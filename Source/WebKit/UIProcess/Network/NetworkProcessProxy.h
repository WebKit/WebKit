/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "AuxiliaryProcessProxy.h"
#include "CallbackID.h"
#include "NetworkProcessProxyMessagesReplies.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "UserContentControllerIdentifier.h"
#include "WebProcessProxyMessagesReplies.h"
#include "WebsiteDataStore.h"
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <memory>
#include <wtf/Deque.h>

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerProxy.h"
#endif

#if PLATFORM(COCOA)
#include "XPCEventHandler.h"
#endif

namespace IPC {
class FormDataReference;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class AuthenticationChallenge;
class ProtectionSpace;
class ResourceRequest;
enum class ShouldSample : bool;
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : bool;
enum class StoredCredentialsPolicy : uint8_t;
class SecurityOrigin;
struct SecurityOriginData;
struct ClientOrigin;
}

namespace WebKit {

class DownloadProxy;
class DownloadProxyMap;
class WebPageProxy;
class WebProcessPool;
class WebUserContentControllerProxy;

enum class ShouldGrandfatherStatistics : bool;
enum class StorageAccessStatus : uint8_t;
enum class WebsiteDataFetchOption : uint8_t;
enum class WebsiteDataType : uint32_t;

struct FrameInfoData;
struct NetworkProcessCreationParameters;
struct ResourceLoadInfo;
struct WebsiteData;

class NetworkProcessProxy final : public AuxiliaryProcessProxy, private ProcessThrottlerClient, public CanMakeWeakPtr<NetworkProcessProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
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

    explicit NetworkProcessProxy(WebProcessPool&);
    ~NetworkProcessProxy();

    void getNetworkProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetNetworkProcessConnectionDelayedReply&&);

    DownloadProxy& createDownloadProxy(WebsiteDataStore&, const WebCore::ResourceRequest&, const FrameInfoData&, WebPageProxy* originatingPage);

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebKit::WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void renameOriginInWebsiteData(PAL::SessionID, const URL&, const URL&, OptionSet<WebsiteDataType>, CompletionHandler<void()>&&);

    void getLocalStorageDetails(PAL::SessionID, CompletionHandler<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&&);

    void preconnectTo(PAL::SessionID, WebPageProxyIdentifier, WebCore::PageIdentifier, const URL&, const String&, WebCore::StoredCredentialsPolicy, Optional<NavigatingToAppBoundDomain>);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void clearPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void clearUserInteraction(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(PAL::SessionID, CompletionHandler<void(String)>&&);
    void updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID, const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void hasHadUserInteraction(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isRelationshipOnlyInDatabaseOnce(PAL::SessionID, const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&&);
    void hasLocalStorage(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isGrandfathered(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(PAL::SessionID, const RedirectedFromDomain&, const RedirectedToDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(PAL::SessionID, const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(PAL::SessionID, const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void logUserInteraction(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(PAL::SessionID, CompletionHandler<void()>&&);
    void setLastSeen(PAL::SessionID, const RegistrableDomain&, Seconds, CompletionHandler<void()>&&);
    void domainIDExistsInDatabase(PAL::SessionID, int domainID, CompletionHandler<void(bool)>&&);
    void statisticsDatabaseHasAllTables(PAL::SessionID, CompletionHandler<void(bool)>&&);
    void mergeStatisticForTesting(PAL::SessionID, const RegistrableDomain&, const TopFrameDomain& topFrameDomain1, const TopFrameDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&&);
    void insertExpiredStatisticForTesting(PAL::SessionID, const RegistrableDomain&, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);
    void setAgeCapForClientSideCookies(PAL::SessionID, Optional<Seconds>, CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setGrandfathered(PAL::SessionID, const RegistrableDomain&, bool isGrandfathered, CompletionHandler<void()>&&);
    void setUseITPDatabase(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID, bool, CompletionHandler<void()>&&);
    void setIsRunningResourceLoadStatisticsTest(PAL::SessionID, bool, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(PAL::SessionID, const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(PAL::SessionID, const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(PAL::SessionID, const SubResourceDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(PAL::SessionID, const SubResourceDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void setTimeToLiveUserInteraction(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(PAL::SessionID, const TopFrameDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(PAL::SessionID, const TopFrameDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void setPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void getResourceLoadStatisticsDataSummary(PAL::SessionID, CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&&);
    void getAllStorageAccessEntries(PAL::SessionID, CompletionHandler<void(Vector<String> domains)>&&);
    void requestStorageAccessConfirm(WebPageProxyIdentifier, WebCore::FrameIdentifier, const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void resetParametersToDefaultValues(PAL::SessionID, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(PAL::SessionID, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(PAL::SessionID, Optional<WallTime> modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(PAL::SessionID, CompletionHandler<void()>&&);
    void submitTelemetry(PAL::SessionID, CompletionHandler<void()>&&);
    void setCacheMaxAgeCapForPrevalentResources(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setGrandfatheringTime(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(PAL::SessionID, size_t maximumEntryCount, CompletionHandler<void()>&&);
    void setMinimumTimeBetweenDataRecordsRemoval(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setPruneEntriesDownTo(PAL::SessionID, size_t pruneTargetCount, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsDebugMode(PAL::SessionID, bool debugMode, CompletionHandler<void()>&&);
    void isResourceLoadStatisticsEphemeral(PAL::SessionID, CompletionHandler<void(bool)>&&);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(PAL::SessionID, bool, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID, CompletionHandler<void()>&&);
    void didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID, const NavigatedFromDomain&, const NavigatedToDomain&, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag>, WebPageProxyIdentifier, WebCore::PageIdentifier);
    void didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(WebPageProxyIdentifier);
    void setCrossSiteLoadWithLinkDecorationForTesting(PAL::SessionID, const NavigatedFromDomain&, const NavigatedToDomain&, CompletionHandler<void()>&&);
    void resetCrossSiteLoadsWithLinkDecorationForTesting(PAL::SessionID, CompletionHandler<void()>&&);
    void deleteCookiesForTesting(PAL::SessionID, const RegistrableDomain&, bool includeHttpOnlyCookies, CompletionHandler<void()>&&);
    void deleteWebsiteDataInUIProcessForRegistrableDomains(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Vector<RegistrableDomain>, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void hasIsolatedSession(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID, const HashSet<RegistrableDomain>&, CompletionHandler<void()>&&);
    void setShouldDowngradeReferrerForTesting(bool, CompletionHandler<void()>&&);
    void setThirdPartyCookieBlockingMode(PAL::SessionID, WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID, WebCore::SameSiteStrictEnforcementEnabled, CompletionHandler<void()>&&);
    void setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID, WebCore::FirstPartyWebsiteDataRemovalMode, CompletionHandler<void()>&&);
    void setToSameSiteStrictCookiesForTesting(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setFirstPartyHostCNAMEDomainForTesting(PAL::SessionID, const String& firstPartyHost, const RegistrableDomain& cnameDomain, CompletionHandler<void()>&&);
    void setThirdPartyCNAMEDomainForTesting(PAL::SessionID, const WebCore::RegistrableDomain&, CompletionHandler<void()>&&);
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
#endif

    void setAdClickAttributionDebugMode(bool);
    
    void synthesizeAppIsBackground(bool background);

    void setIsHoldingLockedFiles(bool);

    void syncAllCookies();
    void didSyncAllCookies();

    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier, Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReplyDelayedReply&&);
    void terminateUnresponsiveServiceWorkerProcesses(WebCore::ProcessIdentifier);

    ProcessThrottler& throttler() final { return m_throttler; }
    void updateProcessAssertion();

    WebProcessPool& processPool() { return m_processPool; }

#if ENABLE(CONTENT_EXTENSIONS)
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);
#endif

    void addSession(Ref<WebsiteDataStore>&&);
    bool hasSession(PAL::SessionID) const;
    void removeSession(PAL::SessionID);
    
#if ENABLE(INDEXED_DATABASE)
    void createSymLinkForFileUpgrade(const String& indexedDatabaseDirectory);
#endif

    // ProcessThrottlerClient
    void sendProcessDidResume() final;
    ASCIILiteral clientName() const final { return "NetworkProcess"_s; }
    
    void sendProcessWillSuspendImminentlyForTesting();

    void registerSchemeForLegacyCustomProtocol(const String&);
    void unregisterSchemeForLegacyCustomProtocol(const String&);

    void resetQuota(PAL::SessionID, CompletionHandler<void()>&&);

    void resourceLoadDidSendRequest(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceRequest&&, Optional<IPC::FormDataReference>&&);
    void resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void resourceLoadDidReceiveChallenge(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::AuthenticationChallenge&&);
    void resourceLoadDidReceiveResponse(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&);
    void resourceLoadDidCompleteWithError(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceError&&);

    void hasAppBoundSession(PAL::SessionID, CompletionHandler<void(bool)>&&);
    void clearAppBoundSession(PAL::SessionID, CompletionHandler<void()>&&);
    void getAppBoundDomains(PAL::SessionID, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);

    // ProcessThrottlerClient
    void sendPrepareToSuspend(IsSuspensionImminent, CompletionHandler<void()>&&) final;
    void updateBundleIdentifier(const String&, CompletionHandler<void()>&&);
    void clearBundleIdentifier(CompletionHandler<void()>&&);

private:
    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "Networking"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void networkProcessCrashed();
    void clearCallbackStates();

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;
    void didReceiveSyncNetworkProcessProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    // Message handlers
    void didReceiveNetworkProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveAuthenticationChallenge(PAL::SessionID, WebPageProxyIdentifier, const Optional<WebCore::SecurityOriginData>&, WebCore::AuthenticationChallenge&&, bool, uint64_t challengeID);
    void negotiatedLegacyTLS(WebPageProxyIdentifier);
    void didNegotiateModernTLS(WebPageProxyIdentifier, const WebCore::AuthenticationChallenge&);
    void didFetchWebsiteData(CallbackID, const WebsiteData&);
    void didDeleteWebsiteData(CallbackID);
    void didDeleteWebsiteDataForOrigins(CallbackID);
    void setWebProcessHasUploads(WebCore::ProcessIdentifier, bool);
    void logDiagnosticMessage(WebPageProxyIdentifier, const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(WebPageProxyIdentifier, const String& message, const String& description, uint32_t result, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(WebPageProxyIdentifier, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void logTestingEvent(PAL::SessionID, const String& event);
    void notifyResourceLoadStatisticsProcessed();
    void notifyWebsiteDataDeletionForRegistrableDomainsFinished();
    void notifyWebsiteDataScanForRegistrableDomainsFinished();
    void notifyResourceLoadStatisticsTelemetryFinished(unsigned numberOfPrevalentResources, unsigned numberOfPrevalentResourcesWithUserInteraction, unsigned numberOfPrevalentResourcesWithoutUserInteraction, unsigned topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, unsigned medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, unsigned top3NumberOfPrevalentResourcesWithUI, unsigned top3MedianSubFrameWithoutUI, unsigned top3MedianSubResourceWithoutUI, unsigned top3MedianUniqueRedirectsWithoutUI, unsigned top3MedianDataRecordsRemovedWithoutUI);
#endif
    void retrieveCacheStorageParameters(PAL::SessionID);

#if ENABLE(CONTENT_EXTENSIONS)
    void contentExtensionRules(UserContentControllerIdentifier);
#endif

#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(WebCore::RegistrableDomain&&, PAL::SessionID, CompletionHandler<void()>&&);
    void workerContextConnectionNoLongerNeeded(WebCore::ProcessIdentifier);
    void registerServiceWorkerClientProcess(WebCore::ProcessIdentifier webProcessIdentifier, WebCore::ProcessIdentifier serviceWorkerProcessIdentifier);
    void unregisterServiceWorkerClientProcess(WebCore::ProcessIdentifier webProcessIdentifier, WebCore::ProcessIdentifier serviceWorkerProcessIdentifier);
#endif

    void terminateWebProcess(WebCore::ProcessIdentifier);

    void requestStorageSpace(PAL::SessionID, const WebCore::ClientOrigin&, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(Optional<uint64_t> quota)>&&);

    WebsiteDataStore* websiteDataStoreFromSessionID(PAL::SessionID);

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;
#if PLATFORM(COCOA)
    RefPtr<XPCEventHandler> xpcEventHandler() const override;
#endif

    void processAuthenticationChallenge(PAL::SessionID, Ref<AuthenticationChallengeProxy>&&);

    WebProcessPool& m_processPool;

    HashMap<CallbackID, CompletionHandler<void(WebsiteData)>> m_pendingFetchWebsiteDataCallbacks;
    HashMap<CallbackID, CompletionHandler<void()>> m_pendingDeleteWebsiteDataCallbacks;
    HashMap<CallbackID, CompletionHandler<void()>> m_pendingDeleteWebsiteDataForOriginsCallbacks;

    std::unique_ptr<DownloadProxyMap> m_downloadProxyMap;
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    LegacyCustomProtocolManagerProxy m_customProtocolManagerProxy;
#endif
    ProcessThrottler m_throttler;
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_activityForHoldingLockedFiles;
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_syncAllCookiesActivity;
    ProcessThrottler::ActivityVariant m_activityFromWebProcesses;
    
    unsigned m_syncAllCookiesCounter { 0 };

#if ENABLE(CONTENT_EXTENSIONS)
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;
#endif

    struct UploadActivity {
        std::unique_ptr<ProcessAssertion> uiAssertion;
        std::unique_ptr<ProcessAssertion> networkAssertion;
        HashMap<WebCore::ProcessIdentifier, std::unique_ptr<ProcessAssertion>> webProcessAssertions;
    };
    Optional<UploadActivity> m_uploadActivity;

#if PLATFORM(COCOA)
    class XPCEventHandler : public WebKit::XPCEventHandler {
    public:
        XPCEventHandler(const NetworkProcessProxy&);

        bool handleXPCEvent(xpc_object_t) const override;

    private:
        WeakPtr<NetworkProcessProxy> m_networkProcess;
    };
#endif

    HashSet<PAL::SessionID> m_sessionIDs;
};

} // namespace WebKit
