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
#include "ProvisionalPageProxy.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "UIMessagePortChannelProvider.h"
#include "UserData.h"
#include "WebBackForwardListItem.h"
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
#include "WebsiteDataFetchOption.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/PrewarmInformation.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/SuddenTermination.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#include "PDFPlugin.h"
#include "UserMediaCaptureManagerProxy.h"
#include "VersionChecks.h"
#endif

#if PLATFORM(MAC)
#include "HighPerformanceGPUManager.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromWebProcess(url), connection())

namespace WebKit {
using namespace WebCore;

static bool isMainThreadOrCheckDisabled()
{
#if PLATFORM(IOS_FAMILY)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfter(SDKVersion::FirstWithMainThreadReleaseAssertionInWebPageProxy);
#elif PLATFORM(MAC)
    return LIKELY(RunLoop::isMain()) || !linkedOnOrAfter(SDKVersion::FirstWithMainThreadReleaseAssertionInWebPageProxy);
#else
    return RunLoop::isMain();
#endif
}

static HashMap<ProcessIdentifier, WebProcessProxy*>& allProcesses()
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<HashMap<ProcessIdentifier, WebProcessProxy*>> map;
    return map;
}

WebProcessProxy* WebProcessProxy::processForIdentifier(ProcessIdentifier identifier)
{
    return allProcesses().get(identifier);
}

uint64_t WebProcessProxy::generatePageID()
{
    static uint64_t uniquePageID;
    return ++uniquePageID;
}

static WebProcessProxy::WebPageProxyMap& globalPageMap()
{
    ASSERT(isMainThreadOrCheckDisabled());
    static NeverDestroyed<WebProcessProxy::WebPageProxyMap> pageMap;
    return pageMap;
}

Ref<WebProcessProxy> WebProcessProxy::create(WebProcessPool& processPool, WebsiteDataStore& websiteDataStore, IsPrewarmed isPrewarmed)
{
    auto proxy = adoptRef(*new WebProcessProxy(processPool, websiteDataStore, isPrewarmed));
    proxy->connect();
    return proxy;
}

WebProcessProxy::WebProcessProxy(WebProcessPool& processPool, WebsiteDataStore& websiteDataStore, IsPrewarmed isPrewarmed)
    : ChildProcessProxy(processPool.alwaysRunsAtBackgroundPriority())
    , m_responsivenessTimer(*this)
    , m_backgroundResponsivenessTimer(*this)
    , m_processPool(processPool, isPrewarmed == IsPrewarmed::Yes ? IsWeak::Yes : IsWeak::No)
    , m_mayHaveUniversalFileReadSandboxExtension(false)
    , m_numberOfTimesSuddenTerminationWasDisabled(0)
    , m_throttler(*this, processPool.shouldTakeUIBackgroundAssertion())
    , m_isResponsive(NoOrMaybe::Maybe)
    , m_visiblePageCounter([this](RefCounterEvent) { updateBackgroundResponsivenessTimer(); })
    , m_websiteDataStore(websiteDataStore)
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    , m_userMediaCaptureManagerProxy(std::make_unique<UserMediaCaptureManagerProxy>(*this))
#endif
    , m_isPrewarmed(isPrewarmed == IsPrewarmed::Yes)
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());

    auto result = allProcesses().add(coreProcessIdentifier(), this);
    ASSERT_UNUSED(result, result.isNewEntry);

    WebPasteboardProxy::singleton().addWebProcessProxy(*this);
}

WebProcessProxy::~WebProcessProxy()
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());
    ASSERT(m_pageURLRetainCountMap.isEmpty());

    auto result = allProcesses().remove(coreProcessIdentifier());
    ASSERT_UNUSED(result, result);

    WebPasteboardProxy::singleton().removeWebProcessProxy(*this);

    if (m_webConnection)
        m_webConnection->invalidate();

    while (m_numberOfTimesSuddenTerminationWasDisabled-- > 0)
        WebCore::enableSuddenTermination();

    for (auto& callback : m_localPortActivityCompletionHandlers.values())
        callback(MessagePortChannelProvider::HasActivity::No);

#if PLATFORM(MAC)
    HighPerformanceGPUManager::singleton().removeProcessRequiringHighPerformance(this);
