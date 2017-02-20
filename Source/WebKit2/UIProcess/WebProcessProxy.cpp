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

#include "config.h"
#include "WebProcessProxy.h"

#include "APIFrameHandle.h"
#include "APIPageGroupHandle.h"
#include "APIPageHandle.h"
#include "DataReference.h"
#include "DownloadProxyMap.h"
#include "Logging.h"
#include "PluginInfoStore.h"
#include "PluginProcessManager.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UnresponsiveWebProcessTerminator.h"
#include "UserData.h"
#include "WebBackForwardListItem.h"
#include "WebIconDatabase.h"
#include "WebInspectorUtilities.h"
#include "WebNavigationDataStore.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPasteboardProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxyMessages.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include <WebCore/SuddenTermination.h>
#include <WebCore/URL.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
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

Ref<WebProcessProxy> WebProcessProxy::create(WebProcessPool& processPool)
{
    return adoptRef(*new WebProcessProxy(processPool));
}

WebProcessProxy::WebProcessProxy(WebProcessPool& processPool)
    : ChildProcessProxy(processPool.alwaysRunsAtBackgroundPriority())
    , m_responsivenessTimer(*this)
    , m_processPool(processPool)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
    , m_numberOfTimesSuddenTerminationWasDisabled(0)
    , m_throttler(*this)
    , m_isResponsive(NoOrMaybe::Maybe)
    , m_visiblePageCounter([this](RefCounterEvent) { updateBackgroundResponsivenessTimer(); })
    , m_backgroundResponsivenessTimer(processPool.configuration().unresponsiveBackgroundProcessesTerminationEnabled() ? std::make_unique<UnresponsiveWebProcessTerminator>(*this) : nullptr)
{
    WebPasteboardProxy::singleton().addWebProcessProxy(*this);

    connect();
}

WebProcessProxy::~WebProcessProxy()
{
    ASSERT(m_pageURLRetainCountMap.isEmpty());
    
    WebPasteboardProxy::singleton().removeWebProcessProxy(*this);

    if (m_webConnection)
        m_webConnection->invalidate();

    while (m_numberOfTimesSuddenTerminationWasDisabled-- > 0)
        WebCore::enableSuddenTermination();
}

void WebProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Web;

    ChildProcessProxy::getLaunchOptions(launchOptions);

    if (WebKit::isInspectorProcessPool(m_processPool))
        launchOptions.extraInitializationData.add(ASCIILiteral("inspector-process"), ASCIILiteral("1"));

    auto overrideLanguages = m_processPool->configuration().overrideLanguages();
    if (overrideLanguages.size()) {
        StringBuilder languageString;
        for (size_t i = 0; i < overrideLanguages.size(); ++i) {
            if (i)
                languageString.append(',');
            languageString.append(overrideLanguages[i]);
        }
        launchOptions.extraInitializationData.add(ASCIILiteral("OverrideLanguages"), languageString.toString());
    }
}

void WebProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
    ASSERT(this->connection() == &connection);

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::singleton().initializeConnection(connection);
#endif

    for (auto& page : m_pageMap.values())
        page->connectionWillOpen(connection);
}

void WebProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);

    for (auto& page : m_pageMap.values())
        page->webProcessWillShutDown();

    releaseRemainingIconsForPageURLs();
}

void WebProcessProxy::shutDown()
{
    shutDownProcess();

    if (m_webConnection) {
        m_webConnection->invalidate();
        m_webConnection = nullptr;
    }

    m_responsivenessTimer.invalidate();
    m_tokenForHoldingLockedFiles = nullptr;

    Vector<RefPtr<WebFrameProxy>> frames;
    copyValuesToVector(m_frameMap, frames);

    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->webProcessWillShutDown();
    m_frameMap.clear();

    for (VisitedLinkStore* visitedLinkStore : m_visitedLinkStores)
        visitedLinkStore->removeProcess(*this);
    m_visitedLinkStores.clear();

    for (WebUserContentControllerProxy* webUserContentControllerProxy : m_webUserContentControllerProxies)
        webUserContentControllerProxy->removeProcess(*this);
    m_webUserContentControllerProxies.clear();

    m_userInitiatedActionMap.clear();

    m_processPool->disconnectProcess(this);
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID)
{
    return globalPageMap().get(pageID);
}

