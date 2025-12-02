/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "WebProcessPool.h"

#include "APIArray.h"
#include "APIAutomationClient.h"
#include "APICustomProtocolManagerClient.h"
#include "APIDownloadClient.h"
#include "APIHTTPCookieStore.h"
#include "APIInjectedBundleClient.h"
#include "APILegacyContextHistoryClient.h"
#include "APINavigation.h"
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "APIString.h"
#include "APIURLRequest.h"
#include "AuxiliaryProcessMessages.h"
#include "AuxiliaryProcessProxy.h"
#include "BrowsingContextGroup.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "FrameProcess.h"
#include "GPUProcessConnectionParameters.h"
#include "GamepadData.h"
#include "LegacyGlobalSettings.h"
#include "LoadedWebArchive.h"
#include "Logging.h"
#include "ModelProcessConnectionParameters.h"
#include "ModelProcessProxy.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "OverrideLanguages.h"
#include "PageLoadState.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "ProcessTerminationReason.h"
#include "RemotePageProxy.h"
#include "RemoteWorkerType.h"
#include "RestrictedOpenerType.h"
#include "SandboxExtension.h"
#include "SuspendedPageProxy.h"
#include "TextChecker.h"
#include "UIGamepad.h"
#include "UIGamepadProvider.h"
#include "UIProcessLogInitialization.h"
#include "WKAPICast.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCompiledContentRuleList.h"
#include "WebContextSupplement.h"
#include "WebFrameProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebInspectorUtilities.h"
#include "WebKit2Initialize.h"
#include "WebKitServiceNames.h"
#include "WebMemorySampler.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebPreferencesKeys.h"
#include "WebProcessCache.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessDataStoreParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <JavaScriptCore/JSCInlines.h>
#include <WebCore/GamepadProvider.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessSwapDisposition.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/Site.h>
#include <algorithm>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URLParser.h>
#include <wtf/WallTime.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ServicesController.h"
#endif

#if ENABLE(GPU_PROCESS)
#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
#include "GPUProcessProxy.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if OS(LINUX)
#include "MemoryPressureMonitor.h"
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#include "WebPrivacyHelpers.h"
#include <WebCore/GameControllerGamepadProvider.h>
#include <WebCore/HIDGamepadProvider.h>
#include <WebCore/MultiGamepadProvider.h>
#include <WebCore/PowerSourceNotifier.h>
#include <wtf/cocoa/Entitlements.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
#include "ExtensionCapabilityGrant.h"
#include "ExtensionCapabilityGranter.h"
#include "MediaCapability.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionMatchPattern.h"
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
#include <sys/sysctl.h>
#endif

#define WEBPROCESSPOOL_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - WebProcessPool::" fmt, static_cast<void*>(this), ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_STATIC(channel, fmt, ...) RELEASE_LOG(channel, "WebProcessPool::" fmt, ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - WebProcessPool::" fmt, static_cast<void*>(this), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

#if ENABLE(GPU_PROCESS)
constexpr Seconds resetGPUProcessCrashCountDelay { 30_s };
constexpr unsigned maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses { 2 };
#endif

#if ENABLE(MODEL_PROCESS)
constexpr Seconds resetModelProcessCrashCountDelay { 30_s };
constexpr unsigned maximumModelProcessRelaunchAttemptsBeforeKillingWebProcesses { 2 };
#endif

Ref<WebProcessPool> WebProcessPool::create(API::ProcessPoolConfiguration& configuration)
{
    InitializeWebKit2();
    return adoptRef(*new WebProcessPool(configuration));
}

static Vector<WeakRef<WebProcessPool>>& processPools()
{
    static NeverDestroyed<Vector<WeakRef<WebProcessPool>>> processPools;
    return processPools;
}

Vector<Ref<WebProcessPool>> WebProcessPool::allProcessPools()
{
    return WTF::map(processPools(), [] (auto&& v) -> Ref<WebProcessPool> {
        return v.get();
    });
}

static HashSet<String, ASCIICaseInsensitiveHash>& globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> set;
    return set;
}

bool WebProcessPool::globalDelaysWebProcessLaunchDefaultValue()
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Delayed process launch is currently disabled on iOS for performance reasons (rdar://problem/49074131).
    return false;
#else
    return true;
#endif
}

Vector<String> WebProcessPool::urlSchemesWithCustomProtocolHandlers()
{
    return copyToVector(globalURLSchemesWithCustomProtocolHandlers());
}

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
static Seconds memoryPressureCheckInterval()
{
    static Seconds interval = []() {
        auto value = CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessMemoryPressureCheckInterval"), kCFPreferencesCurrentApplication, nullptr);
        return value > 0 ? Seconds(value) : 8_min;
    }();
    return interval;
}
#endif

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundleClient(makeUnique<API::InjectedBundleClient>())
    , m_automationClient(makeUnique<API::AutomationClient>())
    , m_historyClient(makeUnique<API::LegacyContextHistoryClient>())
    , m_visitedLinkStore(VisitedLinkStore::create())
#if PLATFORM(MAC)
    , m_smartListsEnabled(TextChecker::state().contains(TextCheckerState::SmartListsEnabled))
    , m_perActivityStateCPUUsageSampler(makeUniqueRefWithoutRefCountedCheck<PerActivityStateCPUUsageSampler>(*this))
#endif
    , m_alwaysRunsAtBackgroundPriority(m_configuration->alwaysRunsAtBackgroundPriority())
    , m_shouldTakeUIBackgroundAssertion(m_configuration->shouldTakeUIBackgroundAssertion())
    , m_userObservablePageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_hiddenPageThrottlingAutoIncreasesCounter([this](RefCounterEvent) { m_hiddenPageThrottlingTimer.startOneShot(0_s); })
    , m_hiddenPageThrottlingTimer(RunLoop::mainSingleton(), "WebProcessPool::HiddenPageThrottlingTimer"_s, this, &WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit)
#if ENABLE(GPU_PROCESS)
    , m_resetGPUProcessCrashCountTimer(RunLoop::mainSingleton(), "WebProcessPool::ResetGPUProcessCrashCountTimer"_s, [this] { m_recentGPUProcessCrashCount = 0; })
#endif
#if ENABLE(MODEL_PROCESS)
    , m_resetModelProcessCrashCountTimer(RunLoop::mainSingleton(), "WebProcessPool::ResetModelProcessCrashCountTimer"_s, [this] { m_recentModelProcessCrashCount = 0; })
#endif
    , m_foregroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backgroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backForwardCache(makeUniqueRefWithoutRefCountedCheck<WebBackForwardCache>(*this))
    , m_webProcessCache(makeUniqueRefWithoutRefCountedCheck<WebProcessCache>(*this))
    , m_webProcessWithAudibleMediaCounter([this](RefCounterEvent) { updateAudibleMediaAssertions(); })
    , m_audibleActivityTimer(RunLoop::mainSingleton(), "WebProcessPool::AudibleActivityTimer"_s, this, &WebProcessPool::clearAudibleActivity)
    , m_webProcessWithMediaStreamingCounter([this](RefCounterEvent) { updateMediaStreamingActivity(); })
#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    , m_lastMemoryPressureStatusTime(ApproximateTime::now() - memoryPressureCheckInterval())
    , m_checkMemoryPressureStatusTimer(RunLoop::mainSingleton(), "WebProcessPool::CheckMemoryPressureStatusTimer"_s, this, &WebProcessPool::checkMemoryPressureStatus)
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    , m_resourceMonitorRuleListRefreshTimer(RunLoop::mainSingleton(), "WebProcessPool::ResourceMonitorRuleListRefreshTimer"_s, this, &WebProcessPool::loadOrUpdateResourceMonitorRuleList)
#endif
#if ENABLE(IPC_TESTING_API)
    , m_ipcTester(IPCTester::create())
#endif
{
    static auto s_needsGlobalStaticInitialization = NeedsGlobalStaticInitialization::Yes;
    auto needsGlobalStaticInitialization = std::exchange(s_needsGlobalStaticInitialization, NeedsGlobalStaticInitialization::No);
    if (needsGlobalStaticInitialization == NeedsGlobalStaticInitialization::Yes) {
        WTF::setProcessPrivileges(allPrivileges());
        WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
        Process::setIdentifier(WebCore::Process::generateIdentifier());
    }

    for (auto& scheme : m_configuration->alwaysRevalidatedURLSchemes())
        m_schemesToRegisterAsAlwaysRevalidated.add(scheme);

    for (const auto& urlScheme : m_configuration->cachePartitionedURLSchemes())
        m_schemesToRegisterAsCachePartitioned.add(urlScheme);

    platformInitialize(needsGlobalStaticInitialization);

#if OS(LINUX)
    if (!MemoryPressureMonitor::disabled())
        MemoryPressureMonitor::singleton().start();
#endif

    addMessageReceiver(Messages::WebProcessPool::messageReceiverName(), *this);
#if ENABLE(IPC_TESTING_API)
    addMessageReceiver(Messages::IPCTester::messageReceiverName(), m_ipcTester.get());
#endif

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    addSupplement<WebGeolocationManagerProxy>();
    addSupplement<WebNotificationManagerProxy>();

    processPools().append(*this);

    resolvePathsForSandboxExtensions();

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    UIProcess::initializeLoggingIfNecessary();
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

    ASSERT(RunLoop::isMain());

    updateBackForwardCacheCapacity();

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    Ref storageAccessUserAgentStringQuirkController = StorageAccessUserAgentStringQuirkController::sharedSingleton();
    Ref storageAccessPromptQuirkController = StorageAccessPromptQuirkController::sharedSingleton();

    m_storageAccessUserAgentStringQuirksDataUpdateObserver = storageAccessUserAgentStringQuirkController->observeUpdates([weakThis = WeakPtr { *this }] {
        // FIXME: Filter by process's site when site isolation is enabled
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendToAllProcesses(Messages::WebProcess::UpdateStorageAccessUserAgentStringQuirks(StorageAccessUserAgentStringQuirkController::sharedSingleton().cachedListData()));
    });

    m_storageAccessPromptQuirksDataUpdateObserver = storageAccessPromptQuirkController->observeUpdates([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            HashSet<WebCore::RegistrableDomain> domainSet;
            for (auto&& entry : StorageAccessPromptQuirkController::sharedSingleton().cachedListData()) {
                if (!entry.triggerPages.isEmpty()) {
                    for (auto&& page : entry.triggerPages)
                        domainSet.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(page.string()));
                    continue;
                }
                for (auto&& domain : entry.quirkDomains.keys())
                    domainSet.add(domain);
            }
            protectedThis->sendToAllProcesses(Messages::WebProcess::UpdateDomainsWithStorageAccessQuirks(domainSet));
        }
    });
    storageAccessPromptQuirkController->initializeIfNeeded();
    storageAccessUserAgentStringQuirkController->initializeIfNeeded();
#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
}

WebProcessPool::~WebProcessPool()
{
#if ENABLE(GPU_PROCESS)
    // Some apps keep destroying and reconstructing new WebProcessPool objects whenever
    // they create new web views (rdar://121128159). To avoid relaunching the GPUProcess
    // unnecessarily in this case, we keep the GPUProcess running for a minute after the
    // last WebProcessPool object gets destroyed.
    if (m_gpuProcess && GPUProcessProxy::singletonIfCreated() == m_gpuProcess.get())
        GPUProcessProxy::keepProcessAliveTemporarily();
#endif

    m_webProcessCache->clear();

    bool removed = processPools().removeFirst(*this);
    ASSERT_UNUSED(removed, removed);

    m_messageReceiverMap.invalidate();

    for (auto& supplement : m_supplements.values()) {
        supplement->processPoolDestroyed();
        supplement->clearProcessPool();
    }

    platformInvalidateContext();

#if ENABLE(GAMEPAD)
    if (!m_processesUsingGamepads.isEmptyIgnoringNullReferences())
        UIGamepadProvider::singleton().processPoolStoppedUsingGamepads(*this);
#endif

    // Only remaining processes should be pre-warmed ones as other keep the process pool alive.
    while (!m_processes.isEmpty()) {
        auto& process = m_processes.first();

        ASSERT(process->isPrewarmed());
        // We need to be the only one holding a reference to the pre-warmed process so that it gets destroyed.
        // WebProcessProxies currently always expect to have a WebProcessPool.
        ASSERT(process->hasOneRef());

        process->shutDown();
    }
}

void WebProcessPool::initializeClient(const WKContextClientBase* client)
{
    m_client.initialize(client);
}

void WebProcessPool::setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&& client)
{
    if (!client)
        m_injectedBundleClient = makeUnique<API::InjectedBundleClient>();
    else
        m_injectedBundleClient = WTFMove(client);
}

void WebProcessPool::setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>&& historyClient)
{
    if (!historyClient)
        m_historyClient = makeUnique<API::LegacyContextHistoryClient>();
    else
        m_historyClient = WTFMove(historyClient);
}

void WebProcessPool::setLegacyDownloadClient(RefPtr<API::DownloadClient>&& client)
{
    m_legacyDownloadClient = WTFMove(client);
}

void WebProcessPool::setAutomationClient(std::unique_ptr<API::AutomationClient>&& automationClient)
{
    if (!automationClient)
        m_automationClient = makeUnique<API::AutomationClient>();
    else
        m_automationClient = WTFMove(automationClient);
}

void WebProcessPool::setOverrideLanguages(Vector<String>&& languages)
{
    WebKit::setOverrideLanguages(WTFMove(languages));

    LOG_WITH_STREAM(Language, stream << "WebProcessPool is setting OverrideLanguages: " << languages);
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(overrideLanguages()));

#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->send(Messages::GPUProcess::UserPreferredLanguagesChanged(overrideLanguages()), 0);
#endif
#if USE(SOUP)
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::UserPreferredLanguagesChanged(overrideLanguages()), 0);
#endif
}

void WebProcessPool::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    sendToAllProcesses(Messages::WebProcess::FullKeyboardAccessModeChanged(fullKeyboardAccessEnabled));
}

#if OS(LINUX)
void WebProcessPool::sendMemoryPressureEvent(bool isCritical)
{
    sendToAllProcesses(Messages::AuxiliaryProcess::DidReceiveMemoryPressureEvent(isCritical));
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::AuxiliaryProcess::DidReceiveMemoryPressureEvent(isCritical), 0);
}
#endif

void WebProcessPool::textCheckerStateChanged()
{
    sendToAllProcesses(Messages::WebProcess::SetTextCheckerState(TextChecker::state()));
}

void WebProcessPool::setApplicationIsActive(bool isActive)
{
    m_webProcessCache->setApplicationIsActive(isActive);
}

