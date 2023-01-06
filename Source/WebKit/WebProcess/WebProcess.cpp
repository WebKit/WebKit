/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
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
#include "APIPageHandle.h"
#include "AudioMediaStreamTrackRendererInternalUnitManager.h"
#include "AuthenticationManager.h"
#include "AuxiliaryProcessMessages.h"
#include "DrawingArea.h"
#include "EventDispatcher.h"
#include "GPUProcessConnectionParameters.h"
#include "InjectedBundle.h"
#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "ProcessAssertion.h"
#include "RemoteAudioHardwareListener.h"
#include "RemoteAudioSession.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteMediaEngineConfigurationFactory.h"
#include "RemoteRemoteCommandListener.h"
#include "RemoteWebLockRegistry.h"
#include "RemoteWorkerType.h"
#include "SpeechRecognitionRealtimeMediaSourceManager.h"
#include "StorageAreaMap.h"
#include "UserData.h"
#include "WebAutomationSessionProxy.h"
#include "WebBadgeClient.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebConnectionToUIProcess.h"
#include "WebCookieJar.h"
#include "WebCoreArgumentCoders.h"
#include "WebFileSystemStorageConnection.h"
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
#include "WebPermissionController.h"
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
#include "WebSharedWorkerContextManagerConnection.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerProvider.h"
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
#include <WebCore/MediaEngineConfigurationFactory.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/MessagePort.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PermissionController.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RemoteCommandListener.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedWorkerContextManager.h>
#include <WebCore/SharedWorkerThreadProxy.h>
#include <WebCore/UserGestureIndicator.h>
#include <pal/Logging.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/DateMath.h>
#include <wtf/Language.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextStream.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#include "ARKitInlinePreviewModelPlayerMac.h"
#endif

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#include "UserMediaCaptureManager.h"
#endif

#if USE(CG)
#include <WebCore/ImageDecoderCG.h>
#endif

#if PLATFORM(MAC)
#include <WebCore/DisplayRefreshMonitorManager.h>
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

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
#include "RemoteImageDecoderAVF.h"
#include <WebCore/ImageDecoder.h>
#endif

#if PLATFORM(COCOA)
#include <WebCore/SystemBattery.h>
#include <WebCore/VP9UtilitiesCocoa.h>
#endif

#if OS(LINUX)
#include <wtf/linux/RealTimeThreads.h>
#endif

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
#include "WebMockContentFilterManager.h"
#endif

#if HAVE(LSDATABASECONTEXT)
#include "LaunchServicesDatabaseManager.h"
#endif

#undef WEBPROCESS_RELEASE_LOG
#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#if RELEASE_LOG_DISABLED
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) UNUSED_VARIABLE(this)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) UNUSED_VARIABLE(this)
#else
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#endif

// This should be less than plugInAutoStartExpirationTimeThreshold in PlugInAutoStartProvider.
static const Seconds plugInAutoStartExpirationTimeUpdateThreshold { 29 * 24 * 60 * 60 };

// This should be greater than tileRevalidationTimeout in TileController.
static const Seconds nonVisibleProcessGraphicsCleanupDelay { 10_s };

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
// This should be long enough to support a workload where a user is actively switching between multiple tabs,
// since our memory cleanup routine could potentially delete a good amount of JIT code.
static const Seconds nonVisibleProcessMemoryCleanupDelay { 120_s };
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

NO_RETURN static void callExit(IPC::Connection*)
{
#if OS(WINDOWS)
    // Calling _exit in non-main threads may cause a deadlock in WTF::Thread::ThreadHolder::~ThreadHolder.
    TerminateProcess(GetCurrentProcess(), EXIT_SUCCESS);
#else
    _exit(EXIT_SUCCESS);
#endif
}

#if PLATFORM(GTK) || PLATFORM(WPE)
static void callExitSoon(IPC::Connection*)
{
    // If the connection has been closed and we haven't responded in the main thread for 10 seconds the process will exit forcibly.
    static const auto watchdogDelay = 10_s;
    WorkQueue::create("WebKit.WebProcess.WatchDogQueue")->dispatchAfter(watchdogDelay, [] {
        callExit(nullptr);
    });
}
#endif

WebProcess& WebProcess::singleton()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_webLoaderStrategy(*new WebLoaderStrategy)
#if PLATFORM(COCOA) && USE(LIBWEBRTC) && ENABLE(WEB_CODECS)
    , m_remoteVideoCodecFactory(*this)
#endif
    , m_cacheStorageProvider(WebCacheStorageProvider::create())
    , m_badgeClient(WebBadgeClient::create())
    , m_broadcastChannelRegistry(WebBroadcastChannelRegistry::create())
    , m_cookieJar(WebCookieJar::create())
    , m_dnsPrefetchHystereris([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) m_dnsPrefetchedHosts.clear(); })
    , m_nonVisibleProcessGraphicsCleanupTimer(*this, &WebProcess::nonVisibleProcessGraphicsCleanupTimerFired)
#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    , m_nonVisibleProcessMemoryCleanupTimer(*this, &WebProcess::nonVisibleProcessMemoryCleanupTimerFired)
#endif
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

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
    addSupplement<RemoteImageDecoderAVFManager>();
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

#if ENABLE(GPU_PROCESS)
    addSupplement<RemoteMediaEngineConfigurationFactory>();
#endif

    Gigacage::forbidDisablingPrimitiveGigacage();

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    WebMockContentFilterManager::singleton().startObservingSettings();
#endif

    WebCore::WebLockRegistry::setSharedRegistry(RemoteWebLockRegistry::create(*this));
    WebCore::PermissionController::setSharedController(WebPermissionController::create(*this));
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

// Do not call exit in background queue for GTK and WPE because we need to ensure
// atexit handlers are called in the main thread to cleanup resources like EGL displays.
// Unless the main thread doesn't exit after 10 senconds to avoid leaking the process.
#if PLATFORM(GTK) || PLATFORM(WPE)
    IPC::Connection::DidCloseOnConnectionWorkQueueCallback callExitCallback = callExitSoon;
