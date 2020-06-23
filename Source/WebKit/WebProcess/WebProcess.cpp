/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcessMessages.h"
#include "DrawingArea.h"
#include "EventDispatcher.h"
#include "InjectedBundle.h"
#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "PluginProcessConnectionManager.h"
#include "ProcessAssertion.h"
#include "RemoteAudioSession.h"
#include "RemoteLegacyCDMFactory.h"
#include "StorageAreaMap.h"
#include "UserData.h"
#include "WebAutomationSessionProxy.h"
#include "WebCacheStorageProvider.h"
#include "WebConnectionToUIProcess.h"
#include "WebCookieJar.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebGamepadProvider.h"
#include "WebGeolocationManager.h"
#include "WebIDBConnectionToServer.h"
#include "WebLoaderStrategy.h"
#include "WebMediaKeyStorageManager.h"
#include "WebMemorySampler.h"
#include "WebMessagePortChannelProvider.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPaymentCoordinator.h"
#include "WebPlatformStrategies.h"
#include "WebPluginInfoProvider.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessDataStoreParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebResourceLoadObserver.h"
#include "WebSWClientConnection.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include "WebSocketStream.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteDataType.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <JavaScriptCore/WasmFaultSignalHandler.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BackForwardCache.h>
#include <WebCore/CPUMonitor.h>
#include <WebCore/CommonVM.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/DNS.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/FontCache.h>
#include <WebCore/FontCascade.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPage.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/MessagePort.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/Language.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringHash.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(WAYLAND)
#include "WaylandCompositorDisplay.h"
#endif

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#include "UserMediaCaptureManager.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "WebSQLiteDatabaseTracker.h"
#endif

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#if ENABLE(NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

#if ENABLE(GPU_PROCESS)
#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnection.h"
#include "GPUProcessConnectionInfo.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if ENABLE(GPU_PROCESS)
#include "RemoteMediaPlayerManager.h"
#endif

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
#include "LibWebRTCCodecs.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactory.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RemoteMediaSessionHelper.h"
#endif

#if ENABLE(ROUTING_ARBITRATION)
#include "AudioSessionRoutingArbitrator.h"
#endif

#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)

// This should be less than plugInAutoStartExpirationTimeThreshold in PlugInAutoStartProvider.
static const Seconds plugInAutoStartExpirationTimeUpdateThreshold { 29 * 24 * 60 * 60 };

// This should be greater than tileRevalidationTimeout in TileController.
static const Seconds nonVisibleProcessCleanupDelay { 10_s };

namespace WebKit {
using namespace JSC;
using namespace WebCore;

NO_RETURN static void callExit(IPC::Connection*)
{
    _exit(EXIT_SUCCESS);
}

WebProcess& WebProcess::singleton()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_eventDispatcher(EventDispatcher::create())
#if PLATFORM(IOS_FAMILY)
    , m_viewUpdateDispatcher(ViewUpdateDispatcher::create())
#endif
    , m_webInspectorInterruptDispatcher(WebInspectorInterruptDispatcher::create())
    , m_webLoaderStrategy(*new WebLoaderStrategy)
    , m_cacheStorageProvider(WebCacheStorageProvider::create())
    , m_cookieJar(WebCookieJar::create())
    , m_dnsPrefetchHystereris([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) m_dnsPrefetchedHosts.clear(); })
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_pluginProcessConnectionManager(PluginProcessConnectionManager::create())
#endif
    , m_nonVisibleProcessCleanupTimer(*this, &WebProcess::nonVisibleProcessCleanupTimerFired)
#if PLATFORM(IOS_FAMILY)
    , m_webSQLiteDatabaseTracker([this](bool isHoldingLockedFiles) { parentProcessConnection()->send(Messages::WebProcessProxy::SetIsHoldingLockedFiles(isHoldingLockedFiles), 0); })
#endif
{
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();

    // FIXME: This should moved to where WebProcess::initialize is called,
    // so that ports have a chance to customize, and ifdefs in this file are
    // limited.
    addSupplement<WebGeolocationManager>();

#if ENABLE(NOTIFICATIONS)
    addSupplement<WebNotificationManager>();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    addSupplement<WebMediaKeyStorageManager>();
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    addSupplement<UserMediaCaptureManager>();
#endif

#if ENABLE(GPU_PROCESS)
    addSupplement<RemoteMediaPlayerManager>();
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
    addSupplement<RemoteCDMFactory>();
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    addSupplement<RemoteLegacyCDMFactory>();
#endif

#if ENABLE(ROUTING_ARBITRATION)
    addSupplement<AudioSessionRoutingArbitrator>();
#endif

    Gigacage::forbidDisablingPrimitiveGigacage();
}

WebProcess::~WebProcess()
{
    ASSERT_NOT_REACHED();
}

void WebProcess::initializeProcess(const AuxiliaryProcessInitializationParameters& parameters)
{
    WTF::setProcessPrivileges({ });

    MessagePortChannelProvider::setSharedProvider(WebMessagePortChannelProvider::singleton());
    
    platformInitializeProcess(parameters);
    updateCPULimit();
}

void WebProcess::initializeConnection(IPC::Connection* connection)
{
    AuxiliaryProcess::initializeConnection(connection);

    // We call _exit() directly from the background queue in case the main thread is unresponsive
    // and AuxiliaryProcess::didClose() does not get called.
    connection->setDidCloseOnConnectionWorkQueueCallback(callExit);

#if !PLATFORM(GTK) && !PLATFORM(WPE)
    connection->setShouldExitOnSyncMessageSendFailure(true);
#endif

#if HAVE(QOS_CLASSES)
    connection->setShouldBoostMainThreadOnSyncMessage(true);
#endif

    m_eventDispatcher->initializeConnection(connection);
#if PLATFORM(IOS_FAMILY)
    m_viewUpdateDispatcher->initializeConnection(connection);
#endif // PLATFORM(IOS_FAMILY)

    m_webInspectorInterruptDispatcher->initializeConnection(connection);

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginProcessConnectionManager->initializeConnection(connection);
#endif

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);

    m_webConnection = WebConnectionToUIProcess::create(this);
}

void WebProcess::initializeWebProcess(WebProcessCreationParameters&& parameters)
{    
    TraceScope traceScope(InitializeWebProcessStart, InitializeWebProcessEnd);

    ASSERT(m_pageMap.isEmpty());

    if (parameters.websiteDataStoreParameters)
        setWebsiteDataStoreParameters(WTFMove(*parameters.websiteDataStoreParameters));

    WebCore::setPresentingApplicationPID(parameters.presentingApplicationPID);

#if OS(LINUX)
    MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);