static bool shouldReportNetworkOrGPUProcessCrash(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
    case ProcessTerminationReason::ExceededCPULimit:
    case ProcessTerminationReason::Unresponsive:
    case ProcessTerminationReason::Crash:
        return true;
    case ProcessTerminationReason::RequestedByClient:
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
    case ProcessTerminationReason::RequestedByModelProcess:
        return false;
    case ProcessTerminationReason::GPUProcessCrashedTooManyTimes:
    case ProcessTerminationReason::ModelProcessCrashedTooManyTimes:
    case ProcessTerminationReason::NonMainFrameWebContentProcessCrash:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

void WebProcessPool::networkProcessDidTerminate(NetworkProcessProxy& networkProcessProxy, ProcessTerminationReason reason)
{
    for (Ref process : m_processes)
        process->resetHasRegisteredServiceWorkerClients();

    if (shouldReportNetworkOrGPUProcessCrash(reason))
        m_client.networkProcessDidCrash(this, networkProcessProxy.processID(), reason);

    if (RefPtr automationSession = m_automationSession)
        automationSession->terminate();

    terminateServiceWorkers();
}

void WebProcessPool::serviceWorkerProcessCrashed(WebProcessProxy& proxy, ProcessTerminationReason reason)
{
    m_client.serviceWorkerProcessDidCrash(this, proxy.processID(), reason);
}

#if ENABLE(GPU_PROCESS)
GPUProcessProxy& WebProcessPool::ensureGPUProcess()
{
    if (!m_gpuProcess) {
        Ref gpuProcess = GPUProcessProxy::getOrCreate();
        m_gpuProcess = gpuProcess.copyRef();
        for (Ref process : m_processes)
            gpuProcess->updatePreferences(process);
        gpuProcess->updateScreenPropertiesIfNeeded();
    }
    return *m_gpuProcess;
}

Ref<GPUProcessProxy> WebProcessPool::ensureProtectedGPUProcess()
{
    return ensureGPUProcess();
}

void WebProcessPool::gpuProcessDidFinishLaunching(ProcessID)
{
    auto processes = m_processes;
    for (Ref process : processes)
        process->gpuProcessDidFinishLaunching();
}

void WebProcessPool::gpuProcessExited(ProcessID identifier, ProcessTerminationReason reason)
{
    WEBPROCESSPOOL_RELEASE_LOG(Process, "gpuProcessDidExit: PID=%d, reason=%" PUBLIC_LOG_STRING, identifier, processTerminationReasonToString(reason).characters());
    m_gpuProcess = nullptr;

    if (shouldReportNetworkOrGPUProcessCrash(reason))
        m_client.gpuProcessDidCrash(this, identifier, reason);

    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (Ref process : processes)
        process->gpuProcessExited(reason);

    if (reason == ProcessTerminationReason::Crash || reason == ProcessTerminationReason::Unresponsive) {
        if (++m_recentGPUProcessCrashCount > maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses) {
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "gpuProcessDidExit: GPU Process has crashed more than %u times in the last %g seconds, terminating all WebProcesses", maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses, resetGPUProcessCrashCountDelay.seconds());
            m_resetGPUProcessCrashCountTimer.stop();
            m_recentGPUProcessCrashCount = 0;
            terminateAllWebContentProcesses(ProcessTerminationReason::GPUProcessCrashedTooManyTimes);
        } else if (!m_resetGPUProcessCrashCountTimer.isActive())
            m_resetGPUProcessCrashCountTimer.startOneShot(resetGPUProcessCrashCountDelay);
    }
}

void WebProcessPool::createGPUProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, WebKit::GPUProcessConnectionParameters&& parameters)
{
#if PLATFORM(COCOA)
    parameters.applicationBundleIdentifier = applicationBundleIdentifier();
#endif
    ensureProtectedGPUProcess()->createGPUProcessConnection(webProcessProxy, WTFMove(connectionIdentifier), WTFMove(parameters));
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
ModelProcessProxy& WebProcessPool::ensureModelProcess()
{
    if (!m_modelProcess) {
        Ref modelProcess = ModelProcessProxy::getOrCreate();
        m_modelProcess = modelProcess.copyRef();
    }
    return *m_modelProcess;
}

Ref<ModelProcessProxy> WebProcessPool::ensureProtectedModelProcess(WebProcessProxy& requestingWebProcess)
{
    RELEASE_ASSERT(requestingWebProcess.sharedPreferencesForWebProcessValue().modelElementEnabled);
    RELEASE_ASSERT(requestingWebProcess.sharedPreferencesForWebProcessValue().modelProcessEnabled);

    return ensureModelProcess();
}

void WebProcessPool::modelProcessDidFinishLaunching(ProcessID)
{
    auto processes = m_processes;
    for (Ref process : processes)
        process->modelProcessDidFinishLaunching();
}

void WebProcessPool::modelProcessExited(ProcessID identifier, ProcessTerminationReason reason)
{
    WEBPROCESSPOOL_RELEASE_LOG(Process, "modelProcessDidExit: PID=%d, reason=%" PUBLIC_LOG_STRING, identifier, processTerminationReasonToString(reason).characters());
    m_modelProcess = nullptr;

    // TODO: notify m_client.modelProcessDidCrash for C API if needed here

    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (Ref process : processes)
        process->modelProcessExited(reason);

    if (reason == ProcessTerminationReason::Crash || reason == ProcessTerminationReason::Unresponsive) {
        if (++m_recentModelProcessCrashCount > maximumModelProcessRelaunchAttemptsBeforeKillingWebProcesses) {
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "modelProcessDidExit: Model Process has crashed more than %u times in the last %g seconds, terminating related WebProcesses", maximumModelProcessRelaunchAttemptsBeforeKillingWebProcesses, resetModelProcessCrashCountDelay.seconds());
            m_resetModelProcessCrashCountTimer.stop();
            m_recentModelProcessCrashCount = 0;
            terminateAllWebContentProcessesWithModelPlayers();
        } else if (!m_resetModelProcessCrashCountTimer.isActive())
            m_resetModelProcessCrashCountTimer.startOneShot(resetModelProcessCrashCountDelay);
    }

    m_processesWithModelPlayers.clear();
}

void WebProcessPool::createModelProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, WebKit::ModelProcessConnectionParameters&& parameters)
{
#if ENABLE(IPC_TESTING_API)
    parameters.ignoreInvalidMessageForTesting = webProcessProxy.ignoreInvalidMessageForTesting();
#endif

#if HAVE(AUDIT_TOKEN)
    parameters.presentingApplicationAuditToken = configuration().presentingApplicationProcessToken();
#endif

    ensureProtectedModelProcess(webProcessProxy)->createModelProcessConnection(webProcessProxy, WTFMove(connectionIdentifier), WTFMove(parameters));
}

void WebProcessPool::startedPlayingModels(IPC::Connection& connection)
{
    RefPtr proxy = webProcessProxyFromConnection(connection);
    if (!proxy)
        return;

    ASSERT(!m_processesWithModelPlayers.contains(*proxy));
    WEBPROCESSPOOL_RELEASE_LOG(Process, "startedPlayingModels (process=%p, PID=%i)", proxy.get(), proxy->processID());
    m_processesWithModelPlayers.add(*proxy);
    updateModelProcessAssertion();
}

void WebProcessPool::stoppedPlayingModels(IPC::Connection& connection)
{
    RefPtr proxy = webProcessProxyFromConnection(connection);
    if (!proxy)
        return;

    if (m_processesWithModelPlayers.contains(*proxy)) {
        WEBPROCESSPOOL_RELEASE_LOG(Process, "stoppedPlayingModels (process=%p, PID=%i)", proxy.get(), proxy->processID());
        m_processesWithModelPlayers.remove(*proxy);
        updateModelProcessAssertion();
    }
}

bool WebProcessPool::hasForegroundWebProcessesWithModels() const
{
    for (Ref process : m_processesWithModelPlayers) {
        if (process->isForeground())
            return true;
    }
    return false;
}

bool WebProcessPool::hasBackgroundWebProcessesWithModels() const
{
    for (Ref process : m_processesWithModelPlayers) {
        if (process->isBackground())
            return true;
    }
    return false;
}

void WebProcessPool::updateModelProcessAssertion()
{
    if (RefPtr modelProcess = ModelProcessProxy::singletonIfCreated())
        modelProcess->updateProcessAssertion();
}

void WebProcessPool::terminateAllWebContentProcessesWithModelPlayers()
{
    WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "terminateAllWebContentProcessesWithModelPlayers");
    for (auto& weakProcess : copyToVector(m_processesWithModelPlayers)) {
        if (RefPtr process = weakProcess.get()) {
            WEBPROCESSPOOL_RELEASE_LOG(Process, "terminateAllWebContentProcessesWithModelPlayers request termination of WebProcess: (process=%p, PID=%i)", process.get(), process->processID());
            process->requestTermination(ProcessTerminationReason::ModelProcessCrashedTooManyTimes);
        }
    }
}
#endif // ENABLE(MODEL_PROCESS)

bool WebProcessPool::s_useSeparateServiceWorkerProcess = false;

void WebProcessPool::establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType workerType, Site&& site, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID sessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&& completionHandler)
{
    RefPtr websiteDataStore = WebsiteDataStore::existingDataStoreForSessionID(sessionID);
    if (!websiteDataStore)
        websiteDataStore = WebsiteDataStore::defaultDataStore();
    if (!processPools().size())
        static NeverDestroyed<Ref<WebProcessPool>> remoteWorkerProcessPool(WebProcessPool::create(API::ProcessPoolConfiguration::create().get()));

    RefPtr requestingProcess = requestingProcessIdentifier ? WebProcessProxy::processForIdentifier(*requestingProcessIdentifier) : nullptr;
    auto lockdownMode = requestingProcess ? requestingProcess->lockdownMode() : (lockdownModeEnabledBySystem() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled);
    auto enhancedSecurity = requestingProcess ? requestingProcess->enhancedSecurity() : WebProcessProxy::EnhancedSecurity::Disabled;
    Ref processPool = requestingProcess ? requestingProcess->processPool() : processPools()[0].get();

    RefPtr<WebProcessProxy> remoteWorkerProcessProxy;

    auto useProcessForRemoteWorkers = [&](WebProcessProxy& process) {
        remoteWorkerProcessProxy = process;
        process.enableRemoteWorkers(workerType, processPool->userContentControllerForRemoteWorkers());
        if (process.isInProcessCache()) {
            processPool->webProcessCache().removeProcess(process, WebProcessCache::ShouldShutDownProcess::No);
            ASSERT(!process.isInProcessCache());
        }
    };

    if (serviceWorkerPageIdentifier) {
        ASSERT(workerType == RemoteWorkerType::ServiceWorker);
        // This is a service worker for a service worker page so we need to make sure we use use the page's WebProcess for the service worker.
        if (RefPtr process = WebProcessProxy::processForIdentifier(serviceWorkerPageIdentifier->processIdentifier()))
            useProcessForRemoteWorkers(*process);
    }

    // Prioritize the requesting WebProcess for running the service worker.
    if (!remoteWorkerProcessProxy && !s_useSeparateServiceWorkerProcess && requestingProcess && requestingProcess->state() != WebProcessProxy::State::Terminated) {
        if (requestingProcess->websiteDataStore() == websiteDataStore && requestingProcess->site() == site)
            useProcessForRemoteWorkers(*requestingProcess);
    }

    if (!remoteWorkerProcessProxy && !s_useSeparateServiceWorkerProcess) {
        for (Ref process : processPool->m_processes) {
            if (process->isPrewarmed() || process->isDummyProcessProxy())
                continue;
            if (process->websiteDataStore() != websiteDataStore)
                continue;
            if (process->site() != site)
                continue;
            if (process->lockdownMode() != lockdownMode)
                continue;

            useProcessForRemoteWorkers(process);

            WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "establishRemoteWorkerContextConnectionToNetworkProcess reusing an existing web process (process=%p, workerType=%" PUBLIC_LOG_STRING ", PID=%d)", remoteWorkerProcessProxy.get(), workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", remoteWorkerProcessProxy->processID());
            break;
        }
    }

    struct WebPreferencesStoreRef {
        RefPtr<WebPreferences> webPreferences;
        RefPtr<WebProcessPool> processPool;
        RefPtr<WebPageGroup> webPageGroup;
        const WebPreferencesStore& store;
    } preferencesStore = [&] {
        if (workerType == RemoteWorkerType::ServiceWorker && websiteDataStore->serviceWorkerOverridePreferences())
            return WebPreferencesStoreRef { websiteDataStore->serviceWorkerOverridePreferences(), nullptr, nullptr, websiteDataStore->serviceWorkerOverridePreferences()->store() };

        if (processPool->m_remoteWorkerPreferences)
            return WebPreferencesStoreRef { nullptr, processPool.copyRef(), nullptr, *processPool->m_remoteWorkerPreferences };

        Ref pageGroup = processPool->m_defaultPageGroup;
        return WebPreferencesStoreRef { nullptr, nullptr, pageGroup.copyRef(), pageGroup->preferences().store() };
    }();

    if (!remoteWorkerProcessProxy) {
        Ref newProcessProxy = WebProcessProxy::createForRemoteWorkers(workerType, processPool, Site { site }, *websiteDataStore, lockdownMode, enhancedSecurity);
        remoteWorkerProcessProxy = newProcessProxy.copyRef();

        WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "establishRemoteWorkerContextConnectionToNetworkProcess creating a new service worker process (process=%p, workerType=%" PUBLIC_LOG_STRING ", PID=%d)", remoteWorkerProcessProxy.get(), workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", remoteWorkerProcessProxy->processID());

        processPool->initializeNewWebProcess(newProcessProxy, websiteDataStore.get());
        processPool->m_processes.append(WTFMove(newProcessProxy));
    }

    auto aggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler), remoteProcessIdentifier = remoteWorkerProcessProxy->coreProcessIdentifier()]() mutable {
        completionHandler(remoteProcessIdentifier);
    });
    websiteDataStore->protectedNetworkProcess()->addAllowedFirstPartyForCookies(*remoteWorkerProcessProxy, site.domain(), LoadedWebArchive::No, [aggregator] { });
    remoteWorkerProcessProxy->establishRemoteWorkerContext(workerType, preferencesStore.store, site, serviceWorkerPageIdentifier, [aggregator] { });

    if (!processPool->m_remoteWorkerUserAgent.isNull())
        remoteWorkerProcessProxy->setRemoteWorkerUserAgent(processPool->m_remoteWorkerUserAgent);

    for (Ref process : processPool->m_processes) {
        if (process->shouldRegisterServiceWorkerClients(site, sessionID))
            process->registerServiceWorkerClients([aggregator] { });
    }
}

void WebProcessPool::addRemoteWorkerProcess(WebProcessProxy& process)
{
    ASSERT(&process.processPool() == this);
    ASSERT(process.isRunningWorkers());
    remoteWorkerProcesses().add(process);
}

void WebProcessPool::removeRemoteWorkerProcess(WebProcessProxy& process)
{
    ASSERT(!process.isRunningWorkers());
    ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == &process; }));
    ASSERT(remoteWorkerProcesses().contains(process));
    remoteWorkerProcesses().remove(process);
}

