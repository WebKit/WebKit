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

#include "AuxiliaryProcess.h"
#include "CacheModel.h"
#include "EventDispatcher.h"
#include "IdentifierTypes.h"
#include "NetworkProcessConnection.h"
#include "ScriptTrackingPrivacyFilter.h"
#include "SharedPreferencesForWebProcess.h"
#include "StorageAreaMapIdentifier.h"
#include "TextCheckerState.h"
#include "WebInspectorInterruptDispatcher.h"
#include "WebPageProxyIdentifier.h"
#include "WebSocketChannelManager.h"
#include <WebCore/ActivityState.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/Timer.h>
#include <WebCore/UserGestureTokenIdentifier.h>
#include <pal/HysteresisActivity.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounter.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

#if ENABLE(MEDIA_STREAM)
#include "MediaDeviceSandboxExtensions.h"
#endif

#if ENABLE(WEB_CODECS)
#include "RemoteVideoCodecFactory.h"
#endif

#if PLATFORM(COCOA)
#include <dispatch/dispatch.h>
#include <wtf/MachSendRight.h>

OBJC_CLASS NSMutableDictionary;

#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "AvailableInputDevices.h"
#include "RendererBufferTransportMode.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "ViewUpdateDispatcher.h"
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include <WebCore/CaptionUserPreferences.h>
#endif

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
#include "WasmDebuggerDispatcher.h"
#endif

namespace API {
class Object;
}

namespace PAL {
class SessionID;
enum class UserInterfaceIdiom : uint8_t;
}

namespace WebCore {
class CPUMonitor;
class Frame;
class PageGroup;
class SecurityOriginData;
class Site;
class UserGestureToken;

enum class EventMakesGamepadsVisible : bool;
enum class PlatformMediaSessionRemoteControlCommandType : uint8_t;
enum class RenderAsTextFlag : uint16_t;
enum class RenderingPurpose : uint8_t;
enum class ScriptTrackingPrivacyCategory : uint8_t;

struct ClientOrigin;
struct DisplayUpdate;
struct MessagePortIdentifier;
struct MessageWithMessagePorts;
struct MockMediaDevice;
struct PrewarmInformation;
struct PlatformMediaSessionRemoteCommandArgument;
struct ScreenProperties;
struct ServiceWorkerContextData;
struct JSHandleIdentifierType;

using JSHandleIdentifier = ProcessQualified<ObjectIdentifier<JSHandleIdentifierType>>;
}