#endif

    platformInitializeWebProcess(parameters);

    // Match the QoS of the UIProcess and the scrolling thread but use a slightly lower priority.
    WTF::Thread::setCurrentThreadIsUserInteractive(-1);

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([this] (Critical critical, Synchronous synchronous) {
#if PLATFORM(MAC)
            // If this is a process we keep around for performance, kill it on memory pressure instead of trying to free up its memory.
            if (m_processType == ProcessType::CachedWebContent || m_processType == ProcessType::PrewarmedWebContent || areAllPagesSuspended()) {
                if (m_processType == ProcessType::CachedWebContent)
                    RELEASE_LOG_IF_ALLOWED(Process, "initializeWebProcess: Cached WebProcess is exiting due to memory pressure");
                else if (m_processType == ProcessType::PrewarmedWebContent)
                    RELEASE_LOG_IF_ALLOWED(Process, "initializeWebProcess: Prewarmed WebProcess is exiting due to memory pressure");
                else
                    RELEASE_LOG_IF_ALLOWED(Process, "initializeWebProcess: Suspended WebProcess is exiting due to memory pressure");
                stopRunLoop();
                return;
            }
#endif

            auto maintainBackForwardCache = m_isSuspending ? WebCore::MaintainBackForwardCache::Yes : WebCore::MaintainBackForwardCache::No;
            auto maintainMemoryCache = m_isSuspending && m_hasSuspendedPageProxy ? WebCore::MaintainMemoryCache::Yes : WebCore::MaintainMemoryCache::No;
            WebCore::releaseMemory(critical, synchronous, maintainBackForwardCache, maintainMemoryCache);
        });
#if ENABLE(PERIODIC_MEMORY_MONITOR)
        memoryPressureHandler.setShouldUsePeriodicMemoryMonitor(true);
        memoryPressureHandler.setMemoryKillCallback([this] () {
            WebCore::logMemoryStatisticsAtTimeOfDeath();
            if (MemoryPressureHandler::singleton().processState() == WebsamProcessState::Active)
                parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedActiveMemoryLimit(), 0);
            else
                parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedInactiveMemoryLimit(), 0);
        });
        memoryPressureHandler.setDidExceedInactiveLimitWhileActiveCallback([this] () {
            parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedInactiveMemoryLimitWhileActive(), 0);
        });
#endif
        memoryPressureHandler.setMemoryPressureStatusChangedCallback([this](bool isUnderMemoryPressure) {
            if (parentProcessConnection())
                parentProcessConnection()->send(Messages::WebProcessProxy::MemoryPressureStatusChanged(isUnderMemoryPressure), 0);
        });
        memoryPressureHandler.install();
    }

    SandboxExtension::consumePermanently(parameters.additionalSandboxExtensionHandles);

    if (!parameters.injectedBundlePath.isEmpty())
        m_injectedBundle = InjectedBundle::create(parameters, transformHandlesToObjects(parameters.initializationUserData.object()).get());

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);

    setCacheModel(parameters.cacheModel);

    if (!parameters.overrideLanguages.isEmpty())
        overrideUserPreferredLanguages(parameters.overrideLanguages);

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
        LegacySchemeRegistry::registerURLSchemeAsCORSEnabled(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsAlwaysRevalidated)
        registerURLSchemeAsAlwaysRevalidated(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsCachePartitioned)
        registerURLSchemeAsCachePartitioned(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest)
        registerURLSchemeAsCanDisplayOnlyIfCanRequest(scheme);

    setDefaultRequestTimeoutInterval(parameters.defaultRequestTimeoutInterval);

    setBackForwardCacheCapacity(parameters.backForwardCacheCapacity);

    setAlwaysUsesComplexTextCodePath(parameters.shouldAlwaysUseComplexTextCodePath);

    setShouldUseFontSmoothing(parameters.shouldUseFontSmoothing);

    setTerminationTimeout(parameters.terminationTimeout);

    for (auto& origin : parameters.plugInAutoStartOrigins)
        m_plugInAutoStartOrigins.add(origin);

    setMemoryCacheDisabled(parameters.memoryCacheDisabled);

    WebCore::RuntimeEnabledFeatures::sharedFeatures().setAttrStyleEnabled(parameters.attrStyleEnabled);

#if ENABLE(SERVICE_CONTROLS)
    setEnabledServices(parameters.hasImageServices, parameters.hasSelectionServices, parameters.hasRichContentServices);
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
#if PLATFORM(IOS)
    Inspector::RemoteInspector::setNeedMachSandboxExtension(true);
#endif
    if (Optional<audit_token_t> auditToken = parentProcessConnection()->getAuditToken()) {
        RetainPtr<CFDataRef> auditData = adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
        Inspector::RemoteInspector::singleton().setParentProcessInformation(WebCore::presentingApplicationPID(), auditData);
    }
#endif

#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    resetPluginLoadClientPolicies(parameters.pluginLoadClientPolicies);
#endif

#if ENABLE(GAMEPAD)
    GamepadProvider::singleton().setSharedProvider(WebGamepadProvider::singleton());
#endif

#if ENABLE(SERVICE_WORKER)
    ServiceWorkerProvider::setSharedProvider(WebServiceWorkerProvider::singleton());
#endif

#if ENABLE(WEBASSEMBLY)
    JSC::Wasm::enableFastMemory();
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    WebResourceLoadObserver::setShouldLogUserInteraction(parameters.shouldLogUserInteraction);
#endif

    RELEASE_LOG_IF_ALLOWED(Process, "initializeWebProcess: Presenting process = %d", WebCore::presentingApplicationPID());
}

