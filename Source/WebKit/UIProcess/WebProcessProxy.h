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

#include "APIUserInitiatedAction.h"
#include "AuxiliaryProcessProxy.h"
#include "BackgroundProcessResponsivenessTimer.h"
#include "GPUProcessConnectionIdentifier.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "RemoteWorkerInitializationData.h"
#include "ResponsivenessTimer.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedPreferencesForWebProcess.h"
#include "SpeechRecognitionServer.h"
#include "UserContentControllerIdentifier.h"
#include "VisibleWebPageCounter.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/CrossOriginMode.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/SharedStringHash.h>
#include <WebCore/Site.h>
#include <WebCore/UserGestureTokenIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainReleaseSwift.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include <WebCore/CaptionUserPreferences.h>
#endif

#if HAVE(DISPLAY_LINK)
#include "DisplayLinkObserverID.h"
#include "DisplayLinkProcessProxyClient.h"
#endif

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
#include "LogStream.h"
#include "LogStreamIdentifier.h"
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
#include "ServiceWorkerDebuggableProxy.h"
#endif

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
#include "WasmDebuggerDebuggable.h"
#endif

namespace API {
class Navigation;
class PageConfiguration;
}

namespace WebCore {
class DeferrableOneShotTimer;
class ResourceRequest;
struct CryptoKeyData;
struct NotificationData;
struct PluginInfo;
struct PrewarmInformation;
struct WebProcessCreationParameters;
class SecurityOriginData;
struct WrappedCryptoKey;

enum class PermissionName : uint8_t;
enum class ThirdPartyCookieBlockingMode : uint8_t;
using FramesPerSecond = unsigned;
using PlatformDisplayID = uint32_t;
}

namespace WTF {
class TextStream;
}

namespace WebKit {

class AudioSessionRoutingArbitratorProxy;
class FrameState;
class JavaScriptEvaluationResult;
class ModelProcessProxy;
class PageClient;
class ProvisionalPageProxy;
class RemotePageProxy;
class SuspendedPageProxy;
class UserMediaCaptureManagerProxy;
class VisitedLinkStore;
class WebBackForwardListItem;
class WebCompiledContentRuleList;
class WebCompiledContentRuleListData;
class WebFrameProxy;
class WebLockRegistryProxy;
class WebPageGroup;
class WebPageProxy;
class WebPermissionControllerProxy;
class WebPreferences;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;

struct CoreIPCAuditToken;
struct GPUProcessConnectionParameters;
struct ModelProcessConnectionParameters;
struct ScriptMessageHandlerIdentifierType;
struct UserContentControllerIdentifierType;
struct UserMessage;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebPreferencesStore;
struct WebsiteData;

enum class ProcessTerminationReason : uint8_t;
enum class ProcessThrottleState : uint8_t;
enum class RemoteWorkerType : uint8_t;
enum class WebsiteDataType : uint32_t;

#if ENABLE(MEDIA_STREAM)
class SpeechRecognitionRemoteRealtimeMediaSourceManager;
#endif

using ScriptMessageHandlerIdentifier = ObjectIdentifier<ScriptMessageHandlerIdentifierType>;
using UserContentControllerIdentifier = ObjectIdentifier<UserContentControllerIdentifierType>;
enum ForegroundWebProcessCounterType { };
typedef RefCounter<ForegroundWebProcessCounterType> ForegroundWebProcessCounter;
typedef ForegroundWebProcessCounter::Token ForegroundWebProcessToken;
enum BackgroundWebProcessCounterType { };
typedef RefCounter<BackgroundWebProcessCounterType> BackgroundWebProcessCounter;
typedef BackgroundWebProcessCounter::Token BackgroundWebProcessToken;
enum WebProcessWithAudibleMediaCounterType { };
using WebProcessWithAudibleMediaCounter = RefCounter<WebProcessWithAudibleMediaCounterType>;
using WebProcessWithAudibleMediaToken = WebProcessWithAudibleMediaCounter::Token;
enum WebProcessWithMediaStreamingCounterType { };
using WebProcessWithMediaStreamingCounter = RefCounter<WebProcessWithMediaStreamingCounterType>;
using WebProcessWithMediaStreamingToken = WebProcessWithMediaStreamingCounter::Token;
enum class CheckBackForwardList : bool { No, Yes };

class WebProcessProxy final : public AuxiliaryProcessProxy {
    WTF_MAKE_TZONE_ALLOCATED(WebProcessProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebProcessProxy);
public:
    using WebPageProxyMap = HashMap<WebPageProxyIdentifier, WeakRef<WebPageProxy>>;
    using UserInitiatedActionByAuthorizationTokenMap = HashMap<WTF::UUID, RefPtr<API::UserInitiatedAction>>;
    typedef HashMap<WebCore::UserGestureTokenIdentifier, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    enum class IsPrewarmed : bool { No, Yes };