#endif
}

void WebProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Web;

    ChildProcessProxy::getLaunchOptions(launchOptions);

    if (!m_processPool->customWebContentServiceBundleIdentifier().isEmpty())
        launchOptions.customWebContentServiceBundleIdentifier = m_processPool->customWebContentServiceBundleIdentifier().ascii();
    if (WebKit::isInspectorProcessPool(processPool()))
        launchOptions.extraInitializationData.add("inspector-process"_s, "1"_s);

    auto overrideLanguages = m_processPool->configuration().overrideLanguages();
    if (overrideLanguages.size()) {
        StringBuilder languageString;
        for (size_t i = 0; i < overrideLanguages.size(); ++i) {
            if (i)
                languageString.append(',');
            languageString.append(overrideLanguages[i]);
        }
        launchOptions.extraInitializationData.add("OverrideLanguages"_s, languageString.toString());
    }

    launchOptions.nonValidInjectedCodeAllowed = shouldAllowNonValidInjectedCode();

    if (isPrewarmed())
        launchOptions.extraInitializationData.add("is-prewarmed"_s, "1"_s);

    if (processPool().shouldMakeNextWebProcessLaunchFailForTesting()) {
        processPool().setShouldMakeNextWebProcessLaunchFailForTesting(false);
        launchOptions.shouldMakeProcessLaunchFailForTesting = true;
    }
}

#if !PLATFORM(GTK) && !PLATFORM(WPE)
void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
}
#endif

void WebProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
    ASSERT(this->connection() == &connection);

    // Throttling IPC messages coming from the WebProcesses so that the UIProcess stays responsive, even
    // if one of the WebProcesses misbehaves.
    connection.enableIncomingMessagesThrottling();

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the UIProcess, which would be unsafe.
    connection.setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);

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
}

void WebProcessProxy::shutDown()
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());

    shutDownProcess();

    if (m_webConnection) {
        m_webConnection->invalidate();
        m_webConnection = nullptr;
    }

    m_responsivenessTimer.invalidate();
    m_backgroundResponsivenessTimer.invalidate();
    m_tokenForHoldingLockedFiles = nullptr;

    for (auto& frame : copyToVector(m_frameMap.values()))
        frame->webProcessWillShutDown();
    m_frameMap.clear();

    for (auto* visitedLinkStore : m_visitedLinkStores)
        visitedLinkStore->removeProcess(*this);
    m_visitedLinkStores.clear();

    for (auto* webUserContentControllerProxy : m_webUserContentControllerProxies)
        webUserContentControllerProxy->removeProcess(*this);
    m_webUserContentControllerProxies.clear();

    m_userInitiatedActionMap.clear();

    for (auto& port : m_processEntangledPorts)
        UIMessagePortChannelProvider::singleton().registry().didCloseMessagePort(port);

    m_processPool->disconnectProcess(this);
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID)
{
    return globalPageMap().get(pageID);
}

void WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType> dataTypes, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPage, CompletionHandler<void (const HashSet<String>&)>&& completionHandler)
{
    // We expect this to be called on the main thread so we get the default website data store.
    ASSERT(isMainThreadOrCheckDisabled());
    
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(HashSet<String>)>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
        {
        }
        void addDomainsWithDeletedWebsiteData(const HashSet<String>& domains)
        {
            domainsWithDeletedWebsiteData.add(domains.begin(), domains.end());
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
                completionHandler(domainsWithDeletedWebsiteData);
        }
        
        unsigned pendingCallbacks = 0;
        CompletionHandler<void(HashSet<String>)> completionHandler;
        HashSet<String> domainsWithDeletedWebsiteData;
    };
    
    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));
    OptionSet<WebsiteDataFetchOption> fetchOptions = WebsiteDataFetchOption::DoNotCreateProcesses;

    HashSet<PAL::SessionID> visitedSessionIDs;
    for (auto& page : globalPageMap()) {
        auto& dataStore = page.value->websiteDataStore();
        if (!dataStore.isPersistent() || visitedSessionIDs.contains(dataStore.sessionID()))
            continue;
        visitedSessionIDs.add(dataStore.sessionID());
        callbackAggregator->addPendingCallback();
        dataStore.removeDataForTopPrivatelyControlledDomains(dataTypes, fetchOptions, topPrivatelyControlledDomains, [callbackAggregator, shouldNotifyPage, page](HashSet<String>&& domainsWithDeletedWebsiteData) {
            // When completing the task, we should be getting called on the main thread.
            ASSERT(isMainThreadOrCheckDisabled());
            
            if (shouldNotifyPage)
                page.value->postMessageToInjectedBundle("WebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished", nullptr);
            
            callbackAggregator->addDomainsWithDeletedWebsiteData(WTFMove(domainsWithDeletedWebsiteData));
            callbackAggregator->removePendingCallback();
        });
    }
}