void WebProcess::setWebsiteDataStoreParameters(WebProcessDataStoreParameters&& parameters)
{
    ASSERT(!m_sessionID);
    m_sessionID = parameters.sessionID;
    
    auto& databaseManager = DatabaseManager::singleton();
    databaseManager.initialize(parameters.webSQLDatabaseDirectory);

    // FIXME: This should be constructed per data store, not per process.
    m_applicationCacheStorage = ApplicationCacheStorage::create(parameters.applicationCacheDirectory, parameters.applicationCacheFlatFileSubdirectoryName);
#if PLATFORM(IOS_FAMILY)
    m_applicationCacheStorage->setDefaultOriginQuota(25ULL * 1024 * 1024);
#endif

#if ENABLE(VIDEO)
    if (!parameters.mediaCacheDirectory.isEmpty())
        WebCore::HTMLMediaElement::setMediaCacheDirectory(parameters.mediaCacheDirectory);
#endif

    setResourceLoadStatisticsEnabled(parameters.resourceLoadStatisticsEnabled);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    m_thirdPartyCookieBlockingMode = parameters.thirdPartyCookieBlockingMode;
    if (parameters.resourceLoadStatisticsEnabled) {
        if (!ResourceLoadObserver::sharedIfExists())
            ResourceLoadObserver::setShared(*new WebResourceLoadObserver(parameters.sessionID.isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
        ResourceLoadObserver::shared().setDomainsWithUserInteraction(WTFMove(parameters.domainsWithUserInteraction));
    }
    
#endif

    resetPlugInAutoStartOriginHashes(WTFMove(parameters.plugInAutoStartOriginHashes));

    for (auto& supplement : m_supplements.values())
        supplement->setWebsiteDataStore(parameters);

    platformSetWebsiteDataStoreParameters(WTFMove(parameters));
    
    ensureNetworkProcessConnection();
}

bool WebProcess::areAllPagesSuspended() const
{
    for (auto& page : m_pageMap.values()) {
        if (!page->isSuspended())
            return false;
    }
    return true;
}

void WebProcess::setHasSuspendedPageProxy(bool hasSuspendedPageProxy)
{
    ASSERT(m_hasSuspendedPageProxy != hasSuspendedPageProxy);
    m_hasSuspendedPageProxy = hasSuspendedPageProxy;
}

void WebProcess::setIsInProcessCache(bool isInProcessCache)
{
#if PLATFORM(COCOA)
    if (isInProcessCache) {
        ASSERT(m_processType == ProcessType::WebContent);
        m_processType = ProcessType::CachedWebContent;
    } else {
        ASSERT(m_processType == ProcessType::CachedWebContent);
        m_processType = ProcessType::WebContent;
    }

    updateProcessName();
#else
    UNUSED_PARAM(isInProcessCache);
#endif
}

void WebProcess::markIsNoLongerPrewarmed()
{
#if PLATFORM(COCOA)
    ASSERT(m_processType == ProcessType::PrewarmedWebContent);
    m_processType = ProcessType::WebContent;

    updateProcessName();
#endif
}

void WebProcess::prewarmGlobally()
{
    WebCore::ProcessWarming::prewarmGlobally();
}

void WebProcess::prewarmWithDomainInformation(const WebCore::PrewarmInformation& prewarmInformation)
{
    WebCore::ProcessWarming::prewarmWithInformation(prewarmInformation);
}

void WebProcess::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    LegacySchemeRegistry::registerURLSchemeAsEmptyDocument(urlScheme);
}

void WebProcess::registerURLSchemeAsSecure(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsSecure(urlScheme);
}

void WebProcess::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
}

void WebProcess::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme) const
{
    LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(true, urlScheme);
}

void WebProcess::registerURLSchemeAsLocal(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsLocal(urlScheme);
}

void WebProcess::registerURLSchemeAsNoAccess(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsNoAccess(urlScheme);
}

void WebProcess::registerURLSchemeAsDisplayIsolated(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsDisplayIsolated(urlScheme);
}

void WebProcess::registerURLSchemeAsCORSEnabled(const String& urlScheme)
{
    LegacySchemeRegistry::registerURLSchemeAsCORSEnabled(urlScheme);
    ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterURLSchemesAsCORSEnabled({ urlScheme }), 0);
}

void WebProcess::registerURLSchemeAsAlwaysRevalidated(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsAlwaysRevalidated(urlScheme);
}

void WebProcess::registerURLSchemeAsCachePartitioned(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsCachePartitioned(urlScheme);
}

void WebProcess::registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String& urlScheme) const
{
    LegacySchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(urlScheme);
}

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

void WebProcess::userPreferredLanguagesChanged() const
{
    WTF::languageDidChange();
}

void WebProcess::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    m_fullKeyboardAccessEnabled = fullKeyboardAccessEnabled;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
PluginProcessConnectionManager& WebProcess::pluginProcessConnectionManager()
{
    return *m_pluginProcessConnectionManager;
}
#endif

void WebProcess::setCacheModel(CacheModel cacheModel)
{
    if (m_hasSetCacheModel && (cacheModel == m_cacheModel))
        return;

    m_hasSetCacheModel = true;
    m_cacheModel = cacheModel;

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    Seconds deadDecodedDataDeletionInterval;
    unsigned backForwardCacheSize = 0;
    calculateMemoryCacheSizes(cacheModel, cacheTotalCapacity, cacheMinDeadCapacity, cacheMaxDeadCapacity, deadDecodedDataDeletionInterval, backForwardCacheSize);

    auto& memoryCache = MemoryCache::singleton();
    memoryCache.setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache.setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    BackForwardCache::singleton().setMaxSize(backForwardCacheSize);

    platformSetCacheModel(cacheModel);
}

WebPage* WebProcess::focusedWebPage() const
{    
    for (auto& page : m_pageMap.values()) {
        if (page->windowAndWebPageAreFocused())
            return page.get();
    }
    return 0;
}
    
WebPage* WebProcess::webPage(PageIdentifier pageID) const
{
    return m_pageMap.get(pageID);
}

void WebProcess::createWebPage(PageIdentifier pageID, WebPageCreationParameters&& parameters)
{
    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way. 
    auto result = m_pageMap.add(pageID, nullptr);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPage::create(pageID, WTFMove(parameters));

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
        updateCPULimit();
    } else
        result.iterator->value->reinitializeWebPage(WTFMove(parameters));

    ASSERT(result.iterator->value);
}

void WebProcess::removeWebPage(PageIdentifier pageID)
{
    ASSERT(m_pageMap.contains(pageID));

    flushResourceLoadStatistics();

    pageWillLeaveWindow(pageID);
    m_pageMap.remove(pageID);

    enableTermination();
    updateCPULimit();
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

    AuxiliaryProcess::terminate();
}

void WebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return;
}

void WebProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::WebProcess::messageReceiverName()) {
        didReceiveWebProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::AuxiliaryProcess::messageReceiverName()) {
        AuxiliaryProcess::didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    // FIXME: Remove?
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }
#endif

    LOG_ERROR("Unhandled web process message '%s' (destination: %" PRIu64 " pid: %d)", description(decoder.messageName()), decoder.destinationID(), static_cast<int>(getCurrentProcessID()));
}

