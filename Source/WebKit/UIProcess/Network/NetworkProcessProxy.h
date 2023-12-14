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

#include "AppPrivacyReport.h"
#include "AuxiliaryProcessProxy.h"
#include "BackgroundFetchState.h"
#include "DataReference.h"
#include "DataTaskIdentifier.h"
#include "IdentifierTypes.h"
#include "NetworkResourceLoadIdentifier.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "QuotaIncreaseRequestIdentifier.h"
#include "UserContentControllerIdentifier.h"
#include "WebsiteDataStore.h"
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NotificationEventType.h>
#include <WebCore/RegistrableDomain.h>
#include <memory>
#include <wtf/Deque.h>
#include <wtf/WeakHashMap.h>

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerProxy.h"
#endif

#if PLATFORM(COCOA)
#include "XPCEventHandler.h"
#include <wtf/OSObjectPtr.h>
#endif

namespace IPC {
class FormDataReference;
}

namespace API {
class CustomProtocolManagerClient;
class DataTask;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class AuthenticationChallenge;
class SharedBuffer;
class ProtectionSpace;
class ResourceRequest;
class SecurityOrigin;
class SecurityOriginData;
enum class ShouldSample : bool;
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : bool;
enum class StoredCredentialsPolicy : uint8_t;
struct ClientOrigin;
struct NotificationData;
struct NotificationPayload;
struct OrganizationStorageAccessPromptQuirk;
}

namespace WebKit {

class DownloadProxy;
class DownloadProxyMap;
class WebPageProxy;
class WebUserContentControllerProxy;
class StorageAccessPromptQuirkObserver;

enum class BackgroundFetchChange : uint8_t;
enum class LoadedWebArchive : bool;
enum class ProcessTerminationReason : uint8_t;
enum class RemoteWorkerType : uint8_t;
enum class ShouldGrandfatherStatistics : bool;
enum class StorageAccessStatus : uint8_t;
enum class WebsiteDataFetchOption : uint8_t;
enum class WebsiteDataType : uint32_t;

struct FrameInfoData;
struct NetworkProcessCreationParameters;
struct ResourceLoadInfo;
struct WebPushMessage;
struct WebsiteData;
struct WebsiteDataStoreParameters;

class NetworkProcessProxy final : public AuxiliaryProcessProxy {
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

    static Ref<NetworkProcessProxy> ensureDefaultNetworkProcess();
    static WeakPtr<NetworkProcessProxy>& defaultNetworkProcess();
    static Ref<NetworkProcessProxy> create() { return adoptRef(*new NetworkProcessProxy); }
    ~NetworkProcessProxy();

    static Vector<Ref<NetworkProcessProxy>> allNetworkProcesses();
    
    void getNetworkProcessConnection(WebProcessProxy&, CompletionHandler<void(NetworkProcessConnectionInfo&&)>&&);

    Ref<DownloadProxy> createDownloadProxy(WebsiteDataStore&, Ref<API::DownloadClient>&&, const WebCore::ResourceRequest&, const FrameInfoData&, WebPageProxy* originatingPage);
    void dataTaskWithRequest(WebPageProxy&, PAL::SessionID, WebCore::ResourceRequest&&, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(API::DataTask&)>&&);

    void addAllowedFirstPartyForCookies(WebProcessProxy&, const WebCore::RegistrableDomain& firstPartyForCookies, LoadedWebArchive, CompletionHandler<void()>&&);

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebKit::WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void renameOriginInWebsiteData(PAL::SessionID, const WebCore::SecurityOriginData&, const WebCore::SecurityOriginData&, OptionSet<WebsiteDataType>, CompletionHandler<void()>&&);
    void websiteDataOriginDirectoryForTesting(PAL::SessionID, WebCore::ClientOrigin&&, OptionSet<WebsiteDataType>, CompletionHandler<void(const String&)>&&);