    enum class ShouldLaunchProcess : bool { No, Yes };
    enum class LockdownMode : bool { Disabled, Enabled };
    enum class EnhancedSecurity : bool { Disabled, Enabled };

    static Ref<WebProcessProxy> create(WebProcessPool&, WebsiteDataStore*, LockdownMode, EnhancedSecurity, IsPrewarmed, WebCore::CrossOriginMode = WebCore::CrossOriginMode::Shared, ShouldLaunchProcess = ShouldLaunchProcess::Yes);
    static Ref<WebProcessProxy> createForRemoteWorkers(RemoteWorkerType, WebProcessPool&, WebCore::Site&&, WebsiteDataStore&, LockdownMode, EnhancedSecurity);

    ~WebProcessProxy();

    static void forWebPagesWithOrigin(PAL::SessionID, const WebCore::SecurityOriginData&, NOESCAPE const Function<void(WebPageProxy&)>&);
    static Vector<std::pair<WebCore::ProcessIdentifier, WebCore::RegistrableDomain>> allowedFirstPartiesForCookies();

    void initializeWebProcess(WebProcessCreationParameters&&);

    unsigned suspendedPageCount() const;
    void addSuspendedPageProxy(SuspendedPageProxy&);
    void removeSuspendedPageProxy(SuspendedPageProxy&);

    WebProcessPool* processPoolIfExists() const;
    inline WebProcessPool& processPool() const; // This function is implemented in WebProcessPool.h.
    inline Ref<WebProcessPool> protectedProcessPool() const; // This function is implemented in WebProcessPool.h.

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_sharedPreferencesForWebProcess; }
    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcessValue() const { return m_sharedPreferencesForWebProcess; }
    std::optional<SharedPreferencesForWebProcess> updateSharedPreferences(const WebPreferencesStore&);
    void didSyncSharedPreferencesForWebProcessWithNetworkProcess(uint64_t syncedPreferencesVersion);
#if ENABLE(GPU_PROCESS)
    void didSyncSharedPreferencesForWebProcessWithGPUProcess(uint64_t syncedPreferencesVersion);
#endif
#if ENABLE(MODEL_PROCESS)
    void didSyncSharedPreferencesForWebProcessWithModelProcess(uint64_t syncedPreferencesVersion);
#endif
    void waitForSharedPreferencesForWebProcessToSync(uint64_t sharedPreferencesVersion, CompletionHandler<void(bool success)>&&);

    enum class SiteState : uint8_t { NotYetSpecified, MultipleSites, SharedProcess };
    const Expected<WebCore::Site, SiteState>& site() const { return m_site; }

    bool isSharedProcess() const { return !m_site && m_site.error() == SiteState::SharedProcess; }
    const std::optional<WebCore::Site>& sharedProcessMainFrameSite() const { return m_sharedProcessMainFrameSite; }
    void addSharedProcessDomain(const WebCore::RegistrableDomain&);
    const HashSet<WebCore::RegistrableDomain>& sharedProcessDomains() const { return m_sharedProcessDomains; }

    enum class WillShutDown : bool { No, Yes };
    void setIsInProcessCache(bool, WillShutDown = WillShutDown::No);
    bool isInProcessCache() const { return m_isInProcessCache; }

    void enableRemoteWorkers(RemoteWorkerType, const WebUserContentControllerProxy&);
    void disableRemoteWorkers(OptionSet<RemoteWorkerType>);

    WebsiteDataStore* websiteDataStore() const { ASSERT(m_websiteDataStore); return m_websiteDataStore.get(); }
    RefPtr<WebsiteDataStore> protectedWebsiteDataStore() const;
    void setWebsiteDataStore(WebsiteDataStore&);
    
    PAL::SessionID sessionID() const;

    static bool hasReachedProcessCountLimit();
    static void setProcessCountLimit(unsigned);

    static RefPtr<WebProcessProxy> processForIdentifier(WebCore::ProcessIdentifier);
    static Ref<WebProcessProxy> fromConnection(const IPC::Connection&);
    static RefPtr<WebPageProxy> webPage(WebPageProxyIdentifier);
    static RefPtr<WebPageProxy> webPage(WebCore::PageIdentifier);
    static RefPtr<WebPageProxy> audioCapturingWebPage();
#if ENABLE(WEBXR)
    static RefPtr<WebPageProxy> webPageWithActiveXRSession();
#endif
    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    enum class BeginsUsingDataStore : bool { No, Yes };
    void addExistingWebPage(WebPageProxy&, BeginsUsingDataStore);

    enum class EndsUsingDataStore : bool { No, Yes };
    void removeWebPage(WebPageProxy&, EndsUsingDataStore);

    void addProvisionalPageProxy(ProvisionalPageProxy&);
    void removeProvisionalPageProxy(ProvisionalPageProxy&);
    void addRemotePageProxy(RemotePageProxy&);
    void removeRemotePageProxy(RemotePageProxy&);