void WebProcessProxy::deleteWebsiteDataForTopPrivatelyOwnedDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType> dataTypes, Vector<String>& topPrivatelyOwnedDomains, bool shouldNotifyPage, std::function<void()> completionHandler)
{
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(std::function<void()> completionHandler)
            : completionHandler(WTFMove(completionHandler))
        {
        }
        
        void addPendingCallback()
        {
            ++pendingCallbacks;
        }
        
        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;
            
            callIfNeeded();
        }
        
        void callIfNeeded()
        {
            if (!pendingCallbacks)
                completionHandler();
        }
        
        unsigned pendingCallbacks = 0;
        std::function<void()> completionHandler;
    };
    
    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));

    for (auto& page : globalPageMap()) {
        if (!page.value->websiteDataStore().isPersistent())
            continue;
        callbackAggregator->addPendingCallback();
        page.value->websiteDataStore().removeDataForTopPrivatelyOwnedDomains(dataTypes, { }, topPrivatelyOwnedDomains, [callbackAggregator, shouldNotifyPage, page]() {
            if (shouldNotifyPage)
                page.value->postMessageToInjectedBundle("WebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished", nullptr);
            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }
}

Ref<WebPageProxy> WebProcessProxy::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    uint64_t pageID = generatePageID();
    Ref<WebPageProxy> webPage = WebPageProxy::create(pageClient, *this, pageID, WTFMove(pageConfiguration));

    m_pageMap.set(pageID, webPage.ptr());
    globalPageMap().set(pageID, webPage.ptr());

    updateBackgroundResponsivenessTimer();

    return webPage;
}

void WebProcessProxy::addExistingWebPage(WebPageProxy* webPage, uint64_t pageID)
{
    ASSERT(!m_pageMap.contains(pageID));
    ASSERT(!globalPageMap().contains(pageID));

    m_pageMap.set(pageID, webPage);
    globalPageMap().set(pageID, webPage);

    updateBackgroundResponsivenessTimer();
}

void WebProcessProxy::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);
    globalPageMap().remove(pageID);

    updateBackgroundResponsivenessTimer();
    
    Vector<uint64_t> itemIDsToRemove;
    for (auto& idAndItem : m_backForwardListItemMap) {
        if (idAndItem.value->pageID() == pageID)
            itemIDsToRemove.append(idAndItem.key);
    }
    for (auto itemID : itemIDsToRemove)
        m_backForwardListItemMap.remove(itemID);

    // If this was the last WebPage open in that web process, and we have no other reason to keep it alive, let it go.
    // We only allow this when using a network process, as otherwise the WebProcess needs to preserve its session state.
    if (state() == State::Terminated || !canTerminateChildProcess())
        return;

    shutDown();
}

void WebProcessProxy::addVisitedLinkStore(VisitedLinkStore& store)
{
    m_visitedLinkStores.add(&store);
    store.addProcess(*this);
}

void WebProcessProxy::addWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    m_webUserContentControllerProxies.add(&proxy);
    proxy.addProcess(*this);
}

void WebProcessProxy::didDestroyVisitedLinkStore(VisitedLinkStore& store)
{
    ASSERT(m_visitedLinkStores.contains(&store));
    m_visitedLinkStores.remove(&store);
}

void WebProcessProxy::didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    ASSERT(m_webUserContentControllerProxies.contains(&proxy));
    m_webUserContentControllerProxies.remove(&proxy);
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

