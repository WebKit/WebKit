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

#include "CacheModel.h"
#include "ChildProcess.h"
#include "PluginProcessConnectionManager.h"
#include "ResourceCachesToClear.h"
#include "SandboxExtension.h"
#include "TextCheckerState.h"
#include "ViewUpdateDispatcher.h"
#include "WebInspectorInterruptDispatcher.h"
#include <WebCore/ActivityState.h>
#if PLATFORM(MAC)
#include <WebCore/ScreenProperties.h>
#endif
#include <WebCore/Timer.h>
#include <pal/HysteresisActivity.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounter.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#if PLATFORM(COCOA)
#include <dispatch/dispatch.h>
#include <wtf/MachSendRight.h>
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
class ResourceRequest;
class UserGestureToken;
struct MessagePortIdentifier;
struct MessageWithMessagePorts;
struct MockMediaDevice;
struct PluginInfo;
struct PrewarmInformation;
struct SecurityOriginData;
struct SoupNetworkProxySettings;

#if ENABLE(SERVICE_WORKER)
struct ServiceWorkerContextData;
#endif
}

namespace WebKit {

class EventDispatcher;
class GamepadData;
class InjectedBundle;
class LibWebRTCNetwork;
class NetworkProcessConnection;
class ObjCObjectGraph;
class UserData;
class WaylandCompositorDisplay;
class WebAutomationSessionProxy;
class WebCacheStorageProvider;
class WebConnectionToUIProcess;
class WebFrame;
class WebLoaderStrategy;
class WebPage;
class WebPageGroupProxy;
class WebProcessSupplement;
enum class WebsiteDataType;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebProcessCreationParameters;
struct WebsiteData;
struct WebsiteDataStoreParameters;

class WebProcess : public ChildProcess {
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
        m_supplements.add(T::supplementName(), std::make_unique<T>(*this));
    }

    WebConnectionToUIProcess* webConnectionToUIProcess() const { return m_webConnection.get(); }

    WebPage* webPage(uint64_t pageID) const;
    void createWebPage(uint64_t pageID, WebPageCreationParameters&&);
    void removeWebPage(uint64_t pageID);
    WebPage* focusedWebPage() const;

    InjectedBundle* injectedBundle() const { return m_injectedBundle.get(); }

#if PLATFORM(COCOA)
    const WTF::MachSendRight& compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif

    bool shouldPlugInAutoStartFromOrigin(WebPage&, const String& pageOrigin, const String& pluginOrigin, const String& mimeType);
    void plugInDidStartFromOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, PAL::SessionID);
    void plugInDidReceiveUserInteraction(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, PAL::SessionID);
    void setPluginLoadClientPolicy(uint8_t policy, const String& host, const String& bundleIdentifier, const String& versionString);
    void resetPluginLoadClientPolicies(const HashMap<String, HashMap<String, HashMap<String, uint8_t>>>&);
    void clearPluginClientPolicies();
    void refreshPlugins();

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

    WebFrame* webFrame(uint64_t) const;
    void addWebFrame(uint64_t, WebFrame*);
    void removeWebFrame(uint64_t);

    WebPageGroupProxy* webPageGroup(WebCore::PageGroup*);
    WebPageGroupProxy* webPageGroup(uint64_t pageGroupID);
    WebPageGroupProxy* webPageGroup(const WebPageGroupData&);

    uint64_t userGestureTokenIdentifier(RefPtr<WebCore::UserGestureToken>);
    void userGestureTokenDestroyed(WebCore::UserGestureToken&);
    
    const TextCheckerState& textCheckerState() const { return m_textCheckerState; }
    void setTextCheckerState(const TextCheckerState&);

    void clearResourceCaches(ResourceCachesToClear = AllResourceCaches);
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginProcessConnectionManager& pluginProcessConnectionManager();
#endif

    EventDispatcher& eventDispatcher() { return *m_eventDispatcher; }

    NetworkProcessConnection& ensureNetworkProcessConnection();
    void networkProcessConnectionClosed(NetworkProcessConnection*);
    NetworkProcessConnection* existingNetworkProcessConnection() { return m_networkProcessConnection.get(); }
    WebLoaderStrategy& webLoaderStrategy();

    LibWebRTCNetwork& libWebRTCNetwork();

    void setCacheModel(CacheModel);

    void ensureLegacyPrivateBrowsingSessionInNetworkProcess();

    void pageDidEnterWindow(uint64_t pageID);
    void pageWillLeaveWindow(uint64_t pageID);

    void nonVisibleProcessCleanupTimerFired();

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> sourceApplicationAuditData() const;
    void destroyRenderingResources();