void WebProcessPool::windowServerConnectionStateChanged()
{
    for (Ref process : m_processes)
        process->windowServerConnectionStateChanged();
}

void (*s_invalidMessageCallback)(WKStringRef messageName);

void WebProcessPool::setInvalidMessageCallback(void (*invalidMessageCallback)(WKStringRef messageName))
{
    s_invalidMessageCallback = invalidMessageCallback;
}

void WebProcessPool::didReceiveInvalidMessage(IPC::MessageName messageName)
{
    if (!s_invalidMessageCallback)
        return;

    s_invalidMessageCallback(toAPI(API::String::create(String::fromLatin1(description(messageName))).ptr()));
}

void WebProcessPool::resolvePathsForSandboxExtensions()
{
    m_resolvedPaths.injectedBundlePath = resolvePathForSandboxExtension(injectedBundlePath());

    m_resolvedPaths.additionalWebProcessSandboxExtensionPaths = WTF::map(m_configuration->additionalReadAccessAllowedPaths(), [](auto& path) {
        return resolvePathForSandboxExtension(path);
    });

    platformResolvePathsForSandboxExtensions();
}

Ref<WebProcessProxy> WebProcessPool::createNewWebProcess(WebsiteDataStore* websiteDataStore, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, WebProcessProxy::IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode)
{
    auto processProxy = WebProcessProxy::create(*this, websiteDataStore, lockdownMode, enhancedSecurity, isPrewarmed, crossOriginMode);
    initializeNewWebProcess(processProxy, websiteDataStore, isPrewarmed);
    m_processes.append(processProxy.copyRef());

    return processProxy;
}

RefPtr<WebProcessProxy> WebProcessPool::tryTakePrewarmedProcess(WebsiteDataStore& websiteDataStore, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, const API::PageConfiguration& pageConfiguration)
{
    RefPtr<WebProcessProxy> prewarmedProcess;

    for (Ref process : m_prewarmedProcesses) {
        if (process->lockdownMode() != lockdownMode || process->enhancedSecurity() != enhancedSecurity)
            continue;

        if (process->wasTerminated()) {
            // There is sometimes a delay until we get notified that a prewarmed process has been terminated (e.g. after resuming
            // from suspension) so make sure the process is still running here before deciding to use it.
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "tryTakePrewarmedProcess: Not using prewarmed process because it has been terminated (process=%p, PID=%d)", process.ptr(), process->processID());
            continue;
        }

        prewarmedProcess = process.ptr();
        break;
    }

    if (!prewarmedProcess) {
        if (configuration().isAutomaticProcessWarmingEnabled())
            WTFEmitSignpost(this, ProcessPrewarming, "Not using prewarmed process");
        return nullptr;
    }

    WTFEmitSignpost(this, ProcessPrewarming, "Using prewarmed process with pid %d", prewarmedProcess->processID());

    bool didRemove = m_prewarmedProcesses.remove(*prewarmedProcess);
    ASSERT_UNUSED(didRemove, didRemove);

#if PLATFORM(GTK) || PLATFORM(WPE)
    // In platforms using Bubblewrap for sandboxing, prewarmed process is launched using the WebProcessPool primary WebsiteDataStore,
    // so we don't use it in case of using a different WebsiteDataStore.
    if (m_sandboxEnabled)
        return nullptr;
#endif

    ASSERT(prewarmedProcess->isPrewarmed());
    prewarmedProcess->setWebsiteDataStore(websiteDataStore);
    prewarmedProcess->markIsNoLongerInPrewarmedPool();

    return prewarmedProcess;
}

#if !PLATFORM(MAC)
void WebProcessPool::registerDisplayConfigurationCallback()
{
}
#endif // !PLATFORM(MAC)

#if !PLATFORM(MAC) && !PLATFORM(IOS) && !PLATFORM(VISION)
void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
}
#endif

WebProcessDataStoreParameters WebProcessPool::webProcessDataStoreParameters(WebProcessProxy& process, WebsiteDataStore& websiteDataStore)
{
    auto& resolvedDirectories = websiteDataStore.resolvedDirectories();
    String mediaCacheDirectory = resolvedDirectories.mediaCacheDirectory;
#if !ENABLE(GPU_PROCESS)
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!mediaCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(mediaCacheDirectory, SandboxExtension::Type::ReadWrite))
            mediaCacheDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif

    String mediaKeyStorageDirectory = resolvedDirectories.mediaKeysStorageDirectory;
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    if (!mediaKeyStorageDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(mediaKeyStorageDirectory, SandboxExtension::Type::ReadWrite))
            mediaKeyStorageDirectoryExtensionHandle = WTFMove(*handle);
    }
    auto mediaKeyStorageSalt = websiteDataStore.mediaKeysStorageSalt();

    String javaScriptConfigurationDirectory;
    if (!m_javaScriptConfigurationDirectory.isEmpty())
        javaScriptConfigurationDirectory = resolvePathForSandboxExtension(m_javaScriptConfigurationDirectory);
    else if (javaScriptConfigurationFileEnabled())
        javaScriptConfigurationDirectory = resolvedDirectories.javaScriptConfigurationDirectory;

    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
    if (!javaScriptConfigurationDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(javaScriptConfigurationDirectory, SandboxExtension::Type::ReadWrite))
            javaScriptConfigurationDirectoryExtensionHandle = WTFMove(*handle);
    }

#if ENABLE(ARKIT_INLINE_PREVIEW) && !PLATFORM(IOS_FAMILY)
    auto modelElementCacheDirectory = resolvedDirectories.modelElementCacheDirectory;
    SandboxExtension::Handle modelElementCacheDirectoryExtensionHandle;
    if (!modelElementCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(modelElementCacheDirectory, SandboxExtension::Type::ReadWrite))
            modelElementCacheDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
    if (auto directory = websiteDataStore.resolvedContainerTemporaryDirectory(); !directory.isEmpty()) {
        if (m_cachedWebContentTempDirectory.isEmpty())
            m_cachedWebContentTempDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(FileSystem::pathByAppendingComponent(directory, webContentServiceName));
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(m_cachedWebContentTempDirectory, SandboxExtension::Type::ReadWrite))
            containerTemporaryDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif

    return WebProcessDataStoreParameters {
        websiteDataStore.sessionID(),
        WTFMove(mediaCacheDirectory),
#if !ENABLE(GPU_PROCESS)
        WTFMove(mediaCacheDirectoryExtensionHandle),
#endif
        WTFMove(mediaKeyStorageDirectory),
        WTFMove(mediaKeyStorageDirectoryExtensionHandle),
        WTFMove(mediaKeyStorageSalt),
        WTFMove(javaScriptConfigurationDirectory),
        WTFMove(javaScriptConfigurationDirectoryExtensionHandle),
        websiteDataStore.thirdPartyCookieBlockingMode(),
        m_domainsWithUserInteraction,
        m_domainsWithCrossPageStorageAccessQuirk,
#if ENABLE(ARKIT_INLINE_PREVIEW) && !PLATFORM(IOS_FAMILY)
        WTFMove(modelElementCacheDirectory),
        WTFMove(modelElementCacheDirectoryExtensionHandle),
#endif
#if PLATFORM(IOS_FAMILY)
        WTFMove(containerTemporaryDirectoryExtensionHandle),
#endif
        websiteDataStore.trackingPreventionEnabled()
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
        , websiteDataStore.isOptInCookiePartitioningEnabled()
#endif
    };
}

void WebProcessPool::initializeNewWebProcess(WebProcessProxy& process, WebsiteDataStore* websiteDataStore, WebProcessProxy::IsPrewarmed isPrewarmed)
{
    WebProcessCreationParameters parameters;
    parameters.auxiliaryProcessParameters = AuxiliaryProcessProxy::auxiliaryProcessParameters();

    parameters.injectedBundlePath = m_resolvedPaths.injectedBundlePath;
    if (!parameters.injectedBundlePath.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.injectedBundlePath, SandboxExtension::Type::ReadOnly))
            parameters.injectedBundlePathExtensionHandle = WTFMove(*handle);
    }

    parameters.additionalSandboxExtensionHandles = WTF::compactMap(m_resolvedPaths.additionalWebProcessSandboxExtensionPaths, [](auto& path) {
        return SandboxExtension::createHandleWithoutResolvingPath(path, SandboxExtension::Type::ReadOnly);
    });

#if PLATFORM(IOS_FAMILY)
    setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    parameters.cacheModel = LegacyGlobalSettings::singleton().cacheModel();
    parameters.overrideLanguages = overrideLanguages();
    LOG_WITH_STREAM(Language, stream << "WebProcessPool is initializing a new web process with overrideLanguages: " << parameters.overrideLanguages);

    parameters.urlSchemesRegisteredAsSecure = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsSecure());
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsBypassingContentSecurityPolicy());
    parameters.urlSchemesRegisteredAsLocal = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsLocal());
#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsNoAccess());
#endif

    parameters.urlSchemesRegisteredAsEmptyDocument = copyToVector(m_schemesToRegisterAsEmptyDocument);
    parameters.urlSchemesForWhichDomainRelaxationIsForbidden = copyToVector(m_schemesToSetDomainRelaxationForbiddenFor);
    parameters.urlSchemesRegisteredAsDisplayIsolated = copyToVector(m_schemesToRegisterAsDisplayIsolated);
    parameters.urlSchemesRegisteredAsCORSEnabled = copyToVector(m_schemesToRegisterAsCORSEnabled);
    parameters.urlSchemesRegisteredAsAlwaysRevalidated = copyToVector(m_schemesToRegisterAsAlwaysRevalidated);
    parameters.urlSchemesRegisteredAsCachePartitioned = copyToVector(m_schemesToRegisterAsCachePartitioned);
    parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest = copyToVector(m_schemesToRegisterAsCanDisplayOnlyIfCanRequest);

#if ENABLE(WK_WEB_EXTENSIONS)
    parameters.urlSchemesRegisteredAsWebExtensions = copyToVector(WebExtensionMatchPattern::extensionSchemes());
#endif

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.disableFontSubpixelAntialiasingForTesting = m_disableFontSubpixelAntialiasingForTesting;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

    parameters.backForwardCacheCapacity = backForwardCache().capacity();

#if ENABLE(NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    if (websiteDataStore)
        parameters.notificationPermissions = websiteDataStore->client().notificationPermissions();
    if (parameters.notificationPermissions.isEmpty())
        parameters.notificationPermissions = protectedSupplement<WebNotificationManagerProxy>()->notificationPermissions();
#endif

    parameters.memoryCacheDisabled = m_memoryCacheDisabled;
    parameters.attrStyleEnabled = m_configuration->attrStyleEnabled();
    parameters.shouldThrowExceptionForGlobalConstantRedeclaration = m_configuration->shouldThrowExceptionForGlobalConstantRedeclaration();
    parameters.crossOriginMode = process.crossOriginMode();

#if ENABLE(SERVICE_CONTROLS)
    auto& serviceController = ServicesController::singleton();
    parameters.hasImageServices = serviceController.hasImageServices();
    parameters.hasSelectionServices = serviceController.hasSelectionServices();
    parameters.hasRichContentServices = serviceController.hasRichContentServices();
    serviceController.refreshExistingServices();
#endif

#if OS(LINUX)
    parameters.shouldEnableMemoryPressureReliefLogging = true;
#endif

    parameters.presentingApplicationPID = m_configuration->presentingApplicationPID();

    parameters.timeZoneOverride = m_configuration->timeZoneOverride();

    parameters.memoryFootprintPollIntervalForTesting = m_configuration->memoryFootprintPollIntervalForTesting();

    parameters.memoryFootprintNotificationThresholds = m_configuration->memoryFootprintNotificationThresholds();

    // Add any platform specific parameters
    platformInitializeWebProcess(process, parameters);

    RefPtr injectedBundleInitializationUserData = m_injectedBundleClient->getInjectedBundleInitializationUserData(*this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process.transformObjectsToHandles(injectedBundleInitializationUserData.get()));
    
    if (websiteDataStore)
        parameters.websiteDataStoreParameters = webProcessDataStoreParameters(process, *websiteDataStore);

    process.initializeWebProcess(WTFMove(parameters));

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    setMediaAccessibilityPreferences(process);
#endif

    if (m_automationSession)
        process.send(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()), 0);

    ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

    if (isPrewarmed == WebProcessProxy::IsPrewarmed::Yes)
        m_prewarmedProcesses.add(process);

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    process.send(Messages::WebProcess::BacklightLevelDidChange(displayBrightness()), 0);
#endif

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
    process.sendAudioComponentRegistrations();
#endif

#if PLATFORM(IOS) && ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
    if (WTF::CocoaApplication::isAppleBooks())
        registerAssetFonts(process);
#endif

    registerDisplayConfigurationCallback();
    registerHighDynamicRangeChangeCallback();
}

void WebProcessPool::prewarmProcess()
{
    if (m_prewarmedProcesses.computeSize() >= prewarmedProcessCountLimit())
        return;

    if (WebProcessProxy::hasReachedProcessCountLimit()) {
        WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "prewarmProcess: Not prewarming a WebProcess due to reaching process count limit");
        return;
    }

    WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "prewarmProcess: Prewarming a WebProcess for performance");

    auto lockdownMode = lockdownModeEnabledBySystem() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;
    auto enhancedSecurity = WebProcessProxy::EnhancedSecurity::Disabled;
    createNewWebProcess(nullptr, lockdownMode, enhancedSecurity, WebProcessProxy::IsPrewarmed::Yes);
}

void WebProcessPool::enableProcessTermination()
{
    WEBPROCESSPOOL_RELEASE_LOG(Process, "enableProcessTermination:");
    m_processTerminationEnabled = true;
    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (Ref process : processes) {
        if (shouldTerminate(process))
            process->terminate();
    }
}

void WebProcessPool::disableProcessTermination()
{
    if (!m_processTerminationEnabled)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(Process, "disableProcessTermination:");
    m_processTerminationEnabled = false;
}

bool WebProcessPool::shouldTerminate(WebProcessProxy& process)
{
    ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == &process; }));

    if (!m_processTerminationEnabled || m_configuration->alwaysKeepAndReuseSwappedProcesses())
        return false;

    return true;
}

