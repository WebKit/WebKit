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

#include "APIUserInitiatedAction.h"
#include "AuxiliaryProcessProxy.h"
#include "BackgroundProcessResponsivenessTimer.h"
#include "DisplayLinkObserverID.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessProxy.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "ResponsivenessTimer.h"
#include "ServiceWorkerInitializationData.h"
#include "UserContentControllerIdentifier.h"
#include "VisibleWebPageCounter.h"
#include "WebConnectionToWebProcess.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxyMessagesReplies.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SharedStringHash.h>
#include <WebCore/SleepDisabler.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashSet.h>

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#include "LayerHostingContext.h"
#endif

namespace API {
class Navigation;
class PageConfiguration;
}

namespace WebCore {
class DeferrableOneShotTimer;
class ResourceRequest;
struct PluginInfo;
struct PrewarmInformation;
struct SecurityOriginData;
enum class ThirdPartyCookieBlockingMode : uint8_t;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class AudioSessionRoutingArbitratorProxy;
class ObjCObjectGraph;
class PageClient;
class ProvisionalPageProxy;
class UserMediaCaptureManagerProxy;
class VisitedLinkStore;
class WebBackForwardListItem;
class WebCompiledContentRuleListData;
class WebFrameProxy;
class WebPageGroup;
class WebPageProxy;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;
enum class WebsiteDataType : uint32_t;
struct BackForwardListItemState;
struct UserMessage;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebPreferencesStore;
struct WebsiteData;

enum ForegroundWebProcessCounterType { };
typedef RefCounter<ForegroundWebProcessCounterType> ForegroundWebProcessCounter;
typedef ForegroundWebProcessCounter::Token ForegroundWebProcessToken;
enum BackgroundWebProcessCounterType { };
typedef RefCounter<BackgroundWebProcessCounterType> BackgroundWebProcessCounter;
typedef BackgroundWebProcessCounter::Token BackgroundWebProcessToken;
enum WebProcessWithAudibleMediaCounterType { };
using WebProcessWithAudibleMediaCounter = RefCounter<WebProcessWithAudibleMediaCounterType>;
using WebProcessWithAudibleMediaToken = WebProcessWithAudibleMediaCounter::Token;
enum class CheckBackForwardList : bool { No, Yes };

class WebProcessProxy : public AuxiliaryProcessProxy, public ResponsivenessTimer::Client, public ThreadSafeRefCounted<WebProcessProxy>, public CanMakeWeakPtr<WebProcessProxy>, private ProcessThrottlerClient {
public:
    typedef HashMap<WebCore::FrameIdentifier, RefPtr<WebFrameProxy>> WebFrameProxyMap;
    typedef HashMap<WebPageProxyIdentifier, WebPageProxy*> WebPageProxyMap;
    typedef HashMap<uint64_t, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    enum class IsPrewarmed {
        No,
        Yes
    };

    enum class ShouldLaunchProcess : bool { No, Yes };

    static Ref<WebProcessProxy> create(WebProcessPool&, WebsiteDataStore*, IsPrewarmed, ShouldLaunchProcess = ShouldLaunchProcess::Yes);
    static Ref<WebProcessProxy> createForServiceWorkers(WebProcessPool&, WebCore::RegistrableDomain&&, WebsiteDataStore&);

    ~WebProcessProxy();

    static void forWebPagesWithOrigin(PAL::SessionID, const WebCore::SecurityOriginData&, const Function<void(WebPageProxy&)>&);

    WebConnection* webConnection() const { return m_webConnection.get(); }

    unsigned suspendedPageCount() const { return m_suspendedPageCount; }
    void incrementSuspendedPageCount();
    void decrementSuspendedPageCount();

    WebProcessPool* processPoolIfExists() const;
    WebProcessPool& processPool() const;

