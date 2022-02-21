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
#include "AuxiliaryProcess.h"
#include "CacheModel.h"
#include "DownloadID.h"
#include "DownloadManager.h"
#include "LocalStorageDatabaseTracker.h"
#include "NetworkContentRuleListManager.h"
#include "NetworkResourceLoadIdentifier.h"
#include "QuotaIncreaseRequestIdentifier.h"
#include "RTCDataChannelRemoteManagerProxy.h"
#include "SandboxExtension.h"
#include "WebPageProxyIdentifier.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/FetchIdentifier.h>
#include <WebCore/MessagePortChannelRegistry.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/StorageQuotaManager.h>
#include <memory>
#include <wtf/CrossThreadTask.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS_FAMILY)
#include "WebSQLiteDatabaseTracker.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/OSObjectPtr.h>
typedef struct OpaqueCFHTTPCookieStorage*  CFHTTPCookieStorageRef;
#endif

namespace IPC {
class FormDataReference;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class CertificateInfo;
class CurlProxySettings;
class ProtectionSpace;
class NetworkStorageSession;
class ResourceError;
class UserContentURLPattern;
enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeHttpOnlyCookies : bool;
enum class StoredCredentialsPolicy : uint8_t;
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : bool;
struct ClientOrigin;
struct MessageWithMessagePorts;
struct SecurityOriginData;
struct SoupNetworkProxySettings;
}

namespace WebKit {

class AuthenticationManager;
class NetworkConnectionToWebProcess;
class NetworkProcessSupplement;
class NetworkProximityManager;
class NetworkResourceLoader;
class ProcessAssertion;
class WebPageNetworkParameters;
enum class CallDownloadDidStart : bool;
enum class ShouldGrandfatherStatistics : bool;
enum class StorageAccessStatus : uint8_t;
enum class WebsiteDataFetchOption : uint8_t;
enum class WebsiteDataType : uint32_t;
struct NetworkProcessCreationParameters;
struct WebPushMessage;
struct WebsiteDataStoreParameters;

namespace NetworkCache {
enum class CacheOption : uint8_t;
}

namespace CacheStorage {
class Engine;
}

class NetworkProcess : public AuxiliaryProcess, private DownloadManager::Client, public ThreadSafeRefCounted<NetworkProcess>
{
    WTF_MAKE_NONCOPYABLE(NetworkProcess);
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

    NetworkProcess(AuxiliaryProcessInitializationParameters&&);
    ~NetworkProcess();
    static constexpr WebCore::AuxiliaryProcessType processType = WebCore::AuxiliaryProcessType::Network;

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    void addSupplement()
    {
        m_supplements.add(T::supplementName(), makeUnique<T>(*this));
    }

    void removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess&);

    AuthenticationManager& authenticationManager();
    DownloadManager& downloadManager();

    void setSession(PAL::SessionID, std::unique_ptr<NetworkSession>&&);
    NetworkSession* networkSession(PAL::SessionID) const final;
    void destroySession(PAL::SessionID);

    void forEachNetworkSession(const Function<void(NetworkSession&)>&);

    void forEachNetworkStorageSession(const Function<void(WebCore::NetworkStorageSession&)>&);
    WebCore::NetworkStorageSession* storageSession(PAL::SessionID) const;
    std::unique_ptr<WebCore::NetworkStorageSession> newTestingSession(PAL::SessionID);
#if PLATFORM(COCOA)
    void ensureSession(PAL::SessionID, bool shouldUseTestingNetworkSession, const String& identifier, RetainPtr<CFHTTPCookieStorageRef>&&);
#else
    void ensureSession(PAL::SessionID, bool shouldUseTestingNetworkSession, const String& identifier);
#endif

