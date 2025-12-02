/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcessMessages.h"
#include "EventDispatcher.h"
#include "InjectedBundle.h"
#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "ModelProcessModelPlayerManager.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkSession.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "RemoteAudioHardwareListener.h"
#include "RemoteAudioSession.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteMediaEngineConfigurationFactory.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteRemoteCommandListener.h"
#include "RemoteSharedResourceCacheProxy.h"
#include "RemoteWebLockRegistry.h"
#include "RemoteWorkerType.h"
#include "SpeechRecognitionRealtimeMediaSourceManager.h"
#include "StorageAreaMap.h"
#include "UserData.h"
#include "WebAutomationSessionProxy.h"
#include "WebBadgeClient.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebChromeClient.h"
#include "WebCookieJar.h"
#include "WebFileSystemStorageConnection.h"
#include "WebFrame.h"
#include "WebFrameMessages.h"
#include "WebGamepadProvider.h"
#include "WebGeolocationManager.h"
#include "WebIDBConnectionToServer.h"
#include "WebLoaderStrategy.h"
#include "WebMediaKeyStorageManager.h"
#include "WebMemorySampler.h"
#include "WebMessagePortChannelProvider.h"
#include "WebNotificationManager.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPageInlines.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPaymentCoordinator.h"
#include "WebPermissionController.h"
#include "WebPlatformStrategies.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessDataStoreParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebResourceLoadObserver.h"
#include "WebSWClientConnection.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include "WebSharedWorkerContextManagerConnection.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerProvider.h"
#include "WebTransportSession.h"
#include "WebUserContentController.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteDataType.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <JavaScriptCore/WasmFaultSignalHandler.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BackForwardCache.h>
#include <WebCore/CPUMonitor.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/CommonVM.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/DNS.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/FontCache.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GarbageCollectionController.h>
#include <WebCore/GlyphPage.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LocalDOMWindow.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/MediaEngineConfigurationFactory.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/MessagePort.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NavigatorGamepad.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PermissionController.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMediaSession.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/Quirks.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RemoteCommandListener.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedWorkerContextManager.h>
#include <WebCore/SharedWorkerThreadProxy.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/WebKitJSHandle.h>
#include <algorithm>
#include <pal/Logging.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CoroutineUtilities.h>
#include <wtf/DateMath.h>
#include <wtf/Language.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URLParser.h>
#include <wtf/WTFProcess.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextStream.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#include "ARKitInlinePreviewModelPlayerMac.h"
#endif

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(COCOA)
#include "UserMediaCaptureManager.h"
#endif

#if USE(CG)
#include <WebCore/ImageDecoderCG.h>
#endif

#if HAVE(DISPLAY_LINK)
#include <WebCore/DisplayRefreshMonitorManager.h>
#endif

#if ENABLE(NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

#if ENABLE(GPU_PROCESS)
#include "GPUProcessConnection.h"
#endif

#if ENABLE(MODEL_PROCESS)
#include "ModelConnectionToWebProcessMessages.h"
#include "ModelProcessConnection.h"
#include "ModelProcessConnectionParameters.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
#include <JavaScriptCore/WasmDebugServer.h>
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
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

#if ENABLE(CONTENT_FILTERING)
#include "WebMockContentFilterManager.h"
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "WebCompiledContentRuleList.h"
#include <WebCore/ResourceMonitorChecker.h>
#endif

#if HAVE(LSDATABASECONTEXT)
#include "LaunchServicesDatabaseManager.h"
#endif

#if ENABLE(LOCKDOWN_MODE_API)
#import <pal/cocoa/LockdownModeCocoa.h>
#endif

#if PLATFORM(COCOA)
#import <pal/cocoa/EnhancedSecurityCocoa.h>
#endif

#if USE(LIBRICE)
#include "RiceBackendProxy.h"
#endif

#if PLATFORM(MAC)
#import <wtf/spi/darwin/SandboxSPI.h>
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include <WebCore/GraphicsContextGLTextureMapperGBM.h>
#endif

#undef WEBPROCESS_RELEASE_LOG
#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#if RELEASE_LOG_DISABLED
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) UNUSED_VARIABLE(this)
#define WEBPROCESS_RELEASE_LOG_FORWARDABLE(channel, fmt, ...) UNUSED_VARIABLE(this)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) UNUSED_VARIABLE(this)
#else
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define WEBPROCESS_RELEASE_LOG_FORWARDABLE(channel, fmt, ...) RELEASE_LOG_FORWARDABLE(channel, fmt, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#endif

// This should be greater than tileRevalidationTimeout in TileController.
static const Seconds nonVisibleProcessEarlyMemoryCleanupDelay { 10_s };

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
// This should be long enough to support a workload where a user is actively switching between multiple tabs,
// since our memory cleanup routine could potentially delete a good amount of JIT code.
static const Seconds nonVisibleProcessMemoryCleanupDelay { 120_s };
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebProcess);

#if !PLATFORM(GTK) && !PLATFORM(WPE)
[[noreturn]] static void callExit(IPC::Connection*)
{
    terminateProcess(EXIT_SUCCESS);
}
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
static void crashAfter10Seconds(IPC::Connection*)
{
    // If the connection has been closed and we haven't responded in the main thread for 10 seconds the process will exit forcibly.
    static const auto watchdogDelay = 10_s;
    WorkQueue::create("WebKit.WebProcess.WatchDogQueue"_s)->dispatchAfter(watchdogDelay, [] {
        // We use g_error() here to cause a crash and allow debugging this unexpected late exit.
        g_error("WebProcess didn't exit as expected after the UI process connection was closed");
    });
}
#endif

WebProcess& WebProcess::singleton()
{
    static NeverDestroyed<Ref<WebProcess>> process = adoptRef(*new WebProcess);
    return process.get().get();
}

WebProcess::WebProcess()
    : m_eventDispatcher(*this)
#if PLATFORM(IOS_FAMILY)
    , m_viewUpdateDispatcher(*this)
#endif
    , m_webInspectorInterruptDispatcher(*this)
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    , m_wasmDebuggerDispatcher(*this)
#endif
    , m_webLoaderStrategy(makeUniqueRefWithoutRefCountedCheck<WebLoaderStrategy>(*this))
#if PLATFORM(COCOA) && USE(LIBWEBRTC) && ENABLE(WEB_CODECS)
    , m_remoteVideoCodecFactory(*this)
#endif
#if ENABLE(MODEL_PROCESS)
    , m_modelProcessModelPlayerManager(ModelProcessModelPlayerManager::create())
#endif
    , m_cacheStorageProvider(WebCacheStorageProvider::create())
    , m_badgeClient(WebBadgeClient::create())
#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
    , m_remoteMediaPlayerManager(RemoteMediaPlayerManager::create())
#endif
#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
    , m_remoteImageDecoderAVFManager(RemoteImageDecoderAVFManager::create())
#endif
    , m_broadcastChannelRegistry(WebBroadcastChannelRegistry::create())
    , m_cookieJar(WebCookieJar::create())
    , m_dnsPrefetchHystereris([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) m_dnsPrefetchedHosts.clear(); })
#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    , m_nonVisibleProcessMemoryCleanupTimer(*this, &WebProcess::nonVisibleProcessMemoryCleanupTimerFired)
#endif
{
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();

    // FIXME: This should moved to where WebProcess::initialize is called,
    // so that ports have a chance to customize, and ifdefs in this file are
    // limited.
    addSupplementWithoutRefCountedCheck<WebGeolocationManager>();

#if ENABLE(NOTIFICATIONS)
    addSupplementWithoutRefCountedCheck<WebNotificationManager>();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    addSupplement<WebMediaKeyStorageManager>();
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    addSupplementWithoutRefCountedCheck<UserMediaCaptureManager>();
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
    addSupplementWithoutRefCountedCheck<RemoteCDMFactory>();
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    addSupplementWithoutRefCountedCheck<RemoteLegacyCDMFactory>();
#endif

#if ENABLE(GPU_PROCESS)
    addSupplementWithoutRefCountedCheck<RemoteMediaEngineConfigurationFactory>();
#endif

    Gigacage::forbidDisablingPrimitiveGigacage();

#if ENABLE(CONTENT_FILTERING)
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
    m_isLockdownModeEnabled = parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("enable-lockdown-mode"_s) == "1"_s;
    m_isEnhancedSecurityEnabled = parameters.extraInitializationData.get<HashTranslatorASCIILiteral>("enable-enhanced-security"_s) == "1"_s;

    WTF::setProcessPrivileges({ });

    {
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::Options::allowNonSPTagging() = false;
        JSC::Options::notifyOptionsChanged();
    }

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
    IPC::Connection::DidCloseOnConnectionWorkQueueCallback callExitCallback = crashAfter10Seconds;
#else
    // We call _exit() directly from the background queue in case the main thread is unresponsive
    // and AuxiliaryProcess::didClose() does not get called.
    IPC::Connection::DidCloseOnConnectionWorkQueueCallback callExitCallback = callExit;
#endif
    connection->setDidCloseOnConnectionWorkQueueCallback(callExitCallback);

#if !PLATFORM(GTK) && !PLATFORM(WPE) && !ENABLE(IPC_TESTING_API)
    connection->setShouldExitOnSyncMessageSendFailure(true);
#endif

    protectedEventDispatcher()->initializeConnection(*connection);
#if PLATFORM(IOS_FAMILY)
    m_viewUpdateDispatcher.initializeConnection(*connection);
#endif // PLATFORM(IOS_FAMILY)

    protectedWebInspectorInterruptDispatcher()->initializeConnection(*connection);
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    protectedWasmDebuggerDispatcher()->initializeConnection(*connection);
#endif

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);
}