void WebProcessProxy::topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<String>&&)>&& completionHandler)
{
    // We expect this to be called on the main thread so we get the default website data store.
    ASSERT(isMainThreadOrCheckDisabled());
    
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(HashSet<String>&&)>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
        {
        }
        
        void addDomainsWithDeletedWebsiteData(HashSet<String>&& domains)
        {
            domainsWithDeletedWebsiteData.add(domains.begin(), domains.end());
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
                completionHandler(WTFMove(domainsWithDeletedWebsiteData));
        }
        
        unsigned pendingCallbacks = 0;
        CompletionHandler<void(HashSet<String>&&)> completionHandler;
        HashSet<String> domainsWithDeletedWebsiteData;
    };
    
    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));
    
    HashSet<PAL::SessionID> visitedSessionIDs;
    for (auto& page : globalPageMap().values()) {
        auto& dataStore = page->websiteDataStore();
        if (!dataStore.isPersistent() || visitedSessionIDs.contains(dataStore.sessionID()))
            continue;
        visitedSessionIDs.add(dataStore.sessionID());
        callbackAggregator->addPendingCallback();
        dataStore.topPrivatelyControlledDomainsWithWebsiteData(dataTypes, { }, [callbackAggregator, shouldNotifyPage, page = makeRef(*page)](HashSet<String>&& domainsWithDataRecords) {
            // When completing the task, we should be getting called on the main thread.
            ASSERT(isMainThreadOrCheckDisabled());
            
            if (shouldNotifyPage)
                page->postMessageToInjectedBundle("WebsiteDataScanForTopPrivatelyControlledDomainsFinished", nullptr);
            
            callbackAggregator->addDomainsWithDeletedWebsiteData(WTFMove(domainsWithDataRecords));
            callbackAggregator->removePendingCallback();
        });
    }

    // FIXME: It's bizarre that this call is on WebProcessProxy and that it doesn't work if there are no visited pages.
    // This should actually be a function of WebsiteDataStore and it should work even if there are no WebViews instances.
    callbackAggregator->callIfNeeded();
}
    
void WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed()
{
    for (auto& page : globalPageMap())
        page.value->postMessageToInjectedBundle("WebsiteDataScanForTopPrivatelyControlledDomainsFinished", nullptr);
}
    
void WebProcessProxy::notifyPageStatisticsTelemetryFinished(API::Object* messageBody)
{
    for (auto& page : globalPageMap())
        page.value->postMessageToInjectedBundle("ResourceLoadStatisticsTelemetryFinished", messageBody);
}
    
Ref<WebPageProxy> WebProcessProxy::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    uint64_t pageID = generatePageID();
    Ref<WebPageProxy> webPage = WebPageProxy::create(pageClient, *this, pageID, WTFMove(pageConfiguration));

    addExistingWebPage(webPage.get(), pageID, BeginsUsingDataStore::Yes);

    return webPage;
}

void WebProcessProxy::addExistingWebPage(WebPageProxy& webPage, uint64_t pageID, BeginsUsingDataStore beginsUsingDataStore)
{
    ASSERT(!m_pageMap.contains(pageID));
    ASSERT(!globalPageMap().contains(pageID));

    if (beginsUsingDataStore == BeginsUsingDataStore::Yes)
        m_processPool->pageBeginUsingWebsiteDataStore(webPage);

    m_pageMap.set(pageID, &webPage);
    globalPageMap().set(pageID, &webPage);

    updateBackgroundResponsivenessTimer();
}

