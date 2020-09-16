/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "GPUProcessProxy.h"
#include "GenericCallback.h"
#include "HiddenPageThrottlingAutoIncreasesCounter.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "PlugInAutoStartProvider.h"
#include "PluginInfoStore.h"
#include "ProcessThrottler.h"
#include "VisitedLinkStore.h"
#include "WebContextClient.h"
#include "WebContextConnectionClient.h"
#include "WebPreferencesStore.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SharedStringHash.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounter.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
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
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS WKPreferenceObserver;
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
#include "DisplayLink.h"
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
class RegistrableDomain;
enum class EventMakesGamepadsVisible : bool;
struct MockMediaDevice;
#if PLATFORM(COCOA)
class PowerSourceNotifier;
#endif
}

namespace WebKit {

class WebBackForwardCache;
class HighPerformanceGraphicsUsageSampler;
class UIGamepad;
class PerActivityStateCPUUsageSampler;
class SuspendedPageProxy;
class WebAutomationSession;
class WebContextSupplement;
class WebPageGroup;
class WebPageProxy;
class WebProcessCache;
struct GPUProcessCreationParameters;
struct NetworkProcessCreationParameters;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;
    
typedef GenericCallback<API::Dictionary*> DictionaryCallback;

#if PLATFORM(COCOA)
int networkProcessLatencyQOS();
int networkProcessThroughputQOS();
int webProcessLatencyQOS();
int webProcessThroughputQOS();
#endif

enum class ProcessSwapRequestedByClient : bool;

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

    void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::ReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::ReceiverName);
    void removeMessageReceiver(IPC::ReceiverName, uint64_t destinationID);

    WebBackForwardCache& backForwardCache() { return m_backForwardCache.get(); }
    
    template <typename T>
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, ObjectIdentifier<T> destinationID, IPC::MessageReceiver& receiver)
    {
        addMessageReceiver(messageReceiverName, destinationID.toUInt64(), receiver);
    }
    
    template <typename T>
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName, ObjectIdentifier<T> destinationID)
    {
        removeMessageReceiver(messageReceiverName, destinationID.toUInt64());
    }

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    void initializeClient(const WKContextClientBase*);
    void setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&&);
    void initializeConnectionClient(const WKContextConnectionClientBase*);
    void setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>&&);
    void setDownloadClient(UniqueRef<API::DownloadClient>&&);
    void setAutomationClient(std::unique_ptr<API::AutomationClient>&&);
    void setLegacyCustomProtocolManagerClient(std::unique_ptr<API::CustomProtocolManagerClient>&&);

    void setCustomWebContentServiceBundleIdentifier(const String&);
    const String& customWebContentServiceBundleIdentifier() { return m_configuration->customWebContentServiceBundleIdentifier(); }

    const Vector<RefPtr<WebProcessProxy>>& processes() const { return m_processes; }

    // WebProcessProxy object which does not have a running process which is used for convenience, to avoid
    // null checks in WebPageProxy.
    WebProcessProxy* dummyProcessProxy(PAL::SessionID sessionID) const { return m_dummyProcessProxies.get(sessionID).get(); }

    // WebProcess or NetworkProcess as approporiate for current process model. The connection must be non-null.
    IPC::Connection* networkingProcessConnection();

    template<typename T> void sendToAllProcesses(const T& message);
    template<typename T> void sendToAllProcessesForSession(const T& message, PAL::SessionID);

    // Sends the message to WebProcess or NetworkProcess as approporiate for current process model.
    template<typename T> void sendToNetworkingProcess(T&& message);
    template<typename T, typename U> void sendSyncToNetworkingProcess(T&& message, U&& reply);
    template<typename T> void sendToNetworkingProcessRelaunchingIfNecessary(T&& message);

    void processDidFinishLaunching(WebProcessProxy*);

    WebProcessCache& webProcessCache() { return m_webProcessCache.get(); }

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    WebKit::WebsiteDataStore* websiteDataStore() const { return m_websiteDataStore.get(); }
    void setPrimaryDataStore(WebKit::WebsiteDataStore& dataStore) { m_websiteDataStore = &dataStore; }

    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    void pageBeginUsingWebsiteDataStore(WebPageProxyIdentifier, WebsiteDataStore&);
    void pageEndUsingWebsiteDataStore(WebPageProxyIdentifier, WebsiteDataStore&);
    bool hasPagesUsingWebsiteDataStore(WebsiteDataStore&) const;

    const String& injectedBundlePath() const { return m_configuration->injectedBundlePath(); }