static void scheduleLogMemoryStatistics(LogMemoryStatisticsReason reason)
{
    // Log stats in the next turn of the run loop so that it runs after the low memory handler.
    RunLoop::mainSingleton().dispatch([reason] {
        WebCore::logMemoryStatistics(reason);
    });
}

void WebProcess::initializeWebProcess(WebProcessCreationParameters&& parameters, CompletionHandler<void(ProcessIdentity)>&& completionHandler)
{
    TraceScope traceScope(InitializeWebProcessStart, InitializeWebProcessEnd);
    // Reply immediately so that the identity is available as soon as possible.
    completionHandler(ProcessIdentity { ProcessIdentity::CurrentProcess });

    ASSERT(m_pageMap.isEmpty());

    if (parameters.websiteDataStoreParameters)
        setWebsiteDataStoreParameters(WTFMove(*parameters.websiteDataStoreParameters));

    setLegacyPresentingApplicationPID(parameters.presentingApplicationPID);

#if OS(LINUX)
    MemoryPressureHandler::ReliefLogger::setLoggingEnabled(parameters.shouldEnableMemoryPressureReliefLogging);
#endif

    platformInitializeWebProcess(parameters);

    // Match the QoS of the UIProcess and the scrolling thread but use a slightly lower priority.
    WTF::Thread::setCurrentThreadIsUserInteractive(-1);

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([this, protectedThis = Ref { *this }] (Critical critical, Synchronous synchronous) {
            // If this process contains only non-visible content (e.g. only contains background
            // tabs), then treat the memory warning as if it was a critical warning to maximize the
            // amount of memory released for foreground apps to use.
            if (m_pagesInWindows.isEmpty() && critical == Critical::No)
                critical = Critical::Yes;

            if (Options::dumpHeapOnLowMemory()) [[unlikely]]
                GarbageCollectionController::singleton().dumpHeap();

#if PLATFORM(COCOA)
            // If this is a process we keep around for performance, kill it on memory pressure instead of trying to free up its memory.
            if (m_allowExitOnMemoryPressure && isProcessBeingCachedForPerformance()) {
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

            auto maintainBackForwardCache = m_allowExitOnMemoryPressure ? WebCore::MaintainBackForwardCache::No : WebCore::MaintainBackForwardCache::Yes;
            auto maintainMemoryCache = (m_allowExitOnMemoryPressure || !m_hasSuspendedPageProxy) ? WebCore::MaintainMemoryCache::No : WebCore::MaintainMemoryCache::Yes;
            WebCore::releaseMemory(critical, synchronous, maintainBackForwardCache, maintainMemoryCache);
            for (auto& page : m_pageMap.values())
                page->releaseMemory(critical);
        });
#if ENABLE(PERIODIC_MEMORY_MONITOR)
        if (auto pollInterval = parameters.memoryFootprintPollIntervalForTesting)
            memoryPressureHandler.setMemoryFootprintPollIntervalForTesting(pollInterval);
#if !USE(SYSTEM_MALLOC)
        // If we're running with FastMalloc disabled, some kind of testing or debugging is probably happening.
        // Let's be nice and not enable the memory kill mechanism.
        memoryPressureHandler.setShouldUsePeriodicMemoryMonitor(isFastMallocEnabled() || JSC::Options::enableStrongRefTracker() || JSC::Options::dumpHeapOnLowMemory());
#endif
        memoryPressureHandler.setMemoryKillCallback([this, protectedThis = Ref { *this }] () {
            WebCore::logMemoryStatistics(LogMemoryStatisticsReason::OutOfMemoryDeath);
            RefPtr parentProcessConnection = this->parentProcessConnection();
            if (MemoryPressureHandler::singleton().processState() == WebsamProcessState::Active)
                parentProcessConnection->send(Messages::WebProcessProxy::DidExceedActiveMemoryLimit(), 0);
            else
                parentProcessConnection->send(Messages::WebProcessProxy::DidExceedInactiveMemoryLimit(), 0);
        });
        memoryPressureHandler.setMemoryFootprintNotificationThresholds(WTFMove(parameters.memoryFootprintNotificationThresholds), [this, protectedThis = Ref { *this }](size_t footprint) {
            protectedParentProcessConnection()->send(Messages::WebProcessProxy::DidExceedMemoryFootprintThreshold(footprint), 0);
        });
#endif
        memoryPressureHandler.setMemoryPressureStatusChangedCallback([this, protectedThis = Ref { *this }]() {
            if (RefPtr parentProcessConnection = this->parentProcessConnection())
                parentProcessConnection->send(Messages::WebProcessProxy::MemoryPressureStatusChanged(MemoryPressureHandler::singleton().memoryPressureStatus()), 0);
        });
        memoryPressureHandler.setDidExceedProcessMemoryLimitCallback([this, protectedThis = Ref { *this }](WTF::ProcessMemoryLimit limit) {
            if (limit == WTF::ProcessMemoryLimit::Warning && !m_loggedProcessLimitWarningMemoryStatistics) {
                m_loggedProcessLimitWarningMemoryStatistics = true;
                scheduleLogMemoryStatistics(LogMemoryStatisticsReason::WarningMemoryPressureNotification);
            } else if (limit == WTF::ProcessMemoryLimit::Critical && !m_loggedProcessLimitCriticalMemoryStatistics) {
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
    PAL::registerNotifyCallback("com.apple.WebKit.logPageState"_s, [this, protectedThis = Ref { *this }] {
        for (auto& page : m_pageMap.values()) {
            int64_t loadCommitTime = 0;
#if USE(OS_STATE)
            loadCommitTime = static_cast<int64_t>(page->loadCommitTime().secondsSinceEpoch().seconds());
#endif

            WTF::TextStream activityStateStream(WTF::TextStream::LineMode::SingleLine);
            activityStateStream << page->activityState();

            RELEASE_LOG(ActivityState, "WebPage %p - load_time: %" PRId64 ", visible: %d, throttleable: %d , suspended: %d , websam_state: %" PUBLIC_LOG_STRING ", activity_state: %" PUBLIC_LOG_STRING ", url: %" PRIVATE_LOG_STRING, page.ptr(), loadCommitTime, page->isVisible(), page->isThrottleable(), page->isSuspended(), MemoryPressureHandler::processStateDescription().characters(), activityStateStream.release().utf8().data(), page->mainWebFrame().url().string().utf8().data());
        }
    });
#endif

    SandboxExtension::consumePermanently(parameters.additionalSandboxExtensionHandles);

#if PLATFORM(MAC) && HAVE(SANDBOX_STATE_FLAGS)
    if (!parameters.injectedBundlePath.endsWith("StoreWebBundle.bundle"_s)) {
        auto auditToken = auditTokenForSelf();
        if (!sandbox_enable_state_flag("WebProcessDidNotInjectStoreBundle", auditToken.value()))
            WEBPROCESS_RELEASE_LOG_ERROR(Process, "Could not state sandbox state flag");
    }
#endif

    if (!parameters.injectedBundlePath.isEmpty()) {
        if (RefPtr injectedBundle = InjectedBundle::create(parameters, transformHandlesToObjects(parameters.initializationUserData.protectedObject().get())))
            lazyInitialize(m_injectedBundle, injectedBundle.releaseNonNull());
    }

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);
#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
    protectedRemoteMediaPlayerManager()->initialize(parameters);
#endif

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

#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
    for (auto& scheme : parameters.urlSchemesRegisteredAsNoAccess)
        registerURLSchemeAsNoAccess(scheme);
#endif

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

#if ENABLE(WK_WEB_EXTENSIONS)
    for (auto& scheme : parameters.urlSchemesRegisteredAsWebExtensions)
        WebExtensionMatchPattern::registerCustomURLScheme(scheme);
#endif

    if (parameters.defaultRequestTimeoutInterval)
        setDefaultRequestTimeoutInterval(*parameters.defaultRequestTimeoutInterval);

    setBackForwardCacheCapacity(parameters.backForwardCacheCapacity);

    setAlwaysUsesComplexTextCodePath(parameters.shouldAlwaysUseComplexTextCodePath);

    setDisableFontSubpixelAntialiasingForTesting(parameters.disableFontSubpixelAntialiasingForTesting);

    setMemoryCacheDisabled(parameters.memoryCacheDisabled);

    WebCore::DeprecatedGlobalSettings::setAttrStyleEnabled(parameters.attrStyleEnabled);
    
    commonVM().setGlobalConstRedeclarationShouldThrow(parameters.shouldThrowExceptionForGlobalConstantRedeclaration);

    ScriptExecutionContext::setCrossOriginMode(parameters.crossOriginMode);
    DeprecatedGlobalSettings::setArePDFImagesEnabled(!isLockdownModeEnabled());

#if ENABLE(LOCKDOWN_MODE_API)
    PAL::setLockdownModeEnabledForCurrentProcess(isLockdownModeEnabled());
#endif

#if PLATFORM(COCOA)
    PAL::setEnhancedSecurityEnabledForCurrentProcess(enhancedSecurityEnabled());
#endif

#if ENABLE(SERVICE_CONTROLS)
    setEnabledServices(parameters.hasImageServices, parameters.hasSelectionServices, parameters.hasRichContentServices);
#endif

#if ENABLE(REMOTE_INSPECTOR) && PLATFORM(COCOA) && !ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
    if (std::optional<audit_token_t> auditToken = protectedParentProcessConnection()->getAuditToken()) {
        RetainPtr<CFDataRef> auditData = adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
        Inspector::RemoteInspector::singleton().setParentProcessInformation(legacyPresentingApplicationPID(), auditData);
    }
    // We need to connect to webinspectord before the first page load for the XPC connection to be successfully opened.
    // This is because we block launchd before the first page load, and launchd is required to establish the connection.
    // This is only done if Web Inspector is enabled, which is determined by the size of the Web Inspector extension vector.
    // See WebProcessProxy::shouldEnableRemoteInspector().
    if (parameters.enableRemoteWebInspectorExtensionHandles.size())
        Inspector::RemoteInspector::singleton().connectToWebInspector();
#endif

#if ENABLE(GAMEPAD)
    GamepadProvider::singleton().setSharedProvider(WebGamepadProvider::singleton());
#endif

    ServiceWorkerProvider::setSharedProvider(WebServiceWorkerProvider::singleton());
    SharedWorkerProvider::setSharedProvider(WebSharedWorkerProvider::singleton());

#if !RELEASE_LOG_DISABLED
    WebResourceLoadObserver::setShouldLogUserInteraction(parameters.shouldLogUserInteraction);
#endif

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent)
        prewarmGlobally();
#endif

    updateStorageAccessUserAgentStringQuirks(WTFMove(parameters.storageAccessUserAgentStringQuirksData));
    updateDomainsWithStorageAccessQuirks(WTFMove(parameters.storageAccessPromptQuirksDomains));
    updateScriptTrackingPrivacyFilter(WTFMove(parameters.scriptTrackingPrivacyRules));

#if ENABLE(GAMEPAD)
    // Web processes need to periodically notify the UI process of gamepad access at least as frequently
    // as the WebPageProxy::gamepadsRecentlyAccessedThreshold value.
    // 3-times-as-often seems like it will guarantee proper behavior for almost all web pages.
    WebCore::NavigatorGamepad::setGamepadsRecentlyAccessedThreshold(WebPageProxy::gamepadsRecentlyAccessedThreshold / 3);
#endif

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    m_shouldInitializeAccessibility = parameters.shouldInitializeAccessibility;
#endif

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
    if (JSC::Options::enableWasmDebugger()) [[unlikely]] {
        if (JSC::VM* vm = WebCore::commonVMOrNull()) {
            bool success = JSC::Wasm::DebugServer::singleton().startRWI(vm, [](const String& response) {
                return WebKit::WebProcess::singleton().send(Messages::WebProcessProxy::SendWasmDebuggerResponse(response), 0);
            });
            if (!success)
                WEBPROCESS_RELEASE_LOG_ERROR(Inspector, "Failed to start WasmDebugServer in RWI mode");
        }
    }
#endif

    WEBPROCESS_RELEASE_LOG(Process, "initializeWebProcess: Presenting processPID=%d", legacyPresentingApplicationPID());
}

void WebProcess::setWebsiteDataStoreParameters(WebProcessDataStoreParameters&& parameters)
{
    ASSERT(!m_sessionID);
    m_sessionID = parameters.sessionID;

#if ENABLE(VIDEO)
    if (!parameters.mediaCacheDirectory.isEmpty())
        WebCore::HTMLMediaElement::setMediaCacheDirectory(parameters.mediaCacheDirectory);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    if (!parameters.modelElementCacheDirectory.isEmpty())
        ARKitInlinePreviewModelPlayerMac::setModelElementCacheDirectory(parameters.modelElementCacheDirectory);
#endif

    setTrackingPreventionEnabled(parameters.trackingPreventionEnabled);

    m_thirdPartyCookieBlockingMode = parameters.thirdPartyCookieBlockingMode;
    if (parameters.trackingPreventionEnabled) {
        if (!ResourceLoadObserver::singletonIfExists())
            ResourceLoadObserver::setShared(*new WebResourceLoadObserver(parameters.sessionID.isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
        ResourceLoadObserver::singleton().setDomainsWithUserInteraction(WTFMove(parameters.domainsWithUserInteraction));
        if (!parameters.sessionID.isEphemeral())
            ResourceLoadObserver::singleton().setDomainsWithCrossPageStorageAccess(WTFMove(parameters.domainsWithStorageAccessQuirk), [] { });
    }

    m_mediaKeysStorageDirectory = parameters.mediaKeyStorageDirectory;
    m_mediaKeysStorageSalt = parameters.mediaKeysStorageSalt;
    for (auto& supplement : m_supplements.values())
        supplement->setWebsiteDataStore(parameters);

    platformSetWebsiteDataStoreParameters(WTFMove(parameters));
    
    ensureNetworkProcessConnection();

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    setOptInCookiePartitioningEnabled(parameters.isOptInCookiePartitioningEnabled);
#endif
}

bool WebProcess::areAllPagesSuspended() const
{
    for (auto& page : m_pageMap.values()) {
        if (!page->isSuspended())
            return false;
    }
    return true;
}

void WebProcess::updateIsWebTransportEnabled()
{
    for (auto& page : m_pageMap.values()) {
        if (page->isWebTransportEnabled()) {
            m_isWebTransportEnabled = true;
            return;
        }
    }
    m_isWebTransportEnabled = false;
}

void WebProcess::updateIsBroadcastChannelEnabled()
{
    if (m_isBroadcastChannelEnabled)
        return;

    for (auto& page : m_pageMap.values()) {
        if (page->corePage()->settings().broadcastChannelEnabled()) {
            m_isBroadcastChannelEnabled = true;
            return;
        }
    }
}

#if USE(AUDIO_SESSION)
void WebProcess::remoteAudioSessionConfigurationChanged(const RemoteAudioSessionConfiguration& configuration)
{
    for (auto& page : m_pageMap.values()) {
        RefPtr manager = page->mediaSessionManagerIfExists();
        if (!manager)
            continue;

        send(Messages::RemoteMediaSessionManagerProxy::RemoteAudioConfigurationChanged(configuration), page->identifier().toUInt64());
    }
}
#endif

void WebProcess::setHasSuspendedPageProxy(bool hasSuspendedPageProxy)
{
    ASSERT(m_hasSuspendedPageProxy != hasSuspendedPageProxy);
    m_hasSuspendedPageProxy = hasSuspendedPageProxy;
}

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
Ref<RemoteImageDecoderAVFManager> WebProcess::protectedRemoteImageDecoderAVFManager()
{
    return m_remoteImageDecoderAVFManager;
}
#endif

void WebProcess::setIsInProcessCache(bool isInProcessCache, CompletionHandler<void()>&& completionHandler)
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
    accessibilityRelayProcessSuspended(isInProcessCache);
#else
    UNUSED_PARAM(isInProcessCache);
#endif
    completionHandler();
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

#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
void WebProcess::registerURLSchemeAsNoAccess(const String& urlScheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsNoAccess(urlScheme);
}
#endif

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

#if ENABLE(WK_WEB_EXTENSIONS)
void WebProcess::registerURLSchemeAsWebExtension(const String& urlScheme) const
{
    WebExtensionMatchPattern::registerCustomURLScheme(urlScheme);
}
#endif

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

    Ref memoryCache = MemoryCache::singleton();
    memoryCache->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    BackForwardCache::singleton().setMaxSize(backForwardCacheSize);

    platformSetCacheModel(cacheModel);
}

WebPage* WebProcess::focusedWebPage() const
{    
    for (auto& page : m_pageMap.values()) {
        if (page->windowAndWebPageAreFocused())
            return page.ptr();
    }
    return 0;
}

void WebProcess::updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&& userAgentStringQuirk)
{
    Quirks::updateStorageAccessUserAgentStringQuirks(WTFMove(userAgentStringQuirk));
}
    
WebPage* WebProcess::webPage(PageIdentifier pageID) const
{
    return m_pageMap.get(pageID);
}

void WebProcess::createWebPage(PageIdentifier pageID, WebPageCreationParameters&& parameters)
{
    m_hasEverHadAnyWebPages = true;

    auto addResult = m_pageMap.ensure(pageID, [&] {
        return WebPage::create(pageID, WTFMove(parameters));
    });
    Ref page { addResult.iterator->value };

    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way.
    if (addResult.isNewEntry) {
#if ENABLE(GPU_PROCESS)
        if (RefPtr gpuProcessConnection = m_gpuProcessConnection)
            page->gpuProcessConnectionDidBecomeAvailable(*gpuProcessConnection);
#endif

        // Balanced by an enableTermination in removeWebPage.
        disableTermination();
        updateCPULimit();
        updateIsWebTransportEnabled();
        updateIsBroadcastChannelEnabled();

#if OS(LINUX)
        RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
    } else
        page->reinitializeWebPage(WTFMove(parameters));

    if (m_hasPendingAccessibilityUnsuspension) {
        m_hasPendingAccessibilityUnsuspension = false;
        accessibilityRelayProcessSuspended(false);
    }
}

Awaitable<unsigned> WebProcess::countWebPagesForTesting()
{
    co_return m_pageMap.size();
}

void WebProcess::removeWebPage(PageIdentifier pageID)
{
    ASSERT(m_pageMap.contains(pageID));

    flushResourceLoadStatistics();

    pageWillLeaveWindow(pageID);
    m_pageMap.remove(pageID);

    enableTermination();
    updateCPULimit();
    updateIsWebTransportEnabled();
    updateIsBroadcastChannelEnabled();

#if OS(LINUX)
    RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
}

bool WebProcess::shouldTerminate()
{
    ASSERT(m_pageMap.isEmpty());

    // FIXME: the ShouldTerminate message should also send termination parameters, such as any session cookies that need to be preserved.
    auto sendResult = protectedParentProcessConnection()->sendSync(Messages::WebProcessProxy::ShouldTerminate(), 0);
    auto [shouldTerminate] = sendResult.takeReplyOr(true);
    return shouldTerminate;
}

void WebProcess::terminate()
{
#ifndef NDEBUG
    // These are done in an attempt to reduce LEAK output.
    GarbageCollectionController::singleton().garbageCollectNow();
    FontCache::invalidateAllFontCaches();
    MemoryCache::singleton().setDisabled(true);
#endif

    platformTerminate();

    AuxiliaryProcess::terminate();
}

bool WebProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebFrame::messageReceiverName()) {
        if (RefPtr frame = FrameIdentifier::isValidIdentifier(decoder.destinationID()) ? webFrame(FrameIdentifier(decoder.destinationID())) : nullptr)
            frame->didReceiveMessage(connection, decoder);
        else
            WebFrame::sendCancelReply(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (RefPtr contextManagerConnection = SWContextManager::singleton().connection())
            downcast<WebSWContextManagerConnection>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::WebSharedWorkerContextManagerConnection::messageReceiverName()) {
        ASSERT(SharedWorkerContextManager::singleton().connection());
        if (RefPtr contextManagerConnection = SharedWorkerContextManager::singleton().connection())
            downcast<WebSharedWorkerContextManagerConnection>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return true;
    }
    return false;
}

bool WebProcess::filterUnhandledMessage(IPC::Connection&, IPC::Decoder& decoder)
{
    // Note: due to receiving messages to non-existing IDs, we have to filter the messages.
    // This should be removed once these messages are fixed.
    LOG_ERROR("Unhandled web process message '%s' (destination: %" PRIu64 " pid: %d)", description(decoder.messageName()).characters(), decoder.destinationID(), static_cast<int>(getCurrentProcessID()));
    return true;
}

void WebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(VIDEO)
    FileSystem::markPurgeable(WebCore::HTMLMediaElement::mediaCacheDirectory());
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    FileSystem::markPurgeable(ARKitInlinePreviewModelPlayerMac::modelElementCacheDirectory());
#endif
    AuxiliaryProcess::didClose(connection);
}

