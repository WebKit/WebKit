/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include "DownloadManager.h"
#include "InjectedBundle.h"
#include "InjectedBundleMessageKinds.h"
#include "InjectedBundleUserMessageCoders.h"
#include "RunLoop.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
#include "WebApplicationCacheManager.h"
#include "WebContextMessages.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManager.h"
#include "WebFrame.h"
#include "WebGeolocationManagerMessages.h"
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
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/Font.h>
#include <WebCore/FontCache.h>
#include <WebCore/Frame.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPageTreeNode.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/Language.h>
#include <WebCore/Logging.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryPressureHandler.h>
#include <WebCore/Page.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageGroup.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageTracker.h>
#include <wtf/HashCountedSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RandomNumber.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if !ENABLE(PLUGIN_PROCESS)
#include "NetscapePluginModule.h"
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

#if OS(WINDOWS)
static void sleep(unsigned seconds)
{
    ::Sleep(seconds * 1000);
}
#endif

static void* randomCrashThread(void*)
{
    // This delay was chosen semi-arbitrarily. We want the crash to happen somewhat quickly to
    // enable useful stress testing, but not so quickly that the web process will always crash soon
    // after launch.
    static const unsigned maximumRandomCrashDelay = 180;

    sleep(randomNumber() * maximumRandomCrashDelay);
    CRASH();
    return 0;
}

static void startRandomCrashThreadIfRequested()
{
    if (!getenv("WEBKIT2_CRASH_WEB_PROCESS_RANDOMLY"))
        return;
    createThread(randomCrashThread, 0, "WebKit2: Random Crash Thread");
}

WebProcess& WebProcess::shared()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

static const double shutdownTimeout = 60;

WebProcess::WebProcess()
    : ChildProcess(shutdownTimeout)
    , m_inDidClose(false)
    , m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
    , m_compositingRenderServerPort(MACH_PORT_NULL)
#endif
    , m_fullKeyboardAccessEnabled(false)
#if PLATFORM(QT)
    , m_networkAccessManager(0)
#endif
    , m_textCheckerState()
    , m_geolocationManager(this)
#if ENABLE(NOTIFICATIONS)
    , m_notificationManager(this)
#endif
    , m_iconDatabaseProxy(this)
#if ENABLE(PLUGIN_PROCESS)
    , m_disablePluginProcessMessageTimeout(false)
#endif
{
#if USE(PLATFORM_STRATEGIES)
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();
#endif // USE(PLATFORM_STRATEGIES)

    WebCore::InitializeLoggingChannelsIfNecessary();
}

void WebProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, RunLoop* runLoop)
{
    ASSERT(!m_connection);

    m_connection = WebConnectionToUIProcess::create(this, serverIdentifier, runLoop);
    m_runLoop = runLoop;

    startRandomCrashThreadIfRequested();
}