namespace WebKit {

class AudioMediaStreamTrackRendererInternalUnitManager;
class AudioSessionRoutingArbitrator;
class GamepadData;
class GPUProcessConnection;
class InjectedBundle;
class LibWebRTCCodecs;
class LibWebRTCNetwork;
class ModelProcessConnection;
class ModelProcessModelPlayerManager;
class RemoteCDMFactory;
class RemoteImageDecoderAVFManager;
class RemoteLegacyCDMFactory;
class RemoteMediaEngineConfigurationFactory;
class RemoteMediaPlayerManager;
class RemoteSharedResourceCacheProxy;
class StorageAreaMap;
class UserData;
class WebAutomationSessionProxy;
class WebBadgeClient;
class WebBroadcastChannelRegistry;
class WebCacheStorageProvider;
class WebCompiledContentRuleListData;
class WebCookieJar;
class WebFileSystemStorageConnection;
class WebFrame;
class WebLoaderStrategy;
class WebNotificationManager;
class WebPage;
class WebPageGroupProxy;
class WebProcessSupplement;
class WebTransportSession;

struct AccessibilityPreferences;
struct AdditionalFonts;
struct ContentWorldIdentifierType;
struct RemoteAudioSessionConfiguration;
struct RemoteWorkerInitializationData;
struct UserMessage;
struct WebProcessCreationParameters;
struct WebProcessDataStoreParameters;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebTransportSessionIdentifierType;
struct WebsiteData;
struct WebsiteDataStoreParameters;

#if USE(LIBRICE)
class RiceBackendProxy;
struct RiceBackendIdentifierType;
using RiceBackendIdentifier = ObjectIdentifier<RiceBackendIdentifierType>;
#endif

enum class RemoteWorkerType : uint8_t;
enum class WebsiteDataType : uint32_t;

using ContentWorldIdentifier = WebCore::ProcessQualified<ObjectIdentifier<ContentWorldIdentifierType>>;
using WebTransportSessionIdentifier = AtomicObjectIdentifier<WebTransportSessionIdentifierType>;

#if PLATFORM(IOS_FAMILY)
class LayerHostingContext;
#endif

#if ENABLE(MEDIA_STREAM)
class SpeechRecognitionRealtimeMediaSourceManager;
#endif

class WebProcess final : public AuxiliaryProcess {
    WTF_MAKE_TZONE_ALLOCATED(WebProcess);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebProcess);
public:
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    static WebProcess& singleton();
    static constexpr WTF::AuxiliaryProcessType processType = WTF::AuxiliaryProcessType::WebContent;

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
        m_supplements.add(T::supplementName(), makeUnique<T>(*this));
    }

    template <typename T>
    void addSupplementWithoutRefCountedCheck()
    {
        // WebProcessSupplement objects forward their ref-counting to the WebProcess. The WebProcess
        // stores this in a HashMap. It is currently safe because we only ever add to the HashMap, never remove.
        // However, the current design is fragile and the need to const_cast here is annoying so it would be good
        // to find a better pattern.
        m_supplements.add(T::supplementName(), const_cast<std::unique_ptr<WebProcessSupplement>&&>(makeUniqueWithoutRefCountedCheck<T, WebProcessSupplement>(*this)));
    }

    // ref() & deref() do nothing since WebProcess is a singleton object.
    // This is for objects owned by the WebProcess to forward their refcounting to their owner.
    void ref() const final { }
    void deref() const final { }

    WebPage* webPage(WebCore::PageIdentifier) const;
    void createWebPage(WebCore::PageIdentifier, WebPageCreationParameters&&);
    Awaitable<unsigned> countWebPagesForTesting();
    void removeWebPage(WebCore::PageIdentifier);
    WebPage* focusedWebPage() const;
    bool hasEverHadAnyWebPages() const { return m_hasEverHadAnyWebPages; }
    bool isWebTransportEnabled() const { return m_isWebTransportEnabled; }
    bool isBroadcastChannelEnabled() const { return m_isBroadcastChannelEnabled; }

    InjectedBundle* injectedBundle() const { return m_injectedBundle.get(); }
    
    PAL::SessionID sessionID() const { ASSERT(m_sessionID); return *m_sessionID; }

    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const { return m_thirdPartyCookieBlockingMode; }

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

    void contentWorldDestroyed(ContentWorldIdentifier);

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool hasMouseDevice() const { return m_hasMouseDevice; }
    void setHasMouseDevice(bool);
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool hasStylusDevice() const { return m_hasStylusDevice; }
    void setHasStylusDevice(bool);
#endif

    void updateStorageAccessUserAgentStringQuirks(HashMap<WebCore::RegistrableDomain, String>&&);

    WebFrame* webFrame(std::optional<WebCore::FrameIdentifier>) const;
    void addWebFrame(WebCore::FrameIdentifier, WebFrame*);
    void removeWebFrame(WebCore::FrameIdentifier, WebPage*);

    WebPageGroupProxy* webPageGroup(WebPageGroupData&&);

    std::optional<WebCore::UserGestureTokenIdentifier> userGestureTokenIdentifier(std::optional<WebCore::PageIdentifier>, RefPtr<WebCore::UserGestureToken>);
    void userGestureTokenDestroyed(WebCore::PageIdentifier, WebCore::UserGestureToken&);
    
    OptionSet<TextCheckerState> textCheckerState() const { return m_textCheckerState; }
    void setTextCheckerState(OptionSet<TextCheckerState>);

    EventDispatcher& eventDispatcher() { return m_eventDispatcher; }
    Ref<EventDispatcher> protectedEventDispatcher() { return m_eventDispatcher; }
    Ref<WebInspectorInterruptDispatcher> protectedWebInspectorInterruptDispatcher() { return m_webInspectorInterruptDispatcher; }
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    Ref<WasmDebuggerDispatcher> protectedWasmDebuggerDispatcher() { return m_wasmDebuggerDispatcher; }
#endif

    NetworkProcessConnection& ensureNetworkProcessConnection();
    Ref<NetworkProcessConnection> ensureProtectedNetworkProcessConnection();

    void networkProcessConnectionClosed(NetworkProcessConnection*);
    NetworkProcessConnection* existingNetworkProcessConnection() { return m_networkProcessConnection.get(); }
    RefPtr<NetworkProcessConnection> protectedNetworkProcessConnection();
    WebLoaderStrategy& webLoaderStrategy();
    Ref<WebLoaderStrategy> protectedWebLoaderStrategy();
    WebFileSystemStorageConnection& fileSystemStorageConnection();
    Ref<WebFileSystemStorageConnection> protectedFileSystemStorageConnection();

    RefPtr<WebTransportSession> webTransportSession(WebTransportSessionIdentifier);
    void addWebTransportSession(WebTransportSessionIdentifier, WebTransportSession&);
    void removeWebTransportSession(WebTransportSessionIdentifier);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_sharedPreferencesForWebProcess; }
    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcessValue() const { return m_sharedPreferencesForWebProcess; }
    void updateSharedPreferencesForWebProcess(SharedPreferencesForWebProcess sharedPreferencesForWebProcess) { m_sharedPreferencesForWebProcess = WTFMove(sharedPreferencesForWebProcess); }