WebFrame* WebProcess::webFrame(std::optional<FrameIdentifier> frameID) const
{
    return frameID ? m_frameMap.get(*frameID) : nullptr;
}

void WebProcess::addWebFrame(FrameIdentifier frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebProcess::removeWebFrame(FrameIdentifier frameID, WebPage* page)
{
    RefPtr frame = m_frameMap.take(frameID).get();
    if (!frame)
        return;
    if (frame->coreLocalFrame() && m_networkProcessConnection)
        m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::ClearFrameLoadRecordsForStorageAccess(frameID), 0);

    // We can end up here after our connection has closed when WebCore's frame life-support timer
    // fires when the application is shutting down. There's no need (and no way) to update the UI
    // process in this case.
    if (!parentProcessConnection())
        return;

    if (!page)
        return;

    if (frame->wasRemovedInAnotherProcess() || page->isClosed())
        return;

    page->send(Messages::WebPageProxy::DidDestroyFrame(frameID));
}

WebPageGroupProxy* WebProcess::webPageGroup(WebPageGroupData&& pageGroupData)
{
    auto result = m_pageGroupMap.add(pageGroupData.pageGroupID, nullptr);
    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = WebPageGroupProxy::create(WTFMove(pageGroupData));
    }

    return result.iterator->value.get();
}

