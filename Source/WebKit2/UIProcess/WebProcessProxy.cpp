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

#include "APIFrameHandle.h"
#include "APIHistoryClient.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "DataReference.h"
#include "DownloadProxyMap.h"
#include "PluginInfoStore.h"
#include "PluginProcessManager.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include "WebBackForwardListItem.h"
#include "WebContext.h"
#include "WebNavigationDataStore.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPluginSiteDataManager.h"
#include "WebProcessMessages.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/SuddenTermination.h>
#include <WebCore/URL.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "PDFPlugin.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

using namespace WebCore;

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())

namespace WebKit {

static uint64_t generatePageID()
{
    static uint64_t uniquePageID;
    return ++uniquePageID;
}

static WebProcessProxy::WebPageProxyMap& globalPageMap()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<WebProcessProxy::WebPageProxyMap> pageMap;
    return pageMap;
}

PassRefPtr<WebProcessProxy> WebProcessProxy::create(WebContext& context)
{
    return adoptRef(new WebProcessProxy(context));
}

WebProcessProxy::WebProcessProxy(WebContext& context)
    : m_responsivenessTimer(this)
    , m_context(context)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
#if ENABLE(CUSTOM_PROTOCOLS)
    , m_customProtocolManagerProxy(this, context)
#endif
#if PLATFORM(COCOA)
    , m_processSuppressionEnabled(false)
#endif
    , m_numberOfTimesSuddenTerminationWasDisabled(0)
{
    connect();
}

WebProcessProxy::~WebProcessProxy()
{
    if (m_webConnection)
        m_webConnection->invalidate();

    while (m_numberOfTimesSuddenTerminationWasDisabled-- > 0)
        WebCore::enableSuddenTermination();
}

void WebProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::WebProcess;
    platformGetLaunchOptions(launchOptions);
}

void WebProcessProxy::connectionWillOpen(IPC::Connection* connection)
{
    ASSERT(this->connection() == connection);

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::shared().initializeConnection(connection);
#endif

    for (WebPageProxyMap::iterator it = m_pageMap.begin(), end = m_pageMap.end(); it != end; ++it)
        it->value->connectionWillOpen(connection);

    m_context->processWillOpenConnection(this);
}

void WebProcessProxy::connectionWillClose(IPC::Connection* connection)
{
    ASSERT(this->connection() == connection);

    for (WebPageProxyMap::iterator it = m_pageMap.begin(), end = m_pageMap.end(); it != end; ++it)
        it->value->connectionWillClose(connection);

    m_context->processWillCloseConnection(this);
}

void WebProcessProxy::disconnect()
{
    clearConnection();

    if (m_webConnection) {
        m_webConnection->invalidate();
        m_webConnection = nullptr;
    }

    m_responsivenessTimer.invalidate();

    Vector<RefPtr<WebFrameProxy>> frames;
    copyValuesToVector(m_frameMap, frames);

    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->disconnect();
    m_frameMap.clear();

    if (m_downloadProxyMap)
        m_downloadProxyMap->processDidClose();

    m_context->disconnectProcess(this);
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID)
{
    return globalPageMap().get(pageID);
}

PassRefPtr<WebPageProxy> WebProcessProxy::createWebPage(PageClient& pageClient, const WebPageConfiguration& configuration)
{
    uint64_t pageID = generatePageID();
    RefPtr<WebPageProxy> webPage = WebPageProxy::create(pageClient, *this, pageID, configuration);
    m_pageMap.set(pageID, webPage.get());
    globalPageMap().set(pageID, webPage.get());
#if PLATFORM(COCOA)
    if (webPage->isProcessSuppressible())
        m_processSuppressiblePages.add(pageID);
    updateProcessSuppressionState();
#endif
    return webPage.release();
}

void WebProcessProxy::addExistingWebPage(WebPageProxy* webPage, uint64_t pageID)
{
    m_pageMap.set(pageID, webPage);
    globalPageMap().set(pageID, webPage);
#if PLATFORM(COCOA)
    if (webPage->isProcessSuppressible())
        m_processSuppressiblePages.add(pageID);
    updateProcessSuppressionState();
#endif
}

