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

#include "config.h"
#include "WebProcessProxy.h"

#include "DataReference.h"
#include "PluginInfoStore.h"
#include "PluginProcessManager.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "WebBackForwardListItem.h"
#include "WebContext.h"
#include "WebNavigationDataStore.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"
#include "WebPluginSiteDataManager.h"
#include "WebProcessMessages.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/KURL.h>
#include <stdio.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "BuiltInPDFView.h"
#endif

using namespace WebCore;
using namespace std;

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())

namespace WebKit {

template<typename HashMap>
static inline bool isGoodKey(const typename HashMap::KeyType& key)
{
    return key != HashTraits<typename HashMap::KeyType>::emptyValue() && !HashTraits<typename HashMap::KeyType>::isDeletedValue(key);
}

static uint64_t generatePageID()
{
    static uint64_t uniquePageID = 1;
    return uniquePageID++;
}

static WorkQueue& pluginWorkQueue()
{
    DEFINE_STATIC_LOCAL(WorkQueue, queue, ("com.apple.CoreIPC.PluginQueue"));
    return queue;
}

PassRefPtr<WebProcessProxy> WebProcessProxy::create(PassRefPtr<WebContext> context)
{
    return adoptRef(new WebProcessProxy(context));
}

WebProcessProxy::WebProcessProxy(PassRefPtr<WebContext> context)
    : m_responsivenessTimer(this)
    , m_context(context)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
{
    connect();
}

WebProcessProxy::~WebProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();
    
    for (size_t i = 0; i < m_pendingMessages.size(); ++i)
        m_pendingMessages[i].first.releaseArguments();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = 0;
    }
}

void WebProcessProxy::connect()
{
    ASSERT(!m_processLauncher);

    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::WebProcess;

#if PLATFORM(MAC)
    // We want the web process to match the architecture of the UI process.
    launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
    launchOptions.executableHeap = false;
#if HAVE(XPC)
    launchOptions.useXPC = getenv("WEBKIT_USE_XPC_SERVICE_FOR_WEB_PROCESS");
#endif
#endif
#ifndef NDEBUG
    const char* webProcessCmdPrefix = getenv("WEB_PROCESS_CMD_PREFIX");
    if (webProcessCmdPrefix && *webProcessCmdPrefix)
        launchOptions.processCmdPrefix = String::fromUTF8(webProcessCmdPrefix);
#endif

    m_processLauncher = ProcessLauncher::create(this, launchOptions);
}

void WebProcessProxy::disconnect()
{
    if (m_connection) {
        m_connection->connection()->removeQueueClient(this);
        m_connection->invalidate();
        m_connection = nullptr;
    }

    m_responsivenessTimer.stop();

    Vector<RefPtr<WebFrameProxy> > frames;
    copyValuesToVector(m_frameMap, frames);

    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->disconnect();
    m_frameMap.clear();

    m_context->disconnectProcess(this);
}

bool WebProcessProxy::sendMessage(CoreIPC::MessageID messageID, PassOwnPtr<CoreIPC::ArgumentEncoder> arguments, unsigned messageSendFlags)
{
    // If we're waiting for the web process to launch, we need to stash away the messages so we can send them once we have
    // a CoreIPC connection.
    if (isLaunching()) {
        m_pendingMessages.append(make_pair(CoreIPC::Connection::OutgoingMessage(messageID, arguments), messageSendFlags));
        return true;
    }

    // If the web process has exited, m_connection will be null here.
    if (!m_connection)
        return false;

    return connection()->sendMessage(messageID, arguments, messageSendFlags);
}

bool WebProcessProxy::isLaunching() const
{
    if (m_processLauncher)
        return m_processLauncher->isLaunching();

    return false;
}

void WebProcessProxy::terminate()
{
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID);
}

PassRefPtr<WebPageProxy> WebProcessProxy::createWebPage(PageClient* pageClient, WebContext* context, WebPageGroup* pageGroup)
{
    uint64_t pageID = generatePageID();
    RefPtr<WebPageProxy> webPage = WebPageProxy::create(pageClient, this, pageGroup, pageID);
    m_pageMap.set(pageID, webPage.get());
    return webPage.release();
}

void WebProcessProxy::addExistingWebPage(WebPageProxy* webPage, uint64_t pageID)
{
    m_pageMap.set(pageID, webPage);
}

void WebProcessProxy::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);
}

#if ENABLE(WEB_INTENTS)
void WebProcessProxy::removeMessagePortChannel(uint64_t channelID)
{
    if (!isValid())
        return;

    send(Messages::WebProcess::RemoveMessagePortChannel(channelID), /* destinationID */ 0);
}
#endif