#if USE(LIBRICE)
    RefPtr<RiceBackendProxy> gstreamerIceBackend(RiceBackendIdentifier);
    void addRiceBackend(RiceBackendIdentifier, RiceBackendProxy&);
    void removeRiceBackend(RiceBackendIdentifier);
#endif

#if ENABLE(GPU_PROCESS)
    GPUProcessConnection& ensureGPUProcessConnection();
    Ref<GPUProcessConnection> ensureProtectedGPUProcessConnection();
    GPUProcessConnection* existingGPUProcessConnection() { return m_gpuProcessConnection.get(); }
    // Returns timeout duration for GPU process connections. Thread-safe.
    Seconds gpuProcessTimeoutDuration() const;
    void gpuProcessConnectionClosed();
    void gpuProcessConnectionDidBecomeUnresponsive();

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    LibWebRTCCodecs& libWebRTCCodecs();
    Ref<LibWebRTCCodecs> protectedLibWebRTCCodecs();
#endif
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    AudioMediaStreamTrackRendererInternalUnitManager& audioMediaStreamTrackRendererInternalUnitManager();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RemoteLegacyCDMFactory& legacyCDMFactory();
    Ref<RemoteLegacyCDMFactory> protectedLegacyCDMFactory();
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    RemoteCDMFactory& cdmFactory();
    Ref<RemoteCDMFactory> protectedCDMFactory();
#endif
    RemoteMediaEngineConfigurationFactory& mediaEngineConfigurationFactory();
    RemoteSharedResourceCacheProxy& gpuProcessSharedResourceCache();
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
    ModelProcessConnection& ensureModelProcessConnection();
    void modelProcessConnectionClosed(ModelProcessConnection&);
    ModelProcessConnection* existingModelProcessConnection() { return m_modelProcessConnection.get(); }
#endif // ENABLE(MODEL_PROCESS)

#if USE(LIBWEBRTC)
    LibWebRTCNetwork& libWebRTCNetwork();
    Ref<LibWebRTCNetwork> protectedLibWebRTCNetwork();
#endif

    void setCacheModel(CacheModel);

    void pageDidEnterWindow(WebCore::PageIdentifier);
    void pageWillLeaveWindow(WebCore::PageIdentifier);

    void nonVisibleProcessEarlyMemoryCleanupTimerFired();

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

#if PLATFORM(COCOA) || PLATFORM(WPE) || PLATFORM(GTK)
    void releaseSystemMallocMemory();
#endif

    const String& uiProcessBundleIdentifier() const { return m_uiProcessBundleIdentifier; }

    void updateActivePages(const String& overrideDisplayName);
    void getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&&);
    void pageActivityStateDidChange(WebCore::PageIdentifier, OptionSet<WebCore::ActivityState> changed);

    void setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds);

    void releaseMemory(CompletionHandler<void()>&&);
    void prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&&);
    void processDidResume();

    void jSHandleDestroyed(WebCore::JSHandleIdentifier);

    void sendPrewarmInformation(const URL&);

    void isJITEnabled(CompletionHandler<void(bool)>&&);
    void isEnhancedSecurityEnabled(CompletionHandler<void(bool)>&&);
    bool enhancedSecurityEnabled() const { return m_isEnhancedSecurityEnabled.value_or(false); }

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices() const { return m_hasImageServices; }
    bool hasSelectionServices() const { return m_hasSelectionServices; }
    bool hasRichContentServices() const { return m_hasRichContentServices; }