void WebProcessProxy::markIsNoLongerInPrewarmedPool()
{
    ASSERT(m_isPrewarmed);

    m_isPrewarmed = false;
    RELEASE_ASSERT(m_processPool);
    m_processPool.setIsWeak(IsWeak::No);

    send(Messages::WebProcess::MarkIsNoLongerPrewarmed(), 0);
}

void WebProcessProxy::removeWebPage(WebPageProxy& webPage, uint64_t pageID, EndsUsingDataStore endsUsingDataStore)
{
    auto* removedPage = m_pageMap.take(pageID);
    ASSERT_UNUSED(removedPage, removedPage == &webPage);
    removedPage = globalPageMap().take(pageID);
    ASSERT_UNUSED(removedPage, removedPage == &webPage);

    if (endsUsingDataStore == EndsUsingDataStore::Yes)
        m_processPool->pageEndUsingWebsiteDataStore(webPage);

    updateBackgroundResponsivenessTimer();

    maybeShutDown();
}

void WebProcessProxy::addVisitedLinkStore(VisitedLinkStore& store)
{
    m_visitedLinkStores.add(&store);
    store.addProcess(*this);
}

void WebProcessProxy::addWebUserContentControllerProxy(WebUserContentControllerProxy& proxy, WebPageCreationParameters& parameters)
{
    m_webUserContentControllerProxies.add(&proxy);
    proxy.addProcess(*this, parameters);
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

void WebProcessProxy::assumeReadAccessToBaseURL(WebPageProxy& page, const String& urlString)
{
    URL url(URL(), urlString);
    if (!url.isLocalFile())
        return;

    // There's a chance that urlString does not point to a directory.
    // Get url's base URL to add to m_localPathsWithAssumedReadAccess.
    URL baseURL(URL(), url.baseAsString());
    String path = baseURL.fileSystemPath();
    if (path.isNull())
        return;
    
    // Client loads an alternate string. This doesn't grant universal file read, but the web process is assumed
    // to have read access to this directory already.
    m_localPathsWithAssumedReadAccess.add(path);
    page.addPreviouslyVisitedPath(path);
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
    for (auto& item : WebBackForwardListItem::allItems().values()) {
        URL itemURL(URL(), item->url());
        if (itemURL.isLocalFile() && itemURL.fileSystemPath() == path)
            return true;
        URL itemOriginalURL(URL(), item->originalURL());
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

void WebProcessProxy::updateBackForwardItem(const BackForwardListItemState& itemState)
{
    if (auto* item = WebBackForwardListItem::itemForID(itemState.identifier))
        item->setPageState(itemState.pageState);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessProxy::getPlugins(bool refresh, Vector<PluginInfo>& plugins, Vector<PluginInfo>& applicationPlugins, Optional<Vector<WebCore::SupportedPluginIdentifier>>& supportedPluginIdentifiers)
{
    if (refresh)
        m_processPool->pluginInfoStore().refresh();

    supportedPluginIdentifiers = m_processPool->pluginInfoStore().supportedPluginIdentifiers();

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
void WebProcessProxy::getPluginProcessConnection(uint64_t pluginProcessToken, Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply&& reply)
{
    PluginProcessManager::singleton().getPluginProcessConnection(pluginProcessToken, WTFMove(reply));
}
#endif

void WebProcessProxy::getNetworkProcessConnection(Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&& reply)
{
    m_processPool->getNetworkProcessConnection(*this, WTFMove(reply));
}

#if !PLATFORM(COCOA)
bool WebProcessProxy::platformIsBeingDebugged() const
{
    return false;
}
#endif

#if !PLATFORM(MAC)
bool WebProcessProxy::shouldAllowNonValidInjectedCode() const
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
    RELEASE_LOG_IF(m_websiteDataStore->sessionID().isAlwaysOnLoggingAllowed(), Process, "%p - WebProcessProxy didClose (web process crash)", this);
    processDidTerminateOrFailedToLaunch();
}

void WebProcessProxy::processDidTerminateOrFailedToLaunch()
{
    // Protect ourselves, as the call to disconnect() below may otherwise cause us
    // to be deleted before we can finish our work.
    Ref<WebProcessProxy> protect(*this);

    if (auto* webConnection = this->webConnection())
        webConnection->didClose();

    auto pages = copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values());
    auto provisionalPages = WTF::map(m_provisionalPages, [](auto* provisionalPage) { return makeWeakPtr(provisionalPage); });

    shutDown();

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (pages.size() == 1) {
        auto& page = *pages[0];
        String domain = topPrivatelyControlledDomain(URL({ }, page.currentURL()).host().toString());
        if (!domain.isEmpty())
            page.logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingCrashKey(), domain, WebCore::ShouldSample::No);
    }
#endif

    for (auto& page : pages)
        page->processDidTerminate(ProcessTerminationReason::Crash);

    for (auto& provisionalPage : provisionalPages) {
        if (provisionalPage)
            provisionalPage->processDidTerminate();
    }
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

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);

    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->processDidBecomeUnresponsive();

    bool isWebProcessResponsive = false;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);
}