    Vector<Ref<WebPageProxy>> pages() const;
    Vector<Ref<WebPageProxy>> mainPages() const;
    unsigned pageCount() const { return m_pageMap.size(); }
    unsigned provisionalPageCount() const;
    unsigned visiblePageCount() const { return m_visiblePageCounter.value(); }

    Vector<WeakPtr<RemotePageProxy>> remotePages() const;

    void activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&&); // This is what is reported to ActivityMonitor.

#if ENABLE(NOTIFY_BLOCKING)
    void getNotifyStateForTesting(const String&, CompletionHandler<void(std::optional<uint64_t>)>&&);
#endif

    bool isRunningServiceWorkers() const { return !!m_serviceWorkerInformation; }
    bool isStandaloneServiceWorkerProcess() const { return isRunningServiceWorkers() && !pageCount(); }
    bool isRunningSharedWorkers() const { return !!m_sharedWorkerInformation; }
    bool isStandaloneSharedWorkerProcess() const { return isRunningSharedWorkers() && !pageCount(); }
    bool isRunningWorkers() const { return m_sharedWorkerInformation || m_serviceWorkerInformation; }

    bool isDummyProcessProxy() const;

    void didCreateWebPageInProcess(WebCore::PageIdentifier);

    void addVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);
    void removeVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);

    void recordUserGestureAuthorizationToken(WebCore::FrameIdentifier, WebCore::PageIdentifier, WTF::UUID);
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(std::optional<WebCore::UserGestureTokenIdentifier>);
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(WebCore::PageIdentifier, std::optional<WTF::UUID>, std::optional<WebCore::UserGestureTokenIdentifier>);

    void consumeIfNotVerifiablyFromUIProcess(WebCore::PageIdentifier, API::UserInitiatedAction&, std::optional<WTF::UUID>);

    bool isResponsive() const;

    VisibleWebPageToken visiblePageToken() const;

    void addPreviouslyApprovedFileURL(const URL&);
    bool wasPreviouslyApprovedFileURL(const URL&) const;

    void updateTextCheckerState();

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(WebPageProxy&, const String&, CompletionHandler<void()>&&, bool directoryOnly = false);
    void assumeReadAccessToBaseURLs(WebPageProxy&, const Vector<String>&, CompletionHandler<void()>&&);
    bool hasAssumedReadAccessToURL(const URL&) const;

    bool checkURLReceivedFromWebProcess(const String&, CheckBackForwardList = CheckBackForwardList::Yes);
    bool checkURLReceivedFromWebProcess(const URL&, CheckBackForwardList = CheckBackForwardList::Yes);

    static bool fullKeyboardAccessEnabled();

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    static void notifyHasMouseDeviceChanged(bool hasMouseDevice);
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    static void notifyHasStylusDeviceChanged(bool hasStylusDevice);
#endif

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);

    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    void setOptInCookiePartitioningEnabled(bool);
#endif

    void didPostMessage(WebPageProxyIdentifier, UserContentControllerIdentifier, FrameInfoData&&, ScriptMessageHandlerIdentifier, JavaScriptEvaluationResult&&, CompletionHandler<void(Expected<WebKit::JavaScriptEvaluationResult, String>&&)>&&);
    void didPostLegacySynchronousMessage(WebPageProxyIdentifier, UserContentControllerIdentifier, FrameInfoData&&, ScriptMessageHandlerIdentifier, JavaScriptEvaluationResult&&, CompletionHandler<void(Expected<JavaScriptEvaluationResult, String>&&)>&&);

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination(ProcessTerminationReason);

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

    void windowServerConnectionStateChanged();

    void isResponsive(CompletionHandler<void(bool isWebProcessResponsive)>&&);
    void isResponsiveWithLazyStop();
    void didReceiveBackgroundResponsivenessPing();

    SystemMemoryPressureStatus memoryPressureStatus() const { return m_memoryPressureStatus; }
    void memoryPressureStatusChanged(SystemMemoryPressureStatus);

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    void updateWebProcessSuspensionDelay();
#endif

    void processTerminated();

    void didExceedCPULimit();
    void didExceedActiveMemoryLimit();
    void didExceedInactiveMemoryLimit();
    void didExceedMemoryFootprintThreshold(uint64_t);

    void didCommitProvisionalLoad() { m_hasCommittedAnyProvisionalLoads = true; }
    bool hasCommittedAnyProvisionalLoads() const { return m_hasCommittedAnyProvisionalLoads; }

    void didCommitMeaningfulProvisionalLoad() { m_hasCommittedAnyMeaningfulProvisionalLoads = true; }
    bool hasCommittedAnyMeaningfulProvisionalLoads() const { return m_hasCommittedAnyMeaningfulProvisionalLoads; }

#if PLATFORM(WATCHOS)
    void startBackgroundActivityForFullscreenInput();
    void endBackgroundActivityForFullscreenInput();