void WebProcessPool::processDidFinishLaunching(WebProcessProxy& process)
{
    ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == &process; }));

    if (!m_visitedLinksPopulated) {
        populateVisitedLinks();
        m_visitedLinksPopulated = true;
    }

    // Sometimes the memorySampler gets initialized after process initialization has happened but before the process has finished launching
    // so check if it needs to be started here
    if (m_memorySamplerEnabled) {
        SandboxExtension::Handle sampleLogSandboxHandle;        
        WallTime now = WallTime::now();
        auto sampleLogFilePath = makeString("WebProcess"_s, now.secondsSinceEpoch().secondsAs<uint64_t>(), "pid"_s, process.processID());
        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite)) {
            sampleLogSandboxHandle = WTFMove(handleAndFilePath->first);
            sampleLogFilePath = WTFMove(handleAndFilePath->second);
        }
        
        process.send(Messages::WebProcess::StartMemorySampler(WTFMove(sampleLogSandboxHandle), sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    if (m_configuration->fullySynchronousModeIsAllowedForTesting())
        process.protectedConnection()->allowFullySynchronousModeForTesting();

    if (m_configuration->ignoreSynchronousMessagingTimeoutsForTesting())
        process.protectedConnection()->ignoreTimeoutsForTesting();

#if ENABLE(EXTENSION_CAPABILITIES)
    for (auto& page : process.pages()) {
        if (RefPtr mediaCapability = page->mediaCapability()) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessCapabilities, "processDidFinishLaunching[envID=%" PUBLIC_LOG_STRING "]: updating media capability", mediaCapability->environmentIdentifier().utf8().data());
            page->updateMediaCapability();
        }
    }
#endif

    // Continue spawning prewarmed processes until we hit the prewarmed process limit.
    if (process.isPrewarmed() && m_prewarmedProcesses.computeSize() < prewarmedProcessCountLimit()) {
#if PLATFORM(COCOA)
        static Seconds prewarmedLaunchInterval = []() {
            auto value = CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessPoolPrewarmingIntervalMillis"), kCFPreferencesCurrentApplication, nullptr);
            return value ? Seconds::fromMilliseconds(value) : 0_s;
        }();
#else
        static Seconds prewarmedLaunchInterval = 0_s;
#endif
        WorkQueue::mainSingleton().dispatchAfter(prewarmedLaunchInterval, [weakThis = WeakPtr { *this }]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->didReachGoodTimeToPrewarm();
        });
    }
}

void WebProcessPool::disconnectProcess(WebProcessProxy& process)
{
    ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == &process; }));

    if (process.isPrewarmed()) {
        bool didRemove = m_prewarmedProcesses.remove(process);
        ASSERT_UNUSED(didRemove, didRemove);
    } else if (process.isDummyProcessProxy()) {
        auto removedProcess = m_dummyProcessProxies.take(process.sessionID());
        ASSERT_UNUSED(removedProcess, removedProcess == &process);
    }

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    Ref protectedProcess { process };

#if ENABLE(MODEL_PROCESS) && HAVE(TASK_IDENTITY_TOKEN)
    process.unregisterMemoryAttributionIDIfNeeded();
#endif

    m_backForwardCache->removeEntriesForProcess(process);

    if (process.isRunningWorkers())
        process.disableRemoteWorkers({ RemoteWorkerType::ServiceWorker, RemoteWorkerType::SharedWorker });
    ASSERT(!remoteWorkerProcesses().contains(process));

    protectedSupplement<WebGeolocationManagerProxy>()->webProcessIsGoingAway(process);

    m_processes.removeFirstMatching([&](auto& item) { return item.ptr() == &process; });

#if ENABLE(GAMEPAD)
    if (m_processesUsingGamepads.contains(process))
        processStoppedUsingGamepads(process);
#endif

#if ENABLE(MODEL_PROCESS)
    if (m_processesWithModelPlayers.contains(process)) {
        WEBPROCESSPOOL_RELEASE_LOG(Process, "disconnectProcess stop tracking WebProcess (process=%p, PID=%i) with connection to Model Process", &process, process.processID());
        m_processesWithModelPlayers.remove(process);
    }
#endif

    removeProcessFromOriginCacheSet(process);

#if ENABLE(EXTENSION_CAPABILITIES)
    ExtensionCapabilityGranter::invalidateGrants(moveToVector(std::exchange(process.extensionCapabilityGrants(), { }).values()));
#endif
}

Ref<WebProcessProxy> WebProcessPool::processForSite(WebsiteDataStore& websiteDataStore, IsSharedProcess isSharedProcess, const std::optional<Site>& site, const std::optional<Site>& mainFrameSite, const HashSet<RegistrableDomain>& isolatedDomains, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, const API::PageConfiguration& pageConfiguration, ProcessSwapDisposition processSwapDisposition)
{
    if (isSharedProcess == IsSharedProcess::Yes) {
        ASSERT(mainFrameSite);
        if (RefPtr process = webProcessCache().takeSharedProcess(*mainFrameSite, websiteDataStore, lockdownMode, enhancedSecurity, pageConfiguration)) {
            if (process->sharedProcessDomains().intersectionWith(isolatedDomains).isEmpty()) {
                ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
                WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForSite: Using shared WebProcess from WebProcess cache (process=%p, PID=%i)", process.get(), process->processID());
                ASSERT(!process->isInProcessCache());
                return process.releaseNonNull();
            } else
                WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForSite: discarding the shared process since it contains a domain that needs to be isolated (process=%p, PID=%i)", process.get(), process->processID());
        }
    } else if (site && !site->isEmpty() && processSwapDisposition != ProcessSwapDisposition::COOP) {
        // We don't reuse cached processess because the process cache is per site, whereas COOP swaps are based on origin.
        if (RefPtr process = webProcessCache().takeProcess(*site, websiteDataStore, lockdownMode, enhancedSecurity, pageConfiguration)) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForSite: Using WebProcess from WebProcess cache (process=%p, PID=%i)", process.get(), process->processID());
            ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
            return process.releaseNonNull();
        }

        // RefPtr if we have a suspended page for the given registrable domain and use its process if we do, for performance reasons.
        if (RefPtr process = SuspendedPageProxy::findReusableSuspendedPageProcess(*this, site->domain(), websiteDataStore, lockdownMode, enhancedSecurity, pageConfiguration)) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForSite: Using WebProcess from a SuspendedPage (process=%p, PID=%i)", process.get(), process->processID());
            ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
            return process.releaseNonNull();
        }
    }

    if (RefPtr process = tryTakePrewarmedProcess(websiteDataStore, lockdownMode, enhancedSecurity, pageConfiguration)) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForSite: Using prewarmed process (process=%p, PID=%i)", process.get(), process->processID());
        if (site && !site->isEmpty())
            tryPrewarmWithDomainInformation(*process, site->domain());
        ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
        return process.releaseNonNull();
    }

    if (usesSingleWebProcess()) {
#if PLATFORM(COCOA)
        bool mustMatchDataStore = WebKit::WebsiteDataStore::defaultDataStoreExists() && &websiteDataStore != WebKit::WebsiteDataStore::defaultDataStore().ptr();
#else
        bool mustMatchDataStore = false;
#endif

        for (Ref process : m_processes) {
            if (process->isPrewarmed() || process->isDummyProcessProxy())
                continue;
            if (process->isRunningServiceWorkers())
                continue;
            if (mustMatchDataStore && process->websiteDataStore() != &websiteDataStore)
                continue;
            return process;
        }
    }
    return createNewWebProcess(&websiteDataStore, lockdownMode, enhancedSecurity);
}

Ref<WebUserContentControllerProxy> WebProcessPool::userContentControllerForRemoteWorkers()
{
    if (!m_userContentControllerForRemoteWorkers)
        m_userContentControllerForRemoteWorkers = WebUserContentControllerProxy::create();

    return *m_userContentControllerForRemoteWorkers;
}

Ref<WebPageProxy> WebProcessPool::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    if (!pageConfiguration->pageGroup())
        pageConfiguration->setPageGroup(m_defaultPageGroup.copyRef());

    RefPtr<WebProcessProxy> process;
    auto lockdownMode = pageConfiguration->lockdownModeEnabled() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;
    auto enhancedSecurity = (pageConfiguration->protectedPreferences()->forceEnhancedSecurity() || pageConfiguration->enhancedSecurityEnabled()) ? WebProcessProxy::EnhancedSecurity::Enabled : WebProcessProxy::EnhancedSecurity::Disabled;

    RefPtr relatedPage = pageConfiguration->relatedPage();

    if (auto& openerInfo = pageConfiguration->openerInfo(); openerInfo && Ref { pageConfiguration->preferences() }->siteIsolationEnabled())
        process = openerInfo->process.ptr();
    else if (relatedPage && !relatedPage->isClosed() && relatedPage->hasSameGPUAndNetworkProcessPreferencesAs(pageConfiguration)) {
        // Sharing processes, e.g. when creating the page via window.open().
        process = relatedPage->ensureRunningProcess();
        // We do not support several WebsiteDataStores sharing a single process.
        ASSERT(process->isDummyProcessProxy() || &pageConfiguration->websiteDataStore() == process->websiteDataStore());
        ASSERT(&pageConfiguration->relatedPage()->websiteDataStore() == &pageConfiguration->websiteDataStore());
    } else if (pageConfiguration->delaysWebProcessLaunchUntilFirstLoad()) {
        WEBPROCESSPOOL_RELEASE_LOG(Process, "createWebPage: delaying WebProcess launch until first load");
        // In the common case, we delay process launch until something is actually loaded in the page.
        process = dummyProcessProxy(pageConfiguration->websiteDataStore().sessionID());
        if (!process) {
            process = WebProcessProxy::create(*this, pageConfiguration->protectedWebsiteDataStore().ptr(), lockdownMode, enhancedSecurity, WebProcessProxy::IsPrewarmed::No, CrossOriginMode::Shared, WebProcessProxy::ShouldLaunchProcess::No);
            m_dummyProcessProxies.add(pageConfiguration->websiteDataStore().sessionID(), *process);
            m_processes.append(*process);
        }
    } else {
        WEBPROCESSPOOL_RELEASE_LOG(Process, "createWebPage: Not delaying WebProcess launch");
        process = processForSite(pageConfiguration->protectedWebsiteDataStore(), IsSharedProcess::No, std::nullopt, std::nullopt, { }, lockdownMode, enhancedSecurity, pageConfiguration, WebCore::ProcessSwapDisposition::None);
    }

    Ref userContentController = pageConfiguration->userContentController();
    
    ASSERT(process);
    
    process->setAllowTestOnlyIPC(pageConfiguration->allowTestOnlyIPC());

    auto page = process->createWebPage(pageClient, WTFMove(pageConfiguration));
    Ref pagePreference = page->preferences();

    if (!m_remoteWorkerPreferences) {
        m_remoteWorkerPreferences = page->preferencesStore();
        for (Ref workerProcess : remoteWorkerProcesses())
            workerProcess->updateRemoteWorkerPreferencesStore(*m_remoteWorkerPreferences);
    }
    m_userContentControllerForRemoteWorkers = userContentController.ptr();

    bool enableProcessSwapOnCrossSiteNavigation = pagePreference->processSwapOnCrossSiteNavigationEnabled();
    Ref configuration = m_configuration;
    bool wasProcessSwappingOnNavigationEnabled = configuration->processSwapsOnNavigation();
    configuration->setProcessSwapsOnNavigationFromExperimentalFeatures(enableProcessSwapOnCrossSiteNavigation);
    if (wasProcessSwappingOnNavigationEnabled != configuration->processSwapsOnNavigation())
        m_webProcessCache->updateCapacity(*this);

#if ENABLE(GPU_PROCESS)
    if (pagePreference->useGPUProcessForDOMRenderingEnabled())
        ensureGPUProcess();

    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated()) {
        gpuProcess->updatePreferences(*process);
        gpuProcess->updateScreenPropertiesIfNeeded();
    }
#endif

#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
    NetworkProcessProxy::ensureDefaultNetworkProcess();
#endif

    return page;
}

void WebProcessPool::updateRemoteWorkerUserAgent(const String& userAgent)
{
    if (m_remoteWorkerUserAgent == userAgent)
        return;
    m_remoteWorkerUserAgent = userAgent;
    for (Ref workerProcess : remoteWorkerProcesses())
        workerProcess->setRemoteWorkerUserAgent(m_remoteWorkerUserAgent);
}

void WebProcessPool::pageBeginUsingWebsiteDataStore(WebPageProxy& page, WebsiteDataStore& dataStore)
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    auto result = m_sessionToPageIDsMap.add(dataStore.sessionID(), HashSet<WebPageProxyIdentifier>()).iterator->value.add(page.identifier());
    ASSERT_UNUSED(result, result.isNewEntry);
    dataStore.addPage(page);
}

void WebProcessPool::pageEndUsingWebsiteDataStore(WebPageProxy& page, WebsiteDataStore& dataStore)
{
    RELEASE_ASSERT(RunLoop::isMain());
    auto sessionID = dataStore.sessionID();
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    auto iterator = m_sessionToPageIDsMap.find(sessionID);
    RELEASE_ASSERT(iterator != m_sessionToPageIDsMap.end());

    auto pageID = page.identifier();
    auto takenPageID = iterator->value.take(pageID);
    ASSERT_UNUSED(takenPageID, takenPageID == pageID);

    if (iterator->value.isEmpty()) {
        m_sessionToPageIDsMap.remove(iterator);

        if (sessionID.isEphemeral())
            m_webProcessCache->clearAllProcessesForSession(sessionID);
    }
    dataStore.removePage(page);
}

bool WebProcessPool::hasPagesUsingWebsiteDataStore(WebsiteDataStore& dataStore) const
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    return m_sessionToPageIDsMap.contains(dataStore.sessionID());
}

Ref<DownloadProxy> WebProcessPool::download(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const ResourceRequest& request, const std::optional<FrameInfoData>& frameInfo, const String& suggestedFilename)
{
    Ref downloadProxy = createDownloadProxy(dataStore, request, initiatingPage, frameInfo);
    dataStore.download(downloadProxy, suggestedFilename);
    return downloadProxy;
}

Ref<DownloadProxy> WebProcessPool::resumeDownload(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path, CallDownloadDidStart callDownloadDidStart)
{
    Ref downloadProxy = createDownloadProxy(dataStore, ResourceRequest(), initiatingPage, { });
    dataStore.resumeDownload(downloadProxy, resumeData, path, callDownloadDidStart);
    return downloadProxy;
}

void WebProcessPool::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    for (Ref process : m_processes) {
        // FIXME: Return early if the message body contains any references to WKPageRefs/WKFrameRefs etc. since they're local to a process.
        process->send(Messages::WebProcess::HandleInjectedBundleMessage(messageName, UserData(process->transformObjectsToHandles(messageBody).get())), 0);
    }
}

static void loadRestrictedOpenerTypeDataIfNeeded()
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RestrictedOpenerDomainsController::singleton();
#endif
}

void WebProcessPool::didReachGoodTimeToPrewarm()
{
    loadRestrictedOpenerTypeDataIfNeeded();

    if (!configuration().isAutomaticProcessWarmingEnabled() || !configuration().processSwapsOnNavigation() || usesSingleWebProcess())
        return;

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "prewarmProcess: Not prewarming a WebProcess due to memory pressure");
        return;
    }

    prewarmProcess();
}

void WebProcessPool::populateVisitedLinks()
{
    m_historyClient->populateVisitedLinks(*this);
}

WebProcessPool::Statistics& WebProcessPool::statistics()
{
    static Statistics statistics = Statistics();

    return statistics;
}