WebBackForwardListItem* WebProcessProxy::webBackForwardItem(uint64_t itemID) const
{
    return m_backForwardListItemMap.get(itemID).get();
}

void WebProcessProxy::registerNewWebBackForwardListItem(WebBackForwardListItem* item)
{
    // This item was just created by the UIProcess and is being added to the map for the first time
    // so we should not already have an item for this ID.
    ASSERT(!m_backForwardListItemMap.contains(item->itemID()));

    m_backForwardListItemMap.set(item->itemID(), item);
}

void WebProcessProxy::assumeReadAccessToBaseURL(const String& urlString)
{
    KURL url(KURL(), urlString);
    if (!url.isLocalFile())
        return;

    // There's a chance that urlString does not point to a directory.
    // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
    KURL baseURL(KURL(), url.baseAsString());
    
    // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
    // to have read access to this directory already.
    m_localPathsWithAssumedReadAccess.add(baseURL.fileSystemPath());
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const String& urlString)
{
    return checkURLReceivedFromWebProcess(KURL(KURL(), urlString));
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const KURL& url)
{
    // FIXME: Consider checking that the URL is valid. Currently, WebProcess sends invalid URLs in many cases, but it probably doesn't have good reasons to do that.

    // Any other non-file URL is OK.
    if (!url.isLocalFile())
        return true;

    // Any file URL is also OK if we've loaded a file URL through API before, granting universal read access.
    if (m_mayHaveUniversalFileReadSandboxExtension)
        return true;

    // If we loaded a string with a file base URL before, loading resources from that subdirectory is fine.
    // There are no ".." components, because all URLs received from WebProcess are parsed with KURL, which removes those.
    String path = url.fileSystemPath();
    for (HashSet<String>::const_iterator iter = m_localPathsWithAssumedReadAccess.begin(); iter != m_localPathsWithAssumedReadAccess.end(); ++iter) {
        if (path.startsWith(*iter))
            return true;
    }

    // Items in back/forward list have been already checked.
    // One case where we don't have sandbox extensions for file URLs in b/f list is if the list has been reinstated after a crash or a browser restart.
    for (WebBackForwardListItemMap::iterator iter = m_backForwardListItemMap.begin(), end = m_backForwardListItemMap.end(); iter != end; ++iter) {
        if (KURL(KURL(), iter->second->url()).fileSystemPath() == path)
            return true;
        if (KURL(KURL(), iter->second->originalURL()).fileSystemPath() == path)
            return true;
    }

    // A Web process that was never asked to load a file URL should not ever ask us to do anything with a file URL.
    WTFLogAlways("Received an unexpected URL from the web process: '%s'\n", url.string().utf8().data());
    return false;
}

#if !PLATFORM(MAC)
bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return false;
}
#endif

void WebProcessProxy::addBackForwardItem(uint64_t itemID, const String& originalURL, const String& url, const String& title, const CoreIPC::DataReference& backForwardData)
{
    MESSAGE_CHECK_URL(originalURL);
    MESSAGE_CHECK_URL(url);

    WebBackForwardListItemMap::AddResult result = m_backForwardListItemMap.add(itemID, 0);
    if (result.isNewEntry) {
        result.iterator->second = WebBackForwardListItem::create(originalURL, url, title, backForwardData.data(), backForwardData.size(), itemID);
        return;
    }

    // Update existing item.
    result.iterator->second->setOriginalURL(originalURL);
    result.iterator->second->setURL(url);
    result.iterator->second->setTitle(title);
    result.iterator->second->setBackForwardData(backForwardData.data(), backForwardData.size());
}

void WebProcessProxy::sendDidGetPlugins(uint64_t requestID, PassOwnPtr<Vector<PluginInfo> > pluginInfos)
{
    ASSERT(isMainThread());

    OwnPtr<Vector<PluginInfo> > plugins(pluginInfos);

#if PLATFORM(MAC)
    // Add built-in PDF last, so that it's not used when a real plug-in is installed.
    // NOTE: This has to be done on the main thread as it calls localizedString().
    if (!m_context->omitPDFSupport())
        plugins->append(BuiltInPDFView::pluginInfo());
#endif

    send(Messages::WebProcess::DidGetPlugins(requestID, *plugins), 0);
}

void WebProcessProxy::handleGetPlugins(uint64_t requestID, bool refresh)
{
    if (refresh)
        m_context->pluginInfoStore().refresh();

    OwnPtr<Vector<PluginInfo> > pluginInfos = adoptPtr(new Vector<PluginInfo>);

    Vector<PluginModuleInfo> plugins = m_context->pluginInfoStore().plugins();
    for (size_t i = 0; i < plugins.size(); ++i)
        pluginInfos->append(plugins[i].info);

    // NOTE: We have to pass the PluginInfo vector to the secondary thread via a pointer as otherwise
    //       we'd end up with a deref() race on all the WTF::Strings it contains.
    RunLoop::main()->dispatch(bind(&WebProcessProxy::sendDidGetPlugins, this, requestID, pluginInfos.release()));
}

