/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include "DisplayLink.h"
#include <WebCore/PowerObserverMac.h>
#include <pal/system/SystemSleepListener.h>
#endif


#if ENABLE(IPC_TESTING_API)
#include "IPCTester.h"
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
enum class EventMakesGamepadsVisible : bool;
enum class GamepadHapticEffectType : uint8_t;
struct GamepadEffectParameters;
struct MockMediaDevice;
#if PLATFORM(COCOA)
class PowerSourceNotifier;
#endif
}

namespace WebKit {

class LockdownModeObserver;
class HighPerformanceGraphicsUsageSampler;
class UIGamepad;
class PerActivityStateCPUUsageSampler;
class SuspendedPageProxy;
class WebAutomationSession;
class WebBackForwardCache;
class WebContextSupplement;
class WebPageGroup;
class WebPageProxy;
class WebProcessCache;
struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;
struct NetworkProcessCreationParameters;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;

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
    static Ref<WebProcessPool> create(API::ProcessPoolConfiguration&);

    explicit WebProcessPool(API::ProcessPoolConfiguration&);        
    virtual ~WebProcessPool();

    API::ProcessPoolConfiguration& configuration() { return m_configuration.get(); }

    static Vector<Ref<WebProcessPool>> allProcessPools();

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
    void addMessageReceiver(IPC::ReceiverName, UInt128 destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::ReceiverName);
    void removeMessageReceiver(IPC::ReceiverName, UInt128 destinationID);

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
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void initializeClient(const WKContextClientBase*);
    void setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&&);
    void initializeConnectionClient(const WKContextConnectionClientBase*);
    void setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>&&);
    void setLegacyDownloadClient(RefPtr<API::DownloadClient>&&);
    void setAutomationClient(std::unique_ptr<API::AutomationClient>&&);

    void setCustomWebContentServiceBundleIdentifier(const String&);
    const String& customWebContentServiceBundleIdentifier() { return m_configuration->customWebContentServiceBundleIdentifier(); }

    const Vector<Ref<WebProcessProxy>>& processes() const { return m_processes; }

    // WebProcessProxy object which does not have a running process which is used for convenience, to avoid
    // null checks in WebPageProxy.
    WebProcessProxy* dummyProcessProxy(PAL::SessionID sessionID) const { return m_dummyProcessProxies.get(sessionID).get(); }

    void forEachProcessForSession(PAL::SessionID, const Function<void(WebProcessProxy&)>&);
    template<typename T> void sendToAllProcesses(const T& message);
    template<typename T> void sendToAllProcessesForSession(const T& message, PAL::SessionID);

    template<typename T> static void sendToAllRemoteWorkerProcesses(const T& message);

    void processDidFinishLaunching(WebProcessProxy&);

    WebProcessCache& webProcessCache() { return m_webProcessCache.get(); }

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy&);

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
    DownloadProxy& resumeDownload(WebsiteDataStore&, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path, CallDownloadDidStart);

    void setInjectedBundleInitializationUserData(RefPtr<API::Object>&& userData) { m_injectedBundleInitializationUserData = WTFMove(userData); }

    void postMessageToInjectedBundle(const String&, API::Object*);

    void populateVisitedLinks();

#if PLATFORM(IOS_FAMILY)
    void applicationIsAboutToSuspend();
    static void notifyProcessPoolsApplicationIsAboutToSuspend();
    void setProcessesShouldSuspend(bool);
#endif

    void handleMemoryPressureWarning(Critical);

#if HAVE(CVDISPLAYLINK)
    DisplayLinkCollection& displayLinks() { return m_displayLinks; }

    std::optional<WebCore::FramesPerSecond> nominalFramesPerSecondForDisplay(WebCore::PlatformDisplayID);

    void startDisplayLink(WebProcessProxy&, DisplayLinkObserverID, WebCore::PlatformDisplayID, WebCore::FramesPerSecond);
    void stopDisplayLink(WebProcessProxy&, DisplayLinkObserverID, WebCore::PlatformDisplayID);
    void setDisplayLinkPreferredFramesPerSecond(WebProcessProxy&, DisplayLinkObserverID, WebCore::PlatformDisplayID, WebCore::FramesPerSecond);
    void stopDisplayLinks(WebProcessProxy&);
    void setDisplayLinkForDisplayWantsFullSpeedUpdates(WebProcessProxy&, WebCore::PlatformDisplayID, bool wantsFullSpeedUpdates);