void WebProcessPool::handleMemoryPressureWarning(Critical)
{
    WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "handleMemoryPressureWarning:");

    // Clear back/forward cache first as processes removed from the back/forward cache will likely
    // be added to the WebProcess cache.
    m_backForwardCache->clear();
    m_webProcessCache->clear();

    for (WeakPtr maybePrewarmedProcess : copyToVector(m_prewarmedProcesses)) {
        if (RefPtr prewarmedProcess = maybePrewarmedProcess.get())
            prewarmedProcess->shutDown();
    }

    ASSERT(m_prewarmedProcesses.isEmptyIgnoringNullReferences());

    for (Ref process : m_processes)
        process->clearSandboxExtensions();
}

HashSet<ProcessID> WebProcessPool::prewarmedProcessIdentifiers()
{
    HashSet<ProcessID> pids;
    for (Ref process : m_prewarmedProcesses) {
        if (auto pid = process->processID())
            pids.add(pid);
    }
    return pids;
}

void WebProcessPool::activePagesOriginsInWebProcessForTesting(ProcessID pid, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    for (Ref process : m_processes) {
        if (process->processID() == pid)
            return process->activePagesDomainsForTesting(WTFMove(completionHandler));
    }
    completionHandler({ });
}

void WebProcessPool::countWebPagesInAllProcessesForTesting(CompletionHandler<void(unsigned)>&& completionHandler)
{
    class ResultAggregator : public RefCounted<ResultAggregator> {
    public:
        static Ref<ResultAggregator> create(CompletionHandler<void(size_t)>&& completionHandler) { return adoptRef(*new ResultAggregator(WTFMove(completionHandler))); }
        void addWebPageCount(unsigned count) { m_count += count; }
        ~ResultAggregator()
        {
            m_completionHandler(m_count);
        }

    private:
        ResultAggregator(CompletionHandler<void(unsigned)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler)) { }

        CompletionHandler<void(unsigned)> m_completionHandler;
        unsigned m_count { 0 };
    };

    Ref aggregator = ResultAggregator::create(WTFMove(completionHandler));
    for (auto& process : m_processes) {
        process->sendWithAsyncReply(Messages::WebProcess::CountWebPagesForTesting(), [aggregator] (unsigned count) mutable {
            aggregator->addWebPageCount(count);
        });
    }
}

void WebProcessPool::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    m_alwaysUsesComplexTextCodePath = alwaysUseComplexText;
    sendToAllProcesses(Messages::WebProcess::SetAlwaysUsesComplexTextCodePath(alwaysUseComplexText));
}

void WebProcessPool::setDisableFontSubpixelAntialiasingForTesting(bool disable)
{
    m_disableFontSubpixelAntialiasingForTesting = disable;
    sendToAllProcesses(Messages::WebProcess::SetDisableFontSubpixelAntialiasingForTesting(disable));
}

void WebProcessPool::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme));
}

void WebProcessPool::registerURLSchemeAsSecure(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsSecure(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsSecure(urlScheme), 0);
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme), 0);
}

void WebProcessPool::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme)
{
    m_schemesToSetDomainRelaxationForbiddenFor.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::SetDomainRelaxationForbiddenForURLScheme(urlScheme));
}

void WebProcessPool::registerURLSchemeAsLocal(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsLocal(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsLocal(urlScheme));
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsLocal(urlScheme), 0);
}

#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsNoAccess(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsNoAccess(urlScheme), 0);
}
#endif

void WebProcessPool::registerURLSchemeAsDisplayIsolated(const String& urlScheme)
{
    m_schemesToRegisterAsDisplayIsolated.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsDisplayIsolated(urlScheme));
}

void WebProcessPool::registerURLSchemeAsCORSEnabled(const String& urlScheme)
{
    m_schemesToRegisterAsCORSEnabled.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCORSEnabled(urlScheme));
}

void WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    InitializeWebKit2();
    globalURLSchemesWithCustomProtocolHandlers().add(urlScheme);
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->registerSchemeForLegacyCustomProtocol(urlScheme);
}

void WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    InitializeWebKit2();
    globalURLSchemesWithCustomProtocolHandlers().remove(urlScheme);
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->unregisterSchemeForLegacyCustomProtocol(urlScheme);
}

void WebProcessPool::registerURLSchemeAsCachePartitioned(const String& urlScheme)
{
    m_schemesToRegisterAsCachePartitioned.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCachePartitioned(urlScheme));
}

void WebProcessPool::registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String& urlScheme)
{
    m_schemesToRegisterAsCanDisplayOnlyIfCanRequest.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCanDisplayOnlyIfCanRequest(urlScheme));
}

void WebProcessPool::updateBackForwardCacheCapacity()
{
    if (!m_configuration->usesBackForwardCache())
        return;

    unsigned dummy = 0;
    Seconds dummyInterval;
    unsigned backForwardCacheCapacity = 0;
    calculateMemoryCacheSizes(LegacyGlobalSettings::singleton().cacheModel(), dummy, dummy, dummy, dummyInterval, backForwardCacheCapacity);

    m_backForwardCache->setCapacity(*this, backForwardCacheCapacity);
}

void WebProcessPool::setCacheModel(CacheModel cacheModel)
{
    updateBackForwardCacheCapacity();

    sendToAllProcesses(Messages::WebProcess::SetCacheModel(cacheModel));

    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->send(Messages::NetworkProcess::SetCacheModel(cacheModel), 0);
}

void WebProcessPool::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    updateBackForwardCacheCapacity();

    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->sendSync(Messages::NetworkProcess::SetCacheModelSynchronouslyForTesting(cacheModel), 0);
}

void WebProcessPool::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

Ref<DownloadProxy> WebProcessPool::createDownloadProxy(WebsiteDataStore& dataStore, const ResourceRequest& request, WebPageProxy* originatingPage, const std::optional<FrameInfoData>& frameInfo)
{
    Ref client = m_legacyDownloadClient ? Ref<API::DownloadClient>(*m_legacyDownloadClient) : adoptRef(*new API::DownloadClient);
    return dataStore.createDownloadProxy(WTFMove(client), request, originatingPage, frameInfo);
}

void WebProcessPool::addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebProcessPool::addMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebProcessPool::removeMessageReceiver(IPC::ReceiverName messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void WebProcessPool::removeMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool WebProcessPool::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool WebProcessPool::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void WebProcessPool::setEnhancedAccessibility(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetEnhancedAccessibility(flag));
}
    
void WebProcessPool::startMemorySampler(const double interval)
{    
    // For new WebProcesses we will also want to start the Memory Sampler
    m_memorySamplerEnabled = true;
    m_memorySamplerInterval = interval;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::singleton()->start(interval);
#endif
    
    for (auto& process : m_processes) {
        if (!process->canSendMessage())
            continue;

        // For WebProcess
        SandboxExtension::Handle sampleLogSandboxHandle;
        WallTime now = WallTime::now();
        auto sampleLogFilePath = makeString("WebProcess"_s, now.secondsSinceEpoch().secondsAs<uint64_t>());
        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite)) {
            sampleLogSandboxHandle = WTFMove(handleAndFilePath->first);
            sampleLogFilePath = WTFMove(handleAndFilePath->second);
        }

        process->send(Messages::WebProcess::StartMemorySampler(WTFMove(sampleLogSandboxHandle), sampleLogFilePath, interval), 0);
    }
}

void WebProcessPool::stopMemorySampler()
{    
    // For WebProcess
    m_memorySamplerEnabled = false;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::singleton()->stop();
#endif

    sendToAllProcesses(Messages::WebProcess::StopMemorySampler());
}

void WebProcessPool::terminateAllWebContentProcesses(ProcessTerminationReason reason)
{
    WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "terminateAllWebContentProcesses");
    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (Ref process : processes)
        process->requestTermination(reason);
}

void WebProcessPool::terminateServiceWorkersForSession(PAL::SessionID sessionID)
{
    Ref protectedThis { *this };
    Vector<Ref<WebProcessProxy>> serviceWorkerProcesses;
    remoteWorkerProcesses().forEach([&](auto& process) {
        if (process.isRunningServiceWorkers() && process.sessionID() == sessionID)
            serviceWorkerProcesses.append(process);
    });
    for (Ref serviceWorkerProcess : serviceWorkerProcesses)
        serviceWorkerProcess->disableRemoteWorkers(RemoteWorkerType::ServiceWorker);
}

void WebProcessPool::terminateServiceWorkers()
{
    Ref protectedThis { *this };
    Vector<Ref<WebProcessProxy>> serviceWorkerProcesses;
    remoteWorkerProcesses().forEach([&](auto& process) {
        if (process.isRunningServiceWorkers())
            serviceWorkerProcesses.append(process);
    });
    for (Ref serviceWorkerProcess : serviceWorkerProcesses)
        serviceWorkerProcess->disableRemoteWorkers(RemoteWorkerType::ServiceWorker);
}

void WebProcessPool::updateAutomationCapabilities() const
{
#if ENABLE(REMOTE_INSPECTOR)
    Inspector::RemoteInspector::singleton().clientCapabilitiesDidChange();
#endif
}

void WebProcessPool::setAutomationSession(RefPtr<WebAutomationSession>&& automationSession)
{
    if (RefPtr previousSession = m_automationSession)
        previousSession->setProcessPool(nullptr);
    
    m_automationSession = WTFMove(automationSession);

#if ENABLE(REMOTE_INSPECTOR)
    if (RefPtr automationSession = m_automationSession) {
        automationSession->init();
        automationSession->setProcessPool(this);

        sendToAllProcesses(Messages::WebProcess::EnsureAutomationSessionProxy(automationSession->sessionIdentifier()));
    } else
        sendToAllProcesses(Messages::WebProcess::DestroyAutomationSessionProxy());
#endif
}

void WebProcessPool::setHTTPPipeliningEnabled(bool enabled)
{
#if PLATFORM(COCOA)
    ResourceRequest::setHTTPPipeliningEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

bool WebProcessPool::httpPipeliningEnabled() const
{
#if PLATFORM(COCOA)
    return ResourceRequest::httpPipeliningEnabled();
#else
    return false;
#endif
}

RefPtr<WebProcessProxy> WebProcessPool::webProcessProxyFromConnection(const IPC::Connection& connection) const
{
    for (Ref process : m_processes) {
        if (process->hasConnection(connection))
            return process;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

std::optional<SharedPreferencesForWebProcess> WebProcessPool::sharedPreferencesForWebProcess(const IPC::Connection& connection) const
{
    return webProcessProxyFromConnection(connection)->sharedPreferencesForWebProcess();
}

void WebProcessPool::handleMessage(IPC::Connection& connection, const String& messageName, const WebKit::UserData& messageBody)
{
    RefPtr webProcessProxy = webProcessProxyFromConnection(connection);
    if (!webProcessProxy)
        return;
    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.protectedObject().get()).get());
}

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    RefPtr webProcessProxy = webProcessProxyFromConnection(connection);
    if (!webProcessProxy)
        return completionHandler({ });

    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.protectedObject().get()).get(), [webProcessProxy = Ref { *webProcessProxy }, completionHandler = WTFMove(completionHandler)] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(webProcessProxy->transformObjectsToHandles(returnData.get())));
    });
}

#if ENABLE(GAMEPAD)

void WebProcessPool::startedUsingGamepads(IPC::Connection& connection)
{
    RefPtr proxy = webProcessProxyFromConnection(connection);
    if (!proxy)
        return;

    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.isEmptyIgnoringNullReferences();
    if (!wereAnyProcessesUsingGamepads)
        UIGamepadProvider::singleton().processPoolStartedUsingGamepads(*this);

    // Add the process proxy after notifying the UIGamepadProvider so that any gamepad connected while starting
    // to monitor gamepads doesn't produce a GamepadConnected message, and it's only sent as initial gamepad.
    ASSERT(!m_processesUsingGamepads.contains(*proxy));
    m_processesUsingGamepads.add(*proxy);

    proxy->send(Messages::WebProcess::SetInitialGamepads(UIGamepadProvider::singleton().snapshotGamepads()), 0);
}

void WebProcessPool::stoppedUsingGamepads(IPC::Connection& connection, CompletionHandler<void()>&& completionHandler)
{
    CompletionHandlerCallingScope callCompletionHandlerOnExit(WTFMove(completionHandler));
    RefPtr proxy = webProcessProxyFromConnection(connection);
    if (!proxy)
        return;

    ASSERT(m_processesUsingGamepads.contains(*proxy));
    processStoppedUsingGamepads(*proxy);
}

void WebProcessPool::playGamepadEffect(unsigned gamepadIndex, const String& gamepadID, WebCore::GamepadHapticEffectType type, const WebCore::GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
    GamepadProvider::singleton().playEffect(gamepadIndex, gamepadID, type, parameters, WTFMove(completionHandler));
}

void WebProcessPool::stopGamepadEffects(unsigned gamepadIndex, const String& gamepadID, CompletionHandler<void()>&& completionHandler)
{
    GamepadProvider::singleton().stopEffects(gamepadIndex, gamepadID, WTFMove(completionHandler));
}

void WebProcessPool::processStoppedUsingGamepads(WebProcessProxy& process)
{
    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.isEmptyIgnoringNullReferences();

    ASSERT(m_processesUsingGamepads.contains(process));
    m_processesUsingGamepads.remove(process);

    if (wereAnyProcessesUsingGamepads && m_processesUsingGamepads.isEmptyIgnoringNullReferences())
        UIGamepadProvider::singleton().processPoolStoppedUsingGamepads(*this);
}

void WebProcessPool::gamepadConnected(const UIGamepad& gamepad, EventMakesGamepadsVisible eventVisibility)
{
    for (Ref process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::GamepadConnected(gamepad.gamepadData(), eventVisibility), 0);
}

void WebProcessPool::gamepadDisconnected(const UIGamepad& gamepad)
{
    for (Ref process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::GamepadDisconnected(gamepad.index()), 0);
}

#endif // ENABLE(GAMEPAD)

size_t WebProcessPool::numberOfConnectedGamepadsForTesting(GamepadType gamepadType)
{
#if ENABLE(GAMEPAD)
    switch (gamepadType) {
    case GamepadType::All:
        return UIGamepadProvider::singleton().numberOfConnectedGamepads();
#if PLATFORM(MAC)
    case GamepadType::HID:
        return HIDGamepadProvider::singleton().numberOfConnectedGamepads();
    case GamepadType::GameControllerFramework:
        return GameControllerGamepadProvider::singleton().numberOfConnectedGamepads();
#else
    case GamepadType::HID:
    case GamepadType::GameControllerFramework:
        return 0;
    default:
        return 0;
#endif
    }
#else
    return 0;
#endif
}

void WebProcessPool::setUsesOnlyHIDGamepadProviderForTesting(bool hidProviderOnly)
{
#if ENABLE(GAMEPAD) && HAVE(MULTIGAMEPADPROVIDER_SUPPORT)
    MultiGamepadProvider::singleton().setUsesOnlyHIDGamepadProvider(hidProviderOnly);
#endif
}

