/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include "BackgroundProcessResponsivenessTimer.h"
#include "ChildProcessProxy.h"
#include "MessageReceiverMap.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "ResponsivenessTimer.h"
#include "VisibleWebPageCounter.h"
#include "WebConnectionToWebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/MessagePortChannelProvider.h>
#include <WebCore/MessagePortIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SharedStringHash.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
#include "DisplayLink.h"
#endif

namespace API {
class Navigation;
class PageConfiguration;
}

namespace WebCore {
class ResourceRequest;
struct PluginInfo;
struct SecurityOriginData;
}

namespace WebKit {

class NetworkProcessProxy;
class ObjCObjectGraph;
class PageClient;
class ProvisionalPageProxy;
class UserMediaCaptureManagerProxy;
class VisitedLinkStore;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageGroup;
class WebPageProxy;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;
enum class WebsiteDataType;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebsiteData;

#if PLATFORM(IOS_FAMILY)
enum ForegroundWebProcessCounterType { };
typedef RefCounter<ForegroundWebProcessCounterType> ForegroundWebProcessCounter;
typedef ForegroundWebProcessCounter::Token ForegroundWebProcessToken;
enum BackgroundWebProcessCounterType { };
typedef RefCounter<BackgroundWebProcessCounterType> BackgroundWebProcessCounter;
typedef BackgroundWebProcessCounter::Token BackgroundWebProcessToken;
#endif

class WebProcessProxy : public ChildProcessProxy, public ResponsivenessTimer::Client, public ThreadSafeRefCounted<WebProcessProxy>, public CanMakeWeakPtr<WebProcessProxy>, private ProcessThrottlerClient {
public:
    typedef HashMap<uint64_t, RefPtr<WebFrameProxy>> WebFrameProxyMap;
    typedef HashMap<uint64_t, WebPageProxy*> WebPageProxyMap;
    typedef HashMap<uint64_t, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    enum class IsPrewarmed {
        No,
        Yes
    };

    static Ref<WebProcessProxy> create(WebProcessPool&, WebsiteDataStore&, IsPrewarmed);
    ~WebProcessProxy();

    WebConnection* webConnection() const { return m_webConnection.get(); }

    WebProcessPool& processPool() const { ASSERT(m_processPool); return *m_processPool.get(); }

    // FIXME: WebsiteDataStores should be made per-WebPageProxy throughout WebKit2
    WebsiteDataStore& websiteDataStore() const { return m_websiteDataStore.get(); }

    static WebProcessProxy* processForIdentifier(WebCore::ProcessIdentifier);
    static WebPageProxy* webPage(uint64_t pageID);
    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);

    enum class BeginsUsingDataStore : bool { No, Yes };
    void addExistingWebPage(WebPageProxy&, uint64_t pageID, BeginsUsingDataStore);

    enum class EndsUsingDataStore : bool { No, Yes };
    void removeWebPage(WebPageProxy&, uint64_t pageID, EndsUsingDataStore);

    void addProvisionalPageProxy(ProvisionalPageProxy& provisionalPage) { ASSERT(!m_provisionalPages.contains(&provisionalPage)); m_provisionalPages.add(&provisionalPage); }
    void removeProvisionalPageProxy(ProvisionalPageProxy& provisionalPage) { ASSERT(m_provisionalPages.contains(&provisionalPage)); m_provisionalPages.remove(&provisionalPage); }

    typename WebPageProxyMap::ValuesConstIteratorRange pages() const { return m_pageMap.values(); }
    unsigned pageCount() const { return m_pageMap.size(); }
    unsigned visiblePageCount() const { return m_visiblePageCounter.value(); }