void WebProcessProxy::didBecomeResponsive()
{
    m_isResponsive = NoOrMaybe::Maybe;

    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->processDidBecomeResponsive();
}

void WebProcessProxy::willChangeIsResponsive()
{
    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->willChangeProcessIsResponsive();
}

void WebProcessProxy::didChangeIsResponsive()
{
    for (auto& page : copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values()))
        page->didChangeProcessIsResponsive();
}

bool WebProcessProxy::mayBecomeUnresponsive()
{
    return !platformIsBeingDebugged();
}

void WebProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    RELEASE_ASSERT(isMainThreadOrCheckDisabled());

    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        RELEASE_LOG_IF(m_websiteDataStore->sessionID().isAlwaysOnLoggingAllowed(), Process, "%p - WebProcessProxy didFinishLaunching - invalid connection identifier (web process failed to launch)", this);
        processDidTerminateOrFailedToLaunch();
        return;
    }

    for (WebPageProxy* page : m_pageMap.values()) {
        ASSERT(this == &page->process());
        page->processDidFinishLaunching();
    }

    for (auto* provisionalPage : m_provisionalPages) {
        ASSERT(this == &provisionalPage->process());
        provisionalPage->processDidFinishLaunching();
    }


    RELEASE_ASSERT(!m_webConnection);
    m_webConnection = WebConnectionToWebProcess::create(this);

    m_processPool->processDidFinishLaunching(this);

#if PLATFORM(IOS_FAMILY)
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

void WebProcessProxy::frameCreated(uint64_t frameID, WebFrameProxy& frameProxy)
{
    m_frameMap.set(frameID, &frameProxy);
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
    for (auto& frame : copyToVector(m_frameMap.values())) {
        if (frame->page() == page)
            frame->webProcessWillShutDown();
    }
}

size_t WebProcessProxy::frameCountInPage(WebPageProxy* page) const
{
    size_t result = 0;
    for (auto& frame : m_frameMap.values()) {
        if (frame->page() == page)
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

bool WebProcessProxy::isResponsive() const
{
    return m_responsivenessTimer.isResponsive() && m_backgroundResponsivenessTimer.isResponsive();
}

void WebProcessProxy::didDestroyUserGestureToken(uint64_t identifier)
{
    ASSERT(UserInitiatedActionMap::isValidKey(identifier));
    m_userInitiatedActionMap.remove(identifier);
}

void WebProcessProxy::maybeShutDown()
{
    if (state() == State::Terminated || !canTerminateChildProcess())
        return;

    shutDown();
}

bool WebProcessProxy::canTerminateChildProcess()
{
    if (!m_pageMap.isEmpty() || m_processPool->hasSuspendedPageFor(*this) || !m_provisionalPages.isEmpty())
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

void WebProcessProxy::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void(WebsiteData)>&& completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is fetching Website data", this);

    connection()->sendWithReply(Messages::WebProcess::FetchWebsiteData(sessionID, dataTypes), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID] (auto reply) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(sessionID);
        UNUSED_PARAM(this);
#endif
        if (!reply) {
            completionHandler(WebsiteData { });
            return;
        }

        completionHandler(WTFMove(std::get<0>(*reply)));
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done fetching Website data", this);
    });
}

void WebProcessProxy::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is deleting Website data", this);

    connection()->sendWithReply(Messages::WebProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID] (auto reply) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(sessionID);
