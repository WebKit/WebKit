/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "HiddenPageThrottlingAutoIncreasesCounter.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "ProcessThrottler.h"
#include "VisitedLinkStore.h"
#include "WebContextClient.h"
#include "WebPreferencesStore.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SharedStringHash.h>
#include <pal/SessionID.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounter.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS WKPreferenceObserver;
OBJC_CLASS WKProcessPoolWeakObserver;
#if PLATFORM(MAC)
OBJC_CLASS WKWebInspectorPreferenceObserver;
#endif
#endif

#if PLATFORM(MAC)
#include <WebCore/PowerObserverMac.h>
#include <pal/system/SystemSleepListener.h>
#endif

#if HAVE(DISPLAY_LINK)
#include "DisplayLink.h"
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTester.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "HardwareKeyboardState.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/cf/NotificationCenterCF.h>
#endif

namespace API {
class AutomationClient;
class DownloadClient;
class HTTPCookieStore;
class InjectedBundleClient;
class LegacyContextHistoryClient;
class LegacyDownloadClient;
class Navigation;
class PageConfiguration;
}

namespace WebCore {
class RegistrableDomain;
class Site;
enum class EventMakesGamepadsVisible : bool;
enum class GamepadHapticEffectType : uint8_t;
enum class ProcessSwapDisposition : uint8_t;
struct GamepadEffectParameters;
struct MockMediaDevice;
#if PLATFORM(COCOA)
class PowerSourceNotifier;
#endif
}

namespace WebKit {

class BrowsingContextGroup;
class ExtensionCapabilityGranter;
class LockdownModeObserver;
class PerActivityStateCPUUsageSampler;
class SuspendedPageProxy;
class UIGamepad;
class WebAutomationSession;
class WebBackForwardCache;
class WebCompiledContentRuleList;
class WebContextSupplement;
class WebPageGroup;
class WebPageProxy;
class WebProcessCache;
struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;
struct NetworkProcessCreationParameters;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
class ListDataObserver;
#endif

#if PLATFORM(COCOA)
int networkProcessLatencyQOS();
int networkProcessThroughputQOS();
int webProcessLatencyQOS();
int webProcessThroughputQOS();
#endif
void addLockdownModeObserver(LockdownModeObserver&);
void removeLockdownModeObserver(LockdownModeObserver&);
bool lockdownModeEnabledBySystem();
void setLockdownModeEnabledGloballyForTesting(std::optional<bool>);

enum class CallDownloadDidStart : bool;
enum class ProcessSwapRequestedByClient : bool;

class WebProcessPool final
    : public API::ObjectImpl<API::Object::Type::ProcessPool>
    , public IPC::MessageReceiver
#if PLATFORM(MAC)
    , private PAL::SystemSleepListener::Client
#endif
{
public:
    USING_CAN_MAKE_WEAKPTR(IPC::MessageReceiver);

    static Ref<WebProcessPool> create(API::ProcessPoolConfiguration&);

    explicit WebProcessPool(API::ProcessPoolConfiguration&);        
    virtual ~WebProcessPool();

    void ref() const final { API::ObjectImpl<API::Object::Type::ProcessPool>::ref(); }
    void deref() const final { API::ObjectImpl<API::Object::Type::ProcessPool>::deref(); }

    API::ProcessPoolConfiguration& configuration() { return m_configuration.get(); }

    static Vector<Ref<WebProcessPool>> allProcessPools();

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    RefPtr<T> protectedSupplement()
    {
        return supplement<T>();
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
    
    template<typename RawValue>
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, const ObjectIdentifierGenericBase<RawValue>& destinationID, IPC::MessageReceiver& receiver)
    {
        addMessageReceiver(messageReceiverName, destinationID.toUInt64(), receiver);
    }
    
    template<typename RawValue>
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName, const ObjectIdentifierGenericBase<RawValue>& destinationID)
    {
        removeMessageReceiver(messageReceiverName, destinationID.toUInt64());
    }

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void initializeClient(const WKContextClientBase*);
    void setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&&);
    void setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>&&);
    void setLegacyDownloadClient(RefPtr<API::DownloadClient>&&);
    void setAutomationClient(std::unique_ptr<API::AutomationClient>&&);

    const Vector<Ref<WebProcessProxy>>& processes() const { return m_processes; }

    // WebProcessProxy object which does not have a running process which is used for convenience, to avoid
    // null checks in WebPageProxy.
    WebProcessProxy* dummyProcessProxy(PAL::SessionID sessionID) const { return m_dummyProcessProxies.get(sessionID); }

    void forEachProcessForSession(PAL::SessionID, NOESCAPE const Function<void(WebProcessProxy&)>&);
    template<typename T> void sendToAllProcesses(const T& message);
    template<typename T> void sendToAllProcessesForSession(const T& message, PAL::SessionID);

    template<typename T> static void sendToAllRemoteWorkerProcesses(const T& message);

    void processDidFinishLaunching(WebProcessProxy&);

    WebProcessCache& webProcessCache() { return m_webProcessCache.get(); }

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy&);

    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    void pageBeginUsingWebsiteDataStore(WebPageProxy&, WebsiteDataStore&);
    void pageEndUsingWebsiteDataStore(WebPageProxy&, WebsiteDataStore&);
    bool hasPagesUsingWebsiteDataStore(WebsiteDataStore&) const;

    const String& injectedBundlePath() const { return m_configuration->injectedBundlePath(); }

    Ref<DownloadProxy> download(WebsiteDataStore&, WebPageProxy* initiatingPage, const WebCore::ResourceRequest&, const std::optional<FrameInfoData>&, const String& suggestedFilename = { });
    Ref<DownloadProxy> resumeDownload(WebsiteDataStore&, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path, CallDownloadDidStart);

    void setInjectedBundleInitializationUserData(RefPtr<API::Object>&& userData) { m_injectedBundleInitializationUserData = WTFMove(userData); }

    void postMessageToInjectedBundle(const String&, API::Object*);

    void populateVisitedLinks();