void WebProcessProxy::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);
    globalPageMap().remove(pageID);
#if PLATFORM(COCOA)
    m_processSuppressiblePages.remove(pageID);
    updateProcessSuppressionState();
#endif

    // If this was the last WebPage open in that web process, and we have no other reason to keep it alive, let it go.
    // We only allow this when using a network process, as otherwise the WebProcess needs to preserve its session state.
    if (m_context->usesNetworkProcess() && canTerminateChildProcess()) {
        abortProcessLaunchIfNeeded();
        disconnect();
    }
}

Vector<WebPageProxy*> WebProcessProxy::pages() const
{
    Vector<WebPageProxy*> result;
    copyValuesToVector(m_pageMap, result);
    return result;
}

WebBackForwardListItem* WebProcessProxy::webBackForwardItem(uint64_t itemID) const
{
    return m_backForwardListItemMap.get(itemID);
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
    URL url(URL(), urlString);
    if (!url.isLocalFile())
        return;

    // There's a chance that urlString does not point to a directory.
    // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
    URL baseURL(URL(), url.baseAsString());
    
    // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
    // to have read access to this directory already.
    m_localPathsWithAssumedReadAccess.add(baseURL.fileSystemPath());
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const String& urlString)
{
    return checkURLReceivedFromWebProcess(URL(URL(), urlString));
}

bool WebProcessProxy::checkURLReceivedFromWebProcess(const URL& url)
{
    // FIXME: Consider checking that the URL is valid. Currently, WebProcess sends invalid URLs in many cases, but it probably doesn't have good reasons to do that.

    // Any other non-file URL is OK.
    if (!url.isLocalFile())
        return true;

    // Any file URL is also OK if we've loaded a file URL through API before, granting universal read access.
    if (m_mayHaveUniversalFileReadSandboxExtension)
        return true;

    // If we loaded a string with a file base URL before, loading resources from that subdirectory is fine.
    // There are no ".." components, because all URLs received from WebProcess are parsed with URL, which removes those.
    String path = url.fileSystemPath();
    for (HashSet<String>::const_iterator iter = m_localPathsWithAssumedReadAccess.begin(); iter != m_localPathsWithAssumedReadAccess.end(); ++iter) {
        if (path.startsWith(*iter))
            return true;
    }

    // Items in back/forward list have been already checked.
    // One case where we don't have sandbox extensions for file URLs in b/f list is if the list has been reinstated after a crash or a browser restart.
    for (WebBackForwardListItemMap::iterator iter = m_backForwardListItemMap.begin(), end = m_backForwardListItemMap.end(); iter != end; ++iter) {
        if (URL(URL(), iter->value->url()).fileSystemPath() == path)
            return true;
        if (URL(URL(), iter->value->originalURL()).fileSystemPath() == path)
            return true;
    }

    // A Web process that was never asked to load a file URL should not ever ask us to do anything with a file URL.
    WTFLogAlways("Received an unexpected URL from the web process: '%s'\n", url.string().utf8().data());
    return false;
}

#if !PLATFORM(COCOA)
bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return false;
}
#endif