WebFrame* WebProcess::webFrame(FrameIdentifier frameID) const
{
    return m_frameMap.get(frameID);
}

Vector<WebFrame*> WebProcess::webFrames() const
{
    return copyToVector(m_frameMap.values());
}

void WebProcess::addWebFrame(FrameIdentifier frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebProcess::removeWebFrame(FrameIdentifier frameID)
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

static uint64_t nextUserGestureTokenIdentifier()
{
    static uint64_t identifier = 1;
    return identifier++;
}

uint64_t WebProcess::userGestureTokenIdentifier(RefPtr<UserGestureToken> token)
{
    if (!token || !token->processingUserGesture())
        return 0;

    auto result = m_userGestureTokens.ensure(token.get(), [] { return nextUserGestureTokenIdentifier(); });
    if (result.isNewEntry) {
        result.iterator->key->addDestructionObserver([] (UserGestureToken& tokenBeingDestroyed) {
            WebProcess::singleton().userGestureTokenDestroyed(tokenBeingDestroyed);
        });
    }
    
    return result.iterator->value;
}

void WebProcess::userGestureTokenDestroyed(UserGestureToken& token)
{
    auto identifier = m_userGestureTokens.take(&token);
    parentProcessConnection()->send(Messages::WebProcessProxy::DidDestroyUserGestureToken(identifier), 0);
}

static inline void addCaseFoldedCharacters(StringHasher& hasher, const String& string)
{
    if (string.isEmpty())
        return;
    if (string.is8Bit()) {
        hasher.addCharacters<LChar, ASCIICaseInsensitiveHash::FoldCase<LChar>>(string.characters8(), string.length());
        return;
    }
    hasher.addCharacters<UChar, ASCIICaseInsensitiveHash::FoldCase<UChar>>(string.characters16(), string.length());
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

bool WebProcess::isPlugInAutoStartOriginHash(unsigned plugInOriginHash)
{
    auto it = m_plugInAutoStartOriginHashes.find(plugInOriginHash);
    if (it == m_plugInAutoStartOriginHashes.end())
        return false;

    return WallTime::now() < it->value;
}

bool WebProcess::shouldPlugInAutoStartFromOrigin(WebPage& webPage, const String& pageOrigin, const String& pluginOrigin, const String& mimeType)
{
    if (!pluginOrigin.isEmpty() && m_plugInAutoStartOrigins.contains(pluginOrigin))
        return true;

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    // The plugin wasn't in the general whitelist, so check if it similar to the primary plugin for the page (if we've found one).
    if (webPage.matchesPrimaryPlugIn(pageOrigin, pluginOrigin, mimeType))
        return true;
#else
    UNUSED_PARAM(webPage);
#endif

    // Lastly check against the more explicit hash list.
    return isPlugInAutoStartOriginHash(hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType));
}

void WebProcess::plugInDidStartFromOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType)
{
    if (pageOrigin.isEmpty()) {
        LOG(Plugins, "Not adding empty page origin");
        return;
    }

    unsigned plugInOriginHash = hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType);
    if (isPlugInAutoStartOriginHash(plugInOriginHash)) {
        LOG(Plugins, "Hash %x already exists as auto-start origin (request for %s)", plugInOriginHash, pageOrigin.utf8().data());
        return;
    }

    // We might attempt to start another plugin before the didAddPlugInAutoStartOrigin message
    // comes back from the parent process. Temporarily add this hash to the list with a thirty
    // second timeout. That way, even if the parent decides not to add it, we'll only be
    // incorrect for a little while.
    m_plugInAutoStartOriginHashes.set(plugInOriginHash, WallTime::now() + 30_s * 1000);

    parentProcessConnection()->send(Messages::WebProcessProxy::AddPlugInAutoStartOriginHash(pageOrigin, plugInOriginHash), 0);
}

void WebProcess::didAddPlugInAutoStartOriginHash(unsigned plugInOriginHash, WallTime expirationTime)
{
    // When called, some web process (which also might be this one) added the origin for auto-starting,
    // or received user interaction.
    // Set the bit to avoid having redundantly call into the UI process upon user interaction.
    m_plugInAutoStartOriginHashes.set(plugInOriginHash, expirationTime);
}

void WebProcess::resetPlugInAutoStartOriginHashes(HashMap<unsigned, WallTime>&& hashes)
{
    m_plugInAutoStartOriginHashes = WTFMove(hashes);
}

void WebProcess::plugInDidReceiveUserInteraction(const String& pageOrigin, const String& pluginOrigin, const String& mimeType)
{
    if (pageOrigin.isEmpty())
        return;

    unsigned plugInOriginHash = hashForPlugInOrigin(pageOrigin, pluginOrigin, mimeType);
    if (!plugInOriginHash)
        return;

    auto it = m_plugInAutoStartOriginHashes.find(plugInOriginHash);
    if (it == m_plugInAutoStartOriginHashes.end())
        return;

    if (it->value - WallTime::now() > plugInAutoStartExpirationTimeUpdateThreshold)
        return;

    parentProcessConnection()->send(Messages::WebProcessProxy::PlugInDidReceiveUserInteraction(plugInOriginHash), 0);
}

void WebProcess::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    WebPluginInfoProvider::singleton().setPluginLoadClientPolicy(policy, host, bundleIdentifier, versionString);
#endif
}

void WebProcess::resetPluginLoadClientPolicies(const HashMap<WTF::String, HashMap<WTF::String, HashMap<WTF::String, WebCore::PluginLoadClientPolicy>>>& pluginLoadClientPolicies)
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    clearPluginClientPolicies();

    for (auto& hostPair : pluginLoadClientPolicies) {
        for (auto& bundleIdentifierPair : hostPair.value) {
            for (auto& versionPair : bundleIdentifierPair.value)
                WebPluginInfoProvider::singleton().setPluginLoadClientPolicy(versionPair.value, hostPair.key, bundleIdentifierPair.key, versionPair.key);
        }
    }
#endif
}

void WebProcess::isJITEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(JSC::Options::useJIT());
}

void WebProcess::clearPluginClientPolicies()
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    WebPluginInfoProvider::singleton().clearPluginClientPolicies();
#endif
}

void WebProcess::refreshPlugins()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebPluginInfoProvider::singleton().refresh(false);
#endif
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    GCController::singleton().garbageCollectNow();
}

