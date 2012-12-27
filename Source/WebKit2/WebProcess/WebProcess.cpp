/*
 * Copyright (C) 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "WebProcess.h"

#include "AuthenticationManager.h"
#include "InjectedBundle.h"
#include "InjectedBundleUserMessageCoders.h"
#include "Logging.h"
#include "StatisticsData.h"
#include "WebApplicationCacheManager.h"
#include "WebConnectionToUIProcess.h"
#include "WebContextMessages.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebGeolocationManager.h"
#include "WebIconDatabaseProxy.h"
#include "WebKeyValueStorageManager.h"
#include "WebMediaCacheManager.h"
#include "WebMemorySampler.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPlatformStrategies.h"
#include "WebPreferencesStore.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebResourceCacheManager.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/Font.h>
#include <WebCore/FontCache.h>
#include <WebCore/Frame.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPageTreeNode.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/InitializeLogging.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/Language.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryPressureHandler.h>
#include <WebCore/Page.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageGroup.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RunLoop.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageTracker.h>
#include <wtf/HashCountedSet.h>
#include <wtf/PassRefPtr.h>

#if ENABLE(WEB_INTENTS)
#include <WebCore/PlatformMessagePortChannel.h>
#endif

#if ENABLE(NETWORK_INFO)
#include "WebNetworkInfoManagerMessages.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessConnection.h"
#endif

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if !ENABLE(PLUGIN_PROCESS)
#include "NetscapePluginModule.h"
#endif

#if ENABLE(CUSTOM_PROTOCOLS)
#include "CustomProtocolManager.h"
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

#if ENABLE(SQL_DATABASE)
#include "WebDatabaseManager.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "WebResourceLoadScheduler.h"
#endif

#if ENABLE(PLUGIN_PROCESS)
#include "PluginProcessConnectionManager.h"
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

WebProcess& WebProcess::shared()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_inDidClose(false)
    , m_shouldTrackVisitedLinks(true)
    , m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
    , m_compositingRenderServerPort(MACH_PORT_NULL)
#endif
#if PLATFORM(MAC)
    , m_clearResourceCachesDispatchGroup(0)
#endif
    , m_fullKeyboardAccessEnabled(false)
#if PLATFORM(QT)
    , m_networkAccessManager(0)
#endif
    , m_textCheckerState()
    , m_geolocationManager(new WebGeolocationManager(this))
    , m_applicationCacheManager(new WebApplicationCacheManager(this))
    , m_resourceCacheManager(new WebResourceCacheManager(this))
    , m_cookieManager(new WebCookieManager(this))
    , m_keyValueStorageManager(new WebKeyValueStorageManager(this))
    , m_mediaCacheManager(new WebMediaCacheManager(this))
    , m_authenticationManager(new AuthenticationManager(this))
#if ENABLE(SQL_DATABASE)
    , m_databaseManager(new WebDatabaseManager(this))
#endif
#if ENABLE(BATTERY_STATUS)
    , m_batteryManager(this)
#endif
#if ENABLE(NETWORK_INFO)
    , m_networkInfoManager(this)
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    , m_notificationManager(new WebNotificationManager(this))
#endif
    , m_iconDatabaseProxy(new WebIconDatabaseProxy(this))
#if ENABLE(NETWORK_PROCESS)
    , m_usesNetworkProcess(false)
    , m_webResourceLoadScheduler(new WebResourceLoadScheduler)
#endif
#if ENABLE(PLUGIN_PROCESS)
    , m_pluginProcessConnectionManager(new PluginProcessConnectionManager)
#endif
#if USE(SOUP)
    , m_soupRequestManager(this)
#endif
{
#if USE(PLATFORM_STRATEGIES)
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();
#endif // USE(PLATFORM_STRATEGIES)

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
    WebKit::initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED


#if ENABLE(CUSTOM_PROTOCOLS)
    CustomProtocolManager::shared().initialize(this);
#endif
}

void WebProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, RunLoop* runLoop)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, runLoop);
    m_connection->setDidCloseOnConnectionWorkQueueCallback(ChildProcess::didCloseOnConnectionWorkQueue);
    m_connection->setShouldExitOnSyncMessageSendFailure(true);
    m_connection->addQueueClient(&m_eventDispatcher);
    m_connection->addQueueClient(this);
    m_connection->open();

    m_webConnection = WebConnectionToUIProcess::create(this);

    m_runLoop = runLoop;
}

void WebProcess::didCreateDownload()
{
    disableTermination();
}

void WebProcess::didDestroyDownload()
{
    enableTermination();
}

CoreIPC::Connection* WebProcess::downloadProxyConnection()
{
    return m_connection.get();
}

AuthenticationManager& WebProcess::downloadsAuthenticationManager()
{
    return *m_authenticationManager;
}

void WebProcess::initializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::MessageDecoder& decoder)
{
    ASSERT(m_pageMap.isEmpty());

    platformInitializeWebProcess(parameters, decoder);

    memoryPressureHandler().install();

    RefPtr<APIObject> injectedBundleInitializationUserData;
    InjectedBundleUserMessageDecoder messageDecoder(injectedBundleInitializationUserData);
    if (!decoder.decode(messageDecoder))
        return;

    if (!parameters.injectedBundlePath.isEmpty()) {
        m_injectedBundle = InjectedBundle::create(parameters.injectedBundlePath);
        m_injectedBundle->setSandboxExtension(SandboxExtension::create(parameters.injectedBundlePathExtensionHandle));

        if (!m_injectedBundle->load(injectedBundleInitializationUserData.get())) {
            // Don't keep around the InjectedBundle reference if the load fails.
            m_injectedBundle.clear();
        }
    }

#if ENABLE(SQL_DATABASE)
    m_databaseManager->initialize(parameters);
#endif

#if ENABLE(ICONDATABASE)
    m_iconDatabaseProxy->setEnabled(parameters.iconDatabaseEnabled);
#endif

    m_keyValueStorageManager->initialize(parameters);

    if (!parameters.applicationCacheDirectory.isEmpty())
        cacheStorage().setCacheDirectory(parameters.applicationCacheDirectory);

    setShouldTrackVisitedLinks(parameters.shouldTrackVisitedLinks);
    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    if (!parameters.languages.isEmpty())
        overrideUserPreferredLanguages(parameters.languages);

    m_textCheckerState = parameters.textCheckerState;

    m_fullKeyboardAccessEnabled = parameters.fullKeyboardAccessEnabled;

    for (size_t i = 0; i < parameters.urlSchemesRegistererdAsEmptyDocument.size(); ++i)
        registerURLSchemeAsEmptyDocument(parameters.urlSchemesRegistererdAsEmptyDocument[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsSecure.size(); ++i)
        registerURLSchemeAsSecure(parameters.urlSchemesRegisteredAsSecure[i]);

    for (size_t i = 0; i < parameters.urlSchemesForWhichDomainRelaxationIsForbidden.size(); ++i)
        setDomainRelaxationForbiddenForURLScheme(parameters.urlSchemesForWhichDomainRelaxationIsForbidden[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsLocal.size(); ++i)
        registerURLSchemeAsLocal(parameters.urlSchemesRegisteredAsLocal[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsNoAccess.size(); ++i)
        registerURLSchemeAsNoAccess(parameters.urlSchemesRegisteredAsNoAccess[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsDisplayIsolated.size(); ++i)
        registerURLSchemeAsDisplayIsolated(parameters.urlSchemesRegisteredAsDisplayIsolated[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsCORSEnabled.size(); ++i)
        registerURLSchemeAsCORSEnabled(parameters.urlSchemesRegisteredAsCORSEnabled[i]);

    setDefaultRequestTimeoutInterval(parameters.defaultRequestTimeoutInterval);

    if (parameters.shouldAlwaysUseComplexTextCodePath)
        setAlwaysUsesComplexTextCodePath(true);

    if (parameters.shouldUseFontSmoothing)
        setShouldUseFontSmoothing(true);

#if (PLATFORM(MAC) || USE(CFNETWORK)) && !PLATFORM(WIN)
    WebFrameNetworkingContext::setPrivateBrowsingStorageSessionIdentifierBase(parameters.uiProcessBundleIdentifier);
#endif

#if ENABLE(NETWORK_PROCESS)
    m_usesNetworkProcess = parameters.usesNetworkProcess;
    ensureNetworkProcessConnection();
#endif
    setTerminationTimeout(parameters.terminationTimeout);

    for (size_t i = 0; i < parameters.plugInAutoStartOrigins.size(); ++i)
        didAddPlugInAutoStartOrigin(parameters.plugInAutoStartOrigins[i]);

#if ENABLE(CUSTOM_PROTOCOLS)
    initializeCustomProtocolManager(parameters);
#endif
}

#if ENABLE(NETWORK_PROCESS)
void WebProcess::ensureNetworkProcessConnection()
{
    if (!m_usesNetworkProcess)
        return;

    if (m_networkProcessConnection)
        return;

    CoreIPC::Attachment encodedConnectionIdentifier;

    if (!connection()->sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(),
        Messages::WebProcessProxy::GetNetworkProcessConnection::Reply(encodedConnectionIdentifier), 0))
        return;

#if PLATFORM(MAC)
    CoreIPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
    if (CoreIPC::Connection::identifierIsNull(connectionIdentifier))
        return;
#else
    ASSERT_NOT_REACHED();
#endif
    m_networkProcessConnection = NetworkProcessConnection::create(connectionIdentifier);
}
#endif // ENABLE(NETWORK_PROCESS)

void WebProcess::setShouldTrackVisitedLinks(bool shouldTrackVisitedLinks)
{
    m_shouldTrackVisitedLinks = shouldTrackVisitedLinks;
    PageGroup::setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
}

void WebProcess::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    SchemeRegistry::registerURLSchemeAsEmptyDocument(urlScheme);
}

void WebProcess::registerURLSchemeAsSecure(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsSecure(urlScheme);
}

void WebProcess::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme) const
{
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(true, urlScheme);
}

void WebProcess::registerURLSchemeAsLocal(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsLocal(urlScheme);
}

void WebProcess::registerURLSchemeAsNoAccess(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsNoAccess(urlScheme);
}

void WebProcess::registerURLSchemeAsDisplayIsolated(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsDisplayIsolated(urlScheme);
}

void WebProcess::registerURLSchemeAsCORSEnabled(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsCORSEnabled(urlScheme);
}

void WebProcess::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    ResourceRequest::setDefaultTimeoutInterval(timeoutInterval);
}

void WebProcess::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    WebCore::Font::setCodePath(alwaysUseComplexText ? WebCore::Font::Complex : WebCore::Font::Auto);
}

void WebProcess::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    WebCore::Font::setShouldUseSmoothing(useFontSmoothing);
}

void WebProcess::userPreferredLanguagesChanged(const Vector<String>& languages) const
{
    overrideUserPreferredLanguages(languages);
    languageDidChange();
}

void WebProcess::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    m_fullKeyboardAccessEnabled = fullKeyboardAccessEnabled;
}

void WebProcess::ensurePrivateBrowsingSession()
{
#if (PLATFORM(MAC) || USE(CFNETWORK)) && !PLATFORM(WIN)
    WebFrameNetworkingContext::ensurePrivateBrowsingSession();
#endif
}

void WebProcess::destroyPrivateBrowsingSession()
{
#if (PLATFORM(MAC) || USE(CFNETWORK)) && !PLATFORM(WIN)
    WebFrameNetworkingContext::destroyPrivateBrowsingSession();
#endif
}

WebGeolocationManager& WebProcess::geolocationManager()
{
    return *m_geolocationManager;
}

WebApplicationCacheManager& WebProcess::applicationCacheManager()
{
    return *m_applicationCacheManager;
}

WebResourceCacheManager& WebProcess::resourceCacheManager()
{
    return *m_resourceCacheManager;
}

WebCookieManager& WebProcess::cookieManager()
{
    return *m_cookieManager;
}

WebKeyValueStorageManager& WebProcess::keyValueStorageManager()
{
    return *m_keyValueStorageManager;
}

DownloadManager& WebProcess::downloadManager()
{
#if ENABLE(NETWORK_PROCESS)
    ASSERT(!m_usesNetworkProcess);
#endif

    DEFINE_STATIC_LOCAL(DownloadManager, downloadManager, (this));
    return downloadManager;
}

AuthenticationManager& WebProcess::authenticationManager()
{
    return *m_authenticationManager;
}

#if ENABLE(SQL_DATABASE)
WebDatabaseManager& WebProcess::databaseManager()
{
    return *m_databaseManager;
}
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
WebNotificationManager& WebProcess::notificationManager()
{
    return *m_notificationManager;
}
#endif

#if ENABLE(PLUGIN_PROCESS)
PluginProcessConnectionManager& WebProcess::pluginProcessConnectionManager()
{
    return *m_pluginProcessConnectionManager;
}
#endif

void WebProcess::setVisitedLinkTable(const SharedMemory::Handle& handle)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(handle, SharedMemory::ReadOnly);
    if (!sharedMemory)
        return;

    m_visitedLinkTable.setSharedMemory(sharedMemory.release());
}

void WebProcess::visitedLinkStateChanged(const Vector<WebCore::LinkHash>& linkHashes)
{
    // FIXME: We may want to track visited links per WebPageGroup rather than per WebContext.
    for (size_t i = 0; i < linkHashes.size(); ++i) {
        HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator it = m_pageGroupMap.begin();
        HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator end = m_pageGroupMap.end();
        for (; it != end; ++it)
            Page::visitedStateChanged(PageGroup::pageGroup(it->value->identifier()), linkHashes[i]);
    }

    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void WebProcess::allVisitedLinkStateChanged()
{
    // FIXME: We may want to track visited links per WebPageGroup rather than per WebContext.
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator it = m_pageGroupMap.begin();
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator end = m_pageGroupMap.end();
    for (; it != end; ++it)
        Page::allVisitedStateChanged(PageGroup::pageGroup(it->value->identifier()));

    pageCache()->markPagesForVistedLinkStyleRecalc();
}

bool WebProcess::isLinkVisited(LinkHash linkHash) const
{
    return m_visitedLinkTable.isLinkVisited(linkHash);
}

void WebProcess::addVisitedLink(WebCore::LinkHash linkHash)
{
    if (isLinkVisited(linkHash) || !m_shouldTrackVisitedLinks)
        return;
    connection()->send(Messages::WebContext::AddVisitedLinkHash(linkHash), 0);
}

void WebProcess::setCacheModel(uint32_t cm)
{
    CacheModel cacheModel = static_cast<CacheModel>(cm);

    if (!m_hasSetCacheModel || cacheModel != m_cacheModel) {
        m_hasSetCacheModel = true;
        m_cacheModel = cacheModel;
        platformSetCacheModel(cacheModel);
    }
}

WebPage* WebProcess::focusedWebPage() const
{    
    HashMap<uint64_t, RefPtr<WebPage> >::const_iterator end = m_pageMap.end();
    for (HashMap<uint64_t, RefPtr<WebPage> >::const_iterator it = m_pageMap.begin(); it != end; ++it) {
        WebPage* page = (*it).value.get();
        if (page->windowAndWebPageAreFocused())
            return page;
    }
    return 0;
}
    
WebPage* WebProcess::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID).get();
}

void WebProcess::createWebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way. 
    HashMap<uint64_t, RefPtr<WebPage> >::AddResult result = m_pageMap.add(pageID, 0);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPage::create(pageID, parameters);

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
    }

    ASSERT(result.iterator->value);
}

void WebProcess::removeWebPage(uint64_t pageID)
{
    ASSERT(m_pageMap.contains(pageID));

    m_pageMap.remove(pageID);

    enableTermination();
}

bool WebProcess::isSeparateProcess() const
{
    // If we're running on the main run loop, we assume that we're in a separate process.
    return m_runLoop == RunLoop::main();
}
 
bool WebProcess::shouldTerminate()
{
    // Keep running forever if we're running in the same process.
    if (!isSeparateProcess())
        return false;

    ASSERT(m_pageMap.isEmpty());

#if ENABLE(NETWORK_PROCESS)
    ASSERT(m_usesNetworkProcess || !downloadManager().isDownloading());
#else
    ASSERT(!downloadManager().isDownloading());
#endif

    // FIXME: the ShouldTerminate message should also send termination parameters, such as any session cookies that need to be preserved.
    bool shouldTerminate = false;
    if (connection()->sendSync(Messages::WebProcessProxy::ShouldTerminate(), Messages::WebProcessProxy::ShouldTerminate::Reply(shouldTerminate), 0)
        && !shouldTerminate)
        return false;

    return true;
}

void WebProcess::terminate()
{
#ifndef NDEBUG
    gcController().garbageCollectNow();
    memoryCache()->setDisabled(true);
#endif

    // Invalidate our connection.
    m_connection->invalidate();
    m_connection = nullptr;

    m_webConnection->invalidate();
    m_webConnection = nullptr;

    platformTerminate();
    m_runLoop->stop();
}

void WebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    m_messageReceiverMap.dispatchSyncMessage(connection, messageID, decoder, replyEncoder);
}

void WebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (m_messageReceiverMap.dispatchMessage(connection, messageID, decoder))
        return;

    if (messageID.is<CoreIPC::MessageClassWebProcess>()) {
        didReceiveWebProcessMessage(connection, messageID, decoder);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebPageGroupProxy>()) {
        uint64_t pageGroupID = decoder.destinationID();
        if (!pageGroupID)
            return;
        
        WebPageGroupProxy* pageGroupProxy = webPageGroup(pageGroupID);
        if (!pageGroupProxy)
            return;
        
        pageGroupProxy->didReceiveMessage(connection, messageID, decoder);
    }
}

void WebProcess::didClose(CoreIPC::Connection*)
{
    // When running in the same process the connection will never be closed.
    ASSERT(isSeparateProcess());

#ifndef NDEBUG
    m_inDidClose = true;

    // Close all the live pages.
    Vector<RefPtr<WebPage> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0; i < pages.size(); ++i)
        pages[i]->close();
    pages.clear();

    gcController().garbageCollectSoon();
    memoryCache()->setDisabled(true);
#endif    

    // The UI process closed this connection, shut down.
    m_runLoop->stop();
}

void WebProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
    // We received an invalid message, but since this is from the UI process (which we trust),
    // we'll let it slide.
}

void WebProcess::didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, bool& didHandleMessage)
{
    if (messageID.is<CoreIPC::MessageClassWebProcess>()) {
        didReceiveWebProcessMessageOnConnectionWorkQueue(connection, messageID, decoder, didHandleMessage);
        return;
    }
}

WebFrame* WebProcess::webFrame(uint64_t frameID) const
{
    return m_frameMap.get(frameID);
}

void WebProcess::addWebFrame(uint64_t frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebProcess::removeWebFrame(uint64_t frameID)
{
    m_frameMap.remove(frameID);

    // We can end up here after our connection has closed when WebCore's frame life-support timer
    // fires when the application is shutting down. There's no need (and no way) to update the UI
    // process in this case.
    if (!m_connection)
        return;

    connection()->send(Messages::WebProcessProxy::DidDestroyFrame(frameID), 0);
}

WebPageGroupProxy* WebProcess::webPageGroup(uint64_t pageGroupID)
{
    return m_pageGroupMap.get(pageGroupID).get();
}

WebPageGroupProxy* WebProcess::webPageGroup(const WebPageGroupData& pageGroupData)
{
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::AddResult result = m_pageGroupMap.add(pageGroupData.pageGroupID, 0);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPageGroupProxy::create(pageGroupData);
    }

    return result.iterator->value.get();
}

#if ENABLE(WEB_INTENTS)
uint64_t WebProcess::addMessagePortChannel(PassRefPtr<PlatformMessagePortChannel> messagePortChannel)
{
    static uint64_t channelID = 0;
    m_messagePortChannels.add(++channelID, messagePortChannel);

    return channelID;
}

PlatformMessagePortChannel* WebProcess::messagePortChannel(uint64_t channelID)
{
    return m_messagePortChannels.get(channelID).get();
}

void WebProcess::removeMessagePortChannel(uint64_t channelID)
{
    m_messagePortChannels.remove(channelID);
}
#endif

void WebProcess::clearResourceCaches(ResourceCachesToClear resourceCachesToClear)
{
    platformClearResourceCaches(resourceCachesToClear);

    // Toggling the cache model like this forces the cache to evict all its in-memory resources.
    // FIXME: We need a better way to do this.
    CacheModel cacheModel = m_cacheModel;
    setCacheModel(CacheModelDocumentViewer);
    setCacheModel(cacheModel);

    memoryCache()->evictResources();

    // Empty the cross-origin preflight cache.
    CrossOriginPreflightResultCache::shared().empty();
}

void WebProcess::clearApplicationCache()
{
    // Empty the application cache.
    cacheStorage().empty();
}

#if ENABLE(NETSCAPE_PLUGIN_API) && !ENABLE(PLUGIN_PROCESS)
void WebProcess::getSitesWithPluginData(const Vector<String>& pluginPaths, uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

    HashSet<String> sitesSet;

#if ENABLE(NETSCAPE_PLUGIN_API)
    for (size_t i = 0; i < pluginPaths.size(); ++i) {
        RefPtr<NetscapePluginModule> netscapePluginModule = NetscapePluginModule::getOrCreate(pluginPaths[i]);
        if (!netscapePluginModule)
            continue;

        Vector<String> sites = netscapePluginModule->sitesWithData();
        for (size_t i = 0; i < sites.size(); ++i)
            sitesSet.add(sites[i]);
    }
#else
    UNUSED_PARAM(pluginPaths);
#endif

    Vector<String> sites;
    copyToVector(sitesSet, sites);

    connection()->send(Messages::WebProcessProxy::DidGetSitesWithPluginData(sites, callbackID), 0);
}

void WebProcess::clearPluginSiteData(const Vector<String>& pluginPaths, const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

#if ENABLE(NETSCAPE_PLUGIN_API)
    for (size_t i = 0; i < pluginPaths.size(); ++i) {
        RefPtr<NetscapePluginModule> netscapePluginModule = NetscapePluginModule::getOrCreate(pluginPaths[i]);
        if (!netscapePluginModule)
            continue;

        if (sites.isEmpty()) {
            // Clear everything.
            netscapePluginModule->clearSiteData(String(), flags, maxAgeInSeconds);
            continue;
        }

        for (size_t i = 0; i < sites.size(); ++i)
            netscapePluginModule->clearSiteData(sites[i], flags, maxAgeInSeconds);
    }
#else
    UNUSED_PARAM(pluginPaths);
    UNUSED_PARAM(sites);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(maxAgeInSeconds);
#endif

    connection()->send(Messages::WebProcessProxy::DidClearPluginSiteData(callbackID), 0);
}
#endif

bool WebProcess::isPlugInAutoStartOrigin(unsigned plugInOriginHash)
{
    return m_plugInAutoStartOrigins.contains(plugInOriginHash);
}

void WebProcess::addPlugInAutoStartOrigin(const String& pageOrigin, unsigned plugInOriginHash)
{
    if (isPlugInAutoStartOrigin(plugInOriginHash)) {
        LOG(Plugins, "Hash %x already exists as auto-start origin (request for %s)", plugInOriginHash, pageOrigin.utf8().data());
        return;
    }

    connection()->send(Messages::WebContext::AddPlugInAutoStartOriginHash(pageOrigin, plugInOriginHash), 0);
}

void WebProcess::didAddPlugInAutoStartOrigin(unsigned plugInOriginHash)
{
    m_plugInAutoStartOrigins.add(plugInOriginHash);
}

void WebProcess::plugInAutoStartOriginsChanged(const Vector<unsigned>& hashes)
{
    m_plugInAutoStartOrigins.clear();
    for (size_t i = 0; i < hashes.size(); ++i)
        didAddPlugInAutoStartOrigin(hashes[i]);
}

static void fromCountedSetToHashMap(TypeCountSet* countedSet, HashMap<String, uint64_t>& map)
{
    TypeCountSet::const_iterator end = countedSet->end();
    for (TypeCountSet::const_iterator it = countedSet->begin(); it != end; ++it)
        map.set(it->key, it->value);
}

static void getWebCoreMemoryCacheStatistics(Vector<HashMap<String, uint64_t> >& result)
{
    DEFINE_STATIC_LOCAL(String, imagesString, (ASCIILiteral("Images")));
    DEFINE_STATIC_LOCAL(String, cssString, (ASCIILiteral("CSS")));
    DEFINE_STATIC_LOCAL(String, xslString, (ASCIILiteral("XSL")));
    DEFINE_STATIC_LOCAL(String, javaScriptString, (ASCIILiteral("JavaScript")));
    
    MemoryCache::Statistics memoryCacheStatistics = memoryCache()->getStatistics();
    
    HashMap<String, uint64_t> counts;
    counts.set(imagesString, memoryCacheStatistics.images.count);
    counts.set(cssString, memoryCacheStatistics.cssStyleSheets.count);
    counts.set(xslString, memoryCacheStatistics.xslStyleSheets.count);
    counts.set(javaScriptString, memoryCacheStatistics.scripts.count);
    result.append(counts);
    
    HashMap<String, uint64_t> sizes;
    sizes.set(imagesString, memoryCacheStatistics.images.size);
    sizes.set(cssString, memoryCacheStatistics.cssStyleSheets.size);
    sizes.set(xslString, memoryCacheStatistics.xslStyleSheets.size);
    sizes.set(javaScriptString, memoryCacheStatistics.scripts.size);
    result.append(sizes);
    
    HashMap<String, uint64_t> liveSizes;
    liveSizes.set(imagesString, memoryCacheStatistics.images.liveSize);
    liveSizes.set(cssString, memoryCacheStatistics.cssStyleSheets.liveSize);
    liveSizes.set(xslString, memoryCacheStatistics.xslStyleSheets.liveSize);
    liveSizes.set(javaScriptString, memoryCacheStatistics.scripts.liveSize);
    result.append(liveSizes);
    
    HashMap<String, uint64_t> decodedSizes;
    decodedSizes.set(imagesString, memoryCacheStatistics.images.decodedSize);
    decodedSizes.set(cssString, memoryCacheStatistics.cssStyleSheets.decodedSize);
    decodedSizes.set(xslString, memoryCacheStatistics.xslStyleSheets.decodedSize);
    decodedSizes.set(javaScriptString, memoryCacheStatistics.scripts.decodedSize);
    result.append(decodedSizes);
    
    HashMap<String, uint64_t> purgeableSizes;
    purgeableSizes.set(imagesString, memoryCacheStatistics.images.purgeableSize);
    purgeableSizes.set(cssString, memoryCacheStatistics.cssStyleSheets.purgeableSize);
    purgeableSizes.set(xslString, memoryCacheStatistics.xslStyleSheets.purgeableSize);
    purgeableSizes.set(javaScriptString, memoryCacheStatistics.scripts.purgeableSize);
    result.append(purgeableSizes);
    
    HashMap<String, uint64_t> purgedSizes;
    purgedSizes.set(imagesString, memoryCacheStatistics.images.purgedSize);
    purgedSizes.set(cssString, memoryCacheStatistics.cssStyleSheets.purgedSize);
    purgedSizes.set(xslString, memoryCacheStatistics.xslStyleSheets.purgedSize);
    purgedSizes.set(javaScriptString, memoryCacheStatistics.scripts.purgedSize);
    result.append(purgedSizes);
}

void WebProcess::getWebCoreStatistics(uint64_t callbackID)
{
    StatisticsData data;
    
    // Gather JavaScript statistics.
    {
        JSLockHolder lock(JSDOMWindow::commonJSGlobalData());
        data.statisticsNumbers.set("JavaScriptObjectsCount", JSDOMWindow::commonJSGlobalData()->heap.objectCount());
        data.statisticsNumbers.set("JavaScriptGlobalObjectsCount", JSDOMWindow::commonJSGlobalData()->heap.globalObjectCount());
        data.statisticsNumbers.set("JavaScriptProtectedObjectsCount", JSDOMWindow::commonJSGlobalData()->heap.protectedObjectCount());
        data.statisticsNumbers.set("JavaScriptProtectedGlobalObjectsCount", JSDOMWindow::commonJSGlobalData()->heap.protectedGlobalObjectCount());
        
        OwnPtr<TypeCountSet> protectedObjectTypeCounts(JSDOMWindow::commonJSGlobalData()->heap.protectedObjectTypeCounts());
        fromCountedSetToHashMap(protectedObjectTypeCounts.get(), data.javaScriptProtectedObjectTypeCounts);
        
        OwnPtr<TypeCountSet> objectTypeCounts(JSDOMWindow::commonJSGlobalData()->heap.objectTypeCounts());
        fromCountedSetToHashMap(objectTypeCounts.get(), data.javaScriptObjectTypeCounts);
        
        uint64_t javaScriptHeapSize = JSDOMWindow::commonJSGlobalData()->heap.size();
        data.statisticsNumbers.set("JavaScriptHeapSize", javaScriptHeapSize);
        data.statisticsNumbers.set("JavaScriptFreeSize", JSDOMWindow::commonJSGlobalData()->heap.capacity() - javaScriptHeapSize);
    }

    WTF::FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();
    data.statisticsNumbers.set("FastMallocReservedVMBytes", fastMallocStatistics.reservedVMBytes);
    data.statisticsNumbers.set("FastMallocCommittedVMBytes", fastMallocStatistics.committedVMBytes);
    data.statisticsNumbers.set("FastMallocFreeListBytes", fastMallocStatistics.freeListBytes);
    
    // Gather icon statistics.
    data.statisticsNumbers.set("IconPageURLMappingCount", iconDatabase().pageURLMappingCount());
    data.statisticsNumbers.set("IconRetainedPageURLCount", iconDatabase().retainedPageURLCount());
    data.statisticsNumbers.set("IconRecordCount", iconDatabase().iconRecordCount());
    data.statisticsNumbers.set("IconsWithDataCount", iconDatabase().iconRecordCountWithData());
    
    // Gather font statistics.
    data.statisticsNumbers.set("CachedFontDataCount", fontCache()->fontDataCount());
    data.statisticsNumbers.set("CachedFontDataInactiveCount", fontCache()->inactiveFontDataCount());
    
    // Gather glyph page statistics.
    data.statisticsNumbers.set("GlyphPageCount", GlyphPageTreeNode::treeGlyphPageCount());
    
    // Get WebCore memory cache statistics
    getWebCoreMemoryCacheStatistics(data.webCoreCacheStatistics);
    
    connection()->send(Messages::WebContext::DidGetWebCoreStatistics(data, callbackID), 0);
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    gcController().garbageCollectNow();
}

void WebProcess::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    gcController().setJavaScriptGarbageCollectorTimerEnabled(flag);
}

void WebProcess::postInjectedBundleMessage(const CoreIPC::DataReference& messageData)
{
    InjectedBundle* injectedBundle = WebProcess::shared().injectedBundle();
    if (!injectedBundle)
        return;

    OwnPtr<CoreIPC::ArgumentDecoder> decoder = CoreIPC::ArgumentDecoder::create(messageData.data(), messageData.size());

    String messageName;
    if (!decoder->decode(messageName))
        return;

    RefPtr<APIObject> messageBody;
    InjectedBundleUserMessageDecoder messageBodyDecoder(messageBody);
    if (!decoder->decode(messageBodyDecoder))
        return;

    injectedBundle->didReceiveMessage(messageName, messageBody.get());
}

#if ENABLE(NETWORK_PROCESS)
NetworkProcessConnection* WebProcess::networkConnection()
{
    ASSERT(m_usesNetworkProcess);

    // If we've lost our connection to the network process (e.g. it crashed) try to re-establish it.
    if (!m_networkProcessConnection)
        ensureNetworkProcessConnection();
    
    // If we failed to re-establish it then we are beyond recovery and should crash.
    if (!m_networkProcessConnection)
        CRASH();
    
    return m_networkProcessConnection.get();
}

void WebProcess::networkProcessConnectionClosed(NetworkProcessConnection* connection)
{
    // FIXME (NetworkProcess): How do we handle not having the connection when the WebProcess needs it?
    // If the NetworkProcess crashed, for example.  Do we respawn it?
    ASSERT(m_networkProcessConnection);
    ASSERT(m_networkProcessConnection == connection);

    m_networkProcessConnection = 0;
    
    m_webResourceLoadScheduler->networkProcessCrashed();
}

WebResourceLoadScheduler& WebProcess::webResourceLoadScheduler()
{
    return *m_webResourceLoadScheduler;
}

#endif

#if ENABLE(PLUGIN_PROCESS)
void WebProcess::pluginProcessCrashed(CoreIPC::Connection*, const String& pluginPath, uint32_t processType)
{
    m_pluginProcessConnectionManager->pluginProcessCrashed(pluginPath, static_cast<PluginProcess::Type>(processType));
}
#endif

void WebProcess::downloadRequest(uint64_t downloadID, uint64_t initiatingPageID, const ResourceRequest& request)
{
    WebPage* initiatingPage = initiatingPageID ? webPage(initiatingPageID) : 0;

    ResourceRequest requestWithOriginalURL = request;
    if (initiatingPage)
        initiatingPage->mainFrame()->loader()->setOriginalURLForDownloadRequest(requestWithOriginalURL);

    downloadManager().startDownload(downloadID, requestWithOriginalURL);
}

void WebProcess::cancelDownload(uint64_t downloadID)
{
    downloadManager().cancelDownload(downloadID);
}

#if PLATFORM(QT)
void WebProcess::startTransfer(uint64_t downloadID, const String& destination)
{
    downloadManager().startTransfer(downloadID, destination);
}
#endif

void WebProcess::setEnhancedAccessibility(bool flag)
{
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(flag);
}
    
void WebProcess::startMemorySampler(const SandboxExtension::Handle& sampleLogFileHandle, const String& sampleLogFilePath, const double interval)
{
#if ENABLE(MEMORY_SAMPLER)    
    WebMemorySampler::shared()->start(sampleLogFileHandle, sampleLogFilePath, interval);
#endif
}
    
void WebProcess::stopMemorySampler()
{
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::shared()->stop();
#endif
}

void WebProcess::setTextCheckerState(const TextCheckerState& textCheckerState)
{
    bool continuousSpellCheckingTurnedOff = !textCheckerState.isContinuousSpellCheckingEnabled && m_textCheckerState.isContinuousSpellCheckingEnabled;
    bool grammarCheckingTurnedOff = !textCheckerState.isGrammarCheckingEnabled && m_textCheckerState.isGrammarCheckingEnabled;

    m_textCheckerState = textCheckerState;

    if (!continuousSpellCheckingTurnedOff && !grammarCheckingTurnedOff)
        return;

    HashMap<uint64_t, RefPtr<WebPage> >::iterator end = m_pageMap.end();
    for (HashMap<uint64_t, RefPtr<WebPage> >::iterator it = m_pageMap.begin(); it != end; ++it) {
        WebPage* page = (*it).value.get();
        if (continuousSpellCheckingTurnedOff)
            page->unmarkAllMisspellings();
        if (grammarCheckingTurnedOff)
            page->unmarkAllBadGrammar();
    }
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcess::didGetPlugins(CoreIPC::Connection*, uint64_t requestID, const Vector<WebCore::PluginInfo>& plugins)
{
#if USE(PLATFORM_STRATEGIES)
    // Pass this to WebPlatformStrategies.cpp.
    handleDidGetPlugins(requestID, plugins);
#endif
}
#endif // ENABLE(PLUGIN_PROCESS)

#if ENABLE(CUSTOM_PROTOCOLS)
void WebProcess::initializeCustomProtocolManager(const WebProcessCreationParameters& parameters)
{
#if ENABLE(NETWORK_PROCESS)
    ASSERT(parameters.urlSchemesRegisteredForCustomProtocols.isEmpty() || !m_usesNetworkProcess);
    if (m_usesNetworkProcess)
        return;
#endif

    CustomProtocolManager::shared().connectionEstablished();
    for (size_t i = 0; i < parameters.urlSchemesRegisteredForCustomProtocols.size(); ++i)
        CustomProtocolManager::shared().registerScheme(parameters.urlSchemesRegisteredForCustomProtocols[i]);
}

void WebProcess::registerSchemeForCustomProtocol(const WTF::String& scheme)
{
    CustomProtocolManager::shared().registerScheme(scheme);
}

void WebProcess::unregisterSchemeForCustomProtocol(const WTF::String& scheme)
{
    CustomProtocolManager::shared().unregisterScheme(scheme);
}
#endif // ENABLE(CUSTOM_PROTOCOLS)

} // namespace WebKit