#if PLATFORM(COCOA)
    NSSet *allowedClassesForParameterCoding() const;
    void initializeClassesForParameterCoding();
#endif

    DownloadProxy& download(WebsiteDataStore&, WebPageProxy* initiatingPage, const WebCore::ResourceRequest&, const String& suggestedFilename = { });
    DownloadProxy& resumeDownload(WebsiteDataStore&, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path);

    void setInjectedBundleInitializationUserData(RefPtr<API::Object>&& userData) { m_injectedBundleInitializationUserData = WTFMove(userData); }

    void postMessageToInjectedBundle(const String&, API::Object*);

    void populateVisitedLinks();

#if PLATFORM(IOS_FAMILY)
    void applicationIsAboutToSuspend();
    static void notifyProcessPoolsApplicationIsAboutToSuspend();
#endif

    void handleMemoryPressureWarning(Critical);

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginsDirectory(const String&);
    void refreshPlugins();

    PluginInfoStore& pluginInfoStore() { return m_pluginInfoStore; }

    void setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString);
    void resetPluginLoadClientPolicies(HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>>&&);
    void clearPluginClientPolicies();
    const HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>>& pluginLoadClientPolicies() const { return m_pluginLoadClientPolicies; }
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    Optional<unsigned> nominalFramesPerSecondForDisplay(WebCore::PlatformDisplayID);
    void startDisplayLink(IPC::Connection&, DisplayLinkObserverID, WebCore::PlatformDisplayID);
    void stopDisplayLink(IPC::Connection&, DisplayLinkObserverID, WebCore::PlatformDisplayID);
    void stopDisplayLinks(IPC::Connection&);
#endif

    void addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions);
    void clearSupportedPlugins();

    ProcessID networkProcessIdentifier();
    ProcessID prewarmedProcessIdentifier();
    void activePagesOriginsInWebProcessForTesting(ProcessID, CompletionHandler<void(Vector<String>&&)>&&);
    bool networkProcessHasEntitlementForTesting(const String&);

    WebPageGroup& defaultPageGroup() { return m_defaultPageGroup.get(); }

    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);
    void registerURLSchemeAsLocal(const String&);
    void registerURLSchemeAsNoAccess(const String&);
    void registerURLSchemeAsDisplayIsolated(const String&);
    void registerURLSchemeAsCORSEnabled(const String&);
    void registerURLSchemeAsCachePartitioned(const String&);
    void registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String&);

    VisitedLinkStore& visitedLinkStore() { return m_visitedLinkStore.get(); }

    void setCacheModel(CacheModel);
    void setCacheModelSynchronouslyForTesting(CacheModel);


    void setDefaultRequestTimeoutInterval(double);

    void startMemorySampler(const double interval);
    void stopMemorySampler();

#if USE(SOUP)
    void setInitialHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy) { m_initialHTTPCookieAcceptPolicy = policy; }
    void setNetworkProxySettings(const WebCore::SoupNetworkProxySettings&);
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy& createDownloadProxy(WebsiteDataStore&, const WebCore::ResourceRequest&, WebPageProxy* originatingPage, const FrameInfoData&);
    API::DownloadClient& downloadClient() { return m_downloadClient.get(); }

    API::LegacyContextHistoryClient& historyClient() { return *m_historyClient; }
    WebContextClient& client() { return m_client; }

    API::CustomProtocolManagerClient& customProtocolManagerClient() const { return *m_customProtocolManagerClient; }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void clearCachedCredentials(const PAL::SessionID&);
    void terminateNetworkProcess();
    void terminateAllWebContentProcesses();
    void sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&&);
    void sendNetworkProcessWillSuspendImminentlyForTesting();
    void sendNetworkProcessDidResume();
    void terminateServiceWorkers();

    void flushCookies(const PAL::SessionID&, CompletionHandler<void()>&&);

    void setShouldMakeNextWebProcessLaunchFailForTesting(bool value) { m_shouldMakeNextWebProcessLaunchFailForTesting = value; }
    bool shouldMakeNextWebProcessLaunchFailForTesting() const { return m_shouldMakeNextWebProcessLaunchFailForTesting; }
    void setShouldMakeNextNetworkProcessLaunchFailForTesting(bool value) { m_shouldMakeNextNetworkProcessLaunchFailForTesting = value; }
    bool shouldMakeNextNetworkProcessLaunchFailForTesting() const { return m_shouldMakeNextNetworkProcessLaunchFailForTesting; }

    void reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState);

    WebProcessProxy& processForRegistrableDomain(WebsiteDataStore&, WebPageProxy*, const WebCore::RegistrableDomain&); // Will return an existing one if limit is met or due to caching.

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

    bool javaScriptConfigurationFileEnabled() { return m_javaScriptConfigurationFileEnabled; }
    void setJavaScriptConfigurationFileEnabled(bool flag);
