/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "AuxiliaryProcess.h"
#include "CacheModel.h"
#if ENABLE(MEDIA_STREAM)
#include "MediaDeviceSandboxExtensions.h"
#endif
#include "PluginProcessConnectionManager.h"
#include "SandboxExtension.h"
#include "StorageAreaIdentifier.h"
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

#if PLATFORM(COCOA)
#include <WebCore/ScreenProperties.h>
#include <dispatch/dispatch.h>
#include <wtf/MachSendRight.h>
#endif

#if PLATFORM(WAYLAND) && USE(WPE_RENDERER)
#include <WebCore/PlatformDisplayLibWPE.h>
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
struct BackForwardItemIdentifier;
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

class EventDispatcher;
class GamepadData;
class GPUProcessConnection;
class InjectedBundle;
class LibWebRTCCodecs;
class LibWebRTCNetwork;
class NetworkProcessConnection;
class ObjCObjectGraph;
class ProcessAssertion;
struct ServiceWorkerInitializationData;
class StorageAreaMap;
class UserData;
class WaylandCompositorDisplay;
class WebAutomationSessionProxy;
class WebCacheStorageProvider;
class WebCookieJar;
class WebCompiledContentRuleListData;
class WebConnectionToUIProcess;
class WebFrame;
class WebLoaderStrategy;
class WebPage;
class WebPageGroupProxy;
struct UserMessage;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;
class WebProcessSupplement;
enum class WebsiteDataType;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebsiteData;
struct WebsiteDataStoreParameters;

#if PLATFORM(IOS_FAMILY)
class LayerHostingContext;
#endif

class WebProcess : public AuxiliaryProcess
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static WebProcess& singleton();
    static constexpr ProcessType processType = ProcessType::WebContent;

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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const { return m_thirdPartyCookieBlockingMode; }
#endif

#if PLATFORM(COCOA)
    const WTF::MachSendRight& compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif

    bool shouldPlugInAutoStartFromOrigin(WebPage&, const String& pageOrigin, const String& pluginOrigin, const String& mimeType);
    void plugInDidStartFromOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType);
    void plugInDidReceiveUserInteraction(const String& pageOrigin, const String& pluginOrigin, const String& mimeType);
    void setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString);
    void resetPluginLoadClientPolicies(const HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>>&);
    void clearPluginClientPolicies();
    void refreshPlugins();

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

    WebFrame* webFrame(WebCore::FrameIdentifier) const;
    Vector<WebFrame*> webFrames() const;
    void addWebFrame(WebCore::FrameIdentifier, WebFrame*);
    void removeWebFrame(WebCore::FrameIdentifier);

    WebPageGroupProxy* webPageGroup(WebCore::PageGroup*);
    WebPageGroupProxy* webPageGroup(uint64_t pageGroupID);
    WebPageGroupProxy* webPageGroup(const WebPageGroupData&);

    uint64_t userGestureTokenIdentifier(RefPtr<WebCore::UserGestureToken>);
    void userGestureTokenDestroyed(WebCore::UserGestureToken&);
    
    const TextCheckerState& textCheckerState() const { return m_textCheckerState; }
    void setTextCheckerState(const TextCheckerState&);
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginProcessConnectionManager& pluginProcessConnectionManager();
#endif

    EventDispatcher& eventDispatcher() { return m_eventDispatcher.get(); }

    NetworkProcessConnection& ensureNetworkProcessConnection();
    void networkProcessConnectionClosed(NetworkProcessConnection*);
    NetworkProcessConnection* existingNetworkProcessConnection() { return m_networkProcessConnection.get(); }
    WebLoaderStrategy& webLoaderStrategy();

#if ENABLE(GPU_PROCESS)
    GPUProcessConnection& ensureGPUProcessConnection();
    void gpuProcessConnectionClosed(GPUProcessConnection*);
    GPUProcessConnection* existingGPUProcessConnection() { return m_gpuProcessConnection.get(); }

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    LibWebRTCCodecs& libWebRTCCodecs();
#endif

#endif // ENABLE(GPU_PROCESS)

    LibWebRTCNetwork& libWebRTCNetwork();

    void setCacheModel(CacheModel);

    void pageDidEnterWindow(WebCore::PageIdentifier);
    void pageWillLeaveWindow(WebCore::PageIdentifier);

    void nonVisibleProcessCleanupTimerFired();

    void registerStorageAreaMap(StorageAreaMap&);
    void unregisterStorageAreaMap(StorageAreaMap&);
    StorageAreaMap* storageAreaMap(StorageAreaIdentifier) const;

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> sourceApplicationAuditData() const;
    void destroyRenderingResources();