#if PLATFORM(IOS_FAMILY)
    void applicationIsAboutToSuspend();
    static void notifyProcessPoolsApplicationIsAboutToSuspend();
    void setProcessesShouldSuspend(bool);
#endif

    void handleMemoryPressureWarning(Critical);

#if PLATFORM(COCOA)
    void screenPropertiesChanged();
#endif

#if PLATFORM(MAC)
    void displayPropertiesChanged(const WebCore::ScreenProperties&, WebCore::PlatformDisplayID, CGDisplayChangeSummaryFlags);
#endif

#if HAVE(DISPLAY_LINK)
    DisplayLinkCollection& displayLinks() { return m_displayLinks; }
#endif

    void addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions);
    void clearSupportedPlugins();

    HashSet<ProcessID> prewarmedProcessIdentifiers();
    void activePagesOriginsInWebProcessForTesting(ProcessID, CompletionHandler<void(Vector<String>&&)>&&);
    void countWebPagesInAllProcessesForTesting(CompletionHandler<void(unsigned)>&&);

    WebPageGroup& defaultPageGroup() { return m_defaultPageGroup.get(); }

    void setAlwaysUsesComplexTextCodePath(bool);
    void setDisableFontSubpixelAntialiasingForTesting(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);
    void registerURLSchemeAsLocal(const String&);
#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
    void registerURLSchemeAsNoAccess(const String&);
#endif
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
    static void setNetworkProcessMemoryPressureHandlerConfiguration(const std::optional<MemoryPressureHandler::Configuration>& configuration) { s_networkProcessMemoryPressureHandlerConfiguration = configuration; }
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    Ref<DownloadProxy> createDownloadProxy(WebsiteDataStore&, const WebCore::ResourceRequest&, WebPageProxy* originatingPage, const std::optional<FrameInfoData>&);

    API::LegacyContextHistoryClient& historyClient() { return *m_historyClient; }
    WebContextClient& client() { return m_client; }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void terminateAllWebContentProcesses(ProcessTerminationReason);
    void sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&&);
    void sendNetworkProcessWillSuspendImminentlyForTesting();
    void sendNetworkProcessDidResume();
    void terminateServiceWorkersForSession(PAL::SessionID);
    void terminateServiceWorkers();

    void setShouldMakeNextWebProcessLaunchFailForTesting(bool value) { m_shouldMakeNextWebProcessLaunchFailForTesting = value; }
    bool shouldMakeNextWebProcessLaunchFailForTesting() const { return m_shouldMakeNextWebProcessLaunchFailForTesting; }

    void reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState);

    enum class IsSharedProcess : bool { No, Yes };
    Ref<WebProcessProxy> processForSite(WebsiteDataStore&, IsSharedProcess, const std::optional<WebCore::Site>&, const std::optional<WebCore::Site>& mainFrameSite, const HashSet<WebCore::RegistrableDomain>& isolatedDomains,
        WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, const API::PageConfiguration&, WebCore::ProcessSwapDisposition); // Will return an existing one if limit is met or due to caching.

    void prewarmProcess();

    bool shouldTerminate(WebProcessProxy&);

    void disableProcessTermination();
    void enableProcessTermination();

    void updateAutomationCapabilities() const;
    void setAutomationSession(RefPtr<WebAutomationSession>&&);
    WebAutomationSession* automationSession() const { return m_automationSession.get(); }

    // Defaults to false.
    void setHTTPPipeliningEnabled(bool);
    bool httpPipeliningEnabled() const;

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(const IPC::Connection&) const;

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

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidFinishLaunching(ProcessID);
    void gpuProcessExited(ProcessID, ProcessTerminationReason);

    void createGPUProcessConnection(WebProcessProxy&, IPC::Connection::Handle&&, WebKit::GPUProcessConnectionParameters&&);

    GPUProcessProxy& ensureGPUProcess();
    Ref<GPUProcessProxy> ensureProtectedGPUProcess();
    GPUProcessProxy* gpuProcess() const { return m_gpuProcess.get(); }
    RefPtr<GPUProcessProxy> protectedGPUProcess() const { return gpuProcess(); }