#if PLATFORM(IOS_FAMILY)
    void setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    enum class GamepadType {
        All,
        HID,
        GameControllerFramework,
    };
    size_t numberOfConnectedGamepadsForTesting(GamepadType);
    void setUsesOnlyHIDGamepadProviderForTesting(bool);

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

#if ENABLE(GPU_PROCESS)
    void gpuProcessCrashed(ProcessID);
    void getGPUProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetGPUProcessConnectionDelayedReply&&);
#endif

    // Network Process Management
    NetworkProcessProxy& ensureNetworkProcess(WebsiteDataStore* withWebsiteDataStore = nullptr);
    NetworkProcessProxy* networkProcess() const { return m_networkProcess.get(); }
    void networkProcessCrashed(NetworkProcessProxy&);

    void getNetworkProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetNetworkProcessConnectionDelayedReply&&);

    bool isServiceWorkerPageID(WebPageProxyIdentifier) const;
#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(NetworkProcessProxy&, WebCore::RegistrableDomain&&, PAL::SessionID, CompletionHandler<void()>&&);
    void removeFromServiceWorkerProcesses(WebProcessProxy&);
    size_t serviceWorkerProxiesCount() const { return m_serviceWorkerProcesses.computeSize(); }
    void setAllowsAnySSLCertificateForServiceWorker(bool allows) { m_allowsAnySSLCertificateForServiceWorker = allows; }
    bool allowsAnySSLCertificateForServiceWorker() const { return m_allowsAnySSLCertificateForServiceWorker; }
    void updateServiceWorkerUserAgent(const String& userAgent);
    const Optional<UserContentControllerIdentifier>& userContentControllerIdentifierForServiceWorkers() const { return m_userContentControllerIDForServiceWorker; }
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
#endif
    void serviceWorkerProcessCrashed(WebProcessProxy&);

#if PLATFORM(COCOA)
    bool processSuppressionEnabled() const;
#endif

    void windowServerConnectionStateChanged();

#if USE(SOUP)
    void setIgnoreTLSErrors(bool);
    bool ignoreTLSErrors() const { return m_ignoreTLSErrors; }
#endif

    static void setInvalidMessageCallback(void (*)(WKStringRef));
    static void didReceiveInvalidMessage(IPC::MessageName);

    bool isURLKnownHSTSHost(const String& urlString) const;

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
    void setFontAllowList(API::Array*);

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

    bool alwaysRunsAtBackgroundPriority() const { return m_alwaysRunsAtBackgroundPriority; }
    bool shouldTakeUIBackgroundAssertion() const { return m_shouldTakeUIBackgroundAssertion; }

    void synthesizeAppIsBackground(bool background);
    
#if ENABLE(GAMEPAD)
    void gamepadConnected(const UIGamepad&, WebCore::EventMakesGamepadsVisible);
    void gamepadDisconnected(const UIGamepad&);
#endif

#if PLATFORM(COCOA)
    bool cookieStoragePartitioningEnabled() const { return m_cookieStoragePartitioningEnabled; }
    void setCookieStoragePartitioningEnabled(bool);

    void clearPermanentCredentialsForProtectionSpace(WebCore::ProtectionSpace&&);