void WebProcessProxy::removeBackForwardItem(uint64_t itemID)
{
    m_backForwardListItemMap.remove(itemID);
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

bool WebProcessProxy::hasAssumedReadAccessToURL(const URL& url) const
{
    if (!url.isLocalFile())
        return false;

    String path = url.fileSystemPath();
    auto startsWithURLPath = [&path](const String& assumedAccessPath) {
        // There are no ".." components, because URL removes those.
        return path.startsWith(assumedAccessPath);
    };

    auto& platformPaths = platformPathsWithAssumedReadAccess();
    auto platformPathsEnd = platformPaths.end();
    if (std::find_if(platformPaths.begin(), platformPathsEnd, startsWithURLPath) != platformPathsEnd)
        return true;

    auto localPathsEnd = m_localPathsWithAssumedReadAccess.end();
    if (std::find_if(m_localPathsWithAssumedReadAccess.begin(), localPathsEnd, startsWithURLPath) != localPathsEnd)
        return true;

    return false;
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
    if (hasAssumedReadAccessToURL(url))
        return true;

    // Items in back/forward list have been already checked.
    // One case where we don't have sandbox extensions for file URLs in b/f list is if the list has been reinstated after a crash or a browser restart.
    String path = url.fileSystemPath();
    for (WebBackForwardListItemMap::iterator iter = m_backForwardListItemMap.begin(), end = m_backForwardListItemMap.end(); iter != end; ++iter) {
        URL itemURL(URL(), iter->value->url());
        if (itemURL.isLocalFile() && itemURL.fileSystemPath() == path)
            return true;
        URL itemOriginalURL(URL(), iter->value->originalURL());
        if (itemOriginalURL.isLocalFile() && itemOriginalURL.fileSystemPath() == path)
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

void WebProcessProxy::addBackForwardItem(uint64_t itemID, uint64_t pageID, const PageState& pageState)
{
    MESSAGE_CHECK_URL(pageState.mainFrameState.originalURLString);
    MESSAGE_CHECK_URL(pageState.mainFrameState.urlString);

    auto& backForwardListItem = m_backForwardListItemMap.add(itemID, nullptr).iterator->value;
    if (!backForwardListItem) {
        BackForwardListItemState backForwardListItemState;
        backForwardListItemState.identifier = itemID;
        backForwardListItemState.pageState = pageState;
        backForwardListItem = WebBackForwardListItem::create(WTFMove(backForwardListItemState), pageID);
        return;
    }

    // Update existing item.
    backForwardListItem->setPageState(pageState);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPlugins(bool refresh, Vector<PluginInfo>& plugins, Vector<PluginInfo>& applicationPlugins)
{
    if (refresh)
        m_processPool->pluginInfoStore().refresh();

    Vector<PluginModuleInfo> pluginModules = m_processPool->pluginInfoStore().plugins();
    for (size_t i = 0; i < pluginModules.size(); ++i)
        plugins.append(pluginModules[i].info);

#if ENABLE(PDFKIT_PLUGIN)
    // Add built-in PDF last, so that it's not used when a real plug-in is installed.
    if (!m_processPool->omitPDFSupport()) {
        plugins.append(PDFPlugin::pluginInfo());
        applicationPlugins.append(PDFPlugin::pluginInfo());
    }
#else
    UNUSED_PARAM(applicationPlugins);
#endif
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPluginProcessConnection(uint64_t pluginProcessToken, Ref<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>&& reply)
{
    PluginProcessManager::singleton().getPluginProcessConnection(pluginProcessToken, WTFMove(reply));
}
#endif

void WebProcessProxy::getNetworkProcessConnection(Ref<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>&& reply)
{
    m_processPool->getNetworkProcessConnection(WTFMove(reply));
}

#if ENABLE(DATABASE_PROCESS)
void WebProcessProxy::getDatabaseProcessConnection(Ref<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>&& reply)
{
    m_processPool->getDatabaseProcessConnection(WTFMove(reply));
}
#endif // ENABLE(DATABASE_PROCESS)

void WebProcessProxy::retainIconForPageURL(const String& pageURL)
{
    WebIconDatabase* iconDatabase = processPool().iconDatabase();
    if (!iconDatabase || pageURL.isEmpty())
        return;

    // Track retain counts so we can release them if the WebProcess terminates early.
    auto result = m_pageURLRetainCountMap.add(pageURL, 1);
    if (!result.isNewEntry)
        ++result.iterator->value;

    iconDatabase->retainIconForPageURL(pageURL);
}

void WebProcessProxy::releaseIconForPageURL(const String& pageURL)
{
    WebIconDatabase* iconDatabase = processPool().iconDatabase();
    if (!iconDatabase || pageURL.isEmpty())
        return;

    // Track retain counts so we can release them if the WebProcess terminates early.
    auto result = m_pageURLRetainCountMap.find(pageURL);
    if (result == m_pageURLRetainCountMap.end())
        return;

    --result->value;
    if (!result->value)
        m_pageURLRetainCountMap.remove(result);

    iconDatabase->releaseIconForPageURL(pageURL);
}

void WebProcessProxy::releaseRemainingIconsForPageURLs()
{
    WebIconDatabase* iconDatabase = processPool().iconDatabase();
    if (!iconDatabase)
        return;

    for (auto& entry : m_pageURLRetainCountMap) {
        uint64_t count = entry.value;
        for (uint64_t i = 0; i < count; ++i)
            iconDatabase->releaseIconForPageURL(entry.key);
    }

    m_pageURLRetainCountMap.clear();
}

#if !PLATFORM(COCOA)
bool WebProcessProxy::platformIsBeingDebugged() const
{
    return false;
}
#endif

void WebProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_processPool->dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName()) {
        didReceiveWebProcessProxyMessage(connection, decoder);
        return;
    }

    // FIXME: Add unhandled message logging.
}

void WebProcessProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    if (m_processPool->dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcessProxy::messageReceiverName()) {
        didReceiveSyncWebProcessProxyMessage(connection, decoder, replyEncoder);
        return;
    }

    // FIXME: Add unhandled message logging.
}

void WebProcessProxy::didClose(IPC::Connection&)
{
    // Protect ourselves, as the call to disconnect() below may otherwise cause us
    // to be deleted before we can finish our work.
    Ref<WebProcessProxy> protect(*this);

    webConnection()->didClose();

    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);

    shutDown();

    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidCrash();

}

void WebProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received an invalid message \"%s.%s\" from the web process.\n", messageReceiverName.toString().data(), messageName.toString().data());

    WebProcessPool::didReceiveInvalidMessage(messageReceiverName, messageName);

    // Terminate the WebProcess.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void WebProcessProxy::didBecomeUnresponsive()
{
    m_isResponsive = NoOrMaybe::No;

    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);

    for (auto& page : pages)
        page->processDidBecomeUnresponsive();

    bool isWebProcessResponsive = false;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);
}