#endif

    void addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions);
    void clearSupportedPlugins();

    ProcessID prewarmedProcessIdentifier();
    void activePagesOriginsInWebProcessForTesting(ProcessID, CompletionHandler<void(Vector<String>&&)>&&);

    WebPageGroup& defaultPageGroup() { return m_defaultPageGroup.get(); }

    void setAlwaysUsesComplexTextCodePath(bool);
    void setDisableFontSubpixelAntialiasingForTesting(bool);
    
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
    static void setNetworkProcessMemoryPressureHandlerConfiguration(const std::optional<MemoryPressureHandler::Configuration>& configuration) { s_networkProcessMemoryPressureHandlerConfiguration = configuration; }
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy& createDownloadProxy(WebsiteDataStore&, const WebCore::ResourceRequest&, WebPageProxy* originatingPage, const FrameInfoData&);
    API::DownloadClient* legacyDownloadClient() { return m_legacyDownloadClient.get(); }

    API::LegacyContextHistoryClient& historyClient() { return *m_historyClient; }
    WebContextClient& client() { return m_client; }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void clearCachedCredentials(PAL::SessionID);
    void terminateAllWebContentProcesses();
    void sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&&);
    void sendNetworkProcessWillSuspendImminentlyForTesting();
    void sendNetworkProcessDidResume();
    void terminateServiceWorkers();

    void setShouldMakeNextWebProcessLaunchFailForTesting(bool value) { m_shouldMakeNextWebProcessLaunchFailForTesting = value; }
    bool shouldMakeNextWebProcessLaunchFailForTesting() const { return m_shouldMakeNextWebProcessLaunchFailForTesting; }

    void reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState);

    Ref<WebProcessProxy> processForRegistrableDomain(WebsiteDataStore&, const WebCore::RegistrableDomain&, WebProcessProxy::LockdownMode); // Will return an existing one if limit is met or due to caching.

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
    GPUProcessProxy* gpuProcess() const { return m_gpuProcess.get(); }
#endif
    // Network Process Management
    void networkProcessDidTerminate(NetworkProcessProxy&, ProcessTerminationReason);

    bool isServiceWorkerPageID(WebPageProxyIdentifier) const;
    void removeFromRemoteWorkerProcesses(WebProcessProxy&);

#if ENABLE(SERVICE_WORKER)
    size_t serviceWorkerProxiesCount() const;
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
#endif
    void serviceWorkerProcessCrashed(WebProcessProxy&, ProcessTerminationReason);

    void updateRemoteWorkerUserAgent(const String& userAgent);
    UserContentControllerIdentifier userContentControllerIdentifierForRemoteWorkers();
    static void establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType, WebCore::RegistrableDomain&&, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&&);

#if PLATFORM(COCOA)
    bool processSuppressionEnabled() const;
#endif

    void windowServerConnectionStateChanged();

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

#if ENABLE(WEBCONTENT_CRASH_TESTING)
    bool shouldCrashWhenCreatingWebProcess() const { return m_shouldCrashWhenCreatingWebProcess; }
#endif

    ForegroundWebProcessToken foregroundWebProcessToken() const { return ForegroundWebProcessToken(m_foregroundWebProcessCounter.count()); }
    BackgroundWebProcessToken backgroundWebProcessToken() const { return BackgroundWebProcessToken(m_backgroundWebProcessCounter.count()); }
    bool hasForegroundWebProcesses() const { return m_foregroundWebProcessCounter.value(); }
    bool hasBackgroundWebProcesses() const { return m_backgroundWebProcessCounter.value(); }

    void processForNavigation(WebPageProxy&, const API::Navigation&, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient, WebProcessProxy::LockdownMode, const FrameInfoData&, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&&);

    void didReachGoodTimeToPrewarm();

    void didCollectPrewarmInformation(const WebCore::RegistrableDomain&, const WebCore::PrewarmInformation&);

    void screenPropertiesStateChanged();

    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void setMockMediaDeviceIsEphemeral(const String&, bool);
    void resetMockMediaDevices();

    void sendDisplayConfigurationChangedMessageForTesting();
    void clearCurrentModifierStateForTesting();

#if ENABLE(TRACKING_PREVENTION)
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&&, CompletionHandler<void()>&&);
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

    void setOverrideLanguages(Vector<String>&&);

    WebProcessDataStoreParameters webProcessDataStoreParameters(WebProcessProxy&, WebsiteDataStore&);
    
    static void setUseSeparateServiceWorkerProcess(bool);
    static bool useSeparateServiceWorkerProcess() { return s_useSeparateServiceWorkerProcess; }

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif

#if PLATFORM(PLAYSTATION)
    const String& webProcessPath() const { return m_resolvedPaths.webProcessPath; }
    const String& networkProcessPath() const { return m_resolvedPaths.networkProcessPath; }
    int32_t userId() const { return m_userId; }
