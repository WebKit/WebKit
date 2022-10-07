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

#include "AccessibilityPreferences.h"
#include "AuxiliaryProcess.h"
#include "CacheModel.h"
#include "EventDispatcher.h"
#include "IdentifierTypes.h"
#include "RemoteVideoCodecFactory.h"
#include "StorageAreaMapIdentifier.h"
#include "TextCheckerState.h"
#include "UserContentControllerIdentifier.h"
#include "ViewUpdateDispatcher.h"
#include "WebInspectorInterruptDispatcher.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessCreationParameters.h"
#include "WebSQLiteDatabaseTracker.h"
#include "WebSocketChannelManager.h"
#include <WebCore/ActivityState.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PluginData.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RenderingMode.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/Timer.h>
#include <pal/HysteresisActivity.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounter.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

#if ENABLE(MEDIA_STREAM)
#include "MediaDeviceSandboxExtensions.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/ScreenProperties.h>
#include <dispatch/dispatch.h>
#include <wtf/MachSendRight.h>
#endif

#if PLATFORM(WAYLAND)
#include <WebCore/PlatformDisplayLibWPE.h>
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include <WebCore/CaptionUserPreferences.h>
#endif

namespace API {
class Object;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class ApplicationCacheStorage;
class CPUMonitor;
class CertificateInfo;
class PageGroup;
class RegistrableDomain;
class ResourceRequest;
class UserGestureToken;

enum class EventMakesGamepadsVisible : bool;

struct BackForwardItemIdentifier;
struct DisplayUpdate;
struct MessagePortIdentifier;
struct MessageWithMessagePorts;
struct MockMediaDevice;
struct PluginInfo;
struct PrewarmInformation;
struct SecurityOriginData;

#if ENABLE(SERVICE_WORKER)
struct ServiceWorkerContextData;
#endif
}

namespace WebKit {

class AudioMediaStreamTrackRendererInternalUnitManager;
class GamepadData;
class GPUProcessConnection;
class InjectedBundle;
class LibWebRTCCodecs;
class LibWebRTCNetwork;
class NetworkProcessConnection;
class ObjCObjectGraph;
class ProcessAssertion;
class RemoteCDMFactory;
class RemoteLegacyCDMFactory;
class RemoteMediaEngineConfigurationFactory;
class StorageAreaMap;
class UserData;
class WebAutomationSessionProxy;
class WebBroadcastChannelRegistry;
class WebCacheStorageProvider;
class WebCookieJar;
class WebCompiledContentRuleListData;
class WebConnectionToUIProcess;
class WebFileSystemStorageConnection;
class WebFrame;
class WebLoaderStrategy;
class WebPage;
class WebPageGroupProxy;
class WebProcessSupplement;

struct RemoteWorkerInitializationData;
struct UserMessage;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebsiteData;
struct WebsiteDataStoreParameters;

enum class RemoteWorkerType : bool;
enum class WebsiteDataType : uint32_t;

#if PLATFORM(IOS_FAMILY)
class LayerHostingContext;
#endif

#if ENABLE(MEDIA_STREAM)
class SpeechRecognitionRealtimeMediaSourceManager;
#endif

class WebProcess : public AuxiliaryProcess
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    static WebProcess& singleton();
    static constexpr WebCore::AuxiliaryProcessType processType = WebCore::AuxiliaryProcessType::WebContent;

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

    WebConnectionToUIProcess* webConnectionToUIProcess() const { return m_webConnection.get(); }

    WebPage* webPage(WebCore::PageIdentifier) const;
    void createWebPage(WebCore::PageIdentifier, WebPageCreationParameters&&);
    void removeWebPage(WebCore::PageIdentifier);
    WebPage* focusedWebPage() const;

    InjectedBundle* injectedBundle() const { return m_injectedBundle.get(); }
    
    PAL::SessionID sessionID() const { ASSERT(m_sessionID); return *m_sessionID; }

#if ENABLE(TRACKING_PREVENTION)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const { return m_thirdPartyCookieBlockingMode; }
#endif

#if PLATFORM(COCOA)
    const WTF::MachSendRight& compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool hasMouseDevice() const { return m_hasMouseDevice; }
    void setHasMouseDevice(bool);
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool hasStylusDevice() const { return m_hasStylusDevice; }
    void setHasStylusDevice(bool);
#endif

    WebFrame* webFrame(WebCore::FrameIdentifier) const;
    Vector<WebFrame*> webFrames() const;
    void addWebFrame(WebCore::FrameIdentifier, WebFrame*);
    void removeWebFrame(WebCore::FrameIdentifier);

