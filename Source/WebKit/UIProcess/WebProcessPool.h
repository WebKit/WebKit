/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "APIDictionary.h"
#include "APIObject.h"
#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataStore.h"
#include "DownloadProxyMap.h"
#include "GenericCallback.h"
#include "HiddenPageThrottlingAutoIncreasesCounter.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "PlugInAutoStartProvider.h"
#include "PluginInfoStore.h"
#include "ProcessThrottler.h"
#include "ServiceWorkerProcessProxy.h"
#include "StatisticsRequest.h"
#include "VisitedLinkStore.h"
#include "WebContextClient.h"
#include "WebContextConnectionClient.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SharedStringHash.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCounter.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if ENABLE(MEDIA_SESSION)
#include "WebMediaSessionFocusManager.h"
#endif

#if USE(SOUP)
#include <WebCore/SoupNetworkProxySettings.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS NSString;
#endif

namespace API {
class AutomationClient;
class CustomProtocolManagerClient;
class DownloadClient;
class HTTPCookieStore;
class InjectedBundleClient;
class LegacyContextHistoryClient;
class Navigation;
class PageConfiguration;
}

namespace WebCore {
struct MockMediaDevice;
}

namespace WebKit {

class DownloadProxy;
class HighPerformanceGraphicsUsageSampler;
class UIGamepad;
class PerActivityStateCPUUsageSampler;
class ServiceWorkerProcessProxy;
class WebAutomationSession;
class WebContextSupplement;
class WebPageGroup;
class WebPageProxy;
struct NetworkProcessCreationParameters;
struct StatisticsData;
struct WebProcessCreationParameters;
    
typedef GenericCallback<API::Dictionary*> DictionaryCallback;

#if PLATFORM(COCOA)
int networkProcessLatencyQOS();
int networkProcessThroughputQOS();
int webProcessLatencyQOS();
int webProcessThroughputQOS();
#endif

enum class ProcessSwapRequestedByClient;

class WebProcessPool final : public API::ObjectImpl<API::Object::Type::ProcessPool>, public CanMakeWeakPtr<WebProcessPool>, private IPC::MessageReceiver {
public:
    static Ref<WebProcessPool> create(API::ProcessPoolConfiguration&);

    explicit WebProcessPool(API::ProcessPoolConfiguration&);        
    virtual ~WebProcessPool();

    void notifyThisWebProcessPoolWasCreated();

    API::ProcessPoolConfiguration& configuration() { return m_configuration.get(); }