    void processWillSuspendImminentlyForTestingSync(CompletionHandler<void()>&&);
    void prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&&);
    void processDidResume();
    void resume();

    CacheModel cacheModel() const { return m_cacheModel; }

    // Diagnostic messages logging.
    void logDiagnosticMessage(WebPageProxyIdentifier, const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(WebPageProxyIdentifier, const String& message, const String& description, WebCore::DiagnosticLoggingResultType, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(WebPageProxyIdentifier, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> sourceApplicationAuditData() const;
    std::optional<audit_token_t> sourceApplicationAuditToken() const;
#endif
#if PLATFORM(COCOA) || USE(SOUP)
    HashSet<String> hostNamesWithHSTSCache(PAL::SessionID) const;
    void deleteHSTSCacheForHostNames(PAL::SessionID, const Vector<String>&);
    void clearHSTSCache(PAL::SessionID, WallTime modifiedSince);
#endif

    void findPendingDownloadLocation(NetworkDataTask&, ResponseCompletionHandler&&, const WebCore::ResourceResponse&);

    void prefetchDNS(const String&);

    void addWebsiteDataStore(WebsiteDataStoreParameters&&);

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    void clearPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void clearUserInteraction(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void deleteAndRestrictWebsiteDataForRegistrableDomains(PAL::SessionID, OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&&);
    void deleteCookiesForTesting(PAL::SessionID, RegistrableDomain, bool includeHttpOnlyCookies, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(PAL::SessionID, CompletionHandler<void(String)>&&);
    void updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID, const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&&);
    void isGrandfathered(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void setAgeCapForClientSideCookies(PAL::SessionID, std::optional<Seconds>, CompletionHandler<void()>&&);
    void isRegisteredAsRedirectingTo(PAL::SessionID, const RedirectedFromDomain&, const RedirectedToDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(PAL::SessionID, const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(PAL::SessionID, const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void setGrandfathered(PAL::SessionID, const RegistrableDomain&, bool isGrandfathered, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(PAL::SessionID, uint64_t maximumEntryCount, CompletionHandler<void()>&&);
    void setPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void()>&&);
    void setPruneEntriesDownTo(PAL::SessionID, uint64_t pruneTargetCount, CompletionHandler<void()>&&);
    void hadUserInteraction(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isRelationshipOnlyInDatabaseOnce(PAL::SessionID, const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&&);
    void hasLocalStorage(PAL::SessionID, const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void getAllStorageAccessEntries(PAL::SessionID, CompletionHandler<void(Vector<String> domains)>&&);
    void logFrameNavigation(PAL::SessionID, const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser);
    void logUserInteraction(PAL::SessionID, const TopFrameDomain&, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID, CompletionHandler<void()>&&);
    void resetParametersToDefaultValues(PAL::SessionID, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(PAL::SessionID, std::optional<WallTime> modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void getResourceLoadStatisticsDataSummary(PAL::SessionID, CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&&);
    void scheduleCookieBlockingUpdate(PAL::SessionID, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(PAL::SessionID, CompletionHandler<void()>&&);
    void statisticsDatabaseHasAllTables(PAL::SessionID, CompletionHandler<void(bool)>&&);
    void setCacheMaxAgeCapForPrevalentResources(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setGrandfatheringTime(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setLastSeen(PAL::SessionID, const RegistrableDomain&, Seconds, CompletionHandler<void()>&&);
    void domainIDExistsInDatabase(PAL::SessionID, int domainID, CompletionHandler<void(bool)>&&);
    void mergeStatisticForTesting(PAL::SessionID, const RegistrableDomain&, const TopFrameDomain& topFrameDomain1, const TopFrameDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&&);
    void insertExpiredStatisticForTesting(PAL::SessionID, const RegistrableDomain&, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);
    void setMinimumTimeBetweenDataRecordsRemoval(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void setIsRunningResourceLoadStatisticsTest(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsEnabled(PAL::SessionID, bool);
    void setResourceLoadStatisticsLogTestingEvent(bool);
    void setResourceLoadStatisticsDebugMode(PAL::SessionID, bool debugMode, CompletionHandler<void()>&&d);
    void isResourceLoadStatisticsEphemeral(PAL::SessionID, CompletionHandler<void(bool)>&&) const;
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(PAL::SessionID, const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(PAL::SessionID, const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(PAL::SessionID, const SubResourceDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(PAL::SessionID, const SubResourceDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void setTimeToLiveUserInteraction(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(PAL::SessionID, const TopFrameDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(PAL::SessionID, const TopFrameDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void registrableDomainsWithWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&);
    void didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag>, WebPageProxyIdentifier, WebCore::PageIdentifier);
    void setCrossSiteLoadWithLinkDecorationForTesting(PAL::SessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, CompletionHandler<void()>&&);
    void resetCrossSiteLoadsWithLinkDecorationForTesting(PAL::SessionID, CompletionHandler<void()>&&);
    void hasIsolatedSession(PAL::SessionID, const WebCore::RegistrableDomain&, CompletionHandler<void(bool)>&&) const;
    void closeITPDatabase(PAL::SessionID, CompletionHandler<void()>&&);
#if ENABLE(APP_BOUND_DOMAINS)
    void setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID, HashSet<WebCore::RegistrableDomain>&&, CompletionHandler<void()>&&);
#endif
    void setShouldDowngradeReferrerForTesting(bool, CompletionHandler<void()>&&);
    void setThirdPartyCookieBlockingMode(PAL::SessionID, WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID, WebCore::SameSiteStrictEnforcementEnabled, CompletionHandler<void()>&&);
    void setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID, WebCore::FirstPartyWebsiteDataRemovalMode, CompletionHandler<void()>&&);
    void setToSameSiteStrictCookiesForTesting(PAL::SessionID, const WebCore::RegistrableDomain&, CompletionHandler<void()>&&);
    void setFirstPartyHostCNAMEDomainForTesting(PAL::SessionID, String&& firstPartyHost, WebCore::RegistrableDomain&& cnameDomain, CompletionHandler<void()>&&);
    void setThirdPartyCNAMEDomainForTesting(PAL::SessionID, WebCore::RegistrableDomain&&, CompletionHandler<void()>&&);
#endif

    void setPrivateClickMeasurementEnabled(bool);
    bool privateClickMeasurementEnabled() const;
    void setPrivateClickMeasurementDebugMode(PAL::SessionID, bool);

    void preconnectTo(PAL::SessionID, WebPageProxyIdentifier, WebCore::PageIdentifier, const URL&, const String&, WebCore::StoredCredentialsPolicy, std::optional<NavigatingToAppBoundDomain>, LastNavigationWasAppInitiated);

    void setSessionIsControlledByAutomation(PAL::SessionID, bool);
    bool sessionIsControlledByAutomation(PAL::SessionID) const;

    void connectionToWebProcessClosed(IPC::Connection&, PAL::SessionID);

#if ENABLE(CONTENT_EXTENSIONS)
    NetworkContentRuleListManager& networkContentRuleListManager() { return m_networkContentRuleListManager; }
#endif

    void syncLocalStorage(CompletionHandler<void()>&&);

    void resetQuota(PAL::SessionID, CompletionHandler<void()>&&);
    void clearStorage(PAL::SessionID, CompletionHandler<void()>&&);
    void didIncreaseQuota(PAL::SessionID, const WebCore::ClientOrigin&, QuotaIncreaseRequestIdentifier, std::optional<uint64_t> newQuota);
    void renameOriginInWebsiteData(PAL::SessionID, const URL&, const URL&, OptionSet<WebsiteDataType>, CompletionHandler<void()>&&);

#if PLATFORM(IOS_FAMILY)
    bool parentProcessHasServiceWorkerEntitlement() const;
    void disableServiceWorkerEntitlement();
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&&);
#else
    bool parentProcessHasServiceWorkerEntitlement() const { return true; }
#endif

    const String& uiProcessBundleIdentifier() const { return m_uiProcessBundleIdentifier; }

    void ref() const override { ThreadSafeRefCounted<NetworkProcess>::ref(); }
    void deref() const override { ThreadSafeRefCounted<NetworkProcess>::deref(); }

    void requestStorageSpace(PAL::SessionID, const WebCore::ClientOrigin&, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(std::optional<uint64_t>)>&&);

    void storePrivateClickMeasurement(PAL::SessionID, WebCore::PrivateClickMeasurement&&);
    void dumpPrivateClickMeasurement(PAL::SessionID, CompletionHandler<void(String)>&&);
    void clearPrivateClickMeasurement(PAL::SessionID, CompletionHandler<void()>&&);
    bool allowsPrivateClickMeasurementTestFunctionality() const;
    void setPrivateClickMeasurementOverrideTimerForTesting(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementEphemeralMeasurementForTesting(PAL::SessionID, bool value, CompletionHandler<void()>&&);
    void simulatePrivateClickMeasurementSessionRestart(PAL::SessionID, CompletionHandler<void()>&&);
    void closePCMDatabase(PAL::SessionID, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenPublicKeyURLForTesting(PAL::SessionID, URL&&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenSignatureURLForTesting(PAL::SessionID, URL&&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAttributionReportURLsForTesting(PAL::SessionID, URL&& sourceURL, URL&& destinationURL, CompletionHandler<void()>&&);
    void markPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID, CompletionHandler<void()>&&);
    void setPCMFraudPreventionValuesForTesting(PAL::SessionID, String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAppBundleIDForTesting(PAL::SessionID, String&& appBundleIDForTesting, CompletionHandler<void()>&&);

    RefPtr<WebCore::StorageQuotaManager> storageQuotaManager(PAL::SessionID, const WebCore::ClientOrigin&);

    void addKeptAliveLoad(Ref<NetworkResourceLoader>&&);
    void removeKeptAliveLoad(NetworkResourceLoader&);

    const OptionSet<NetworkCache::CacheOption>& cacheOptions() const { return m_cacheOptions; }

    NetworkConnectionToWebProcess* webProcessConnection(WebCore::ProcessIdentifier) const;
    WebCore::MessagePortChannelRegistry& messagePortChannelRegistry() { return m_messagePortChannelRegistry; }

    void setServiceWorkerFetchTimeoutForTesting(Seconds, CompletionHandler<void()>&&);
    void resetServiceWorkerFetchTimeoutForTesting(CompletionHandler<void()>&&);
    Seconds serviceWorkerFetchTimeout() const { return m_serviceWorkerFetchTimeout; }

    static Seconds randomClosedPortDelay();

    void cookieAcceptPolicyChanged(WebCore::HTTPCookieAcceptPolicy);

#if ENABLE(APP_BOUND_DOMAINS)
    void hasAppBoundSession(PAL::SessionID, CompletionHandler<void(bool)>&&) const;
    void clearAppBoundSession(PAL::SessionID, CompletionHandler<void()>&&);
#endif

    void broadcastConsoleMessage(PAL::SessionID, JSC::MessageSource, JSC::MessageLevel, const String& message);
    void updateBundleIdentifier(String&&, CompletionHandler<void()>&&);
    void clearBundleIdentifier(CompletionHandler<void()>&&);

    bool shouldDisableCORSForRequestTo(WebCore::PageIdentifier, const URL&) const;
    void setCORSDisablingPatterns(WebCore::PageIdentifier, Vector<String>&&);

#if PLATFORM(COCOA)
    void appPrivacyReportTestingData(PAL::SessionID, CompletionHandler<void(const AppPrivacyReportTestingData&)>&&);
    void clearAppPrivacyReportTestingData(PAL::SessionID, CompletionHandler<void()>&&);
#endif

#if ENABLE(WEB_RTC)
    RTCDataChannelRemoteManagerProxy& rtcDataChannelProxy();
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif

    bool ftpEnabled() const { return m_ftpEnabled; }

#if ENABLE(SERVICE_WORKER)
    void getPendingPushMessages(PAL::SessionID, CompletionHandler<void(const Vector<WebPushMessage>&)>&&);
    void processPushMessage(PAL::SessionID, WebPushMessage&&, CompletionHandler<void(bool)>&&);
#endif

    void deletePushAndNotificationRegistration(PAL::SessionID, const WebCore::SecurityOriginData&, CompletionHandler<void(const String&)>&&);
    void getOriginsWithPushAndNotificationPermissions(PAL::SessionID, CompletionHandler<void(const Vector<WebCore::SecurityOriginData>&)>&&);

    void setSessionStorageQuotaManagerIDBRootPath(PAL::SessionID, const String& idbRootPath);

private:
    void platformInitializeNetworkProcess(const NetworkProcessCreationParameters&);

    void didReceiveNetworkProcessMessage(IPC::Connection&, IPC::Decoder&);

    void terminate() override;
    void platformTerminate();

    void lowMemoryHandler(Critical);
    
    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    void initializeConnection(IPC::Connection*) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;

    // DownloadManager::Client
    void didCreateDownload() override;
    void didDestroyDownload() override;
    IPC::Connection* downloadProxyConnection() override;
    IPC::Connection* parentProcessConnectionForDownloads() override { return parentProcessConnection(); }
    AuthenticationManager& downloadsAuthenticationManager() override;

    // Message Handlers
    bool didReceiveSyncNetworkProcessMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void initializeNetworkProcess(NetworkProcessCreationParameters&&);
    void createNetworkConnectionToWebProcess(WebCore::ProcessIdentifier, PAL::SessionID, CompletionHandler<void(std::optional<IPC::Attachment>&&, WebCore::HTTPCookieAcceptPolicy)>&&);

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, CompletionHandler<void(WebsiteData&&)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostnames, const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);

    void clearCachedCredentials(PAL::SessionID);

    void initializeQuotaUsers(WebCore::StorageQuotaManager&, PAL::SessionID, const WebCore::ClientOrigin&);

    // FIXME: This should take a session ID so we can identify which disk cache to delete.
    void clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&&);

    void downloadRequest(PAL::SessionID, DownloadID, const WebCore::ResourceRequest&, std::optional<NavigatingToAppBoundDomain>, const String& suggestedFilename);
    void resumeDownload(PAL::SessionID, DownloadID, const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&&, CallDownloadDidStart);
    void cancelDownload(DownloadID, CompletionHandler<void(const IPC::DataReference&)>&&);
#if PLATFORM(COCOA)
    void publishDownloadProgress(DownloadID, const URL&, SandboxExtension::Handle&&);
#endif
    void continueWillSendRequest(DownloadID, WebCore::ResourceRequest&&);
    void requestResource(WebPageProxyIdentifier, PAL::SessionID, WebCore::ResourceRequest&&, IPC::FormDataReference&&, CompletionHandler<void(IPC::DataReference, WebCore::ResourceResponse, WebCore::ResourceError)>&&);
    void applicationDidEnterBackground();
    void applicationWillEnterForeground();

    void setCacheModel(CacheModel);
    void setCacheModelSynchronouslyForTesting(CacheModel, CompletionHandler<void()>&&);
    void allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo&, const String& host);
    void allowTLSCertificateChainForLocalPCMTesting(PAL::SessionID, const WebCore::CertificateInfo&);
    void setAllowsAnySSLCertificateForWebSocket(bool, CompletionHandler<void()>&&);

    void flushCookies(PAL::SessionID, CompletionHandler<void()>&&);

    void addWebPageNetworkParameters(PAL::SessionID, WebPageProxyIdentifier, WebPageNetworkParameters&&);
    void removeWebPageNetworkParameters(PAL::SessionID, WebPageProxyIdentifier);
    void countNonDefaultSessionSets(PAL::SessionID, CompletionHandler<void(size_t)>&&);

#if USE(SOUP)
    void setIgnoreTLSErrors(PAL::SessionID, bool);
    void userPreferredLanguagesChanged(const Vector<String>&);
    void setNetworkProxySettings(PAL::SessionID, WebCore::SoupNetworkProxySettings&&);
    void setPersistentCredentialStorageEnabled(PAL::SessionID, bool);
#endif

#if USE(CURL)
    void setNetworkProxySettings(PAL::SessionID, WebCore::CurlProxySettings&&);
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    static void setSharedHTTPCookieStorage(const Vector<uint8_t>& identifier);
#endif

    void platformFlushCookies(PAL::SessionID, CompletionHandler<void()>&&);
    
    void registerURLSchemeAsSecure(const String&) const;
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
    void registerURLSchemeAsNoAccess(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&) const;

#if PLATFORM(IOS_FAMILY)
    void setIsHoldingLockedFiles(bool);
#endif

    class SessionStorageQuotaManager {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        SessionStorageQuotaManager() = delete;
        SessionStorageQuotaManager(const String& cacheRootPath, uint64_t defaultQuota, uint64_t defaultThirdPartyQuota)
            : m_cacheRootPath(cacheRootPath)
            , m_defaultQuota(defaultQuota)
            , m_defaultThirdPartyQuota(defaultThirdPartyQuota)
        {
        }
        uint64_t defaultQuota(const WebCore::ClientOrigin& origin) const { return origin.topOrigin == origin.clientOrigin ? m_defaultQuota : m_defaultThirdPartyQuota; }

        Ref<WebCore::StorageQuotaManager> ensureOriginStorageQuotaManager(WebCore::ClientOrigin origin, uint64_t quota, WebCore::StorageQuotaManager::UsageGetter&& usageGetter, WebCore::StorageQuotaManager::QuotaIncreaseRequester&& quotaIncreaseRequester)
        {
            auto iter = m_storageQuotaManagers.ensure(origin, [quota, usageGetter = WTFMove(usageGetter), quotaIncreaseRequester = WTFMove(quotaIncreaseRequester)]() mutable {
                return WebCore::StorageQuotaManager::create(quota, WTFMove(usageGetter), WTFMove(quotaIncreaseRequester));
            }).iterator;
            return *iter->value;
        }

        auto existingStorageQuotaManagers() { return m_storageQuotaManagers.values(); }

        const String& cacheRootPath() const { return m_cacheRootPath; }

        void setIDBRootPath(const String& idbRootPath) { m_idbRootPath = idbRootPath; }
        const String& idbRootPath() const { return m_idbRootPath; }

    private:
        String m_cacheRootPath;
        String m_idbRootPath;
        uint64_t m_defaultQuota;
        uint64_t m_defaultThirdPartyQuota;
        HashMap<WebCore::ClientOrigin, RefPtr<WebCore::StorageQuotaManager>> m_storageQuotaManagers;
    };
    void addSessionStorageQuotaManager(PAL::SessionID, uint64_t defaultQuota, uint64_t defaultThirdPartyQuota, const String& cacheRootPath, SandboxExtension::Handle&);
    void removeSessionStorageQuotaManager(PAL::SessionID);

    // Connections to WebProcesses.
    HashMap<WebCore::ProcessIdentifier, Ref<NetworkConnectionToWebProcess>> m_webProcessConnections;

    bool m_hasSetCacheModel { false };
    CacheModel m_cacheModel { CacheModel::DocumentViewer };
    bool m_suppressMemoryPressureHandler { false };
    String m_uiProcessBundleIdentifier;
    DownloadManager m_downloadManager;

    typedef HashMap<const char*, std::unique_ptr<NetworkProcessSupplement>, PtrHash<const char*>> NetworkProcessSupplementMap;
    NetworkProcessSupplementMap m_supplements;

    HashSet<PAL::SessionID> m_sessionsControlledByAutomation;

    HashMap<PAL::SessionID, std::unique_ptr<NetworkSession>> m_networkSessions;
    HashMap<PAL::SessionID, std::unique_ptr<WebCore::NetworkStorageSession>> m_networkStorageSessions;

#if PLATFORM(COCOA)
    void platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters&);

    // FIXME: We'd like to be able to do this without the #ifdef, but WorkQueue + BinarySemaphore isn't good enough since
    // multiple requests to clear the cache can come in before previous requests complete, and we need to wait for all of them.
    // In the future using WorkQueue and a counting semaphore would work, as would WorkQueue supporting the libdispatch concept of "work groups".
    OSObjectPtr<dispatch_group_t> m_clearCacheDispatchGroup;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    NetworkContentRuleListManager m_networkContentRuleListManager;
#endif

#if PLATFORM(IOS_FAMILY)
    WebSQLiteDatabaseTracker m_webSQLiteDatabaseTracker;
    RefPtr<ProcessAssertion> m_holdingLockedFileAssertion;
#endif
    
#if ENABLE(WEB_RTC)
    RefPtr<RTCDataChannelRemoteManagerProxy> m_rtcDataChannelProxy;
#endif

    Lock m_sessionStorageQuotaManagersLock;
    HashMap<PAL::SessionID, std::unique_ptr<SessionStorageQuotaManager>> m_sessionStorageQuotaManagers WTF_GUARDED_BY_LOCK(m_sessionStorageQuotaManagersLock);
    bool m_quotaLoggingEnabled { false };

    OptionSet<NetworkCache::CacheOption> m_cacheOptions;
    WebCore::MessagePortChannelRegistry m_messagePortChannelRegistry;

    static const Seconds defaultServiceWorkerFetchTimeout;
    Seconds m_serviceWorkerFetchTimeout { defaultServiceWorkerFetchTimeout };

    HashMap<WebCore::PageIdentifier, Vector<WebCore::UserContentURLPattern>> m_extensionCORSDisablingPatterns;

    bool m_privateClickMeasurementEnabled { true };
    bool m_ftpEnabled { false };
};

} // namespace WebKit