#endif

    void prefetchDNS(const String&);

    WebAutomationSessionProxy* automationSessionProxy() { return m_automationSessionProxy.get(); }
#if ENABLE(MODEL_PROCESS)
    ModelProcessModelPlayerManager& modelProcessModelPlayerManager() { return m_modelProcessModelPlayerManager.get(); }
#endif
    WebCacheStorageProvider& cacheStorageProvider() { return m_cacheStorageProvider.get(); }
    WebBadgeClient& badgeClient() { return m_badgeClient.get(); }
#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
    RemoteMediaPlayerManager& remoteMediaPlayerManager() { return m_remoteMediaPlayerManager.get(); }
    Ref<RemoteMediaPlayerManager> protectedRemoteMediaPlayerManager();
#endif
#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
    RemoteImageDecoderAVFManager& remoteImageDecoderAVFManager() { return m_remoteImageDecoderAVFManager.get(); }
    Ref<RemoteImageDecoderAVFManager> protectedRemoteImageDecoderAVFManager();
#endif
    WebBroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry.get(); }
    WebCookieJar& cookieJar() { return m_cookieJar.get(); }
    Ref<WebCookieJar> protectedCookieJar();
    WebSocketChannelManager& webSocketChannelManager() { return m_webSocketChannelManager; }

    Ref<WebNotificationManager> protectedNotificationManager();

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float backlightLevel() const { return m_backlightLevel; }
#endif

#if PLATFORM(COCOA)
    void setMediaMIMETypes(const Vector<String>);
#if ENABLE(REMOTE_INSPECTOR)
    void enableRemoteWebInspector();
#endif
    void unblockServicesRequiredByAccessibility(Vector<SandboxExtension::Handle>&&);
    static id accessibilityFocusedUIElement();
    void powerSourceDidChange(bool);
#endif

#if PLATFORM(MAC)
    void openDirectoryCacheInvalidated(SandboxExtension::Handle&&, SandboxExtension::Handle&&);
#endif

#if ENABLE(NOTIFY_BLOCKING)
    void postNotification(const String& message, std::optional<uint64_t> state);
    void postObserverNotification(const String& message);
    void getNotifyStateForTesting(const String&, CompletionHandler<void(std::optional<uint64_t>)>&&);
    void setNotifyState(const String&, uint64_t state);
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    void setResourceMonitorContentRuleList(WebCompiledContentRuleListData&&);
    void setResourceMonitorContentRuleListAsync(WebCompiledContentRuleListData&&, CompletionHandler<void()>&&);
#endif

    bool areAllPagesThrottleable() const;

    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

    void addServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);
    bool removeServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier);

    bool registerServiceWorker(WebCore::ServiceWorkerIdentifier);
    bool unregisterServiceWorker(WebCore::ServiceWorkerIdentifier);

    void grantAccessToAssetServices(Vector<WebKit::SandboxExtensionHandle>&& assetServicesHandles);
    void revokeAccessToAssetServices();
    void switchFromStaticFontRegistryToUserFontRegistry(Vector<SandboxExtension::Handle>&& fontMachExtensionHandles);

    void disableURLSchemeCheckInDataDetectors() const;

#if PLATFORM(MAC)
    void updatePageScreenProperties();