    void preconnectTo(PAL::SessionID, WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::ResourceRequest&&, WebCore::StoredCredentialsPolicy, std::optional<NavigatingToAppBoundDomain>);

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
    void insertExpiredStatisticForTesting(PAL::SessionID, const RegistrableDomain&, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setGrandfathered(PAL::SessionID, const RegistrableDomain&, bool isGrandfathered, CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID, bool, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsTimeAdvanceForTesting(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
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
    void getResourceLoadStatisticsDataSummary(PAL::SessionID, CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&&);
    void getAllStorageAccessEntries(PAL::SessionID, CompletionHandler<void(Vector<String> domains)>&&);
    void requestStorageAccessConfirm(WebPageProxyIdentifier, WebCore::FrameIdentifier, const SubFrameDomain&, const TopFrameDomain&, std::optional<WebCore::OrganizationStorageAccessPromptQuirk>&&, CompletionHandler<void(bool)>&&);
    void resetParametersToDefaultValues(PAL::SessionID, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(PAL::SessionID, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(PAL::SessionID, std::optional<WallTime> modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(PAL::SessionID, CompletionHandler<void()>&&);
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
    void deleteWebsiteDataInUIProcessForRegistrableDomains(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Vector<RegistrableDomain>&&, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void hasIsolatedSession(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
#if ENABLE(APP_BOUND_DOMAINS)
    void setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID, const HashSet<RegistrableDomain>&, CompletionHandler<void()>&&);
#endif
#if ENABLE(MANAGED_DOMAINS)
    void setManagedDomainsForResourceLoadStatistics(PAL::SessionID, const HashSet<RegistrableDomain>&, CompletionHandler<void()>&&);
#endif
    void setShouldDowngradeReferrerForTesting(bool, CompletionHandler<void()>&&);
    void setThirdPartyCookieBlockingMode(PAL::SessionID, WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID, WebCore::SameSiteStrictEnforcementEnabled, CompletionHandler<void()>&&);
    void setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID, WebCore::FirstPartyWebsiteDataRemovalMode, CompletionHandler<void()>&&);
    void setToSameSiteStrictCookiesForTesting(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setFirstPartyHostCNAMEDomainForTesting(PAL::SessionID, const String& firstPartyHost, const RegistrableDomain& cnameDomain, CompletionHandler<void()>&&);
    void setThirdPartyCNAMEDomainForTesting(PAL::SessionID, const WebCore::RegistrableDomain&, CompletionHandler<void()>&&);
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&&, CompletionHandler<void()>&&);

    void setPrivateClickMeasurementDebugMode(PAL::SessionID, bool);

    void synthesizeAppIsBackground(bool background);

    void flushCookies(PAL::SessionID, CompletionHandler<void()>&&);

    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier, CompletionHandler<void(bool)>&&);
    void terminateUnresponsiveServiceWorkerProcesses(WebCore::ProcessIdentifier);

    void requestTermination();

    ProcessThrottler& throttler() final { return m_throttler; }
    void updateProcessAssertion();

#if ENABLE(CONTENT_EXTENSIONS)
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);
#endif

    enum class SendParametersToNetworkProcess : bool { No, Yes };
    void addSession(WebsiteDataStore&, SendParametersToNetworkProcess);
    void removeSession(WebsiteDataStore&, CompletionHandler<void(String&&)>&&);
    
    void createSymLinkForFileUpgrade(const String& indexedDatabaseDirectory);

    // ProcessThrottlerClient
    void sendProcessDidResume(ResumeReason) final;
    ASCIILiteral clientName() const final { return "NetworkProcess"_s; }
    
    static void setSuspensionAllowedForTesting(bool);
    void sendProcessWillSuspendImminentlyForTesting();

    void registerSchemeForLegacyCustomProtocol(const String&);
    void unregisterSchemeForLegacyCustomProtocol(const String&);

    void networkProcessDidTerminate(ProcessTerminationReason);
    
    void resetQuota(PAL::SessionID, CompletionHandler<void()>&&);
    void resetStoragePersistedState(PAL::SessionID, CompletionHandler<void()>&&);
#if PLATFORM(IOS_FAMILY)
    void setBackupExclusionPeriodForTesting(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
#endif

#if ENABLE(NETWORK_ISSUE_REPORTING)
    void reportNetworkIssue(WebPageProxyIdentifier, const URL&);
#endif

    void resourceLoadDidSendRequest(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceRequest&&, std::optional<IPC::FormDataReference>&&);
    void resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void resourceLoadDidReceiveChallenge(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::AuthenticationChallenge&&);
    void resourceLoadDidReceiveResponse(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&);
    void resourceLoadDidCompleteWithError(WebPageProxyIdentifier, ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceError&&);

#if ENABLE(APP_BOUND_DOMAINS)
    void hasAppBoundSession(PAL::SessionID, CompletionHandler<void(bool)>&&);
    void clearAppBoundSession(PAL::SessionID, CompletionHandler<void()>&&);
    void getAppBoundDomains(PAL::SessionID, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
    void getWindowSceneIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&)>&&);
#endif
    // ProcessThrottlerClient
    void sendPrepareToSuspend(IsSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&&) final;
    void updateBundleIdentifier(const String&, CompletionHandler<void()>&&);
    void clearBundleIdentifier(CompletionHandler<void()>&&);

    API::CustomProtocolManagerClient& customProtocolManagerClient() { return m_customProtocolManagerClient.get(); }

#if PLATFORM(COCOA)
    bool sendXPCEndpointToProcess(AuxiliaryProcessProxy&);
    xpc_object_t xpcEndpointMessage() const { return m_endpointMessage.get(); }
#endif

    void getPendingPushMessages(PAL::SessionID, CompletionHandler<void(const Vector<WebPushMessage>&)>&&);
    void processPushMessage(PAL::SessionID, const WebPushMessage&, CompletionHandler<void(bool wasProcessed, std::optional<WebCore::NotificationPayload>&&)>&&);
    void processNotificationEvent(const WebCore::NotificationData&, WebCore::NotificationEventType, CompletionHandler<void(bool wasProcessed)>&&);

    void getAllBackgroundFetchIdentifiers(PAL::SessionID, CompletionHandler<void(Vector<String>&&)>&&);
    void getBackgroundFetchState(PAL::SessionID, const String&, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&&);
    void abortBackgroundFetch(PAL::SessionID, const String&, CompletionHandler<void()>&&);
    void pauseBackgroundFetch(PAL::SessionID, const String&, CompletionHandler<void()>&&);
    void resumeBackgroundFetch(PAL::SessionID, const String&, CompletionHandler<void()>&&);
    void clickBackgroundFetch(PAL::SessionID, const String&, CompletionHandler<void()>&&);

    void setPushAndNotificationsEnabledForOrigin(PAL::SessionID, const WebCore::SecurityOriginData&, bool, CompletionHandler<void()>&&);
    void deletePushAndNotificationRegistration(PAL::SessionID, const WebCore::SecurityOriginData&, CompletionHandler<void(const String&)>&&);
    void hasPushSubscriptionForTesting(PAL::SessionID, const URL&, CompletionHandler<void(bool)>&&);

    void dataTaskReceivedChallenge(DataTaskIdentifier, WebCore::AuthenticationChallenge&&, CompletionHandler<void(AuthenticationChallengeDisposition, WebCore::Credential&&)>&&);
    void dataTaskWillPerformHTTPRedirection(DataTaskIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, CompletionHandler<void(bool)>&&);
    void dataTaskDidReceiveResponse(DataTaskIdentifier, WebCore::ResourceResponse&&, CompletionHandler<void(bool)>&&);
    void dataTaskDidReceiveData(DataTaskIdentifier, const IPC::DataReference&);
    void dataTaskDidCompleteWithError(DataTaskIdentifier, WebCore::ResourceError&&);
    void cancelDataTask(DataTaskIdentifier, PAL::SessionID);

    void deleteWebsiteDataInWebProcessesForOrigin(OptionSet<WebsiteDataType>, const WebCore::ClientOrigin&, PAL::SessionID, WebPageProxyIdentifier, CompletionHandler<void()>&&);
    void reloadExecutionContextsForOrigin(const WebCore::ClientOrigin&, PAL::SessionID, std::optional<WebCore::FrameIdentifier> triggeringFrame, CompletionHandler<void()>&&);

    void terminateRemoteWorkerContextConnectionWhenPossible(RemoteWorkerType, PAL::SessionID, const WebCore::RegistrableDomain&, WebCore::ProcessIdentifier);

    void openWindowFromServiceWorker(PAL::SessionID, const String& urlString, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(std::optional<WebCore::PageIdentifier>&&)>&&);

    void navigateServiceWorkerClient(WebCore::FrameIdentifier, WebCore::ScriptExecutionContextIdentifier, const URL&, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&&);

    void cookiesDidChange(PAL::SessionID);

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    void setEmulatedConditions(PAL::SessionID, std::optional<int64_t>&& bytesPerSecondLimit);
#endif

    void notifyMediaStreamingActivity(bool);

private:
    explicit NetworkProcessProxy();

    void sendCreationParametersToNewProcess();

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "Networking"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;
    void terminate() final;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;
    bool didReceiveSyncNetworkProcessProxyMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() final;

    // Message handlers
    void didReceiveNetworkProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveAuthenticationChallenge(PAL::SessionID, WebPageProxyIdentifier, const std::optional<WebCore::SecurityOriginData>&, WebCore::AuthenticationChallenge&&, bool, AuthenticationChallengeIdentifier);
    void negotiatedLegacyTLS(WebPageProxyIdentifier);
    void didNegotiateModernTLS(WebPageProxyIdentifier, const URL&);
    void didBlockLoadToKnownTracker(WebPageProxyIdentifier, const URL&);
    void setWebProcessHasUploads(WebCore::ProcessIdentifier, bool);
    void logDiagnosticMessage(WebPageProxyIdentifier, const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(WebPageProxyIdentifier, const String& message, const String& description, uint32_t result, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(WebPageProxyIdentifier, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);
    void logTestingEvent(PAL::SessionID, const String& event);
    void notifyResourceLoadStatisticsProcessed();
    void notifyWebsiteDataDeletionForRegistrableDomainsFinished();
    void notifyWebsiteDataScanForRegistrableDomainsFinished();

#if ENABLE(CONTENT_EXTENSIONS)
    void contentExtensionRules(UserContentControllerIdentifier);
#endif

#if USE(RUNNINGBOARD)
    void wakeUpWebProcessForIPC(WebCore::ProcessIdentifier);
#endif

    void startServiceWorkerBackgroundProcessing(WebCore::ProcessIdentifier serviceWorkerProcessIdentifier);
    void endServiceWorkerBackgroundProcessing(WebCore::ProcessIdentifier serviceWorkerProcessIdentifier);
    void requestBackgroundFetchPermission(PAL::SessionID, const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void notifyBackgroundFetchChange(PAL::SessionID, const String&, BackgroundFetchChange);
    void remoteWorkerContextConnectionNoLongerNeeded(RemoteWorkerType, WebCore::ProcessIdentifier);
    void establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType, WebCore::RegistrableDomain&&, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&&);
    void registerRemoteWorkerClientProcess(RemoteWorkerType, WebCore::ProcessIdentifier clientProcessIdentifier, WebCore::ProcessIdentifier remoteWorkerProcessIdentifier);
    void unregisterRemoteWorkerClientProcess(RemoteWorkerType, WebCore::ProcessIdentifier clientProcessIdentifier, WebCore::ProcessIdentifier remoteWorkerProcessIdentifier);
    void reportConsoleMessage(PAL::SessionID, const URL&, const WebCore::SecurityOriginData&, MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier);

    void terminateWebProcess(WebCore::ProcessIdentifier);

    void triggerBrowsingContextGroupSwitchForNavigation(WebPageProxyIdentifier, uint64_t navigationID, WebCore::BrowsingContextGroupSwitchDecision, const WebCore::RegistrableDomain& responseDomain, NetworkResourceLoadIdentifier existingNetworkResourceLoadIdentifierToResume, CompletionHandler<void(bool success)>&&);

    void requestStorageSpace(PAL::SessionID, const WebCore::ClientOrigin&, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(std::optional<uint64_t> quota)>&&);
    void increaseQuota(PAL::SessionID, const WebCore::ClientOrigin&, QuotaIncreaseRequestIdentifier, uint64_t currentQuota, uint64_t currentUsage, uint64_t spaceRequested);

    WebsiteDataStore* websiteDataStoreFromSessionID(PAL::SessionID);

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;
#if PLATFORM(COCOA)
    RefPtr<XPCEventHandler> xpcEventHandler() const override;
#endif

    void processAuthenticationChallenge(PAL::SessionID, Ref<AuthenticationChallengeProxy>&&);

#if USE(SOUP)
    void didExceedMemoryLimit();
#endif

    void applicationDidEnterBackground();
    void applicationWillEnterForeground();
#if PLATFORM(IOS_FAMILY)
    void addBackgroundStateObservers();
    void removeBackgroundStateObservers();
#endif

    std::unique_ptr<DownloadProxyMap> m_downloadProxyMap;

    UniqueRef<API::CustomProtocolManagerClient> m_customProtocolManagerClient;
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    LegacyCustomProtocolManagerProxy m_customProtocolManagerProxy;
#endif

    ProcessThrottler m_throttler;
    ProcessThrottler::ActivityVariant m_activityFromWebProcesses;

#if ENABLE(CONTENT_EXTENSIONS)
    WeakHashSet<WebUserContentControllerProxy> m_webUserContentControllerProxies;
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RefPtr<StorageAccessPromptQuirkObserver> m_storageAccessPromptQuirksDataUpdateObserver;
#endif

    struct UploadActivity {
        RefPtr<ProcessAssertion> uiAssertion;
        RefPtr<ProcessAssertion> networkAssertion;
        HashMap<WebCore::ProcessIdentifier, RefPtr<ProcessAssertion>> webProcessAssertions;
    };
    std::optional<UploadActivity> m_uploadActivity;

#if PLATFORM(COCOA)
    class XPCEventHandler : public WebKit::XPCEventHandler {
    public:
        XPCEventHandler(const NetworkProcessProxy&);

        bool handleXPCEvent(xpc_object_t) const override;

    private:
        WeakPtr<NetworkProcessProxy> m_networkProcess;
    };
    OSObjectPtr<xpc_object_t> m_endpointMessage;
#endif

    WeakHashSet<WebsiteDataStore> m_websiteDataStores;
    WeakHashMap<WebProcessProxy, std::pair<LoadedWebArchive, HashSet<WebCore::RegistrableDomain>>> m_allowedFirstPartiesForCookies;
    HashMap<DataTaskIdentifier, Ref<API::DataTask>> m_dataTasks;
#if PLATFORM(MAC)
    // On macOS, we prevent suspension of the NetworkProcess to avoid kills when holding
    // locked database files. The WebSQLiteDatabaseTracker is not functional on macOS
    // because the network process is not allowed to talk to talk to runningboardd due
    // to sandboxing. See rdar://112406083 & rdar://112086186 for potential long-term
    // fixes.
    UniqueRef<ProcessThrottlerActivity> m_backgroundActivityToPreventSuspension;
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<id> m_backgroundObserver;
    RetainPtr<id> m_foregroundObserver;
#endif
};

} // namespace WebKit