void WebProcessPool::setJavaScriptConfigurationFileEnabled(bool flag)
{
    m_javaScriptConfigurationFileEnabled = flag;
}

void WebProcessPool::garbageCollectJavaScriptObjects()
{
    sendToAllProcesses(Messages::WebProcess::GarbageCollectJavaScriptObjects());
}

void WebProcessPool::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetJavaScriptGarbageCollectorTimerEnabled(flag));
}

void WebProcessPool::addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions)
{
    UNUSED_PARAM(matchingDomain);
    UNUSED_PARAM(name);
    UNUSED_PARAM(mimeTypes);
    UNUSED_PARAM(extensions);
}

void WebProcessPool::clearSupportedPlugins()
{
}

void WebProcessPool::setMemoryCacheDisabled(bool disabled)
{
    m_memoryCacheDisabled = disabled;
    sendToAllProcesses(Messages::WebProcess::SetMemoryCacheDisabled(disabled));
}

void WebProcessPool::setFontAllowList(API::Array* array)
{
    m_fontAllowList.clear();
    if (array) {
        for (size_t i = 0; i < array->size(); ++i) {
            if (RefPtr font = array->at<API::String>(i))
                m_fontAllowList.append(font->string());
        }
    }
}

void WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit()
{
    // We're estimating an upper bound for a set of background timer fires for a page to be 200ms
    // (including all timer fires, all paging-in, and any resulting GC). To ensure this does not
    // result in more than 1% CPU load allow for one timer fire per 100x this duration.
    static int maximumTimerThrottlePerPageInMS = 200 * 100;

    int limitInMilliseconds = maximumTimerThrottlePerPageInMS * m_hiddenPageThrottlingAutoIncreasesCounter.value();
    sendToAllProcesses(Messages::WebProcess::SetHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds::fromMilliseconds(limitInMilliseconds)));
}

void WebProcessPool::reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState)
{
#if PLATFORM(MAC)
    m_perActivityStateCPUUsageSampler->reportWebContentCPUTime(cpuTime, static_cast<WebCore::ActivityStateForCPUSampling>(activityState));
#else
    UNUSED_PARAM(cpuTime);
    UNUSED_PARAM(activityState);
#endif
}

WeakHashSet<WebProcessProxy>& WebProcessPool::remoteWorkerProcesses()
{
    static NeverDestroyed<WeakHashSet<WebProcessProxy>> processes;
    return processes;
}

void WebProcessPool::updateProcessAssertions()
{
    if (RefPtr networkProcess = NetworkProcessProxy::defaultNetworkProcess().get())
        networkProcess->updateProcessAssertion();

#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->updateProcessAssertion();
#endif

#if ENABLE(MODEL_PROCESS)
    updateModelProcessAssertion();
#endif

    // Check on next run loop since the web process proxy tokens are probably being updated.
    callOnMainRunLoop([] {
        remoteWorkerProcesses().forEach([](auto& workerProcess) {
            if (workerProcess.isRunningServiceWorkers())
                workerProcess.updateRemoteWorkerProcessAssertion(RemoteWorkerType::ServiceWorker);
            if (workerProcess.isRunningSharedWorkers())
                workerProcess.updateRemoteWorkerProcessAssertion(RemoteWorkerType::SharedWorker);
        });
    });
}

bool WebProcessPool::isServiceWorkerPageID(WebPageProxyIdentifier pageID) const
{
    // FIXME: This is inefficient.
    return std::ranges::any_of(remoteWorkerProcesses(), [pageID](auto& process) {
        return process.hasServiceWorkerPageProxy(pageID);
    });
    return false;
}

void WebProcessPool::addProcessToOriginCacheSet(WebProcessProxy& process, const URL& url)
{
    auto registrableDomain = WebCore::RegistrableDomain { url };
    auto result = m_swappedProcessesPerRegistrableDomain.add(registrableDomain, &process);
    if (!result.isNewEntry)
        result.iterator->value = process;

    LOG(ProcessSwapping, "(ProcessSwapping) Registrable domain %s just saved a cached process with pid %i", registrableDomain.string().utf8().data(), process.processID());
    if (!result.isNewEntry)
        LOG(ProcessSwapping, "(ProcessSwapping) Note: It already had one saved");
}

void WebProcessPool::removeProcessFromOriginCacheSet(WebProcessProxy& process)
{
    LOG(ProcessSwapping, "(ProcessSwapping) Removing process with pid %i from the origin cache set", process.processID());

    // FIXME: This can be very inefficient as the number of remembered origins and processes grows
    m_swappedProcessesPerRegistrableDomain.removeIf([&](auto& entry) {
        return entry.value == &process;
    });
}

unsigned WebProcessPool::prewarmedProcessCountLimit() const
{
#if PLATFORM(COCOA)
    static unsigned processCountLimitFromPreferences = []() {
        return static_cast<unsigned>(CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessPoolPrewarmedProcessCountLimit"), kCFPreferencesCurrentApplication, nullptr));
    }();

    if (processCountLimitFromPreferences)
        return processCountLimitFromPreferences;
#endif

    if (unsigned limit = m_configuration->prewarmedProcessCountLimitForTesting())
        return limit;

    return m_hasUsedSiteIsolation ? 2 : 1;
}

void WebProcessPool::processForNavigation(WebPageProxy& page, WebFrameProxy& frame, const API::Navigation& navigation, const URL& sourceURL, BrowsingContextGroup& browsingContextGroup, IsSharedProcess isSharedProcess, const Site& mainFrameSite, ProcessSwapRequestedByClient processSwapRequestedByClient, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, LoadedWebArchive loadedWebArchive, const FrameInfoData& frameInfo, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, ASCIILiteral)>&& completionHandler)
{
    Site site { navigation.currentRequest().url() };

    bool siteIsolationEnabled = page.protectedPreferences()->siteIsolationEnabled();
    if (siteIsolationEnabled && !m_hasUsedSiteIsolation)
        m_hasUsedSiteIsolation = true;

    bool isMainFrameNavigation = frame.isMainFrame();
    Ref sourceProcess = frame.process();
    if (siteIsolationEnabled && !isMainFrameNavigation && page.didLoadWebArchive()) {
        ASSERT(sourceProcess.ptr() == &page.protectedMainFrame()->process());
        return completionHandler(sourceProcess.copyRef(), nullptr, "Navigation is treated as same-site (archive load)"_s);
    }

    if (siteIsolationEnabled && !site.isEmpty()) {
        ASSERT(frameInfo.isMainFrame ? site == mainFrameSite : Site(URL(page.protectedPageLoadState()->activeURL())) == mainFrameSite);
        if (!frame.isMainFrame() && site == mainFrameSite) {
            Ref mainFrameProcess = page.protectedMainFrame()->process();
            if (!mainFrameProcess->isInProcessCache())
                return completionHandler(mainFrameProcess.copyRef(), nullptr, "Found process for the same site as main frame"_s);
        }
        RefPtr<WebProcessProxy> process;
        if (RefPtr frameProcess = browsingContextGroup.processForSite(site))
            process = &frameProcess->process();
        if (process && process->websiteDataStore() == dataStore.ptr() && process->websiteDataStore() == &page.websiteDataStore() && !process->isInProcessCache()) {
            dataStore->protectedNetworkProcess()->addAllowedFirstPartyForCookies(*process, mainFrameSite.domain(), LoadedWebArchive::No, [completionHandler = WTFMove(completionHandler), process] () mutable {
                completionHandler(process.releaseNonNull(), nullptr, "Found process for the same site"_s);
            });
            return;
        }
    }

    ASSERT(isSharedProcess == IsSharedProcess::No);
    auto [process, suspendedPage, reason] = processForNavigationInternal(page, navigation, sourceProcess.copyRef(), sourceURL, isSharedProcess, mainFrameSite, processSwapRequestedByClient, lockdownMode, enhancedSecurity, frameInfo, dataStore.copyRef());

    // We are process-swapping so automatic process prewarming would be beneficial if the client has not explicitly enabled / disabled it.
    bool doingAnAutomaticProcessSwap = processSwapRequestedByClient == ProcessSwapRequestedByClient::No && process.ptr() != sourceProcess.ptr();
    Ref configuration = this->configuration();
    if (doingAnAutomaticProcessSwap && !configuration->wasAutomaticProcessWarmingSetByClient() && !configuration->clientWouldBenefitFromAutomaticProcessPrewarming()) {
        WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "processForNavigation: Automatically turning on process prewarming because the client would benefit from it");
        configuration->setClientWouldBenefitFromAutomaticProcessPrewarming(true);
    }

    if (configuration->alwaysKeepAndReuseSwappedProcesses() && process.ptr() != sourceProcess.ptr()) {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            WTFLogAlways("WARNING: The option to always keep swapped web processes alive is active. This is meant for debugging and testing only.");
        });

        addProcessToOriginCacheSet(sourceProcess, sourceURL);

        LOG(ProcessSwapping, "(ProcessSwapping) Navigating from %s to %s, keeping around old process. Now holding on to old processes for %u origins.", sourceURL.string().utf8().data(), navigation.currentRequest().url().string().utf8().data(), m_swappedProcessesPerRegistrableDomain.size());
    }

    if (!isMainFrameNavigation && siteIsolationEnabled)
        return completionHandler(WTFMove(process), suspendedPage.get(), reason);

    ASSERT(process->state() != AuxiliaryProcessProxy::State::Terminated);
    prepareProcessForNavigation(WTFMove(process), page, suspendedPage.get(), reason, isSharedProcess, site, mainFrameSite, navigation, lockdownMode, enhancedSecurity, loadedWebArchive, WTFMove(dataStore), WTFMove(completionHandler));
}

void WebProcessPool::prepareProcessForNavigation(Ref<WebProcessProxy>&& process, WebPageProxy& page, SuspendedPageProxy* suspendedPage, ASCIILiteral reason, IsSharedProcess isSharedProcess, const Site& site, const Site& mainFrameSite,
    const API::Navigation& navigation, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, LoadedWebArchive loadedWebArchive, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, ASCIILiteral)>&& completionHandler, unsigned previousAttemptsCount)
{
    static constexpr unsigned maximumNumberOfAttempts = 3;
    auto preventProcessShutdownScope = process->shutdownPreventingScope();
    auto callCompletionHandler = [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), page = Ref { page }, navigation = Ref { navigation }, process, preventProcessShutdownScope = WTFMove(preventProcessShutdownScope), reason, dataStore, lockdownMode, enhancedSecurity, loadedWebArchive, previousAttemptsCount, isSharedProcess, site, mainFrameSite](SuspendedPageProxy* suspendedPage) mutable {
        // Since the IPC is asynchronous, make sure the destination process and suspended page are still valid.
        if (process->state() == AuxiliaryProcessProxy::State::Terminated && previousAttemptsCount < maximumNumberOfAttempts) {
            // The destination process crashed during the IPC to the network process, use a new process.
            ASSERT(isSharedProcess == IsSharedProcess::No);
            Ref fallbackProcess = processForSite(dataStore, isSharedProcess, site, mainFrameSite, { }, lockdownMode, enhancedSecurity, page->configuration(), WebCore::ProcessSwapDisposition::None);
            prepareProcessForNavigation(WTFMove(fallbackProcess), page, nullptr, reason, isSharedProcess, site, mainFrameSite, navigation, lockdownMode, enhancedSecurity, loadedWebArchive, WTFMove(dataStore), WTFMove(completionHandler), previousAttemptsCount + 1);
            return;
        }
        if (suspendedPage) {
            if (RefPtr targetItem = navigation->targetItem(); !targetItem || suspendedPage != targetItem->suspendedPage())
                suspendedPage = nullptr;
        }
        completionHandler(WTFMove(process), suspendedPage, reason);
    };

    dataStore->protectedNetworkProcess()->addAllowedFirstPartyForCookies(process, site.domain(), loadedWebArchive, [callCompletionHandler = WTFMove(callCompletionHandler), weakSuspendedPage = WeakPtr { suspendedPage }]() mutable {
        if (RefPtr suspendedPage = weakSuspendedPage.get())
            suspendedPage->waitUntilReadyToUnsuspend(WTFMove(callCompletionHandler));
        else
            callCompletionHandler(nullptr);
    });
}