#endif

    bool isPrewarmed() const { return m_isPrewarmed; }
    void markIsNoLongerInPrewarmedPool();

    bool isForeground() const { return !!m_foregroundToken; }
    bool isBackground() const { return !!m_backgroundToken; }

#if PLATFORM(COCOA)
    Vector<String> mediaMIMETypes() const;
    void cacheMediaMIMETypes(const Vector<String>&);
#endif

#if HAVE(DISPLAY_LINK)
    DisplayLink::Client& displayLinkClient() { return m_displayLinkClient; }
    std::optional<unsigned> nominalFramesPerSecondForDisplay(WebCore::PlatformDisplayID);

    void startDisplayLink(DisplayLinkObserverID, WebCore::PlatformDisplayID, WebCore::FramesPerSecond);
    void stopDisplayLink(DisplayLinkObserverID, WebCore::PlatformDisplayID);
    void setDisplayLinkPreferredFramesPerSecond(DisplayLinkObserverID, WebCore::PlatformDisplayID, WebCore::FramesPerSecond);
    void setDisplayLinkForDisplayWantsFullSpeedUpdates(WebCore::PlatformDisplayID, bool wantsFullSpeedUpdates);
#endif

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void shutDown();

    enum ShutdownPreventingScopeType { };
    using ShutdownPreventingScopeCounter = RefCounter<ShutdownPreventingScopeType>;
    ShutdownPreventingScopeCounter::Token shutdownPreventingScope() { return m_shutdownPreventingScopeCounter.count(); }

    void didStartProvisionalLoadForMainFrame(const URL&);
    void didStartUsingProcessForSiteIsolation(const std::optional<WebCore::Site>&, const WebCore::Site& mainFrameSite);

    // ProcessThrottlerClient
    void sendPrepareToSuspend(IsSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&&) final;
    void sendProcessDidResume(ResumeReason) final;
    void didChangeThrottleState(ProcessThrottleState) final;
    void prepareToDropLastAssertion(CompletionHandler<void()>&&) final;
    void didDropLastAssertion() final;
    ASCIILiteral clientName() const final { return "WebProcess"_s; }
    String environmentIdentifier() const final;

#if PLATFORM(COCOA)
    enum SandboxExtensionType : uint32_t {
        None = 0,
        Video = 1 << 0,
        Audio = 1 << 1
    };

    typedef uint32_t MediaCaptureSandboxExtensions;

    bool hasVideoCaptureExtension() const { return m_mediaCaptureSandboxExtensions & Video; }
    void grantVideoCaptureExtension() { m_mediaCaptureSandboxExtensions |= Video; }
    void revokeVideoCaptureExtension() { m_mediaCaptureSandboxExtensions &= ~Video; }

    bool hasAudioCaptureExtension() const { return m_mediaCaptureSandboxExtensions & Audio; }
    void grantAudioCaptureExtension() { m_mediaCaptureSandboxExtensions |= Audio; }
    void revokeAudioCaptureExtension() { m_mediaCaptureSandboxExtensions &= ~Audio; }

    void sendAudioComponentRegistrations();
#endif

    bool hasSameGPUAndNetworkProcessPreferencesAs(const API::PageConfiguration&) const;

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    void enableRemoteInspectorIfNeeded();
#endif
    
#if PLATFORM(COCOA)
    void unblockAccessibilityServerIfNeeded();
#endif

    void updateAudibleMediaAssertions();
    void updateMediaStreamingActivity();

    void setRemoteWorkerUserAgent(const String&);
    void updateRemoteWorkerPreferencesStore(const WebPreferencesStore&);
    void establishRemoteWorkerContext(RemoteWorkerType, const WebPreferencesStore&, const WebCore::Site&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&&);
    void registerRemoteWorkerClientProcess(RemoteWorkerType, WebProcessProxy&);
    void unregisterRemoteWorkerClientProcess(RemoteWorkerType, WebProcessProxy&);
    void updateRemoteWorkerProcessAssertion(RemoteWorkerType);
    bool hasServiceWorkerPageProxy(WebPageProxyIdentifier pageProxyID) { return m_serviceWorkerInformation && m_serviceWorkerInformation->remoteWorkerPageProxyID == pageProxyID; }
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
    void startServiceWorkerBackgroundProcessing();
    void endServiceWorkerBackgroundProcessing();
    void setThrottleStateForTesting(ProcessThrottleState);

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidFinishLaunching();
    void gpuProcessExited(ProcessTerminationReason);
#endif

#if ENABLE(MODEL_PROCESS)
    void modelProcessDidFinishLaunching();
    void modelProcessExited(ProcessTerminationReason);
#if HAVE(TASK_IDENTITY_TOKEN)
    void createMemoryAttributionIDIfNeeded(CompletionHandler<void(const std::optional<String>&)>&&);
    void unregisterMemoryAttributionIDIfNeeded();
#endif
#endif