void WebProcess::initializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::ArgumentDecoder* arguments)
{
    ASSERT(m_pageMap.isEmpty());

    platformInitializeWebProcess(parameters, arguments);

    memoryPressureHandler().install();

    RefPtr<APIObject> injectedBundleInitializationUserData;
    InjectedBundleUserMessageDecoder messageDecoder(injectedBundleInitializationUserData);
    if (!arguments->decode(messageDecoder))
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
    // Make sure the WebDatabaseManager is initialized so that the Database directory is set.
    WebDatabaseManager::initialize(parameters.databaseDirectory);
#endif

#if ENABLE(ICONDATABASE)
    m_iconDatabaseProxy.setEnabled(parameters.iconDatabaseEnabled);
#endif

    StorageTracker::initializeTracker(parameters.localStorageDirectory, &WebKeyValueStorageManager::shared());
    m_localStorageDirectory = parameters.localStorageDirectory;

    if (!parameters.applicationCacheDirectory.isEmpty())
        cacheStorage().setCacheDirectory(parameters.applicationCacheDirectory);

    setShouldTrackVisitedLinks(parameters.shouldTrackVisitedLinks);
    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    if (!parameters.languageCode.isEmpty())
        overrideDefaultLanguage(parameters.languageCode);

    m_textCheckerState = parameters.textCheckerState;

    m_fullKeyboardAccessEnabled = parameters.fullKeyboardAccessEnabled;

    for (size_t i = 0; i < parameters.urlSchemesRegistererdAsEmptyDocument.size(); ++i)
        registerURLSchemeAsEmptyDocument(parameters.urlSchemesRegistererdAsEmptyDocument[i]);

    for (size_t i = 0; i < parameters.urlSchemesRegisteredAsSecure.size(); ++i)
        registerURLSchemeAsSecure(parameters.urlSchemesRegisteredAsSecure[i]);

    for (size_t i = 0; i < parameters.urlSchemesForWhichDomainRelaxationIsForbidden.size(); ++i)
        setDomainRelaxationForbiddenForURLScheme(parameters.urlSchemesForWhichDomainRelaxationIsForbidden[i]);

    setDefaultRequestTimeoutInterval(parameters.defaultRequestTimeoutInterval);

    for (size_t i = 0; i < parameters.mimeTypesWithCustomRepresentation.size(); ++i)
        m_mimeTypesWithCustomRepresentations.add(parameters.mimeTypesWithCustomRepresentation[i]);
    
#if PLATFORM(MAC)
    m_presenterApplicationPid = parameters.presenterApplicationPid;
#endif

    if (parameters.shouldAlwaysUseComplexTextCodePath)
        setAlwaysUsesComplexTextCodePath(true);

    if (parameters.shouldUseFontSmoothing)
        setShouldUseFontSmoothing(true);

#if USE(CFURLSTORAGESESSIONS)
    WebCore::ResourceHandle::setPrivateBrowsingStorageSessionIdentifierBase(parameters.uiProcessBundleIdentifier);
#endif

#if ENABLE(PLUGIN_PROCESS)
    m_disablePluginProcessMessageTimeout = parameters.disablePluginProcessMessageTimeout;
#endif
}

void WebProcess::setShouldTrackVisitedLinks(bool shouldTrackVisitedLinks)
{
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

void WebProcess::languageChanged(const String& language) const
{
    overrideDefaultLanguage(language);
}

void WebProcess::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    m_fullKeyboardAccessEnabled = fullKeyboardAccessEnabled;
}

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
            Page::visitedStateChanged(PageGroup::pageGroup(it->second->identifier()), linkHashes[i]);
    }

    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void WebProcess::allVisitedLinkStateChanged()
{
    // FIXME: We may want to track visited links per WebPageGroup rather than per WebContext.
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator it = m_pageGroupMap.begin();
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::const_iterator end = m_pageGroupMap.end();
    for (; it != end; ++it)
        Page::allVisitedStateChanged(PageGroup::pageGroup(it->second->identifier()));

    pageCache()->markPagesForVistedLinkStyleRecalc();
}

bool WebProcess::isLinkVisited(LinkHash linkHash) const
{
    return m_visitedLinkTable.isLinkVisited(linkHash);
}

void WebProcess::addVisitedLink(WebCore::LinkHash linkHash)
{
    if (isLinkVisited(linkHash))
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

void WebProcess::calculateCacheSizes(CacheModel cacheModel, uint64_t memorySize, uint64_t diskFreeSize,
    unsigned& cacheTotalCapacity, unsigned& cacheMinDeadCapacity, unsigned& cacheMaxDeadCapacity, double& deadDecodedDataDeletionInterval,
    unsigned& pageCacheCapacity, unsigned long& urlCacheMemoryCapacity, unsigned long& urlCacheDiskCapacity)
{
    switch (cacheModel) {
    case CacheModelDocumentViewer: {
        // Page cache capacity (in pages)
        pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memorySize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memorySize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        // Foundation memory cache capacity (in bytes)
        urlCacheMemoryCapacity = 0;

        // Foundation disk cache capacity (in bytes)
        urlCacheDiskCapacity = 0;

        break;
    }
    case CacheModelDocumentBrowser: {
        // Page cache capacity (in pages)
        if (memorySize >= 1024)
            pageCacheCapacity = 3;
        else if (memorySize >= 512)
            pageCacheCapacity = 2;
        else if (memorySize >= 256)
            pageCacheCapacity = 1;
        else
            pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memorySize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memorySize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        // Foundation memory cache capacity (in bytes)
        if (memorySize >= 2048)
            urlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memorySize >= 1024)
            urlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memorySize >= 512)
            urlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            urlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            urlCacheDiskCapacity = 50 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            urlCacheDiskCapacity = 40 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            urlCacheDiskCapacity = 30 * 1024 * 1024;
        else
            urlCacheDiskCapacity = 20 * 1024 * 1024;

        break;
    }
    case CacheModelPrimaryWebBrowser: {
        // Page cache capacity (in pages)
        // (Research indicates that value / page drops substantially after 3 pages.)
        if (memorySize >= 2048)
            pageCacheCapacity = 5;
        else if (memorySize >= 1024)
            pageCacheCapacity = 4;
        else if (memorySize >= 512)
            pageCacheCapacity = 3;
        else if (memorySize >= 256)
            pageCacheCapacity = 2;
        else
            pageCacheCapacity = 1;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial 
        // value / MB for some content / browsing patterns.)
        if (memorySize >= 2048)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memorySize >= 1536)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memorySize >= 1024)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memorySize >= 512)
            cacheTotalCapacity = 32 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = std::max(24u, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60;

        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memorySize >= 1024)
            urlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memorySize >= 512)
            urlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memorySize >= 256)
            urlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            urlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            urlCacheDiskCapacity = 175 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            urlCacheDiskCapacity = 150 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            urlCacheDiskCapacity = 125 * 1024 * 1024;
        else if (diskFreeSize >= 2048)
            urlCacheDiskCapacity = 100 * 1024 * 1024;
        else if (diskFreeSize >= 1024)
            urlCacheDiskCapacity = 75 * 1024 * 1024;
        else
            urlCacheDiskCapacity = 50 * 1024 * 1024;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };
}