    void activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&&); // This is what is reported to ActivityMonitor.

    virtual bool isServiceWorkerProcess() const { return false; }

    void addVisitedLinkStore(VisitedLinkStore&);
    void addWebUserContentControllerProxy(WebUserContentControllerProxy&, WebPageCreationParameters&);
    void didDestroyVisitedLinkStore(VisitedLinkStore&);
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);

    RefPtr<API::UserInitiatedAction> userInitiatedActivity(uint64_t);

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }
    bool isResponsive() const;

    WebFrameProxy* webFrame(uint64_t) const;
    bool canCreateFrame(uint64_t frameID) const;
    void frameCreated(uint64_t, WebFrameProxy&);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

    VisibleWebPageToken visiblePageToken() const;

    void updateTextCheckerState();

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(WebPageProxy&, const String&);
    bool hasAssumedReadAccessToURL(const URL&) const;

    bool checkURLReceivedFromWebProcess(const String&);
    bool checkURLReceivedFromWebProcess(const URL&);

    static bool fullKeyboardAccessEnabled();

    void didSaveToPageCache();
    void releasePageCache();

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&&);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    static void notifyPageStatisticsAndDataRecordsProcessed();
    static void notifyPageStatisticsTelemetryFinished(API::Object* messageBody);

    static void notifyWebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished();
    static void notifyWebsiteDataScanForTopPrivatelyControlledDomainsFinished();
#endif

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination(ProcessTerminationReason);

    void stopResponsivenessTimer();

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

    void windowServerConnectionStateChanged();

    void processReadyToSuspend();
    void didCancelProcessSuspension();

    void setIsHoldingLockedFiles(bool);

    ProcessThrottler& throttler() { return m_throttler; }

    void isResponsive(WTF::Function<void(bool isWebProcessResponsive)>&&);
    void didReceiveMainThreadPing();
    void didReceiveBackgroundResponsivenessPing();

    void memoryPressureStatusChanged(bool isUnderMemoryPressure) { m_isUnderMemoryPressure = isUnderMemoryPressure; }
    bool isUnderMemoryPressure() const { return m_isUnderMemoryPressure; }
    void didExceedInactiveMemoryLimitWhileActive();

    void processTerminated();

    void didExceedCPULimit();
    void didExceedActiveMemoryLimit();
    void didExceedInactiveMemoryLimit();

    void checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier&, CompletionHandler<void(WebCore::MessagePortChannelProvider::HasActivity)>&&);

    void didCommitProvisionalLoad() { m_hasCommittedAnyProvisionalLoads = true; }
    bool hasCommittedAnyProvisionalLoads() const { return m_hasCommittedAnyProvisionalLoads; }

#if PLATFORM(WATCHOS)
    void takeBackgroundActivityTokenForFullscreenInput();
    void releaseBackgroundActivityTokenForFullscreenInput();
#endif

    bool isPrewarmed() const { return m_isPrewarmed; }
    void markIsNoLongerInPrewarmedPool();

#if PLATFORM(COCOA)
    Vector<String> mediaMIMETypes();
    void cacheMediaMIMETypes(const Vector<String>&);
#endif

#if PLATFORM(MAC)
    void requestHighPerformanceGPU();
    void releaseHighPerformanceGPU();
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    void startDisplayLink(unsigned observerID, uint32_t displayID);
    void stopDisplayLink(unsigned observerID, uint32_t displayID);
#endif

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void shutDown();
    void maybeShutDown();

protected:
    static uint64_t generatePageID();
    WebProcessProxy(WebProcessPool&, WebsiteDataStore&, IsPrewarmed);

    // ChildProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

#if PLATFORM(COCOA)
    void cacheMediaMIMETypesInternal(const Vector<String>&);
#endif

    bool isJITEnabled() const final;