std::optional<WebCore::UserGestureTokenIdentifier> WebProcess::userGestureTokenIdentifier(std::optional<PageIdentifier> pageID, RefPtr<UserGestureToken> token)
{
    if (!pageID)
        return std::nullopt;

    if (!token || !token->processingUserGesture())
        return std::nullopt;

    auto result = m_userGestureTokens.ensure(*token, [] {
        return UserGestureTokenIdentifier::generate();
    });
    if (result.isNewEntry) {
        Ref { result.iterator->key }->addDestructionObserver([pageID] (UserGestureToken& tokenBeingDestroyed) {
            WebProcess::singleton().userGestureTokenDestroyed(*pageID, tokenBeingDestroyed);
        });
    }
    
    return result.iterator->value;
}

void WebProcess::userGestureTokenDestroyed(PageIdentifier pageID, UserGestureToken& token)
{
    auto identifier = m_userGestureTokens.take(token);
    protectedParentProcessConnection()->send(Messages::WebProcessProxy::DidDestroyUserGestureToken(pageID, identifier), 0);
}

void WebProcess::isJITEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(JSC::Options::useJIT());
}

void WebProcess::isEnhancedSecurityEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_isEnhancedSecurityEnabled.value_or(false));
}

void WebProcess::garbageCollectJavaScriptObjects()
{
    GarbageCollectionController::singleton().garbageCollectNow();
}

void WebProcess::backgroundResponsivenessPing()
{
    protectedParentProcessConnection()->send(Messages::WebProcessProxy::DidReceiveBackgroundResponsivenessPing(), 0);
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

void WebProcess::setInitialGamepads(const Vector<std::optional<GamepadData>>& gamepadDatas)
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
    GarbageCollectionController::singleton().setJavaScriptGarbageCollectorTimerEnabled(flag);
}

void WebProcess::handleInjectedBundleMessage(const String& messageName, const UserData& messageBody)
{
    RefPtr injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->didReceiveMessage(messageName, transformHandlesToObjects(messageBody.protectedObject().get()));
}

void WebProcess::setInjectedBundleParameter(const String& key, std::span<const uint8_t> value)
{
    RefPtr injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->setBundleParameter(key, value);
}

void WebProcess::setInjectedBundleParameters(std::span<const uint8_t> value)
{
    RefPtr injectedBundle = WebProcess::singleton().injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->setBundleParameters(value);
}

[[noreturn]] inline void failedToGetNetworkProcessConnection()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    // GTK and WPE ports don't exit on send sync message failure.
    // In this particular case, the network process can be terminated by the UI process while the
    // Web process is still initializing, so we always want to exit instead of crashing. This can
    // happen when the WebView is created and then destroyed quickly.
    // See https://bugs.webkit.org/show_bug.cgi?id=183348.
    exitProcess(0);
#else
    CRASH();
#endif
}

static NetworkProcessConnectionInfo getNetworkProcessConnection(IPC::Connection& connection)
{
    NetworkProcessConnectionInfo connectionInfo;
    auto requestConnection = [&]() -> bool {
        RELEASE_LOG(Process, "getNetworkProcessConnection: Request connection for core identifier %" PRIu64, WebCore::Process::identifier().toUInt64());
        auto sendResult = connection.sendSync(Messages::WebProcessProxy::GetNetworkProcessConnection(), 0, IPC::Timeout::infinity(), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages);
        if (!sendResult.succeeded()) {
            RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to send message or receive invalid message: error %" PUBLIC_LOG_STRING, IPC::errorAsString(sendResult.error()).characters());
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
        auto connectionInfo = getNetworkProcessConnection(Ref { *parentProcessConnection() });

        m_networkProcessConnection = NetworkProcessConnection::create(IPC::Connection::Identifier { WTFMove(connectionInfo.connection) }, connectionInfo.cookieAcceptPolicy);
#if HAVE(AUDIT_TOKEN)
        m_networkProcessConnection->setNetworkProcessAuditToken(connectionInfo.auditToken ? std::optional(connectionInfo.auditToken->auditToken()) : std::nullopt);
#endif
        m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::RegisterURLSchemesAsCORSEnabled(WebCore::LegacySchemeRegistry::allURLSchemesRegisteredAsCORSEnabled()), 0);

        if (!Document::allDocuments().isEmpty() || SharedWorkerThreadProxy::hasInstances())
            protectedNetworkProcessConnection()->protectedServiceWorkerConnection()->registerServiceWorkerClients();

#if HAVE(LSDATABASECONTEXT)
        // On Mac, this needs to be called before NSApplication is being initialized.
        // The NSApplication initialization is being done in [NSApplication _accessibilityInitialize]
        LaunchServicesDatabaseManager::singleton().waitForDatabaseUpdate();
#endif
#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
        if (auto auditToken = auditTokenForSelf()) {
            m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::CheckInWebProcess(*auditToken), 0);
            if (!m_pendingDisplayName.isNull())
                m_networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::UpdateActivePages(std::exchange(m_pendingDisplayName, String()), { }, *auditToken), 0);
        }