    bool isMatchingRegistrableDomain(const WebCore::RegistrableDomain& domain) const { return m_registrableDomain ? *m_registrableDomain == domain : false; }
    WebCore::RegistrableDomain registrableDomain() const { return m_registrableDomain.valueOr(WebCore::RegistrableDomain { }); }
    void setIsInProcessCache(bool);
    bool isInProcessCache() const { return m_isInProcessCache; }

    void enableServiceWorkers(const Optional<UserContentControllerIdentifier>&);
    void disableServiceWorkers();

    WebsiteDataStore& websiteDataStore() const { ASSERT(m_websiteDataStore); return *m_websiteDataStore; }
    void setWebsiteDataStore(WebsiteDataStore&);
    
    PAL::SessionID sessionID() const;

    static WebProcessProxy* processForIdentifier(WebCore::ProcessIdentifier);
    static WebPageProxy* webPage(WebPageProxyIdentifier);
    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    enum class BeginsUsingDataStore : bool { No, Yes };
    void addExistingWebPage(WebPageProxy&, BeginsUsingDataStore);

    enum class EndsUsingDataStore : bool { No, Yes };
    void removeWebPage(WebPageProxy&, EndsUsingDataStore);

    void addProvisionalPageProxy(ProvisionalPageProxy&);
    void removeProvisionalPageProxy(ProvisionalPageProxy&);
    
    typename WebPageProxyMap::ValuesConstIteratorRange pages() const { return m_pageMap.values(); }
    unsigned pageCount() const { return m_pageMap.size(); }
    unsigned provisionalPageCount() const { return m_provisionalPages.size(); }
    unsigned visiblePageCount() const { return m_visiblePageCounter.value(); }

    void activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&&); // This is what is reported to ActivityMonitor.

    bool isRunningServiceWorkers() const { return !!m_serviceWorkerInformation; }
    bool isStandaloneServiceWorkerProcess() const { return isRunningServiceWorkers() && !pageCount(); }

    bool isDummyProcessProxy() const;

    void didCreateWebPageInProcess(WebCore::PageIdentifier);

    void addVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);
    void removeVisitedLinkStoreUser(VisitedLinkStore&, WebPageProxyIdentifier);

    void addWebUserContentControllerProxy(WebUserContentControllerProxy&);
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);

    RefPtr<API::UserInitiatedAction> userInitiatedActivity(uint64_t);

    bool isResponsive() const;

    WebFrameProxy* webFrame(WebCore::FrameIdentifier) const;
    bool canCreateFrame(WebCore::FrameIdentifier) const;
    void frameCreated(WebCore::FrameIdentifier, WebFrameProxy&);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

    VisibleWebPageToken visiblePageToken() const;

    void updateTextCheckerState();

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(WebPageProxy&, const String&);
    bool hasAssumedReadAccessToURL(const URL&) const;

    bool checkURLReceivedFromWebProcess(const String&, CheckBackForwardList = CheckBackForwardList::Yes);
    bool checkURLReceivedFromWebProcess(const URL&, CheckBackForwardList = CheckBackForwardList::Yes);

    static bool fullKeyboardAccessEnabled();

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    static void notifyPageStatisticsAndDataRecordsProcessed();
    static void notifyPageStatisticsTelemetryFinished(API::Object* messageBody);

    static void notifyWebsiteDataDeletionForRegistrableDomainsFinished();
    static void notifyWebsiteDataScanForRegistrableDomainsFinished();

    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