WebPage* WebProcess::focusedWebPage() const
{    
    HashMap<uint64_t, RefPtr<WebPage> >::const_iterator end = m_pageMap.end();
    for (HashMap<uint64_t, RefPtr<WebPage> >::const_iterator it = m_pageMap.begin(); it != end; ++it) {
        WebPage* page = (*it).second.get();
        if (page->windowIsFocused())
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
    std::pair<HashMap<uint64_t, RefPtr<WebPage> >::iterator, bool> result = m_pageMap.add(pageID, 0);
    if (result.second) {
        ASSERT(!result.first->second);
        result.first->second = WebPage::create(pageID, parameters);

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
    }

    ASSERT(result.first->second);
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
    ASSERT(!DownloadManager::shared().isDownloading());

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

    platformTerminate();
    m_runLoop->stop();
}

void WebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{   
    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPage* page = webPage(pageID);
    if (!page)
        return;
    
    page->didReceiveSyncMessage(connection, messageID, arguments, reply);
}

void WebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcess>()) {
        didReceiveWebProcessMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassAuthenticationManager>()) {
        AuthenticationManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebApplicationCacheManager>()) {
        WebApplicationCacheManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebCookieManager>()) {
        WebCookieManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebDatabaseManager>()) {
        WebDatabaseManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebGeolocationManager>()) {
        m_geolocationManager.didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebIconDatabaseProxy>()) {
        m_iconDatabaseProxy.didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebKeyValueStorageManager>()) {
        WebKeyValueStorageManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebMediaCacheManager>()) {
        WebMediaCacheManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

#if ENABLE(NOTIFICATIONS)
    if (messageID.is<CoreIPC::MessageClassWebNotificationManager>()) {
        m_notificationManager.didReceiveMessage(connection, messageID, arguments);
        return;
    }
#endif
    
    if (messageID.is<CoreIPC::MessageClassWebResourceCacheManager>()) {
        WebResourceCacheManager::shared().didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassInjectedBundle>()) {
        if (!m_injectedBundle)
            return;
        m_injectedBundle->didReceiveMessage(connection, messageID, arguments);    
        return;
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPage* page = webPage(pageID);
    if (!page)
        return;
    
    page->didReceiveMessage(connection, messageID, arguments);
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

    gcController().garbageCollectNow();
    memoryCache()->setDisabled(true);
#endif    

    // The UI process closed this connection, shut down.
    m_runLoop->stop();
}

void WebProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
    // We received an invalid message, but since this is from the UI process (which we trust),
    // we'll let it slide.
}

void WebProcess::syncMessageSendTimedOut(CoreIPC::Connection*)
{
}

bool WebProcess::willProcessMessageOnClientRunLoop(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcess>())
        return willProcessWebProcessMessageOnClientRunLoop(connection, messageID, arguments);

    return true;
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
    std::pair<HashMap<uint64_t, RefPtr<WebPageGroupProxy> >::iterator, bool> result = m_pageGroupMap.add(pageGroupData.pageGroupID, 0);
    if (result.second) {
        ASSERT(!result.first->second);
        result.first->second = WebPageGroupProxy::create(pageGroupData);
    }

    return result.first->second.get();
}

