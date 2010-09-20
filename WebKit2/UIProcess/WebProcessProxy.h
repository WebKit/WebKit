/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "PlatformProcessIdentifier.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessModel.h"
#include "ResponsivenessTimer.h"
#include "ThreadLauncher.h"
#include "WebPageProxy.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    class KURL;
};

namespace WebKit {

class WebBackForwardListItem;
class WebContext;
class WebPageNamespace;
    
class WebProcessProxy : public RefCounted<WebProcessProxy>, CoreIPC::Connection::Client, ResponsivenessTimer::Client, ProcessLauncher::Client, ThreadLauncher::Client {
public:
    typedef HashMap<uint64_t, RefPtr<WebPageProxy> > WebPageProxyMap;
    typedef WebPageProxyMap::const_iterator::Values pages_const_iterator;
    typedef HashMap<uint64_t, RefPtr<WebBackForwardListItem> > WebBackForwardListItemMap;

    static PassRefPtr<WebProcessProxy> create(WebContext*);
    ~WebProcessProxy();

    void terminate();

    template<typename E, typename T> bool send(E messageID, uint64_t destinationID, const T& arguments);
    
    CoreIPC::Connection* connection() const
    { 
        ASSERT(m_connection);
        
        return m_connection.get(); 
    }

    PlatformProcessIdentifier processIdentifier() const { return m_processLauncher->processIdentifier(); }

    WebPageProxy* webPage(uint64_t pageID) const;
    WebPageProxy* createWebPage(WebPageNamespace*);
    void addExistingWebPage(WebPageProxy*, uint64_t pageID);
    void removeWebPage(uint64_t pageID);

    pages_const_iterator pages_begin();
    pages_const_iterator pages_end();
    size_t numberOfPages();

    WebBackForwardListItem* webBackForwardItem(uint64_t itemID) const;

    ResponsivenessTimer* responsivenessTimer() { return &m_responsivenessTimer; }

    bool isValid() const { return m_connection; }
    bool isLaunching() const;

    WebFrameProxy* webFrame(uint64_t) const;
    void frameCreated(uint64_t, WebFrameProxy*);
    void frameDestroyed(uint64_t);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

private:
    explicit WebProcessProxy(WebContext*);

    void connect();
#if USE(ACCELERATED_COMPOSITING)
    void setUpAcceleratedCompositing();
#endif

    bool sendMessage(CoreIPC::MessageID, PassOwnPtr<CoreIPC::ArgumentEncoder>);

    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>&);
    void getPluginHostConnection(const String& mimeType, const WebCore::KURL& url, String& pluginPath);

    void addOrUpdateBackForwardListItem(uint64_t itemID, const String& originalURLString, const String& urlString, const String& title);
    void addVisitedLink(WebCore::LinkHash);

    // CoreIPC::Connection::Client
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);
    void didClose(CoreIPC::Connection*);
    void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);
        
    // ResponsivenessTimer::Client
    void didBecomeUnresponsive(ResponsivenessTimer*);
    void didBecomeResponsive(ResponsivenessTimer*);

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier);

    // ThreadLauncher::Client
    virtual void didFinishLaunching(ThreadLauncher*, CoreIPC::Connection::Identifier);

    void didFinishLaunching(CoreIPC::Connection::Identifier);

    ResponsivenessTimer m_responsivenessTimer;
    RefPtr<CoreIPC::Connection> m_connection;

    Vector<CoreIPC::Connection::OutgoingMessage> m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<ThreadLauncher> m_threadLauncher;

    WebContext* m_context;

    // NOTE: This map is for WebPageProxies in all WebPageNamespaces that use this process.
    WebPageProxyMap m_pageMap;

    // NOTE: This map is for WebBackForwardListItems in all WebPageNamespaces and WebPageProxies that use this process.
    WebBackForwardListItemMap m_backForwardListItemMap;

    HashMap<uint64_t, RefPtr<WebFrameProxy> > m_frameMap;
};

template<typename E, typename T>
bool WebProcessProxy::send(E messageID, uint64_t destinationID, const T& arguments)
{
    OwnPtr<CoreIPC::ArgumentEncoder> argumentEncoder(new CoreIPC::ArgumentEncoder(destinationID));
    argumentEncoder->encode(arguments);

    return sendMessage(CoreIPC::MessageID(messageID), argumentEncoder.release());
}

} // namespace WebKit

#endif // WebProcessProxy_h