#endif

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination(ProcessTerminationReason);

    enum class UseLazyStop : bool { No, Yes };
    void startResponsivenessTimer(UseLazyStop = UseLazyStop::No);
    void stopResponsivenessTimer();

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

    void windowServerConnectionStateChanged();

    void setIsHoldingLockedFiles(bool);

    ProcessThrottler& throttler() final { return m_throttler; }

    void isResponsive(CompletionHandler<void(bool isWebProcessResponsive)>&&);
    void isResponsiveWithLazyStop();
    void didReceiveMainThreadPing();
    void didReceiveBackgroundResponsivenessPing();

    void memoryPressureStatusChanged(bool isUnderMemoryPressure) { m_isUnderMemoryPressure = isUnderMemoryPressure; }
    bool isUnderMemoryPressure() const { return m_isUnderMemoryPressure; }
    void didExceedInactiveMemoryLimitWhileActive();

    void processTerminated();

    void didExceedCPULimit();
    void didExceedActiveMemoryLimit();
    void didExceedInactiveMemoryLimit();

    void didCommitProvisionalLoad() { m_hasCommittedAnyProvisionalLoads = true; }
    bool hasCommittedAnyProvisionalLoads() const { return m_hasCommittedAnyProvisionalLoads; }

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

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    void startDisplayLink(DisplayLinkObserverID, WebCore::PlatformDisplayID);
    void stopDisplayLink(DisplayLinkObserverID, WebCore::PlatformDisplayID);
#endif

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void shutDown();

    class ScopePreventingShutdown {
    public:
        explicit ScopePreventingShutdown(WebProcessProxy& process)
            : m_process(process)
        {
            ++(m_process->m_shutdownPreventingScopeCount);
        }

        ~ScopePreventingShutdown()
        {
            ASSERT(m_process->m_shutdownPreventingScopeCount);
            if (!--(m_process->m_shutdownPreventingScopeCount))
                m_process->maybeShutDown();
        }

    private:
        Ref<WebProcessProxy> m_process;
    };

    ScopePreventingShutdown makeScopePreventingShutdown() { return ScopePreventingShutdown { *this }; }

    void didStartProvisionalLoadForMainFrame(const URL&);

    // ProcessThrottlerClient
    void sendPrepareToSuspend(IsSuspensionImminent, CompletionHandler<void()>&&) final;
    void sendProcessDidResume() final;
    void didSetAssertionType(ProcessAssertionType) final;
    ASCIILiteral clientName() const final { return "WebProcess"_s; }

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
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    void enableRemoteInspectorIfNeeded();
#endif
    
#if PLATFORM(COCOA)
    void unblockAccessibilityServerIfNeeded();
#if ENABLE(CFPREFS_DIRECT_MODE)
    void unblockPreferenceServiceIfNeeded();
#endif
#endif

    void updateAudibleMediaAssertions();

    void ref() final { ThreadSafeRefCounted::ref(); }
    void deref() final { ThreadSafeRefCounted::deref(); }

#if ENABLE(SERVICE_WORKER)
    void establishServiceWorkerContext(const WebPreferencesStore&, CompletionHandler<void()>&&);
    void setServiceWorkerUserAgent(const String&);
    void updateServiceWorkerPreferencesStore(const WebPreferencesStore&);
    bool hasServiceWorkerPageProxy(WebPageProxyIdentifier pageProxyID) { return m_serviceWorkerInformation && m_serviceWorkerInformation->serviceWorkerPageProxyID == pageProxyID; }
    void updateServiceWorkerProcessAssertion();
    void registerServiceWorkerClientProcess(WebProcessProxy&);
    void unregisterServiceWorkerClientProcess(WebProcessProxy&);
    bool hasServiceWorkerForegroundActivityForTesting() const;
    bool hasServiceWorkerBackgroundActivityForTesting() const;
#endif
    void setAssertionTypeForTesting(ProcessAssertionType type) { didSetAssertionType(type); }

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    UserMediaCaptureManagerProxy* userMediaCaptureManagerProxy() { return m_userMediaCaptureManagerProxy.get(); }
#endif

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(IOS_FAMILY)
    bool hasIssuedAttachmentElementRelatedSandboxExtensions() const { return m_hasIssuedAttachmentElementRelatedSandboxExtensions; }
    void setHasIssuedAttachmentElementRelatedSandboxExtensions() { m_hasIssuedAttachmentElementRelatedSandboxExtensions = true; }
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID);
#endif