#endif
        // This can be called during a WebPage's constructor, so wait until after the constructor returns to touch the WebPage.
        RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
            for (auto& webPage : m_pageMap.values())
                webPage->synchronizeCORSDisablingPatternsWithNetworkProcess();
        });
    }
    
    return *m_networkProcessConnection;
}

Ref<NetworkProcessConnection> WebProcess::ensureProtectedNetworkProcessConnection()
{
    return ensureNetworkProcessConnection();
}

RefPtr<NetworkProcessConnection> WebProcess::protectedNetworkProcessConnection()
{
    return existingNetworkProcessConnection();
}

void WebProcess::logDiagnosticMessageForNetworkProcessCrash()
{
    RefPtr<WebCore::Page> page;

    if (RefPtr webPage = focusedWebPage())
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
        page->checkedDiagnosticLoggingClient()->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::networkProcessCrashedKey(), WebCore::ShouldSample::No);
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
        if (RefPtr map = m_storageAreaMaps.get(key))
            map->disconnect();
    }

    for (auto& page : m_pageMap.values()) {
        RefPtr corePage = page->corePage();
        RefPtr idbConnection = corePage->optionalIDBConnection();
        if (!idbConnection)
            continue;
        
        if (RefPtr existingIDBConnectionToServer = connection->existingIDBConnectionToServer()) {
            ASSERT_UNUSED(existingIDBConnectionToServer, idbConnection.get() == &existingIDBConnectionToServer->coreConnectionToServer());
            corePage->clearIDBConnection();
        }
    }

    if (SWContextManager::singleton().connection())
        SWContextManager::singleton().stopAllServiceWorkers();

    m_networkProcessConnection = nullptr;

    logDiagnosticMessageForNetworkProcessCrash();

    m_webLoaderStrategy->networkProcessCrashed();
    m_webSocketChannelManager.networkProcessCrashed();
    m_broadcastChannelRegistry->networkProcessCrashed();

#if USE(LIBWEBRTC)
    if (m_libWebRTCNetwork)
        m_libWebRTCNetwork->networkProcessCrashed();
#endif

    for (auto& page : m_pageMap.values()) {
        page->stopAllURLSchemeTasks();
#if ENABLE(APPLE_PAY)
        if (RefPtr paymentCoordinator = page->paymentCoordinator())
            paymentCoordinator->networkProcessConnectionClosed();
#endif
    }

    // Recreate a new connection with valid IPC connection on next operation.
    if (RefPtr fileSystemStorageConnection = m_fileSystemStorageConnection) {
        fileSystemStorageConnection->connectionClosed();
        m_fileSystemStorageConnection = nullptr;
    }

    m_cacheStorageProvider->networkProcessConnectionClosed();

    Vector<ThreadSafeWeakPtr<WebTransportSession>> sessions;
    {
        Locker locker { m_webTransportSessionsLock };
        sessions = copyToVector(m_webTransportSessions.values());
    }
    for (auto& weakSession : sessions) {
        if (RefPtr webtransportSession = weakSession.get())
            webtransportSession->didFail(std::nullopt, String(emptyString()));
    }
}

WebFileSystemStorageConnection& WebProcess::fileSystemStorageConnection()
{
    if (!m_fileSystemStorageConnection)
        m_fileSystemStorageConnection = WebFileSystemStorageConnection::create(Ref { ensureNetworkProcessConnection().connection() });

    return *m_fileSystemStorageConnection;
}

Ref<WebFileSystemStorageConnection> WebProcess::protectedFileSystemStorageConnection()
{
    return fileSystemStorageConnection();
}

WebLoaderStrategy& WebProcess::webLoaderStrategy()
{
    return m_webLoaderStrategy;
}

Ref<WebLoaderStrategy> WebProcess::protectedWebLoaderStrategy()
{
    return m_webLoaderStrategy.get();
}

#if ENABLE(GPU_PROCESS)

GPUProcessConnection& WebProcess::ensureGPUProcessConnection()
{
    RELEASE_ASSERT(RunLoop::isMain());

    // If we've lost our connection to the GPU process (e.g. it crashed) try to re-establish it.
    if (!m_gpuProcessConnection) {
        auto connectionIdentifiers = IPC::Connection::createConnectionIdentifierPair();
        if (!connectionIdentifiers)
            CRASH();

        Ref gpuConnection = IPC::Connection::createServerConnection(WTFMove(connectionIdentifiers->server));
#if ENABLE(IPC_TESTING_API)
        if (gpuConnection->ignoreInvalidMessageForTesting())
            gpuConnection->setIgnoreInvalidMessageForTesting();
#endif
        Ref gpuProcessConnection = GPUProcessConnection::create(WTFMove(gpuConnection));
        m_gpuProcessConnection = gpuProcessConnection.ptr();
        protectedParentProcessConnection()->send(Messages::WebProcessProxy::CreateGPUProcessConnection(m_gpuProcessConnection->identifier(),  WTFMove(connectionIdentifiers->client)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        for (auto& page : m_pageMap.values())
            page->gpuProcessConnectionDidBecomeAvailable(gpuProcessConnection);
        if (m_sharedResourceCache)
            Ref { *m_sharedResourceCache }->gpuProcessConnectionDidBecomeAvailable(gpuProcessConnection);
    }
    return *m_gpuProcessConnection;
}

Ref<GPUProcessConnection> WebProcess::ensureProtectedGPUProcessConnection()
{
    return ensureGPUProcessConnection();
}

Seconds WebProcess::gpuProcessTimeoutDuration() const
{
    constexpr Seconds defaultTimeoutDuration = 15_s;
    return m_childProcessDebuggabilityEnabled ? Seconds::infinity() : defaultTimeoutDuration;
}

void WebProcess::gpuProcessConnectionClosed()
{
    ASSERT(m_gpuProcessConnection);
    m_gpuProcessConnection = nullptr;

    for (auto& page : m_pageMap.values())
        page->gpuProcessConnectionWasDestroyed();

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    if (m_audioMediaStreamTrackRendererInternalUnitManager)
        m_audioMediaStreamTrackRendererInternalUnitManager->restartAllUnits();
#endif
}

void WebProcess::gpuProcessConnectionDidBecomeUnresponsive()
{
    ASSERT(m_gpuProcessConnection);
    protectedParentProcessConnection()->send(Messages::WebProcessProxy::GPUProcessConnectionDidBecomeUnresponsive(m_gpuProcessConnection->identifier()), 0);
}

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
LibWebRTCCodecs& WebProcess::libWebRTCCodecs()
{
    if (!m_libWebRTCCodecs)
        m_libWebRTCCodecs = LibWebRTCCodecs::create();
    return *m_libWebRTCCodecs;
}

Ref<LibWebRTCCodecs> WebProcess::protectedLibWebRTCCodecs()
{
    return libWebRTCCodecs();
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

RemoteSharedResourceCacheProxy& WebProcess::gpuProcessSharedResourceCache()
{
    if (!m_sharedResourceCache) {
        m_sharedResourceCache = RemoteSharedResourceCacheProxy::create();
        if (m_gpuProcessConnection)
            Ref { *m_sharedResourceCache }->gpuProcessConnectionDidBecomeAvailable(Ref { *m_gpuProcessConnection });
    }
    return *m_sharedResourceCache;
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)

ModelProcessConnection& WebProcess::ensureModelProcessConnection()
{
    RELEASE_ASSERT(RunLoop::isMain());

    // If we've lost our connection to the model process (e.g. it crashed) try to re-establish it.
    if (!m_modelProcessConnection) {
        m_modelProcessConnection = ModelProcessConnection::create(Ref { *parentProcessConnection() });

        for (auto& page : m_pageMap.values())
            page->modelProcessConnectionDidBecomeAvailable(Ref { *m_modelProcessConnection });
    }

    return *m_modelProcessConnection;
}

void WebProcess::modelProcessConnectionClosed(ModelProcessConnection& connection)
{
    ASSERT(m_modelProcessConnection);
    ASSERT_UNUSED(connection, m_modelProcessConnection == &connection);

    m_modelProcessConnection = nullptr;
}

#endif // ENABLE(MODEL_PROCESS)

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

void WebProcess::setTextCheckerState(OptionSet<TextCheckerState> textCheckerState)
{
    bool continuousSpellCheckingTurnedOff = !textCheckerState.contains(TextCheckerState::ContinuousSpellCheckingEnabled) && m_textCheckerState.contains(TextCheckerState::ContinuousSpellCheckingEnabled);
    bool grammarCheckingTurnedOff = !textCheckerState.contains(TextCheckerState::GrammarCheckingEnabled) && m_textCheckerState.contains(TextCheckerState::GrammarCheckingEnabled);

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

    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics))
        clearResourceLoadStatistics();

    completionHandler();
}

void WebProcess::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    m_cookieJar->clearCache();
    completionHandler();
}

void WebProcess::deleteWebsiteDataForOrigin(OptionSet<WebsiteDataType> websiteDataTypes, const ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(websiteDataTypes.contains(WebsiteDataType::MemoryCache)); // This would be useless IPC otherwise.
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache)) {
        MemoryCache::singleton().removeResourcesWithOrigin(origin);
        if (origin.topOrigin == origin.clientOrigin)
            BackForwardCache::singleton().clearEntriesForOrigins({ RefPtr<SecurityOrigin> { origin.clientOrigin.securityOrigin() } });
    }
    completionHandler();
}