#if PLATFORM(COCOA)
    bool hasNetworkExtensionSandboxAccess() const { return m_hasNetworkExtensionSandboxAccess; }
    void markHasNetworkExtensionSandboxAccess() { m_hasNetworkExtensionSandboxAccess = true; }
#endif

#if ENABLE(ROUTING_ARBITRATION)
    AudioSessionRoutingArbitratorProxy* audioSessionRoutingArbitrator() { return m_routingArbitrator.get(); }
#endif

#if ENABLE(IPC_TESTING_API)
    bool ignoreInvalidMessageForTesting() const { return m_ignoreInvalidMessageForTesting; }
    void setIgnoreInvalidMessageForTesting();
#endif
    
    bool allowTestOnlyIPC() const { return m_allowTestOnlyIPC; }
    void setAllowTestOnlyIPC(bool allowTestOnlyIPC) { m_allowTestOnlyIPC = allowTestOnlyIPC; }

#if ENABLE(MEDIA_STREAM)
    static void muteCaptureInPagesExcept(WebCore::PageIdentifier);
    SpeechRecognitionRemoteRealtimeMediaSourceManager& ensureSpeechRecognitionRemoteRealtimeMediaSourceManager();
    RefPtr<SpeechRecognitionRemoteRealtimeMediaSourceManager> protectedSpeechRecognitionRemoteRealtimeMediaSourceManager();
#endif
    void pageMutedStateChanged(WebCore::PageIdentifier, WebCore::MediaProducerMutedStateFlags);
    void pageIsBecomingInvisible(WebCore::PageIdentifier);

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    static bool shouldEnableRemoteInspector();
#endif

    void markProcessAsRecentlyUsed();

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    void platformSuspendProcess();
    void platformResumeProcess();
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    void setCaptionDisplayMode(WebCore::CaptionUserPreferences::CaptionDisplayMode);
    void setCaptionLanguage(const String&);
#endif
    void getNotifications(const URL&, const String&, CompletionHandler<void(Vector<WebCore::NotificationData>&&)>&&);
    void serializeAndWrapCryptoKey(WebCore::CryptoKeyData&&, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);
    void unwrapCryptoKey(WebCore::WrappedCryptoKey&&, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);

    void setAppBadgeFromWorker(const WebCore::SecurityOriginData&, std::optional<uint64_t> badge);

    WebCore::CrossOriginMode crossOriginMode() const { return m_crossOriginMode; }
    LockdownMode lockdownMode() const { return m_lockdownMode; }
    EnhancedSecurity enhancedSecurity() const { return m_enhancedSecurity; }

#if PLATFORM(COCOA)
    std::optional<audit_token_t> auditToken() const;
    std::optional<Vector<SandboxExtension::Handle>> fontdMachExtensionHandles();
#endif

    bool isConnectedToHardwareConsole() const { return m_isConnectedToHardwareConsole; }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void hardwareConsoleStateChanged();
#endif

    const WeakHashSet<WebProcessProxy>* serviceWorkerClientProcesses() const;
    const WeakHashSet<WebProcessProxy>* sharedWorkerClientProcesses() const;

    static void permissionChanged(WebCore::PermissionName, const WebCore::SecurityOriginData&);
    void processPermissionChanged(WebCore::PermissionName, const WebCore::SecurityOriginData&);

    Logger& logger();

    void resetState();

    ProcessThrottleState throttleStateForStatistics() const { return m_throttleStateForStatistics; }
    Seconds totalForegroundTime() const;
    Seconds totalBackgroundTime() const;
    Seconds totalSuspendedTime() const;

#if ENABLE(WEBXR)
    const WebCore::ProcessIdentity& processIdentity();
#endif

    bool isAlwaysOnLoggingAllowed() const;

    bool shouldRegisterServiceWorkerClients(const WebCore::Site&, PAL::SessionID) const;
    void registerServiceWorkerClients(CompletionHandler<void()>&&);
    void resetHasRegisteredServiceWorkerClients() { m_hasRegisteredServiceWorkerClients = false; }

#if HAVE(AUDIT_TOKEN)
    HashMap<WebCore::PageIdentifier, CoreIPCAuditToken> presentingApplicationAuditTokens() const;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    void requestResourceMonitorRuleLists(bool forTesting);
    void setResourceMonitorRuleListsIfRequired(RefPtr<WebCompiledContentRuleList>);
    void setResourceMonitorRuleLists(RefPtr<WebCompiledContentRuleList>, CompletionHandler<void()>&&);
#endif

    std::optional<SandboxExtension::Handle> sandboxExtensionForFile(const String& fileName) const;
    void addSandboxExtensionForFile(const String& fileName, SandboxExtension::Handle);
    void clearSandboxExtensions();

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    bool receivedLogsDuringLaunchForTesting() const { return m_didReceiveLogsDuringLaunchForTesting; }
#endif

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    void createWasmDebuggerTarget();
    void destroyWasmDebuggerTarget();
    void connectWasmDebuggerTarget(bool isAutomaticConnection, bool immediatelyPause);
    void disconnectWasmDebuggerTarget();
    void dispatchWasmDebuggerMessage(const String& message);
    void setWasmDebuggerTargetIndicating(bool);

    void sendWasmDebuggerResponse(const String& response);