#endif

    static void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);
    static Vector<String> urlSchemesWithCustomProtocolHandlers();

    Ref<WebProcessProxy> createNewWebProcess(WebsiteDataStore*, WebProcessProxy::LockdownMode, WebProcessProxy::IsPrewarmed = WebProcessProxy::IsPrewarmed::No, WebCore::CrossOriginMode = WebCore::CrossOriginMode::Shared);

    bool hasAudibleMediaActivity() const { return !!m_audibleMediaActivity; }
#if PLATFORM(IOS_FAMILY)
    bool processesShouldSuspend() const { return m_processesShouldSuspend; }
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void hardwareConsoleStateChanged();
#endif

    bool operator==(const WebProcessPool& other) const { return (this == &other); }
    bool operator!=(const WebProcessPool& other) const { return !(this == &other); }

private:
    void platformInitialize();

    void platformInitializeWebProcess(const WebProcessProxy&, WebProcessCreationParameters&);
    void platformInvalidateContext();

    void processForNavigationInternal(WebPageProxy&, const API::Navigation&, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient, WebProcessProxy::LockdownMode, const FrameInfoData&, Ref<WebsiteDataStore>&&, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&&);

    RefPtr<WebProcessProxy> tryTakePrewarmedProcess(WebsiteDataStore&, WebProcessProxy::LockdownMode);

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

    void updateProcessAssertions();
    void updateAudibleMediaAssertions();

    // IPC::MessageReceiver.
    // Implemented in generated WebProcessPoolMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    bool usesSingleWebProcess() const { return m_configuration->usesSingleWebProcess(); }

#if PLATFORM(COCOA)
    void addCFNotificationObserver(CFNotificationCallback, CFStringRef name, CFNotificationCenterRef = CFNotificationCenterGetDarwinNotifyCenter());
    void removeCFNotificationObserver(CFStringRef name, CFNotificationCenterRef = CFNotificationCenterGetDarwinNotifyCenter());

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

    Ref<API::ProcessPoolConfiguration> m_configuration;

    IPC::MessageReceiverMap m_messageReceiverMap;

    Vector<Ref<WebProcessProxy>> m_processes;
    WeakPtr<WebProcessProxy> m_prewarmedProcess;

    HashMap<PAL::SessionID, WeakPtr<WebProcessProxy>> m_dummyProcessProxies; // Lightweight WebProcessProxy objects without backing process.

    static WeakHashSet<WebProcessProxy>& remoteWorkerProcesses();

    std::optional<WebPreferencesStore> m_remoteWorkerPreferences;
    RefPtr<WebUserContentControllerProxy> m_userContentControllerForRemoteWorkers;
    String m_remoteWorkerUserAgent;

#if ENABLE(GPU_PROCESS)
    RefPtr<GPUProcessProxy> m_gpuProcess;
#endif

    Ref<WebPageGroup> m_defaultPageGroup;

    RefPtr<API::Object> m_injectedBundleInitializationUserData;
    std::unique_ptr<API::InjectedBundleClient> m_injectedBundleClient;

    WebContextClient m_client;
    WebContextConnectionClient m_connectionClient;
    std::unique_ptr<API::AutomationClient> m_automationClient;
    RefPtr<API::DownloadClient> m_legacyDownloadClient;
    std::unique_ptr<API::LegacyContextHistoryClient> m_historyClient;

    RefPtr<WebAutomationSession> m_automationSession;

    Ref<VisitedLinkStore> m_visitedLinkStore;
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

    typedef HashMap<const char*, RefPtr<WebContextSupplement>, PtrHash<const char*>> WebContextSupplementMap;
    WebContextSupplementMap m_supplements;

#if USE(SOUP)
    WebCore::HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy { WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain };
    static std::optional<MemoryPressureHandler::Configuration> s_networkProcessMemoryPressureHandlerConfiguration;
#endif

#if PLATFORM(MAC)
    RetainPtr<NSObject> m_enhancedAccessibilityObserver;
    RetainPtr<NSObject> m_automaticTextReplacementNotificationObserver;
    RetainPtr<NSObject> m_automaticSpellingCorrectionNotificationObserver;
    RetainPtr<NSObject> m_automaticQuoteSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_automaticDashSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_accessibilityDisplayOptionsNotificationObserver;
    RetainPtr<NSObject> m_scrollerStyleNotificationObserver;
    RetainPtr<NSObject> m_deactivationObserver;
    RetainPtr<WKWebInspectorPreferenceObserver> m_webInspectorPreferenceObserver;

    std::unique_ptr<HighPerformanceGraphicsUsageSampler> m_highPerformanceGraphicsUsageSampler;
    std::unique_ptr<PerActivityStateCPUUsageSampler> m_perActivityStateCPUUsageSampler;
