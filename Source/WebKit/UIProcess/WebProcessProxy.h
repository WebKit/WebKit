/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "GPUProcessPreferencesForWebProcess.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "ProcessLauncher.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "RemoteWorkerInitializationData.h"
#include "ResponsivenessTimer.h"
#include "SpeechRecognitionServer.h"
#include "UserContentControllerIdentifier.h"
#include "VisibleWebPageCounter.h"
#include "WebConnectionToWebProcess.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/CrossOriginMode.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SharedStringHash.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RobinHoodHashSet.h>
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

namespace API {
class Navigation;
class PageConfiguration;
}

namespace WebCore {
class DeferrableOneShotTimer;
class ResourceRequest;
struct NotificationData;
struct PluginInfo;
struct PrewarmInformation;
class SecurityOriginData;
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
class ObjCObjectGraph;
class PageClient;
class ProvisionalPageProxy;
class RemotePageProxy;
class SuspendedPageProxy;
class UserMediaCaptureManagerProxy;
class VisitedLinkStore;
class WebBackForwardListItem;
class WebCompiledContentRuleListData;
class WebFrameProxy;
class WebLockRegistryProxy;
class WebPageGroup;
class WebPageProxy;
class WebPermissionControllerProxy;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;
struct BackForwardListItemState;
struct CoreIPCAuditToken;
struct GPUProcessConnectionParameters;
struct UserMessage;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebPreferencesStore;
struct WebsiteData;

enum class RemoteWorkerType : uint8_t;
enum class WebsiteDataType : uint32_t;

#if ENABLE(MEDIA_STREAM)
class SpeechRecognitionRemoteRealtimeMediaSourceManager;
#endif

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

class WebProcessProxy : public AuxiliaryProcessProxy {
public:
    using WebPageProxyMap = HashMap<WebPageProxyIdentifier, WeakRef<WebPageProxy>>;
    using UserInitiatedActionByAuthorizationTokenMap = HashMap<WTF::UUID, RefPtr<API::UserInitiatedAction>>;
    typedef HashMap<uint64_t, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    enum class IsPrewarmed : bool { No, Yes };

    enum class ShouldLaunchProcess : bool { No, Yes };
    enum class LockdownMode : bool { Disabled, Enabled };

    static Ref<WebProcessProxy> create(WebProcessPool&, WebsiteDataStore*, LockdownMode, IsPrewarmed, WebCore::CrossOriginMode = WebCore::CrossOriginMode::Shared, ShouldLaunchProcess = ShouldLaunchProcess::Yes);
    static Ref<WebProcessProxy> createForRemoteWorkers(RemoteWorkerType, WebProcessPool&, WebCore::RegistrableDomain&&, WebsiteDataStore&);

    ~WebProcessProxy();

    static void forWebPagesWithOrigin(PAL::SessionID, const WebCore::SecurityOriginData&, const Function<void(WebPageProxy&)>&);
    static Vector<std::pair<WebCore::ProcessIdentifier, WebCore::RegistrableDomain>> allowedFirstPartiesForCookies();

    WebConnection* webConnection() const { return m_webConnection.get(); }
    RefPtr<WebConnection> protectedWebConnection() const { return m_webConnection; }

    unsigned suspendedPageCount() const { return m_suspendedPages.computeSize(); }
    void addSuspendedPageProxy(SuspendedPageProxy&);
    void removeSuspendedPageProxy(SuspendedPageProxy&);

    WebProcessPool* processPoolIfExists() const;
    WebProcessPool& processPool() const;
    Ref<WebProcessPool> protectedProcessPool() const;

#if ENABLE(GPU_PROCESS)
    const std::optional<GPUProcessPreferencesForWebProcess>& preferencesForGPUProcess() const { return m_preferencesForGPUProcess; }
#endif