void WebProcessProxy::didBecomeResponsive()
{
    m_isResponsive = NoOrMaybe::Maybe;

    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (auto& page : pages)
        page->processDidBecomeResponsive();
}

void WebProcessProxy::willChangeIsResponsive()
{
    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (auto& page : pages)
        page->willChangeProcessIsResponsive();
}

void WebProcessProxy::didChangeIsResponsive()
{
    Vector<RefPtr<WebPageProxy>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (auto& page : pages)
        page->didChangeProcessIsResponsive();
}

bool WebProcessProxy::mayBecomeUnresponsive()
{
    return !platformIsBeingDebugged();
}

void WebProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    for (WebPageProxy* page : m_pageMap.values()) {
        ASSERT(this == &page->process());
        page->processDidFinishLaunching();
    }

    m_webConnection = WebConnectionToWebProcess::create(this);

    m_processPool->processDidFinishLaunching(this);

#if PLATFORM(IOS)
    if (connection()) {
        if (xpc_connection_t xpcConnection = connection()->xpcConnection())
            m_throttler.didConnectToProcess(xpc_connection_get_pid(xpcConnection));
    }
#endif
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
            frames[i]->webProcessWillShutDown();
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

auto WebProcessProxy::visiblePageToken() const -> VisibleWebPageToken
{
    return m_visiblePageCounter.count();
}

RefPtr<API::UserInitiatedAction> WebProcessProxy::userInitiatedActivity(uint64_t identifier)
{
    if (!UserInitiatedActionMap::isValidKey(identifier) || !identifier)
        return nullptr;

    auto result = m_userInitiatedActionMap.ensure(identifier, [] { return API::UserInitiatedAction::create(); });
    return result.iterator->value;
}

void WebProcessProxy::didDestroyUserGestureToken(uint64_t identifier)
{
    ASSERT(UserInitiatedActionMap::isValidKey(identifier));
    m_userInitiatedActionMap.remove(identifier);
}

bool WebProcessProxy::canTerminateChildProcess()
{
    if (!m_pageMap.isEmpty())
        return false;

    if (!m_processPool->shouldTerminate(this))
        return false;

    return true;
}

void WebProcessProxy::shouldTerminate(bool& shouldTerminate)
{
    shouldTerminate = canTerminateChildProcess();
    if (shouldTerminate) {
        // We know that the web process is going to terminate so start shutting it down in the UI process.
        shutDown();
    }
}

void WebProcessProxy::updateTextCheckerState()
{
    if (canSendMessage())
        send(Messages::WebProcess::SetTextCheckerState(TextChecker::state()), 0);
}

void WebProcessProxy::didSaveToPageCache()
{
    m_processPool->processDidCachePage(this);
}

void WebProcessProxy::releasePageCache()
{
    if (canSendMessage())
        send(Messages::WebProcess::ReleasePageCache(), 0);
}