#endif

#if ENABLE(MODEL_PROCESS)
    void modelProcessDidFinishLaunching(ProcessID);
    void modelProcessExited(ProcessID, ProcessTerminationReason);

    void createModelProcessConnection(WebProcessProxy&, IPC::Connection::Handle&&, WebKit::ModelProcessConnectionParameters&&);

    Ref<ModelProcessProxy> ensureProtectedModelProcess(WebProcessProxy& requestingWebProcess);
    ModelProcessProxy* modelProcess() const { return m_modelProcess.get(); }
#endif

    // Network Process Management
    void networkProcessDidTerminate(NetworkProcessProxy&, ProcessTerminationReason);

    bool isServiceWorkerPageID(WebPageProxyIdentifier) const;

    size_t serviceWorkerProxiesCount() const;
    void isJITDisabledInAllRemoteWorkerProcesses(CompletionHandler<void(bool)>&&) const;
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
    void serviceWorkerProcessCrashed(WebProcessProxy&, ProcessTerminationReason);

    void updateRemoteWorkerUserAgent(const String& userAgent);
    Ref<WebUserContentControllerProxy> userContentControllerForRemoteWorkers();
    static void establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType, WebCore::Site&&, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&&);

#if PLATFORM(COCOA)
    bool processSuppressionEnabled() const;
#endif

    void windowServerConnectionStateChanged();

    static void setInvalidMessageCallback(void (*)(WKStringRef));
    static void didReceiveInvalidMessage(IPC::MessageName);

    bool isURLKnownHSTSHost(const String& urlString) const;

    static void registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);
    static void unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);

    void notifyMediaStreamingActivity(bool);

#if PLATFORM(COCOA)
    void updateProcessSuppressionState();

    NSMutableDictionary *ensureBundleParameters();
    RetainPtr<NSMutableDictionary> ensureProtectedBundleParameters();
    NSMutableDictionary *bundleParameters() { return m_bundleParameters.get(); }
    RetainPtr<NSMutableDictionary> protectedBundleParameters();
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
    static bool anyProcessPoolNeedsUIBackgroundAssertion();

#if ENABLE(GAMEPAD)
    void gamepadConnected(const UIGamepad&, WebCore::EventMakesGamepadsVisible);
    void gamepadDisconnected(const UIGamepad&);
#endif

#if PLATFORM(COCOA)
    bool cookieStoragePartitioningEnabled() const { return m_cookieStoragePartitioningEnabled; }
    void setCookieStoragePartitioningEnabled(bool);

    void clearPermanentCredentialsForProtectionSpace(WebCore::ProtectionSpace&&);

    void lockdownModeStateChanged();
#endif

    ForegroundWebProcessToken foregroundWebProcessToken() const { return ForegroundWebProcessToken(m_foregroundWebProcessCounter.count()); }
    BackgroundWebProcessToken backgroundWebProcessToken() const { return BackgroundWebProcessToken(m_backgroundWebProcessCounter.count()); }
    bool hasForegroundWebProcesses() const { return m_foregroundWebProcessCounter.value(); }
    bool hasBackgroundWebProcesses() const { return m_backgroundWebProcessCounter.value(); }

#if ENABLE(MODEL_PROCESS)
    bool hasForegroundWebProcessesWithModels() const;
    bool hasBackgroundWebProcessesWithModels() const;