void WebProcess::mainThreadPing()
{
    parentProcessConnection()->send(Messages::WebProcessProxy::DidReceiveMainThreadPing(), 0);
}

void WebProcess::backgroundResponsivenessPing()
{
    parentProcessConnection()->send(Messages::WebProcessProxy::DidReceiveBackgroundResponsivenessPing(), 0);
}

void WebProcess::messagesAvailableForPort(const MessagePortIdentifier& identifier)
{
    MessagePort::notifyMessageAvailable(identifier);
}

#if ENABLE(GAMEPAD)

void WebProcess::setInitialGamepads(const Vector<WebKit::GamepadData>& gamepadDatas)
{
    WebGamepadProvider::singleton().setInitialGamepads(gamepadDatas);
}

void WebProcess::gamepadConnected(const GamepadData& gamepadData)
{
    WebGamepadProvider::singleton().gamepadConnected(gamepadData);
}

void WebProcess::gamepadDisconnected(unsigned index)
{
    WebGamepadProvider::singleton().gamepadDisconnected(index);
}

#endif

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

static NetworkProcessConnectionInfo getNetworkProcessConnection(IPC::Connection& connection)
{
    NetworkProcessConnectionInfo connectionInfo;
    if (!connection.sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(), Messages::WebProcessProxy::GetNetworkProcessConnection::Reply(connectionInfo), 0)) {
        // If we failed the first time, retry once. The attachment may have become invalid
        // before it was received by the web process if the network process crashed.
        if (!connection.sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(), Messages::WebProcessProxy::GetNetworkProcessConnection::Reply(connectionInfo), 0)) {
#if PLATFORM(GTK) || PLATFORM(WPE)
            // GTK+ and WPE ports don't exit on send sync message failure.
            // In this particular case, the network process can be terminated by the UI process while the
            // Web process is still initializing, so we always want to exit instead of crashing. This can
            // happen when the WebView is created and then destroyed quickly.
            // See https://bugs.webkit.org/show_bug.cgi?id=183348.
            exit(0);
#else
            CRASH();
#endif
        }
    }

    return connectionInfo;
}

NetworkProcessConnection& WebProcess::ensureNetworkProcessConnection()
{
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(m_sessionID);

    // If we've lost our connection to the network process (e.g. it crashed) try to re-establish it.
    if (!m_networkProcessConnection) {
        auto connectionInfo = getNetworkProcessConnection(*parentProcessConnection());

        // Retry once if the IPC to get the connectionIdentifier succeeded but the connectionIdentifier we received
        // is invalid. This may indicate that the network process has crashed.
        if (!IPC::Connection::identifierIsValid(connectionInfo.identifier()))
            connectionInfo = getNetworkProcessConnection(*parentProcessConnection());

        if (!IPC::Connection::identifierIsValid(connectionInfo.identifier()))
            CRASH();

        m_networkProcessConnection = NetworkProcessConnection::create(connectionInfo.releaseIdentifier(), connectionInfo.cookieAcceptPolicy);
#if HAVE(AUDIT_TOKEN)
        m_networkProcessConnection->setNetworkProcessAuditToken(WTFMove(connectionInfo.auditToken));
#endif
        m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::RegisterURLSchemesAsCORSEnabled(WebCore::LegacySchemeRegistry::allURLSchemesRegisteredAsCORSEnabled()), 0);

#if ENABLE(SERVICE_WORKER)
        if (!Document::allDocuments().isEmpty())
            m_networkProcessConnection->serviceWorkerConnection().registerServiceWorkerClients();
#endif
    }
    
    return *m_networkProcessConnection;
}

void WebProcess::logDiagnosticMessageForNetworkProcessCrash()
{
    WebCore::Page* page = nullptr;

    if (auto* webPage = focusedWebPage())
        page = webPage->corePage();

    if (!page) {
        for (auto& webPage : m_pageMap.values()) {
            if (auto* corePage = webPage->corePage()) {
                page = corePage;
                break;
            }
        }
    }

    if (page)
        page->diagnosticLoggingClient().logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::networkProcessCrashedKey(), WebCore::ShouldSample::No);
}