    static const Vector<WebProcessPool*>& allProcessPools();

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    void addSupplement()
    {
        m_supplements.add(T::supplementName(), T::create(this));
    }

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    void initializeClient(const WKContextClientBase*);
    void setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&&);
    void initializeConnectionClient(const WKContextConnectionClientBase*);
    void setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>&&);
    void setDownloadClient(std::unique_ptr<API::DownloadClient>&&);
    void setAutomationClient(std::unique_ptr<API::AutomationClient>&&);
    void setLegacyCustomProtocolManagerClient(std::unique_ptr<API::CustomProtocolManagerClient>&&);

    void setMaximumNumberOfProcesses(unsigned); // Can only be called when there are no processes running.
    unsigned maximumNumberOfProcesses() const { return !m_configuration->maximumProcessCount() ? UINT_MAX : m_configuration->maximumProcessCount(); }

    void setCustomWebContentServiceBundleIdentifier(const String&);
    const String& customWebContentServiceBundleIdentifier() { return m_configuration->customWebContentServiceBundleIdentifier(); }

    const Vector<RefPtr<WebProcessProxy>>& processes() const { return m_processes; }

    // WebProcess or NetworkProcess as approporiate for current process model. The connection must be non-null.
    IPC::Connection* networkingProcessConnection();

    template<typename T> void sendToAllProcesses(const T& message);
    template<typename T> void sendToAllProcessesRelaunchingThemIfNecessary(const T& message);
    template<typename T> void sendToOneProcess(T&& message);

    // Sends the message to WebProcess or NetworkProcess as approporiate for current process model.
    template<typename T> void sendToNetworkingProcess(T&& message);
    template<typename T, typename U> void sendSyncToNetworkingProcess(T&& message, U&& reply);
    template<typename T> void sendToNetworkingProcessRelaunchingIfNecessary(T&& message);

    void processDidFinishLaunching(WebProcessProxy*);

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    API::WebsiteDataStore* websiteDataStore() const { return m_websiteDataStore.get(); }
    void setPrimaryDataStore(API::WebsiteDataStore& dataStore) { m_websiteDataStore = &dataStore; }

    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    void pageBeginUsingWebsiteDataStore(WebPageProxy&);
    void pageEndUsingWebsiteDataStore(WebPageProxy&);

    const String& injectedBundlePath() const { return m_configuration->injectedBundlePath(); }

    DownloadProxy* download(WebPageProxy* initiatingPage, const WebCore::ResourceRequest&, const String& suggestedFilename = { });
    DownloadProxy* resumeDownload(const API::Data* resumeData, const String& path);

    void setInjectedBundleInitializationUserData(RefPtr<API::Object>&& userData) { m_injectedBundleInitializationUserData = WTFMove(userData); }

    void postMessageToInjectedBundle(const String&, API::Object*);

    void populateVisitedLinks();

    void handleMemoryPressureWarning(Critical);

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginsDirectory(const String&);
    void refreshPlugins();

    PluginInfoStore& pluginInfoStore() { return m_pluginInfoStore; }

    void setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString);
    void resetPluginLoadClientPolicies(HashMap<String, HashMap<String, HashMap<String, uint8_t>>>&&);
    void clearPluginClientPolicies();
    const HashMap<String, HashMap<String, HashMap<String, uint8_t>>>& pluginLoadClientPolicies() const { return m_pluginLoadClientPolicies; }
#endif

    void addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions);
    void clearSupportedPlugins();

    ProcessID networkProcessIdentifier();
    Vector<String> activePagesOriginsInWebProcessForTesting(ProcessID);

    WebPageGroup& defaultPageGroup() { return m_defaultPageGroup.get(); }

    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);
    void setCanHandleHTTPSServerTrustEvaluation(bool);
    void registerURLSchemeAsLocal(const String&);
    void registerURLSchemeAsNoAccess(const String&);
    void registerURLSchemeAsDisplayIsolated(const String&);
    void registerURLSchemeAsCORSEnabled(const String&);
    void registerURLSchemeAsCachePartitioned(const String&);
    void registerURLSchemeServiceWorkersCanHandle(const String&);
    void registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String&);

    void preconnectToServer(const WebCore::URL&);

    VisitedLinkStore& visitedLinkStore() { return m_visitedLinkStore.get(); }

    void setCacheModel(CacheModel);
    CacheModel cacheModel() const { return m_configuration->cacheModel(); }

    void setDefaultRequestTimeoutInterval(double);

    void startMemorySampler(const double interval);
    void stopMemorySampler();