void WebProcessProxy::addBackForwardItem(uint64_t itemID, const String& originalURL, const String& url, const String& title, const IPC::DataReference& backForwardData)
{
    MESSAGE_CHECK_URL(originalURL);
    MESSAGE_CHECK_URL(url);

    WebBackForwardListItemMap::AddResult result = m_backForwardListItemMap.add(itemID, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = WebBackForwardListItem::create(originalURL, url, title, backForwardData.data(), backForwardData.size(), itemID);
        return;
    }

    // Update existing item.
    result.iterator->value->setOriginalURL(originalURL);
    result.iterator->value->setURL(url);
    result.iterator->value->setTitle(title);
    result.iterator->value->setBackForwardData(backForwardData.data(), backForwardData.size());
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPlugins(bool refresh, Vector<PluginInfo>& plugins, Vector<PluginInfo>& applicationPlugins)
{
    if (refresh)
        m_context->pluginInfoStore().refresh();

    Vector<PluginModuleInfo> pluginModules = m_context->pluginInfoStore().plugins();
    for (size_t i = 0; i < pluginModules.size(); ++i)
        plugins.append(pluginModules[i].info);

#if ENABLE(PDFKIT_PLUGIN)
    // Add built-in PDF last, so that it's not used when a real plug-in is installed.
    if (!m_context->omitPDFSupport()) {
        plugins.append(PDFPlugin::pluginInfo());
        applicationPlugins.append(PDFPlugin::pluginInfo());
    }
#else
    UNUSED_PARAM(applicationPlugins);
#endif
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPluginProcessConnection(uint64_t pluginProcessToken, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply> reply)
{
    PluginProcessManager::shared().getPluginProcessConnection(pluginProcessToken, reply);
}
#endif

#if ENABLE(NETWORK_PROCESS)
void WebProcessProxy::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    m_context->getNetworkProcessConnection(reply);
}
#endif // ENABLE(NETWORK_PROCESS)

#if ENABLE(DATABASE_PROCESS)
void WebProcessProxy::getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply)
{
    m_context->getDatabaseProcessConnection(reply);
}
#endif // ENABLE(DATABASE_PROCESS)

void WebProcessProxy::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_context->dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName()) {
        didReceiveWebProcessProxyMessage(connection, decoder);
        return;
    }

    // FIXME: Add unhandled message logging.
}

void WebProcessProxy::didReceiveSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    if (m_context->dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName()) {
        didReceiveSyncWebProcessProxyMessage(connection, decoder, replyEncoder);
        return;
    }

    // FIXME: Add unhandled message logging.
}

void WebProcessProxy::didClose(IPC::Connection*)
{
    // Protect ourselves, as the call to disconnect() below may otherwise cause us
    // to be deleted before we can finish our work.
    Ref<WebProcessProxy> protect(*this);

    webConnection()->didClose();

    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);

    disconnect();

    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidCrash();

}

void WebProcessProxy::didReceiveInvalidMessage(IPC::Connection* connection, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received an invalid message \"%s.%s\" from the web process.\n", messageReceiverName.toString().data(), messageName.toString().data());

    WebContext::didReceiveInvalidMessage(messageReceiverName, messageName);

    // Terminate the WebProcess.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void WebProcessProxy::didBecomeUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeUnresponsive();
}

void WebProcessProxy::interactionOccurredWhileUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->interactionOccurredWhileProcessUnresponsive();
}

void WebProcessProxy::didBecomeResponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeResponsive();
}

void WebProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    m_webConnection = WebConnectionToWebProcess::create(this);

    m_context->processDidFinishLaunching(this);

#if PLATFORM(COCOA)
    updateProcessSuppressionState();
#endif
    updateProcessState();
}

WebFrameProxy* WebProcessProxy::webFrame(uint64_t frameID) const
{
    if (!WebFrameProxyMap::isValidKey(frameID))
        return 0;

    return m_frameMap.get(frameID);
}

bool WebProcessProxy::canCreateFrame(uint64_t frameID) const
{
    return WebFrameProxyMap::isValidKey(frameID) && !m_frameMap.contains(frameID);
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
    ASSERT(WebFrameProxyMap::isValidKey(frameID));
    m_frameMap.remove(frameID);
}

void WebProcessProxy::disconnectFramesFromPage(WebPageProxy* page)
{
    Vector<RefPtr<WebFrameProxy>> frames;
    copyValuesToVector(m_frameMap, frames);
    for (size_t i = 0, size = frames.size(); i < size; ++i) {
        if (frames[i]->page() == page)
            frames[i]->disconnect();
    }
}

size_t WebProcessProxy::frameCountInPage(WebPageProxy* page) const
{
    size_t result = 0;
    for (HashMap<uint64_t, RefPtr<WebFrameProxy>>::const_iterator iter = m_frameMap.begin(); iter != m_frameMap.end(); ++iter) {
        if (iter->value->page() == page)
            ++result;
    }
    return result;
}

bool WebProcessProxy::canTerminateChildProcess()
{
    if (!m_pageMap.isEmpty())
        return false;

    if (m_downloadProxyMap && !m_downloadProxyMap->isEmpty())
        return false;

    if (!m_context->shouldTerminate(this))
        return false;

    return true;
}