#else
    // We call _exit() directly from the background queue in case the main thread is unresponsive
    // and AuxiliaryProcess::didClose() does not get called.
    IPC::Connection::DidCloseOnConnectionWorkQueueCallback callExitCallback = callExit;
#endif
    connection->setDidCloseOnConnectionWorkQueueCallback(callExitCallback);

#if !PLATFORM(GTK) && !PLATFORM(WPE) && !ENABLE(IPC_TESTING_API)
    connection->setShouldExitOnSyncMessageSendFailure(true);
#endif

    m_eventDispatcher.initializeConnection(*connection);
#if PLATFORM(IOS_FAMILY)
    m_viewUpdateDispatcher.initializeConnection(*connection);
#endif // PLATFORM(IOS_FAMILY)

    m_webInspectorInterruptDispatcher.initializeConnection(*connection);

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);

    m_webConnection = WebConnectionToUIProcess::create(this);
}

static void scheduleLogMemoryStatistics(LogMemoryStatisticsReason reason)
{
    // Log stats in the next turn of the run loop so that it runs after the low memory handler.
    RunLoop::main().dispatch([reason] {
        WebCore::logMemoryStatistics(reason);
    });
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
            // If this process contains only non-visible content (e.g. only contains background
            // tabs), then treat the memory warning as if it was a critical warning to maximize the
            // amount of memory released for foreground apps to use.
            if (m_pagesInWindows.isEmpty() && critical == Critical::No)
                critical = Critical::Yes;

#if PLATFORM(MAC)
            // If this is a process we keep around for performance, kill it on memory pressure instead of trying to free up its memory.
            if (m_processType == ProcessType::CachedWebContent || m_processType == ProcessType::PrewarmedWebContent || areAllPagesSuspended()) {
                if (m_processType == ProcessType::CachedWebContent)
                    WEBPROCESS_RELEASE_LOG(Process, "initializeWebProcess: Cached WebProcess is exiting due to memory pressure");
                else if (m_processType == ProcessType::PrewarmedWebContent)
                    WEBPROCESS_RELEASE_LOG(Process, "initializeWebProcess: Prewarmed WebProcess is exiting due to memory pressure");
                else
                    WEBPROCESS_RELEASE_LOG(Process, "initializeWebProcess: Suspended WebProcess is exiting due to memory pressure");
                stopRunLoop();
                return;
            }
#endif

            auto maintainBackForwardCache = m_isSuspending ? WebCore::MaintainBackForwardCache::Yes : WebCore::MaintainBackForwardCache::No;
            auto maintainMemoryCache = m_isSuspending && m_hasSuspendedPageProxy ? WebCore::MaintainMemoryCache::Yes : WebCore::MaintainMemoryCache::No;
            WebCore::releaseMemory(critical, synchronous, maintainBackForwardCache, maintainMemoryCache);
            for (auto& page : m_pageMap.values())
                page->releaseMemory(critical);
        });
#if ENABLE(PERIODIC_MEMORY_MONITOR)
        memoryPressureHandler.setShouldUsePeriodicMemoryMonitor(true);
        memoryPressureHandler.setMemoryKillCallback([this] () {
            WebCore::logMemoryStatistics(LogMemoryStatisticsReason::OutOfMemoryDeath);
            if (MemoryPressureHandler::singleton().processState() == WebsamProcessState::Active)
                parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedActiveMemoryLimit(), 0);
            else
                parentProcessConnection()->send(Messages::WebProcessProxy::DidExceedInactiveMemoryLimit(), 0);
        });
#endif
        memoryPressureHandler.setMemoryPressureStatusChangedCallback([this](WTF::MemoryPressureStatus memoryPressureStatus) {
            if (parentProcessConnection())
                parentProcessConnection()->send(Messages::WebProcessProxy::MemoryPressureStatusChanged(MemoryPressureHandler::singleton().isUnderMemoryPressure()), 0);

            if (memoryPressureStatus == WTF::MemoryPressureStatus::ProcessLimitWarning && !m_loggedProcessLimitWarningMemoryStatistics) {
                m_loggedProcessLimitWarningMemoryStatistics = true;
                scheduleLogMemoryStatistics(LogMemoryStatisticsReason::WarningMemoryPressureNotification);
            } else if (memoryPressureStatus == WTF::MemoryPressureStatus::ProcessLimitCritical && !m_loggedProcessLimitCriticalMemoryStatistics) {
                m_loggedProcessLimitCriticalMemoryStatistics = true;
                scheduleLogMemoryStatistics(LogMemoryStatisticsReason::CriticalMemoryPressureNotification);
            }
        });
        memoryPressureHandler.install();

        PAL::registerNotifyCallback("com.apple.WebKit.logMemStats"_s, [] {
            WebCore::logMemoryStatistics(LogMemoryStatisticsReason::DebugNotification);
        });
    }

#if !RELEASE_LOG_DISABLED
    PAL::registerNotifyCallback("com.apple.WebKit.logPageState"_s, [this] {
        for (auto& page : m_pageMap.values()) {
            int64_t loadCommitTime = 0;
#if USE(OS_STATE)
            loadCommitTime = static_cast<int64_t>(page->loadCommitTime().secondsSinceEpoch().seconds());
#endif

            WTF::TextStream activityStateStream(WTF::TextStream::LineMode::SingleLine);
            activityStateStream << page->activityState();

            RELEASE_LOG(ActivityState, "WebPage %p - load_time: %lld, visible: %d, throttleable: %d , suspended: %d , websam_state: %" PUBLIC_LOG_STRING ", activity_state: %" PUBLIC_LOG_STRING ", url: %" PRIVATE_LOG_STRING, page.get(), loadCommitTime, page->isVisible(), page->isThrottleable(), page->isSuspended(), MemoryPressureHandler::processStateDescription().characters(), activityStateStream.release().utf8().data(), page->mainWebFrame().url().string().utf8().data());
        }
    });