#endif

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::PowerSourceNotifier> m_powerSourceNotifier;
    RetainPtr<NSObject> m_activationObserver;
    RetainPtr<NSObject> m_accessibilityEnabledObserver;
    RetainPtr<NSObject> m_applicationLaunchObserver;

    RetainPtr<WKProcessPoolWeakObserver> m_weakObserver;
#endif

    bool m_processTerminationEnabled { true };

    bool m_memoryCacheDisabled { false };
    bool m_javaScriptConfigurationFileEnabled { false };
    String m_javaScriptConfigurationDirectory;
    bool m_alwaysRunsAtBackgroundPriority;
    bool m_shouldTakeUIBackgroundAssertion;
    bool m_shouldMakeNextWebProcessLaunchFailForTesting { false };
    bool m_tccPreferenceEnabled { false };

    UserObservablePageCounter m_userObservablePageCounter;
    ProcessSuppressionDisabledCounter m_processSuppressionDisabledForPageCounter;
    HiddenPageThrottlingAutoIncreasesCounter m_hiddenPageThrottlingAutoIncreasesCounter;
    RunLoop::Timer m_hiddenPageThrottlingTimer;

#if ENABLE(GPU_PROCESS)
    RunLoop::Timer m_resetGPUProcessCrashCountTimer;
    unsigned m_recentGPUProcessCrashCount { 0 };
#endif

#if PLATFORM(COCOA)
    RetainPtr<NSMutableDictionary> m_bundleParameters;
    mutable RetainPtr<NSSet> m_classesForParameterCoder;
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

#if ENABLE(WEBCONTENT_CRASH_TESTING)
    bool m_shouldCrashWhenCreatingWebProcess { false };
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

    UniqueRef<WebBackForwardCache> m_backForwardCache;

    UniqueRef<WebProcessCache> m_webProcessCache;
    HashMap<WebCore::RegistrableDomain, RefPtr<WebProcessProxy>> m_swappedProcessesPerRegistrableDomain;

    HashMap<WebCore::RegistrableDomain, std::unique_ptr<WebCore::PrewarmInformation>> m_prewarmInformationPerRegistrableDomain;

#if HAVE(CVDISPLAYLINK)
    DisplayLinkCollection m_displayLinks;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    bool m_sandboxEnabled { false };
    HashMap<CString, SandboxPermission> m_extraSandboxPaths;

    Function<void(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&)> m_userMessageHandler;
#endif

    WebProcessWithAudibleMediaCounter m_webProcessWithAudibleMediaCounter;

    struct AudibleMediaActivity {
        Ref<ProcessAssertion> uiProcessMediaPlaybackAssertion;
#if ENABLE(GPU_PROCESS)
        RefPtr<ProcessAssertion> gpuProcessMediaPlaybackAssertion;
#endif
    };
    std::optional<AudibleMediaActivity> m_audibleMediaActivity;

#if PLATFORM(PLAYSTATION)
    int32_t m_userId { -1 };
#endif

#if PLATFORM(IOS)
    // FIXME: Delayed process launch is currently disabled on iOS for performance reasons (rdar://problem/49074131).
    bool m_isDelayedWebProcessLaunchDisabled { true };
#else
    bool m_isDelayedWebProcessLaunchDisabled { false };
#endif
    static bool s_useSeparateServiceWorkerProcess;

#if ENABLE(TRACKING_PREVENTION)
    HashSet<WebCore::RegistrableDomain> m_domainsWithUserInteraction;
    HashMap<TopFrameDomain, SubResourceDomain> m_domainsWithCrossPageStorageAccessQuirk;
#endif
    
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::PowerObserver> m_powerObserver;
    std::unique_ptr<PAL::SystemSleepListener> m_systemSleepListener;
    Vector<int> m_openDirectoryNotifyTokens;
#endif
#if ENABLE(IPC_TESTING_API)
    IPCTester m_ipcTester;
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_processesShouldSuspend { false };
#endif
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
    forEachProcessForSession(sessionID, [&](auto& process) {
        process.send(T(message), 0);
    });
}

template<typename T>
void WebProcessPool::sendToAllRemoteWorkerProcesses(const T& message)
{
    for (auto& process : remoteWorkerProcesses()) {
        if (process.canSendMessage())
            process.send(T(message), 0);
    }
}

} // namespace WebKit