#if USE(SOUP)
    void setInitialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy) { m_initialHTTPCookieAcceptPolicy = policy; }
    void setNetworkProxySettings(const WebCore::SoupNetworkProxySettings&);
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy* createDownloadProxy(const WebCore::ResourceRequest&, WebPageProxy* originatingPage);
    API::DownloadClient& downloadClient() { return *m_downloadClient; }

    API::LegacyContextHistoryClient& historyClient() { return *m_historyClient; }
    WebContextClient& client() { return m_client; }

    API::CustomProtocolManagerClient& customProtocolManagerClient() const { return *m_customProtocolManagerClient; }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void useTestingNetworkSession();
    bool isUsingTestingNetworkSession() const { return m_shouldUseTestingNetworkSession; }

    void setAllowsAnySSLCertificateForWebSocket(bool);

    void clearCachedCredentials();
    void terminateNetworkProcess();
    void terminateServiceWorkerProcesses();
    void disableServiceWorkerProcessTerminationDelay();

    void syncNetworkProcessCookies();

    void setShouldMakeNextWebProcessLaunchFailForTesting(bool value) { m_shouldMakeNextWebProcessLaunchFailForTesting = value; }
    bool shouldMakeNextWebProcessLaunchFailForTesting() const { return m_shouldMakeNextWebProcessLaunchFailForTesting; }
    void setShouldMakeNextNetworkProcessLaunchFailForTesting(bool value) { m_shouldMakeNextNetworkProcessLaunchFailForTesting = value; }
    bool shouldMakeNextNetworkProcessLaunchFailForTesting() const { return m_shouldMakeNextNetworkProcessLaunchFailForTesting; }

    void reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState);

    void allowSpecificHTTPSCertificateForHost(const WebCertificateInfo*, const String& host);

    WebProcessProxy& createNewWebProcessRespectingProcessCountLimit(WebsiteDataStore&); // Will return an existing one if limit is met.
    void prewarmProcess();

    bool shouldTerminate(WebProcessProxy*);

    void disableProcessTermination() { m_processTerminationEnabled = false; }
    void enableProcessTermination();

    void updateAutomationCapabilities() const;
    void setAutomationSession(RefPtr<WebAutomationSession>&&);
    WebAutomationSession* automationSession() const { return m_automationSession.get(); }

    // Defaults to false.
    void setHTTPPipeliningEnabled(bool);
    bool httpPipeliningEnabled() const;

    void getStatistics(uint32_t statisticsMask, Function<void (API::Dictionary*, CallbackBase::Error)>&&);
    
    bool javaScriptConfigurationFileEnabled() { return m_javaScriptConfigurationFileEnabled; }
    void setJavaScriptConfigurationFileEnabled(bool flag);
#if PLATFORM(IOS_FAMILY)
    void setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

#if PLATFORM(COCOA)
    static bool omitPDFSupport();
#endif

    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);
#if OS(LINUX)
    void sendMemoryPressureEvent(bool isCritical);
#endif
    void textCheckerStateChanged();

    Ref<API::Dictionary> plugInAutoStartOriginHashes() const;
    void setPlugInAutoStartOriginHashes(API::Dictionary&);
    void setPlugInAutoStartOrigins(API::Array&);
    void setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary&, WallTime);

    // Network Process Management
    NetworkProcessProxy& ensureNetworkProcess(WebsiteDataStore* withWebsiteDataStore = nullptr);
    NetworkProcessProxy* networkProcess() { return m_networkProcess.get(); }
    void networkProcessCrashed(NetworkProcessProxy&, Vector<std::pair<RefPtr<WebProcessProxy>, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>>&&);

    void getNetworkProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&&);

#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(NetworkProcessProxy&, WebCore::SecurityOriginData&&, std::optional<PAL::SessionID>);
    ServiceWorkerProcessProxy* serviceWorkerProcessProxyFromPageID(uint64_t pageID) const;
    const HashMap<WebCore::SecurityOriginData, ServiceWorkerProcessProxy*>& serviceWorkerProxies() const { return m_serviceWorkerProcesses; }
    void setAllowsAnySSLCertificateForServiceWorker(bool allows) { m_allowsAnySSLCertificateForServiceWorker = allows; }
    bool allowsAnySSLCertificateForServiceWorker() const { return m_allowsAnySSLCertificateForServiceWorker; }
    void updateServiceWorkerUserAgent(const String& userAgent);
    bool mayHaveRegisteredServiceWorkers(const WebsiteDataStore&);
#endif

#if PLATFORM(COCOA)
    bool processSuppressionEnabled() const;
#endif

    void windowServerConnectionStateChanged();

    static void willStartUsingPrivateBrowsing();
    static void willStopUsingPrivateBrowsing();

#if USE(SOUP)
    void setIgnoreTLSErrors(bool);
    bool ignoreTLSErrors() const { return m_ignoreTLSErrors; }