#endif

    void setChildProcessDebuggabilityEnabled(bool);

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

    bool requiresScriptTrackingPrivacyProtections(const URL&, const WebCore::SecurityOrigin& topOrigin) const;
    bool shouldAllowScriptAccess(const URL&, const WebCore::SecurityOrigin& topOrigin, WebCore::ScriptTrackingPrivacyCategory) const;

    bool isLockdownModeEnabled() const { return m_isLockdownModeEnabled.value(); }
    bool imageAnimationEnabled() const { return m_imageAnimationEnabled; }
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    bool prefersNonBlinkingCursor() const { return m_prefersNonBlinkingCursor; }
#endif

    void setHadMainFrameMainResourcePrivateRelayed() { m_hadMainFrameMainResourcePrivateRelayed = true; }
    bool hadMainFrameMainResourcePrivateRelayed() const { return m_hadMainFrameMainResourcePrivateRelayed; }

    void deleteWebsiteDataForOrigin(OptionSet<WebsiteDataType>, const WebCore::ClientOrigin&, CompletionHandler<void()>&&);
    void reloadExecutionContextsForOrigin(const WebCore::ClientOrigin&, std::optional<WebCore::FrameIdentifier> triggeringFrame, CompletionHandler<void()>&&);

    void setAppBadge(WebCore::Frame*, const WebCore::SecurityOriginData&, std::optional<uint64_t> badge);

    void deferNonVisibleProcessEarlyMemoryCleanupTimer();

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void revokeLaunchServicesSandboxExtension();
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    const OptionSet<RendererBufferTransportMode>& rendererBufferTransportMode() const { return m_rendererBufferTransportMode; }
    void initializePlatformDisplayIfNeeded() const;
    const OptionSet<AvailableInputDevices>& availableInputDevices() const { return m_availableInputDevices; }
    std::optional<AvailableInputDevices> primaryPointingDevice() const;
    void setAvailableInputDevices(OptionSet<AvailableInputDevices>);
#endif // PLATFORM(WPE)

    String mediaKeysStorageDirectory() const { return m_mediaKeysStorageDirectory; }
    FileSystem::Salt mediaKeysStorageSalt() const { return m_mediaKeysStorageSalt; }

    bool haveStorageAccessQuirksForDomain(const WebCore::RegistrableDomain&);
    void updateCachedCookiesEnabled();
    void enableMediaPlayback();
#if ENABLE(ROUTING_ARBITRATION)
    AudioSessionRoutingArbitrator* audioSessionRoutingArbitrator() const { return m_routingArbitrator.get(); }
#endif

    bool mediaPlaybackEnabled() const { return m_mediaPlaybackEnabled; }

#if PLATFORM(COCOA)
    void registerAdditionalFonts(AdditionalFonts&&);
    void registerFontMap(HashMap<String, URL>&&, HashMap<String, Vector<String>>&&, Vector<SandboxExtension::Handle>&& sandboxExtensions);
#endif

    void didReceiveRemoteCommand(WebCore::PlatformMediaSessionRemoteControlCommandType, const WebCore::PlatformMediaSessionRemoteCommandArgument&);

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    void initializeAccessibility(Vector<SandboxExtension::Handle>&&);
    bool shouldInitializeAccessibility() const { return m_shouldInitializeAccessibility; }
#endif

#if USE(AUDIO_SESSION)
    void remoteAudioSessionConfigurationChanged(const RemoteAudioSessionConfiguration&);
#endif

private:
    WebProcess();
    ~WebProcess();

    void initializeWebProcess(WebProcessCreationParameters&&, CompletionHandler<void(WebCore::ProcessIdentity)>&&);
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

    void destroyDecodedDataForAllImages();

    void platformTerminate();

    void setHasSuspendedPageProxy(bool);
    void setIsInProcessCache(bool, CompletionHandler<void()>&&);
    void markIsNoLongerPrewarmed();

    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&) const;
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&) const;
    void setDomainRelaxationForbiddenForURLScheme(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
    void registerURLSchemeAsNoAccess(const String&) const;
#endif
    void registerURLSchemeAsDisplayIsolated(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&);
    void registerURLSchemeAsAlwaysRevalidated(const String&) const;
    void registerURLSchemeAsCachePartitioned(const String&) const;
    void registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String&) const;

#if ENABLE(WK_WEB_EXTENSIONS)
    void registerURLSchemeAsWebExtension(const String&) const;
#endif

    void setDefaultRequestTimeoutInterval(double);
    void setAlwaysUsesComplexTextCodePath(bool);
    void setDisableFontSubpixelAntialiasingForTesting(bool);
    void setTrackingPreventionEnabled(bool);
    void clearResourceLoadStatistics();
    void flushResourceLoadStatistics();
    void seedResourceLoadStatisticsForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&&);
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    void setOptInCookiePartitioningEnabled(bool);
#endif

    void platformSetCacheModel(CacheModel);

    void setEnhancedAccessibility(bool);
    void bindAccessibilityFrameWithData(WebCore::FrameIdentifier, std::span<const uint8_t>);

    void startMemorySampler(SandboxExtension::Handle&&, const String&, const double);
    void stopMemorySampler();
    
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void backgroundResponsivenessPing();

