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

#include "PlatformProcessIdentifier.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessModel.h"
#include "ResponsivenessTimer.h"
#include "ThreadLauncher.h"
#include "WebConnectionToWebProcess.h"
#include "WebPageProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    class KURL;
};

namespace WebKit {

#if PLATFORM(MAC)
class SecItemRequestData;
class SecItemResponseData;
#endif

class WebBackForwardListItem;
class WebContext;
class WebPageGroup;
struct WebNavigationDataStore;

class WebProcessProxy : public RefCounted<WebProcessProxy>, CoreIPC::Connection::Client, ResponsivenessTimer::Client, ProcessLauncher::Client, ThreadLauncher::Client {
public:
    typedef HashMap<uint64_t, RefPtr<WebFrameProxy> > WebFrameProxyMap;
    typedef HashMap<uint64_t, RefPtr<WebBackForwardListItem> > WebBackForwardListItemMap;

    static PassRefPtr<WebProcessProxy> create(PassRefPtr<WebContext>);
    ~WebProcessProxy();

    void terminate();

    template<typename T> bool send(const T& message, uint64_t destinationID, unsigned messageSendFlags = 0);
    template<typename U> bool sendSync(const U& message, const typename U::Reply& reply, uint64_t destinationID, double timeout = 1);
    
    CoreIPC::Connection* connection() const
    { 
        ASSERT(m_connection);
        
        return m_connection->connection(); 
    }
    WebConnection* webConnection() const { return m_connection.get(); }

    WebContext* context() const { return m_context.get(); }

    PlatformProcessIdentifier processIdentifier() const { return m_processLauncher->processIdentifier(); }

    WebPageProxy* webPage(uint64_t pageID) const;
    PassRefPtr<WebPageProxy> createWebPage(PageClient*, WebContext*, WebPageGroup*);
    void addExistingWebPage(WebPageProxy*, uint64_t pageID);
    void removeWebPage(uint64_t pageID);

    WebBackForwardListItem* webBackForwardItem(uint64_t itemID) const;

    ResponsivenessTimer* responsivenessTimer() { return &m_responsivenessTimer; }

    bool isValid() const { return m_connection; }
    bool isLaunching() const;
    bool canSendMessage() const { return isValid() || isLaunching(); }

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
    bool checkURLReceivedFromWebProcess(const WebCore::KURL&);

    static bool fullKeyboardAccessEnabled();

    // FIXME: This variant of send is deprecated. All clients should move to an overload that take a message type.
    template<typename E, typename T> bool deprecatedSend(E messageID, uint64_t destinationID, const T& arguments);

private:
    explicit WebProcessProxy(PassRefPtr<WebContext>);

    // Initializes the process or thread launcher which will begin launching the process.
    void connect();

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void disconnect();

    bool sendMessage(CoreIPC::MessageID, PassOwnPtr<CoreIPC::ArgumentEncoder>, unsigned messageSendFlags);

    // CoreIPC message handlers.
    void addBackForwardItem(uint64_t itemID, const String& originalURLString, const String& urlString, const String& title, const CoreIPC::DataReference& backForwardData);
    void didDestroyFrame(uint64_t);
    
    void shouldTerminate(bool& shouldTerminate);

#if ENABLE(PLUGIN_PROCESS)
    void getPluginProcessConnection(const String& pluginPath, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>);
    void pluginSyncMessageSendTimedOut(const String& pluginPath);
#endif
#if PLATFORM(MAC)
    void secItemCopyMatching(const SecItemRequestData&, SecItemResponseData&);
    void secItemAdd(const SecItemRequestData&, SecItemResponseData&);
    void secItemUpdate(const SecItemRequestData&, SecItemResponseData&);
    void secItemDelete(const SecItemRequestData&, SecItemResponseData&);
    void secKeychainItemCopyContent(const SecKeychainItemRequestData&, SecKeychainItemResponseData&);
    void secKeychainItemCreateFromContent(const SecKeychainItemRequestData&, SecKeychainItemResponseData&);
    void secKeychainItemModifyContent(const SecKeychainItemRequestData&, SecKeychainItemResponseData&);
#endif

    // CoreIPC::Connection::Client
    friend class WebConnectionToWebProcess;
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);
    virtual void syncMessageSendTimedOut(CoreIPC::Connection*);
#if PLATFORM(WIN)
    virtual Vector<HWND> windowsToReceiveSentMessagesWhileWaitingForSyncReply();
#endif

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive(ResponsivenessTimer*);
    void didBecomeResponsive(ResponsivenessTimer*);

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier);

    // ThreadLauncher::Client
    virtual void didFinishLaunching(ThreadLauncher*, CoreIPC::Connection::Identifier);

    void didFinishLaunching(CoreIPC::Connection::Identifier);

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncWebProcessProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply);

    ResponsivenessTimer m_responsivenessTimer;
    
    // This is not a CoreIPC::Connection so that we can wrap the CoreIPC::Connection in
    // an API object.
    RefPtr<WebConnectionToWebProcess> m_connection;

    Vector<std::pair<CoreIPC::Connection::OutgoingMessage, unsigned> > m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<ThreadLauncher> m_threadLauncher;

    RefPtr<WebContext> m_context;

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    HashMap<uint64_t, WebPageProxy*> m_pageMap;
    WebFrameProxyMap m_frameMap;
    WebBackForwardListItemMap m_backForwardListItemMap;
};

template<typename E, typename T>
bool WebProcessProxy::deprecatedSend(E messageID, uint64_t destinationID, const T& arguments)
{
    OwnPtr<CoreIPC::ArgumentEncoder> argumentEncoder = CoreIPC::ArgumentEncoder::create(destinationID);
    argumentEncoder->encode(arguments);

    return sendMessage(CoreIPC::MessageID(messageID), argumentEncoder.release(), 0);
}

template<typename T>
bool WebProcessProxy::send(const T& message, uint64_t destinationID, unsigned messageSendFlags)
{
    OwnPtr<CoreIPC::ArgumentEncoder> argumentEncoder = CoreIPC::ArgumentEncoder::create(destinationID);
    argumentEncoder->encode(message);

    return sendMessage(CoreIPC::MessageID(T::messageID), argumentEncoder.release(), messageSendFlags);
}

template<typename U> 
bool WebProcessProxy::sendSync(const U& message, const typename U::Reply& reply, uint64_t destinationID, double timeout)
{
    if (!m_connection)
        return false;

    return connection()->sendSync(message, reply, destinationID, timeout);
}
    
} // namespace WebKit

#endif // WebProcessProxy_h
