/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WebProcessProxy_h
#define WebProcessProxy_h

#include "APISession.h"
#include "ChildProcessProxy.h"
#include "MessageReceiverMap.h"
#include "PlatformProcessIdentifier.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ResponsivenessTimer.h"
#include "WebConnectionToWebProcess.h"
#include "WebPageProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if ENABLE(CUSTOM_PROTOCOLS)
#include "CustomProtocolManagerProxy.h"
#endif

namespace WebCore {
class URL;
struct PluginInfo;
};

namespace WebKit {

class DownloadProxyMap;
class WebBackForwardListItem;
class WebContext;
class WebPageGroup;
struct WebNavigationDataStore;

class WebProcessProxy : public ChildProcessProxy, ResponsivenessTimer::Client {
public:
    typedef HashMap<uint64_t, RefPtr<WebBackForwardListItem>> WebBackForwardListItemMap;
    typedef HashMap<uint64_t, RefPtr<WebFrameProxy>> WebFrameProxyMap;
    typedef HashMap<uint64_t, WebPageProxy*> WebPageProxyMap;

    static PassRefPtr<WebProcessProxy> create(WebContext&);
    ~WebProcessProxy();

    static WebProcessProxy* fromConnection(IPC::Connection* connection)
    {
        return static_cast<WebProcessProxy*>(ChildProcessProxy::fromConnection(connection));
    }

    WebConnection* webConnection() const { return m_webConnection.get(); }

    WebContext& context() { return m_context.get(); }

    static WebPageProxy* webPage(uint64_t pageID);
    PassRefPtr<WebPageProxy> createWebPage(PageClient&, const WebPageConfiguration&);
    void addExistingWebPage(WebPageProxy*, uint64_t pageID);
    void removeWebPage(uint64_t pageID);
    Vector<WebPageProxy*> pages() const;

    WebBackForwardListItem* webBackForwardItem(uint64_t itemID) const;

    ResponsivenessTimer* responsivenessTimer() { return &m_responsivenessTimer; }

    WebFrameProxy* webFrame(uint64_t) const;
    bool canCreateFrame(uint64_t frameID) const;
    void frameCreated(uint64_t, WebFrameProxy*);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

    void updateTextCheckerState();

    void registerNewWebBackForwardListItem(WebBackForwardListItem*);

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(const String&);

    bool checkURLReceivedFromWebProcess(const String&);
    bool checkURLReceivedFromWebProcess(const WebCore::URL&);

    static bool fullKeyboardAccessEnabled();

    DownloadProxy* createDownloadProxy();

    void pageSuppressibilityChanged(WebPageProxy*);
    void pagePreferencesChanged(WebPageProxy*);

    void didSaveToPageCache();
    void releasePageCache();

#if PLATFORM(COCOA)
    bool allPagesAreProcessSuppressible() const;
    void updateProcessSuppressionState();
#endif

    void enableSuddenTermination();
    void disableSuddenTermination();

    void requestTermination();

    RefPtr<API::Object> apiObjectByConvertingToHandles(API::Object*);

    void windowServerConnectionStateChanged();

private:
    explicit WebProcessProxy(WebContext&);

    // From ChildProcessProxy
    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&);
    virtual void connectionWillOpen(IPC::Connection*) override;
    virtual void connectionWillClose(IPC::Connection*) override;

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void disconnect();

    // IPC message handlers.
    void addBackForwardItem(uint64_t itemID, const String& originalURLString, const String& urlString, const String& title, const IPC::DataReference& backForwardData);
    void didDestroyFrame(uint64_t);
    
    void shouldTerminate(bool& shouldTerminate);

    // Plugins
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>& plugins, Vector<WebCore::PluginInfo>& applicationPlugins);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPluginProcessConnection(uint64_t pluginProcessToken, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>);
#endif
#if ENABLE(NETWORK_PROCESS)
    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);
#endif
#if ENABLE(DATABASE_PROCESS)
    void getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>);
#endif

    // IPC::Connection::Client
    friend class WebConnectionToWebProcess;
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive(ResponsivenessTimer*) override;
    void interactionOccurredWhileUnresponsive(ResponsivenessTimer*) override;
    void didBecomeResponsive(ResponsivenessTimer*) override;

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // History client
    void didNavigateWithNavigationData(uint64_t pageID, const WebNavigationDataStore&, uint64_t frameID);
    void didPerformClientRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID);
    void didPerformServerRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID);
    void didUpdateHistoryTitle(uint64_t pageID, const String& title, const String& url, uint64_t frameID);

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection*, IPC::MessageDecoder&);
    void didReceiveSyncWebProcessProxyMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

    bool canTerminateChildProcess();

    ResponsivenessTimer m_responsivenessTimer;
    
    RefPtr<WebConnectionToWebProcess> m_webConnection;
    Ref<WebContext> m_context;

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    WebPageProxyMap m_pageMap;
    WebFrameProxyMap m_frameMap;
    WebBackForwardListItemMap m_backForwardListItemMap;

    OwnPtr<DownloadProxyMap> m_downloadProxyMap;

#if ENABLE(CUSTOM_PROTOCOLS)
    CustomProtocolManagerProxy m_customProtocolManagerProxy;
#endif

#if PLATFORM(COCOA)
    HashSet<uint64_t> m_processSuppressiblePages;
    bool m_processSuppressionEnabled;
#endif

    int m_numberOfTimesSuddenTerminationWasDisabled;
};
    
} // namespace WebKit

#endif // WebProcessProxy_h