#if ENABLE(GAMEPAD)
    void setInitialGamepads(const Vector<std::optional<GamepadData>>&);
    void gamepadConnected(const GamepadData&, WebCore::EventMakesGamepadsVisible);
    void gamepadDisconnected(unsigned index);
#endif

    void establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType, PageGroupIdentifier, WebPageProxyIdentifier, WebCore::PageIdentifier, const WebPreferencesStore&, WebCore::Site&&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, RemoteWorkerInitializationData&&, CompletionHandler<void()>&&);
    void registerServiceWorkerClients(CompletionHandler<void(bool)>&&);

    void fetchWebsiteData(OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData&&)>&&);
    void deleteWebsiteData(OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&&);
    void deleteAllCookies(CompletionHandler<void()>&&);

    void setMemoryCacheDisabled(bool);

    void setBackForwardCacheCapacity(unsigned);
    void clearCachedPage(WebCore::BackForwardItemIdentifier, CompletionHandler<void()>&&);

#if ENABLE(SERVICE_CONTROLS)
    void setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices);
#endif

    void handleInjectedBundleMessage(const String& messageName, const UserData& messageBody);
    void setInjectedBundleParameter(const String& key, std::span<const uint8_t>);
    void setInjectedBundleParameters(std::span<const uint8_t>);

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

    bool filterUnhandledMessage(IPC::Connection&, IPC::Decoder&) override;

#if ENABLE(MEDIA_STREAM)
    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void setMockMediaDeviceIsEphemeral(const String&, bool);
    void resetMockMediaDevices();
#if ENABLE(SANDBOX_EXTENSIONS)
    void grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&&);
    void revokeUserMediaDeviceSandboxExtensions(const Vector<String>&);
#endif

#endif

    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&&);
    void setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, Vector<SubResourceDomain>>&&, CompletionHandler<void()>&&);
    void sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&&);

    void updateDomainsWithStorageAccessQuirks(HashSet<WebCore::RegistrableDomain>&&);

    void updateScriptTrackingPrivacyFilter(ScriptTrackingPrivacyRules&&);

#if HAVE(DISPLAY_LINK)
    void displayDidRefresh(uint32_t displayID, const WebCore::DisplayUpdate&);
#endif

#if PLATFORM(MAC)
    void systemWillPowerOn();
    void systemWillSleep();
    void systemDidWake();
#endif

    void platformInitializeProcess(const AuxiliaryProcessInitializationParameters&);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) final;
    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);

#if PLATFORM(MAC)
    void scrollerStylePreferenceChanged(bool useOverlayScrollbars);
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    void setScreenProperties(const WebCore::ScreenProperties&);
#endif

#if PLATFORM(COCOA)
    enum class IsInProcessInitialization : bool { No, Yes };
    void updateProcessName(IsInProcessInitialization);
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    void backlightLevelDidChange(float backlightLevel);
#endif

    void accessibilityPreferencesDidChange(const AccessibilityPreferences&);
    void updatePageAccessibilitySettings();
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    void setMediaAccessibilityPreferences(WebCore::CaptionUserPreferences::CaptionDisplayMode, const Vector<String>&);
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void colorPreferencesDidChange();
#endif

#if PLATFORM(IOS_FAMILY)
    void userInterfaceIdiomDidChange(PAL::UserInterfaceIdiom);

    bool shouldFreezeOnSuspension() const;
    void updateFreezerStatus();
#endif

#if ENABLE(VIDEO)
    void suspendAllMediaBuffering();
    void resumeAllMediaBuffering();
#endif

    void clearCurrentModifierStateForTesting();

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebProcessExtension(UserMessage&&);
#endif

#if PLATFORM(GTK) && !USE(GTK4) && USE(CAIRO)
    void setUseSystemAppearanceForScrollbars(bool);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void handlePreferenceChange(const String& domain, const String& key, id value) final;
    void dispatchSimulatedNotificationsForPreferenceChange(const String& key) final;

    void accessibilitySettingsDidChange() final;