    bool isMatchingRegistrableDomain(const WebCore::RegistrableDomain& domain) const { return m_registrableDomain ? *m_registrableDomain == domain : false; }
    WebCore::RegistrableDomain registrableDomain() const { return valueOrDefault(m_registrableDomain); }
    const std::optional<WebCore::RegistrableDomain>& optionalRegistrableDomain() const { return m_registrableDomain; }

    enum class WillShutDown : bool { No, Yes };
    void setIsInProcessCache(bool, WillShutDown = WillShutDown::No);
    bool isInProcessCache() const { return m_isInProcessCache; }

    void enableRemoteWorkers(RemoteWorkerType, const UserContentControllerIdentifier&);
    void disableRemoteWorkers(OptionSet<RemoteWorkerType>);

    WebsiteDataStore* websiteDataStore() const { ASSERT(m_websiteDataStore); return m_websiteDataStore.get(); }
    RefPtr<WebsiteDataStore> protectedWebsiteDataStore() const;
    void setWebsiteDataStore(WebsiteDataStore&);
    
    PAL::SessionID sessionID() const;

    static bool hasReachedProcessCountLimit();
    static void setProcessCountLimit(unsigned);

    static RefPtr<WebProcessProxy> processForIdentifier(WebCore::ProcessIdentifier);
    static RefPtr<WebPageProxy> webPage(WebPageProxyIdentifier);
    static RefPtr<WebPageProxy> audioCapturingWebPage();
#if ENABLE(WEBXR) && !USE(OPENXR)
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
    unsigned pageCount() const { return m_pageMap.size(); }
    unsigned provisionalPageCount() const { return m_provisionalPages.computeSize(); }
    unsigned visiblePageCount() const { return m_visiblePageCounter.value(); }

    Vector<WeakPtr<RemotePageProxy>> remotePages() const;

    void activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&&); // This is what is reported to ActivityMonitor.

    bool isRunningServiceWorkers() const { return !!m_serviceWorkerInformation; }
    bool isStandaloneServiceWorkerProcess() const { return isRunningServiceWorkers() && !pageCount(); }
    bool isRunningSharedWorkers() const { return !!m_sharedWorkerInformation; }
    bool isStandaloneSharedWorkerProcess() const { return isRunningSharedWorkers() && !pageCount(); }
    bool isRunningWorkers() const { return m_sharedWorkerInformation || m_serviceWorkerInformation; }

    bool isDummyProcessProxy() const;

    void didCreateWebPageInProcess(WebCore::PageIdentifier);

    void addVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);
    void removeVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);

    void addWebUserContentControllerProxy(WebUserContentControllerProxy&);
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);

    void recordUserGestureAuthorizationToken(WebCore::PageIdentifier, WTF::UUID);
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(uint64_t);
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(WebCore::PageIdentifier, std::optional<WTF::UUID>, uint64_t);

    void consumeIfNotVerifiablyFromUIProcess(WebCore::PageIdentifier, API::UserInitiatedAction&, std::optional<WTF::UUID>);

    bool isResponsive() const;

    VisibleWebPageToken visiblePageToken() const;

    void addPreviouslyApprovedFileURL(const URL&);
    bool wasPreviouslyApprovedFileURL(const URL&) const;

    void updateTextCheckerState();

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(WebPageProxy&, const String&);
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

    static void notifyPageStatisticsAndDataRecordsProcessed();

    static void notifyWebsiteDataDeletionForRegistrableDomainsFinished();
    static void notifyWebsiteDataScanForRegistrableDomainsFinished();

    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination(ProcessTerminationReason);

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

    void windowServerConnectionStateChanged();

    void setIsHoldingLockedFiles(bool);

    ProcessThrottler& throttler() final { return m_throttler; }
    const ProcessThrottler& throttler() const { return m_throttler; }

    void isResponsive(CompletionHandler<void(bool isWebProcessResponsive)>&&);
    void isResponsiveWithLazyStop();
    void didReceiveBackgroundResponsivenessPing();

    void memoryPressureStatusChanged(bool isUnderMemoryPressure) { m_isUnderMemoryPressure = isUnderMemoryPressure; }
    bool isUnderMemoryPressure() const { return m_isUnderMemoryPressure; }

    void processTerminated();

    void didExceedCPULimit();
    void didExceedActiveMemoryLimit();
    void didExceedInactiveMemoryLimit();

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