std::tuple<Ref<WebProcessProxy>, RefPtr<SuspendedPageProxy>, ASCIILiteral> WebProcessPool::processForNavigationInternal(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& pageSourceURL, IsSharedProcess isSharedProcess, const Site& mainFrameSite, ProcessSwapRequestedByClient processSwapRequestedByClient, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::EnhancedSecurity enhancedSecurity, const FrameInfoData& frameInfo, Ref<WebsiteDataStore>&& dataStore)
{
    auto& targetURL = navigation.currentRequest().url();
    auto targetSite = Site { targetURL };
    Ref pageConfiguration = page.configuration();

    auto createNewProcess = [&] () -> Ref<WebProcessProxy> {
        ASSERT(isSharedProcess == IsSharedProcess::No);
        return processForSite(dataStore, isSharedProcess, targetSite, mainFrameSite, { }, lockdownMode, enhancedSecurity, pageConfiguration, WebCore::ProcessSwapDisposition::None);
    };

    if (usesSingleWebProcess())
        return { WTFMove(sourceProcess), nullptr, "Single WebProcess mode is enabled"_s };

    if (pageConfiguration->relatedPage() && page.alwaysUseRelatedPageProcess())
        return { WTFMove(sourceProcess), nullptr, "Always using related page process"_s };

    if (sourceProcess->lockdownMode() != lockdownMode)
        return { createNewProcess(), nullptr, "Process swap due to Lockdown mode change"_s };

    if (sourceProcess->enhancedSecurity() != enhancedSecurity)
        return { createNewProcess(), nullptr, "Process swap due to EnhancedSecurity change"_s };

    if (processSwapRequestedByClient == ProcessSwapRequestedByClient::Yes)
        return { createNewProcess(), nullptr, "Process swap was requested by the client"_s };

    if (!m_configuration->processSwapsOnNavigation())
        return { WTFMove(sourceProcess), nullptr, "Feature is disabled"_s };

    if (m_automationSession)
        return { WTFMove(sourceProcess), nullptr, "An automation session is active"_s };

    // Redirects to a different scheme for which the client has registered their own custom handler.
    // We need to process swap so that we end up with a fresh navigation instead of a redirect, so
    // that the app's scheme handler gets used (rdar://117891282).
    if (navigation.currentRequestIsRedirect() && navigation.originalRequest().url().protocol() != targetURL.protocol() && page.urlSchemeHandlerForScheme(targetURL.protocol().toString()))
        return { createNewProcess(), nullptr, "Redirect to a different scheme for which the app registered a custom handler"_s };

    // FIXME: We ought to be able to re-use processes that haven't committed anything with site isolation enabled, but cross-site redirects are tricky. <rdar://116203552>
    bool siteIsolationEnabled = page.protectedPreferences()->siteIsolationEnabled();
    if (!sourceProcess->hasCommittedAnyProvisionalLoads() && !siteIsolationEnabled) {
        tryPrewarmWithDomainInformation(sourceProcess, targetSite.domain());
        return { WTFMove(sourceProcess), nullptr, "Process has not yet committed any provisional loads"_s };
    }

    if (siteIsolationEnabled && navigation.currentRequest().url().isAboutBlank()) {
        RefPtr navigationOriginatorFrame = navigation.originatingFrameInfo() ? WebFrameProxy::webFrame(navigation.originatingFrameInfo()->frameID) : nullptr;
        if (navigationOriginatorFrame)
            return { navigationOriginatorFrame->process(), nullptr, "Frame is navigating to about:blank from a cross site origin. Switching to process that originated navigation."_s };
    }

    // FIXME: We should support process swap when a window has been opened via window.open() without 'noopener'.
    // The issue is that the opener has a handle to the WindowProxy.
    //
    // We may allow a process swap to occur even when the window has an opener if the request came from the client
    // (e.g. a location bar navigation as opposed to a link click). If there's substitute data, then the response
    // may be a response generated by the engine, so consider those navigations as non-client-initiated.
    bool isRequestFromClientOrUserInput = navigation.isRequestFromClientOrUserInput() && !navigation.substituteData();
    if (navigation.openedByDOMWithOpener()
        && page.mainFrame() && page.mainFrame()->opener()
        && !(isRequestFromClientOrUserInput || siteIsolationEnabled || page.protectedWebsiteDataStore()->openerTypeForDomain(targetSite.domain()) == RestrictedOpenerType::NoOpener))
        return { WTFMove(sourceProcess), nullptr, "Browsing context been opened by DOM without 'noopener'"_s };

    // FIXME: We should support process swap when a window has opened other windows via window.open.
    // Currently we only allow the swap if the user has performed some manual navigation action
    // (e.g. navigated via typing in the location bar).
    if (navigation.hasOpenedFrames()
        && page.hasOpenedPage()
        && !(isRequestFromClientOrUserInput || siteIsolationEnabled))
        return { WTFMove(sourceProcess), nullptr, "Browsing context has opened other windows"_s };

    if (RefPtr targetItem = navigation.targetItem(); targetItem && !siteIsolationEnabled) {
        if (CheckedPtr suspendedPage = targetItem->suspendedPage()) {
            if (suspendedPage->protectedProcess()->state() != AuxiliaryProcessProxy::State::Terminated)
                return { suspendedPage->process(), suspendedPage.get(), "Using target back/forward item's process and suspended page"_s };
        }

        if (RefPtr process = WebProcessProxy::processForIdentifier(targetItem->lastProcessIdentifier())) {
            if (process->state() != WebProcessProxy::State::Terminated && process->hasSameGPUAndNetworkProcessPreferencesAs(pageConfiguration)) {
                // Make sure we remove the process from the cache if it is in there since we're about to use it.
                if (process->isInProcessCache()) {
                    m_webProcessCache->removeProcess(*process, WebProcessCache::ShouldShutDownProcess::No);
                    ASSERT(!process->isInProcessCache());
                }

                return { process.releaseNonNull(), nullptr, "Using target back/forward item's process"_s };
            }
        }
    }

    // If it is the first navigation in a DOM popup and there is no opener, then force a process swap no matter what since
    // popup windows are originally created in their opener's process.
    // Note that we currently do not process swap if the window popup has a name. In theory, we should be able to swap in this case too
    // but we would need to transfer over the name to the new process. At this point, it is not clear it is worth the extra complexity.
    if (page.openedByDOM() && !navigation.openedByDOMWithOpener() && !page.hasCommittedAnyProvisionalLoads() && frameInfo.frameName.isEmpty() && !targetURL.protocolIsBlob())
        return { createNewProcess(), nullptr, "Process swap because this is a first navigation in a DOM popup without opener"_s };

    if (navigation.treatAsSameOriginNavigation())
        return { WTFMove(sourceProcess), nullptr, "The treatAsSameOriginNavigation flag is set"_s };

    URL sourceURL;
    if (page.isPageOpenedByDOMShowingInitialEmptyDocument() && !navigation.requesterOrigin().isNull())
        sourceURL = URL { navigation.requesterOrigin().toString() };
    else
        sourceURL = pageSourceURL;

    if (sourceURL.isEmpty()) {
        if (RefPtr relatedPage = pageConfiguration->relatedPage()) {
            sourceURL = URL { relatedPage->pageLoadState().url() };
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForNavigationInternal: Using related page's URL as source URL for process swap decision (page=%p)", pageConfiguration->relatedPage());
        }
    }

    // For non-HTTP(s) URLs, we only swap when navigating to a new scheme, unless processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol is set.
    if (!m_configuration->processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol() && !sourceURL.protocolIsInHTTPFamily() && sourceURL.protocol() == targetURL.protocol() && !siteIsolationEnabled)
        return { WTFMove(sourceProcess), nullptr, "Navigation within the same non-HTTP(s) protocol"_s };

    if (!sourceURL.isValid()
        || !targetURL.isValid()
        || sourceURL.isEmpty()
        || (siteIsolationEnabled ? targetSite.matches(sourceURL) : targetSite.domain().matches(sourceURL)))
        return { WTFMove(sourceProcess), nullptr, "Navigation is same-site"_s };

    if (sourceURL.protocolIsAbout()) {
        if (auto sourceSite = sourceProcess->site()) {
            if (!siteIsolationEnabled && sourceSite->domain().matches(targetURL))
                return { WTFMove(sourceProcess), nullptr, "Navigation is treated as same-site (matched domain)"_s };
            if (siteIsolationEnabled && sourceSite->matches(targetURL))
                return { WTFMove(sourceProcess), nullptr, "Navigation is treated as same-site (matched site)"_s };
        }

        const bool treatAsSameSiteForAboutNavigation = [&sourceProcess, &siteIsolationEnabled] {
            if (sourceProcess->hasCommittedAnyMeaningfulProvisionalLoads())
                return false;
            if (siteIsolationEnabled) {
                auto sourceSite = sourceProcess->site();
                if (sourceSite)
                    return sourceSite->isEmpty();
                return sourceSite.error() == WebProcessProxy::SiteState::NotYetSpecified;
            }

            return true;
        }();

        if (treatAsSameSiteForAboutNavigation)
            return { WTFMove(sourceProcess), nullptr, "Navigation is treated as same-site"_s };
    }

    auto reason = "Navigation is cross-site"_s;
    
    if (m_configuration->alwaysKeepAndReuseSwappedProcesses()) {
        LOG(ProcessSwapping, "(ProcessSwapping) Considering re-use of a previously cached process for domain %s", targetSite.domain().string().utf8().data());

        if (RefPtr process = m_swappedProcessesPerRegistrableDomain.get(targetSite.domain())) {
            if (process->websiteDataStore() == dataStore.ptr() && process->state() != AuxiliaryProcessProxy::State::Terminated) {
                LOG(ProcessSwapping, "(ProcessSwapping) Reusing a previously cached process with pid %i to continue navigation to URL %s", process->processID(), targetURL.string().utf8().data());

                return { process.releaseNonNull(), nullptr, reason };
            }
        }
    }

    return { createNewProcess(), nullptr, reason };
}

void WebProcessPool::addMockMediaDevice(const MockMediaDevice& device)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::addDevice(device);
    sendToAllProcesses(Messages::WebProcess::AddMockMediaDevice { device });
#if ENABLE(GPU_PROCESS) && !USE(GSTREAMER)
    ensureProtectedGPUProcess()->addMockMediaDevice(device);
#endif
#endif
}

void WebProcessPool::clearMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::setDevices({ });
    sendToAllProcesses(Messages::WebProcess::ClearMockMediaDevices { });
#if ENABLE(GPU_PROCESS) && !USE(GSTREAMER)
    ensureProtectedGPUProcess()->clearMockMediaDevices();
#endif
#endif
}

void WebProcessPool::removeMockMediaDevice(const String& persistentId)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::removeDevice(persistentId);
    sendToAllProcesses(Messages::WebProcess::RemoveMockMediaDevice { persistentId });
#if ENABLE(GPU_PROCESS) && !USE(GSTREAMER)
    ensureProtectedGPUProcess()->removeMockMediaDevice(persistentId);
#endif
#endif
}


void WebProcessPool::setMockMediaDeviceIsEphemeral(const String& persistentId, bool isEphemeral)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::setDeviceIsEphemeral(persistentId, isEphemeral);
    sendToAllProcesses(Messages::WebProcess::SetMockMediaDeviceIsEphemeral { persistentId, isEphemeral });
#if ENABLE(GPU_PROCESS) && !USE(GSTREAMER)
    ensureProtectedGPUProcess()->setMockMediaDeviceIsEphemeral(persistentId, isEphemeral);
#endif
#endif
}

void WebProcessPool::resetMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::resetDevices();
    sendToAllProcesses(Messages::WebProcess::ResetMockMediaDevices { });
#if ENABLE(GPU_PROCESS) && !USE(GSTREAMER)
    ensureProtectedGPUProcess()->resetMockMediaDevices();
#endif
#endif
}

void WebProcessPool::didCollectPrewarmInformation(const WebCore::RegistrableDomain& registrableDomain, const WebCore::PrewarmInformation& prewarmInformation)
{
    static const size_t maximumSizeToPreventUnlimitedGrowth = 100;
    if (m_prewarmInformationPerRegistrableDomain.size() == maximumSizeToPreventUnlimitedGrowth)
        m_prewarmInformationPerRegistrableDomain.remove(m_prewarmInformationPerRegistrableDomain.random());

    auto& value = m_prewarmInformationPerRegistrableDomain.ensure(registrableDomain, [] {
        return makeUnique<WebCore::PrewarmInformation>();
    }).iterator->value;

    *value = prewarmInformation;
}

void WebProcessPool::tryPrewarmWithDomainInformation(WebProcessProxy& process, const RegistrableDomain& registrableDomain)
{
    auto* prewarmInformation = m_prewarmInformationPerRegistrableDomain.get(registrableDomain);
    if (!prewarmInformation)
        return;
    process.send(Messages::WebProcess::PrewarmWithDomainInformation(*prewarmInformation), 0);
}

void WebProcessPool::clearCurrentModifierStateForTesting()
{
    sendToAllProcesses(Messages::WebProcess::ClearCurrentModifierStateForTesting());
}

void WebProcessPool::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    sendToAllProcesses(Messages::WebProcess::SetDomainsWithUserInteraction(domains));
    m_domainsWithUserInteraction = WTFMove(domains);
}

void WebProcessPool::setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, Vector<SubResourceDomain>>&& domains, CompletionHandler<void()>&& completionHandler)
{    
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (Ref process : processes())
        process->sendWithAsyncReply(Messages::WebProcess::SetDomainsWithCrossPageStorageAccess(domains), [callbackAggregator] { });

    for (auto& topDomain : domains.keys())
        m_domainsWithCrossPageStorageAccessQuirk.set(topDomain, domains.get(topDomain));
}

void WebProcessPool::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (Ref process : processes())
        process->sendWithAsyncReply(Messages::WebProcess::SeedResourceLoadStatisticsForTesting(firstPartyDomain, thirdPartyDomain, shouldScheduleNotification), [callbackAggregator] { });
}

void WebProcessPool::sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (Ref process : processes()) {
        // WebProcess already flushes outstanding stats to NetworkProcess on suspend, so there's no
        // need to resume a suspended process to force another flush.
        if (!process->pageCount() || process->throttler().isSuspended())
            continue;

        process->sendWithAsyncReply(Messages::WebProcess::SendResourceLoadStatisticsDataImmediately(), [callbackAggregator] { });
    }
}

WebProcessWithAudibleMediaToken WebProcessPool::webProcessWithAudibleMediaToken() const
{
    return m_webProcessWithAudibleMediaCounter.count();
}

void WebProcessPool::clearAudibleActivity()
{
    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "clearAudibleActivity: The number of processes playing audible media is now zero. Releasing UI process assertion.");
    ASSERT(!m_webProcessWithAudibleMediaCounter.value());
    m_audibleMediaActivity = std::nullopt;
}

void WebProcessPool::updateAudibleMediaAssertions()
{
    if (!m_webProcessWithAudibleMediaCounter.value()) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: Starting timer to clear audible activity in %g seconds because we are no longer playing audio", audibleActivityClearDelay.seconds());
        // We clear the audible activity on a timer for 2 reasons:
        // 1. Media may start playing shortly after (e.g. switching from one track to another)
        // 2. It minimizes the risk of the GPUProcess getting suspended while shutting down the media stack.
        m_audibleActivityTimer.startOneShot(audibleActivityClearDelay);
        return;
    }

    m_audibleActivityTimer.stop();
    if (m_audibleMediaActivity)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: The number of processes playing audible media is now greater than zero. Taking UI process assertion.");
#if PLATFORM(COCOA)
    bool shouldTakeUIProcessAssertion = WTF::processHasEntitlement("com.apple.runningboard.assertions.webkit"_s);
#else
    bool shouldTakeUIProcessAssertion = true;
#endif
    m_audibleMediaActivity = AudibleMediaActivity {
        shouldTakeUIProcessAssertion ?  RefPtr<ProcessAssertion> { ProcessAssertion::create(getCurrentProcessID(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback) } : nullptr
#if ENABLE(GPU_PROCESS)
        , gpuProcess() ? RefPtr<ProcessAssertion> { ProcessAssertion::create(*protectedGPUProcess(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback) } : nullptr
#endif
    };
}

WebProcessWithMediaStreamingToken WebProcessPool::webProcessWithMediaStreamingToken() const
{
    return m_webProcessWithMediaStreamingCounter.count();
}

void WebProcessPool::updateMediaStreamingActivity()
{
    if (!m_webProcessWithMediaStreamingCounter.value()) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateMediaStreamingActivity: The number of processes with media networking now zero. Notify network.");
        m_mediaStreamingActivity = false;
        notifyMediaStreamingActivity(false);
        return;
    }

    if (m_mediaStreamingActivity)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateMediaStreamingActivity: The number of processes with media networking is now greater than zero. Notify network.");
    m_mediaStreamingActivity = true;
    notifyMediaStreamingActivity(true);
}

void WebProcessPool::notifyMediaStreamingActivity(bool activity)
{
    if (RefPtr networkProcess = NetworkProcessProxy::defaultNetworkProcess().get())
        networkProcess->notifyMediaStreamingActivity(activity);
}

void WebProcessPool::setUseSeparateServiceWorkerProcess(bool useSeparateServiceWorkerProcess)
{
    if (s_useSeparateServiceWorkerProcess == useSeparateServiceWorkerProcess)
        return;

    WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "setUseSeparateServiceWorkerProcess: (useSeparateServiceWorkerProcess=%d)", useSeparateServiceWorkerProcess);

    s_useSeparateServiceWorkerProcess = useSeparateServiceWorkerProcess;
    for (Ref processPool : allProcessPools())
        processPool->terminateServiceWorkers();
}