#endif

    SandboxExtension::consumePermanently(parameters.additionalSandboxExtensionHandles);

    if (!parameters.injectedBundlePath.isEmpty())
        m_injectedBundle = InjectedBundle::create(parameters, transformHandlesToObjects(parameters.initializationUserData.object()).get());

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);

    setCacheModel(parameters.cacheModel);

    if (!parameters.timeZoneOverride.isEmpty())
        setTimeZoneOverride(parameters.timeZoneOverride);

    if (!parameters.overrideLanguages.isEmpty()) {
        LOG_WITH_STREAM(Language, stream << "Web Process initialization is setting overrideLanguages: " << parameters.overrideLanguages);
        overrideUserPreferredLanguages(parameters.overrideLanguages);
    } else
        LOG(Language, "Web process initialization is not setting overrideLanguages");

    m_textCheckerState = parameters.textCheckerState;

    m_fullKeyboardAccessEnabled = parameters.fullKeyboardAccessEnabled;

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    m_hasMouseDevice = parameters.hasMouseDevice;
#endif

#if HAVE(STYLUS_DEVICE_OBSERVATION)
    m_hasStylusDevice = parameters.hasStylusDevice;
#endif

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

    setDisableFontSubpixelAntialiasingForTesting(parameters.disableFontSubpixelAntialiasingForTesting);

    setMemoryCacheDisabled(parameters.memoryCacheDisabled);

    WebCore::DeprecatedGlobalSettings::setAttrStyleEnabled(parameters.attrStyleEnabled);
    
    commonVM().setGlobalConstRedeclarationShouldThrow(parameters.shouldThrowExceptionForGlobalConstantRedeclaration);

    ScriptExecutionContext::setCrossOriginMode(parameters.crossOriginMode);
    m_isLockdownModeEnabled = parameters.isLockdownModeEnabled;
    DeprecatedGlobalSettings::setArePDFImagesEnabled(!m_isLockdownModeEnabled);

#if ENABLE(SERVICE_CONTROLS)
    setEnabledServices(parameters.hasImageServices, parameters.hasSelectionServices, parameters.hasRichContentServices);
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA)
    if (std::optional<audit_token_t> auditToken = parentProcessConnection()->getAuditToken()) {
        RetainPtr<CFDataRef> auditData = adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
        Inspector::RemoteInspector::singleton().setParentProcessInformation(WebCore::presentingApplicationPID(), auditData);
    }
#endif

#if ENABLE(GAMEPAD)
    GamepadProvider::singleton().setSharedProvider(WebGamepadProvider::singleton());
#endif

#if ENABLE(SERVICE_WORKER)
    ServiceWorkerProvider::setSharedProvider(WebServiceWorkerProvider::singleton());
#endif
    SharedWorkerProvider::setSharedProvider(WebSharedWorkerProvider::singleton());

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
    WebResourceLoadObserver::setShouldLogUserInteraction(parameters.shouldLogUserInteraction);
#endif

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent)
        prewarmGlobally();
#endif

    WEBPROCESS_RELEASE_LOG(Process, "initializeWebProcess: Presenting processPID=%d", WebCore::presentingApplicationPID());
}

void WebProcess::setWebsiteDataStoreParameters(WebProcessDataStoreParameters&& parameters)
{
    ASSERT(!m_sessionID);
    m_sessionID = parameters.sessionID;

    // FIXME: This should be constructed per data store, not per process.
    m_applicationCacheStorage = ApplicationCacheStorage::create(parameters.applicationCacheDirectory, parameters.applicationCacheFlatFileSubdirectoryName);
#if PLATFORM(IOS_FAMILY)
    m_applicationCacheStorage->setDefaultOriginQuota(25ULL * 1024 * 1024);
#endif

#if ENABLE(VIDEO)
    if (!parameters.mediaCacheDirectory.isEmpty())
        WebCore::HTMLMediaElement::setMediaCacheDirectory(parameters.mediaCacheDirectory);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    if (!parameters.modelElementCacheDirectory.isEmpty())
        ARKitInlinePreviewModelPlayerMac::setModelElementCacheDirectory(parameters.modelElementCacheDirectory);
#endif

    setTrackingPreventionEnabled(parameters.trackingPreventionEnabled);

#if ENABLE(TRACKING_PREVENTION)
    m_thirdPartyCookieBlockingMode = parameters.thirdPartyCookieBlockingMode;
    if (parameters.trackingPreventionEnabled) {
        if (!ResourceLoadObserver::sharedIfExists())
            ResourceLoadObserver::setShared(*new WebResourceLoadObserver(parameters.sessionID.isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
        ResourceLoadObserver::shared().setDomainsWithUserInteraction(WTFMove(parameters.domainsWithUserInteraction));
        if (!parameters.sessionID.isEphemeral())
            ResourceLoadObserver::shared().setDomainsWithCrossPageStorageAccess(WTFMove(parameters.domainsWithStorageAccessQuirk), [] { });
    }
    
#endif

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

    updateProcessName(IsInProcessInitialization::No);

    IPC::AccessibilityProcessSuspendedNotification(isInProcessCache);
#else
    UNUSED_PARAM(isInProcessCache);
#endif
}

void WebProcess::markIsNoLongerPrewarmed()
{
#if PLATFORM(COCOA)
    ASSERT(m_processType == ProcessType::PrewarmedWebContent);
    m_processType = ProcessType::WebContent;

    updateProcessName(IsInProcessInitialization::No);
#endif
}

void WebProcess::prewarmGlobally()
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        RELEASE_LOG(PerformanceLogging, "WebProcess::prewarmGlobally: Not prewarming because the system in under memory pressure");
        return;
    }
    WebCore::ProcessWarming::prewarmGlobally();
}

void WebProcess::prewarmWithDomainInformation(WebCore::PrewarmInformation&& prewarmInformation)
{
    WebCore::ProcessWarming::prewarmWithInformation(WTFMove(prewarmInformation));
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
    WebCore::FontCascade::setCodePath(alwaysUseComplexText ? WebCore::FontCascade::CodePath::Complex : WebCore::FontCascade::CodePath::Auto);
}

void WebProcess::setDisableFontSubpixelAntialiasingForTesting(bool disable)
{
    WebCore::FontCascade::setDisableFontSubpixelAntialiasingForTesting(disable);
}