#if PLATFORM(COCOA)
    Vector<String> mediaMIMETypes() const;
    void cacheMediaMIMETypes(const Vector<String>&);
#endif

#if PLATFORM(MAC)
    void requestHighPerformanceGPU();
    void releaseHighPerformanceGPU();
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

    bool hasSameGPUProcessPreferencesAs(const API::PageConfiguration&) const;

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
    void establishRemoteWorkerContext(RemoteWorkerType, const WebPreferencesStore&, const WebCore::RegistrableDomain&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&&);
    void registerRemoteWorkerClientProcess(RemoteWorkerType, WebProcessProxy&);
    void unregisterRemoteWorkerClientProcess(RemoteWorkerType, WebProcessProxy&);
    void updateRemoteWorkerProcessAssertion(RemoteWorkerType);
    bool hasServiceWorkerPageProxy(WebPageProxyIdentifier pageProxyID) { return m_serviceWorkerInformation && m_serviceWorkerInformation->remoteWorkerPageProxyID == pageProxyID; }
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
    void startServiceWorkerBackgroundProcessing();
    void endServiceWorkerBackgroundProcessing();
    void setThrottleStateForTesting(ProcessThrottleState);

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    UserMediaCaptureManagerProxy* userMediaCaptureManagerProxy() { return m_userMediaCaptureManagerProxy.get(); }
#endif

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidFinishLaunching();
    void gpuProcessExited(ProcessTerminationReason);
#endif

#if PLATFORM(COCOA)
    bool hasNetworkExtensionSandboxAccess() const { return m_hasNetworkExtensionSandboxAccess; }
    void markHasNetworkExtensionSandboxAccess() { m_hasNetworkExtensionSandboxAccess = true; }
#endif

#if ENABLE(ROUTING_ARBITRATION)
    AudioSessionRoutingArbitratorProxy& audioSessionRoutingArbitrator() { return m_routingArbitrator.get(); }
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
#endif
    void pageMutedStateChanged(WebCore::PageIdentifier, WebCore::MediaProducerMutedStateFlags);
    void pageIsBecomingInvisible(WebCore::PageIdentifier);

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    static bool shouldEnableRemoteInspector();
#endif

    void markProcessAsRecentlyUsed();

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
    void platformSuspendProcess();
    void platformResumeProcess();
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    void setCaptionDisplayMode(WebCore::CaptionUserPreferences::CaptionDisplayMode);
    void setCaptionLanguage(const String&);
#endif
    void getNotifications(const URL&, const String&, CompletionHandler<void(Vector<WebCore::NotificationData>&&)>&&);

    void setAppBadge(std::optional<WebPageProxyIdentifier>, const WebCore::SecurityOriginData&, std::optional<uint64_t> badge);
    void setClientBadge(WebPageProxyIdentifier, const WebCore::SecurityOriginData&, std::optional<uint64_t> badge);

    WebCore::CrossOriginMode crossOriginMode() const { return m_crossOriginMode; }
    LockdownMode lockdownMode() const { return m_lockdownMode; }

#if PLATFORM(COCOA)
    std::optional<audit_token_t> auditToken() const;
    Vector<SandboxExtension::Handle> fontdMachExtensionHandles(SandboxExtension::MachBootstrapOptions) const;
#endif

    bool isConnectedToHardwareConsole() const { return m_isConnectedToHardwareConsole; }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void hardwareConsoleStateChanged();