#endif

    static void setInvalidMessageCallback(void (*)(WKStringRef));
    static void didReceiveInvalidMessage(const IPC::StringReference& messageReceiverName, const IPC::StringReference& messageName);

    void processDidCachePage(WebProcessProxy*);

    bool isURLKnownHSTSHost(const String& urlString, bool privateBrowsingEnabled) const;
    void resetHSTSHosts();
    void resetHSTSHostsAddedAfterDate(double startDateIntervalSince1970);

    void registerSchemeForCustomProtocol(const String&);
    void unregisterSchemeForCustomProtocol(const String&);

    static void registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);
    static void unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);

#if PLATFORM(COCOA)
    void updateProcessSuppressionState();

    NSMutableDictionary *ensureBundleParameters();
    NSMutableDictionary *bundleParameters() { return m_bundleParameters.get(); }
#else
    void updateProcessSuppressionState() const { }
#endif

    void updateHiddenPageThrottlingAutoIncreaseLimit();

    void setMemoryCacheDisabled(bool);
    void setFontWhitelist(API::Array*);

    UserObservablePageCounter::Token userObservablePageCount()
    {
        return m_userObservablePageCounter.count();
    }

    ProcessSuppressionDisabledToken processSuppressionDisabledForPageCount()
    {
        return m_processSuppressionDisabledForPageCounter.count();
    }

    HiddenPageThrottlingAutoIncreasesCounter::Token hiddenPageThrottlingAutoIncreasesCount()
    {
        return m_hiddenPageThrottlingAutoIncreasesCounter.count();
    }

    void setResourceLoadStatisticsEnabled(bool);
    void clearResourceLoadStatistics();

    bool alwaysRunsAtBackgroundPriority() const { return m_alwaysRunsAtBackgroundPriority; }
    bool shouldTakeUIBackgroundAssertion() const { return m_shouldTakeUIBackgroundAssertion; }

#if ENABLE(GAMEPAD)
    void gamepadConnected(const UIGamepad&);
    void gamepadDisconnected(const UIGamepad&);

    void setInitialConnectedGamepads(const Vector<std::unique_ptr<UIGamepad>>&);
#endif

#if PLATFORM(COCOA)
    bool cookieStoragePartitioningEnabled() const { return m_cookieStoragePartitioningEnabled; }
    void setCookieStoragePartitioningEnabled(bool);
    bool storageAccessAPIEnabled() const { return m_storageAccessAPIEnabled; }
    void setStorageAccessAPIEnabled(bool);
#endif

#if ENABLE(SERVICE_WORKER)
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin);
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&&, const WebCore::ServiceWorkerOrClientIdentifier& source, WebCore::SWServerConnectionIdentifier);
#endif

    static uint64_t registerProcessPoolCreationListener(Function<void(WebProcessPool&)>&&);
    static void unregisterProcessPoolCreationListener(uint64_t identifier);

#if PLATFORM(IOS_FAMILY)
    ForegroundWebProcessToken foregroundWebProcessToken() const { return ForegroundWebProcessToken(m_foregroundWebProcessCounter.count()); }
    BackgroundWebProcessToken backgroundWebProcessToken() const { return BackgroundWebProcessToken(m_backgroundWebProcessCounter.count()); }
#endif

    Ref<WebProcessProxy> processForNavigation(WebPageProxy&, const API::Navigation&, ProcessSwapRequestedByClient, WebCore::PolicyAction&, String& reason);

    // SuspendedPageProxy management.
    void addSuspendedPageProxy(std::unique_ptr<SuspendedPageProxy>&&);
    void removeAllSuspendedPageProxiesForPage(WebPageProxy&);
    std::unique_ptr<SuspendedPageProxy> takeSuspendedPageProxy(SuspendedPageProxy&);
    bool hasSuspendedPageProxyFor(WebProcessProxy&) const;

    void didReachGoodTimeToPrewarm();

    void didCollectPrewarmInformation(const String& registrableDomain, const WebCore::PrewarmInformation&);

    void screenPropertiesStateChanged();

    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String& persistentId);
    void resetMockMediaDevices();

    void sendDisplayConfigurationChangedMessageForTesting();

#if PLATFORM(GTK) || PLATFORM(WPE)
    void setSandboxEnabled(bool enabled) { m_sandboxEnabled = enabled; };
    bool sandboxEnabled() const { return m_sandboxEnabled; };