void WebProcess::userPreferredLanguagesChanged(const Vector<String>& languages) const
{
    LOG_WITH_STREAM(Language, stream << "The web process's userPreferredLanguagesChanged: " << languages);
    overrideUserPreferredLanguages(languages);
}

void WebProcess::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    m_fullKeyboardAccessEnabled = fullKeyboardAccessEnabled;
}

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
        auto page = WebPage::create(pageID, WTFMove(parameters));
        result.iterator->value = page.ptr();

#if ENABLE(GPU_PROCESS)
        if (m_gpuProcessConnection)
            page->gpuProcessConnectionDidBecomeAvailable(*m_gpuProcessConnection);
#endif

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
        updateCPULimit();
#if OS(LINUX)
        RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
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
#if OS(LINUX)
    RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
}

bool WebProcess::shouldTerminate()
{
    ASSERT(m_pageMap.isEmpty());

    // FIXME: the ShouldTerminate message should also send termination parameters, such as any session cookies that need to be preserved.
    auto sendResult = parentProcessConnection()->sendSync(Messages::WebProcessProxy::ShouldTerminate(), 0);
    auto [shouldTerminate] = sendResult.takeReplyOr(true);
    return shouldTerminate;
}

void WebProcess::terminate()
{
#ifndef NDEBUG
    // These are done in an attempt to reduce LEAK output.
    GCController::singleton().garbageCollectNow();
    FontCache::invalidateAllFontCaches();
    MemoryCache::singleton().setDisabled(true);
#endif

    m_webConnection->invalidate();
    m_webConnection = nullptr;

    platformTerminate();

    AuxiliaryProcess::terminate();
}

bool WebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;
    return false;
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
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }
#endif

    if (decoder.messageReceiverName() == Messages::WebSharedWorkerContextManagerConnection::messageReceiverName()) {
        ASSERT(SharedWorkerContextManager::singleton().connection());
        if (auto* contextManagerConnection = SharedWorkerContextManager::singleton().connection())
            static_cast<WebSharedWorkerContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }

    LOG_ERROR("Unhandled web process message '%s' (destination: %" PRIu64 " pid: %d)", description(decoder.messageName()), static_cast<uint64_t>(decoder.destinationID()), static_cast<int>(getCurrentProcessID()));
}

void WebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(VIDEO)
    FileSystem::markPurgeable(WebCore::HTMLMediaElement::mediaCacheDirectory());
#endif
    if (m_applicationCacheStorage)
        FileSystem::markPurgeable(m_applicationCacheStorage->cacheDirectory());
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    FileSystem::markPurgeable(ARKitInlinePreviewModelPlayerMac::modelElementCacheDirectory());
#endif
    AuxiliaryProcess::didClose(connection);
}

WebFrame* WebProcess::webFrame(FrameIdentifier frameID) const
{
    return m_frameMap.get(frameID).get();
}

void WebProcess::addWebFrame(FrameIdentifier frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebProcess::removeWebFrame(FrameIdentifier frameID, std::optional<WebPageProxyIdentifier> pageID)
{
    m_frameMap.remove(frameID);

    // We can end up here after our connection has closed when WebCore's frame life-support timer
    // fires when the application is shutting down. There's no need (and no way) to update the UI
    // process in this case.
    if (!parentProcessConnection())
        return;

    if (!pageID)
        return;

    parentProcessConnection()->send(Messages::WebProcessProxy::DidDestroyFrame(frameID, *pageID), 0);
}

WebPageGroupProxy* WebProcess::webPageGroup(PageGroup* pageGroup)
{
    for (auto& page : m_pageGroupMap.values()) {
        if (page->corePageGroup() == pageGroup)
            return page.get();
    }

    return 0;
}

WebPageGroupProxy* WebProcess::webPageGroup(PageGroupIdentifier pageGroupID)
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

void WebProcess::isJITEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(JSC::Options::useJIT());
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    GCController::singleton().garbageCollectNow();
}

void WebProcess::backgroundResponsivenessPing()
{
    parentProcessConnection()->send(Messages::WebProcessProxy::DidReceiveBackgroundResponsivenessPing(), 0);
}

void WebProcess::messagesAvailableForPort(const MessagePortIdentifier& identifier)
{
    MessagePort::notifyMessageAvailable(identifier);
}

#if HAVE(MOUSE_DEVICE_OBSERVATION)