    WebPageGroupProxy* webPageGroup(WebCore::PageGroup*);
    WebPageGroupProxy* webPageGroup(PageGroupIdentifier);
    WebPageGroupProxy* webPageGroup(const WebPageGroupData&);

    uint64_t userGestureTokenIdentifier(RefPtr<WebCore::UserGestureToken>);
    void userGestureTokenDestroyed(WebCore::UserGestureToken&);
    
    const TextCheckerState& textCheckerState() const { return m_textCheckerState; }
    void setTextCheckerState(const TextCheckerState&);

    EventDispatcher& eventDispatcher() { return m_eventDispatcher; }

    NetworkProcessConnection& ensureNetworkProcessConnection();
    void networkProcessConnectionClosed(NetworkProcessConnection*);
    NetworkProcessConnection* existingNetworkProcessConnection() { return m_networkProcessConnection.get(); }
    WebLoaderStrategy& webLoaderStrategy();
    WebFileSystemStorageConnection& fileSystemStorageConnection();

#if ENABLE(GPU_PROCESS)
    GPUProcessConnection& ensureGPUProcessConnection();
    void gpuProcessConnectionClosed(GPUProcessConnection&);
    GPUProcessConnection* existingGPUProcessConnection() { return m_gpuProcessConnection.get(); }

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    LibWebRTCCodecs& libWebRTCCodecs();
#endif
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    AudioMediaStreamTrackRendererInternalUnitManager& audioMediaStreamTrackRendererInternalUnitManager();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RemoteLegacyCDMFactory& legacyCDMFactory();
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    RemoteCDMFactory& cdmFactory();
#endif
    RemoteMediaEngineConfigurationFactory& mediaEngineConfigurationFactory();
#endif // ENABLE(GPU_PROCESS)

    LibWebRTCNetwork& libWebRTCNetwork();

    void setCacheModel(CacheModel);

    void pageDidEnterWindow(WebCore::PageIdentifier);
    void pageWillLeaveWindow(WebCore::PageIdentifier);

    void nonVisibleProcessGraphicsCleanupTimerFired();

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    void nonVisibleProcessMemoryCleanupTimerFired();
#endif

    void registerStorageAreaMap(StorageAreaMap&);
    void unregisterStorageAreaMap(StorageAreaMap&);
    WeakPtr<StorageAreaMap> storageAreaMap(StorageAreaMapIdentifier) const;

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> sourceApplicationAuditData() const;
    void destroyRenderingResources();
    void getProcessDisplayName(CompletionHandler<void(String&&)>&&);
    std::optional<audit_token_t> auditTokenForSelf();
#endif

    const String& uiProcessBundleIdentifier() const { return m_uiProcessBundleIdentifier; }

    void updateActivePages(const String& overrideDisplayName);
    void getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&&);
    void pageActivityStateDidChange(WebCore::PageIdentifier, OptionSet<WebCore::ActivityState::Flag> changed);

    void setHiddenPageDOMTimerThrottlingIncreaseLimit(int milliseconds);

    void prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&&);
    void processDidResume();

    void sendPrewarmInformation(const URL&);

    void isJITEnabled(CompletionHandler<void(bool)>&&);

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices() const { return m_hasImageServices; }
    bool hasSelectionServices() const { return m_hasSelectionServices; }
    bool hasRichContentServices() const { return m_hasRichContentServices; }
#endif

    WebCore::ApplicationCacheStorage& applicationCacheStorage() { return *m_applicationCacheStorage; }

    void prefetchDNS(const String&);

    WebAutomationSessionProxy* automationSessionProxy() { return m_automationSessionProxy.get(); }

    WebCacheStorageProvider& cacheStorageProvider() { return m_cacheStorageProvider.get(); }
    WebBroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry.get(); }
    WebCookieJar& cookieJar() { return m_cookieJar.get(); }
    WebSocketChannelManager& webSocketChannelManager() { return m_webSocketChannelManager; }

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float backlightLevel() const { return m_backlightLevel; }
#endif

#if PLATFORM(COCOA)
    void setMediaMIMETypes(const Vector<String>);
#if ENABLE(REMOTE_INSPECTOR)
    void enableRemoteWebInspector();
#endif
    void unblockServicesRequiredByAccessibility(const Vector<SandboxExtension::Handle>&);
#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif
    void powerSourceDidChange(bool);
#endif

#if PLATFORM(MAC)
    void openDirectoryCacheInvalidated(SandboxExtension::Handle&&);
#endif

    bool areAllPagesThrottleable() const;

    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

#if ENABLE(SERVICE_WORKER)
    void addServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);
    bool removeServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);