#endif

    const WeakHashSet<WebProcessProxy>* serviceWorkerClientProcesses() const;
    const WeakHashSet<WebProcessProxy>* sharedWorkerClientProcesses() const;

    static void permissionChanged(WebCore::PermissionName, const WebCore::SecurityOriginData&);
    void processPermissionChanged(WebCore::PermissionName, const WebCore::SecurityOriginData&);

    void addAllowedFirstPartyForCookies(const WebCore::RegistrableDomain&);

    Logger& logger();

    void resetState();

protected:
    WebProcessProxy(WebProcessPool&, WebsiteDataStore*, IsPrewarmed, WebCore::CrossOriginMode, LockdownMode);

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
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;
    bool shouldConfigureJSCForTesting() const final;
    bool isJITEnabled() const final;
    bool shouldEnableSharedArrayBuffer() const final { return m_crossOriginMode == WebCore::CrossOriginMode::Isolated; }
    bool shouldEnableLockdownMode() const final { return m_lockdownMode == LockdownMode::Enabled; }

    void validateFreezerStatus();

private:
    using WebProcessProxyMap = HashMap<WebCore::ProcessIdentifier, CheckedRef<WebProcessProxy>>;
    static WebProcessProxyMap& allProcessMap();
    static Vector<Ref<WebProcessProxy>> allProcesses();
    static WebPageProxyMap& globalPageMap();
    static Vector<Ref<WebPageProxy>> globalPages();

    void initializePreferencesForGPUProcess(const WebPageProxy&);

    void reportProcessDisassociatedWithPageIfNecessary(WebPageProxyIdentifier);
    bool isAssociatedWithPage(WebPageProxyIdentifier) const;

    void platformInitialize();
    void platformDestroy();

    // IPC message handlers.
    void updateBackForwardItem(const BackForwardListItemState&);
    void didDestroyFrame(WebCore::FrameIdentifier, WebPageProxyIdentifier);
    void didDestroyUserGestureToken(WebCore::PageIdentifier, uint64_t);

    bool canBeAddedToWebProcessCache() const;
    void shouldTerminate(CompletionHandler<void(bool)>&&);

    bool hasProvisionalPageWithID(WebPageProxyIdentifier) const;
    bool isAllowedToUpdateBackForwardItem(WebBackForwardListItem&) const;
    
    void getNetworkProcessConnection(CompletionHandler<void(NetworkProcessConnectionInfo&&)>&&);

#if ENABLE(GPU_PROCESS)
    void createGPUProcessConnection(IPC::Connection::Handle&&, WebKit::GPUProcessConnectionParameters&&);
#endif

    bool shouldAllowNonValidInjectedCode() const;

    static const MemoryCompactLookupOnlyRobinHoodHashSet<String>& platformPathsWithAssumedReadAccess();

    void updateBackgroundResponsivenessTimer();

    void processDidTerminateOrFailedToLaunch(ProcessTerminationReason);

    // IPC::Connection::Client
    friend class WebConnectionToWebProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override;
    void willChangeIsResponsive() override;
    void didChangeIsResponsive() override;

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

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
    
#if PLATFORM(MAC)
    void isAXAuthenticated(CoreIPCAuditToken&&, CompletionHandler<void(bool)>&&);
#endif

#if PLATFORM(COCOA)
    bool messageSourceIsValidWebContentProcess();