void WebProcess::setHasMouseDevice(bool hasMouseDevice)
{
    if (hasMouseDevice == m_hasMouseDevice)
        return;

    m_hasMouseDevice = hasMouseDevice;

    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

#endif // HAVE(MOUSE_DEVICE_OBSERVATION)

#if HAVE(STYLUS_DEVICE_OBSERVATION)

void WebProcess::setHasStylusDevice(bool hasStylusDevice)
{
    if (hasStylusDevice == m_hasStylusDevice)
        return;

    m_hasStylusDevice = hasStylusDevice;

    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

#endif // HAVE(STYLUS_DEVICE_OBSERVATION)

#if ENABLE(GAMEPAD)

void WebProcess::setInitialGamepads(const Vector<WebKit::GamepadData>& gamepadDatas)
{
    WebGamepadProvider::singleton().setInitialGamepads(gamepadDatas);
}

void WebProcess::gamepadConnected(const GamepadData& gamepadData, WebCore::EventMakesGamepadsVisible eventVisibility)
{
    WebGamepadProvider::singleton().gamepadConnected(gamepadData, eventVisibility);
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

NO_RETURN inline void failedToGetNetworkProcessConnection()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    // GTK and WPE ports don't exit on send sync message failure.
    // In this particular case, the network process can be terminated by the UI process while the
    // Web process is still initializing, so we always want to exit instead of crashing. This can
    // happen when the WebView is created and then destroyed quickly.
    // See https://bugs.webkit.org/show_bug.cgi?id=183348.
    exit(0);
#else
    CRASH();
#endif
}

static NetworkProcessConnectionInfo getNetworkProcessConnection(IPC::Connection& connection)
{
    NetworkProcessConnectionInfo connectionInfo;
    auto requestConnection = [&]() -> bool {
        auto sendResult = connection.sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(), 0);
        if (!sendResult) {
            RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to send message or receive invalid message");
            failedToGetNetworkProcessConnection();
        }
        std::tie(connectionInfo) = sendResult.takeReply();
        return !!connectionInfo.connection;
    };

    static constexpr unsigned maxFailedAttempts = 30;
    unsigned failedAttempts = 0;
    while (!requestConnection()) {
        if (++failedAttempts >= maxFailedAttempts)
            failedToGetNetworkProcessConnection();

        RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will retry...");

        // If we failed, retry after a delay. The attachment may have become invalid
        // before it was received by the web process if the network process crashed.
        sleep(100_ms);
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

        m_networkProcessConnection = NetworkProcessConnection::create(IPC::Connection::Identifier { WTFMove(connectionInfo.connection) }, connectionInfo.cookieAcceptPolicy);
#if HAVE(AUDIT_TOKEN)
        m_networkProcessConnection->setNetworkProcessAuditToken(WTFMove(connectionInfo.auditToken));
#endif
        m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::RegisterURLSchemesAsCORSEnabled(WebCore::LegacySchemeRegistry::allURLSchemesRegisteredAsCORSEnabled()), 0);

#if ENABLE(SERVICE_WORKER)
        if (!Document::allDocuments().isEmpty() || SharedWorkerThreadProxy::hasInstances())
            m_networkProcessConnection->serviceWorkerConnection().registerServiceWorkerClients();
#endif

#if HAVE(LSDATABASECONTEXT)
        // On Mac, this needs to be called before NSApplication is being initialized.
        // The NSApplication initialization is being done in [NSApplication _accessibilityInitialize]
        LaunchServicesDatabaseManager::singleton().waitForDatabaseUpdate();
#endif

        // This can be called during a WebPage's constructor, so wait until after the constructor returns to touch the WebPage.
        RunLoop::main().dispatch([this] {
            for (auto& webPage : m_pageMap.values())
                webPage->synchronizeCORSDisablingPatternsWithNetworkProcess();
        });
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
#if OS(DARWIN)
    WEBPROCESS_RELEASE_LOG(Loading, "networkProcessConnectionClosed: NetworkProcess (%d) closed its connection (Crashed)", connection ? connection->connection().remoteProcessID() : 0);
#else
    WEBPROCESS_RELEASE_LOG(Loading, "networkProcessConnectionClosed: NetworkProcess closed its connection (Crashed)");
#endif

    ASSERT(m_networkProcessConnection);
    ASSERT_UNUSED(connection, m_networkProcessConnection == connection);

    for (auto key : copyToVector(m_storageAreaMaps.keys())) {
        if (auto map = m_storageAreaMaps.get(key))
            map->disconnect();
    }

    for (auto& page : m_pageMap.values()) {
        auto idbConnection = page->corePage()->optionalIDBConnection();
        if (!idbConnection)
            continue;
        
        if (auto* existingIDBConnectionToServer = connection->existingIDBConnectionToServer()) {
            ASSERT_UNUSED(existingIDBConnectionToServer, idbConnection == &existingIDBConnectionToServer->coreConnectionToServer());
            page->corePage()->clearIDBConnection();
        }
    }

#if ENABLE(SERVICE_WORKER)
    if (SWContextManager::singleton().connection())
        SWContextManager::singleton().stopAllServiceWorkers();
#endif

    m_networkProcessConnection = nullptr;

    logDiagnosticMessageForNetworkProcessCrash();

    m_webLoaderStrategy.networkProcessCrashed();
    WebSocketStream::networkProcessCrashed();
    m_webSocketChannelManager.networkProcessCrashed();
    m_broadcastChannelRegistry->networkProcessCrashed();

    if (m_libWebRTCNetwork)
        m_libWebRTCNetwork->networkProcessCrashed();

    for (auto& page : m_pageMap.values()) {
        page->stopAllURLSchemeTasks();
#if ENABLE(APPLE_PAY)
        if (auto paymentCoordinator = page->paymentCoordinator())
            paymentCoordinator->networkProcessConnectionClosed();
#endif
    }

    // Recreate a new connection with valid IPC connection on next operation.
    if (m_fileSystemStorageConnection) {
        m_fileSystemStorageConnection->connectionClosed();
        m_fileSystemStorageConnection = nullptr;
    }

    m_cacheStorageProvider->networkProcessConnectionClosed();
}

WebFileSystemStorageConnection& WebProcess::fileSystemStorageConnection()
{
    if (!m_fileSystemStorageConnection)
        m_fileSystemStorageConnection = WebFileSystemStorageConnection::create(ensureNetworkProcessConnection().connection());

    return *m_fileSystemStorageConnection;
}

WebLoaderStrategy& WebProcess::webLoaderStrategy()
{
    return m_webLoaderStrategy;
}

#if ENABLE(GPU_PROCESS)

GPUProcessConnection& WebProcess::ensureGPUProcessConnection()
{
    RELEASE_ASSERT(RunLoop::isMain());

    // If we've lost our connection to the GPU process (e.g. it crashed) try to re-establish it.
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = GPUProcessConnection::create(*parentProcessConnection());

        for (auto& page : m_pageMap.values()) {
            // If page is null, then it is currently being constructed.
            if (page)
                page->gpuProcessConnectionDidBecomeAvailable(*m_gpuProcessConnection);
        }
    }
    
    return *m_gpuProcessConnection;
}

void WebProcess::gpuProcessConnectionClosed(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection);
    ASSERT_UNUSED(connection, m_gpuProcessConnection == &connection);

    m_gpuProcessConnection = nullptr;

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    if (m_audioMediaStreamTrackRendererInternalUnitManager)
        m_audioMediaStreamTrackRendererInternalUnitManager->restartAllUnits();