#endif

#if ENABLE(IPC_TESTING_API)
    void takeInvalidMessageStringForTesting(CompletionHandler<void(String&&)>&&);
#endif

private:
    Type type() const final { return Type::WebContent; }

    WebProcessProxy(WebProcessPool&, WebsiteDataStore*, IsPrewarmed, WebCore::CrossOriginMode, LockdownMode, EnhancedSecurity);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "WebContent"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;
    bool shouldSendPendingMessage(const PendingMessage&) final;
    
#if PLATFORM(COCOA)
    void cacheMediaMIMETypesInternal(const Vector<String>&);
#endif

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier&&) override;
    bool shouldConfigureJSCForTesting() const final;
    bool isJITEnabled() const final;
    bool shouldEnableSharedArrayBuffer() const final { return m_crossOriginMode == WebCore::CrossOriginMode::Isolated; }
    bool shouldEnableLockdownMode() const final { return m_lockdownMode == LockdownMode::Enabled; }
    bool shouldEnableEnhancedSecurity() const final { return m_enhancedSecurity == EnhancedSecurity::Enabled; }
    bool shouldDisableJITCage() const final;
#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    RefPtr<XPCEventHandler> xpcEventHandler() const final;
#endif

    void validateFreezerStatus();

    void getWebCryptoMasterKey(CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);
    void wrapCryptoKey(Vector<uint8_t>&&, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);

    using WebProcessProxyMap = HashMap<WebCore::ProcessIdentifier, CheckedRef<WebProcessProxy>>;
    static WebProcessProxyMap& allProcessMap();
    static Vector<Ref<WebProcessProxy>> allProcesses();
    static WebPageProxyMap& globalPageMap();
    static Vector<Ref<WebPageProxy>> globalPages();

    void initializePreferencesForGPUAndNetworkProcesses(const WebPageProxy&);

    void reportProcessDisassociatedWithPageIfNecessary(WebPageProxyIdentifier);
    bool isAssociatedWithPage(WebPageProxyIdentifier) const;

    void platformInitialize();
    void platformDestroy();

#if PLATFORM(COCOA)
    static void registerNotifyObservers();
#endif

    ProcessTerminationReason terminationReason() const;

    // IPC message handlers.
    void didDestroyUserGestureToken(WebCore::PageIdentifier, WebCore::UserGestureTokenIdentifier);

    bool canBeAddedToWebProcessCache() const;
    void shouldTerminate(CompletionHandler<void(bool)>&&);
    
    void getNetworkProcessConnection(CompletionHandler<void(NetworkProcessConnectionInfo&&)>&&);

#if ENABLE(GPU_PROCESS)
    void createGPUProcessConnection(GPUProcessConnectionIdentifier, IPC::Connection::Handle&&);
    void gpuProcessConnectionDidBecomeUnresponsive(GPUProcessConnectionIdentifier);
#endif

#if ENABLE(MODEL_PROCESS)
    void createModelProcessConnection(IPC::Connection::Handle&&);
#endif

    bool shouldAllowNonValidInjectedCode() const;

    static const MemoryCompactLookupOnlyRobinHoodHashSet<String>& platformPathsWithAssumedReadAccess();

    void updateBackgroundResponsivenessTimer();

    void processDidTerminateOrFailedToLaunch(ProcessTerminationReason);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) override;
    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override;
    void willChangeIsResponsive() override;
    void didChangeIsResponsive() override;
    bool canTerminateAuxiliaryProcess();

    void didCollectPrewarmInformation(const WebCore::RegistrableDomain&, const WebCore::PrewarmInformation&);

    void logDiagnosticMessageForResourceLimitTermination(const String& limitKey);
    
    void updateRegistrationWithDataStore();

    void maybeShutDown();

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebContext(UserMessage&&);
    void sendMessageToWebContextWithReply(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);
#endif

    void createSpeechRecognitionServer(SpeechRecognitionServerIdentifier);
    void destroySpeechRecognitionServer(SpeechRecognitionServerIdentifier);

    void systemBeep();
    
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void isAXAuthenticated(CoreIPCAuditToken&&, CompletionHandler<void(bool)>&&);
#endif

#if PLATFORM(COCOA)
    bool messageSourceIsValidWebContentProcess();
#endif

    bool shouldTakeNearSuspendedAssertion() const;
    bool shouldDropNearSuspendedAssertionAfterDelay() const;

    void updateRuntimeStatistics();
    void enableMediaPlaybackIfNecessary();
    void sharedPreferencesDidChange();

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    void createLogStream(IPC::StreamServerConnectionHandle&&, LogStreamIdentifier, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&&);
#else
    void createLogStream(LogStreamIdentifier, CompletionHandler<void()>&&);