#endif

    static uint64_t registerProcessPoolCreationListener(Function<void(WebProcessPool&)>&&);
    static void unregisterProcessPoolCreationListener(uint64_t identifier);

    ForegroundWebProcessToken foregroundWebProcessToken() const { return ForegroundWebProcessToken(m_foregroundWebProcessCounter.count()); }
    BackgroundWebProcessToken backgroundWebProcessToken() const { return BackgroundWebProcessToken(m_backgroundWebProcessCounter.count()); }
    bool hasForegroundWebProcesses() const { return m_foregroundWebProcessCounter.value(); }
    bool hasBackgroundWebProcesses() const { return m_backgroundWebProcessCounter.value(); }

    void processForNavigation(WebPageProxy&, const API::Navigation&, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&&);

    void didReachGoodTimeToPrewarm();

    void didCollectPrewarmInformation(const WebCore::RegistrableDomain&, const WebCore::PrewarmInformation&);

    void screenPropertiesStateChanged();

    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String& persistentId);
    void resetMockMediaDevices();

    void sendDisplayConfigurationChangedMessageForTesting();
    void clearCurrentModifierStateForTesting();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID, const WebCore::RegistrableDomain& fromDomain, const WebCore::RegistrableDomain& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag>, WebPageProxyIdentifier, WebCore::PageIdentifier);
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void seedResourceLoadStatisticsForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&&);
    void sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&&);
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    void setSandboxEnabled(bool enabled) { m_sandboxEnabled = enabled; };
    void addSandboxPath(const CString& path, SandboxPermission permission) { m_extraSandboxPaths.add(path, permission); };
    const HashMap<CString, SandboxPermission>& sandboxPaths() const { return m_extraSandboxPaths; };
    bool sandboxEnabled() const { return m_sandboxEnabled; };

    void setUserMessageHandler(Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)>&& handler) { m_userMessageHandler = WTFMove(handler); }
    const Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)>& userMessageHandler() const { return m_userMessageHandler; }
#endif

    WebProcessWithAudibleMediaToken webProcessWithAudibleMediaToken() const;

    void disableDelayedWebProcessLaunch() { m_isDelayedWebProcessLaunchDisabled = true; }

    void setJavaScriptConfigurationDirectory(String&& directory) { m_javaScriptConfigurationDirectory = directory; }
    const String& javaScriptConfigurationDirectory() const { return m_javaScriptConfigurationDirectory; }
    
    WebProcessDataStoreParameters webProcessDataStoreParameters(WebProcessProxy&, WebsiteDataStore&);
    
    PlugInAutoStartProvider& plugInAutoStartProvider() { return m_plugInAutoStartProvider; }

    void setUseSeparateServiceWorkerProcess(bool);
    bool useSeparateServiceWorkerProcess() const { return m_useSeparateServiceWorkerProcess; }

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const Optional<String>& encodedValue);
#endif

#if PLATFORM(PLAYSTATION)
    const String& webProcessPath() const { return m_resolvedPaths.webProcessPath; }
    const String& networkProcessPath() const { return m_resolvedPaths.networkProcessPath; }
    int32_t userId() const { return m_userId; }
#endif

#if PLATFORM(COCOA)
    OSObjectPtr<xpc_object_t> xpcEndpointMessage() const;
    void sendNetworkProcessXPCEndpointToWebProcess(OSObjectPtr<xpc_object_t> endpointMessage);
#endif

private:
    void platformInitialize();

    void platformInitializeWebProcess(const WebProcessProxy&, WebProcessCreationParameters&);
    void platformInvalidateContext();

    void processForNavigationInternal(WebPageProxy&, const API::Navigation&, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&&);

    RefPtr<WebProcessProxy> tryTakePrewarmedProcess(WebsiteDataStore&);

    WebProcessProxy& createNewWebProcess(WebsiteDataStore*, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No);
    void initializeNewWebProcess(WebProcessProxy&, WebsiteDataStore*, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No);

    void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);

    void handleMessage(IPC::Connection&, const String& messageName, const UserData& messageBody);
    void handleSynchronousMessage(IPC::Connection&, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&&);

#if ENABLE(GAMEPAD)
    void startedUsingGamepads(IPC::Connection&);
    void stoppedUsingGamepads(IPC::Connection&);

    void processStoppedUsingGamepads(WebProcessProxy&);
#endif

    void updateProcessAssertions();
    void updateAudibleMediaAssertions();

    // IPC::MessageReceiver.
    // Implemented in generated WebProcessPoolMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    static void languageChanged(void* context);
    void languageChanged();

    bool usesSingleWebProcess() const { return m_configuration->usesSingleWebProcess(); }

#if PLATFORM(IOS_FAMILY)
    static String cookieStorageDirectory();
    static String parentBundleDirectory();
    static String networkingCachesDirectory();
    static String webContentCachesDirectory();
    static String containerTemporaryDirectory();
#endif

#if PLATFORM(COCOA)
    void registerNotificationObservers();
    void unregisterNotificationObservers();