void WebProcess::reloadExecutionContextsForOrigin(const ClientOrigin& origin, std::optional<FrameIdentifier> triggeringFrame, CompletionHandler<void()>&& completionHandler)
{
    for (auto& page : m_pageMap.values()) {
        if (RefPtr corePage = page->corePage())
            corePage->reloadExecutionContextsForOrigin(origin, triggeringFrame);
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
        BackForwardCache::singleton().clearEntriesForOrigins(origins);
    }
    completionHandler();
}

void WebProcess::setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds seconds)
{
    for (auto& page : m_pageMap.values())
        page->setHiddenPageDOMTimerThrottlingIncreaseLimit(seconds);
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

void WebProcess::bindAccessibilityFrameWithData(WebCore::FrameIdentifier, std::span<const uint8_t>)
{
}

#endif

void WebProcess::pageActivityStateDidChange(PageIdentifier, OptionSet<WebCore::ActivityState> changed)
{
    if (changed & WebCore::ActivityState::IsVisible) {
        updateCPUMonitorState(CPUMonitorUpdateReason::VisibilityHasChanged);
#if OS(LINUX)
        RealTimeThreads::singleton().setEnabled(hasVisibleWebPage());
#endif
    }
}

void WebProcess::releaseMemory(CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "releaseMemory: BEGIN");
    SetForScope allowExitScope(m_allowExitOnMemoryPressure, false);
    MemoryPressureHandler::singleton().releaseMemory(Critical::Yes, Synchronous::Yes);
    for (auto& page : m_pageMap.values())
        page->releaseMemory(Critical::Yes);
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "releaseMemory: END");
    completionHandler();
}

void WebProcess::prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&& completionHandler)
{
#if !RELEASE_LOG_DISABLED
    auto nowTime = MonotonicTime::now();
    double remainingRunTime = nowTime > estimatedSuspendTime ? (nowTime - estimatedSuspendTime).value() : 0.0;
#endif

    WEBPROCESS_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPROCESS_PREPARE_TO_SUSPEND, isSuspensionImminent, remainingRunTime);
    SetForScope allowExitScope(m_allowExitOnMemoryPressure, false);
    m_processIsSuspended = true;

    flushResourceLoadStatistics();

#if PLATFORM(COCOA)
    if (m_processType == ProcessType::PrewarmedWebContent) {
        WEBPROCESS_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPROCESS_READY_TO_SUSPEND);
        return completionHandler();
    }
#endif

#if ENABLE(VIDEO)
    suspendAllMediaBuffering();

    for (auto& page : m_pageMap.values())
        page->processWillSuspend();
#endif

    // Ask the process to slim down before it suspends, in case it suspends for a very long time.
    // We only allow this once for every time the process contains a visible page, to prevent
    // ourselves from constantly running releaseMemory if the process suspends and resumes a lot
    // while in the background. If the process is being cached for perf reasons, we don't dump
    // caches since we want those memory caches to contain useful state once the process is reused.
    if (!m_suppressMemoryPressureHandler && m_wasVisibleSinceLastProcessSuspensionEvent && !m_pageMap.isEmpty() && !isProcessBeingCachedForPerformance())
        releaseMemory([] { });
    m_wasVisibleSinceLastProcessSuspensionEvent = false;

    freezeAllLayerTrees();

#if PLATFORM(COCOA)
    destroyRenderingResources();
    accessibilityRelayProcessSuspended(true);
#endif

#if PLATFORM(IOS_FAMILY)
    updateFreezerStatus();
#endif

    markAllLayersVolatile([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        WEBPROCESS_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPROCESS_READY_TO_SUSPEND);
        completionHandler();
    });
}

void WebProcess::accessibilityRelayProcessSuspended(bool suspended)
{
    if (m_pageMap.isEmpty()) {
        // Depending on timing, we can get a call to unsuspend the process at a moment when we don't have
        // any webpages (but may gain them soon). Set this flag so we can unsuspend when we do get a webpage,
        // as otherwise assistive technologies may think we are permanently suspended.
        m_hasPendingAccessibilityUnsuspension = !suspended;
        return;
    }

    // Take the first webpage. We only need to have the process on the other side relay this for the WebProcess.
    AXRelayProcessSuspendedNotification(m_pageMap.begin()->value, AXRelayProcessSuspendedNotification::AutomaticallySend::No).sendProcessSuspendMessage(suspended);
}

void WebProcess::markAllLayersVolatile(CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESS_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPROCESS_MARK_ALL_LAYERS_VOLATILE);
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& page : m_pageMap.values()) {
        page->markLayersVolatile([this, protectedThis = Ref { *this }, callbackAggregator, pageID = page->identifier()] (bool succeeded) {
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
    WEBPROCESS_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPROCESS_FREEZE_ALL_LAYER_TREES);
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

#if PLATFORM(COCOA)
    accessibilityRelayProcessSuspended(false);
#endif

#if ENABLE(VIDEO)
    for (auto& page : m_pageMap.values())
        page->processDidResume();
    resumeAllMediaBuffering();
#endif
}

void WebProcess::sendPrewarmInformation(const URL& url)
{
    auto registrableDomain = WebCore::RegistrableDomain { url };
    if (registrableDomain.isEmpty())
        return;
    protectedParentProcessConnection()->send(Messages::WebProcessProxy::DidCollectPrewarmInformation(registrableDomain, WebCore::ProcessWarming::collectPrewarmInformation()), 0);
}

void WebProcess::jSHandleDestroyed(WebCore::JSHandleIdentifier identifier)
{
    WebCore::WebKitJSHandle::jsHandleDestroyed(identifier);
}

void WebProcess::pageDidEnterWindow(PageIdentifier pageID)
{
    m_pagesInWindows.add(pageID);
    m_nonVisibleProcessEarlyMemoryCleanupTimer.reset();

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
    m_nonVisibleProcessMemoryCleanupTimer.stop();
#endif

    m_wasVisibleSinceLastProcessSuspensionEvent = true;
}

void WebProcess::pageWillLeaveWindow(PageIdentifier pageID)
{
    m_pagesInWindows.remove(pageID);

    if (m_pagesInWindows.isEmpty()) {
        if (!m_nonVisibleProcessEarlyMemoryCleanupTimer)
            m_nonVisibleProcessEarlyMemoryCleanupTimer.emplace(*this, &WebProcess::nonVisibleProcessEarlyMemoryCleanupTimerFired, nonVisibleProcessEarlyMemoryCleanupDelay);
        m_nonVisibleProcessEarlyMemoryCleanupTimer->restart();

#if ENABLE(NON_VISIBLE_WEBPROCESS_MEMORY_CLEANUP_TIMER)
        if (!m_nonVisibleProcessMemoryCleanupTimer.isActive())
            m_nonVisibleProcessMemoryCleanupTimer.startOneShot(nonVisibleProcessMemoryCleanupDelay);
#endif
    }
}

bool WebProcess::isProcessBeingCachedForPerformance()
{
#if PLATFORM(COCOA)
    return m_processType == ProcessType::CachedWebContent || m_processType == ProcessType::PrewarmedWebContent || areAllPagesSuspended();
#else
    return false;
#endif
}

void WebProcess::nonVisibleProcessEarlyMemoryCleanupTimerFired()
{
    ASSERT(m_pagesInWindows.isEmpty());
    if (!m_pagesInWindows.isEmpty())
        return;

    destroyDecodedDataForAllImages();

#if PLATFORM(COCOA) || PLATFORM(WPE) || PLATFORM(GTK)
#if PLATFORM(COCOA)
    destroyRenderingResources();
#endif
    releaseSystemMallocMemory();
#endif
}

void WebProcess::destroyDecodedDataForAllImages()
{
    // Only do this when UI side compositing is enabled, since for now only
    // RemoteLayerTreeDrawingArea will correctly synchronously decode images
    // after their decoded data has been destroyed.
    for (auto& page : m_pageMap.values()) {
        if (!page->isUsingUISideCompositing())
            return;
    }

    for (auto& page : m_pageMap.values())
        page->willDestroyDecodedDataForAllImages();

    MemoryCache::singleton().destroyDecodedDataForAllImages();
}

void WebProcess::deferNonVisibleProcessEarlyMemoryCleanupTimer()
{
    if (m_nonVisibleProcessEarlyMemoryCleanupTimer)
        m_nonVisibleProcessEarlyMemoryCleanupTimer->restart();
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
    ASSERT(m_storageAreaMaps.get(identifier) == &storageAreaMap);
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
    if (enabled && !ResourceLoadObserver::singletonIfExists())
        WebCore::ResourceLoadObserver::setShared(*new WebResourceLoadObserver(m_sessionID && m_sessionID->isEphemeral() ? WebCore::ResourceLoadStatistics::IsEphemeral::Yes : WebCore::ResourceLoadStatistics::IsEphemeral::No));
}

void WebProcess::clearResourceLoadStatistics()
{
    if (auto* observer = ResourceLoadObserver::singletonIfExists())
        observer->clearState();
    for (auto& page : m_pageMap.values()) {
        page->clearPageLevelStorageAccess();
        page->revokeFrameSpecificStorageAccess();
    }
}

void WebProcess::flushResourceLoadStatistics()
{
    if (auto* observer = ResourceLoadObserver::singletonIfExists())
        observer->updateCentralStatisticsStore([] { });
}

void WebProcess::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
    if (auto* observer = ResourceLoadObserver::singletonIfExists())
        observer->logSubresourceLoadingForTesting(firstPartyDomain, thirdPartyDomain, shouldScheduleNotification);
    completionHandler();
}