#endif

    void grantAccessToAssetServices(WebKit::SandboxExtension::Handle&& mobileAssetV2Handle);
    void revokeAccessToAssetServices();
    void switchFromStaticFontRegistryToUserFontRegistry(WebKit::SandboxExtension::Handle&& fontMachExtensionHandle);

    void disableURLSchemeCheckInDataDetectors() const;

#if PLATFORM(MAC)
    void updatePageScreenProperties();
#endif

#if ENABLE(GPU_PROCESS)
    void setUseGPUProcessForCanvasRendering(bool);
    void setUseGPUProcessForDOMRendering(bool);
    void setUseGPUProcessForMedia(bool);
    bool shouldUseRemoteRenderingFor(WebCore::RenderingPurpose);
#if ENABLE(WEBGL)
    void setUseGPUProcessForWebGL(bool);
    bool shouldUseRemoteRenderingForWebGL() const;
#endif
#endif

#if PLATFORM(COCOA)
    void willWriteToPasteboardAsynchronously(const String& pasteboardName);
    void waitForPendingPasteboardWritesToFinish(const String& pasteboardName);
    void didWriteToPasteboardAsynchronously(const String& pasteboardName);
#endif

#if ENABLE(MEDIA_STREAM)
    SpeechRecognitionRealtimeMediaSourceManager& ensureSpeechRecognitionRealtimeMediaSourceManager();
#endif

    bool isCaptivePortalModeEnabled() const { return m_isCaptivePortalModeEnabled; }

    void setHadMainFrameMainResourcePrivateRelayed() { m_hadMainFrameMainResourcePrivateRelayed = true; }
    bool hadMainFrameMainResourcePrivateRelayed() const { return m_hadMainFrameMainResourcePrivateRelayed; }

    void deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&&);

private:
    WebProcess();
    ~WebProcess();

    void initializeWebProcess(WebProcessCreationParameters&&);
    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void setWebsiteDataStoreParameters(WebProcessDataStoreParameters&&);
    void platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&&);

    void prewarmGlobally();
    void prewarmWithDomainInformation(WebCore::PrewarmInformation&&);

#if USE(OS_STATE)
    RetainPtr<NSDictionary> additionalStateForDiagnosticReport() const final;
#endif

    void markAllLayersVolatile(CompletionHandler<void()>&&);
    void cancelMarkAllLayersVolatile();

    void freezeAllLayerTrees();
    void unfreezeAllLayerTrees();

    void processSuspensionCleanupTimerFired();

    void platformTerminate();

    void setHasSuspendedPageProxy(bool);
    void setIsInProcessCache(bool);
    void markIsNoLongerPrewarmed();

    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&) const;
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&) const;
    void setDomainRelaxationForbiddenForURLScheme(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
    void registerURLSchemeAsNoAccess(const String&) const;
    void registerURLSchemeAsDisplayIsolated(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&);
    void registerURLSchemeAsAlwaysRevalidated(const String&) const;
    void registerURLSchemeAsCachePartitioned(const String&) const;
    void registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String&) const;

    void setDefaultRequestTimeoutInterval(double);
    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothingForTesting(bool);
    void setResourceLoadStatisticsEnabled(bool);
    void clearResourceLoadStatistics();
    void flushResourceLoadStatistics();
    void seedResourceLoadStatisticsForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&&);
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    void platformSetCacheModel(CacheModel);

    void setEnhancedAccessibility(bool);
    
    void startMemorySampler(SandboxExtension::Handle&&, const String&, const double);
    void stopMemorySampler();
    
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void backgroundResponsivenessPing();

#if ENABLE(GAMEPAD)
    void setInitialGamepads(const Vector<GamepadData>&);
    void gamepadConnected(const GamepadData&, WebCore::EventMakesGamepadsVisible);
    void gamepadDisconnected(unsigned index);
#endif

    void establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType, PageGroupIdentifier, WebPageProxyIdentifier, WebCore::PageIdentifier, const WebPreferencesStore&, WebCore::RegistrableDomain&&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, RemoteWorkerInitializationData&&, CompletionHandler<void()>&&);

    void fetchWebsiteData(OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData&&)>&&);
    void deleteWebsiteData(OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);

    void setMemoryCacheDisabled(bool);

    void setBackForwardCacheCapacity(unsigned);
    void clearCachedPage(WebCore::BackForwardItemIdentifier, CompletionHandler<void()>&&);

#if ENABLE(SERVICE_CONTROLS)
    void setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices);