private:
    // IPC message handlers.
    void updateBackForwardItem(const BackForwardListItemState&);
    void didDestroyFrame(uint64_t);
    void didDestroyUserGestureToken(uint64_t);

    void shouldTerminate(bool& shouldTerminate);

    void createNewMessagePortChannel(const WebCore::MessagePortIdentifier& port1, const WebCore::MessagePortIdentifier& port2);
    void entangleLocalPortInThisProcessToRemote(const WebCore::MessagePortIdentifier& local, const WebCore::MessagePortIdentifier& remote);
    void messagePortDisentangled(const WebCore::MessagePortIdentifier&);
    void messagePortClosed(const WebCore::MessagePortIdentifier&);
    void takeAllMessagesForPort(const WebCore::MessagePortIdentifier&, uint64_t messagesCallbackIdentifier);
    void postMessageToRemote(WebCore::MessageWithMessagePorts&&, const WebCore::MessagePortIdentifier&);
    void checkRemotePortForActivity(const WebCore::MessagePortIdentifier, uint64_t callbackIdentifier);
    void didDeliverMessagePortMessages(uint64_t messageBatchIdentifier);
    void didCheckProcessLocalPortForActivity(uint64_t callbackIdentifier, bool isLocallyReachable);

    // Plugins
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>& plugins, Vector<WebCore::PluginInfo>& applicationPlugins, Optional<Vector<WebCore::SupportedPluginIdentifier>>&);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPluginProcessConnection(uint64_t pluginProcessToken, Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply&&);
#endif
    void getNetworkProcessConnection(Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&&);

    bool platformIsBeingDebugged() const;
    bool shouldAllowNonValidInjectedCode() const;

    static const HashSet<String>& platformPathsWithAssumedReadAccess();

    void updateBackgroundResponsivenessTimer();

    void processDidTerminateOrFailedToLaunch();

    // IPC::Connection::Client
    friend class WebConnectionToWebProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override;
    void willChangeIsResponsive() override;
    void didChangeIsResponsive() override;
    bool mayBecomeUnresponsive() override;

    // ProcessThrottlerClient
    void sendProcessWillSuspendImminently() override;
    void sendPrepareToSuspend() override;
    void sendCancelPrepareToSuspend() override;
    void sendProcessDidResume() override;
    void didSetAssertionState(AssertionState) override;

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool canTerminateChildProcess();

    void didCollectPrewarmInformation(const String& domain, const WebCore::PrewarmInformation&);

    void logDiagnosticMessageForResourceLimitTermination(const String& limitKey);

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
    WeakOrStrongPtr<WebProcessPool> m_processPool; // Pre-warmed processes do not hold a strong reference to their pool.

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    WebPageProxyMap m_pageMap;
    WebFrameProxyMap m_frameMap;
    HashSet<ProvisionalPageProxy*> m_provisionalPages;
    UserInitiatedActionMap m_userInitiatedActionMap;

    HashSet<VisitedLinkStore*> m_visitedLinkStores;
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;

    int m_numberOfTimesSuddenTerminationWasDisabled;
    ProcessThrottler m_throttler;
    ProcessThrottler::BackgroundActivityToken m_tokenForHoldingLockedFiles;
#if PLATFORM(IOS_FAMILY)
    ForegroundWebProcessToken m_foregroundToken;
    BackgroundWebProcessToken m_backgroundToken;
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<WTF::Function<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;

    VisibleWebPageCounter m_visiblePageCounter;

    // FIXME: WebsiteDataStores should be made per-WebPageProxy throughout WebKit2. Get rid of this member.
    Ref<WebsiteDataStore> m_websiteDataStore;

    bool m_isUnderMemoryPressure { false };

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
#endif

    HashSet<WebCore::MessagePortIdentifier> m_processEntangledPorts;
    HashMap<uint64_t, Function<void()>> m_messageBatchDeliveryCompletionHandlers;
    HashMap<uint64_t, CompletionHandler<void(WebCore::MessagePortChannelProvider::HasActivity)>> m_localPortActivityCompletionHandlers;

    bool m_hasCommittedAnyProvisionalLoads { false };
    bool m_isPrewarmed;

#if PLATFORM(WATCHOS)
    ProcessThrottler::BackgroundActivityToken m_backgroundActivityTokenForFullscreenFormControls;
#endif

#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    Vector<std::unique_ptr<DisplayLink>> m_displayLinks;
#endif
};

} // namespace WebKit