#endif

    void accessibilityRelayProcessSuspended(bool);

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    void initializeLogForwarding(const WebProcessCreationParameters&);
#endif

    bool isProcessBeingCachedForPerformance();

    HashMap<WebCore::PageIdentifier, Ref<WebPage>> m_pageMap;
    HashMap<PageGroupIdentifier, RefPtr<WebPageGroupProxy>> m_pageGroupMap;
    const RefPtr<InjectedBundle> m_injectedBundle;

    EventDispatcher m_eventDispatcher;
#if PLATFORM(IOS_FAMILY)
    ViewUpdateDispatcher m_viewUpdateDispatcher;
#endif
    WebInspectorInterruptDispatcher m_webInspectorInterruptDispatcher;
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    WasmDebuggerDispatcher m_wasmDebuggerDispatcher;
#endif

    bool m_hasSetCacheModel { false };
    CacheModel m_cacheModel { CacheModel::DocumentViewer };

    bool m_fullKeyboardAccessEnabled { false };

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool m_hasMouseDevice { false };
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool m_hasStylusDevice { false };
#endif

    HashMap<WebCore::FrameIdentifier, WeakPtr<WebFrame>> m_frameMap;

    using WebProcessSupplementMap = HashMap<ASCIILiteral, std::unique_ptr<WebProcessSupplement>>;
    WebProcessSupplementMap m_supplements;

    OptionSet<TextCheckerState> m_textCheckerState;

    String m_uiProcessBundleIdentifier;
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    const UniqueRef<WebLoaderStrategy> m_webLoaderStrategy;
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
    RefPtr<RemoteSharedResourceCacheProxy> m_sharedResourceCache;
#endif

#if ENABLE(MODEL_PROCESS)
    const Ref<ModelProcessModelPlayerManager> m_modelProcessModelPlayerManager;
    RefPtr<ModelProcessConnection> m_modelProcessConnection;
#endif

    const Ref<WebCacheStorageProvider> m_cacheStorageProvider;
    const Ref<WebBadgeClient> m_badgeClient;
#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
    const Ref<RemoteMediaPlayerManager> m_remoteMediaPlayerManager;
#endif
#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
    const Ref<RemoteImageDecoderAVFManager> m_remoteImageDecoderAVFManager;
#endif
    const Ref<WebBroadcastChannelRegistry> m_broadcastChannelRegistry;
    const Ref<WebCookieJar> m_cookieJar;
    WebSocketChannelManager m_webSocketChannelManager;

#if USE(LIBWEBRTC)
    const std::unique_ptr<LibWebRTCNetwork> m_libWebRTCNetwork;
#endif

    HashSet<String> m_dnsPrefetchedHosts;
    PAL::HysteresisActivity m_dnsPrefetchHystereris;

    RefPtr<WebAutomationSessionProxy> m_automationSessionProxy;

#if ENABLE(SERVICE_CONTROLS)
    bool m_hasImageServices { false };
    bool m_hasSelectionServices { false };
    bool m_hasRichContentServices { false };
#endif

    bool m_processIsSuspended { false };

    HashSet<WebCore::PageIdentifier> m_pagesInWindows;
    std::optional<WebCore::DeferrableOneShotTimer> m_nonVisibleProcessEarlyMemoryCleanupTimer;

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    WebCore::Timer m_nonVisibleProcessMemoryCleanupTimer;
#endif

    bool m_suppressMemoryPressureHandler { false };
    bool m_loggedProcessLimitWarningMemoryStatistics { false };
    bool m_loggedProcessLimitCriticalMemoryStatistics { false };
    bool m_wasVisibleSinceLastProcessSuspensionEvent { false };
#if PLATFORM(MAC)
    std::unique_ptr<WebCore::CPUMonitor> m_cpuMonitor;
    std::optional<double> m_cpuLimit;

    String m_uiProcessName;
    WebCore::RegistrableDomain m_registrableDomain;
#endif
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    RefPtr<SandboxExtension> m_launchServicesExtension;
#endif

#if PLATFORM(COCOA)
    enum class ProcessType { Inspector, ServiceWorker, PrewarmedWebContent, CachedWebContent, WebContent };
    ProcessType m_processType { ProcessType::WebContent };