void WebProcessProxy::getPlugins(CoreIPC::Connection*, uint64_t requestID, bool refresh)
{
    pluginWorkQueue().dispatch(bind(&WebProcessProxy::handleGetPlugins, this, requestID, refresh));
}

void WebProcessProxy::getPluginPath(const String& mimeType, const String& urlString, String& pluginPath, bool& blocked)
{
    MESSAGE_CHECK_URL(urlString);

    String newMimeType = mimeType.lower();

    blocked = false;
    PluginModuleInfo plugin = m_context->pluginInfoStore().findPlugin(newMimeType, KURL(KURL(), urlString));
    if (!plugin.path)
        return;

    if (m_context->pluginInfoStore().shouldBlockPlugin(plugin)) {
        blocked = true;
        return;
    }

    pluginPath = plugin.path;
}

#if ENABLE(PLUGIN_PROCESS)

void WebProcessProxy::getPluginProcessConnection(const String& pluginPath, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply> reply)
{
    PluginProcessManager::shared().getPluginProcessConnection(m_context->pluginInfoStore(), pluginPath, reply);
}

void WebProcessProxy::pluginSyncMessageSendTimedOut(const String& pluginPath)
{
    PluginProcessManager::shared().pluginSyncMessageSendTimedOut(pluginPath);
}

#else

void WebProcessProxy::didGetSitesWithPluginData(const Vector<String>& sites, uint64_t callbackID)
{
    m_context->pluginSiteDataManager()->didGetSitesWithData(sites, callbackID);
}

void WebProcessProxy::didClearPluginSiteData(uint64_t callbackID)
{
    m_context->pluginSiteDataManager()->didClearSiteData(callbackID);
}

#endif

void WebProcessProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        didReceiveWebProcessProxyMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebContext>()
        || messageID.is<CoreIPC::MessageClassWebContextLegacy>()
        || messageID.is<CoreIPC::MessageClassDownloadProxy>()
        || messageID.is<CoreIPC::MessageClassWebApplicationCacheManagerProxy>()
#if ENABLE(BATTERY_STATUS)
        || messageID.is<CoreIPC::MessageClassWebBatteryManagerProxy>()
#endif
        || messageID.is<CoreIPC::MessageClassWebCookieManagerProxy>()
        || messageID.is<CoreIPC::MessageClassWebDatabaseManagerProxy>()
        || messageID.is<CoreIPC::MessageClassWebGeolocationManagerProxy>()
        || messageID.is<CoreIPC::MessageClassWebIconDatabase>()
        || messageID.is<CoreIPC::MessageClassWebKeyValueStorageManagerProxy>()
        || messageID.is<CoreIPC::MessageClassWebMediaCacheManagerProxy>()
#if ENABLE(NETWORK_INFO)
        || messageID.is<CoreIPC::MessageClassWebNetworkInfoManagerProxy>()
#endif
        || messageID.is<CoreIPC::MessageClassWebNotificationManagerProxy>()
#if USE(SOUP)
        || messageID.is<CoreIPC::MessageClassWebSoupRequestManagerProxy>()
#endif
#if ENABLE(VIBRATION)
        || messageID.is<CoreIPC::MessageClassWebVibrationProxy>()
#endif
        || messageID.is<CoreIPC::MessageClassWebResourceCacheManagerProxy>()) {
        m_context->didReceiveMessage(this, messageID, arguments);
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

void WebProcessProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        didReceiveSyncWebProcessProxyMessage(connection, messageID, arguments, reply);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebContext>() || messageID.is<CoreIPC::MessageClassWebContextLegacy>()
#if ENABLE(NETWORK_INFO)
        || messageID.is<CoreIPC::MessageClassWebNetworkInfoManagerProxy>()
#endif
        || messageID.is<CoreIPC::MessageClassDownloadProxy>() || messageID.is<CoreIPC::MessageClassWebIconDatabase>()) {
        m_context->didReceiveSyncMessage(this, messageID, arguments, reply);
        return;
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPageProxy* pageProxy = webPage(pageID);
    if (!pageProxy)
        return;
    
    pageProxy->didReceiveSyncMessage(connection, messageID, arguments, reply);
}

void WebProcessProxy::didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, bool& didHandleMessage)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>())
        didReceiveWebProcessProxyMessageOnConnectionWorkQueue(connection, messageID, arguments, didHandleMessage);
}