#endif

private:
    void platformInitialize();

    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void platformInvalidateContext();

    Ref<WebProcessProxy> processForNavigationInternal(WebPageProxy&, const API::Navigation&, ProcessSwapRequestedByClient, WebCore::PolicyAction&, String& reason);

    RefPtr<WebProcessProxy> tryTakePrewarmedProcess(WebsiteDataStore&);

    WebProcessProxy& createNewWebProcess(WebsiteDataStore&, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No);
    void initializeNewWebProcess(WebProcessProxy&, WebsiteDataStore&);

    void requestWebContentStatistics(StatisticsRequest*);
    void requestNetworkingStatistics(StatisticsRequest*);

    void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);

    void handleMessage(IPC::Connection&, const String& messageName, const UserData& messageBody);
    void handleSynchronousMessage(IPC::Connection&, const String& messageName, const UserData& messageBody, UserData& returnUserData);

    void didGetStatistics(const StatisticsData&, uint64_t callbackID);

#if ENABLE(GAMEPAD)
    void startedUsingGamepads(IPC::Connection&);
    void stoppedUsingGamepads(IPC::Connection&);

    void processStoppedUsingGamepads(WebProcessProxy&);
#endif

    void reinstateNetworkProcessAssertionState(NetworkProcessProxy&);
    void updateProcessAssertions();

    // IPC::MessageReceiver.
    // Implemented in generated WebProcessPoolMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    static void languageChanged(void* context);
    void languageChanged();

#if PLATFORM(IOS_FAMILY)
    String cookieStorageDirectory() const;
#endif

#if PLATFORM(IOS_FAMILY)
    String parentBundleDirectory() const;
    String networkingCachesDirectory() const;
    String webContentCachesDirectory() const;
    String containerTemporaryDirectory() const;
#endif

#if PLATFORM(COCOA)
    void registerNotificationObservers();
    void unregisterNotificationObservers();
#endif

    void addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, PAL::SessionID);
    void plugInDidReceiveUserInteraction(unsigned plugInOriginHash, PAL::SessionID);

    void setAnyPageGroupMightHavePrivateBrowsingEnabled(bool);

    void resolvePathsForSandboxExtensions();
    void platformResolvePathsForSandboxExtensions();

    void addProcessToOriginCacheSet(WebPageProxy&);
    void removeProcessFromOriginCacheSet(WebProcessProxy&);

    void tryPrewarmWithDomainInformation(WebProcessProxy&, const WebCore::URL&);

    Ref<API::ProcessPoolConfiguration> m_configuration;

    IPC::MessageReceiverMap m_messageReceiverMap;

    Vector<RefPtr<WebProcessProxy>> m_processes;
    WebProcessProxy* m_prewarmedProcess { nullptr };

    WebProcessProxy* m_processWithPageCache { nullptr };
#if ENABLE(SERVICE_WORKER)
    HashMap<WebCore::SecurityOriginData, ServiceWorkerProcessProxy*> m_serviceWorkerProcesses;
    bool m_waitingForWorkerContextProcessConnection { false };
    bool m_allowsAnySSLCertificateForServiceWorker { false };
    bool m_shouldDisableServiceWorkerProcessTerminationDelay { false };
    String m_serviceWorkerUserAgent;
    std::optional<WebPreferencesStore> m_serviceWorkerPreferences;
    HashMap<String, bool> m_mayHaveRegisteredServiceWorkers;
#endif

    Ref<WebPageGroup> m_defaultPageGroup;

    RefPtr<API::Object> m_injectedBundleInitializationUserData;
    std::unique_ptr<API::InjectedBundleClient> m_injectedBundleClient;

    WebContextClient m_client;
    WebContextConnectionClient m_connectionClient;
    std::unique_ptr<API::AutomationClient> m_automationClient;
    std::unique_ptr<API::DownloadClient> m_downloadClient;
    std::unique_ptr<API::LegacyContextHistoryClient> m_historyClient;
    std::unique_ptr<API::CustomProtocolManagerClient> m_customProtocolManagerClient;

    RefPtr<WebAutomationSession> m_automationSession;