#endif

    const String& uiProcessBundleIdentifier() const { return m_uiProcessBundleIdentifier; }

    void updateActivePages();
    void getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&&);
    void pageActivityStateDidChange(uint64_t pageID, OptionSet<WebCore::ActivityState::Flag> changed);

    void setHiddenPageDOMTimerThrottlingIncreaseLimit(int milliseconds);

    void processWillSuspendImminently(bool& handled);
    void prepareToSuspend();
    void cancelPrepareToSuspend();
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

#if PLATFORM(IOS_FAMILY)
    void accessibilityProcessSuspendedNotification(bool);
#endif

#if PLATFORM(COCOA)
    void setMediaMIMETypes(const Vector<String>);
#endif

private:
    WebProcess();
    ~WebProcess();

    void initializeWebProcess(WebProcessCreationParameters&&);
    void platformInitializeWebProcess(WebProcessCreationParameters&&);

    void prewarmGlobally();
    void prewarmWithDomainInformation(const WebCore::PrewarmInformation&);

#if USE(OS_STATE)
    void registerWithStateDumper();
#endif

    void markAllLayersVolatile(WTF::Function<void(bool)>&& completionHandler);
    void cancelMarkAllLayersVolatile();

    void freezeAllLayerTrees();
    void unfreezeAllLayerTrees();

    void processSuspensionCleanupTimerFired();

    void platformTerminate();

    void markIsNoLongerPrewarmed();

    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&) const;
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&) const;
    void setDomainRelaxationForbiddenForURLScheme(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
    void registerURLSchemeAsNoAccess(const String&) const;
    void registerURLSchemeAsDisplayIsolated(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&) const;
    void registerURLSchemeAsAlwaysRevalidated(const String&) const;
    void registerURLSchemeAsCachePartitioned(const String&) const;
    void registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String&) const;

    void setDefaultRequestTimeoutInterval(double);
    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    void setResourceLoadStatisticsEnabled(bool);
    void clearResourceLoadStatistics();
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    bool isPlugInAutoStartOriginHash(unsigned plugInOriginHash, PAL::SessionID);
    void didAddPlugInAutoStartOriginHash(unsigned plugInOriginHash, WallTime expirationTime, PAL::SessionID);
    void resetPlugInAutoStartOriginDefaultHashes(const HashMap<unsigned, WallTime>& hashes);
    void resetPlugInAutoStartOriginHashes(const HashMap<PAL::SessionID, HashMap<unsigned, WallTime>>& hashes);

    void platformSetCacheModel(CacheModel);

    void setEnhancedAccessibility(bool);
    
    void startMemorySampler(SandboxExtension::Handle&&, const String&, const double);
    void stopMemorySampler();
    
    void getWebCoreStatistics(uint64_t callbackID);
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void mainThreadPing();
    void backgroundResponsivenessPing();

    void didTakeAllMessagesForPort(Vector<WebCore::MessageWithMessagePorts>&& messages, uint64_t messageCallbackIdentifier, uint64_t messageBatchIdentifier);
    void checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier&, uint64_t callbackIdentifier);
    void didCheckRemotePortForActivity(uint64_t callbackIdentifier, bool hasActivity);
    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

#if ENABLE(GAMEPAD)
    void setInitialGamepads(const Vector<GamepadData>&);
    void gamepadConnected(const GamepadData&);
    void gamepadDisconnected(unsigned index);
#endif
#if USE(SOUP)
    void setNetworkProxySettings(const WebCore::SoupNetworkProxySettings&);
#endif
#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(uint64_t pageGroupID, uint64_t pageID, const WebPreferencesStore&, PAL::SessionID);
    void registerServiceWorkerClients();
#endif

    void releasePageCache();

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WebsiteData&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins);

    void setMemoryCacheDisabled(bool);

#if ENABLE(SERVICE_CONTROLS)
    void setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices);