#endif
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    void createServiceWorkerDebuggable(WebCore::ServiceWorkerIdentifier, URL&&, WebCore::ServiceWorkerIsInspectable, CompletionHandler<void(bool shouldWaitForAutoInspection)>&&);
    void deleteServiceWorkerDebuggable(WebCore::ServiceWorkerIdentifier);
    void sendMessageToInspector(WebCore::ServiceWorkerIdentifier, String&& message);
#endif

    enum class IsWeak : bool { No, Yes };
    template<typename T> class WeakOrStrongPtr {
    public:
        WeakOrStrongPtr(T& object, IsWeak isWeak)
            : m_isWeak(isWeak)
            , m_weakObject(object)
        {
            updateStrongReference();
        }

        void setIsWeak(IsWeak isWeak)
        {
            m_isWeak = isWeak;
            updateStrongReference();
        }

        T* get() const { return m_weakObject.get(); }
        T* operator->() const { return m_weakObject.get(); }
        T& operator*() const { return *m_weakObject; }
        explicit operator bool() const { return !!m_weakObject; }

    private:
        void updateStrongReference()
        {
            m_strongObject = m_isWeak == IsWeak::Yes ? nullptr : m_weakObject.get();
        }

        IsWeak m_isWeak;
        WeakPtr<T> m_weakObject;
        RefPtr<T> m_strongObject;
    };

    const UniqueRef<BackgroundProcessResponsivenessTimer> m_backgroundResponsivenessTimer;
    
    WeakOrStrongPtr<WebProcessPool> m_processPool; // Pre-warmed and cached processes do not hold a strong reference to their pool.

    bool m_mayHaveUniversalFileReadSandboxExtension { false }; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;
    HashSet<String> m_previouslyApprovedFilePaths;

    WebPageProxyMap m_pageMap;
    WeakHashSet<RemotePageProxy> m_remotePages;
    WeakHashSet<ProvisionalPageProxy> m_provisionalPages;
    WeakHashSet<SuspendedPageProxy> m_suspendedPages;
    UserInitiatedActionMap m_userInitiatedActionMap;
    HashMap<WebCore::PageIdentifier, UserInitiatedActionByAuthorizationTokenMap> m_userInitiatedActionByAuthorizationTokenMap;

    WeakHashMap<VisitedLinkStore, HashSet<WebPageProxyIdentifier>> m_visitedLinkStoresWithUsers;

    int m_numberOfTimesSuddenTerminationWasDisabled { 0 };
    ForegroundWebProcessToken m_foregroundToken;
    BackgroundWebProcessToken m_backgroundToken;
    bool m_areThrottleStateChangesEnabled { true };

#if HAVE(DISPLAY_LINK)
    const UniqueRef<DisplayLinkProcessProxyClient> m_displayLinkClient;
#endif

#if PLATFORM(COCOA)
    bool m_hasSentMessageToUnblockAccessibilityServer { false };
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    Expected<WebCore::Site, SiteState> m_site { Unexpected<SiteState> { SiteState::NotYetSpecified } };
    std::optional<WebCore::Site> m_sharedProcessMainFrameSite;
    HashSet<WebCore::RegistrableDomain> m_sharedProcessDomains;
    bool m_isInProcessCache { false };

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<CompletionHandler<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;

    VisibleWebPageCounter m_visiblePageCounter;
    RefPtr<WebsiteDataStore> m_websiteDataStore;

    SystemMemoryPressureStatus m_memoryPressureStatus { SystemMemoryPressureStatus::Normal };

    bool m_hasCommittedAnyProvisionalLoads { false };
    bool m_hasCommittedAnyMeaningfulProvisionalLoads { false }; // True if the process has committed a provisional load to a URL that was not about:*.
    bool m_isPrewarmed;
    LockdownMode m_lockdownMode { LockdownMode::Disabled };
    EnhancedSecurity m_enhancedSecurity { EnhancedSecurity::Disabled };
    WebCore::CrossOriginMode m_crossOriginMode { WebCore::CrossOriginMode::Shared };
#if PLATFORM(COCOA)
    bool m_hasNetworkExtensionSandboxAccess { false };
    bool m_sentFontdMachExtensionHandles { false };
#endif

#if PLATFORM(WATCHOS)
    RefPtr<ProcessThrottler::BackgroundActivity> m_backgroundActivityForFullscreenFormControls;
#endif

#if PLATFORM(COCOA)
    MediaCaptureSandboxExtensions m_mediaCaptureSandboxExtensions { SandboxExtensionType::None };