#endif

    void processForNavigation(WebPageProxy&, WebFrameProxy&, const API::Navigation&, const URL& sourceURL, BrowsingContextGroup&, IsSharedProcess, const WebCore::Site& mainFrameSite, ProcessSwapRequestedByClient, WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, LoadedWebArchive, const FrameInfoData&, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, ASCIILiteral)>&&);

    void didReachGoodTimeToPrewarm();

    void didCollectPrewarmInformation(const WebCore::RegistrableDomain&, const WebCore::PrewarmInformation&);

    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void setMockMediaDeviceIsEphemeral(const String&, bool);
    void resetMockMediaDevices();

    void clearCurrentModifierStateForTesting();

    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, Vector<SubResourceDomain>>&&, CompletionHandler<void()>&&);
    void seedResourceLoadStatisticsForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&&);
    void sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&&);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void setSandboxEnabled(bool);
    void addSandboxPath(const CString& path, SandboxPermission permission) { m_extraSandboxPaths.add(path, permission); };
    const HashMap<CString, SandboxPermission>& sandboxPaths() const { return m_extraSandboxPaths; };
    bool sandboxEnabled() const { return m_sandboxEnabled; };

    void setUserMessageHandler(Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)>&& handler) { m_userMessageHandler = WTFMove(handler); }
    const Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)>& userMessageHandler() const { return m_userMessageHandler; }

#if USE(ATSPI)
    const String& accessibilityBusAddress() const;
    const String& accessibilityBusName() const;
    const String& sandboxedAccessibilityBusAddress() const;

    const String& generateNextAccessibilityBusName();
#endif
#endif

    WebProcessWithAudibleMediaToken webProcessWithAudibleMediaToken() const;
    WebProcessWithMediaStreamingToken webProcessWithMediaStreamingToken() const;

    static bool globalDelaysWebProcessLaunchDefaultValue();
    bool delaysWebProcessLaunchDefaultValue() const { return m_delaysWebProcessLaunchDefaultValue; }
    void setDelaysWebProcessLaunchDefaultValue(bool delaysWebProcessLaunchDefaultValue) { m_delaysWebProcessLaunchDefaultValue = delaysWebProcessLaunchDefaultValue; }

    void setJavaScriptConfigurationDirectory(String&& directory) { m_javaScriptConfigurationDirectory = directory; }
    const String& javaScriptConfigurationDirectory() const { return m_javaScriptConfigurationDirectory; }

    void setOverrideLanguages(Vector<String>&&);

    WebProcessDataStoreParameters webProcessDataStoreParameters(WebProcessProxy&, WebsiteDataStore&);
    
    static void setUseSeparateServiceWorkerProcess(bool);
    static bool useSeparateServiceWorkerProcess() { return s_useSeparateServiceWorkerProcess; }

    void addRemoteWorkerProcess(WebProcessProxy&);
    void removeRemoteWorkerProcess(WebProcessProxy&);

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif

#if PLATFORM(PLAYSTATION)
    const String& webProcessPath() const { return m_resolvedPaths.webProcessPath; }
    const String& networkProcessPath() const { return m_resolvedPaths.networkProcessPath; }
    int32_t userId() const { return m_userId; }
#endif

#if PLATFORM(WIN) // FIXME: remove this line when this feature is enabled for playstation port.
#if ENABLE(REMOTE_INSPECTOR)
    void setPagesControlledByAutomation(bool);
#endif
#endif

    static void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);
    static Vector<String> urlSchemesWithCustomProtocolHandlers();

    Ref<WebProcessProxy> createNewWebProcess(WebsiteDataStore*, WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No, WebCore::CrossOriginMode = WebCore::CrossOriginMode::Shared);

    bool hasAudibleMediaActivity() const { return !!m_audibleMediaActivity; }
#if PLATFORM(IOS_FAMILY)
    bool processesShouldSuspend() const { return m_processesShouldSuspend; }
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void hardwareConsoleStateChanged();
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    ExtensionCapabilityGranter& extensionCapabilityGranter();
#endif

    bool usesSingleWebProcess() const { return m_configuration->usesSingleWebProcess(); }

    bool operator==(const WebProcessPool& other) const { return (this == &other); }

#if PLATFORM(IOS_FAMILY)
    HardwareKeyboardState cachedHardwareKeyboardState() const;