#endif

    void handleInjectedBundleMessage(const String& messageName, const UserData& messageBody);
    void setInjectedBundleParameter(const String& key, const IPC::DataReference&);
    void setInjectedBundleParameters(const IPC::DataReference&);

    bool areAllPagesSuspended() const;

    void ensureAutomationSessionProxy(const String& sessionIdentifier);
    void destroyAutomationSessionProxy();

    void logDiagnosticMessageForNetworkProcessCrash();
    bool hasVisibleWebPage() const;
    void updateCPULimit();
    enum class CPUMonitorUpdateReason { LimitHasChanged, VisibilityHasChanged };
    void updateCPUMonitorState(CPUMonitorUpdateReason);

    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    void initializeConnection(IPC::Connection*) override;
    bool shouldTerminate() override;
    void terminate() override;

#if USE(APPKIT) || PLATFORM(GTK) || PLATFORM(WPE)
    void stopRunLoop() override;
#endif

#if ENABLE(MEDIA_STREAM)
    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String& persistentId);
    void resetMockMediaDevices();
#if ENABLE(SANDBOX_EXTENSIONS)
    void grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&&);
    void revokeUserMediaDeviceSandboxExtensions(const Vector<String>&);
#endif

#endif

#if ENABLE(TRACKING_PREVENTION)
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&&, CompletionHandler<void()>&&);
    void sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&&);
#endif

#if HAVE(CVDISPLAYLINK)
    void displayWasRefreshed(uint32_t displayID, const WebCore::DisplayUpdate&);
#endif

#if PLATFORM(MAC)
    void systemWillPowerOn();
    void systemWillSleep();
    void systemDidWake();
#endif

    void platformInitializeProcess(const AuxiliaryProcessInitializationParameters&);

    // IPC::Connection::Client
    friend class WebConnectionToUIProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // Implemented in generated WebProcessMessageReceiver.cpp
    void didReceiveWebProcessMessage(IPC::Connection&, IPC::Decoder&);

#if PLATFORM(MAC)
    void scrollerStylePreferenceChanged(bool useOverlayScrollbars);
    void displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
#endif

#if PLATFORM(COCOA)
    void setScreenProperties(const WebCore::ScreenProperties&);

    enum class IsInProcessInitialization : bool { No, Yes };
    void updateProcessName(IsInProcessInitialization);
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    void backlightLevelDidChange(float backlightLevel);
#endif

    void accessibilityPreferencesDidChange(const AccessibilityPreferences&);
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    void setMediaAccessibilityPreferences(WebCore::CaptionUserPreferences::CaptionDisplayMode, const Vector<String>&);
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void colorPreferencesDidChange();
#endif

#if PLATFORM(IOS_FAMILY)
    void userInterfaceIdiomDidChange(bool);

    bool shouldFreezeOnSuspension() const;
    void updateFreezerStatus();
#endif

#if ENABLE(VIDEO)
    void suspendAllMediaBuffering();
    void resumeAllMediaBuffering();
#endif

    void clearCurrentModifierStateForTesting();

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebExtension(UserMessage&&);
#endif

#if PLATFORM(GTK) && !USE(GTK4)
    void setUseSystemAppearanceForScrollbars(bool);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void handlePreferenceChange(const String& domain, const String& key, id value) final;
    void dispatchSimulatedNotificationsForPreferenceChange(const String& key) final;
#endif

    RefPtr<WebConnectionToUIProcess> m_webConnection;

    HashMap<WebCore::PageIdentifier, RefPtr<WebPage>> m_pageMap;
    HashMap<PageGroupIdentifier, RefPtr<WebPageGroupProxy>> m_pageGroupMap;
    RefPtr<InjectedBundle> m_injectedBundle;

    EventDispatcher m_eventDispatcher;
#if PLATFORM(IOS_FAMILY)
    ViewUpdateDispatcher m_viewUpdateDispatcher;
#endif
    WebInspectorInterruptDispatcher m_webInspectorInterruptDispatcher;

    bool m_hasSetCacheModel { false };
    CacheModel m_cacheModel { CacheModel::DocumentViewer };

#if PLATFORM(COCOA)
    WTF::MachSendRight m_compositingRenderServerPort;
#endif

    bool m_fullKeyboardAccessEnabled { false };

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool m_hasMouseDevice { false };
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool m_hasStylusDevice { false };
#endif

    HashMap<WebCore::FrameIdentifier, WebFrame*> m_frameMap;

    typedef HashMap<const char*, std::unique_ptr<WebProcessSupplement>, PtrHash<const char*>> WebProcessSupplementMap;
    WebProcessSupplementMap m_supplements;

    TextCheckerState m_textCheckerState;

    String m_uiProcessBundleIdentifier;
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    WebLoaderStrategy& m_webLoaderStrategy;
    RefPtr<WebFileSystemStorageConnection> m_fileSystemStorageConnection;