#endif

    bool shouldTakeNearSuspendedAssertion() const;
    bool shouldDropNearSuspendedAssertionAfterDelay() const;

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

    BackgroundProcessResponsivenessTimer m_backgroundResponsivenessTimer;
    
    RefPtr<WebConnectionToWebProcess> m_webConnection;
    WeakOrStrongPtr<WebProcessPool> m_processPool; // Pre-warmed and cached processes do not hold a strong reference to their pool.

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;
    HashSet<String> m_previouslyApprovedFilePaths;

    WebPageProxyMap m_pageMap;
    WeakHashSet<RemotePageProxy> m_remotePages;
    WeakHashSet<ProvisionalPageProxy> m_provisionalPages;
    WeakHashSet<SuspendedPageProxy> m_suspendedPages;
    UserInitiatedActionMap m_userInitiatedActionMap;
    HashMap<WebCore::PageIdentifier, UserInitiatedActionByAuthorizationTokenMap> m_userInitiatedActionByAuthorizationTokenMap;

    WeakHashMap<VisitedLinkStore, HashSet<WebPageProxyIdentifier>> m_visitedLinkStoresWithUsers;
    WeakHashSet<WebUserContentControllerProxy> m_webUserContentControllerProxies;

    int m_numberOfTimesSuddenTerminationWasDisabled;
    ProcessThrottler m_throttler;
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_activityForHoldingLockedFiles;
    ForegroundWebProcessToken m_foregroundToken;
    BackgroundWebProcessToken m_backgroundToken;
    bool m_areThrottleStateChangesEnabled { true };

#if HAVE(DISPLAY_LINK)
    DisplayLinkProcessProxyClient m_displayLinkClient;
#endif

#if PLATFORM(COCOA)
    bool m_hasSentMessageToUnblockAccessibilityServer { false };
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    std::optional<WebCore::RegistrableDomain> m_registrableDomain;
    bool m_isInProcessCache { false };

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<CompletionHandler<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;

    VisibleWebPageCounter m_visiblePageCounter;
    RefPtr<WebsiteDataStore> m_websiteDataStore;

    bool m_isUnderMemoryPressure { false };

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
#endif

    bool m_hasCommittedAnyProvisionalLoads { false };
    bool m_hasCommittedAnyMeaningfulProvisionalLoads { false }; // True if the process has committed a provisional load to a URL that was not about:*.
    bool m_isPrewarmed;
    LockdownMode m_lockdownMode { LockdownMode::Disabled };
    WebCore::CrossOriginMode m_crossOriginMode { WebCore::CrossOriginMode::Shared };
#if PLATFORM(COCOA)
    bool m_hasNetworkExtensionSandboxAccess { false };
#endif

#if PLATFORM(WATCHOS)
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_backgroundActivityForFullscreenFormControls;
#endif

#if PLATFORM(COCOA)
    MediaCaptureSandboxExtensions m_mediaCaptureSandboxExtensions { SandboxExtensionType::None };
#endif
    RefPtr<Logger> m_logger;

    struct RemoteWorkerInformation {
        WebPageProxyIdentifier remoteWorkerPageProxyID;
        WebCore::PageIdentifier remoteWorkerPageID;
        RemoteWorkerInitializationData initializationData;
        ProcessThrottler::ActivityVariant activity;
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

    using SpeechRecognitionServerMap = HashMap<SpeechRecognitionServerIdentifier, std::unique_ptr<SpeechRecognitionServer>>;
    SpeechRecognitionServerMap m_speechRecognitionServerMap;
#if ENABLE(MEDIA_STREAM)
    std::unique_ptr<SpeechRecognitionRemoteRealtimeMediaSourceManager> m_speechRecognitionRemoteRealtimeMediaSourceManager;
#endif
    std::unique_ptr<WebLockRegistryProxy> m_webLockRegistry;
    std::unique_ptr<WebPermissionControllerProxy> m_webPermissionController;
#if ENABLE(ROUTING_ARBITRATION)
    UniqueRef<AudioSessionRoutingArbitratorProxy> m_routingArbitrator;
#endif
    bool m_isConnectedToHardwareConsole { true };
#if PLATFORM(MAC)
    bool m_platformSuspendDidReleaseNearSuspendedAssertion { false };
#endif
    mutable String m_environmentIdentifier;
#if ENABLE(GPU_PROCESS)
    mutable std::optional<GPUProcessPreferencesForWebProcess> m_preferencesForGPUProcess;
#endif
};

WTF::TextStream& operator<<(WTF::TextStream&, const WebProcessProxy&);

} // namespace WebKit