RefPtr<API::Object> WebProcess::transformHandlesToObjects(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle:
                return downcast<const API::FrameHandle>(object).isAutoconverting();

            case API::Object::Type::PageHandle:
                return downcast<const API::PageHandle>(object).isAutoconverting();

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::FrameHandle: {
                auto frameID = downcast<const API::FrameHandle>(object).frameID();
                return frameID ? WebProcess::singleton().webFrame(*frameID) : nullptr;
            }
            case API::Object::Type::PageHandle:
                return WebProcess::singleton().webPage(downcast<const API::PageHandle>(object).webPageID());

            default:
                return &object;
            }
        }
    };

    return UserData::transform(object, Transformer());
}

RefPtr<API::Object> WebProcess::transformObjectsToHandles(API::Object* object)
{
    struct Transformer final : UserData::Transformer {
        bool shouldTransformObject(const API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::BundleFrame:
            case API::Object::Type::BundlePage:
                return true;

            default:
                return false;
            }
        }

        RefPtr<API::Object> transformObject(API::Object& object) const override
        {
            switch (object.type()) {
            case API::Object::Type::BundleFrame:
                return API::FrameHandle::createAutoconverting(downcast<const WebFrame>(object).frameID());

            case API::Object::Type::BundlePage:
                return API::PageHandle::createAutoconverting(downcast<const WebPage>(object).webPageProxyIdentifier(), downcast<const WebPage>(object).identifier());

            default:
                return &object;
            }
        }
    };

    return UserData::transform(object, Transformer());
}

void WebProcess::setMemoryCacheDisabled(bool disabled)
{
    Ref memoryCache = MemoryCache::singleton();
    if (memoryCache->disabled() != disabled)
        memoryCache->setDisabled(disabled);
}

#if ENABLE(SERVICE_CONTROLS)
void WebProcess::setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices)
{
    m_hasImageServices = hasImageServices;
    m_hasSelectionServices = hasSelectionServices;
    m_hasRichContentServices = hasRichContentServices;
}
#endif

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
void WebProcess::setOptInCookiePartitioningEnabled(bool enabled)
{
    m_cookieJar->setOptInCookiePartitioningEnabled(enabled);
}
#endif

void WebProcess::ensureAutomationSessionProxy(const String& sessionIdentifier)
{
    m_automationSessionProxy = WebAutomationSessionProxy::create(sessionIdentifier);
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
    BackForwardCache::singleton().remove(backForwardItemID);
    completionHandler();
}

#if USE(LIBWEBRTC)
LibWebRTCNetwork& WebProcess::libWebRTCNetwork()
{
    if (!m_libWebRTCNetwork)
        lazyInitialize(m_libWebRTCNetwork, makeUniqueWithoutRefCountedCheck<LibWebRTCNetwork>(*this));
    return *m_libWebRTCNetwork;
}

Ref<LibWebRTCNetwork> WebProcess::protectedLibWebRTCNetwork()
{
    return libWebRTCNetwork();
}
#endif

void WebProcess::establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType workerType, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, Site&& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, RemoteWorkerInitializationData&& initializationData, CompletionHandler<void()>&& completionHandler)
{
    // We are in the Remote Worker context process and the call below establishes our connection to the Network Process
    // by calling ensureNetworkProcessConnection. SWContextManager / SharedWorkerContextManager need to use the same underlying IPC::Connection as the
    // NetworkProcessConnection for synchronization purposes.
    Ref ipcConnection = ensureNetworkProcessConnection().connection();
    switch (workerType) {
    case RemoteWorkerType::ServiceWorker:
        SWContextManager::singleton().setConnection(WebSWContextManagerConnection::create(WTFMove(ipcConnection), WTFMove(site), serviceWorkerPageIdentifier, pageGroupID, webPageProxyID, pageID, store, WTFMove(initializationData)));
        SWContextManager::singleton().protectedConnection()->establishConnection(WTFMove(completionHandler));
        break;
    case RemoteWorkerType::SharedWorker:
        SharedWorkerContextManager::singleton().setConnection(WebSharedWorkerContextManagerConnection::create(WTFMove(ipcConnection), WTFMove(site), pageGroupID, webPageProxyID, pageID, store, WTFMove(initializationData)));
        SharedWorkerContextManager::singleton().protectedConnection()->establishConnection(WTFMove(completionHandler));
        break;
    }
}

void WebProcess::registerServiceWorkerClients(CompletionHandler<void(bool)>&& completionHandler)
{
    ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::PingPongForServiceWorkers { }, WTFMove(completionHandler));
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

bool WebProcess::registerServiceWorker(WebCore::ServiceWorkerIdentifier identifier)
{
    return m_swServiceWorkerCounts.add(identifier).isNewEntry;
}

bool WebProcess::unregisterServiceWorker(WebCore::ServiceWorkerIdentifier identifier)
{
    ASSERT(m_swServiceWorkerCounts.contains(identifier));
    return m_swServiceWorkerCounts.remove(identifier);
}

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
        RefPtr { extension.second }->consume();
        WEBPROCESS_RELEASE_LOG(WebRTC, "grantUserMediaDeviceSandboxExtensions: granted extension %s", extension.first.utf8().data());
        m_mediaCaptureSandboxExtensions.add(extension.first, extension.second.copyRef());
    }
    m_machBootstrapExtension = extensions.machBootstrapExtension();
    if (RefPtr machBootstrapExtension = m_machBootstrapExtension)
        machBootstrapExtension->consume();
}

static inline void checkDocumentsCaptureStateConsistency(const Vector<String>& extensionIDs)
{
#if ASSERT_ENABLED
    bool isCapturingAudio = std::ranges::any_of(Document::allDocumentsMap().values(), [](auto& document) {
        return static_cast<bool>(document->mediaState() & MediaProducer::MicrophoneCaptureMask);
    });
    bool isCapturingVideo = std::ranges::any_of(Document::allDocumentsMap().values(), [](auto& document) {
        return static_cast<bool>(document->mediaState() & MediaProducer::VideoCaptureMask);
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
    
    if (RefPtr machBootstrapExtension = m_machBootstrapExtension)
        machBootstrapExtension->revoke();
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
    return std::ranges::all_of(m_pageMap.values(), [](auto& page) {
        return page->isThrottleable();
    });
}

void WebProcess::setAppBadge(WebCore::Frame* frame, const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (DeprecatedGlobalSettings::builtInNotificationsEnabled()) {
        if (m_sessionID)
            ensureNetworkProcessConnection().connection().send(Messages::NotificationManagerMessageHandler::SetAppBadge({ origin, badge }), m_sessionID->toUInt64());
        return;
    }
#endif

    RefPtr protectedFrame = frame;
    if (frame) {
        if (auto webFrame = WebFrame::fromCoreFrame(*frame))
            webFrame->setAppBadge(origin, badge);
    } else
        protectedParentProcessConnection()->send(Messages::WebProcessProxy::SetAppBadgeFromWorker(origin, badge), 0);
}

#if HAVE(DISPLAY_LINK)
void WebProcess::displayDidRefresh(uint32_t displayID, const DisplayUpdate& displayUpdate)
{
    ASSERT(RunLoop::isMain());
    protectedEventDispatcher()->notifyScrollingTreesDisplayDidRefresh(displayID);
    DisplayRefreshMonitorManager::sharedManager().displayDidRefresh(displayID, displayUpdate);
}
#endif

void WebProcess::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode, CompletionHandler<void()>&& completionHandler)
{
    if (m_thirdPartyCookieBlockingMode != thirdPartyCookieBlockingMode) {
        m_thirdPartyCookieBlockingMode = thirdPartyCookieBlockingMode;
        if (m_thirdPartyCookieBlockingMode != ThirdPartyCookieBlockingMode::All)
            updateCachedCookiesEnabled();
    }
    completionHandler();
}

void WebProcess::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    ResourceLoadObserver::singleton().setDomainsWithUserInteraction(WTFMove(domains));
}

void WebProcess::setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, Vector<SubResourceDomain>>&& domains, CompletionHandler<void()>&& completionHandler)
{
    for (auto& [domain, subResourceDomains] : domains) {
        for (auto& subResourceDomain : subResourceDomains) {
            for (auto& webPage : m_pageMap.values())
                webPage->addDomainWithPageLevelStorageAccess(domain, subResourceDomain);
        }
    }
    ResourceLoadObserver::singleton().setDomainsWithCrossPageStorageAccess(WTFMove(domains), WTFMove(completionHandler));
}

void WebProcess::sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&& completionHandler)
{
    ResourceLoadObserver::singleton().updateCentralStatisticsStore(WTFMove(completionHandler));
}