#if ENABLE(GPU_PROCESS)
    RefPtr<GPUProcessConnection> m_gpuProcessConnection;
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    RefPtr<LibWebRTCCodecs> m_libWebRTCCodecs;
#if ENABLE(WEB_CODECS)
    RemoteVideoCodecFactory m_remoteVideoCodecFactory;
#endif
#endif
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    std::unique_ptr<AudioMediaStreamTrackRendererInternalUnitManager> m_audioMediaStreamTrackRendererInternalUnitManager;
#endif
#endif
    Ref<WebCacheStorageProvider> m_cacheStorageProvider;
    Ref<WebBroadcastChannelRegistry> m_broadcastChannelRegistry;
    Ref<WebCookieJar> m_cookieJar;
    WebSocketChannelManager m_webSocketChannelManager;

    std::unique_ptr<LibWebRTCNetwork> m_libWebRTCNetwork;

    HashSet<String> m_dnsPrefetchedHosts;
    PAL::HysteresisActivity m_dnsPrefetchHystereris;

    std::unique_ptr<WebAutomationSessionProxy> m_automationSessionProxy;

#if ENABLE(SERVICE_CONTROLS)
    bool m_hasImageServices { false };
    bool m_hasSelectionServices { false };
    bool m_hasRichContentServices { false };
#endif

    bool m_processIsSuspended { false };

    HashSet<WebCore::PageIdentifier> m_pagesInWindows;
    WebCore::Timer m_nonVisibleProcessGraphicsCleanupTimer;

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    WebCore::Timer m_nonVisibleProcessMemoryCleanupTimer;
#endif

    RefPtr<WebCore::ApplicationCacheStorage> m_applicationCacheStorage;

#if PLATFORM(IOS_FAMILY)
    WebSQLiteDatabaseTracker m_webSQLiteDatabaseTracker;
#endif

    bool m_suppressMemoryPressureHandler { false };
    bool m_loggedProcessLimitWarningMemoryStatistics { false };
    bool m_loggedProcessLimitCriticalMemoryStatistics { false };
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::CPUMonitor> m_cpuMonitor;
    std::optional<double> m_cpuLimit;

    String m_uiProcessName;
    WebCore::RegistrableDomain m_registrableDomain;
#endif

#if PLATFORM(COCOA)
    enum class ProcessType { Inspector, ServiceWorker, PrewarmedWebContent, CachedWebContent, WebContent };
    ProcessType m_processType { ProcessType::WebContent };
#endif

    HashMap<WebCore::UserGestureToken *, uint64_t> m_userGestureTokens;

#if PLATFORM(WAYLAND)
    std::unique_ptr<WebCore::PlatformDisplayLibWPE> m_wpeDisplay;
#endif

    bool m_hasSuspendedPageProxy { false };
    bool m_isSuspending { false };
    bool m_isCaptivePortalModeEnabled { false };

#if ENABLE(MEDIA_STREAM) && ENABLE(SANDBOX_EXTENSIONS)
    HashMap<String, RefPtr<SandboxExtension>> m_mediaCaptureSandboxExtensions;
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float m_backlightLevel { 0 };
#endif

#if ENABLE(SERVICE_WORKER)
    HashCountedSet<WebCore::ServiceWorkerRegistrationIdentifier> m_swRegistrationCounts;
#endif

    HashMap<StorageAreaMapIdentifier, WeakPtr<StorageAreaMap>> m_storageAreaMaps;
    
    // Prewarmed WebProcesses do not have an associated sessionID yet, which is why this is an optional.
    // By the time the WebProcess gets a WebPage, it is guaranteed to have a sessionID.
    std::optional<PAL::SessionID> m_sessionID;

#if ENABLE(TRACKING_PREVENTION)
    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
#endif

    RefPtr<SandboxExtension> m_assetServiceV2Extension;

#if PLATFORM(COCOA)
    HashCountedSet<String> m_pendingPasteboardWriteCounts;
#endif

#if ENABLE(GPU_PROCESS)
    bool m_useGPUProcessForCanvasRendering { false };
    bool m_useGPUProcessForDOMRendering { false };
    bool m_useGPUProcessForMedia { false };
#if ENABLE(WEBGL)
    bool m_useGPUProcessForWebGL { false };
#endif
#endif

#if ENABLE(MEDIA_STREAM)
    std::unique_ptr<SpeechRecognitionRealtimeMediaSourceManager> m_speechRecognitionRealtimeMediaSourceManager;
#endif
    bool m_hadMainFrameMainResourcePrivateRelayed { false };
};

} // namespace WebKit