#endif

    bool webProcessStateUpdatesForPageClientEnabled() const { return m_webProcessStateUpdatesForPageClientEnabled; }
    void setWebProcessStateUpdatesForPageClientEnabled(bool enabled) { m_webProcessStateUpdatesForPageClientEnabled = enabled; }

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    void observeScriptTrackingPrivacyUpdatesIfNeeded();
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    void memoryPressureStatusChangedForProcess(WebProcessProxy&, SystemMemoryPressureStatus);
    void checkMemoryPressureStatus();

    static Seconds defaultWebProcessSuspensionDelay();
    Seconds webProcessSuspensionDelay() const;
    void updateWebProcessSuspensionDelay();
    void updateWebProcessSuspensionDelayWithPacing(WeakHashSet<WebProcessProxy>&&);
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    WebCompiledContentRuleList* cachedResourceMonitorRuleList(bool forTesting);
    void setResourceMonitorURLsForTesting(const String& rulesText, CompletionHandler<void()>&&);
#endif

#if PLATFORM(COCOA)
    void registerAssetFonts(WebProcessProxy&);
#endif

#if PLATFORM(MAC)
    void registerUserInstalledFonts(WebProcessProxy&);
    void registerAdditionalFonts(NSArray *fontNames);
#endif

#if PLATFORM(IOS_FAMILY)
    void didRefreshDisplay();
#endif
    void suppressEDR(bool);

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    void initializeAccessibilityIfNecessary();
#endif

    void setPLTResourceDelayInterval(Seconds interval) { m_pltResourceDelayInterval = interval; }
    Seconds pltResourceDelayInterval() const { return m_pltResourceDelayInterval; }

private:
    enum class NeedsGlobalStaticInitialization : bool { No, Yes };
    void platformInitialize(NeedsGlobalStaticInitialization);

    RefPtr<WebProcessProxy> webProcessProxyFromConnection(const IPC::Connection&) const;

    void platformInitializeWebProcess(const WebProcessProxy&, WebProcessCreationParameters&);
    void platformInvalidateContext();

    std::tuple<Ref<WebProcessProxy>, RefPtr<SuspendedPageProxy>, ASCIILiteral> processForNavigationInternal(WebPageProxy&, const API::Navigation&, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, IsSharedProcess, const WebCore::Site& mainFrameSite, ProcessSwapRequestedByClient, WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, const FrameInfoData&, Ref<WebsiteDataStore>&&);
    void prepareProcessForNavigation(Ref<WebProcessProxy>&&, WebPageProxy&, SuspendedPageProxy*, ASCIILiteral reason, IsSharedProcess, const WebCore::Site&, const WebCore::Site& mainFrameSite, const API::Navigation&, WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, LoadedWebArchive, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, ASCIILiteral)>&&, unsigned previousAttemptsCount = 0);

    RefPtr<WebProcessProxy> tryTakePrewarmedProcess(WebsiteDataStore&, WebProcessProxy::LockdownMode, WebProcessProxy::EnhancedSecurity, const API::PageConfiguration&);
    unsigned prewarmedProcessCountLimit() const;

    void initializeNewWebProcess(WebProcessProxy&, WebsiteDataStore*, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No);

    void handleMessage(IPC::Connection&, const String& messageName, const UserData& messageBody);
    void handleSynchronousMessage(IPC::Connection&, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&&);

#if ENABLE(GAMEPAD)
    void startedUsingGamepads(IPC::Connection&);
    void stoppedUsingGamepads(IPC::Connection&, CompletionHandler<void()>&&);
    void playGamepadEffect(unsigned gamepadIndex, const String& gamepadID, WebCore::GamepadHapticEffectType, const WebCore::GamepadEffectParameters&, CompletionHandler<void(bool)>&&);
    void stopGamepadEffects(unsigned gamepadIndex, const String& gamepadID, CompletionHandler<void()>&&);

    void processStoppedUsingGamepads(WebProcessProxy&);
#endif

#if ENABLE(MODEL_PROCESS)
    void startedPlayingModels(IPC::Connection&);
    void stoppedPlayingModels(IPC::Connection&);
#endif

    void updateProcessAssertions();
    static constexpr Seconds audibleActivityClearDelay = 5_s;
    void updateAudibleMediaAssertions();
    void updateMediaStreamingActivity();

    // IPC::MessageReceiver.
    // Implemented in generated WebProcessPoolMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

#if PLATFORM(COCOA)
    void addCFNotificationObserver(CFNotificationCallback, CFStringRef name, CFNotificationCenterRef = CFNotificationCenterGetDarwinNotifyCenterSingleton());
    void removeCFNotificationObserver(CFStringRef name, CFNotificationCenterRef = CFNotificationCenterGetDarwinNotifyCenterSingleton());

    void registerNotificationObservers();
    void unregisterNotificationObservers();

#if ENABLE(NOTIFY_BLOCKING)
    void setNotifyState(const String&, int, uint64_t);