#endif

    void setApplicationIsActive(bool);

    void resolvePathsForSandboxExtensions();
    void platformResolvePathsForSandboxExtensions();

    void addProcessToOriginCacheSet(WebProcessProxy&, const URL&);
    void removeProcessFromOriginCacheSet(WebProcessProxy&);

    void tryPrewarmWithDomainInformation(WebProcessProxy&, const WebCore::RegistrableDomain&);

    void updateBackForwardCacheCapacity();

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    static float displayBrightness();
    static void backlightLevelDidChangeCallback(CFNotificationCenterRef, void *observer, CFStringRef name, const void *, CFDictionaryRef userInfo);    
#if ENABLE(REMOTE_INSPECTOR)
    static void remoteWebInspectorEnabledCallback(CFNotificationCenterRef, void *observer, CFStringRef name, const void *, CFDictionaryRef userInfo);
#endif
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void startObservingPreferenceChanges();
#endif

    static void registerHighDynamicRangeChangeCallback();

    Ref<API::ProcessPoolConfiguration> m_configuration;

    IPC::MessageReceiverMap m_messageReceiverMap;

    Vector<RefPtr<WebProcessProxy>> m_processes;
    WebProcessProxy* m_prewarmedProcess { nullptr };

    HashMap<PAL::SessionID, WeakPtr<WebProcessProxy>> m_dummyProcessProxies; // Lightweight WebProcessProxy objects without backing process.

#if ENABLE(SERVICE_WORKER)
    WeakHashSet<WebProcessProxy> m_serviceWorkerProcesses;
    bool m_waitingForWorkerContextProcessConnection { false };
    bool m_allowsAnySSLCertificateForServiceWorker { false };
    String m_serviceWorkerUserAgent;
    Optional<WebPreferencesStore> m_serviceWorkerPreferences;
    Optional<UserContentControllerIdentifier> m_userContentControllerIDForServiceWorker;
#endif

    Ref<WebPageGroup> m_defaultPageGroup;

    RefPtr<API::Object> m_injectedBundleInitializationUserData;
    std::unique_ptr<API::InjectedBundleClient> m_injectedBundleClient;

    WebContextClient m_client;
    WebContextConnectionClient m_connectionClient;
    std::unique_ptr<API::AutomationClient> m_automationClient;
    UniqueRef<API::DownloadClient> m_downloadClient;
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
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;
    HashSet<String> m_schemesToRegisterAsDisplayIsolated;
    HashSet<String> m_schemesToRegisterAsCORSEnabled;
    HashSet<String> m_schemesToRegisterAsAlwaysRevalidated;
    HashSet<String> m_schemesToRegisterAsCachePartitioned;
    HashSet<String> m_schemesToRegisterAsCanDisplayOnlyIfCanRequest;

    bool m_alwaysUsesComplexTextCodePath { false };
    bool m_shouldUseFontSmoothing { true };

    Vector<String> m_fontAllowList;

    // Messages that were posted before any pages were created.
    // The client should use initialization messages instead, so that a restarted process would get the same state.
    Vector<std::pair<String, RefPtr<API::Object>>> m_messagesToInjectedBundlePostedToEmptyContext;

    bool m_memorySamplerEnabled { false };
    double m_memorySamplerInterval { 1400.0 };

    RefPtr<WebKit::WebsiteDataStore> m_websiteDataStore;

    typedef HashMap<const char*, RefPtr<WebContextSupplement>, PtrHash<const char*>> WebContextSupplementMap;
    WebContextSupplementMap m_supplements;

#if USE(SOUP)
    WebCore::HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy { WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain };
    WebCore::SoupNetworkProxySettings m_networkProxySettings;
#endif

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
    RetainPtr<NSObject> m_deactivationObserver;

    std::unique_ptr<HighPerformanceGraphicsUsageSampler> m_highPerformanceGraphicsUsageSampler;
    std::unique_ptr<PerActivityStateCPUUsageSampler> m_perActivityStateCPUUsageSampler;
#endif

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::PowerSourceNotifier> m_powerSourceNotifier;
    RetainPtr<NSObject> m_activationObserver;
    RetainPtr<NSObject> m_accessibilityEnabledObserver;
#endif

    bool m_processTerminationEnabled { true };

    std::unique_ptr<NetworkProcessProxy> m_networkProcess;

    HashMap<uint64_t, RefPtr<DictionaryCallback>> m_dictionaryCallbacks;