bool WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion()
{
    return std::ranges::any_of(WebProcessPool::allProcessPools(), [](auto& processPool) {
        return processPool->shouldTakeUIBackgroundAssertion();
    });
}

void WebProcessPool::forEachProcessForSession(PAL::SessionID sessionID, NOESCAPE const Function<void(WebProcessProxy&)>& apply)
{
    for (Ref process : m_processes) {
        if (process->isPrewarmed() || process->sessionID() != sessionID)
            continue;
        apply(process);
    }
}

size_t WebProcessPool::serviceWorkerProxiesCount() const
{
    unsigned count = 0;
    remoteWorkerProcesses().forEach([&](auto& process) {
        if (process.isRunningServiceWorkers())
            ++count;
    });
    return count;
}

void WebProcessPool::isJITDisabledInAllRemoteWorkerProcesses(CompletionHandler<void(bool)>&& completionHandler) const
{
    class JITDisabledCallbackAggregator : public RefCounted<JITDisabledCallbackAggregator> {
    public:
        static auto create(CompletionHandler<void(bool)>&& callback) { return adoptRef(*new JITDisabledCallbackAggregator(WTFMove(callback))); }

        ~JITDisabledCallbackAggregator()
        {
            if (m_callback)
                m_callback(m_isJITDisabled);
        }

        void setJITEnabled(bool isJITEnabled) { m_isJITDisabled &= !isJITEnabled; }

    private:
        explicit JITDisabledCallbackAggregator(CompletionHandler<void(bool)>&& callback)
            : m_callback(WTFMove(callback))
        { }

        CompletionHandler<void(bool)> m_callback;
        bool m_isJITDisabled { true };
    };

    Ref callbackAggregator = JITDisabledCallbackAggregator::create(WTFMove(completionHandler));
    remoteWorkerProcesses().forEach([&](auto& process) {
        process.sendWithAsyncReply(Messages::WebProcess::IsJITEnabled(), [callbackAggregator](bool isJITEnabled) {
            callbackAggregator->setJITEnabled(isJITEnabled);
        }, 0);
    });
}

bool WebProcessPool::hasServiceWorkerForegroundActivityForTesting() const
{
    return std::ranges::any_of(remoteWorkerProcesses(), [](auto& process) {
        return process.hasServiceWorkerForegroundActivityForTesting();
    });
}

bool WebProcessPool::hasServiceWorkerBackgroundActivityForTesting() const
{
    return std::ranges::any_of(remoteWorkerProcesses(), [](auto& process) {
        return process.hasServiceWorkerBackgroundActivityForTesting();
    });
}

#if !PLATFORM(COCOA)
void addLockdownModeObserver(LockdownModeObserver&)
{
}
void removeLockdownModeObserver(LockdownModeObserver&)
{
}
bool lockdownModeEnabledBySystem()
{
    return false;
}
void setLockdownModeEnabledGloballyForTesting(std::optional<bool>)
{
}
#endif

#if PLATFORM(WIN) // FIXME: remove this line when this feature is enabled for playstation port.
#if ENABLE(REMOTE_INSPECTOR)
void WebProcessPool::setPagesControlledByAutomation(bool controlled)
{
    for (auto& process : m_processes) {
        for (auto& page : process->pages())
            page->setControlledByAutomation(controlled);
    }
}
#endif
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

void WebProcessPool::observeScriptTrackingPrivacyUpdatesIfNeeded()
{
    if (m_scriptTrackingPrivacyDataUpdateObserver)
        return;

    Ref controller = ScriptTrackingPrivacyController::sharedSingleton();
    m_scriptTrackingPrivacyDataUpdateObserver = controller->observeUpdates([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (auto data = ScriptTrackingPrivacyController::sharedSingleton().cachedListData(); !data.isEmpty())
            protectedThis->sendToAllProcesses(Messages::WebProcess::UpdateScriptTrackingPrivacyFilter(WTFMove(data)));
    });
    controller->initializeIfNeeded();
}

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

static ASCIILiteral systemMemoryPressureStatusDescription(SystemMemoryPressureStatus status)
{
    switch (status) {
    case SystemMemoryPressureStatus::Normal:
        return "normal"_s;
    case SystemMemoryPressureStatus::Warning:
        return "warning"_s;
    case SystemMemoryPressureStatus::Critical:
        return "critical"_s;
    default:
        return "unknown"_s;
    }
}

static SystemMemoryPressureStatus systemMemoryPressureStatus()
{
    int level = DISPATCH_MEMORYPRESSURE_NORMAL;
    size_t length = sizeof(level);
    sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &length, nullptr, 0);

    switch (level) {
    case DISPATCH_MEMORYPRESSURE_WARN:
        return SystemMemoryPressureStatus::Warning;
    case DISPATCH_MEMORYPRESSURE_CRITICAL:
        return SystemMemoryPressureStatus::Critical;
    default:
        return SystemMemoryPressureStatus::Normal;
    }
}

static bool shouldSuspendAggressivelyBasedOnSystemMemoryPressureStatus(SystemMemoryPressureStatus status)
{
    static int threshold = []() {
        auto value = CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessMemoryPressureThresholdForAggressiveSuspension"), kCFPreferencesCurrentApplication, nullptr);
        return value ? value : DISPATCH_MEMORYPRESSURE_WARN;
    }();

    if (status == SystemMemoryPressureStatus::Warning)
        return threshold <= DISPATCH_MEMORYPRESSURE_WARN;
    if (status == SystemMemoryPressureStatus::Critical)
        return threshold <= DISPATCH_MEMORYPRESSURE_CRITICAL;
    return false;
}

void WebProcessPool::checkMemoryPressureStatus()
{
    auto now = ApproximateTime::now();

    // Typically a WebContent process receiving a memory pressure notification would update this,
    // but double check in case no WebContent processes have run recently.
    auto status = systemMemoryPressureStatus();
    if (shouldSuspendAggressivelyBasedOnSystemMemoryPressureStatus(status)) {
        WEBPROCESSPOOL_RELEASE_LOG(MemoryPressure, "checkMemoryPressureStatus: System is at %" PUBLIC_LOG_STRING " memory pressure level. WebContent processes will continue suspending aggressively.", systemMemoryPressureStatusDescription(status).characters());
        m_lastMemoryPressureStatusTime = now;
    }

    auto intervalSinceLastMemoryPressureEvent = now - m_lastMemoryPressureStatusTime;

    if (intervalSinceLastMemoryPressureEvent >= memoryPressureCheckInterval()) {
        WEBPROCESSPOOL_RELEASE_LOG(MemoryPressure, "checkMemoryPressureStatus: System has not been under memory pressure for a long period of time. WebContent processes will suspend normally.");
        updateWebProcessSuspensionDelay();
        return;
    }

    m_checkMemoryPressureStatusTimer.startOneShot(memoryPressureCheckInterval() - intervalSinceLastMemoryPressureEvent);
}

Seconds WebProcessPool::defaultWebProcessSuspensionDelay()
{
    static Seconds delay = []() {
        auto value = CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessSuspensionDelay"), kCFPreferencesCurrentApplication, nullptr);
        return value > 0 ? Seconds(value) : 8_min;
    }();
    return delay;
}

Seconds WebProcessPool::webProcessSuspensionDelay() const
{
    static Seconds fastSuspensionDelay = []() {
        auto value = CFPreferencesGetAppIntegerValue(CFSTR("DebugWebProcessFastSuspensionDelay"), kCFPreferencesCurrentApplication, nullptr);
        return value > 0 ? Seconds(value) : 10_s;
    }();

    if (!m_configuration->suspendsWebProcessesAggressivelyOnMemoryPressure())
        return defaultWebProcessSuspensionDelay();

    // The system has been under critical memory pressure recently, so suspend processes faster than normal.
    if (ApproximateTime::now() < m_lastMemoryPressureStatusTime + memoryPressureCheckInterval())
        return fastSuspensionDelay;

    return defaultWebProcessSuspensionDelay();
}

void WebProcessPool::memoryPressureStatusChangedForProcess(WebProcessProxy& process, SystemMemoryPressureStatus)
{
    // Read the most up-to-date status from the kernel to make sure this isn't a stale event from a
    // suspended process that just resumed.
    SystemMemoryPressureStatus status = systemMemoryPressureStatus();

    if (status == SystemMemoryPressureStatus::Critical && process.isRunningServiceWorkers()) {
        RefPtr store = process.websiteDataStore();
        RefPtr networkProcess = store ? store->networkProcessIfExists() : nullptr;
        if (networkProcess)
            networkProcess->terminateIdleServiceWorkers(process.coreProcessIdentifier(), [activity = process.protectedThrottler()->backgroundActivity("Idle service worker processing"_s)] { });
    }

    if (!m_configuration->suspendsWebProcessesAggressivelyOnMemoryPressure() || !shouldSuspendAggressivelyBasedOnSystemMemoryPressureStatus(status))
        return;

    m_lastMemoryPressureStatusTime = ApproximateTime::now();

    // Immediately tell this process to update its suspension delay, which might cause it to suspend
    // and help relieve the pressure.
    process.updateWebProcessSuspensionDelay();

    if (m_checkMemoryPressureStatusTimer.isActive())
        return;
    m_checkMemoryPressureStatusTimer.startOneShot(memoryPressureCheckInterval());

    WEBPROCESSPOOL_RELEASE_LOG(MemoryPressure, "memoryPressureStatusChangedForProcess: System is at %" PUBLIC_LOG_STRING " memory pressure level. WebContent processes will suspend aggressively.", systemMemoryPressureStatusDescription(status).characters());
    updateWebProcessSuspensionDelay();
}

void WebProcessPool::updateWebProcessSuspensionDelay()
{
    WeakHashSet<WebProcessProxy> remainingProcesses;
    for (Ref process : processes()) {
        if (process->throttler().isSuspended()) {
            process->updateWebProcessSuspensionDelay();
            continue;
        }

        // This process is currently running, so updating its suspension delay might cause it to
        // suspend. We pace these state changes to reduce the likelihood of CPU or swap storms.
        remainingProcesses.add(process);
    }

    updateWebProcessSuspensionDelayWithPacing(WTFMove(remainingProcesses));
}

void WebProcessPool::updateWebProcessSuspensionDelayWithPacing(WeakHashSet<WebProcessProxy>&& processes)
{
    RefPtr process = processes.takeAny();
    if (!process)
        return;

    process->updateWebProcessSuspensionDelay();

    // Updating the activity timeout of a running process can cause it to suspend, which then
    // triggers other operations (like full GC and cache clearing) that can be expensive. We pace
    // these state changes to reduce the likelihood of CPU or swap storms.
    static constexpr Seconds intervalBetweenUpdates { 500_ms };
    WorkQueue::mainSingleton().dispatchAfter(intervalBetweenUpdates, [weakThis = WeakPtr { *this }, processes = WTFMove(processes)]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->updateWebProcessSuspensionDelayWithPacing(WTFMove(processes));
    });
}

#endif // ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

#if ENABLE(CONTENT_EXTENSIONS)

constexpr static Seconds resourceMonitorRuleListCheckInterval = 24_h;

WebCompiledContentRuleList* WebProcessPool::cachedResourceMonitorRuleList(bool forTesting)
{
    if (m_resourceMonitorRuleListCache)
        return m_resourceMonitorRuleListCache.get();

    if (forTesting) {
        setResourceMonitorURLsForTesting(platformResourceMonitorRuleListSourceForTesting(), [] { });
        return nullptr;
    }

    loadOrUpdateResourceMonitorRuleList();
    return nullptr;
}

void WebProcessPool::loadOrUpdateResourceMonitorRuleList()
{
    if (m_resourceMonitorRuleListLoading || m_resourceMonitorRuleListFailed)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ResourceMonitoring, "loadOrUpdateResourceMonitorRuleList: rule list is requested");

    m_resourceMonitorRuleListLoading = true;

    void* savedRuleList = reinterpret_cast<void*>(m_resourceMonitorRuleListCache.get());
    platformLoadResourceMonitorRuleList([weakThis = WeakPtr { *this }, savedRuleList](RefPtr<WebCompiledContentRuleList> ruleList) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_resourceMonitorRuleListLoading = false;

        if (!ruleList) {
            protectedThis->m_resourceMonitorRuleListFailed = true;
            return;
        }

        // Check somebody already updates the list or not.
        if (savedRuleList != protectedThis->m_resourceMonitorRuleListCache.get())
            return;

        protectedThis->m_resourceMonitorRuleListCache = ruleList;

        if (!protectedThis->m_resourceMonitorRuleListRefreshTimer.isActive()) {
            auto interval = resourceMonitorRuleListCheckInterval + Seconds::fromHours(cryptographicallyRandomUnitInterval());
            protectedThis->m_resourceMonitorRuleListRefreshTimer.startOneShot(interval);
        }

        if (protectedThis->m_processes.isEmpty())
            return;

        for (Ref process : protectedThis->m_processes)
            process->setResourceMonitorRuleListsIfRequired(RefPtr { ruleList });
    });
}

void WebProcessPool::setResourceMonitorURLsForTesting(const String& rulesText, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    platformCompileResourceMonitorRuleList(rulesText, [weakThis = WeakPtr { *this }, callbackAggregator](auto ruleList) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !ruleList)
            return;

        protectedThis->m_resourceMonitorRuleListCache = ruleList;
        protectedThis->m_resourceMonitorRuleListRefreshTimer.stop();

        if (protectedThis->m_processes.isEmpty())
            return;

        for (Ref process : protectedThis->m_processes)
            process->setResourceMonitorRuleLists(RefPtr { ruleList }, [callbackAggregator] { });
    });
}

#if !PLATFORM(COCOA)
void WebProcessPool::platformLoadResourceMonitorRuleList(CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&& completionHandler)
{
    notImplemented();
    completionHandler(nullptr);
}

void WebProcessPool::platformCompileResourceMonitorRuleList(const String& rulesText, CompletionHandler<void(RefPtr<WebCompiledContentRuleList>)>&& completionHandler)
{
    notImplemented();
    completionHandler(nullptr);
}

String WebProcessPool::platformResourceMonitorRuleListSourceForTesting()
{
    notImplemented();
    return "[]"_s;
}

#endif

#endif

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
void WebProcessPool::initializeAccessibilityIfNecessary()
{
    RELEASE_LOG(Process, "WebProcessPool::initializeAccessibility");
    if (m_hasReceivedAXRequestInUIProcess)
        return;

    for (auto& process : m_processes) {
        auto handleArray = SandboxExtension::createHandlesForMachLookup({ }, process->auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
        process->send(Messages::WebProcess::InitializeAccessibility(WTFMove(handleArray)), 0);
    }

    m_hasReceivedAXRequestInUIProcess = true;
}
#endif

} // namespace WebKit