#endif
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
    static void backlightLevelDidChangeCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#if ENABLE(REMOTE_INSPECTOR)
    static void remoteWebInspectorEnabledCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#endif
#endif

#if PLATFORM(COCOA)
    static void lockdownModeConfigurationUpdateCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#endif
    
#if PLATFORM(COCOA)
    static void accessibilityPreferencesChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    static void mediaAccessibilityPreferencesChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#endif

#if PLATFORM(MAC)
    static void colorPreferencesDidChangeCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void* postingObject, CFDictionaryRef userInfo);
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY) && ENABLE(GPU_PROCESS)
    static void powerLogTaskModeStartedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void startObservingPreferenceChanges();
#endif

    static void registerDisplayConfigurationCallback();
    static void registerHighDynamicRangeChangeCallback();

#if PLATFORM(MAC)
    // PAL::SystemSleepListener
    void systemWillSleep() final;
    void systemDidWake() final;
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    void setMediaAccessibilityPreferences(WebProcessProxy&);
#endif
    void clearAudibleActivity();

#if PLATFORM(IOS_FAMILY)
    static void hardwareKeyboardAvailabilityChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef);
    void initializeHardwareKeyboardAvailability();
    void hardwareKeyboardAvailabilityChanged();
    void setCachedHardwareKeyboardState(HardwareKeyboardState);
#endif

#if ENABLE(MODEL_PROCESS)
    ModelProcessProxy& ensureModelProcess();
    void updateModelProcessAssertion();
    void terminateAllWebContentProcessesWithModelPlayers();
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    void loadOrUpdateResourceMonitorRuleList();

    void platformLoadResourceMonitorRuleList(CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&&);
    void platformCompileResourceMonitorRuleList(const String& rulesText, CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&&);
    String platformResourceMonitorRuleListSourceForTesting();
#endif

    const Ref<API::ProcessPoolConfiguration> m_configuration;

    IPC::MessageReceiverMap m_messageReceiverMap;

    Vector<Ref<WebProcessProxy>> m_processes;
    WeakHashSet<WebProcessProxy> m_prewarmedProcesses;

    bool m_hasUsedSiteIsolation { false };

    HashMap<PAL::SessionID, WeakPtr<WebProcessProxy>> m_dummyProcessProxies; // Lightweight WebProcessProxy objects without backing process.

    static WeakHashSet<WebProcessProxy>& remoteWorkerProcesses();

    std::optional<WebPreferencesStore> m_remoteWorkerPreferences;
    RefPtr<WebUserContentControllerProxy> m_userContentControllerForRemoteWorkers;
    String m_remoteWorkerUserAgent;

#if ENABLE(GPU_PROCESS)
    RefPtr<GPUProcessProxy> m_gpuProcess;
#endif
#if ENABLE(MODEL_PROCESS)
    RefPtr<ModelProcessProxy> m_modelProcess;
    WeakHashSet<WebProcessProxy> m_processesWithModelPlayers;
#endif

    const Ref<WebPageGroup> m_defaultPageGroup;

    RefPtr<API::Object> m_injectedBundleInitializationUserData;
    std::unique_ptr<API::InjectedBundleClient> m_injectedBundleClient;

    WebContextClient m_client;
    std::unique_ptr<API::AutomationClient> m_automationClient;
    RefPtr<API::DownloadClient> m_legacyDownloadClient;
    std::unique_ptr<API::LegacyContextHistoryClient> m_historyClient;

    RefPtr<WebAutomationSession> m_automationSession;

    const Ref<VisitedLinkStore> m_visitedLinkStore;
    bool m_visitedLinksPopulated { false };

    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;
    HashSet<String> m_schemesToRegisterAsDisplayIsolated;
    HashSet<String> m_schemesToRegisterAsCORSEnabled;
    HashSet<String> m_schemesToRegisterAsAlwaysRevalidated;
    HashSet<String> m_schemesToRegisterAsCachePartitioned;
    HashSet<String> m_schemesToRegisterAsCanDisplayOnlyIfCanRequest;

    bool m_alwaysUsesComplexTextCodePath { false };
    bool m_disableFontSubpixelAntialiasingForTesting { false };

    Vector<String> m_fontAllowList;

    // Messages that were posted before any pages were created.
    // The client should use initialization messages instead, so that a restarted process would get the same state.
    Vector<std::pair<String, RefPtr<API::Object>>> m_messagesToInjectedBundlePostedToEmptyContext;

    bool m_memorySamplerEnabled { false };
    double m_memorySamplerInterval { 1400.0 };

    using WebContextSupplementMap = HashMap<ASCIILiteral, RefPtr<WebContextSupplement>>;
    WebContextSupplementMap m_supplements;

