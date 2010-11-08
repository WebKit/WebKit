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

#include "WebProcessProxy.h"

#include "PluginInfoStore.h"
#include "PluginProcessManager.h"
#include "WebBackForwardListItem.h"
#include "WebContext.h"
#include "WebNavigationDataStore.h"
#include "WebPageNamespace.h"
#include "WebPageProxy.h"
#include "WebProcessManager.h"
#include "WebProcessMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebProcessProxyMessageKinds.h"
#include <WebCore/KURL.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generatePageID()
{
    static uint64_t uniquePageID = 1;
    return uniquePageID++;
}

PassRefPtr<WebProcessProxy> WebProcessProxy::create(WebContext* context)
{
    return adoptRef(new WebProcessProxy(context));
}

WebProcessProxy::WebProcessProxy(WebContext* context)
    : m_responsivenessTimer(this)
    , m_context(context)
{
    connect();
}

WebProcessProxy::~WebProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();
    
    for (size_t i = 0; i < m_pendingMessages.size(); ++i)
        m_pendingMessages[i].releaseArguments();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = 0;
    }

    if (m_threadLauncher) {
        m_threadLauncher->invalidate();
        m_threadLauncher = 0;
    }
}

void WebProcessProxy::connect()
{
    if (m_context->processModel() == ProcessModelSharedSecondaryThread) {
        ASSERT(!m_threadLauncher);
        m_threadLauncher = ThreadLauncher::create(this);
    } else {
        ASSERT(!m_processLauncher);

        ProcessLauncher::LaunchOptions launchOptions;
        launchOptions.processType = ProcessLauncher::WebProcess;
#if PLATFORM(MAC)
        // We want the web process to match the architecture of the UI process.
        launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
#endif
        m_processLauncher = ProcessLauncher::create(this, launchOptions);
    }
}

bool WebProcessProxy::sendMessage(CoreIPC::MessageID messageID, PassOwnPtr<CoreIPC::ArgumentEncoder> arguments)
{
    // If we're waiting for the web process to launch, we need to stash away the messages so we can send them once we have
    // a CoreIPC connection.
    if (isLaunching()) {
        m_pendingMessages.append(CoreIPC::Connection::OutgoingMessage(messageID, arguments));
        return true;
    }

    // If the web process has exited, m_connection will be null here.
    if (!m_connection)
        return false;

    return m_connection->sendMessage(messageID, arguments);
}

bool WebProcessProxy::isLaunching() const
{
    if (m_processLauncher)
        return m_processLauncher->isLaunching();
    if (m_threadLauncher)
        return m_threadLauncher->isLaunching();

    return false;
}

void WebProcessProxy::terminate()
{
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID).get();
}

WebPageProxy* WebProcessProxy::createWebPage(WebPageNamespace* pageNamespace)
{
    ASSERT(pageNamespace->process() == this);

    unsigned pageID = generatePageID();
    RefPtr<WebPageProxy> webPage = WebPageProxy::create(pageNamespace, pageID);
    m_pageMap.set(pageID, webPage);
    return webPage.get();
}

void WebProcessProxy::addExistingWebPage(WebPageProxy* webPage, uint64_t pageID)
{
    m_pageMap.set(pageID, webPage);
}

void WebProcessProxy::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);
}

WebProcessProxy::pages_const_iterator WebProcessProxy::pages_begin()
{
    return m_pageMap.begin().values();
}

WebProcessProxy::pages_const_iterator WebProcessProxy::pages_end()
{
    return m_pageMap.end().values();
}

size_t WebProcessProxy::numberOfPages()
{
    return m_pageMap.size();
}

WebBackForwardListItem* WebProcessProxy::webBackForwardItem(uint64_t itemID) const
{
    return m_backForwardListItemMap.get(itemID).get();
}

void WebProcessProxy::getPlugins(bool refresh, Vector<PluginInfo>& plugins)
{
    if (refresh)
        m_context->pluginInfoStore()->refresh();
    m_context->pluginInfoStore()->getPlugins(plugins);
}

#if ENABLE(PLUGIN_PROCESS)
void WebProcessProxy::getPluginProcessConnection(const String& pluginPath, CoreIPC::ArgumentEncoder* reply)
{
    PluginProcessManager::shared().getPluginProcessConnection(pluginPath, this, reply);
}
#endif

void WebProcessProxy::getPluginPath(const String& mimeType, const String& urlString, String& pluginPath)
{
    String newMimeType = mimeType.lower();

    PluginInfoStore::Plugin plugin = m_context->pluginInfoStore()->findPlugin(newMimeType, KURL(ParsedURLString, urlString));
    if (!plugin.path)
        return;

    pluginPath = plugin.path;
}

void WebProcessProxy::addBackForwardItem(uint64_t itemID, const String& originalURL, const String& url, const String& title)
{
    std::pair<WebBackForwardListItemMap::iterator, bool> result = m_backForwardListItemMap.add(itemID, 0);
    if (result.second) {
        // New item.
        result.first->second = WebBackForwardListItem::create(originalURL, url, title, itemID);
        return;
    }

    // Update existing item.
    result.first->second->setOriginalURL(originalURL);
    result.first->second->setURL(url);
    result.first->second->setTitle(title);
}

void WebProcessProxy::addVisitedLink(LinkHash linkHash)
{
    m_context->addVisitedLink(linkHash);
}

void WebProcessProxy::didNavigateWithNavigationData(uint64_t pageID, const WebNavigationDataStore& store, uint64_t frameID) 
{
    m_context->didNavigateWithNavigationData(webFrame(frameID), store);
}