#if ENABLE(GPU_PROCESS)
    void gpuProcessCrashed();
#endif

    bool hasSleepDisabler() const;

#if PLATFORM(COCOA)
    bool hasNetworkExtensionSandboxAccess() const { return m_hasNetworkExtensionSandboxAccess; }
    void markHasNetworkExtensionSandboxAccess() { m_hasNetworkExtensionSandboxAccess = true; }
#endif
#if PLATFORM(IOS)
    bool hasManagedSessionSandboxAccess() const { return m_hasManagedSessionSandboxAccess; }
    void markHasManagedSessionSandboxAccess() { m_hasManagedSessionSandboxAccess = true; }
#endif

#if ENABLE(ROUTING_ARBITRATION)
    AudioSessionRoutingArbitratorProxy& audioSessionRoutingArbitrator() { return m_routingArbitrator.get(); }
#endif

protected:
    WebProcessProxy(WebProcessPool&, WebsiteDataStore*, IsPrewarmed);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "WebContent"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;
    bool shouldSendPendingMessage(const PendingMessage&) final;
    
    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

#if PLATFORM(COCOA)
    void cacheMediaMIMETypesInternal(const Vector<String>&);
#endif

    bool shouldConfigureJSCForTesting() const final;
    bool isJITEnabled() const final;

    void validateFreezerStatus();

private:
    // IPC message handlers.
    void updateBackForwardItem(const BackForwardListItemState&);
    void didDestroyFrame(WebCore::FrameIdentifier);
    void didDestroyUserGestureToken(uint64_t);

    bool canBeAddedToWebProcessCache() const;
    void shouldTerminate(CompletionHandler<void(bool)>&&);

    bool hasProvisionalPageWithID(WebPageProxyIdentifier) const;
    bool isAllowedToUpdateBackForwardItem(WebBackForwardListItem&) const;

    // Plugins
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPlugins(bool refresh, CompletionHandler<void(Vector<WebCore::PluginInfo>&& plugins, Vector<WebCore::PluginInfo>&& applicationPlugins, Optional<Vector<WebCore::SupportedPluginIdentifier>>&&)>&&);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPluginProcessConnection(uint64_t pluginProcessToken, Messages::WebProcessProxy::GetPluginProcessConnectionDelayedReply&&);
#endif
    void addPlugInAutoStartOriginHash(String&& pageOrigin, uint32_t hash);
    void plugInDidReceiveUserInteraction(uint32_t hash);
    
    void getNetworkProcessConnection(Messages::WebProcessProxy::GetNetworkProcessConnectionDelayedReply&&);

#if ENABLE(GPU_PROCESS)
    void getGPUProcessConnection(Messages::WebProcessProxy::GetGPUProcessConnectionDelayedReply&&);
#endif

    bool platformIsBeingDebugged() const;
    bool shouldAllowNonValidInjectedCode() const;

    static const HashSet<String>& platformPathsWithAssumedReadAccess();

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }
    void updateBackgroundResponsivenessTimer();

    void processDidTerminateOrFailedToLaunch();

    bool isReleaseLoggingAllowed() const;

    // IPC::Connection::Client
    friend class WebConnectionToWebProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override;
    void willChangeIsResponsive() override;
    void didChangeIsResponsive() override;
    bool mayBecomeUnresponsive() override;

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool canTerminateAuxiliaryProcess();

    void didCollectPrewarmInformation(const WebCore::RegistrableDomain&, const WebCore::PrewarmInformation&);

    void logDiagnosticMessageForResourceLimitTermination(const String& limitKey);
    
    void updateRegistrationWithDataStore();
    Vector<String> platformOverrideLanguages() const;

    void maybeShutDown();

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebContext(UserMessage&&);
    void sendMessageToWebContextWithReply(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);
