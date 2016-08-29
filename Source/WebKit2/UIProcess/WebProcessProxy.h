/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "APISession.h"
#include "APIUserInitiatedAction.h"
#include "ChildProcessProxy.h"
#include "CustomProtocolManagerProxy.h"
#include "MessageReceiverMap.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessThrottlerClient.h"
#include "ResponsivenessTimer.h"
#include "WebConnectionToWebProcess.h"
#include "WebPageProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LinkHash.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(IOS)
#include "ProcessThrottler.h"
#endif

namespace WebCore {
class ResourceRequest;
class URL;
struct PluginInfo;
};

namespace WebKit {

class NetworkProcessProxy;
class WebBackForwardListItem;
class WebPageGroup;
class WebProcessPool;
enum class WebsiteDataType;
struct WebNavigationDataStore;
    
class WebProcessProxy : public ChildProcessProxy, ResponsivenessTimer::Client, private ProcessThrottlerClient {
public:
    typedef HashMap<uint64_t, RefPtr<WebBackForwardListItem>> WebBackForwardListItemMap;
    typedef HashMap<uint64_t, RefPtr<WebFrameProxy>> WebFrameProxyMap;
    typedef HashMap<uint64_t, WebPageProxy*> WebPageProxyMap;
    typedef HashMap<uint64_t, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    static Ref<WebProcessProxy> create(WebProcessPool&);
    ~WebProcessProxy();

    WebConnection* webConnection() const { return m_webConnection.get(); }

    WebProcessPool& processPool() { return m_processPool; }

    static WebPageProxy* webPage(uint64_t pageID);
    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);
    void addExistingWebPage(WebPageProxy*, uint64_t pageID);
    void removeWebPage(uint64_t pageID);

    WTF::IteratorRange<WebPageProxyMap::const_iterator::Values> pages() const { return m_pageMap.values(); }
    unsigned pageCount() const { return m_pageMap.size(); }

    void addVisitedLinkStore(VisitedLinkStore&);
    void addWebUserContentControllerProxy(WebUserContentControllerProxy&);
    void didDestroyVisitedLinkStore(VisitedLinkStore&);
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);

    WebBackForwardListItem* webBackForwardItem(uint64_t itemID) const;
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(uint64_t);

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }

    WebFrameProxy* webFrame(uint64_t) const;
    bool canCreateFrame(uint64_t frameID) const;
    void frameCreated(uint64_t, WebFrameProxy*);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

    void updateTextCheckerState();

    void registerNewWebBackForwardListItem(WebBackForwardListItem*);
    void removeBackForwardItem(uint64_t);

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(const String&);
    bool hasAssumedReadAccessToURL(const WebCore::URL&) const;

    bool checkURLReceivedFromWebProcess(const String&);
    bool checkURLReceivedFromWebProcess(const WebCore::URL&);

    static bool fullKeyboardAccessEnabled();

    void didSaveToPageCache();
    void releasePageCache();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, std::function<void (WebsiteData)> completionHandler);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType>, const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, std::function<void ()> completionHandler);

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination();

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

    void reinstateNetworkProcessAssertionState(NetworkProcessProxy&);

    void isResponsive(std::function<void(bool isWebProcessResponsive)>);
    void didReceiveMainThreadPing();

private:
    explicit WebProcessProxy(WebProcessPool&);

    // From ChildProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void shutDown();

    // IPC message handlers.
    void addBackForwardItem(uint64_t itemID, uint64_t pageID, const PageState&);
    void didDestroyFrame(uint64_t);
    void didDestroyUserGestureToken(uint64_t);

    void shouldTerminate(bool& shouldTerminate);

    void didFetchWebsiteData(uint64_t callbackID, const WebsiteData&);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForOrigins(uint64_t callbackID);

    // Plugins
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>& plugins, Vector<WebCore::PluginInfo>& applicationPlugins);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPluginProcessConnection(uint64_t pluginProcessToken, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>);
#endif
    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);
#if ENABLE(DATABASE_PROCESS)
    void getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>);
#endif

    void retainIconForPageURL(const String& pageURL);
    void releaseIconForPageURL(const String& pageURL);
    void releaseRemainingIconsForPageURLs();

    static const HashSet<String>& platformPathsWithAssumedReadAccess();

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

    // ProcessThrottlerClient
    void sendProcessWillSuspendImminently() override;
    void sendPrepareToSuspend() override;
    void sendCancelPrepareToSuspend() override;
    void sendProcessDidResume() override;
    void didSetAssertionState(AssertionState) override;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool canTerminateChildProcess();

    ResponsivenessTimer m_responsivenessTimer;
    
    RefPtr<WebConnectionToWebProcess> m_webConnection;
    Ref<WebProcessPool> m_processPool;

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    WebPageProxyMap m_pageMap;
    WebFrameProxyMap m_frameMap;
    WebBackForwardListItemMap m_backForwardListItemMap;
    UserInitiatedActionMap m_userInitiatedActionMap;

    HashSet<VisitedLinkStore*> m_visitedLinkStores;
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;

    CustomProtocolManagerProxy m_customProtocolManagerProxy;

    HashMap<uint64_t, std::function<void (WebsiteData)>> m_pendingFetchWebsiteDataCallbacks;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataCallbacks;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataForOriginsCallbacks;

    int m_numberOfTimesSuddenTerminationWasDisabled;
    ProcessThrottler m_throttler;
    ProcessThrottler::BackgroundActivityToken m_tokenForHoldingLockedFiles;
#if PLATFORM(IOS)
    ProcessThrottler::ForegroundActivityToken m_foregroundTokenForNetworkProcess;
    ProcessThrottler::BackgroundActivityToken m_backgroundTokenForNetworkProcess;
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<std::function<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;
};

} // namespace WebKit