#endif
}

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
LibWebRTCCodecs& WebProcess::libWebRTCCodecs()
{
    if (!m_libWebRTCCodecs)
        m_libWebRTCCodecs = LibWebRTCCodecs::create();
    return *m_libWebRTCCodecs;
}
#endif

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
AudioMediaStreamTrackRendererInternalUnitManager& WebProcess::audioMediaStreamTrackRendererInternalUnitManager()
{
    if (!m_audioMediaStreamTrackRendererInternalUnitManager)
        m_audioMediaStreamTrackRendererInternalUnitManager = makeUnique<AudioMediaStreamTrackRendererInternalUnitManager>();
    return *m_audioMediaStreamTrackRendererInternalUnitManager;
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
        auto origins = MemoryCache::singleton().originsWithCache(sessionID());
        websiteData.entries = WTF::map(origins, [](auto& origin) {
            return WebsiteData::Entry { origin->data(), WebsiteDataType::MemoryCache, 0 };
        });
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

void WebProcess::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    m_cookieJar->clearCache();
    completionHandler();
}

void WebProcess::deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& originDatas, CompletionHandler<void()>&& completionHandler)
{
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        HashSet<RefPtr<SecurityOrigin>> origins;
        for (auto& originData : originDatas)
            origins.add(originData.securityOrigin());

        MemoryCache::singleton().removeResourcesWithOrigins(sessionID(), origins);
        BackForwardCache::singleton().clearEntriesForOrigins(origins);
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
    if (changed & WebCore::ActivityState::IsVisible) {
        updateCPUMonitorState(CPUMonitorUpdateReason::VisibilityHasChanged);
#if OS(LINUX)
        RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
    }
}

void WebProcess::prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&& completionHandler)
{
#if !RELEASE_LOG_DISABLED
    auto nowTime = MonotonicTime::now();
    double remainingRunTime = nowTime > estimatedSuspendTime ? (nowTime - estimatedSuspendTime).value() : 0.0;
#endif
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "prepareToSuspend: isSuspensionImminent=%d, remainingRunTime=%fs", isSuspensionImminent, remainingRunTime);
    SetForScope suspensionScope(m_isSuspending, true);
    m_processIsSuspended = true;

    flushResourceLoadStatistics();

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent) {
        WEBPROCESS_RELEASE_LOG(ProcessSuspension, "prepareToSuspend: Process is ready to suspend");
        return completionHandler();
    }
#endif

#if ENABLE(VIDEO)
    suspendAllMediaBuffering();
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->processWillSuspend();
#endif

    if (!m_suppressMemoryPressureHandler) {
        MemoryPressureHandler::singleton().releaseMemory(Critical::Yes, Synchronous::Yes);
        for (auto& page : m_pageMap.values())
            page->releaseMemory(Critical::Yes);
    }

    freezeAllLayerTrees();

#if PLATFORM(COCOA)
    destroyRenderingResources();
#endif

#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(true);
    SQLiteDatabase::setIsDatabaseOpeningForbidden(true);
    IPC::AccessibilityProcessSuspendedNotification(true);
    updateFreezerStatus();
#endif

    markAllLayersVolatile([this, completionHandler = WTFMove(completionHandler)]() mutable {
        WEBPROCESS_RELEASE_LOG(ProcessSuspension, "prepareToSuspend: Process is ready to suspend");
        completionHandler();
    });
}

void WebProcess::markAllLayersVolatile(CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "markAllLayersVolatile:");
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& page : m_pageMap.values()) {
        page->markLayersVolatile([this, callbackAggregator, pageID = page->identifier()] (bool succeeded) {
            if (succeeded)
                WEBPROCESS_RELEASE_LOG(ProcessSuspension, "markAllLayersVolatile: Successfuly marked layers as volatile for webPageID=%" PRIu64, pageID.toUInt64());
            else
                WEBPROCESS_RELEASE_LOG_ERROR(ProcessSuspension, "markAllLayersVolatile: Failed to mark layers as volatile for webPageID=%" PRIu64, pageID.toUInt64());
        });
    }
}

void WebProcess::cancelMarkAllLayersVolatile()
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "cancelMarkAllLayersVolatile:");
    for (auto& page : m_pageMap.values())
        page->cancelMarkLayersVolatile();
}

void WebProcess::freezeAllLayerTrees()
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "freezeAllLayerTrees: WebProcess is freezing all layer trees");
    for (auto& page : m_pageMap.values())
        page->freezeLayerTree(WebPage::LayerTreeFreezeReason::ProcessSuspended);
}

void WebProcess::unfreezeAllLayerTrees()
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "unfreezeAllLayerTrees: WebProcess is unfreezing all layer trees");
    for (auto& page : m_pageMap.values())
        page->unfreezeLayerTree(WebPage::LayerTreeFreezeReason::ProcessSuspended);
}

void WebProcess::processDidResume()
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "processDidResume:");

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
    IPC::AccessibilityProcessSuspendedNotification(false);
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
    m_nonVisibleProcessGraphicsCleanupTimer.stop();

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    m_nonVisibleProcessMemoryCleanupTimer.stop();
#endif
}

void WebProcess::pageWillLeaveWindow(PageIdentifier pageID)
{
    m_pagesInWindows.remove(pageID);

    if (m_pagesInWindows.isEmpty()) {
        if (!m_nonVisibleProcessGraphicsCleanupTimer.isActive())
            m_nonVisibleProcessGraphicsCleanupTimer.startOneShot(nonVisibleProcessGraphicsCleanupDelay);

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
        if (!m_nonVisibleProcessMemoryCleanupTimer.isActive())
            m_nonVisibleProcessMemoryCleanupTimer.startOneShot(nonVisibleProcessMemoryCleanupDelay);
#endif
    }
}
    
void WebProcess::nonVisibleProcessGraphicsCleanupTimerFired()
{
    ASSERT(m_pagesInWindows.isEmpty());
    if (!m_pagesInWindows.isEmpty())
        return;

#if PLATFORM(COCOA)
    destroyRenderingResources();
#endif
}

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
void WebProcess::nonVisibleProcessMemoryCleanupTimerFired()
{
    ASSERT(m_pagesInWindows.isEmpty());
    if (!m_pagesInWindows.isEmpty())
        return;

    // If this is a process that we keep around for performance, then don't proactively slim it down until absolutely necessary (in the memory pressure handler).
    if (m_processType == ProcessType::CachedWebContent || areAllPagesSuspended())
        return;

    WebCore::releaseMemory(Critical::Yes, Synchronous::No, MaintainBackForwardCache::Yes, MaintainMemoryCache::No);
    for (auto& page : m_pageMap.values())
        page->releaseMemory(Critical::Yes);
}
#endif