void WebProcessProxy::didClose(CoreIPC::Connection*)
{
    // Protect ourselves, as the call to disconnect() below may otherwise cause us
    // to be deleted before we can finish our work.
    RefPtr<WebProcessProxy> protect(this);

    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);

    disconnect();

    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidCrash();
}

void WebProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID)
{
    WTFLogAlways("Received an invalid message from the web process with message ID %x\n", messageID.toInt());

    // Terminate the WebProcesses.
    terminate();
}

void WebProcessProxy::syncMessageSendTimedOut(CoreIPC::Connection*)
{
}

void WebProcessProxy::didBecomeUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeUnresponsive();
}

void WebProcessProxy::interactionOccurredWhileUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->interactionOccurredWhileProcessUnresponsive();
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

void WebProcessProxy::didFinishLaunching(CoreIPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);
    
    m_connection = WebConnectionToWebProcess::create(this, connectionIdentifier, RunLoop::main());
    m_connection->connection()->addQueueClient(this);
    m_connection->connection()->open();

    for (size_t i = 0; i < m_pendingMessages.size(); ++i) {
        CoreIPC::Connection::OutgoingMessage& outgoingMessage = m_pendingMessages[i].first;
        unsigned messageSendFlags = m_pendingMessages[i].second;
        connection()->sendMessage(outgoingMessage.messageID(), adoptPtr(outgoingMessage.arguments()), messageSendFlags);
    }

    m_pendingMessages.clear();

    // Tell the context that we finished launching.
    m_context->processDidFinishLaunching(this);
}

WebFrameProxy* WebProcessProxy::webFrame(uint64_t frameID) const
{
    return isGoodKey<WebFrameProxyMap>(frameID) ? m_frameMap.get(frameID).get() : 0;
}

bool WebProcessProxy::canCreateFrame(uint64_t frameID) const
{
    return isGoodKey<WebFrameProxyMap>(frameID) && !m_frameMap.contains(frameID);
}

void WebProcessProxy::frameCreated(uint64_t frameID, WebFrameProxy* frameProxy)
{
    ASSERT(canCreateFrame(frameID));
    m_frameMap.set(frameID, frameProxy);
}

void WebProcessProxy::didDestroyFrame(uint64_t frameID)
{
    // If the page is closed before it has had the chance to send the DidCreateMainFrame message
    // back to the UIProcess, then the frameDestroyed message will still be received because it
    // gets sent directly to the WebProcessProxy.
    ASSERT(isGoodKey<WebFrameProxyMap>(frameID));
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

void WebProcessProxy::shouldTerminate(bool& shouldTerminate)
{
    if (!m_pageMap.isEmpty() || !m_context->shouldTerminate(this)) {
        shouldTerminate = false;
        return;
    }

    shouldTerminate = true;

    // We know that the web process is going to terminate so disconnect it from the context.
    disconnect();
}

void WebProcessProxy::updateTextCheckerState()
{
    if (!isValid())
        return;

    send(Messages::WebProcess::SetTextCheckerState(TextChecker::state()), 0);
}

void WebProcessProxy::didNavigateWithNavigationData(uint64_t pageID, const WebNavigationDataStore& store, uint64_t frameID) 
{
    WebPageProxy* page = webPage(pageID);
    if (!page)
        return;
    
    WebFrameProxy* frame = webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == page);
    
    m_context->historyClient().didNavigateWithNavigationData(m_context.get(), page, store, frame);
}

void WebProcessProxy::didPerformClientRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    WebPageProxy* page = webPage(pageID);
    if (!page)
        return;

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    WebFrameProxy* frame = webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == page);
    MESSAGE_CHECK_URL(sourceURLString);
    MESSAGE_CHECK_URL(destinationURLString);

    m_context->historyClient().didPerformClientRedirect(m_context.get(), page, sourceURLString, destinationURLString, frame);
}

void WebProcessProxy::didPerformServerRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    WebPageProxy* page = webPage(pageID);
    if (!page)
        return;
    
    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    WebFrameProxy* frame = webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == page);
    MESSAGE_CHECK_URL(sourceURLString);
    MESSAGE_CHECK_URL(destinationURLString);

    m_context->historyClient().didPerformServerRedirect(m_context.get(), page, sourceURLString, destinationURLString, frame);
}

void WebProcessProxy::didUpdateHistoryTitle(uint64_t pageID, const String& title, const String& url, uint64_t frameID)
{
    WebPageProxy* page = webPage(pageID);
    if (!page)
        return;

    WebFrameProxy* frame = webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == page);
    MESSAGE_CHECK_URL(url);

    m_context->historyClient().didUpdateHistoryTitle(m_context.get(), page, title, url, frame);
}

} // namespace WebKit