#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginInfoStore m_pluginInfoStore;
#endif
    Ref<VisitedLinkStore> m_visitedLinkStore;
    bool m_visitedLinksPopulated { false };

    PlugInAutoStartProvider m_plugInAutoStartProvider { this };
        
    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToRegisterAsBypassingContentSecurityPolicy;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;
    HashSet<String> m_schemesToRegisterAsLocal;
    HashSet<String> m_schemesToRegisterAsNoAccess;
    HashSet<String> m_schemesToRegisterAsDisplayIsolated;
    HashSet<String> m_schemesToRegisterAsCORSEnabled;
    HashSet<String> m_schemesToRegisterAsAlwaysRevalidated;
    HashSet<String> m_schemesToRegisterAsCachePartitioned;
    HashSet<String> m_schemesServiceWorkersCanHandle;
    HashSet<String> m_schemesToRegisterAsCanDisplayOnlyIfCanRequest;

    bool m_alwaysUsesComplexTextCodePath { false };
    bool m_shouldUseFontSmoothing { true };

    Vector<String> m_fontWhitelist;

    // Messages that were posted before any pages were created.
    // The client should use initialization messages instead, so that a restarted process would get the same state.
    Vector<std::pair<String, RefPtr<API::Object>>> m_messagesToInjectedBundlePostedToEmptyContext;

    bool m_memorySamplerEnabled { false };
    double m_memorySamplerInterval { 1400.0 };

    RefPtr<API::WebsiteDataStore> m_websiteDataStore;

    typedef HashMap<const char*, RefPtr<WebContextSupplement>, PtrHash<const char*>> WebContextSupplementMap;
    WebContextSupplementMap m_supplements;

#if USE(SOUP)
    HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy { HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain };
    WebCore::SoupNetworkProxySettings m_networkProxySettings;
#endif
    HashSet<String, ASCIICaseInsensitiveHash> m_urlSchemesRegisteredForCustomProtocols;

#if PLATFORM(MAC)
    RetainPtr<NSObject> m_enhancedAccessibilityObserver;
    RetainPtr<NSObject> m_automaticTextReplacementNotificationObserver;
    RetainPtr<NSObject> m_automaticSpellingCorrectionNotificationObserver;
    RetainPtr<NSObject> m_automaticQuoteSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_automaticDashSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_accessibilityDisplayOptionsNotificationObserver;
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    RetainPtr<NSObject> m_scrollerStyleNotificationObserver;
#endif

    std::unique_ptr<HighPerformanceGraphicsUsageSampler> m_highPerformanceGraphicsUsageSampler;
    std::unique_ptr<PerActivityStateCPUUsageSampler> m_perActivityStateCPUUsageSampler;
#endif

    bool m_shouldUseTestingNetworkSession { false };

    bool m_processTerminationEnabled { true };

    bool m_canHandleHTTPSServerTrustEvaluation { true };
    bool m_didNetworkProcessCrash { false };
    std::unique_ptr<NetworkProcessProxy> m_networkProcess;

    HashMap<uint64_t, RefPtr<DictionaryCallback>> m_dictionaryCallbacks;
    HashMap<uint64_t, RefPtr<StatisticsRequest>> m_statisticsRequests;

#if USE(SOUP)
    bool m_ignoreTLSErrors { true };
#endif

    bool m_memoryCacheDisabled { false };
    bool m_javaScriptConfigurationFileEnabled { false };
    bool m_alwaysRunsAtBackgroundPriority;
    bool m_shouldTakeUIBackgroundAssertion;
    bool m_shouldMakeNextWebProcessLaunchFailForTesting { false };
    bool m_shouldMakeNextNetworkProcessLaunchFailForTesting { false };

    UserObservablePageCounter m_userObservablePageCounter;
    ProcessSuppressionDisabledCounter m_processSuppressionDisabledForPageCounter;
    HiddenPageThrottlingAutoIncreasesCounter m_hiddenPageThrottlingAutoIncreasesCounter;
    RunLoop::Timer<WebProcessPool> m_hiddenPageThrottlingTimer;