void WebProcess::networkProcessConnectionClosed(NetworkProcessConnection* connection)
{
    ASSERT(m_networkProcessConnection);
    ASSERT_UNUSED(connection, m_networkProcessConnection == connection);

    for (auto* storageAreaMap : copyToVector(m_storageAreaMaps.values()))
        storageAreaMap->disconnect();

#if ENABLE(INDEXED_DATABASE)
    for (auto& page : m_pageMap.values()) {
        auto idbConnection = page->corePage()->optionalIDBConnection();
        if (!idbConnection)
            continue;
        
        if (auto* existingIDBConnectionToServer = connection->existingIDBConnectionToServer()) {
            ASSERT_UNUSED(existingIDBConnectionToServer, idbConnection == &existingIDBConnectionToServer->coreConnectionToServer());
            page->corePage()->clearIDBConnection();
        }
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (SWContextManager::singleton().connection())
        SWContextManager::singleton().stopAllServiceWorkers();
#endif

    m_networkProcessConnection = nullptr;

    logDiagnosticMessageForNetworkProcessCrash();

    m_webLoaderStrategy.networkProcessCrashed();
    WebSocketStream::networkProcessCrashed();
    m_webSocketChannelManager.networkProcessCrashed();

    if (m_libWebRTCNetwork)
        m_libWebRTCNetwork->networkProcessCrashed();

    for (auto& page : m_pageMap.values()) {
        page->stopAllURLSchemeTasks();
#if ENABLE(APPLE_PAY)
        if (auto paymentCoordinator = page->paymentCoordinator())
            paymentCoordinator->networkProcessConnectionClosed();
#endif
    }
}

WebLoaderStrategy& WebProcess::webLoaderStrategy()
{
    return m_webLoaderStrategy;
}

#if ENABLE(GPU_PROCESS)

static GPUProcessConnectionInfo getGPUProcessConnection(IPC::Connection& connection)
{
    GPUProcessConnectionInfo connectionInfo;
    if (!connection.sendSync(Messages::WebProcessProxy::GetGPUProcessConnection(), Messages::WebProcessProxy::GetGPUProcessConnection::Reply(connectionInfo), 0)) {
        // If we failed the first time, retry once. The attachment may have become invalid
        // before it was received by the web process if the network process crashed.
        if (!connection.sendSync(Messages::WebProcessProxy::GetGPUProcessConnection(), Messages::WebProcessProxy::GetGPUProcessConnection::Reply(connectionInfo), 0))
            CRASH();
    }

    return connectionInfo;
}

GPUProcessConnection& WebProcess::ensureGPUProcessConnection()
{
    RELEASE_ASSERT(RunLoop::isMain());

    // If we've lost our connection to the GPU process (e.g. it crashed) try to re-establish it.
    if (!m_gpuProcessConnection) {
        auto connectionInfo = getGPUProcessConnection(*parentProcessConnection());

        // Retry once if the IPC to get the connectionIdentifier succeeded but the connectionIdentifier we received
        // is invalid. This may indicate that the GPU process has crashed.
        if (!IPC::Connection::identifierIsValid(connectionInfo.identifier()))
            connectionInfo = getGPUProcessConnection(*parentProcessConnection());

        if (!IPC::Connection::identifierIsValid(connectionInfo.identifier()))
            CRASH();

        m_gpuProcessConnection = GPUProcessConnection::create(connectionInfo.releaseIdentifier());
#if HAVE(AUDIT_TOKEN)
        ASSERT(connectionInfo.auditToken);
        m_gpuProcessConnection->setAuditToken(WTFMove(connectionInfo.auditToken));
#endif
    }
    
    return *m_gpuProcessConnection;
}

void WebProcess::gpuProcessConnectionClosed(GPUProcessConnection* connection)
{
    ASSERT(m_gpuProcessConnection);
    ASSERT_UNUSED(connection, m_gpuProcessConnection == connection);

    m_gpuProcessConnection = nullptr;
}

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
LibWebRTCCodecs& WebProcess::libWebRTCCodecs()
{
    if (!m_libWebRTCCodecs)
        m_libWebRTCCodecs = makeUnique<LibWebRTCCodecs>();
    return *m_libWebRTCCodecs;
}
#endif

#endif // ENABLE(GPU_PROCESS)

void WebProcess::setEnhancedAccessibility(bool flag)
{
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(flag);
}
    
void WebProcess::startMemorySampler(SandboxExtension::Handle&& sampleLogFileHandle, const String& sampleLogFilePath, const double interval)
{
#if ENABLE(MEMORY_SAMPLER)    
    WebMemorySampler::singleton()->start(WTFMove(sampleLogFileHandle), sampleLogFilePath, interval);
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

void WebProcess::fetchWebsiteData(OptionSet<WebsiteDataType> websiteDataTypes, CompletionHandler<void(WebsiteData&&)>&& completionHandler)
{
    WebsiteData websiteData;
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        for (auto& origin : MemoryCache::singleton().originsWithCache(sessionID()))
            websiteData.entries.append(WebsiteData::Entry { origin->data(), WebsiteDataType::MemoryCache, 0 });
    }
    completionHandler(WTFMove(websiteData));
}

void WebProcess::deleteWebsiteData(OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    UNUSED_PARAM(modifiedSince);

    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        BackForwardCache::singleton().pruneToSizeNow(0, PruningReason::None);
        MemoryCache::singleton().evictResources(sessionID());

        CrossOriginPreflightResultCache::singleton().clear();
    }
    completionHandler();
}

void WebProcess::deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& originDatas, CompletionHandler<void()>&& completionHandler)
{
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        HashSet<RefPtr<SecurityOrigin>> origins;
        for (auto& originData : originDatas)
            origins.add(originData.securityOrigin());

        MemoryCache::singleton().removeResourcesWithOrigins(sessionID(), origins);
    }
    completionHandler();
}

void WebProcess::setHiddenPageDOMTimerThrottlingIncreaseLimit(int milliseconds)
{
    for (auto& page : m_pageMap.values())
        page->setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds::fromMilliseconds(milliseconds));
}

#if !PLATFORM(COCOA)
void WebProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
}

void WebProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void WebProcess::platformInitializeProcess(const AuxiliaryProcessInitializationParameters&)
{
}

void WebProcess::updateActivePages(const String& overrideDisplayName)
{
}

void WebProcess::getActivePagesOriginsForTesting(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebProcess::updateCPULimit()
{
}

void WebProcess::updateCPUMonitorState(CPUMonitorUpdateReason)
{
}

#endif

void WebProcess::pageActivityStateDidChange(PageIdentifier, OptionSet<WebCore::ActivityState::Flag> changed)
{
    if (changed & WebCore::ActivityState::IsVisible)
        updateCPUMonitorState(CPUMonitorUpdateReason::VisibilityHasChanged);
}

#if PLATFORM(IOS_FAMILY)
void WebProcess::resetAllGeolocationPermissions()
{
    for (auto& page : m_pageMap.values()) {
        if (Frame* mainFrame = page->mainFrame())
            mainFrame->resetAllGeolocationPermission();
    }
}
#endif

void WebProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "prepareToSuspend: isSuspensionImminent: %d", isSuspensionImminent);
    SetForScope<bool> suspensionScope(m_isSuspending, true);
    m_processIsSuspended = true;

    flushResourceLoadStatistics();

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent) {
        RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "prepareToSuspend: Process is ready to suspend");
        return completionHandler();
    }
#endif

#if ENABLE(VIDEO)
    suspendAllMediaBuffering();
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->processWillSuspend();
#endif

    if (!m_suppressMemoryPressureHandler)
        MemoryPressureHandler::singleton().releaseMemory(Critical::Yes, Synchronous::Yes);

    freezeAllLayerTrees();

#if PLATFORM(COCOA)
    destroyRenderingResources();
#endif

#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(true);
    SQLiteDatabase::setIsDatabaseOpeningForbidden(true);
    if (DatabaseTracker::isInitialized())
        DatabaseTracker::singleton().closeAllDatabases(CurrentQueryBehavior::Interrupt);
    accessibilityProcessSuspendedNotification(true);
    updateFreezerStatus();
#endif

    markAllLayersVolatile([this, completionHandler = WTFMove(completionHandler)]() mutable {
        RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "prepareToSuspend: Process is ready to suspend");
        completionHandler();
    });
}

void WebProcess::markAllLayersVolatile(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "markAllLayersVolatile:");
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& page : m_pageMap.values()) {
        page->markLayersVolatile([this, callbackAggregator, pageID = page->identifier()] (bool succeeded) {
            if (succeeded)
                RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "markAllLayersVolatile: Successfuly marked layers as volatile for webPageID=%" PRIu64, pageID.toUInt64());
            else
                RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSuspension, "markAllLayersVolatile: Failed to mark layers as volatile for webPageID=%" PRIu64, pageID.toUInt64());
        });
    }
}