#endif

    WeakHashMap<WebCore::UserGestureToken, WebCore::UserGestureTokenIdentifier> m_userGestureTokens;

#if PLATFORM(GTK) || PLATFORM(WPE)
    OptionSet<RendererBufferTransportMode> m_rendererBufferTransportMode;
    OptionSet<AvailableInputDevices> m_availableInputDevices;
#endif

    bool m_hasSuspendedPageProxy { false };
    bool m_allowExitOnMemoryPressure { true };
    std::optional<bool> m_isLockdownModeEnabled;
    std::optional<bool> m_isEnhancedSecurityEnabled;

#if ENABLE(MEDIA_STREAM) && ENABLE(SANDBOX_EXTENSIONS)
    HashMap<String, RefPtr<SandboxExtension>> m_mediaCaptureSandboxExtensions;
    RefPtr<SandboxExtension> m_machBootstrapExtension;
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    float m_backlightLevel { 0 };
#endif

    HashCountedSet<WebCore::ServiceWorkerRegistrationIdentifier> m_swRegistrationCounts;
    HashCountedSet<WebCore::ServiceWorkerIdentifier> m_swServiceWorkerCounts;

    HashMap<StorageAreaMapIdentifier, WeakPtr<StorageAreaMap>> m_storageAreaMaps;

    void updateIsWebTransportEnabled();
    void updateIsBroadcastChannelEnabled();
    
    // Prewarmed WebProcesses do not have an associated sessionID yet, which is why this is an optional.
    // By the time the WebProcess gets a WebPage, it is guaranteed to have a sessionID.
    std::optional<PAL::SessionID> m_sessionID;

    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };

    Vector<RefPtr<SandboxExtension>> m_assetServicesExtensions;

#if PLATFORM(COCOA)
    HashCountedSet<String> m_pendingPasteboardWriteCounts;
    std::optional<audit_token_t> m_auditTokenForSelf;
    RetainPtr<NSMutableDictionary> m_accessibilityRemoteFrameTokenCache;
#endif

    bool m_childProcessDebuggabilityEnabled { false };

#if ENABLE(GPU_PROCESS)
    bool m_useGPUProcessForCanvasRendering { false };
    bool m_useGPUProcessForDOMRendering { false };
    bool m_useGPUProcessForMedia { false };
#if ENABLE(WEBGL)
    bool m_useGPUProcessForWebGL { false };
#endif
#endif

#if ENABLE(MEDIA_STREAM)
    const std::unique_ptr<SpeechRecognitionRealtimeMediaSourceManager> m_speechRecognitionRealtimeMediaSourceManager;
#endif
#if ENABLE(ROUTING_ARBITRATION)
    const std::unique_ptr<AudioSessionRoutingArbitrator> m_routingArbitrator;
#endif
    bool m_hadMainFrameMainResourcePrivateRelayed { false };
    bool m_imageAnimationEnabled { true };
    bool m_hasEverHadAnyWebPages { false };
    bool m_hasPendingAccessibilityUnsuspension { false };
    bool m_isWebTransportEnabled { false };
    bool m_isBroadcastChannelEnabled { false };

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    bool m_prefersNonBlinkingCursor { false };
#endif

    String m_mediaKeysStorageDirectory;
    FileSystem::Salt m_mediaKeysStorageSalt;

    Lock m_webTransportSessionsLock;
    HashMap<WebTransportSessionIdentifier, ThreadSafeWeakPtr<WebTransportSession>> m_webTransportSessions WTF_GUARDED_BY_LOCK(m_webTransportSessionsLock);

#if USE(LIBRICE)
    HashMap<RiceBackendIdentifier, ThreadSafeWeakPtr<RiceBackendProxy>> m_gstreamerIceBackends;
#endif

    HashSet<WebCore::RegistrableDomain> m_domainsWithStorageAccessQuirks;
    std::unique_ptr<ScriptTrackingPrivacyFilter> m_scriptTrackingPrivacyFilter;
    bool m_mediaPlaybackEnabled { false };

    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;

#if ENABLE(NOTIFY_BLOCKING)
    HashMap<String, int> m_notifyTokens;
#endif
#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
    String m_pendingDisplayName;
#endif
#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    bool m_shouldInitializeAccessibility { false };
#endif
};

} // namespace WebKit