#if PLATFORM(COCOA)
    RetainPtr<NSMutableDictionary> m_bundleParameters;
    ProcessSuppressionDisabledToken m_pluginProcessManagerProcessSuppressionDisabledToken;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, String> m_encodedContentExtensions;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, uint8_t>>> m_pluginLoadClientPolicies;
#endif

#if ENABLE(GAMEPAD)
    HashSet<WebProcessProxy*> m_processesUsingGamepads;
#endif

#if PLATFORM(COCOA)
    bool m_cookieStoragePartitioningEnabled { false };
    bool m_storageAccessAPIEnabled { false };
#endif

    struct Paths {
        String injectedBundlePath;
        String applicationCacheDirectory;
        String webSQLDatabaseDirectory;
        String mediaCacheDirectory;
        String mediaKeyStorageDirectory;
        String uiProcessBundleResourcePath;
        String indexedDatabaseDirectory;

#if PLATFORM(IOS_FAMILY)
        String cookieStorageDirectory;
        String containerCachesDirectory;
        String containerTemporaryDirectory;
#endif

        Vector<String> additionalWebProcessSandboxExtensionPaths;
    };
    Paths m_resolvedPaths;

    HashMap<PAL::SessionID, HashSet<WebPageProxy*>> m_sessionToPagesMap;
    RunLoop::Timer<WebProcessPool> m_serviceWorkerProcessesTerminationTimer;

#if PLATFORM(IOS_FAMILY)
    ForegroundWebProcessCounter m_foregroundWebProcessCounter;
    BackgroundWebProcessCounter m_backgroundWebProcessCounter;
    ProcessThrottler::ForegroundActivityToken m_foregroundTokenForNetworkProcess;
    ProcessThrottler::BackgroundActivityToken m_backgroundTokenForNetworkProcess;
#if ENABLE(SERVICE_WORKER)
    HashMap<WebCore::SecurityOriginData, ProcessThrottler::ForegroundActivityToken> m_foregroundTokensForServiceWorkerProcesses;
    HashMap<WebCore::SecurityOriginData, ProcessThrottler::BackgroundActivityToken> m_backgroundTokensForServiceWorkerProcesses;
#endif
#endif

    Deque<std::unique_ptr<SuspendedPageProxy>> m_suspendedPages;
    HashMap<String, RefPtr<WebProcessProxy>> m_swappedProcessesPerRegistrableDomain;

    HashMap<String, std::unique_ptr<WebCore::PrewarmInformation>> m_prewarmInformationPerRegistrableDomain;

#if PLATFORM(GTK) || PLATFORM(WPE)
    bool m_sandboxEnabled { false };
#endif
};

template<typename T>
void WebProcessPool::sendToNetworkingProcess(T&& message)
{
    if (m_networkProcess && m_networkProcess->canSendMessage())
        m_networkProcess->send(std::forward<T>(message), 0);
}

template<typename T>
void WebProcessPool::sendToNetworkingProcessRelaunchingIfNecessary(T&& message)
{
    ensureNetworkProcess();
    m_networkProcess->send(std::forward<T>(message), 0);
}

template<typename T>
void WebProcessPool::sendToAllProcesses(const T& message)
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage())
            process->send(T(message), 0);
    }
}

template<typename T>
void WebProcessPool::sendToAllProcessesRelaunchingThemIfNecessary(const T& message)
{
    // FIXME (Multi-WebProcess): WebProcessPool doesn't track processes that have exited, so it cannot relaunch these. Perhaps this functionality won't be needed in this mode.
    sendToAllProcesses(message);
}

template<typename T>
void WebProcessPool::sendToOneProcess(T&& message)
{
    bool messageSent = false;
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage()) {
            process->send(std::forward<T>(message), 0);
            messageSent = true;
            break;
        }
    }

    if (!messageSent) {
        prewarmProcess();
        RefPtr<WebProcessProxy> process = m_processes.last();
        if (process->canSendMessage())
            process->send(std::forward<T>(message), 0);
    }
}

} // namespace WebKit