void WebProcess::cancelMarkAllLayersVolatile()
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "cancelMarkAllLayersVolatile:");
    for (auto& page : m_pageMap.values())
        page->cancelMarkLayersVolatile();
}

void WebProcess::freezeAllLayerTrees()
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "freezeAllLayerTrees: WebProcess is freezing all layer trees");
    for (auto& page : m_pageMap.values())
        page->freezeLayerTree(WebPage::LayerTreeFreezeReason::ProcessSuspended);
}

void WebProcess::unfreezeAllLayerTrees()
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "unfreezeAllLayerTrees: WebProcess is unfreezing all layer trees");
    for (auto& page : m_pageMap.values())
        page->unfreezeLayerTree(WebPage::LayerTreeFreezeReason::ProcessSuspended);
}

void WebProcess::processDidResume()
{
    RELEASE_LOG_IF_ALLOWED(ProcessSuspension, "processDidResume:");

    m_processIsSuspended = false;

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent)
        return;
#endif

    cancelMarkAllLayersVolatile();
    unfreezeAllLayerTrees();
    
#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(false);
    SQLiteDatabase::setIsDatabaseOpeningForbidden(false);
    accessibilityProcessSuspendedNotification(false);
#endif

#if ENABLE(VIDEO)
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->processDidResume();
    resumeAllMediaBuffering();
#endif
}

void WebProcess::sendPrewarmInformation(const URL& url)
{
    auto registrableDomain = WebCore::RegistrableDomain { url };
    if (registrableDomain.isEmpty())
        return;
    parentProcessConnection()->send(Messages::WebProcessProxy::DidCollectPrewarmInformation(registrableDomain, WebCore::ProcessWarming::collectPrewarmInformation()), 0);
}

void WebProcess::pageDidEnterWindow(PageIdentifier pageID)
{
    m_pagesInWindows.add(pageID);
    m_nonVisibleProcessCleanupTimer.stop();
}

void WebProcess::pageWillLeaveWindow(PageIdentifier pageID)
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

void WebProcess::registerStorageAreaMap(StorageAreaMap& storageAreaMap)
{
    ASSERT(storageAreaMap.identifier());
    ASSERT(!m_storageAreaMaps.contains(*storageAreaMap.identifier()));
    m_storageAreaMaps.set(*storageAreaMap.identifier(), &storageAreaMap);
}

void WebProcess::unregisterStorageAreaMap(StorageAreaMap& storageAreaMap)
{
    ASSERT(storageAreaMap.identifier());
    ASSERT(m_storageAreaMaps.contains(*storageAreaMap.identifier()));
    ASSERT(m_storageAreaMaps.get(*storageAreaMap.identifier()) == &storageAreaMap);
    m_storageAreaMaps.remove(*storageAreaMap.identifier());
}

StorageAreaMap* WebProcess::storageAreaMap(StorageAreaIdentifier identifier) const
{
    return m_storageAreaMaps.get(identifier);
}

void WebProcess::setResourceLoadStatisticsEnabled(bool enabled)
{
    if (WebCore::DeprecatedGlobalSettings::resourceLoadStatisticsEnabled() == enabled)
        return;
    WebCore::DeprecatedGlobalSettings::setResourceLoadStatisticsEnabled(enabled);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (enabled && !ResourceLoadObserver::sharedIfExists())
        WebCore::ResourceLoadObserver::setShared(*new WebResourceLoadObserver(m_sessionID && m_sessionID->isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
#endif
}

void WebProcess::clearResourceLoadStatistics()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* observer = ResourceLoadObserver::sharedIfExists())
        observer->clearState();
#endif
}

void WebProcess::flushResourceLoadStatistics()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* observer = ResourceLoadObserver::sharedIfExists())
        observer->updateCentralStatisticsStore();
#endif
}

void WebProcess::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* observer = ResourceLoadObserver::sharedIfExists())
        observer->logSubresourceLoadingForTesting(firstPartyDomain, thirdPartyDomain, shouldScheduleNotification);
#endif
    completionHandler();
}

RefPtr<API::Object> WebProcess::transformHandlesToObjects(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        Transformer(WebProcess& webProcess)
            : m_webProcess(webProcess)
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
                return m_webProcess.webFrame(static_cast<const API::FrameHandle&>(object).frameID());

            case API::Object::Type::PageGroupHandle:
                return m_webProcess.webPageGroup(static_cast<const API::PageGroupHandle&>(object).webPageGroupData());

            case API::Object::Type::PageHandle:
                return m_webProcess.webPage(static_cast<const API::PageHandle&>(object).webPageID());

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
        bool shouldTransformObject(const API::Object& object) const override
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

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::BundleFrame:
                return API::FrameHandle::createAutoconverting(static_cast<const WebFrame&>(object).frameID());

            case API::Object::Type::BundlePage:
                return API::PageHandle::createAutoconverting(static_cast<const WebPage&>(object).webPageProxyIdentifier(), static_cast<const WebPage&>(object).identifier());

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

void WebProcess::ensureAutomationSessionProxy(const String& sessionIdentifier)
{
    m_automationSessionProxy = makeUnique<WebAutomationSessionProxy>(sessionIdentifier);
}

void WebProcess::destroyAutomationSessionProxy()
{
    m_automationSessionProxy = nullptr;
}

void WebProcess::prefetchDNS(const String& hostname)
{
    if (hostname.isEmpty())
        return;

    if (m_dnsPrefetchedHosts.add(hostname).isNewEntry)
        ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::PrefetchDNS(hostname), 0);
    // The DNS prefetched hosts cache is only to avoid asking for the same hosts too many times
    // in a very short period of time, producing a lot of IPC traffic. So we clear this cache after
    // some time of no DNS requests.
    m_dnsPrefetchHystereris.impulse();
}

bool WebProcess::hasVisibleWebPage() const
{
    for (auto& page : m_pageMap.values()) {
        if (page->isVisible())
            return true;
    }
    return false;
}

void WebProcess::setBackForwardCacheCapacity(unsigned capacity)
{
    BackForwardCache::singleton().setMaxSize(capacity);
}

void WebProcess::clearCachedPage(BackForwardItemIdentifier backForwardItemID, CompletionHandler<void()>&& completionHandler)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    if (!item)
        return completionHandler();

    BackForwardCache::singleton().remove(*item);
    completionHandler();
}