bool WebProcess::haveStorageAccessQuirksForDomain(const WebCore::RegistrableDomain& domain)
{
    return m_domainsWithStorageAccessQuirks.contains(domain);
}

void WebProcess::updateDomainsWithStorageAccessQuirks(HashSet<WebCore::RegistrableDomain>&& domainsWithStorageAccessQuirks)
{
    m_domainsWithStorageAccessQuirks.clear();
    for (auto&& domain : domainsWithStorageAccessQuirks)
        m_domainsWithStorageAccessQuirks.add(domain);
}

void WebProcess::updateScriptTrackingPrivacyFilter(ScriptTrackingPrivacyRules&& rules)
{
    if (rules.isEmpty())
        return;

    m_scriptTrackingPrivacyFilter = WTF::makeUnique<ScriptTrackingPrivacyFilter>(WTFMove(rules));
}

void WebProcess::setChildProcessDebuggabilityEnabled(bool childProcessDebuggabilityEnabled)
{
    m_childProcessDebuggabilityEnabled = childProcessDebuggabilityEnabled;
}

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
        protectedCDMFactory()->registerFactory(cdmFactories);
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
        MediaSessionHelper::setSharedHelper(adoptRef(*new RemoteMediaSessionHelper()));
    else
        MediaSessionHelper::resetSharedHelper();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (useGPUProcessForMedia)
        protectedLegacyCDMFactory()->registerFactory();
    else
        LegacyCDM::resetFactories();
#endif

    if (useGPUProcessForMedia)
        Ref { mediaEngineConfigurationFactory() }->registerFactory();
    else
        MediaEngineConfigurationFactory::resetFactories();

    if (useGPUProcessForMedia) {
        WebCore::AudioHardwareListener::setCreationFunction([] (WebCore::AudioHardwareListener::Client& client) {
            return RemoteAudioHardwareListener::create(client);
        });
    } else
        WebCore::AudioHardwareListener::resetCreationFunction();

    if (useGPUProcessForMedia) {
        WebCore::RemoteCommandListener::setCreationFunction([] (WebCore::RemoteCommandListenerClient& client) {
            return RemoteRemoteCommandListener::create(client);
        });
    } else
        WebCore::RemoteCommandListener::resetCreationFunction();

#if PLATFORM(COCOA)
    if (useGPUProcessForMedia) {
        SystemBatteryStatusTestingOverrides::singleton().setConfigurationChangedCallback([this, protectedThis = Ref { *this }] (bool forceUpdate) {
            ensureProtectedGPUProcessConnection()->updateMediaConfiguration(forceUpdate);
        });
#if ENABLE(VP9)
        VP9TestingOverrides::singleton().setConfigurationChangedCallback([this, protectedThis = Ref { *this }] (bool forceUpdate) {
            ensureProtectedGPUProcessConnection()->updateMediaConfiguration(forceUpdate);
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
    case RenderingPurpose::ShareableLocalSnapshot:
    case RenderingPurpose::Unspecified:
        return false;
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
#if USE(COORDINATED_GRAPHICS)
#if USE(GBM)
    return m_useGPUProcessForWebGL && WebCore::GraphicsContextGLTextureMapperGBM::checkRequirements();
#else
    return false;
#endif
#else
    return m_useGPUProcessForWebGL;
#endif
}
#endif // ENABLE(WEBGL)

#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MEDIA_STREAM)
SpeechRecognitionRealtimeMediaSourceManager& WebProcess::ensureSpeechRecognitionRealtimeMediaSourceManager()
{
    if (!m_speechRecognitionRealtimeMediaSourceManager)
        lazyInitialize(m_speechRecognitionRealtimeMediaSourceManager, makeUniqueWithoutRefCountedCheck<SpeechRecognitionRealtimeMediaSourceManager>(*this));

    return *m_speechRecognitionRealtimeMediaSourceManager;
}
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
RemoteLegacyCDMFactory& WebProcess::legacyCDMFactory()
{
    return *supplement<RemoteLegacyCDMFactory>();
}

Ref<RemoteLegacyCDMFactory> WebProcess::protectedLegacyCDMFactory()
{
    return legacyCDMFactory();
}
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
RemoteCDMFactory& WebProcess::cdmFactory()
{
    return *supplement<RemoteCDMFactory>();
}

Ref<RemoteCDMFactory> WebProcess::protectedCDMFactory()
{
    return cdmFactory();
}
#endif

#if ENABLE(GPU_PROCESS)
RemoteMediaEngineConfigurationFactory& WebProcess::mediaEngineConfigurationFactory()
{
    return *supplement<RemoteMediaEngineConfigurationFactory>();
}
#endif

Ref<WebNotificationManager> WebProcess::protectedNotificationManager()
{
    return *supplement<WebNotificationManager>();
}

RefPtr<WebTransportSession> WebProcess::webTransportSession(WebTransportSessionIdentifier identifier)
{
    Locker locker { m_webTransportSessionsLock };
    return m_webTransportSessions.get(identifier).get();
}

void WebProcess::addWebTransportSession(WebTransportSessionIdentifier identifier, WebTransportSession& session)
{
    Locker locker { m_webTransportSessionsLock };
    ASSERT(!m_webTransportSessions.contains(identifier));
    m_webTransportSessions.set(identifier, session);
}

void WebProcess::removeWebTransportSession(WebTransportSessionIdentifier identifier)
{
    Locker locker { m_webTransportSessionsLock };
    ASSERT(m_webTransportSessions.contains(identifier));
    m_webTransportSessions.remove(identifier);
}

#if USE(LIBRICE)
RefPtr<RiceBackendProxy> WebProcess::gstreamerIceBackend(RiceBackendIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    return m_gstreamerIceBackends.get(identifier).get();
}

void WebProcess::addRiceBackend(RiceBackendIdentifier identifier, RiceBackendProxy& backend)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_gstreamerIceBackends.contains(identifier));
    m_gstreamerIceBackends.set(identifier, backend);
}

void WebProcess::removeRiceBackend(RiceBackendIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_gstreamerIceBackends.contains(identifier));
    m_gstreamerIceBackends.remove(identifier);
}
#endif // USE(LIBRICE)

void WebProcess::updateCachedCookiesEnabled()
{
    for (auto& document : Document::allDocuments())
        document->updateCachedCookiesEnabled();
}

bool WebProcess::requiresScriptTrackingPrivacyProtections(const URL& url, const SecurityOrigin& topOrigin) const
{
    return m_scriptTrackingPrivacyFilter && m_scriptTrackingPrivacyFilter->matches(url, topOrigin);
}

bool WebProcess::shouldAllowScriptAccess(const URL& url, const SecurityOrigin& topOrigin, ScriptTrackingPrivacyCategory category) const
{
    return m_scriptTrackingPrivacyFilter && m_scriptTrackingPrivacyFilter->shouldAllowAccess(url, topOrigin, category);
}

void WebProcess::enableMediaPlayback()
{
    m_mediaPlaybackEnabled = true;

#if USE(AUDIO_SESSION)
    if (!WebCore::AudioSession::enableMediaPlayback())
        return;
#endif

#if ENABLE(ROUTING_ARBITRATION)
    lazyInitialize(m_routingArbitrator, makeUniqueWithoutRefCountedCheck<AudioSessionRoutingArbitrator>(*this));
#endif
}

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
Ref<RemoteMediaPlayerManager> WebProcess::protectedRemoteMediaPlayerManager()
{
    return m_remoteMediaPlayerManager;
}
#endif

Ref<WebCookieJar> WebProcess::protectedCookieJar()
{
    return m_cookieJar;
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebProcess::setResourceMonitorContentRuleList(WebCompiledContentRuleListData&& ruleListData)
{
    WEBPROCESS_RELEASE_LOG(ResourceMonitoring, "setResourceMonitorContentRuleList");

    RefPtr compiledContentRuleList = WebCompiledContentRuleList::create(WTFMove(ruleListData));
    if (!compiledContentRuleList) {
        WEBPROCESS_RELEASE_LOG_ERROR(ResourceMonitoring, "setResourceMonitorContentRuleList: Failed to create rule list");
        return;
    }

    WebCore::ContentExtensions::ContentExtensionsBackend backend;
    auto identifier = compiledContentRuleList->data().identifier;
    backend.addContentExtension(identifier, compiledContentRuleList.releaseNonNull(), { }, ContentExtensions::ContentExtension::ShouldCompileCSS::No);

    WebCore::ResourceMonitorChecker::singleton().setContentRuleList(WTFMove(backend));
}

void WebProcess::setResourceMonitorContentRuleListAsync(WebCompiledContentRuleListData&& ruleListData, CompletionHandler<void()>&& completionHandler)
{
    WEBPROCESS_RELEASE_LOG(ResourceMonitoring, "setResourceMonitorContentRuleListAsync");
    setResourceMonitorContentRuleList(WTFMove(ruleListData));
    completionHandler();
}
#endif

void WebProcess::didReceiveRemoteCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    for (auto& page : m_pageMap.values())
        page->didReceiveRemoteCommand(type, argument);
}

void WebProcess::contentWorldDestroyed(ContentWorldIdentifier identifier)
{
    WebUserContentController::removeContentWorld(identifier);
}

} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef WEBPROCESS_RELEASE_LOG
#undef WEBPROCESS_RELEASE_LOG_ERROR
