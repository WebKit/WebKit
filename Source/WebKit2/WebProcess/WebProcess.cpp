/*
 * Copyright (C) 2009, 2010, 2012, 2014 Apple Inc. All rights reserved.
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

#include "APIFrameHandle.h"
#include "APIPageGroupHandle.h"
#include "APIPageHandle.h"
#include "AuthenticationManager.h"
#include "ChildProcessMessages.h"
#include "DrawingArea.h"
#include "EventDispatcher.h"
#include "InjectedBundle.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PluginProcessConnectionManager.h"
#include "SessionTracker.h"
#include "StatisticsData.h"
#include "UserData.h"
#include "WebConnectionToUIProcess.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebGeolocationManager.h"
#include "WebIconDatabaseProxy.h"
#include "WebLoaderStrategy.h"
#include "WebMediaKeyStorageManager.h"
#include "WebMemorySampler.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebPageGroupProxyMessages.h"
#include "WebPlatformStrategies.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebsiteData.h"
#include "WebsiteDataType.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/DNS.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/FontCache.h>
#include <WebCore/FontCascade.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPage.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/Language.h>
#include <WebCore/MainFrame.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryPressureHandler.h>
#include <WebCore/Page.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <unistd.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashCountedSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#endif

#if PLATFORM(COCOA)
#include "CookieStorageShim.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#if ENABLE(DATABASE_PROCESS)
#include "WebToDatabaseProcessConnection.h"
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManager.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

using namespace JSC;
using namespace WebCore;

// This should be less than plugInAutoStartExpirationTimeThreshold in PlugInAutoStartProvider.
static const double plugInAutoStartExpirationTimeUpdateThreshold = 29 * 24 * 60 * 60;

// This should be greater than tileRevalidationTimeout in TileController.
static const double nonVisibleProcessCleanupDelay = 10;

namespace WebKit {

WebProcess& WebProcess::singleton()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_eventDispatcher(EventDispatcher::create())
#if PLATFORM(IOS)
    , m_viewUpdateDispatcher(ViewUpdateDispatcher::create())
#endif
    , m_processSuspensionCleanupTimer(*this, &WebProcess::processSuspensionCleanupTimerFired)
    , m_inDidClose(false)
    , m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_fullKeyboardAccessEnabled(false)
    , m_textCheckerState()
    , m_iconDatabaseProxy(*new WebIconDatabaseProxy(this))
    , m_webLoaderStrategy(*new WebLoaderStrategy)
    , m_dnsPrefetchHystereris([this](HysteresisState state) { if (state == HysteresisState::Stopped) m_dnsPrefetchedHosts.clear(); })
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_pluginProcessConnectionManager(PluginProcessConnectionManager::create())
#endif
#if ENABLE(SERVICE_CONTROLS)
    , m_hasImageServices(false)
    , m_hasSelectionServices(false)
    , m_hasRichContentServices(false)
#endif
    , m_nonVisibleProcessCleanupTimer(*this, &WebProcess::nonVisibleProcessCleanupTimerFired)
#if PLATFORM(IOS)
    , m_webSQLiteDatabaseTracker(*this)
#endif
{
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();

    // FIXME: This should moved to where WebProcess::initialize is called,
    // so that ports have a chance to customize, and ifdefs in this file are
    // limited.
    addSupplement<WebGeolocationManager>();
    addSupplement<WebCookieManager>();
    addSupplement<AuthenticationManager>();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    addSupplement<WebNotificationManager>();
#endif
#if ENABLE(BATTERY_STATUS)
    addSupplement<WebBatteryManager>();
#endif
#if ENABLE(ENCRYPTED_MEDIA_V2)
    addSupplement<WebMediaKeyStorageManager>();
#endif
    m_plugInAutoStartOriginHashes.add(SessionID::defaultSessionID(), HashMap<unsigned, double>());

#if ENABLE(INDEXED_DATABASE)
    RuntimeEnabledFeatures::sharedFeatures().setWebkitIndexedDBEnabled(true);
#endif

#if PLATFORM(IOS)
    PageCache::singleton().setShouldClearBackingStores(true);
#endif
}

WebProcess::~WebProcess()
{
}

void WebProcess::initializeProcess(const ChildProcessInitializationParameters& parameters)
{
    platformInitializeProcess(parameters);
}

void WebProcess::initializeConnection(IPC::Connection* connection)
{
    ChildProcess::initializeConnection(connection);

    connection->setShouldExitOnSyncMessageSendFailure(true);

#if HAVE(QOS_CLASSES)
    connection->setShouldBoostMainThreadOnSyncMessage(true);
#endif

    m_eventDispatcher->initializeConnection(connection);
#if PLATFORM(IOS)
    m_viewUpdateDispatcher->initializeConnection(connection);
#endif // PLATFORM(IOS)

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginProcessConnectionManager->initializeConnection(connection);
#endif

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShim::singleton().initializeConnection(connection);
#endif

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);

    m_webConnection = WebConnectionToUIProcess::create(this);

    // In order to ensure that the asynchronous messages that are used for notifying the UI process
    // about when WebFrame objects come and go are always delivered before the synchronous policy messages,
    // use this flag to force synchronous messages to be treated as asynchronous messages in the UI process
    // unless when doing so would lead to a deadlock.
    connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
}

void WebProcess::initializeWebProcess(WebProcessCreationParameters&& parameters)
{
    ASSERT(m_pageMap.isEmpty());

#if OS(LINUX)
    WebCore::MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);
#endif

    platformInitializeWebProcess(WTFMove(parameters));

    WTF::setCurrentThreadIsUserInitiated();

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    if (!m_suppressMemoryPressureHandler)
        MemoryPressureHandler::singleton().install();

    if (!parameters.injectedBundlePath.isEmpty())
        m_injectedBundle = InjectedBundle::create(parameters, transformHandlesToObjects(parameters.initializationUserData.object()).get());

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);

    auto& databaseManager = DatabaseManager::singleton();
    databaseManager.initialize(parameters.webSQLDatabaseDirectory);

#if ENABLE(ICONDATABASE)
    m_iconDatabaseProxy.setEnabled(parameters.iconDatabaseEnabled);
#endif

    if (!parameters.applicationCacheDirectory.isEmpty())
        ApplicationCacheStorage::singleton().setCacheDirectory(parameters.applicationCacheDirectory);

    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    if (!parameters.languages.isEmpty())
        overrideUserPreferredLanguages(parameters.languages);

    m_textCheckerState = parameters.textCheckerState;

    m_fullKeyboardAccessEnabled = parameters.fullKeyboardAccessEnabled;

    for (auto& scheme : parameters.urlSchemesRegisteredAsEmptyDocument)
        registerURLSchemeAsEmptyDocument(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsSecure)
        registerURLSchemeAsSecure(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy)
        registerURLSchemeAsBypassingContentSecurityPolicy(scheme);

    for (auto& scheme : parameters.urlSchemesForWhichDomainRelaxationIsForbidden)
        setDomainRelaxationForbiddenForURLScheme(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsLocal)
        registerURLSchemeAsLocal(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsNoAccess)
        registerURLSchemeAsNoAccess(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsDisplayIsolated)
        registerURLSchemeAsDisplayIsolated(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsCORSEnabled)
        registerURLSchemeAsCORSEnabled(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsAlwaysRevalidated)
        registerURLSchemeAsAlwaysRevalidated(scheme);

    WebCore::Settings::setShouldRewriteConstAsVar(parameters.shouldRewriteConstAsVar);

#if ENABLE(CACHE_PARTITIONING)
    for (auto& scheme : parameters.urlSchemesRegisteredAsCachePartitioned)
        registerURLSchemeAsCachePartitioned(scheme);
#endif

    setDefaultRequestTimeoutInterval(parameters.defaultRequestTimeoutInterval);

    if (parameters.shouldAlwaysUseComplexTextCodePath)
        setAlwaysUsesComplexTextCodePath(true);

    if (parameters.shouldUseFontSmoothing)
        setShouldUseFontSmoothing(true);

#if PLATFORM(COCOA) || USE(CFNETWORK)
    SessionTracker::setIdentifierBase(parameters.uiProcessBundleIdentifier);
#endif

    if (parameters.shouldUseTestingNetworkSession)
        NetworkStorageSession::switchToNewTestingSession();

    ensureNetworkProcessConnection();

#if PLATFORM(COCOA)
    CookieStorageShim::singleton().initialize();
#endif
    setTerminationTimeout(parameters.terminationTimeout);

    resetPlugInAutoStartOriginHashes(parameters.plugInAutoStartOriginHashes);
    for (auto& origin : parameters.plugInAutoStartOrigins)
        m_plugInAutoStartOrigins.add(origin);

    setMemoryCacheDisabled(parameters.memoryCacheDisabled);

#if ENABLE(SERVICE_CONTROLS)
    setEnabledServices(parameters.hasImageServices, parameters.hasSelectionServices, parameters.hasRichContentServices);
#endif

#if ENABLE(REMOTE_INSPECTOR)
    audit_token_t auditToken;
    if (parentProcessConnection()->getAuditToken(auditToken)) {
        RetainPtr<CFDataRef> auditData = adoptCF(CFDataCreate(nullptr, (const UInt8*)&auditToken, sizeof(auditToken)));
        Inspector::RemoteInspector::singleton().setParentProcessInformation(presenterApplicationPid(), auditData);
    }
#endif

#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    for (auto hostIter = parameters.pluginLoadClientPolicies.begin(); hostIter != parameters.pluginLoadClientPolicies.end(); ++hostIter) {
        for (auto bundleIdentifierIter = hostIter->value.begin(); bundleIdentifierIter != hostIter->value.end(); ++bundleIdentifierIter) {
            for (auto versionIter = bundleIdentifierIter->value.begin(); versionIter != bundleIdentifierIter->value.end(); ++versionIter)
                platformStrategies()->pluginStrategy()->setPluginLoadClientPolicy(static_cast<PluginLoadClientPolicy>(versionIter->value), hostIter->key, bundleIdentifierIter->key, versionIter->key);
        }
    }
#endif
}

void WebProcess::ensureNetworkProcessConnection()
{
    if (m_networkProcessConnection)
        return;

    IPC::Attachment encodedConnectionIdentifier;

    if (!parentProcessConnection()->sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(),
        Messages::WebProcessProxy::GetNetworkProcessConnection::Reply(encodedConnectionIdentifier), 0))
        return;

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.releaseFileDescriptor();
#elif OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#else
    ASSERT_NOT_REACHED();
#endif
    if (IPC::Connection::identifierIsNull(connectionIdentifier))
        return;
    m_networkProcessConnection = NetworkProcessConnection::create(connectionIdentifier);
}

void WebProcess::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    SchemeRegistry::registerURLSchemeAsEmptyDocument(urlScheme);
}

void WebProcess::registerURLSchemeAsSecure(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsSecure(urlScheme);
}

void WebProcess::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
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

void WebProcess::registerURLSchemeAsAlwaysRevalidated(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsAlwaysRevalidated(urlScheme);
}

#if ENABLE(CACHE_PARTITIONING)
void WebProcess::registerURLSchemeAsCachePartitioned(const String& urlScheme) const
{
    SchemeRegistry::registerURLSchemeAsCachePartitioned(urlScheme);
}
#endif

void WebProcess::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    ResourceRequest::setDefaultTimeoutInterval(timeoutInterval);
}

void WebProcess::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    WebCore::FontCascade::setCodePath(alwaysUseComplexText ? WebCore::FontCascade::Complex : WebCore::FontCascade::Auto);
}

void WebProcess::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    WebCore::FontCascade::setShouldUseSmoothing(useFontSmoothing);
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

void WebProcess::ensurePrivateBrowsingSession(SessionID sessionID)
{
    WebFrameNetworkingContext::ensurePrivateBrowsingSession(sessionID);
}

void WebProcess::destroyPrivateBrowsingSession(SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
PluginProcessConnectionManager& WebProcess::pluginProcessConnectionManager()
{
    return *m_pluginProcessConnectionManager;
}
#endif

void WebProcess::setCacheModel(uint32_t cm)
{
    CacheModel cacheModel = static_cast<CacheModel>(cm);

    if (!m_hasSetCacheModel || cacheModel != m_cacheModel) {
        m_hasSetCacheModel = true;
        m_cacheModel = cacheModel;
        platformSetCacheModel(cacheModel);
    }
}

void WebProcess::clearCachedCredentials()
{
    NetworkStorageSession::defaultStorageSession().credentialStorage().clearCredentials();
}

WebPage* WebProcess::focusedWebPage() const
{    
    for (auto& page : m_pageMap.values()) {
        if (page->windowAndWebPageAreFocused())
            return page.get();
    }
    return 0;
}
    
WebPage* WebProcess::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID);
}

void WebProcess::createWebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way. 
    HashMap<uint64_t, RefPtr<WebPage>>::AddResult result = m_pageMap.add(pageID, nullptr);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPage::create(pageID, parameters);

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
    } else
        result.iterator->value->reinitializeWebPage(parameters);

    ASSERT(result.iterator->value);
}

void WebProcess::removeWebPage(uint64_t pageID)
{
    ASSERT(m_pageMap.contains(pageID));

    pageWillLeaveWindow(pageID);
    m_pageMap.remove(pageID);

    enableTermination();
}

bool WebProcess::shouldTerminate()
{
    ASSERT(m_pageMap.isEmpty());

    // FIXME: the ShouldTerminate message should also send termination parameters, such as any session cookies that need to be preserved.
    bool shouldTerminate = false;
    if (parentProcessConnection()->sendSync(Messages::WebProcessProxy::ShouldTerminate(), Messages::WebProcessProxy::ShouldTerminate::Reply(shouldTerminate), 0)
        && !shouldTerminate)
        return false;

    return true;
}

void WebProcess::terminate()
{
#ifndef NDEBUG
    GCController::singleton().garbageCollectNow();
    FontCache::singleton().invalidate();
    MemoryCache::singleton().setDisabled(true);
#endif

    m_webConnection->invalidate();
    m_webConnection = nullptr;

    platformTerminate();

    ChildProcess::terminate();
}

void WebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    didReceiveSyncWebProcessMessage(connection, decoder, replyEncoder);
}

void WebProcess::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcess::messageReceiverName()) {
        didReceiveWebProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::WebPageGroupProxy::messageReceiverName()) {
        uint64_t pageGroupID = decoder.destinationID();
        if (!pageGroupID)
            return;
        
        WebPageGroupProxy* pageGroupProxy = webPageGroup(pageGroupID);
        if (!pageGroupProxy)
            return;
        
        pageGroupProxy->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::ChildProcess::messageReceiverName()) {
        ChildProcess::didReceiveMessage(connection, decoder);
        return;
    }

    LOG_ERROR("Unhandled web process message '%s:%s'", decoder.messageReceiverName().toString().data(), decoder.messageName().toString().data());
}

void WebProcess::didClose(IPC::Connection&)
{
#ifndef NDEBUG
    m_inDidClose = true;

    // Close all the live pages.
    Vector<RefPtr<WebPage>> pages;
    copyValuesToVector(m_pageMap, pages);
    for (auto& page : pages)
        page->close();
    pages.clear();

    GCController::singleton().garbageCollectSoon();
    FontCache::singleton().invalidate();
    MemoryCache::singleton().setDisabled(true);
#endif

#if ENABLE(VIDEO)
    // FIXME(146657): This explicit media stop command should not be necessary
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->stopAllMediaPlaybackForProcess();
#endif

    // The UI process closed this connection, shut down.
    stopRunLoop();
}

void WebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
    // We received an invalid message, but since this is from the UI process (which we trust),
    // we'll let it slide.
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
    if (!parentProcessConnection())
        return;

    parentProcessConnection()->send(Messages::WebProcessProxy::DidDestroyFrame(frameID), 0);
}

WebPageGroupProxy* WebProcess::webPageGroup(PageGroup* pageGroup)
{
    for (auto& page : m_pageGroupMap.values()) {
        if (page->corePageGroup() == pageGroup)
            return page.get();
    }

    return 0;
}

WebPageGroupProxy* WebProcess::webPageGroup(uint64_t pageGroupID)
{
    return m_pageGroupMap.get(pageGroupID);
}

WebPageGroupProxy* WebProcess::webPageGroup(const WebPageGroupData& pageGroupData)
{
    auto result = m_pageGroupMap.add(pageGroupData.pageGroupID, nullptr);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPageGroupProxy::create(pageGroupData);
    }

    return result.iterator->value.get();
}

void WebProcess::clearResourceCaches(ResourceCachesToClear resourceCachesToClear)
{
    platformClearResourceCaches(resourceCachesToClear);

    // Toggling the cache model like this forces the cache to evict all its in-memory resources.
    // FIXME: We need a better way to do this.
    CacheModel cacheModel = m_cacheModel;
    setCacheModel(CacheModelDocumentViewer);
    setCacheModel(cacheModel);

    MemoryCache::singleton().evictResources();

    // Empty the cross-origin preflight cache.
    CrossOriginPreflightResultCache::singleton().empty();
}

void WebProcess::clearApplicationCache()
{
    // Empty the application cache.
    ApplicationCacheStorage::singleton().empty();
}

static inline void addCaseFoldedCharacters(StringHasher& hasher, const String& string)
{
    if (string.isEmpty())
        return;
    if (string.is8Bit()) {
        hasher.addCharacters<LChar, ASCIICaseInsensitiveHash::foldCase<LChar>>(string.characters8(), string.length());
        return;
    }
    hasher.addCharacters<UChar, ASCIICaseInsensitiveHash::foldCase<UChar>>(string.characters16(), string.length());
}

static unsigned hashForPlugInOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType)
{
    // We want to avoid concatenating the strings and then taking the hash, since that could lead to an expensive conversion.
    // We also want to avoid using the hash() function in StringImpl or ASCIICaseInsensitiveHash because that masks out bits for the use of flags.
    StringHasher hasher;
    addCaseFoldedCharacters(hasher, pageOrigin);
    hasher.addCharacter(0);
    addCaseFoldedCharacters(hasher, pluginOrigin);
    hasher.addCharacter(0);
    addCaseFoldedCharacters(hasher, mimeType);
    return hasher.hash();
}

bool WebProcess::isPlugInAutoStartOriginHash(unsigned plugInOriginHash, SessionID sessionID)
{
    HashMap<WebCore::SessionID, HashMap<unsigned, double>>::const_iterator sessionIterator = m_plugInAutoStartOriginHashes.find(sessionID);
    HashMap<unsigned, double>::const_iterator it;
    bool contains = false;

    if (sessionIterator != m_plugInAutoStartOriginHashes.end()) {
        it = sessionIterator->value.find(plugInOriginHash);
        contains = it != sessionIterator->value.end();
    }
    if (!contains) {
        sessionIterator = m_plugInAutoStartOriginHashes.find(SessionID::defaultSessionID());
        it = sessionIterator->value.find(plugInOriginHash);
        if (it == sessionIterator->value.end())
            return false;
    }
    return currentTime() < it->value;
}

bool WebProcess::shouldPlugInAutoStartFromOrigin(WebPage& webPage, const String& pageOrigin, const String& pluginOrigin, const String& mimeType)
{
    if (!pluginOrigin.isEmpty() && m_plugInAutoStartOrigins.contains(pluginOrigin))
        return true;

#ifdef ENABLE_PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC
    // The plugin wasn't in the general whitelist, so check if it similar to the primary plugin for the page (if we've found one).
    if (webPage.matchesPrimaryPlugIn(pageOrigin, pluginOrigin, mimeType))
        return true;
#else
    UNUSED_PARAM(webPage);
#endif

    // Lastly check against the more explicit hash list.
    return isPlugInAutoStartOriginHash(hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType), webPage.sessionID());
}

void WebProcess::plugInDidStartFromOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, SessionID sessionID)
{
    if (pageOrigin.isEmpty()) {
        LOG(Plugins, "Not adding empty page origin");
        return;
    }

    unsigned plugInOriginHash = hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType);
    if (isPlugInAutoStartOriginHash(plugInOriginHash, sessionID)) {
        LOG(Plugins, "Hash %x already exists as auto-start origin (request for %s)", plugInOriginHash, pageOrigin.utf8().data());
        return;
    }

    // We might attempt to start another plugin before the didAddPlugInAutoStartOrigin message
    // comes back from the parent process. Temporarily add this hash to the list with a thirty
    // second timeout. That way, even if the parent decides not to add it, we'll only be
    // incorrect for a little while.
    m_plugInAutoStartOriginHashes.add(sessionID, HashMap<unsigned, double>()).iterator->value.set(plugInOriginHash, currentTime() + 30 * 1000);

    parentProcessConnection()->send(Messages::WebProcessPool::AddPlugInAutoStartOriginHash(pageOrigin, plugInOriginHash, sessionID), 0);
}

void WebProcess::didAddPlugInAutoStartOriginHash(unsigned plugInOriginHash, double expirationTime, SessionID sessionID)
{
    // When called, some web process (which also might be this one) added the origin for auto-starting,
    // or received user interaction.
    // Set the bit to avoid having redundantly call into the UI process upon user interaction.
    m_plugInAutoStartOriginHashes.add(sessionID, HashMap<unsigned, double>()).iterator->value.set(plugInOriginHash, expirationTime);
}

void WebProcess::resetPlugInAutoStartOriginDefaultHashes(const HashMap<unsigned, double>& hashes)
{
    m_plugInAutoStartOriginHashes.clear();
    m_plugInAutoStartOriginHashes.add(SessionID::defaultSessionID(), HashMap<unsigned, double>()).iterator->value.swap(const_cast<HashMap<unsigned, double>&>(hashes));
}

void WebProcess::resetPlugInAutoStartOriginHashes(const HashMap<SessionID, HashMap<unsigned, double>>& hashes)
{
    m_plugInAutoStartOriginHashes.swap(const_cast<HashMap<SessionID, HashMap<unsigned, double>>&>(hashes));
}

void WebProcess::plugInDidReceiveUserInteraction(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, SessionID sessionID)
{
    if (pageOrigin.isEmpty())
        return;

    unsigned plugInOriginHash = hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType);
    if (!plugInOriginHash)
        return;

    HashMap<WebCore::SessionID, HashMap<unsigned, double>>::const_iterator sessionIterator = m_plugInAutoStartOriginHashes.find(sessionID);
    HashMap<unsigned, double>::const_iterator it;
    bool contains = false;
    if (sessionIterator != m_plugInAutoStartOriginHashes.end()) {
        it = sessionIterator->value.find(plugInOriginHash);
        contains = it != sessionIterator->value.end();
    }
    if (!contains) {
        sessionIterator = m_plugInAutoStartOriginHashes.find(SessionID::defaultSessionID());
        it = sessionIterator->value.find(plugInOriginHash);
        if (it == sessionIterator->value.end())
            return;
    }

    if (it->value - currentTime() > plugInAutoStartExpirationTimeUpdateThreshold)
        return;

    parentProcessConnection()->send(Messages::WebProcessPool::PlugInDidReceiveUserInteraction(plugInOriginHash, sessionID), 0);
}

void WebProcess::setPluginLoadClientPolicy(uint8_t policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    platformStrategies()->pluginStrategy()->setPluginLoadClientPolicy(static_cast<PluginLoadClientPolicy>(policy), host, bundleIdentifier, versionString);
#endif
}

void WebProcess::clearPluginClientPolicies()
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    platformStrategies()->pluginStrategy()->clearPluginClientPolicies();
#endif
}

static void fromCountedSetToHashMap(TypeCountSet* countedSet, HashMap<String, uint64_t>& map)
{
    TypeCountSet::const_iterator end = countedSet->end();
    for (TypeCountSet::const_iterator it = countedSet->begin(); it != end; ++it)
        map.set(it->key, it->value);
}

static void getWebCoreMemoryCacheStatistics(Vector<HashMap<String, uint64_t>>& result)
{
    String imagesString(ASCIILiteral("Images"));
    String cssString(ASCIILiteral("CSS"));
    String xslString(ASCIILiteral("XSL"));
    String javaScriptString(ASCIILiteral("JavaScript"));
    
    MemoryCache::Statistics memoryCacheStatistics = MemoryCache::singleton().getStatistics();
    
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
}

void WebProcess::getWebCoreStatistics(uint64_t callbackID)
{
    StatisticsData data;
    
    // Gather JavaScript statistics.
    {
        JSLockHolder lock(JSDOMWindow::commonVM());
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptObjectsCount"), JSDOMWindow::commonVM().heap.objectCount());
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptGlobalObjectsCount"), JSDOMWindow::commonVM().heap.globalObjectCount());
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptProtectedObjectsCount"), JSDOMWindow::commonVM().heap.protectedObjectCount());
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptProtectedGlobalObjectsCount"), JSDOMWindow::commonVM().heap.protectedGlobalObjectCount());
        
        std::unique_ptr<TypeCountSet> protectedObjectTypeCounts(JSDOMWindow::commonVM().heap.protectedObjectTypeCounts());
        fromCountedSetToHashMap(protectedObjectTypeCounts.get(), data.javaScriptProtectedObjectTypeCounts);
        
        std::unique_ptr<TypeCountSet> objectTypeCounts(JSDOMWindow::commonVM().heap.objectTypeCounts());
        fromCountedSetToHashMap(objectTypeCounts.get(), data.javaScriptObjectTypeCounts);
        
        uint64_t javaScriptHeapSize = JSDOMWindow::commonVM().heap.size();
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptHeapSize"), javaScriptHeapSize);
        data.statisticsNumbers.set(ASCIILiteral("JavaScriptFreeSize"), JSDOMWindow::commonVM().heap.capacity() - javaScriptHeapSize);
    }

    WTF::FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();
    data.statisticsNumbers.set(ASCIILiteral("FastMallocReservedVMBytes"), fastMallocStatistics.reservedVMBytes);
    data.statisticsNumbers.set(ASCIILiteral("FastMallocCommittedVMBytes"), fastMallocStatistics.committedVMBytes);
    data.statisticsNumbers.set(ASCIILiteral("FastMallocFreeListBytes"), fastMallocStatistics.freeListBytes);
    
    // Gather icon statistics.
    data.statisticsNumbers.set(ASCIILiteral("IconPageURLMappingCount"), iconDatabase().pageURLMappingCount());
    data.statisticsNumbers.set(ASCIILiteral("IconRetainedPageURLCount"), iconDatabase().retainedPageURLCount());
    data.statisticsNumbers.set(ASCIILiteral("IconRecordCount"), iconDatabase().iconRecordCount());
    data.statisticsNumbers.set(ASCIILiteral("IconsWithDataCount"), iconDatabase().iconRecordCountWithData());
    
    // Gather font statistics.
    auto& fontCache = FontCache::singleton();
    data.statisticsNumbers.set(ASCIILiteral("CachedFontDataCount"), fontCache.fontCount());
    data.statisticsNumbers.set(ASCIILiteral("CachedFontDataInactiveCount"), fontCache.inactiveFontCount());
    
    // Gather glyph page statistics.
    data.statisticsNumbers.set(ASCIILiteral("GlyphPageCount"), GlyphPage::count());
    
    // Get WebCore memory cache statistics
    getWebCoreMemoryCacheStatistics(data.webCoreCacheStatistics);
    
    parentProcessConnection()->send(Messages::WebProcessPool::DidGetStatistics(data, callbackID), 0);
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    GCController::singleton().garbageCollectNow();
}

void WebProcess::mainThreadPing()
{
    parentProcessConnection()->send(Messages::WebProcessProxy::DidReceiveMainThreadPing(), 0);
}

void WebProcess::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    GCController::singleton().setJavaScriptGarbageCollectorTimerEnabled(flag);
}

void WebProcess::handleInjectedBundleMessage(const String& messageName, const UserData& messageBody)
{
    InjectedBundle* injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->didReceiveMessage(messageName, transformHandlesToObjects(messageBody.object()).get());
}

void WebProcess::setInjectedBundleParameter(const String& key, const IPC::DataReference& value)
{
    InjectedBundle* injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->setBundleParameter(key, value);
}

void WebProcess::setInjectedBundleParameters(const IPC::DataReference& value)
{
    InjectedBundle* injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->setBundleParameters(value);
}

NetworkProcessConnection* WebProcess::networkConnection()
{
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
    ASSERT(m_networkProcessConnection);
    ASSERT_UNUSED(connection, m_networkProcessConnection == connection);

    m_networkProcessConnection = nullptr;
    
    m_webLoaderStrategy.networkProcessCrashed();
}

WebLoaderStrategy& WebProcess::webLoaderStrategy()
{
    return m_webLoaderStrategy;
}

#if ENABLE(DATABASE_PROCESS)
void WebProcess::webToDatabaseProcessConnectionClosed(WebToDatabaseProcessConnection* connection)
{
    ASSERT(m_webToDatabaseProcessConnection);
    ASSERT(m_webToDatabaseProcessConnection == connection);

    m_webToDatabaseProcessConnection = nullptr;
}

WebToDatabaseProcessConnection* WebProcess::webToDatabaseProcessConnection()
{
    if (!m_webToDatabaseProcessConnection)
        ensureWebToDatabaseProcessConnection();

    return m_webToDatabaseProcessConnection.get();
}

void WebProcess::ensureWebToDatabaseProcessConnection()
{
    if (m_webToDatabaseProcessConnection)
        return;

    IPC::Attachment encodedConnectionIdentifier;

    if (!parentProcessConnection()->sendSync(Messages::WebProcessProxy::GetDatabaseProcessConnection(),
        Messages::WebProcessProxy::GetDatabaseProcessConnection::Reply(encodedConnectionIdentifier), 0))
        return;

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.releaseFileDescriptor();
#elif OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#else
    ASSERT_NOT_REACHED();
#endif
    if (IPC::Connection::identifierIsNull(connectionIdentifier))
        return;
    m_webToDatabaseProcessConnection = WebToDatabaseProcessConnection::create(connectionIdentifier);
}

#endif // ENABLED(DATABASE_PROCESS)

void WebProcess::setEnhancedAccessibility(bool flag)
{
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(flag);
}
    
void WebProcess::startMemorySampler(const SandboxExtension::Handle& sampleLogFileHandle, const String& sampleLogFilePath, const double interval)
{
#if ENABLE(MEMORY_SAMPLER)    
    WebMemorySampler::singleton()->start(sampleLogFileHandle, sampleLogFilePath, interval);
#else
    UNUSED_PARAM(sampleLogFileHandle);
    UNUSED_PARAM(sampleLogFilePath);
    UNUSED_PARAM(interval);
#endif
}
    
void WebProcess::stopMemorySampler()
{
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::singleton()->stop();
#endif
}

void WebProcess::setTextCheckerState(const TextCheckerState& textCheckerState)
{
    bool continuousSpellCheckingTurnedOff = !textCheckerState.isContinuousSpellCheckingEnabled && m_textCheckerState.isContinuousSpellCheckingEnabled;
    bool grammarCheckingTurnedOff = !textCheckerState.isGrammarCheckingEnabled && m_textCheckerState.isGrammarCheckingEnabled;

    m_textCheckerState = textCheckerState;

    if (!continuousSpellCheckingTurnedOff && !grammarCheckingTurnedOff)
        return;

    for (auto& page : m_pageMap.values()) {
        if (continuousSpellCheckingTurnedOff)
            page->unmarkAllMisspellings();
        if (grammarCheckingTurnedOff)
            page->unmarkAllBadGrammar();
    }
}

void WebProcess::releasePageCache()
{
    PageCache::singleton().pruneToSizeNow(0, PruningReason::MemoryPressure);
}

void WebProcess::fetchWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID)
{
    WebsiteData websiteData;

    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        for (auto& origin : MemoryCache::singleton().originsWithCache(sessionID))
            websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::MemoryCache, 0 });
    }

    parentProcessConnection()->send(Messages::WebProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
}

void WebProcess::deleteWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
    UNUSED_PARAM(modifiedSince);

    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        PageCache::singleton().pruneToSizeNow(0, PruningReason::None);
        MemoryCache::singleton().evictResources(sessionID);

        CrossOriginPreflightResultCache::singleton().empty();
    }

    parentProcessConnection()->send(Messages::WebProcessProxy::DidDeleteWebsiteData(callbackID), 0);
}

void WebProcess::deleteWebsiteDataForOrigins(WebCore::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& originDatas, uint64_t callbackID)
{
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        HashSet<RefPtr<SecurityOrigin>> origins;
        for (auto& originData : originDatas)
            origins.add(originData.securityOrigin());

        MemoryCache::singleton().removeResourcesWithOrigins(sessionID, origins);
    }

    parentProcessConnection()->send(Messages::WebProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
}

void WebProcess::setHiddenPageTimerThrottlingIncreaseLimit(int milliseconds)
{
    for (auto& page : m_pageMap.values())
        page->setHiddenPageTimerThrottlingIncreaseLimit(std::chrono::milliseconds(milliseconds));
}

#if !PLATFORM(COCOA)
void WebProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void WebProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void WebProcess::platformInitializeProcess(const ChildProcessInitializationParameters&)
{
}

void WebProcess::updateActivePages()
{
}

#endif

#if PLATFORM(IOS)
void WebProcess::resetAllGeolocationPermissions()
{
    for (auto& page : m_pageMap.values()) {
        if (Frame* mainFrame = page->mainFrame())
            mainFrame->resetAllGeolocationPermission();
    }
}
#endif

void WebProcess::actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend shouldAcknowledgeWhenReadyToSuspend)
{
    if (!m_suppressMemoryPressureHandler)
        MemoryPressureHandler::singleton().releaseMemory(Critical::Yes, Synchronous::Yes);

    setAllLayerTreeStatesFrozen(true);

    if (markAllLayersVolatileIfPossible()) {
        if (shouldAcknowledgeWhenReadyToSuspend == ShouldAcknowledgeWhenReadyToSuspend::Yes)
            parentProcessConnection()->send(Messages::WebProcessProxy::ProcessReadyToSuspend(), 0);
        return;
    }
    m_shouldAcknowledgeWhenReadyToSuspend = shouldAcknowledgeWhenReadyToSuspend;
    m_processSuspensionCleanupTimer.startRepeating(std::chrono::milliseconds(20));
}

void WebProcess::processWillSuspendImminently(bool& handled)
{
    if (parentProcessConnection()->inSendSync()) {
        // Avoid reentrency bugs such as rdar://problem/21605505 by just bailing
        // if we get an incoming ProcessWillSuspendImminently message when waiting for a
        // reply to a sync message.
        // FIXME: ProcessWillSuspendImminently should not be a sync message.
        return;
    }

    DatabaseTracker::tracker().closeAllDatabases();
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::No);
    handled = true;
}

void WebProcess::prepareToSuspend()
{
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::Yes);
}

void WebProcess::cancelPrepareToSuspend()
{
    setAllLayerTreeStatesFrozen(false);

    // If we've already finished cleaning up and sent ProcessReadyToSuspend, we
    // shouldn't send DidCancelProcessSuspension; the UI process strictly expects one or the other.
    if (!m_processSuspensionCleanupTimer.isActive())
        return;

    m_processSuspensionCleanupTimer.stop();
    parentProcessConnection()->send(Messages::WebProcessProxy::DidCancelProcessSuspension(), 0);
}

bool WebProcess::markAllLayersVolatileIfPossible()
{
    bool successfullyMarkedAllLayersVolatile = true;
    for (auto& page : m_pageMap.values())
        successfullyMarkedAllLayersVolatile &= page->markLayersVolatileImmediatelyIfPossible();

    return successfullyMarkedAllLayersVolatile;
}

void WebProcess::setAllLayerTreeStatesFrozen(bool frozen)
{
    for (auto& page : m_pageMap.values())
        page->setLayerTreeStateIsFrozen(frozen);
}

void WebProcess::processSuspensionCleanupTimerFired()
{
    if (!markAllLayersVolatileIfPossible())
        return;
    m_processSuspensionCleanupTimer.stop();
    if (m_shouldAcknowledgeWhenReadyToSuspend == ShouldAcknowledgeWhenReadyToSuspend::Yes)
        parentProcessConnection()->send(Messages::WebProcessProxy::ProcessReadyToSuspend(), 0);
}
    
void WebProcess::processDidResume()
{
    m_processSuspensionCleanupTimer.stop();
    setAllLayerTreeStatesFrozen(false);
}

void WebProcess::pageDidEnterWindow(uint64_t pageID)
{
    m_pagesInWindows.add(pageID);
    m_nonVisibleProcessCleanupTimer.stop();
}

void WebProcess::pageWillLeaveWindow(uint64_t pageID)
{
    m_pagesInWindows.remove(pageID);

    if (m_pagesInWindows.isEmpty() && !m_nonVisibleProcessCleanupTimer.isActive())
        m_nonVisibleProcessCleanupTimer.startOneShot(nonVisibleProcessCleanupDelay);
}
    
void WebProcess::nonVisibleProcessCleanupTimerFired()
{
    ASSERT(m_pagesInWindows.isEmpty());
    if (!m_pagesInWindows.isEmpty())
        return;

#if PLATFORM(COCOA)
    destroyRenderingResources();
#endif
}

RefPtr<API::Object> WebProcess::transformHandlesToObjects(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        Transformer(WebProcess& webProcess)
            : m_webProcess(webProcess)
        {
        }

        virtual bool shouldTransformObject(const API::Object& object) const override
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

        virtual RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                return m_webProcess.webFrame(static_cast<const API::FrameHandle&>(object).frameID());

            case API::Object::Type::PageGroupHandle:
                return m_webProcess.webPageGroup(static_cast<const API::PageGroupHandle&>(object).webPageGroupData());

            case API::Object::Type::PageHandle:
                return m_webProcess.webPage(static_cast<const API::PageHandle&>(object).pageID());

#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
                return m_webProcess.transformHandlesToObjects(static_cast<ObjCObjectGraph&>(object));
#endif
            default:
                return &object;
            }
        }

        WebProcess& m_webProcess;
    };

    return UserData::transform(object, Transformer(*this));
}

RefPtr<API::Object> WebProcess::transformObjectsToHandles(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        virtual bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::BundleFrame:
            case API::Object::Type::BundlePage:
            case API::Object::Type::BundlePageGroup:
#if PLATFORM(COCOA)
            case API::Object::Type::ObjCObjectGraph:
#endif
                return true;

            default:
                return false;
            }
        }

        virtual RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::BundleFrame:
                return API::FrameHandle::createAutoconverting(static_cast<const WebFrame&>(object).frameID());

            case API::Object::Type::BundlePage:
                return API::PageHandle::createAutoconverting(static_cast<const WebPage&>(object).pageID());

            case API::Object::Type::BundlePageGroup: {
                WebPageGroupData pageGroupData;
                pageGroupData.pageGroupID = static_cast<const WebPageGroupProxy&>(object).pageGroupID();

                return API::PageGroupHandle::create(WTFMove(pageGroupData));
            }

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

void WebProcess::setMemoryCacheDisabled(bool disabled)
{
    auto& memoryCache = MemoryCache::singleton();
    if (memoryCache.disabled() != disabled)
        memoryCache.setDisabled(disabled);
}

#if ENABLE(SERVICE_CONTROLS)
void WebProcess::setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices)
{
    m_hasImageServices = hasImageServices;
    m_hasSelectionServices = hasSelectionServices;
    m_hasRichContentServices = hasRichContentServices;
}
#endif

void WebProcess::prefetchDNS(const String& hostname)
{
    if (hostname.isEmpty())
        return;

    if (m_dnsPrefetchedHosts.add(hostname).isNewEntry)
        networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::PrefetchDNS(hostname), 0);
    // The DNS prefetched hosts cache is only to avoid asking for the same hosts too many times
    // in a very short period of time, producing a lot of IPC traffic. So we clear this cache after
    // some time of no DNS requests.
    m_dnsPrefetchHystereris.impulse();
}

} // namespace WebKit