#if USE(SOUP)
    static std::optional<MemoryPressureHandler::Configuration> s_networkProcessMemoryPressureHandlerConfiguration;
#endif

#if PLATFORM(MAC)
    RetainPtr<NSObject> m_enhancedAccessibilityObserver;
    RetainPtr<NSObject> m_automaticTextReplacementNotificationObserver;
    RetainPtr<NSObject> m_automaticSpellingCorrectionNotificationObserver;
    RetainPtr<NSObject> m_automaticQuoteSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_automaticDashSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_smartListsNotificationObserver;
    RetainPtr<NSObject> m_accessibilityDisplayOptionsNotificationObserver;
    RetainPtr<NSObject> m_scrollerStyleNotificationObserver;
    RetainPtr<NSObject> m_deactivationObserver;
    RetainPtr<NSObject> m_didChangeScreenParametersNotificationObserver;
    bool m_smartListsEnabled { false };
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    RetainPtr<NSObject> m_didBeginSuppressingHighDynamicRange;
    RetainPtr<NSObject> m_didEndSuppressingHighDynamicRange;
#endif
    RetainPtr<WKWebInspectorPreferenceObserver> m_webInspectorPreferenceObserver;

    const UniqueRef<PerActivityStateCPUUsageSampler> m_perActivityStateCPUUsageSampler;
#endif

#if PLATFORM(IOS_FAMILY) && HAVE(SUPPORT_HDR_DISPLAY)
    float m_currentEDRHeadroom { 1 };
#endif

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::PowerSourceNotifier> m_powerSourceNotifier;
    RetainPtr<NSObject> m_activationObserver;
    RetainPtr<NSObject> m_accessibilityEnabledObserver;
    RetainPtr<NSObject> m_applicationLaunchObserver;
    RetainPtr<NSObject> m_finishedMobileAssetFontDownloadObserver;

    RetainPtr<WKProcessPoolWeakObserver> m_weakObserver;
#endif

    bool m_processTerminationEnabled { true };

    bool m_memoryCacheDisabled { false };
    bool m_javaScriptConfigurationFileEnabled { false };
    String m_javaScriptConfigurationDirectory;
    bool m_alwaysRunsAtBackgroundPriority;
    bool m_shouldTakeUIBackgroundAssertion;
    bool m_shouldMakeNextWebProcessLaunchFailForTesting { false };

    UserObservablePageCounter m_userObservablePageCounter;
    ProcessSuppressionDisabledCounter m_processSuppressionDisabledForPageCounter;
    HiddenPageThrottlingAutoIncreasesCounter m_hiddenPageThrottlingAutoIncreasesCounter;
    RunLoop::Timer m_hiddenPageThrottlingTimer;

#if ENABLE(GPU_PROCESS)
    RunLoop::Timer m_resetGPUProcessCrashCountTimer;
    unsigned m_recentGPUProcessCrashCount { 0 };
#endif

#if ENABLE(MODEL_PROCESS)
    RunLoop::Timer m_resetModelProcessCrashCountTimer;
    unsigned m_recentModelProcessCrashCount { 0 };
#endif

#if PLATFORM(COCOA)
    RetainPtr<NSMutableDictionary> m_bundleParameters;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, String> m_encodedContentExtensions;
#endif

#if ENABLE(GAMEPAD)
    WeakHashSet<WebProcessProxy> m_processesUsingGamepads;
#endif

#if PLATFORM(COCOA)
    bool m_cookieStoragePartitioningEnabled { false };
#endif

    struct Paths {
        String injectedBundlePath;
        String uiProcessBundleResourcePath;

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

    const UniqueRef<WebBackForwardCache> m_backForwardCache;

    const UniqueRef<WebProcessCache> m_webProcessCache;
    HashMap<WebCore::RegistrableDomain, RefPtr<WebProcessProxy>> m_swappedProcessesPerRegistrableDomain;

    HashMap<WebCore::RegistrableDomain, std::unique_ptr<WebCore::PrewarmInformation>> m_prewarmInformationPerRegistrableDomain;

#if HAVE(DISPLAY_LINK)
    DisplayLinkCollection m_displayLinks;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    bool m_sandboxEnabled { false };
    HashMap<CString, SandboxPermission> m_extraSandboxPaths;

    Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)> m_userMessageHandler;

#if USE(ATSPI)
    mutable std::optional<String> m_accessibilityBusAddress;
    mutable std::optional<String> m_accessibilityBusName;
    String m_sandboxedAccessibilityBusAddress;