#endif

    void didCreateSleepDisabler(WebCore::SleepDisablerIdentifier, const String& reason, bool display);
    void didDestroySleepDisabler(WebCore::SleepDisablerIdentifier);

    enum class IsWeak { No, Yes };
    template<typename T> class WeakOrStrongPtr {
    public:
        WeakOrStrongPtr(T& object, IsWeak isWeak)
            : m_isWeak(isWeak)
            , m_weakObject(makeWeakPtr(object))
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

    ResponsivenessTimer m_responsivenessTimer;
    BackgroundProcessResponsivenessTimer m_backgroundResponsivenessTimer;
    
    RefPtr<WebConnectionToWebProcess> m_webConnection;
    WeakOrStrongPtr<WebProcessPool> m_processPool; // Pre-warmed and cached processes do not hold a strong reference to their pool.

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    WebPageProxyMap m_pageMap;
    WebFrameProxyMap m_frameMap;
    HashSet<ProvisionalPageProxy*> m_provisionalPages;
    UserInitiatedActionMap m_userInitiatedActionMap;

    HashMap<VisitedLinkStore*, HashSet<WebPageProxyIdentifier>> m_visitedLinkStoresWithUsers;
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;

    int m_numberOfTimesSuddenTerminationWasDisabled;
    ProcessThrottler m_throttler;
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_activityForHoldingLockedFiles;
    ForegroundWebProcessToken m_foregroundToken;
    BackgroundWebProcessToken m_backgroundToken;

#if ENABLE(ROUTING_ARBITRATION)
    UniqueRef<AudioSessionRoutingArbitratorProxy> m_routingArbitrator;
#endif

#if PLATFORM(COCOA)
    bool m_hasSentMessageToUnblockAccessibilityServer { false };
    bool m_hasSentMessageToUnblockPreferenceService { false };
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    Optional<WebCore::RegistrableDomain> m_registrableDomain;
    bool m_isInProcessCache { false };

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<CompletionHandler<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;

    VisibleWebPageCounter m_visiblePageCounter;

    RefPtr<WebsiteDataStore> m_websiteDataStore;

    bool m_isUnderMemoryPressure { false };

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
#endif

    unsigned m_suspendedPageCount { 0 };
    unsigned m_shutdownPreventingScopeCount { 0 };
    bool m_hasCommittedAnyProvisionalLoads { false };
    bool m_isPrewarmed;
#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(IOS_FAMILY)
    bool m_hasIssuedAttachmentElementRelatedSandboxExtensions { false };
#endif
#if PLATFORM(COCOA)
    bool m_hasNetworkExtensionSandboxAccess { false };
#endif
#if PLATFORM(IOS)
    bool m_hasManagedSessionSandboxAccess { false };
#endif
    Optional<UseLazyStop> m_shouldStartResponsivenessTimerWhenLaunched;

#if PLATFORM(WATCHOS)
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_backgroundActivityForFullscreenFormControls;
#endif

#if PLATFORM(COCOA)
    MediaCaptureSandboxExtensions m_mediaCaptureSandboxExtensions { SandboxExtensionType::None };
#endif
    RefPtr<Logger> m_logger;

    struct ServiceWorkerInformation {
        WebPageProxyIdentifier serviceWorkerPageProxyID;
        WebCore::PageIdentifier serviceWorkerPageID;
        ServiceWorkerInitializationData initializationData;
        ProcessThrottler::ActivityVariant activity;
        WeakHashSet<WebProcessProxy> clientProcesses;
    };
    Optional<ServiceWorkerInformation> m_serviceWorkerInformation;

    HashMap<WebCore::SleepDisablerIdentifier, std::unique_ptr<WebCore::SleepDisabler>> m_sleepDisablers;

    struct AudibleMediaActivity {
        UniqueRef<ProcessAssertion> assertion;
        WebProcessWithAudibleMediaToken token;
    };
    Optional<AudibleMediaActivity> m_audibleMediaActivity;
};

} // namespace WebKit