void WebProcessProxy::didPerformClientRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    m_context->didPerformClientRedirect(webFrame(frameID), sourceURLString, destinationURLString);
}

void WebProcessProxy::didPerformServerRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    m_context->didPerformClientRedirect(webFrame(frameID), sourceURLString, destinationURLString);
}

void WebProcessProxy::didUpdateHistoryTitle(uint64_t pageID, const String& title, const String& url, uint64_t frameID)
{
    m_context->didUpdateHistoryTitle(webFrame(frameID), title, url);
}

void WebProcessProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        didReceiveWebProcessProxyMessage(connection, messageID, arguments);
        return;
    }
    if (messageID.is<CoreIPC::MessageClassWebContext>() || messageID.is<CoreIPC::MessageClassDownloadProxy>()) {
        m_context->didReceiveMessage(connection, messageID, arguments);    
        return;
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;

    WebPageProxy* pageProxy = webPage(pageID);
    if (!pageProxy)
        return;
    
    pageProxy->didReceiveMessage(connection, messageID, arguments);    
}

CoreIPC::SyncReplyMode WebProcessProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        didReceiveSyncWebProcessProxyMessage(connection, messageID, arguments, reply);
        return CoreIPC::AutomaticReply;
    }

#if ENABLE(PLUGIN_PROCESS)
    if (messageID.is<CoreIPC::MessageClassWebProcessProxyLegacy>()) {
        switch (messageID.get<WebProcessProxyLegacyMessage::Kind>()) {
            case WebProcessProxyLegacyMessage::GetPluginProcessConnection: {
                String pluginPath;
                
                if (!arguments->decode(CoreIPC::Out(pluginPath)))
                    return CoreIPC::AutomaticReply;

                getPluginProcessConnection(pluginPath, reply);
                return CoreIPC::ManualReply;
            }
        }
    }
#endif

    if (messageID.is<CoreIPC::MessageClassWebContext>()) {
        m_context->didReceiveSyncMessage(connection, messageID, arguments, reply);    
        return CoreIPC::AutomaticReply;
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return CoreIPC::AutomaticReply;
    
    WebPageProxy* pageProxy = webPage(pageID);
    if (!pageProxy)
        return CoreIPC::AutomaticReply;
    
    pageProxy->didReceiveSyncMessage(connection, messageID, arguments, reply);
    return CoreIPC::AutomaticReply;
}

void WebProcessProxy::didClose(CoreIPC::Connection*)
{
    // Protect ourselves, as the call to the shared WebProcessManager's processDidClose()
    // below may otherwise cause us to be deleted before we can finish our work.
    RefPtr<WebProcessProxy> protect(this);
    
    m_connection = nullptr;
    m_responsivenessTimer.stop();

    Vector<RefPtr<WebFrameProxy> > frames;
    copyValuesToVector(m_frameMap, frames);

    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->disconnect();
    m_frameMap.clear();

    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);

    m_context->processDidClose(this);

    WebProcessManager::shared().processDidClose(this, m_context);

    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidCrash();

}

void WebProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID)
{
    // We received an invalid message from the web process, invalidate our connection and kill it.
    m_connection->invalidate();

    terminate();
}

void WebProcessProxy::didBecomeUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeUnresponsive();
}

void WebProcessProxy::didBecomeResponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeResponsive();
}

void WebProcessProxy::didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier connectionIdentifier)
{
    didFinishLaunching(connectionIdentifier);
}

void WebProcessProxy::didFinishLaunching(ThreadLauncher*, CoreIPC::Connection::Identifier connectionIdentifier)
{
    didFinishLaunching(connectionIdentifier);
}

void WebProcessProxy::didFinishLaunching(CoreIPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);
    
    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->open();
    
    for (size_t i = 0; i < m_pendingMessages.size(); ++i) {
        CoreIPC::Connection::OutgoingMessage& outgoingMessage = m_pendingMessages[i];
        m_connection->sendMessage(outgoingMessage.messageID(), adoptPtr(outgoingMessage.arguments()));
    }

    m_pendingMessages.clear();

    // Tell the context that we finished launching.
    m_context->processDidFinishLaunching(this);
}

WebFrameProxy* WebProcessProxy::webFrame(uint64_t frameID) const
{
    return m_frameMap.get(frameID).get();
}

void WebProcessProxy::frameCreated(uint64_t frameID, WebFrameProxy* frameProxy)
{
    ASSERT(!m_frameMap.contains(frameID));
    m_frameMap.set(frameID, frameProxy);
}

void WebProcessProxy::didDestroyFrame(uint64_t frameID)
{
    // If the page is closed before it has had the chance to send the DidCreateMainFrame message
    // back to the UIProcess, then the frameDestroyed message will still be received because it
    // gets sent directly to the WebProcessProxy.
    m_frameMap.remove(frameID);
}

void WebProcessProxy::disconnectFramesFromPage(WebPageProxy* page)
{
    Vector<RefPtr<WebFrameProxy> > frames;
    copyValuesToVector(m_frameMap, frames);
    for (size_t i = 0, size = frames.size(); i < size; ++i) {
        if (frames[i]->page() == page)
            frames[i]->disconnect();
    }
}

size_t WebProcessProxy::frameCountInPage(WebPageProxy* page) const
{
    size_t result = 0;
    for (HashMap<uint64_t, RefPtr<WebFrameProxy> >::const_iterator iter = m_frameMap.begin(); iter != m_frameMap.end(); ++iter) {
        if (iter->second->page() == page)
            ++result;
    }
    return result;
}

} // namespace WebKit