void WebProcessProxy::shouldTerminate(bool& shouldTerminate)
{
    shouldTerminate = canTerminateChildProcess();
    if (shouldTerminate) {
        // We know that the web process is going to terminate so disconnect it from the context.
        disconnect();
    }
}

void WebProcessProxy::updateTextCheckerState()
{
    if (canSendMessage())
        send(Messages::WebProcess::SetTextCheckerState(TextChecker::state()), 0);
}

DownloadProxy* WebProcessProxy::createDownloadProxy()
{
#if ENABLE(NETWORK_PROCESS)
    ASSERT(!m_context->usesNetworkProcess());
#endif

    if (!m_downloadProxyMap)
        m_downloadProxyMap = adoptPtr(new DownloadProxyMap(this));

    return m_downloadProxyMap->createDownloadProxy(m_context.get());
}

void WebProcessProxy::didNavigateWithNavigationData(uint64_t pageID, const WebNavigationDataStore& store, uint64_t frameID) 
{
    WebPageProxy* page = webPage(pageID);
    if (!page)
        return;
    
    WebFrameProxy* frame = webFrame(frameID);
    MESSAGE_CHECK(frame);
    MESSAGE_CHECK(frame->page() == page);
    
    m_context->historyClient().didNavigateWithNavigationData(&m_context.get(), page, store, frame);
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

    m_context->historyClient().didPerformClientRedirect(&m_context.get(), page, sourceURLString, destinationURLString, frame);
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

    m_context->historyClient().didPerformServerRedirect(&m_context.get(), page, sourceURLString, destinationURLString, frame);
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

    m_context->historyClient().didUpdateHistoryTitle(&m_context.get(), page, title, url, frame);
}

void WebProcessProxy::pageSuppressibilityChanged(WebKit::WebPageProxy *page)
{
#if PLATFORM(COCOA)
    if (page->isProcessSuppressible())
        m_processSuppressiblePages.add(page->pageID());
    else
        m_processSuppressiblePages.remove(page->pageID());
    updateProcessSuppressionState();
#else
    UNUSED_PARAM(page);
#endif
}

void WebProcessProxy::pagePreferencesChanged(WebKit::WebPageProxy *page)
{
#if PLATFORM(COCOA)
    if (page->isProcessSuppressible())
        m_processSuppressiblePages.add(page->pageID());
    else
        m_processSuppressiblePages.remove(page->pageID());
    updateProcessSuppressionState();
#else
    UNUSED_PARAM(page);
#endif
}

void WebProcessProxy::didSaveToPageCache()
{
    m_context->processDidCachePage(this);
}

void WebProcessProxy::releasePageCache()
{
    if (canSendMessage())
        send(Messages::WebProcess::ReleasePageCache(), 0);
}

void WebProcessProxy::windowServerConnectionStateChanged()
{
    for (const auto& page : m_pageMap.values())
        page->viewStateDidChange(ViewState::IsVisuallyIdle);
}

void WebProcessProxy::requestTermination()
{
    if (!isValid())
        return;

    ChildProcessProxy::terminate();

    if (webConnection())
        webConnection()->didClose();

    disconnect();
}

void WebProcessProxy::enableSuddenTermination()
{
    if (!isValid())
        return;

    ASSERT(m_numberOfTimesSuddenTerminationWasDisabled);
    WebCore::enableSuddenTermination();
    --m_numberOfTimesSuddenTerminationWasDisabled;
}

void WebProcessProxy::disableSuddenTermination()
{
    if (!isValid())
        return;

    WebCore::disableSuddenTermination();
    ++m_numberOfTimesSuddenTerminationWasDisabled;
}

RefPtr<API::Object> WebProcessProxy::apiObjectByConvertingToHandles(API::Object* object)
{
    return UserData::transform(object, [](const API::Object& object) -> RefPtr<API::Object> {
        switch (object.type()) {
        case API::Object::Type::Frame: {
            auto& frame = static_cast<const WebFrameProxy&>(object);
            return API::FrameHandle::create(frame.frameID());
        }

        default:
            return nullptr;
        }
    });
}

} // namespace WebKit