#endif
#endif

#if ENABLE(WPE_PLATFORM)
    unsigned long m_availableInputDevicesSignalID { 0 };
#endif

    WebProcessWithAudibleMediaCounter m_webProcessWithAudibleMediaCounter;

    struct AudibleMediaActivity {
        RefPtr<ProcessAssertion> uiProcessMediaPlaybackAssertion;
#if ENABLE(GPU_PROCESS)
        RefPtr<ProcessAssertion> gpuProcessMediaPlaybackAssertion;
#endif
    };
    std::optional<AudibleMediaActivity> m_audibleMediaActivity;
    RunLoop::Timer m_audibleActivityTimer;

    WebProcessWithMediaStreamingCounter m_webProcessWithMediaStreamingCounter;
    bool m_mediaStreamingActivity { false };

#if PLATFORM(PLAYSTATION)
    int32_t m_userId { -1 };
#endif

    bool m_delaysWebProcessLaunchDefaultValue { globalDelaysWebProcessLaunchDefaultValue() };

    static bool s_useSeparateServiceWorkerProcess;

    HashSet<WebCore::RegistrableDomain> m_domainsWithUserInteraction;
    HashMap<TopFrameDomain, Vector<SubResourceDomain>> m_domainsWithCrossPageStorageAccessQuirk;
    
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::PowerObserver> m_powerObserver;
    std::unique_ptr<PAL::SystemSleepListener> m_systemSleepListener;
    Vector<int> m_openDirectoryNotifyTokens;
#endif
#if ENABLE(NOTIFY_BLOCKING)
    HashMap<String, uint64_t> m_notifyState;
    Vector<int> m_notifyTokens;
    Vector<RetainPtr<NSObject>> m_notificationObservers;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    RefPtr<ExtensionCapabilityGranter> m_extensionCapabilityGranter;
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_processesShouldSuspend { false };
    HardwareKeyboardState m_hardwareKeyboardState;
    String m_cachedWebContentTempDirectory;
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RefPtr<ListDataObserver> m_storageAccessUserAgentStringQuirksDataUpdateObserver;
    RefPtr<ListDataObserver> m_storageAccessPromptQuirksDataUpdateObserver;
    RefPtr<ListDataObserver> m_scriptTrackingPrivacyDataUpdateObserver;
#endif

    bool m_webProcessStateUpdatesForPageClientEnabled { false };

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    ApproximateTime m_lastMemoryPressureStatusTime;
    RunLoop::Timer m_checkMemoryPressureStatusTimer;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    RefPtr<WebCompiledContentRuleList> m_resourceMonitorRuleListCache;
    bool m_resourceMonitorRuleListLoading { false };
    bool m_resourceMonitorRuleListFailed { false };
    RunLoop::Timer m_resourceMonitorRuleListRefreshTimer;
#endif

#if PLATFORM(COCOA)
    std::optional<Vector<URL>> m_assetFontURLs;
    std::optional<HashMap<String, URL>> m_userInstalledFontURLs;
    std::optional<HashMap<String, Vector<String>>> m_userInstalledFontFamilyMap;
    std::optional<Vector<URL>> m_sandboxExtensionURLs;
#endif

#if ENABLE(IPC_TESTING_API)
    const Ref<IPCTester> m_ipcTester;
#endif

    bool m_hasReceivedAXRequestInUIProcess { false };
    bool m_suppressEDR { false };

    Seconds m_pltResourceDelayInterval { 100_ms };
};

template<typename T>
void WebProcessPool::sendToAllProcesses(const T& message)
{
    for (auto& process : m_processes) {
        if (process->canSendMessage())
            process->send(T(message), 0);
    }
}

template<typename T>
void WebProcessPool::sendToAllProcessesForSession(const T& message, PAL::SessionID sessionID)
{
    SUPPRESS_FORWARD_DECL_ARG forEachProcessForSession(sessionID, [&](WebProcessProxy& process) {
        process.send(T(message), 0);
    });
}

template<typename T>
void WebProcessPool::sendToAllRemoteWorkerProcesses(const T& message)
{
    for (Ref process : remoteWorkerProcesses()) {
        if (process->canSendMessage())
            process->send(T(message), 0);
    }
}

inline WebProcessPool& WebProcessProxy::processPool() const
{
    ASSERT(m_processPool);
    return *m_processPool.get();
}

inline Ref<WebProcessPool> WebProcessProxy::protectedProcessPool() const
{
    return processPool();
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebProcessPool)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::ProcessPool; }
SPECIALIZE_TYPE_TRAITS_END()