#endif
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done deleting Website data", this);
    });
}

void WebProcessProxy::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(canSendMessage());

    auto token = throttler().backgroundActivityToken();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is taking a background assertion because the Web process is deleting Website data for several origins", this);

    connection()->sendWithReply(Messages::WebProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, origins), 0, RunLoop::main(), [this, token, completionHandler = WTFMove(completionHandler), sessionID] (auto reply) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(sessionID);
#endif
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - WebProcessProxy is releasing a background assertion because the Web process is done deleting Website data for several origins", this);
    });
}

void WebProcessProxy::requestTermination(ProcessTerminationReason reason)
{
    if (state() == State::Terminated)
        return;

    auto protectedThis = makeRef(*this);
    RELEASE_LOG_IF(m_websiteDataStore->sessionID().isAlwaysOnLoggingAllowed(), Process, "%p - WebProcessProxy::requestTermination - reason %d", this, reason);

    ChildProcessProxy::terminate();

    if (webConnection())
        webConnection()->didClose();

    auto pages = copyToVectorOf<RefPtr<WebPageProxy>>(m_pageMap.values());

    shutDown();

    for (auto& page : pages)
        page->processDidTerminate(reason);
}

void WebProcessProxy::stopResponsivenessTimer()
{
    responsivenessTimer().stop();
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

void WebProcessProxy::didSetAssertionState(AssertionState state)
{
#if PLATFORM(IOS_FAMILY)
    if (isServiceWorkerProcess())
        return;

    ASSERT(!m_backgroundToken || !m_foregroundToken);

    switch (state) {
    case AssertionState::Suspended:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Suspended) release all assertions for network process", this);
        m_foregroundToken = nullptr;
        m_backgroundToken = nullptr;
        for (auto& page : m_pageMap.values())
            page->processWillBecomeSuspended();
        break;

    case AssertionState::Background:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Background) taking background assertion for network process", this);
        m_backgroundToken = processPool().backgroundWebProcessToken();
        m_foregroundToken = nullptr;
        break;
    
    case AssertionState::Foreground:
        RELEASE_LOG(ProcessSuspension, "%p - WebProcessProxy::didSetAssertionState(Foreground) taking foreground assertion for network process", this);
        m_foregroundToken = processPool().foregroundWebProcessToken();
        m_backgroundToken = nullptr;
        for (auto& page : m_pageMap.values())
            page->processWillBecomeForeground();
        break;
    }

    ASSERT(!m_backgroundToken || !m_foregroundToken);
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

void WebProcessProxy::isResponsive(WTF::Function<void(bool isWebProcessResponsive)>&& callback)
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
        m_isResponsiveCallbacks.append(WTFMove(callback));

    responsivenessTimer().start();
    send(Messages::WebProcess::MainThreadPing(), 0);
}

bool WebProcessProxy::isJITEnabled() const
{
    return processPool().configuration().isJITEnabled();
}

void WebProcessProxy::didReceiveMainThreadPing()
{
    responsivenessTimer().stop();

    auto isResponsiveCallbacks = WTFMove(m_isResponsiveCallbacks);
    bool isWebProcessResponsive = true;
    for (auto& callback : isResponsiveCallbacks)
        callback(isWebProcessResponsive);
}

void WebProcessProxy::didReceiveBackgroundResponsivenessPing()
{
    m_backgroundResponsivenessTimer.didReceiveBackgroundResponsivenessPong();
}

void WebProcessProxy::processTerminated()
{
    m_responsivenessTimer.processTerminated();
    m_backgroundResponsivenessTimer.processTerminated();
}

void WebProcessProxy::logDiagnosticMessageForResourceLimitTermination(const String& limitKey)
{
    if (pageCount())
        (*pages().begin())->logDiagnosticMessage(DiagnosticLoggingKeys::simulatedPageCrashKey(), limitKey, ShouldSample::No);
}

void WebProcessProxy::didExceedInactiveMemoryLimitWhileActive()
{
    for (auto& page : pages())
        page->didExceedInactiveMemoryLimitWhileActive();
}