#if USE(SOUP)
    bool m_ignoreTLSErrors { true };
#endif

    bool m_memoryCacheDisabled { false };
    bool m_javaScriptConfigurationFileEnabled { false };
    String m_javaScriptConfigurationDirectory;
    bool m_alwaysRunsAtBackgroundPriority;
    bool m_shouldTakeUIBackgroundAssertion;
    bool m_shouldMakeNextWebProcessLaunchFailForTesting { false };
    bool m_shouldMakeNextNetworkProcessLaunchFailForTesting { false };
    bool m_tccPreferenceEnabled { false };

    UserObservablePageCounter m_userObservablePageCounter;
    ProcessSuppressionDisabledCounter m_processSuppressionDisabledForPageCounter;
    HiddenPageThrottlingAutoIncreasesCounter m_hiddenPageThrottlingAutoIncreasesCounter;
    RunLoop::Timer<WebProcessPool> m_hiddenPageThrottlingTimer;

#if PLATFORM(COCOA)
    RetainPtr<NSMutableDictionary> m_bundleParameters;
    ProcessSuppressionDisabledToken m_pluginProcessManagerProcessSuppressionDisabledToken;
    mutable RetainPtr<NSSet> m_classesForParameterCoder;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, String> m_encodedContentExtensions;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>> m_pluginLoadClientPolicies;
#endif

#if ENABLE(GAMEPAD)
    HashSet<WebProcessProxy*> m_processesUsingGamepads;
#endif

#if PLATFORM(COCOA)
    bool m_cookieStoragePartitioningEnabled { false };
#endif

    struct Paths {
        String injectedBundlePath;
        String uiProcessBundleResourcePath;

#if PLATFORM(IOS_FAMILY)
        String cookieStorageDirectory;
        String containerCachesDirectory;
        String containerTemporaryDirectory;
#endif

#if PLATFORM(PLAYSTATION)
        String webProcessPath;
        String networkProcessPath;
#endif

        Vector<String> additionalWebProcessSandboxExtensionPaths;
    };
    Paths m_resolvedPaths;

    HashMap<PAL::SessionID, HashSet<WebPageProxyIdentifier>> m_sessionToPageIDsMap;

    ForegroundWebProcessCounter m_foregroundWebProcessCounter;
    BackgroundWebProcessCounter m_backgroundWebProcessCounter;

    UniqueRef<WebBackForwardCache> m_backForwardCache;

    UniqueRef<WebProcessCache> m_webProcessCache;
    HashMap<WebCore::RegistrableDomain, RefPtr<WebProcessProxy>> m_swappedProcessesPerRegistrableDomain;

    HashMap<WebCore::RegistrableDomain, std::unique_ptr<WebCore::PrewarmInformation>> m_prewarmInformationPerRegistrableDomain;

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    Vector<std::unique_ptr<DisplayLink>> m_displayLinks;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    bool m_sandboxEnabled { false };
    HashMap<CString, SandboxPermission> m_extraSandboxPaths;

    Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)> m_userMessageHandler;
#endif

    WebProcessWithAudibleMediaCounter m_webProcessWithAudibleMediaCounter;

    struct AudibleMediaActivity {
        UniqueRef<ProcessAssertion> uiProcessMediaPlaybackAssertion;
#if ENABLE(GPU_PROCESS)
        std::unique_ptr<ProcessAssertion> gpuProcessMediaPlaybackAssertion;
#endif
    };
    Optional<AudibleMediaActivity> m_audibleMediaActivity;

#if PLATFORM(PLAYSTATION)
    int32_t m_userId { -1 };
#endif

#if PLATFORM(IOS)
    // FIXME: Delayed process launch is currently disabled on iOS for performance reasons (rdar://problem/49074131).
    bool m_isDelayedWebProcessLaunchDisabled { true };
#else
    bool m_isDelayedWebProcessLaunchDisabled { false };
#endif
    bool m_useSeparateServiceWorkerProcess { false };

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    HashSet<WebCore::RegistrableDomain> m_domainsWithUserInteraction;
#endif

#if PLATFORM(COCOA)
    OSObjectPtr<xpc_object_t> m_endpointMessage;
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
void WebProcessPool::sendToAllProcessesForSession(const T& message, PAL::SessionID sessionID)
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage() && !process->isPrewarmed() && process->sessionID() == sessionID)
            process->send(T(message), 0);
    }
}

} // namespace WebKit