void WebProcessProxy::windowServerConnectionStateChanged()
{
    for (const auto& page : m_pageMap.values())
        page->activityStateDidChange(ActivityState::IsVisuallyIdle);
}

void WebProcessProxy::fetchWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, Function<void (WebsiteData)> completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is fetching Website data", this);

    connection()->sendWithReply(Messages::WebProcess::FetchWebsiteData(sessionID, dataTypes), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID](auto reply) {
        if (!reply) {
            completionHandler(WebsiteData { });
            return;
        }

        completionHandler(WTFMove(std::get<0>(*reply)));
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done fetching Website data", this);
    });
}

void WebProcessProxy::deleteWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, std::chrono::system_clock::time_point modifiedSince, Function<void ()> completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is deleting Website data", this);

    connection()->sendWithReply(Messages::WebProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID](auto reply) {
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done deleting Website data", this);
    });
}

void WebProcessProxy::deleteWebsiteDataForOrigins(SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, Function<void()> completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is deleting Website data for several origins", this);

    connection()->sendWithReply(Messages::WebProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, origins), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID](auto reply) {
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done deleting Website data for several origins", this);
    });
}

void WebProcessProxy::requestTermination()
{
    if (state() == State::Terminated)
        return;

    ChildProcessProxy::terminate();

    if (webConnection())
        webConnection()->didClose();

    shutDown();
}

void WebProcessProxy::enableSuddenTermination()
{
    if (state() != State::Running)
        return;

    ASSERT(m_numberOfTimesSuddenTerminationWasDisabled);
    WebCore::enableSuddenTermination();
    --m_numberOfTimesSuddenTerminationWasDisabled;
}

void WebProcessProxy::disableSuddenTermination()
{
    if (state() != State::Running)
        return;

    WebCore::disableSuddenTermination();
    ++m_numberOfTimesSuddenTerminationWasDisabled;
}

RefPtr<API::Object> WebProcessProxy::transformHandlesToObjects(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        Transformer(WebProcessProxy& webProcessProxy)
            : m_webProcessProxy(webProcessProxy)
        {
        }

        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                return static_cast<const API::FrameHandle&>(object).isAutoconverting();

            case API::Object::Type::PageHandle:
                return static_cast<const API::PageHandle&>(object).isAutoconverting();

            case API::Object::Type::PageGroupHandle:
#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
#endif
                return true;

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                ASSERT(static_cast<API::FrameHandle&>(object).isAutoconverting());
                return m_webProcessProxy.webFrame(static_cast<API::FrameHandle&>(object).frameID());

            case API::Object::Type::PageGroupHandle:
                return WebPageGroup::get(static_cast<API::PageGroupHandle&>(object).webPageGroupData().pageGroupID);

            case API::Object::Type::PageHandle:
                ASSERT(static_cast<API::PageHandle&>(object).isAutoconverting());
                return m_webProcessProxy.webPage(static_cast<API::PageHandle&>(object).pageID());

#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
                return m_webProcessProxy.transformHandlesToObjects(static_cast<ObjCObjectGraph&>(object));
#endif
            default:
                return &object;
            }
        }

        WebProcessProxy& m_webProcessProxy;
    };

    return UserData::transform(object, Transformer(*this));
}

RefPtr<API::Object> WebProcessProxy::transformObjectsToHandles(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::Frame:
            case API::Object::Type::Page:
            case API::Object::Type::PageGroup:
#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
#endif
                return true;

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::Frame:
                return API::FrameHandle::createAutoconverting(static_cast<const WebFrameProxy&>(object).frameID());

            case API::Object::Type::Page:
                return API::PageHandle::createAutoconverting(static_cast<const WebPageProxy&>(object).pageID());

            case API::Object::Type::PageGroup:
                return API::PageGroupHandle::create(WebPageGroupData(static_cast<const WebPageGroup&>(object).data()));

#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
                return transformObjectsToHandles(static_cast<ObjCObjectGraph&>(object));
#endif

            default:
                return &object;
            }
        }
    };

    return UserData::transform(object, Transformer());
}

void WebProcessProxy::sendProcessWillSuspendImminently()
{
    if (!canSendMessage())
        return;

    bool handled = false;
    sendSync(Messages::WebProcess::ProcessWillSuspendImminently(), Messages::WebProcess::ProcessWillSuspendImminently::Reply(handled), 0, 1_s);
}