#endif
    RefPtr<Logger> m_logger;

    struct RemoteWorkerInformation {
        WebPageProxyIdentifier remoteWorkerPageProxyID;
        WebCore::PageIdentifier remoteWorkerPageID;
        RemoteWorkerInitializationData initializationData;
        RefPtr<ProcessThrottler::Activity> activity;
        WeakHashSet<WebProcessProxy> clientProcesses;
    };
    std::optional<RemoteWorkerInformation> m_serviceWorkerInformation;
    std::optional<RemoteWorkerInformation> m_sharedWorkerInformation;
    bool m_hasServiceWorkerBackgroundProcessing { false };

    struct AudibleMediaActivity {
        Ref<ProcessAssertion> assertion;
        WebProcessWithAudibleMediaToken token;
    };
    std::optional<AudibleMediaActivity> m_audibleMediaActivity;

    std::optional<WebProcessWithMediaStreamingToken> m_mediaStreamingActivity;

    ShutdownPreventingScopeCounter m_shutdownPreventingScopeCounter;

#if ENABLE(IPC_TESTING_API)
    bool m_ignoreInvalidMessageForTesting { false };
#endif
    
    bool m_allowTestOnlyIPC { false };

    using SpeechRecognitionServerMap = HashMap<SpeechRecognitionServerIdentifier, Ref<SpeechRecognitionServer>>;
    SpeechRecognitionServerMap m_speechRecognitionServerMap;
#if ENABLE(MEDIA_STREAM)
    const std::unique_ptr<SpeechRecognitionRemoteRealtimeMediaSourceManager> m_speechRecognitionRemoteRealtimeMediaSourceManager;
#endif
    const std::unique_ptr<WebLockRegistryProxy> m_webLockRegistry;
    const UniqueRef<WebPermissionControllerProxy> m_webPermissionController;
#if ENABLE(ROUTING_ARBITRATION)
    const std::unique_ptr<AudioSessionRoutingArbitratorProxy> m_routingArbitrator;
#endif
    bool m_isConnectedToHardwareConsole { true };
#if PLATFORM(COCOA)
    bool m_platformSuspendDidReleaseNearSuspendedAssertion { false };
#endif
    mutable String m_environmentIdentifier;
    mutable SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
    uint64_t m_sharedPreferencesVersionInNetworkProcess { 0 };
#if ENABLE(GPU_PROCESS)
    uint64_t m_sharedPreferencesVersionInGPUProcess { 0 };
#endif
#if ENABLE(MODEL_PROCESS)
    uint64_t m_sharedPreferencesVersionInModelProcess { 0 };
    std::optional<String> m_memoryAttributionID;
#endif
    uint64_t m_awaitedSharedPreferencesVersion { 0 };
    CompletionHandler<void(bool success)> m_sharedPreferencesForWebProcessCompletionHandler;
#if ENABLE(GPU_PROCESS)
    Markable<GPUProcessConnectionIdentifier> m_gpuProcessConnectionIdentifier;
#endif

    ProcessThrottleState m_throttleStateForStatistics { ProcessThrottleState::Suspended };
    MonotonicTime m_throttleStateForStatisticsTimestamp;
    Seconds m_totalForegroundTime;
    Seconds m_totalBackgroundTime;
    Seconds m_totalSuspendedTime;
    WebCore::ProcessIdentity m_processIdentity;

    bool m_hasRegisteredServiceWorkerClients { true };

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    IPC::ScopedActiveMessageReceiveQueue<LogStream> m_logStream;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    bool m_resourceMonitorRuleListRequestedBySomePage { false };
    RefPtr<WebCompiledContentRuleList> m_resourceMonitorRuleList;
#endif
#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    HashMap<WebCore::ServiceWorkerIdentifier, Ref<ServiceWorkerDebuggableProxy>> m_serviceWorkerDebuggableProxies;
#endif
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    RefPtr<WasmDebuggerDebuggable> m_wasmDebuggerDebuggable;
#endif

    HashMap<String, SandboxExtension::Handle> m_fileSandboxExtensions;

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    class WebProcessXPCEventHandler final : public XPCEventHandler {
    public:
        explicit WebProcessXPCEventHandler(const WebProcessProxy&);

        bool handleXPCEvent(xpc_object_t) final;

    private:
        WeakPtr<WebProcessProxy> m_webProcess;

        bool m_logEndpointEnabled { true };
    };

    bool m_didReceiveLogsDuringLaunchForTesting { false };
#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
} SWIFT_SHARED_REFERENCE(refWebProcessProxy, derefWebProcessProxy);

WTF::TextStream& operator<<(WTF::TextStream&, const WebProcessProxy&);

} // namespace WebKit

inline void refWebProcessProxy(WebKit::WebProcessProxy* WTF_NONNULL obj)
{
    WTF::ref(obj);
}

inline void derefWebProcessProxy(WebKit::WebProcessProxy* WTF_NONNULL obj)
{
    WTF::deref(obj);
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebProcessProxy)
static bool isType(const WebKit::AuxiliaryProcessProxy& process) { return process.type() == WebKit::AuxiliaryProcessProxy::Type::WebContent; }
SPECIALIZE_TYPE_TRAITS_END()