#endif

    void handleInjectedBundleMessage(const String& messageName, const UserData& messageBody);
    void setInjectedBundleParameter(const String& key, const IPC::DataReference&);
    void setInjectedBundleParameters(const IPC::DataReference&);

    enum class ShouldAcknowledgeWhenReadyToSuspend { No, Yes };
    void actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend);

    bool hasPageRequiringPageCacheWhileSuspended() const;

    void ensureAutomationSessionProxy(const String& sessionIdentifier);
    void destroyAutomationSessionProxy();

    void logDiagnosticMessageForNetworkProcessCrash();
    bool hasVisibleWebPage() const;
    void updateCPULimit();
    enum class CPUMonitorUpdateReason { LimitHasChanged, VisibilityHasChanged };
    void updateCPUMonitorState(CPUMonitorUpdateReason);

    // ChildProcess
    void initializeProcess(const ChildProcessInitializationParameters&) override;
    void initializeProcessName(const ChildProcessInitializationParameters&) override;
    void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
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
#endif

    void platformInitializeProcess(const ChildProcessInitializationParameters&);

    // IPC::Connection::Client
    friend class WebConnectionToUIProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Implemented in generated WebProcessMessageReceiver.cpp
    void didReceiveWebProcessMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncWebProcessMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

#if PLATFORM(MAC)
    void updateProcessName();
    void setScreenProperties(const WebCore::ScreenProperties&);
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    void scrollerStylePreferenceChanged(bool useOverlayScrollbars);
    void displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
    void displayWasRefreshed(CGDirectDisplayID);
#endif
#endif

    void clearCurrentModifierStateForTesting();

    RefPtr<WebConnectionToUIProcess> m_webConnection;

    HashMap<uint64_t, RefPtr<WebPage>> m_pageMap;
    HashMap<uint64_t, RefPtr<WebPageGroupProxy>> m_pageGroupMap;
    RefPtr<InjectedBundle> m_injectedBundle;

    RefPtr<EventDispatcher> m_eventDispatcher;
#if PLATFORM(IOS_FAMILY)
    RefPtr<ViewUpdateDispatcher> m_viewUpdateDispatcher;
#endif
    RefPtr<WebInspectorInterruptDispatcher> m_webInspectorInterruptDispatcher;

    HashMap<PAL::SessionID, HashMap<unsigned, WallTime>> m_plugInAutoStartOriginHashes;
    HashSet<String> m_plugInAutoStartOrigins;

    bool m_hasSetCacheModel { false };
    CacheModel m_cacheModel { CacheModel::DocumentViewer };

#if PLATFORM(COCOA)
    WTF::MachSendRight m_compositingRenderServerPort;
#endif

    bool m_fullKeyboardAccessEnabled { false };

    HashMap<uint64_t, WebFrame*> m_frameMap;

    typedef HashMap<const char*, std::unique_ptr<WebProcessSupplement>, PtrHash<const char*>> WebProcessSupplementMap;
    WebProcessSupplementMap m_supplements;

    TextCheckerState m_textCheckerState;

    String m_uiProcessBundleIdentifier;
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    WebLoaderStrategy& m_webLoaderStrategy;

    Ref<WebCacheStorageProvider> m_cacheStorageProvider;

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

    HashSet<uint64_t> m_pagesInWindows;
    WebCore::Timer m_nonVisibleProcessCleanupTimer;

    RefPtr<WebCore::ApplicationCacheStorage> m_applicationCacheStorage;

    enum PageMarkingLayersAsVolatileCounterType { };
    using PageMarkingLayersAsVolatileCounter = RefCounter<PageMarkingLayersAsVolatileCounterType>;
    std::unique_ptr<PageMarkingLayersAsVolatileCounter> m_pageMarkingLayersAsVolatileCounter;
    unsigned m_countOfPagesFailingToMarkVolatile { 0 };

    bool m_suppressMemoryPressureHandler { false };
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::CPUMonitor> m_cpuMonitor;
    Optional<double> m_cpuLimit;

    enum class ProcessType { Inspector, ServiceWorker, PrewarmedWebContent, WebContent };
    ProcessType m_processType { ProcessType::WebContent };
    String m_uiProcessName;
    String m_securityOrigin;
#endif

    HashMap<WebCore::UserGestureToken *, uint64_t> m_userGestureTokens;

#if PLATFORM(WAYLAND)
    std::unique_ptr<WaylandCompositorDisplay> m_waylandCompositorDisplay;
#endif
    bool m_isSuspending { false };
};

} // namespace WebKit