void WebProcess::registerStorageAreaMap(StorageAreaMap& storageAreaMap)
{
    auto identifier = storageAreaMap.identifier();
    ASSERT(!m_storageAreaMaps.contains(identifier));
    m_storageAreaMaps.add(identifier, storageAreaMap);
}

void WebProcess::unregisterStorageAreaMap(StorageAreaMap& storageAreaMap)
{
    auto identifier = storageAreaMap.identifier();
    ASSERT(m_storageAreaMaps.contains(identifier));
    ASSERT(m_storageAreaMaps.get(identifier).get() == &storageAreaMap);
    m_storageAreaMaps.remove(identifier);
}

WeakPtr<StorageAreaMap> WebProcess::storageAreaMap(StorageAreaMapIdentifier identifier) const
{
    return m_storageAreaMaps.get(identifier);
}

void WebProcess::setTrackingPreventionEnabled(bool enabled)
{
    if (WebCore::DeprecatedGlobalSettings::trackingPreventionEnabled() == enabled)
        return;
    WebCore::DeprecatedGlobalSettings::setTrackingPreventionEnabled(enabled);
#if ENABLE(TRACKING_PREVENTION)
    if (enabled && !ResourceLoadObserver::sharedIfExists())
        WebCore::ResourceLoadObserver::setShared(*new WebResourceLoadObserver(m_sessionID && m_sessionID->isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
#endif
}

void WebProcess::clearResourceLoadStatistics()
{
#if ENABLE(TRACKING_PREVENTION)
    if (auto* observer = ResourceLoadObserver::sharedIfExists())
        observer->clearState();
    for (auto& page : m_pageMap.values())
        page->clearPageLevelStorageAccess();
#endif
}

void WebProcess::flushResourceLoadStatistics()
{
#if ENABLE(TRACKING_PREVENTION)
    if (auto* observer = ResourceLoadObserver::sharedIfExists())
        observer->updateCentralStatisticsStore([] { });
#endif
}

void WebProcess::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(TRACKING_PREVENTION)
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
        m_libWebRTCNetwork = LibWebRTCNetwork::create().moveToUniquePtr();
    return *m_libWebRTCNetwork;
}

void WebProcess::establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType workerType, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, RegistrableDomain&& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, RemoteWorkerInitializationData&& initializationData, CompletionHandler<void()>&& completionHandler)
{
    // We are in the Remote Worker context process and the call below establishes our connection to the Network Process
    // by calling ensureNetworkProcessConnection. SWContextManager / SharedWorkerContextManager need to use the same underlying IPC::Connection as the
    // NetworkProcessConnection for synchronization purposes.
    auto& ipcConnection = ensureNetworkProcessConnection().connection();
    switch (workerType) {
    case RemoteWorkerType::ServiceWorker:
#if ENABLE(SERVICE_WORKER)
        SWContextManager::singleton().setConnection(WebSWContextManagerConnection::create(ipcConnection, WTFMove(registrableDomain), serviceWorkerPageIdentifier, pageGroupID, webPageProxyID, pageID, store, WTFMove(initializationData)));
        SWContextManager::singleton().connection()->establishConnection(WTFMove(completionHandler));
#endif
        break;
    case RemoteWorkerType::SharedWorker:
        SharedWorkerContextManager::singleton().setConnection(makeUnique<WebSharedWorkerContextManagerConnection>(ipcConnection, WTFMove(registrableDomain), pageGroupID, webPageProxyID, pageID, store, WTFMove(initializationData)));
        SharedWorkerContextManager::singleton().connection()->establishConnection(WTFMove(completionHandler));
        break;
    }
}

#if ENABLE(SERVICE_WORKER)
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

void WebProcess::setMockMediaDeviceIsEphemeral(const String& persistentId, bool isEphemeral)
{
    MockRealtimeMediaSourceCenter::setDeviceIsEphemeral(persistentId, isEphemeral);
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
        WEBPROCESS_RELEASE_LOG(WebRTC, "grantUserMediaDeviceSandboxExtensions: granted extension %s", extension.first.utf8().data());
        m_mediaCaptureSandboxExtensions.add(extension.first, extension.second.copyRef());
    }
    m_machBootstrapExtension = extensions.machBootstrapExtension();
    if (m_machBootstrapExtension)
        m_machBootstrapExtension->consume();
}

static inline void checkDocumentsCaptureStateConsistency(const Vector<String>& extensionIDs)
{
#if ASSERT_ENABLED
    bool isCapturingAudio = WTF::anyOf(Document::allDocumentsMap().values(), [](auto* document) {
        return document->mediaState() & MediaProducer::MicrophoneCaptureMask;
    });
    bool isCapturingVideo = WTF::anyOf(Document::allDocumentsMap().values(), [](auto* document) {
        return document->mediaState() & MediaProducer::VideoCaptureMask;
    });

    if (isCapturingAudio)
        ASSERT(!extensionIDs.containsIf([](auto& id) { return id.contains("microphone"_s); }));
    if (isCapturingVideo)
        ASSERT(!extensionIDs.containsIf([](auto& id) { return id.contains("camera"_s); }));
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
            WEBPROCESS_RELEASE_LOG(WebRTC, "revokeUserMediaDeviceSandboxExtensions: revoked extension %s", extensionID.utf8().data());
        }
    }
    
    if (m_machBootstrapExtension)
        m_machBootstrapExtension->revoke();
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

void WebProcess::setAppBadge(std::optional<WebPageProxyIdentifier> pageIdentifier, const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    parentProcessConnection()->send(Messages::WebProcessProxy::SetAppBadge(pageIdentifier, origin, badge), 0);
}

void WebProcess::setClientBadge(WebPageProxyIdentifier pageIdentifier, const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    parentProcessConnection()->send(Messages::WebProcessProxy::SetClientBadge(pageIdentifier, origin, badge), 0);
}