#endif

    const String& uiProcessBundleIdentifier() const { return m_uiProcessBundleIdentifier; }

    void updateActivePages(const String& overrideDisplayName);
    void getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&&);
    void pageActivityStateDidChange(WebCore::PageIdentifier, OptionSet<WebCore::ActivityState::Flag> changed);

    void setHiddenPageDOMTimerThrottlingIncreaseLimit(int milliseconds);

    void prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&&);
    void processDidResume();

    void sendPrewarmInformation(const URL&);

    void isJITEnabled(CompletionHandler<void(bool)>&&);

#if PLATFORM(IOS_FAMILY)
    void resetAllGeolocationPermissions();
#endif

#if PLATFORM(WAYLAND)
    WaylandCompositorDisplay* waylandCompositorDisplay() const { return m_waylandCompositorDisplay.get(); }
#endif

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
    WebCookieJar& cookieJar() { return m_cookieJar.get(); }
    WebSocketChannelManager& webSocketChannelManager() { return m_webSocketChannelManager; }

#if PLATFORM(IOS_FAMILY)
    void accessibilityProcessSuspendedNotification(bool);
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float backlightLevel() const { return m_backlightLevel; }
#endif

#if PLATFORM(COCOA)
    void setMediaMIMETypes(const Vector<String>);
#if ENABLE(REMOTE_INSPECTOR)
    void enableRemoteWebInspector(const SandboxExtension::Handle&);
#endif
    void unblockServicesRequiredByAccessibility(const SandboxExtension::HandleArray&);
#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const Optional<String>& encodedValue);
    void unblockPreferenceService(SandboxExtension::HandleArray&&);
#endif
#endif

    bool areAllPagesThrottleable() const;

    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

#if ENABLE(SERVICE_WORKER)
    void addServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);
    bool removeServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);
#endif

#if PLATFORM(IOS)
    void grantAccessToAssetServices(WebKit::SandboxExtension::Handle&& mobileAssetHandle,  WebKit::SandboxExtension::Handle&& mobileAssetV2Handle);
    void revokeAccessToAssetServices();
#endif

#if PLATFORM(MAC)
    void updatePageScreenProperties();
#endif

#if ENABLE(GPU_PROCESS)
    void setUseGPUProcessForMedia(bool);
#endif

private:
    WebProcess();
    ~WebProcess();

    void initializeWebProcess(WebProcessCreationParameters&&);
    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void setWebsiteDataStoreParameters(WebProcessDataStoreParameters&&);
    void platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&&);

    void prewarmGlobally();
    void prewarmWithDomainInformation(const WebCore::PrewarmInformation&);

#if USE(OS_STATE)
    void registerWithStateDumper();
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
    void setShouldUseFontSmoothing(bool);
    void setResourceLoadStatisticsEnabled(bool);
    void clearResourceLoadStatistics();
    void flushResourceLoadStatistics();
    void seedResourceLoadStatisticsForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&&);
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    bool isPlugInAutoStartOriginHash(unsigned plugInOriginHash);
    void didAddPlugInAutoStartOriginHash(unsigned plugInOriginHash, WallTime expirationTime);
    void resetPlugInAutoStartOriginHashes(HashMap<unsigned, WallTime>&& hashes);

    void platformSetCacheModel(CacheModel);

    void setEnhancedAccessibility(bool);
    
    void startMemorySampler(SandboxExtension::Handle&&, const String&, const double);
    void stopMemorySampler();
    
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void mainThreadPing();
    void backgroundResponsivenessPing();

#if ENABLE(GAMEPAD)
    void setInitialGamepads(const Vector<GamepadData>&);
    void gamepadConnected(const GamepadData&);
    void gamepadDisconnected(unsigned index);
#endif

#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(uint64_t pageGroupID, WebPageProxyIdentifier, WebCore::PageIdentifier, const WebPreferencesStore&, WebCore::RegistrableDomain&&, ServiceWorkerInitializationData&&, CompletionHandler<void()>&&);
#endif

    void fetchWebsiteData(OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData&&)>&&);
    void deleteWebsiteData(OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&&);

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

#if USE(APPKIT)
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
#endif

    void platformInitializeProcess(const AuxiliaryProcessInitializationParameters&);

    // IPC::Connection::Client
    friend class WebConnectionToUIProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Implemented in generated WebProcessMessageReceiver.cpp
    void didReceiveWebProcessMessage(IPC::Connection&, IPC::Decoder&);

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    void scrollerStylePreferenceChanged(bool useOverlayScrollbars);
    void displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
#endif

#if PLATFORM(COCOA)
    void setScreenProperties(const WebCore::ScreenProperties&);
    void updateProcessName();
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    void backlightLevelDidChange(float backlightLevel);
#endif

#if PLATFORM(IOS_FAMILY)
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

    bool isAlwaysOnLoggingAllowed() { return m_sessionID ? m_sessionID->isAlwaysOnLoggingAllowed() : true; }

    RefPtr<WebConnectionToUIProcess> m_webConnection;

    HashMap<WebCore::PageIdentifier, RefPtr<WebPage>> m_pageMap;
    HashMap<uint64_t, RefPtr<WebPageGroupProxy>> m_pageGroupMap;
    RefPtr<InjectedBundle> m_injectedBundle;

    Ref<EventDispatcher> m_eventDispatcher;