void WebProcessProxy::sendPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::WebProcess::PrepareToSuspend(), 0);
}

void WebProcessProxy::sendCancelPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::WebProcess::CancelPrepareToSuspend(), 0);
}

void WebProcessProxy::sendProcessDidResume()
{
    if (canSendMessage())
        send(Messages::WebProcess::ProcessDidResume(), 0);
}

void WebProcessProxy::processReadyToSuspend()
{
    m_throttler.processReadyToSuspend();
}

void WebProcessProxy::didCancelProcessSuspension()
{
    m_throttler.didCancelProcessSuspension();
}

void WebProcessProxy::reinstateNetworkProcessAssertionState(NetworkProcessProxy& newNetworkProcessProxy)
{
#if PLATFORM(IOS)
    ASSERT(!m_backgroundTokenForNetworkProcess || !m_foregroundTokenForNetworkProcess);

    // The network process crashed; take new tokens for the new network process.
    if (m_backgroundTokenForNetworkProcess)
        m_backgroundTokenForNetworkProcess = newNetworkProcessProxy.throttler().backgroundActivityToken();
    else if (m_foregroundTokenForNetworkProcess)
        m_foregroundTokenForNetworkProcess = newNetworkProcessProxy.throttler().foregroundActivityToken();
#else
    UNUSED_PARAM(newNetworkProcessProxy);
#endif
}

void WebProcessProxy::didSetAssertionState(AssertionState state)
{
#if PLATFORM(IOS)
    ASSERT(!m_backgroundTokenForNetworkProcess || !m_foregroundTokenForNetworkProcess);

    switch (state) {
    case AssertionState::Suspended:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Suspended) release all assertions for network process", this);
        m_foregroundTokenForNetworkProcess = nullptr;
        m_backgroundTokenForNetworkProcess = nullptr;
        for (auto& page : m_pageMap.values())
            page->processWillBecomeSuspended();
        break;

    case AssertionState::Background:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Background) taking background assertion for network process", this);
        m_backgroundTokenForNetworkProcess = processPool().ensureNetworkProcess().throttler().backgroundActivityToken();
        m_foregroundTokenForNetworkProcess = nullptr;
        break;
    
    case AssertionState::Foreground:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Foreground) taking foreground assertion for network process", this);
        m_foregroundTokenForNetworkProcess = processPool().ensureNetworkProcess().throttler().foregroundActivityToken();
        m_backgroundTokenForNetworkProcess = nullptr;
        for (auto& page : m_pageMap.values())
            page->processWillBecomeForeground();
        break;
    }

    ASSERT(!m_backgroundTokenForNetworkProcess || !m_foregroundTokenForNetworkProcess);
#else
    UNUSED_PARAM(state);
#endif
}
    
void WebProcessProxy::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    if (!isHoldingLockedFiles) {
        RELEASE_LOG(ProcessSuspension, "UIProcess is releasing a background assertion because the WebContent process is no longer holding locked files");
        m_tokenForHoldingLockedFiles = nullptr;
        return;
    }
    if (!m_tokenForHoldingLockedFiles) {
        RELEASE_LOG(ProcessSuspension, "UIProcess is taking a background assertion because the WebContent process is holding locked files");
        m_tokenForHoldingLockedFiles = m_throttler.backgroundActivityToken();
    }
}

void WebProcessProxy::isResponsive(std::function<void(bool isWebProcessResponsive)> callback)
{
    if (m_isResponsive == NoOrMaybe::No) {
        if (callback) {
            RunLoop::main().dispatch([callback = WTFMove(callback)] {
                bool isWebProcessResponsive = false;
                callback(isWebProcessResponsive);
            });
        }
        return;
    }

    if (callback)
        m_isResponsiveCallbacks.append(callback);

    responsivenessTimer().start();
    send(Messages::WebProcess::MainThreadPing(), 0);
}

void WebProcessProxy::didReceiveMainThreadPing()
{
    responsivenessTimer().stop();

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);
    bool isWebProcessResponsive = true;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);
}

void WebProcessProxy::updateBackgroundResponsivenessTimer()
{
    if (m_backgroundResponsivenessTimer)
        m_backgroundResponsivenessTimer->updateState();
}

#if !PLATFORM(COCOA)
const HashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<HashSet<String>> platformPathsWithAssumedReadAccess;
    return platformPathsWithAssumedReadAccess;
}
#endif

} // namespace WebKit