void WebProcessProxy::didExceedActiveMemoryLimit()
{
    RELEASE_LOG_ERROR(PerformanceLogging, "%p - WebProcessProxy::didExceedActiveMemoryLimit() Terminating WebProcess with pid %d that has exceeded the active memory limit", this, processIdentifier());
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededActiveMemoryLimitKey());
    requestTermination(ProcessTerminationReason::ExceededMemoryLimit);
}

void WebProcessProxy::didExceedInactiveMemoryLimit()
{
    RELEASE_LOG_ERROR(PerformanceLogging, "%p - WebProcessProxy::didExceedInactiveMemoryLimit() Terminating WebProcess with pid %d that has exceeded the inactive memory limit", this, processIdentifier());
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededInactiveMemoryLimitKey());
    requestTermination(ProcessTerminationReason::ExceededMemoryLimit);
}

void WebProcessProxy::didExceedCPULimit()
{
    for (auto& page : pages()) {
        if (page->isPlayingAudio()) {
            RELEASE_LOG(PerformanceLogging, "%p - WebProcessProxy::didExceedCPULimit() WebProcess with pid %d has exceeded the background CPU limit but we are not terminating it because there is audio playing", this, processIdentifier());
            return;
        }

        if (page->hasActiveAudioStream() || page->hasActiveVideoStream()) {
            RELEASE_LOG(PerformanceLogging, "%p - WebProcessProxy::didExceedCPULimit() WebProcess with pid %d has exceeded the background CPU limit but we are not terminating it because it is capturing audio / video", this, processIdentifier());
            return;
        }
    }

    bool hasVisiblePage = false;
    for (auto& page : pages()) {
        if (page->isViewVisible()) {
            page->didExceedBackgroundCPULimitWhileInForeground();
            hasVisiblePage = true;
        }
    }

    // We only notify the client that the process exceeded the CPU limit when it is visible, we do not terminate it.
    if (hasVisiblePage)
        return;

    RELEASE_LOG_ERROR(PerformanceLogging, "%p - WebProcessProxy::didExceedCPULimit() Terminating background WebProcess with pid %d that has exceeded the background CPU limit", this, processIdentifier());
    logDiagnosticMessageForResourceLimitTermination(DiagnosticLoggingKeys::exceededBackgroundCPULimitKey());
    requestTermination(ProcessTerminationReason::ExceededCPULimit);
}

void WebProcessProxy::updateBackgroundResponsivenessTimer()
{
    m_backgroundResponsivenessTimer.updateState();
}

#if !PLATFORM(COCOA)
const HashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<HashSet<String>> platformPathsWithAssumedReadAccess;
    return platformPathsWithAssumedReadAccess;
}
#endif

void WebProcessProxy::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    m_processEntangledPorts.add(port1);
    m_processEntangledPorts.add(port2);
    UIMessagePortChannelProvider::singleton().registry().didCreateMessagePortChannel(port1, port2);
}

void WebProcessProxy::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    m_processEntangledPorts.add(local);
    UIMessagePortChannelProvider::singleton().registry().didEntangleLocalToRemote(local, remote, coreProcessIdentifier());

    auto* channel = UIMessagePortChannelProvider::singleton().registry().existingChannelContainingPort(local);
    if (channel && channel->hasAnyMessagesPendingOrInFlight())
        send(Messages::WebProcess::MessagesAvailableForPort(local), 0);
}

void WebProcessProxy::messagePortDisentangled(const MessagePortIdentifier& port)
{
    auto result = m_processEntangledPorts.remove(port);
    ASSERT_UNUSED(result, result);

    UIMessagePortChannelProvider::singleton().registry().didDisentangleMessagePort(port);
}

void WebProcessProxy::messagePortClosed(const MessagePortIdentifier& port)
{
    UIMessagePortChannelProvider::singleton().registry().didCloseMessagePort(port);
}