LibWebRTCNetwork& WebProcess::libWebRTCNetwork()
{
    if (!m_libWebRTCNetwork)
        m_libWebRTCNetwork = makeUnique<LibWebRTCNetwork>();
    return *m_libWebRTCNetwork;
}

#if ENABLE(SERVICE_WORKER)
void WebProcess::establishWorkerContextConnectionToNetworkProcess(uint64_t pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, RegistrableDomain&& registrableDomain, ServiceWorkerInitializationData&& initializationData, CompletionHandler<void()>&& completionHandler)
{
    // We are in the Service Worker context process and the call below establishes our connection to the Network Process
    // by calling ensureNetworkProcessConnection. SWContextManager needs to use the same underlying IPC::Connection as the
    // NetworkProcessConnection for synchronization purposes.
    auto& ipcConnection = ensureNetworkProcessConnection().connection();
    SWContextManager::singleton().setConnection(makeUnique<WebSWContextManagerConnection>(ipcConnection, WTFMove(registrableDomain), pageGroupID, webPageProxyID, pageID, store, WTFMove(initializationData)));
    SWContextManager::singleton().connection()->establishConnection(WTFMove(completionHandler));
}

void WebProcess::addServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier identifier)
{
    m_swRegistrationCounts.add(identifier);
}

bool WebProcess::removeServiceWorkerRegistration(WebCore::ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(m_swRegistrationCounts.contains(identifier));
    return m_swRegistrationCounts.remove(identifier);
}
#endif

#if ENABLE(MEDIA_STREAM)
void WebProcess::addMockMediaDevice(const WebCore::MockMediaDevice& device)
{
    MockRealtimeMediaSourceCenter::addDevice(device);
}

void WebProcess::clearMockMediaDevices()
{
    MockRealtimeMediaSourceCenter::setDevices({ });
}

void WebProcess::removeMockMediaDevice(const String& persistentId)
{
    MockRealtimeMediaSourceCenter::removeDevice(persistentId);
}

void WebProcess::resetMockMediaDevices()
{
    MockRealtimeMediaSourceCenter::resetDevices();
}

#if ENABLE(SANDBOX_EXTENSIONS)
void WebProcess::grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&& extensions)
{
    for (size_t i = 0; i < extensions.size(); i++) {
        const auto& extension = extensions[i];
        extension.second->consume();
        RELEASE_LOG_IF_ALLOWED(WebRTC, "grantUserMediaDeviceSandboxExtensions: granted extension %s", extension.first.utf8().data());
        m_mediaCaptureSandboxExtensions.add(extension.first, extension.second.copyRef());
    }
}

static inline void checkDocumentsCaptureStateConsistency(const Vector<String>& extensionIDs)
{
#if ASSERT_ENABLED
    bool isCapturingAudio = WTF::anyOf(Document::allDocumentsMap().values(), [](auto* document) {
        return document->mediaState() & MediaProducer::AudioCaptureMask;
    });
    bool isCapturingVideo = WTF::anyOf(Document::allDocumentsMap().values(), [](auto* document) {
        return document->mediaState() & MediaProducer::VideoCaptureMask;
    });

    if (isCapturingAudio)
        ASSERT(extensionIDs.findMatching([](auto& id) { return id.contains("microphone"); }) == notFound);
    if (isCapturingVideo)
        ASSERT(extensionIDs.findMatching([](auto& id) { return id.contains("camera"); }) == notFound);
#endif // ASSERT_ENABLED
}

void WebProcess::revokeUserMediaDeviceSandboxExtensions(const Vector<String>& extensionIDs)
{
    checkDocumentsCaptureStateConsistency(extensionIDs);

    for (const auto& extensionID : extensionIDs) {
        auto extension = m_mediaCaptureSandboxExtensions.take(extensionID);
        ASSERT(extension || MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled());
        if (extension) {
            extension->revoke();
            RELEASE_LOG_IF_ALLOWED(WebRTC, "revokeUserMediaDeviceSandboxExtensions: revoked extension %s", extensionID.utf8().data());
        }
    }
}
#endif
#endif

#if ENABLE(VIDEO)
void WebProcess::suspendAllMediaBuffering()
{
    for (auto& page : m_pageMap.values())
        page->suspendAllMediaBuffering();
}

void WebProcess::resumeAllMediaBuffering()
{
    for (auto& page : m_pageMap.values())
        page->resumeAllMediaBuffering();
}
#endif

void WebProcess::clearCurrentModifierStateForTesting()
{
    PlatformKeyboardEvent::setCurrentModifierState({ });
}

bool WebProcess::areAllPagesThrottleable() const
{
    return WTF::allOf(m_pageMap.values(), [](auto& page) {
        return page->isThrottleable();
    });
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebProcess::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode, CompletionHandler<void()>&& completionHandler)
{
    m_thirdPartyCookieBlockingMode = thirdPartyCookieBlockingMode;
    completionHandler();
}

void WebProcess::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    ResourceLoadObserver::shared().setDomainsWithUserInteraction(WTFMove(domains));
}
#endif

#if ENABLE(GPU_PROCESS)
void WebProcess::setUseGPUProcessForMedia(bool useGPUProcessForMedia)
{
    if (useGPUProcessForMedia == m_useGPUProcessForMedia)
        return;

    m_useGPUProcessForMedia = useGPUProcessForMedia;

#if ENABLE(ENCRYPTED_MEDIA)
    auto& cdmFactories = CDMFactory::registeredFactories();
    cdmFactories.clear();

    if (useGPUProcessForMedia)
        ensureGPUProcessConnection().cdmFactory().registerFactory(cdmFactories);
    else
        CDMFactory::platformRegisterFactories(cdmFactories);
#endif

#if USE(AUDIO_SESSION)
    if (useGPUProcessForMedia)
        AudioSession::setSharedSession(RemoteAudioSession::create(*this));
    else
        AudioSession::setSharedSession(AudioSession::create());
#endif

#if PLATFORM(IOS_FAMILY)
    if (useGPUProcessForMedia)
        MediaSessionHelper::setSharedHelper(makeUniqueRef<RemoteMediaSessionHelper>(*this));
    else
        MediaSessionHelper::resetSharedHelper();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (useGPUProcessForMedia)
        ensureGPUProcessConnection().legacyCDMFactory().registerFactory();
    else
        LegacyCDM::resetFactories();
#endif
}
#endif

} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