#if PLATFORM(IOS_FAMILY)
    RefPtr<ViewUpdateDispatcher> m_viewUpdateDispatcher;
#endif
    RefPtr<WebInspectorInterruptDispatcher> m_webInspectorInterruptDispatcher;

    HashMap<unsigned, WallTime> m_plugInAutoStartOriginHashes;
    HashSet<String> m_plugInAutoStartOrigins;

    bool m_hasSetCacheModel { false };
    CacheModel m_cacheModel { CacheModel::DocumentViewer };

#if PLATFORM(COCOA)
    WTF::MachSendRight m_compositingRenderServerPort;
#endif

    bool m_fullKeyboardAccessEnabled { false };

    HashMap<WebCore::FrameIdentifier, WebFrame*> m_frameMap;

    typedef HashMap<const char*, std::unique_ptr<WebProcessSupplement>, PtrHash<const char*>> WebProcessSupplementMap;
    WebProcessSupplementMap m_supplements;

    TextCheckerState m_textCheckerState;

    String m_uiProcessBundleIdentifier;
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    WebLoaderStrategy& m_webLoaderStrategy;

#if ENABLE(GPU_PROCESS)
    RefPtr<GPUProcessConnection> m_gpuProcessConnection;
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    std::unique_ptr<LibWebRTCCodecs> m_libWebRTCCodecs;
#endif
#endif

    Ref<WebCacheStorageProvider> m_cacheStorageProvider;
    Ref<WebCookieJar> m_cookieJar;
    WebSocketChannelManager m_webSocketChannelManager;

    std::unique_ptr<LibWebRTCNetwork> m_libWebRTCNetwork;

    HashSet<String> m_dnsPrefetchedHosts;
    PAL::HysteresisActivity m_dnsPrefetchHystereris;

    std::unique_ptr<WebAutomationSessionProxy> m_automationSessionProxy;

#if ENABLE(NETSCAPE_PLUGIN_API)
    RefPtr<PluginProcessConnectionManager> m_pluginProcessConnectionManager;
#endif

#if ENABLE(SERVICE_CONTROLS)
    bool m_hasImageServices { false };
    bool m_hasSelectionServices { false };
    bool m_hasRichContentServices { false };
#endif

    bool m_processIsSuspended { false };

    HashSet<WebCore::PageIdentifier> m_pagesInWindows;
    WebCore::Timer m_nonVisibleProcessCleanupTimer;

    RefPtr<WebCore::ApplicationCacheStorage> m_applicationCacheStorage;

#if PLATFORM(IOS_FAMILY)
    WebSQLiteDatabaseTracker m_webSQLiteDatabaseTracker;
#endif

    bool m_suppressMemoryPressureHandler { false };
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::CPUMonitor> m_cpuMonitor;
    Optional<double> m_cpuLimit;

    String m_uiProcessName;
    WebCore::RegistrableDomain m_registrableDomain;
#endif

#if PLATFORM(COCOA)
    enum class ProcessType { Inspector, ServiceWorker, PrewarmedWebContent, CachedWebContent, WebContent };
    ProcessType m_processType { ProcessType::WebContent };
#endif

    HashMap<WebCore::UserGestureToken *, uint64_t> m_userGestureTokens;

#if PLATFORM(WAYLAND)
    std::unique_ptr<WaylandCompositorDisplay> m_waylandCompositorDisplay;
#endif

#if PLATFORM(WAYLAND) && USE(WPE_RENDERER)
    std::unique_ptr<WebCore::PlatformDisplayLibWPE> m_wpeDisplay;
#endif

    bool m_hasSuspendedPageProxy { false };
    bool m_isSuspending { false };

#if ENABLE(MEDIA_STREAM) && ENABLE(SANDBOX_EXTENSIONS)
    HashMap<String, RefPtr<SandboxExtension>> m_mediaCaptureSandboxExtensions;
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float m_backlightLevel { 0 };
#endif

#if ENABLE(SERVICE_WORKER)
    HashCountedSet<WebCore::ServiceWorkerRegistrationIdentifier> m_swRegistrationCounts;
#endif

    HashMap<StorageAreaIdentifier, StorageAreaMap*> m_storageAreaMaps;
    
    // Prewarmed WebProcesses do not have an associated sessionID yet, which is why this is an optional.
    // By the time the WebProcess gets a WebPage, it is guaranteed to have a sessionID.
    Optional<PAL::SessionID> m_sessionID;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
#endif

#if PLATFORM(IOS)
    RefPtr<SandboxExtension> m_assetServiceExtension;
    RefPtr<SandboxExtension> m_assetServiceV2Extension;
#endif

    bool m_useGPUProcessForMedia { false };
};

} // namespace WebKit