void WebProcessProxy::takeAllMessagesForPort(const MessagePortIdentifier& port, uint64_t messagesCallbackIdentifier)
{
    UIMessagePortChannelProvider::singleton().registry().takeAllMessagesForPort(port, [this, protectedThis = makeRef(*this), messagesCallbackIdentifier](Vector<MessageWithMessagePorts>&& messages, Function<void()>&& deliveryCallback) {

        static uint64_t currentMessageBatchIdentifier;
        auto result = m_messageBatchDeliveryCompletionHandlers.ensure(++currentMessageBatchIdentifier, [deliveryCallback = WTFMove(deliveryCallback)]() mutable {
            return WTFMove(deliveryCallback);
        });
        ASSERT_UNUSED(result, result.isNewEntry);

        send(Messages::WebProcess::DidTakeAllMessagesForPort(WTFMove(messages), messagesCallbackIdentifier, currentMessageBatchIdentifier), 0);
    });
}

void WebProcessProxy::didDeliverMessagePortMessages(uint64_t messageBatchIdentifier)
{
    auto callback = m_messageBatchDeliveryCompletionHandlers.take(messageBatchIdentifier);
    ASSERT(callback);
    callback();
}

void WebProcessProxy::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& port)
{
    if (UIMessagePortChannelProvider::singleton().registry().didPostMessageToRemote(WTFMove(message), port)) {
        // Look up the process for that port
        auto* channel = UIMessagePortChannelProvider::singleton().registry().existingChannelContainingPort(port);
        ASSERT(channel);
        auto processIdentifier = channel->processForPort(port);
        if (processIdentifier) {
            if (auto* process = WebProcessProxy::processForIdentifier(*processIdentifier))
                process->send(Messages::WebProcess::MessagesAvailableForPort(port), 0);
        }
    }
}

void WebProcessProxy::checkRemotePortForActivity(const WebCore::MessagePortIdentifier port, uint64_t callbackIdentifier)
{
    UIMessagePortChannelProvider::singleton().registry().checkRemotePortForActivity(port, [this, protectedThis = makeRef(*this), callbackIdentifier](MessagePortChannelProvider::HasActivity hasActivity) {
        send(Messages::WebProcess::DidCheckRemotePortForActivity(callbackIdentifier, hasActivity == MessagePortChannelProvider::HasActivity::Yes), 0);
    });
}

void WebProcessProxy::checkProcessLocalPortForActivity(const MessagePortIdentifier& port, CompletionHandler<void(MessagePortChannelProvider::HasActivity)>&& callback)
{
    static uint64_t currentCallbackIdentifier;
    auto result = m_localPortActivityCompletionHandlers.ensure(++currentCallbackIdentifier, [callback = WTFMove(callback)]() mutable {
        return WTFMove(callback);
    });
    ASSERT_UNUSED(result, result.isNewEntry);

    send(Messages::WebProcess::CheckProcessLocalPortForActivity(port, currentCallbackIdentifier), 0);
}

void WebProcessProxy::didCheckProcessLocalPortForActivity(uint64_t callbackIdentifier, bool isLocallyReachable)
{
    auto callback = m_localPortActivityCompletionHandlers.take(callbackIdentifier);
    if (!callback)
        return;

    callback(isLocallyReachable ? MessagePortChannelProvider::HasActivity::Yes : MessagePortChannelProvider::HasActivity::No);
}

void WebProcessProxy::didCollectPrewarmInformation(const String& domain, const WebCore::PrewarmInformation& prewarmInformation)
{
    processPool().didCollectPrewarmInformation(domain, prewarmInformation);
}

void WebProcessProxy::activePagesDomainsForTesting(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    connection()->sendWithAsyncReply(Messages::WebProcess::GetActivePagesOriginsForTesting(), WTFMove(completionHandler));
}

#if PLATFORM(WATCHOS)

void WebProcessProxy::takeBackgroundActivityTokenForFullscreenInput()
{
    if (m_backgroundActivityTokenForFullscreenFormControls)
        return;

    m_backgroundActivityTokenForFullscreenFormControls = m_throttler.backgroundActivityToken();
    RELEASE_LOG(ProcessSuspension, "UIProcess is taking a background assertion because it is presenting fullscreen UI for form controls.");
}

void WebProcessProxy::releaseBackgroundActivityTokenForFullscreenInput()
{
    if (!m_backgroundActivityTokenForFullscreenFormControls)
        return;

    m_backgroundActivityTokenForFullscreenFormControls = nullptr;
    RELEASE_LOG(ProcessSuspension, "UIProcess is releasing a background assertion because it has dismissed fullscreen UI for form controls.");
}

#endif

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_URL