static bool canPluginHandleResponse(const ResourceResponse& response)
{
    String pluginPath;

    if (!WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPluginPath(response.mimeType(), response.url().string()), Messages::WebContext::GetPluginPath::Reply(pluginPath), 0))
        return false;

    return !pluginPath.isEmpty();
}

bool WebProcess::shouldUseCustomRepresentationForResponse(const ResourceResponse& response) const
{
    if (!m_mimeTypesWithCustomRepresentations.contains(response.mimeType()))
        return false;

    // If a plug-in exists that claims to support this response, it should take precedence over the custom representation.
    return !canPluginHandleResponse(response);
}

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

#if !ENABLE(PLUGIN_PROCESS)
void WebProcess::getSitesWithPluginData(const Vector<String>& pluginPaths, uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

    HashSet<String> sitesSet;

    for (size_t i = 0; i < pluginPaths.size(); ++i) {
        RefPtr<NetscapePluginModule> netscapePluginModule = NetscapePluginModule::getOrCreate(pluginPaths[i]);
        if (!netscapePluginModule)
            continue;

        Vector<String> sites = netscapePluginModule->sitesWithData();
        for (size_t i = 0; i < sites.size(); ++i)
            sitesSet.add(sites[i]);
    }

    Vector<String> sites;
    copyToVector(sitesSet, sites);

    connection()->send(Messages::WebContext::DidGetSitesWithPluginData(sites, callbackID), 0);
}

void WebProcess::clearPluginSiteData(const Vector<String>& pluginPaths, const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

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

    connection()->send(Messages::WebContext::DidClearPluginSiteData(callbackID), 0);
}
#endif
    
static void fromCountedSetToHashMap(TypeCountSet* countedSet, HashMap<String, uint64_t>& map)
{
    TypeCountSet::const_iterator end = countedSet->end();
    for (TypeCountSet::const_iterator it = countedSet->begin(); it != end; ++it)
        map.set(it->first, it->second);
}

static void getWebCoreMemoryCacheStatistics(Vector<HashMap<String, uint64_t> >& result)
{
    DEFINE_STATIC_LOCAL(String, imagesString, ("Images"));
    DEFINE_STATIC_LOCAL(String, cssString, ("CSS"));
    DEFINE_STATIC_LOCAL(String, xslString, ("XSL"));
    DEFINE_STATIC_LOCAL(String, javaScriptString, ("JavaScript"));
    
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
        JSLock lock(SilenceAssertionsOnly);
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
#if !(PLATFORM(QT) && !HAVE(QRAWFONT)) // Qt doesn't use the glyph page tree currently. See: bug 63467.
    data.statisticsNumbers.set("GlyphPageCount", GlyphPageTreeNode::treeGlyphPageCount());
#endif
    
    // Get WebCore memory cache statistics
    getWebCoreMemoryCacheStatistics(data.webCoreCacheStatistics);
    
    connection()->send(Messages::WebContext::DidGetWebCoreStatistics(data, callbackID), 0);
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    gcController().garbageCollectNow();
}

#if ENABLE(PLUGIN_PROCESS)
void WebProcess::pluginProcessCrashed(const String& pluginPath)
{
    m_pluginProcessConnectionManager.pluginProcessCrashed(pluginPath);
}
#endif

void WebProcess::downloadRequest(uint64_t downloadID, uint64_t initiatingPageID, const ResourceRequest& request)
{
    WebPage* initiatingPage = initiatingPageID ? webPage(initiatingPageID) : 0;

    ResourceRequest requestWithOriginalURL = request;
    initiatingPage->mainFrame()->loader()->setOriginalURLForDownloadRequest(requestWithOriginalURL);

    DownloadManager::shared().startDownload(downloadID, initiatingPage, requestWithOriginalURL);
}

void WebProcess::cancelDownload(uint64_t downloadID)
{
    DownloadManager::shared().cancelDownload(downloadID);
}

#if PLATFORM(QT)
void WebProcess::startTransfer(uint64_t downloadID, const String& destination)
{
    DownloadManager::shared().startTransfer(downloadID, destination);
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
        WebPage* page = (*it).second.get();
        if (continuousSpellCheckingTurnedOff)
            page->unmarkAllMisspellings();
        if (grammarCheckingTurnedOff)
            page->unmarkAllBadGrammar();
    }
}

} // namespace WebKit