#if HAVE(CVDISPLAYLINK)
void WebProcess::displayWasRefreshed(uint32_t displayID, const DisplayUpdate& displayUpdate)
{
    ASSERT(RunLoop::isMain());
    m_eventDispatcher.notifyScrollingTreesDisplayWasRefreshed(displayID);
    DisplayRefreshMonitorManager::sharedManager().displayWasUpdated(displayID, displayUpdate);
}
#endif

#if ENABLE(TRACKING_PREVENTION)
void WebProcess::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode, CompletionHandler<void()>&& completionHandler)
{
    m_thirdPartyCookieBlockingMode = thirdPartyCookieBlockingMode;
    completionHandler();
}

void WebProcess::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    ResourceLoadObserver::shared().setDomainsWithUserInteraction(WTFMove(domains));
}

void WebProcess::setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    for (auto& domain : domains.keys()) {
        for (auto& webPage : m_pageMap.values())
            webPage->addDomainWithPageLevelStorageAccess(domain, domains.get(domain));
    }
    ResourceLoadObserver::shared().setDomainsWithCrossPageStorageAccess(WTFMove(domains), WTFMove(completionHandler));
}

void WebProcess::sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&& completionHandler)
{
    ResourceLoadObserver::shared().updateCentralStatisticsStore(WTFMove(completionHandler));
}
#endif

#if ENABLE(GPU_PROCESS)
void WebProcess::setUseGPUProcessForCanvasRendering(bool useGPUProcessForCanvasRendering)
{
    m_useGPUProcessForCanvasRendering = useGPUProcessForCanvasRendering;
}

void WebProcess::setUseGPUProcessForDOMRendering(bool useGPUProcessForDOMRendering)
{
    m_useGPUProcessForDOMRendering = useGPUProcessForDOMRendering;
}

void WebProcess::setUseGPUProcessForMedia(bool useGPUProcessForMedia)
{
    if (useGPUProcessForMedia == m_useGPUProcessForMedia)
        return;

    m_useGPUProcessForMedia = useGPUProcessForMedia;

#if ENABLE(ENCRYPTED_MEDIA)
    auto& cdmFactories = CDMFactory::registeredFactories();
    cdmFactories.clear();
    if (useGPUProcessForMedia)
        cdmFactory().registerFactory(cdmFactories);
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
        legacyCDMFactory().registerFactory();
    else
        LegacyCDM::resetFactories();
#endif

    if (useGPUProcessForMedia)
        mediaEngineConfigurationFactory().registerFactory();
    else
        MediaEngineConfigurationFactory::resetFactories();

    if (useGPUProcessForMedia)
        WebCore::AudioHardwareListener::setCreationFunction([this] (WebCore::AudioHardwareListener::Client& client) { return RemoteAudioHardwareListener::create(client, *this); });
    else
        WebCore::AudioHardwareListener::resetCreationFunction();

    if (useGPUProcessForMedia)
        WebCore::RemoteCommandListener::setCreationFunction([this] (WebCore::RemoteCommandListenerClient& client) { return RemoteRemoteCommandListener::create(client, *this); });
    else
        WebCore::RemoteCommandListener::resetCreationFunction();

#if PLATFORM(COCOA)
    if (useGPUProcessForMedia) {
        SystemBatteryStatusTestingOverrides::singleton().setConfigurationChangedCallback([this] () {
            ensureGPUProcessConnection().updateMediaConfiguration();
        });
#if ENABLE(VP9)
        VP9TestingOverrides::singleton().setConfigurationChangedCallback([this] () {
            ensureGPUProcessConnection().updateMediaConfiguration();
        });
#endif
    } else {
        SystemBatteryStatusTestingOverrides::singleton().setConfigurationChangedCallback(nullptr);
#if ENABLE(VP9)
        VP9TestingOverrides::singleton().setConfigurationChangedCallback(nullptr);
#endif
    }
#endif
}

bool WebProcess::shouldUseRemoteRenderingFor(RenderingPurpose purpose)
{
    switch (purpose) {
    case RenderingPurpose::Canvas:
        return m_useGPUProcessForCanvasRendering;
    case RenderingPurpose::DOM:
    case RenderingPurpose::LayerBacking:
    case RenderingPurpose::Snapshot:
    case RenderingPurpose::ShareableSnapshot:
        return m_useGPUProcessForDOMRendering;
    case RenderingPurpose::MediaPainting:
        return m_useGPUProcessForMedia;
    default:
        break;
    }
    return false;
}

#if ENABLE(WEBGL)
void WebProcess::setUseGPUProcessForWebGL(bool useGPUProcessForWebGL)
{
    m_useGPUProcessForWebGL = useGPUProcessForWebGL;
}

bool WebProcess::shouldUseRemoteRenderingForWebGL() const
{
    return m_useGPUProcessForWebGL;
}
#endif // ENABLE(WEBGL)

#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MEDIA_STREAM)
SpeechRecognitionRealtimeMediaSourceManager& WebProcess::ensureSpeechRecognitionRealtimeMediaSourceManager()
{
    if (!m_speechRecognitionRealtimeMediaSourceManager)
        m_speechRecognitionRealtimeMediaSourceManager = makeUnique<SpeechRecognitionRealtimeMediaSourceManager>(*parentProcessConnection());

    return *m_speechRecognitionRealtimeMediaSourceManager;
}
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
RemoteLegacyCDMFactory& WebProcess::legacyCDMFactory()
{
    return *supplement<RemoteLegacyCDMFactory>();
}
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
RemoteCDMFactory& WebProcess::cdmFactory()
{
    return *supplement<RemoteCDMFactory>();
}
#endif

#if ENABLE(GPU_PROCESS)
RemoteMediaEngineConfigurationFactory& WebProcess::mediaEngineConfigurationFactory()
{
    return *supplement<RemoteMediaEngineConfigurationFactory>();
}
#endif
} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef WEBPROCESS_RELEASE_LOG
#undef WEBPROCESS_RELEASE_LOG_ERROR
