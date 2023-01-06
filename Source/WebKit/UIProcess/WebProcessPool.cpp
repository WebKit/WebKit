/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcessMessages.h"
#include "AuxiliaryProcessProxy.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "GPUProcessConnectionParameters.h"
#include "GamepadData.h"
#include "HighPerformanceGraphicsUsageSampler.h"
#include "LegacyGlobalSettings.h"
#include "Logging.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "OverrideLanguages.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "RemoteWorkerType.h"
#include "SandboxExtension.h"
#include "TextChecker.h"
#include "UIGamepad.h"
#include "UIGamepadProvider.h"
#include "UIProcessLogInitialization.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebContextSupplement.h"
#include "WebCoreArgumentCoders.h"
#include "WebGeolocationManagerProxy.h"
#include "WebInspectorUtilities.h"
#include "WebKit2Initialize.h"
#include "WebMemorySampler.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
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
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/GamepadProvider.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RegistrationDatabase.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URLParser.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

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
#include <WebCore/GameControllerGamepadProvider.h>
#include <WebCore/HIDGamepadProvider.h>
#include <WebCore/MultiGamepadProvider.h>
#include <WebCore/PowerSourceNotifier.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

#define WEBPROCESSPOOL_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_STATIC(channel, fmt, ...) RELEASE_LOG(channel, "WebProcessPool::" fmt, ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, processPoolCounter, ("WebProcessPool"));

constexpr Seconds serviceWorkerTerminationDelay { 5_s };

#if ENABLE(GPU_PROCESS)
constexpr Seconds resetGPUProcessCrashCountDelay { 30_s };
constexpr unsigned maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses { 2 };
#endif

Ref<WebProcessPool> WebProcessPool::create(API::ProcessPoolConfiguration& configuration)
{
    InitializeWebKit2();
    return adoptRef(*new WebProcessPool(configuration));
}

static Vector<WebProcessPool*>& processPools()
{
    static NeverDestroyed<Vector<WebProcessPool*>> processPools;
    return processPools;
}

Vector<Ref<WebProcessPool>> WebProcessPool::allProcessPools()
{
    return WTF::map(processPools(), [] (auto&& v) -> Ref<WebProcessPool> {
        return *v;
    });
}

static HashSet<String, ASCIICaseInsensitiveHash>& globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> set;
    return set;
}

Vector<String> WebProcessPool::urlSchemesWithCustomProtocolHandlers()
{
    return copyToVector(globalURLSchemesWithCustomProtocolHandlers());
}

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundleClient(makeUnique<API::InjectedBundleClient>())
    , m_automationClient(makeUnique<API::AutomationClient>())
    , m_historyClient(makeUnique<API::LegacyContextHistoryClient>())
    , m_visitedLinkStore(VisitedLinkStore::create())
#if PLATFORM(MAC)
    , m_highPerformanceGraphicsUsageSampler(makeUnique<HighPerformanceGraphicsUsageSampler>(*this))
    , m_perActivityStateCPUUsageSampler(makeUnique<PerActivityStateCPUUsageSampler>(*this))
#endif
    , m_alwaysRunsAtBackgroundPriority(m_configuration->alwaysRunsAtBackgroundPriority())
    , m_shouldTakeUIBackgroundAssertion(m_configuration->shouldTakeUIBackgroundAssertion())
    , m_userObservablePageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_hiddenPageThrottlingAutoIncreasesCounter([this](RefCounterEvent) { m_hiddenPageThrottlingTimer.startOneShot(0_s); })
    , m_hiddenPageThrottlingTimer(RunLoop::main(), this, &WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit)
#if ENABLE(GPU_PROCESS)
    , m_resetGPUProcessCrashCountTimer(RunLoop::main(), [this] { m_recentGPUProcessCrashCount = 0; })
#endif
    , m_foregroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backgroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backForwardCache(makeUniqueRef<WebBackForwardCache>(*this))
    , m_webProcessCache(makeUniqueRef<WebProcessCache>(*this))
    , m_webProcessWithAudibleMediaCounter([this](RefCounterEvent) { updateAudibleMediaAssertions(); })
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        WTF::setProcessPrivileges(allPrivileges());
        WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
        Process::setIdentifier(WebCore::ProcessIdentifier::generate());
#if PLATFORM(COCOA)
        determineTrackingPreventionState();

        // This can be removed once Safari calls _setLinkedOnOrAfterEverything everywhere that WebKit deploys.
#if PLATFORM(IOS_FAMILY)
        bool isSafari = WebCore::IOSApplication::isMobileSafari();
#elif PLATFORM(MAC)
        bool isSafari = WebCore::MacApplication::isSafari();
#endif
        if (isSafari)
            enableAllSDKAlignedBehaviors();
#endif
    });

    for (auto& scheme : m_configuration->alwaysRevalidatedURLSchemes())
        m_schemesToRegisterAsAlwaysRevalidated.add(scheme);

    for (const auto& urlScheme : m_configuration->cachePartitionedURLSchemes())
        m_schemesToRegisterAsCachePartitioned.add(urlScheme);

    platformInitialize();

#if OS(LINUX)
    MemoryPressureMonitor::singleton().start();
#endif

    addMessageReceiver(Messages::WebProcessPool::messageReceiverName(), *this);
#if ENABLE(IPC_TESTING_API)
    addMessageReceiver(Messages::IPCTester::messageReceiverName(), m_ipcTester);
#endif

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    addSupplement<WebGeolocationManagerProxy>();
    addSupplement<WebNotificationManagerProxy>();

    processPools().append(this);

    resolvePathsForSandboxExtensions();

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    UIProcess::initializeLoggingIfNecessary();
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef NDEBUG
    processPoolCounter.increment();
#endif

    ASSERT(RunLoop::isMain());

    updateBackForwardCacheCapacity();

#if PLATFORM(IOS)
    if (WebCore::IOSApplication::isLutron() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SharedNetworkProcess)) {
        callOnMainRunLoop([] {
            if (WebsiteDataStore::defaultDataStoreExists())
                WebsiteDataStore::defaultDataStore()->terminateNetworkProcess();
        });
    }
#endif
}

WebProcessPool::~WebProcessPool()
{
    m_webProcessCache->clear();

    bool removed = processPools().removeFirst(this);
    ASSERT_UNUSED(removed, removed);

    m_messageReceiverMap.invalidate();

    for (auto& supplement : m_supplements.values()) {
        supplement->processPoolDestroyed();
        supplement->clearProcessPool();
    }

    platformInvalidateContext();

#ifndef NDEBUG
    processPoolCounter.decrement();
#endif

#if ENABLE(GAMEPAD)
    if (!m_processesUsingGamepads.computesEmpty())
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

void WebProcessPool::initializeConnectionClient(const WKContextConnectionClientBase* client)
{
    m_connectionClient.initialize(client);
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

void WebProcessPool::setCustomWebContentServiceBundleIdentifier(const String& customWebContentServiceBundleIdentifier)
{
    // Guard against API misuse.
    if (!customWebContentServiceBundleIdentifier.isAllASCII())
        CRASH();

    m_configuration->setCustomWebContentServiceBundleIdentifier(customWebContentServiceBundleIdentifier);
}

void WebProcessPool::setOverrideLanguages(Vector<String>&& languages)
{
    WebKit::setOverrideLanguages(WTFMove(languages));

    LOG_WITH_STREAM(Language, stream << "WebProcessPool is setting OverrideLanguages: " << languages);
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(overrideLanguages()));
#if USE(SOUP)
    for (auto networkProcess : NetworkProcessProxy::allNetworkProcesses())
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
    for (auto networkProcess : NetworkProcessProxy::allNetworkProcesses())
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

void WebProcessPool::screenPropertiesStateChanged()
{
#if PLATFORM(COCOA)
    auto screenProperties = WebCore::collectScreenProperties();
    sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));

#if PLATFORM(MAC)
    if (auto process = gpuProcess())
        process->setScreenProperties(screenProperties);
#endif
#endif
}

static bool shouldReportAuxiliaryProcessCrash(ProcessTerminationReason reason)
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
        return false;
    }

    return false;
}

void WebProcessPool::networkProcessDidTerminate(NetworkProcessProxy& networkProcessProxy, ProcessTerminationReason reason)
{
    if (shouldReportAuxiliaryProcessCrash(reason))
        m_client.networkProcessDidCrash(this, networkProcessProxy.processIdentifier(), reason);

    if (m_automationSession)
        m_automationSession->terminate();

    terminateServiceWorkers();
}

void WebProcessPool::serviceWorkerProcessCrashed(WebProcessProxy& proxy, ProcessTerminationReason reason)
{
#if ENABLE(SERVICE_WORKER)
    m_client.serviceWorkerProcessDidCrash(this, proxy.processIdentifier(), reason);
#endif
}

#if ENABLE(GPU_PROCESS)
GPUProcessProxy& WebProcessPool::ensureGPUProcess()
{
    if (!m_gpuProcess) {
        m_gpuProcess = GPUProcessProxy::getOrCreate();
        for (auto& process : m_processes)
            m_gpuProcess->updatePreferences(process);
        m_gpuProcess->updateScreenPropertiesIfNeeded();
    }
    return *m_gpuProcess;
}


void WebProcessPool::gpuProcessDidFinishLaunching(ProcessID)
{
    auto processes = m_processes;
    for (auto& process : processes)
        process->gpuProcessDidFinishLaunching();
}

void WebProcessPool::gpuProcessExited(ProcessID identifier, ProcessTerminationReason reason)
{
    WEBPROCESSPOOL_RELEASE_LOG(Process, "gpuProcessDidExit: PID=%d, reason=%" PUBLIC_LOG_STRING, identifier, processTerminationReasonToString(reason));
    m_gpuProcess = nullptr;

    if (shouldReportAuxiliaryProcessCrash(reason))
        m_client.gpuProcessDidCrash(this, identifier, reason);

    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (auto& process : processes)
        process->gpuProcessExited(reason);

    if (reason == ProcessTerminationReason::Crash || reason == ProcessTerminationReason::Unresponsive) {
        if (++m_recentGPUProcessCrashCount > maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses) {
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "gpuProcessDidExit: GPU Process has crashed more than %u times in the last %g seconds, terminating all WebProcesses", maximumGPUProcessRelaunchAttemptsBeforeKillingWebProcesses, resetGPUProcessCrashCountDelay.seconds());
            m_resetGPUProcessCrashCountTimer.stop();
            m_recentGPUProcessCrashCount = 0;
            terminateAllWebContentProcesses();
        } else if (!m_resetGPUProcessCrashCountTimer.isActive())
            m_resetGPUProcessCrashCountTimer.startOneShot(resetGPUProcessCrashCountDelay);
    }
}

void WebProcessPool::createGPUProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, WebKit::GPUProcessConnectionParameters&& parameters)
{
#if ENABLE(IPC_TESTING_API)
    parameters.ignoreInvalidMessageForTesting = webProcessProxy.ignoreInvalidMessageForTesting();
#endif

#if HAVE(AUDIT_TOKEN)
    parameters.presentingApplicationAuditToken = configuration().presentingApplicationProcessToken();
#endif

    parameters.isLockdownModeEnabled = webProcessProxy.lockdownMode() == WebProcessProxy::LockdownMode::Enabled;
    ensureGPUProcess().createGPUProcessConnection(webProcessProxy, WTFMove(connectionIdentifier), WTFMove(parameters));
}
#endif

bool WebProcessPool::s_useSeparateServiceWorkerProcess = false;

void WebProcessPool::establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType workerType, RegistrableDomain&& registrableDomain, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID sessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&& completionHandler)
{
    auto* websiteDataStore = WebsiteDataStore::existingDataStoreForSessionID(sessionID);
    if (!websiteDataStore)
        websiteDataStore = WebsiteDataStore::defaultDataStore().ptr();
    if (!processPools().size())
        static NeverDestroyed<Ref<WebProcessPool>> remoteWorkerProcessPool(WebProcessPool::create(API::ProcessPoolConfiguration::create().get()));

    RefPtr<WebProcessProxy> requestingProcess = requestingProcessIdentifier ? WebProcessProxy::processForIdentifier(*requestingProcessIdentifier) : nullptr;
    WebProcessPool* processPool = requestingProcess ? &requestingProcess->processPool() : processPools()[0];
    ASSERT(processPool);

    WebProcessProxy* remoteWorkerProcessProxy { nullptr };

    auto useProcessForRemoteWorkers = [&](WebProcessProxy& process) {
        remoteWorkerProcessProxy = &process;
        process.enableRemoteWorkers(workerType, processPool->userContentControllerIdentifierForRemoteWorkers());
        if (process.isInProcessCache()) {
            processPool->webProcessCache().removeProcess(process, WebProcessCache::ShouldShutDownProcess::No);
            ASSERT(!process.isInProcessCache());
        }
    };

    if (serviceWorkerPageIdentifier) {
        ASSERT(workerType == RemoteWorkerType::ServiceWorker);
        // This is a service worker for a service worker page so we need to make sure we use use the page's WebProcess for the service worker.
        if (auto process = WebProcessProxy::processForIdentifier(serviceWorkerPageIdentifier->processIdentifier()))
            useProcessForRemoteWorkers(*process);
    }

    // Prioritize the requesting WebProcess for running the service worker.
    if (!remoteWorkerProcessProxy && !s_useSeparateServiceWorkerProcess && requestingProcess) {
        if (requestingProcess->websiteDataStore() == websiteDataStore && requestingProcess->isMatchingRegistrableDomain(registrableDomain))
            useProcessForRemoteWorkers(*requestingProcess);
    }

    if (!remoteWorkerProcessProxy && !s_useSeparateServiceWorkerProcess) {
        for (auto& process : processPool->m_processes) {
            if (process.ptr() == processPool->m_prewarmedProcess.get() || process->isDummyProcessProxy())
                continue;
            if (process->websiteDataStore() != websiteDataStore)
                continue;
            if (!process->isMatchingRegistrableDomain(registrableDomain))
                continue;

            useProcessForRemoteWorkers(process);

            WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "establishRemoteWorkerContextConnectionToNetworkProcess reusing an existing web process (process=%p, workerType=%" PUBLIC_LOG_STRING ", PID=%d)", remoteWorkerProcessProxy, workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", remoteWorkerProcessProxy->processIdentifier());
            break;
        }
    }

    if (!remoteWorkerProcessProxy) {
        auto newProcessProxy = WebProcessProxy::createForRemoteWorkers(workerType, *processPool, RegistrableDomain  { registrableDomain }, *websiteDataStore);
        remoteWorkerProcessProxy = newProcessProxy.ptr();

        WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "establishRemoteWorkerContextConnectionToNetworkProcess creating a new service worker process (process=%p, workerType=%" PUBLIC_LOG_STRING ", PID=%d)", remoteWorkerProcessProxy, workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", remoteWorkerProcessProxy->processIdentifier());

        processPool->initializeNewWebProcess(newProcessProxy, websiteDataStore);
        processPool->m_processes.append(WTFMove(newProcessProxy));
    }

    remoteWorkerProcesses().add(*remoteWorkerProcessProxy);

    const WebPreferencesStore* preferencesStore = nullptr;
    if (workerType == RemoteWorkerType::ServiceWorker) {
        if (auto* preferences = websiteDataStore->serviceWorkerOverridePreferences())
            preferencesStore = &preferences->store();
    }

    if (!preferencesStore && processPool->m_remoteWorkerPreferences)
        preferencesStore = &processPool->m_remoteWorkerPreferences.value();

    if (!preferencesStore)
        preferencesStore = &processPool->m_defaultPageGroup->preferences().store();

    ASSERT(preferencesStore);

    remoteWorkerProcessProxy->establishRemoteWorkerContext(workerType, *preferencesStore, registrableDomain, serviceWorkerPageIdentifier, [completionHandler = WTFMove(completionHandler), remoteProcessIdentifier = remoteWorkerProcessProxy->coreProcessIdentifier()] () mutable {
        completionHandler(remoteProcessIdentifier);
    });

    if (!processPool->m_remoteWorkerUserAgent.isNull())
        remoteWorkerProcessProxy->setRemoteWorkerUserAgent(processPool->m_remoteWorkerUserAgent);
}

void WebProcessPool::removeFromRemoteWorkerProcesses(WebProcessProxy& process)
{
    ASSERT(remoteWorkerProcesses().contains(process));
    remoteWorkerProcesses().remove(process);
}

void WebProcessPool::windowServerConnectionStateChanged()
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i)
        m_processes[i]->windowServerConnectionStateChanged();
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

    m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.reserveCapacity(m_configuration->additionalReadAccessAllowedPaths().size());
    for (const auto& path : m_configuration->additionalReadAccessAllowedPaths())
        m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.uncheckedAppend(resolvePathForSandboxExtension(path));

    platformResolvePathsForSandboxExtensions();
}

Ref<WebProcessProxy> WebProcessPool::createNewWebProcess(WebsiteDataStore* websiteDataStore, WebProcessProxy::LockdownMode lockdownMode, WebProcessProxy::IsPrewarmed isPrewarmed, CrossOriginMode crossOriginMode)
{
#if PLATFORM(COCOA)
    m_tccPreferenceEnabled = doesAppHaveTrackingPreventionEnabled();
    if (websiteDataStore && !websiteDataStore->isTrackingPreventionStateExplicitlySet())
        websiteDataStore->setTrackingPreventionEnabled(m_tccPreferenceEnabled);
#endif

    auto processProxy = WebProcessProxy::create(*this, websiteDataStore, lockdownMode, isPrewarmed, crossOriginMode);
    initializeNewWebProcess(processProxy, websiteDataStore, isPrewarmed);
    m_processes.append(processProxy.copyRef());

#if ENABLE(WEBCONTENT_CRASH_TESTING)
    if (shouldCrashWhenCreatingWebProcess()) {
        auto crashyProcessProxy = WebProcessProxy::createForWebContentCrashy(*this);
        initializeNewWebProcess(crashyProcessProxy, nullptr);
        m_processes.append(crashyProcessProxy.copyRef());
    }
#endif

    return processProxy;
}

RefPtr<WebProcessProxy> WebProcessPool::tryTakePrewarmedProcess(WebsiteDataStore& websiteDataStore, WebProcessProxy::LockdownMode lockdownMode)
{
    if (!m_prewarmedProcess)
        return nullptr;
    
    // There is sometimes a delay until we get notified that a prewarmed process has been terminated (e.g. after resuming
    // from suspension) so make sure the process is still running here before deciding to use it.
    if (m_prewarmedProcess->wasTerminated()) {
        WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "tryTakePrewarmedProcess: Not using prewarmed process because it has been terminated (process=%p, PID=%d)", m_prewarmedProcess.get(), m_prewarmedProcess->processIdentifier());
        m_prewarmedProcess = nullptr;
        return nullptr;
    }

    if (m_prewarmedProcess->lockdownMode() != lockdownMode)
        return nullptr;

#if PLATFORM(GTK) || PLATFORM(WPE)
    // In platforms using Bubblewrap for sandboxing, prewarmed process is launched using the WebProcessPool primary WebsiteDataStore,
    // so we don't use it in case of using a different WebsiteDataStore.
    if (m_sandboxEnabled)
        return nullptr;
#endif

    ASSERT(m_prewarmedProcess->isPrewarmed());
    m_prewarmedProcess->setWebsiteDataStore(websiteDataStore);
    m_prewarmedProcess->markIsNoLongerInPrewarmedPool();

    return std::exchange(m_prewarmedProcess, nullptr).get();
}

#if !PLATFORM(MAC)
void WebProcessPool::registerDisplayConfigurationCallback()
{
}
#endif // !PLATFORM(MAC)

#if !PLATFORM(MAC) && !PLATFORM(IOS)
void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
}
#endif

WebProcessDataStoreParameters WebProcessPool::webProcessDataStoreParameters(WebProcessProxy& process, WebsiteDataStore& websiteDataStore)
{
    websiteDataStore.resolveDirectoriesIfNecessary();

    String applicationCacheDirectory = websiteDataStore.resolvedApplicationCacheDirectory();
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    if (!applicationCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(applicationCacheDirectory, SandboxExtension::Type::ReadWrite))
            applicationCacheDirectoryExtensionHandle = WTFMove(*handle);
    }

    String applicationCacheFlatFileSubdirectoryName = websiteDataStore.applicationCacheFlatFileSubdirectoryName();

    String mediaCacheDirectory = websiteDataStore.resolvedMediaCacheDirectory();
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!mediaCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(mediaCacheDirectory, SandboxExtension::Type::ReadWrite))
            mediaCacheDirectoryExtensionHandle = WTFMove(*handle);
    }

    String mediaKeyStorageDirectory = websiteDataStore.resolvedMediaKeysDirectory();
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    if (!mediaKeyStorageDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(mediaKeyStorageDirectory, SandboxExtension::Type::ReadWrite))
            mediaKeyStorageDirectoryExtensionHandle = WTFMove(*handle);
    }

    String javaScriptConfigurationDirectory;
    if (!m_javaScriptConfigurationDirectory.isEmpty())
        javaScriptConfigurationDirectory = resolvePathForSandboxExtension(m_javaScriptConfigurationDirectory);
    else if (javaScriptConfigurationFileEnabled())
        javaScriptConfigurationDirectory = websiteDataStore.resolvedJavaScriptConfigurationDirectory();

    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
    if (!javaScriptConfigurationDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(javaScriptConfigurationDirectory, SandboxExtension::Type::ReadWrite))
            javaScriptConfigurationDirectoryExtensionHandle = WTFMove(*handle);
    }

#if ENABLE(ARKIT_INLINE_PREVIEW)
    auto modelElementCacheDirectory = websiteDataStore.resolvedModelElementCacheDirectory();
    SandboxExtension::Handle modelElementCacheDirectoryExtensionHandle;
    if (!modelElementCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(modelElementCacheDirectory, SandboxExtension::Type::ReadWrite))
            modelElementCacheDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<SandboxExtension::Handle> cookieStorageDirectoryExtensionHandle;
    if (auto directory = websiteDataStore.resolvedCookieStorageDirectory(); !directory.isEmpty())
        cookieStorageDirectoryExtensionHandle = SandboxExtension::createHandleWithoutResolvingPath(directory, SandboxExtension::Type::ReadWrite);
    std::optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    if (auto directory = websiteDataStore.resolvedContainerCachesWebContentDirectory(); !directory.isEmpty())
        containerCachesDirectoryExtensionHandle = SandboxExtension::createHandleWithoutResolvingPath(directory, SandboxExtension::Type::ReadWrite);
    std::optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
    if (auto directory = websiteDataStore.resolvedContainerTemporaryDirectory(); !directory.isEmpty())
        containerTemporaryDirectoryExtensionHandle = SandboxExtension::createHandleWithoutResolvingPath(directory, SandboxExtension::Type::ReadWrite);
#endif

    return WebProcessDataStoreParameters {
        websiteDataStore.sessionID(),
        WTFMove(applicationCacheDirectory),
        WTFMove(applicationCacheDirectoryExtensionHandle),
        WTFMove(applicationCacheFlatFileSubdirectoryName),
        WTFMove(mediaCacheDirectory),
        WTFMove(mediaCacheDirectoryExtensionHandle),
        WTFMove(mediaKeyStorageDirectory),
        WTFMove(mediaKeyStorageDirectoryExtensionHandle),
        WTFMove(javaScriptConfigurationDirectory),
        WTFMove(javaScriptConfigurationDirectoryExtensionHandle),
#if ENABLE(TRACKING_PREVENTION)
        websiteDataStore.thirdPartyCookieBlockingMode(),
        m_domainsWithUserInteraction,
        m_domainsWithCrossPageStorageAccessQuirk,
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
        WTFMove(modelElementCacheDirectory),
        WTFMove(modelElementCacheDirectoryExtensionHandle),
#endif
#if PLATFORM(IOS_FAMILY)
        WTFMove(cookieStorageDirectoryExtensionHandle),
        WTFMove(containerCachesDirectoryExtensionHandle),
        WTFMove(containerTemporaryDirectoryExtensionHandle),
#endif
        websiteDataStore.trackingPreventionEnabled()
    };
}

void WebProcessPool::initializeNewWebProcess(WebProcessProxy& process, WebsiteDataStore* websiteDataStore, WebProcessProxy::IsPrewarmed isPrewarmed)
{
    auto initializationActivity = process.throttler().backgroundActivity("WebProcess initialization"_s);
    auto scopeExit = makeScopeExit([&process, initializationActivity = WTFMove(initializationActivity)]() mutable {
        // Round-trip to the Web Content process before releasing the
        // initialization activity, so that we're sure that all
        // messages sent from this function have been handled.
        process.isResponsive([initializationActivity = WTFMove(initializationActivity)] (bool) { });
    });

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

    parameters.urlSchemesRegisteredAsEmptyDocument = copyToVector(m_schemesToRegisterAsEmptyDocument);
    parameters.urlSchemesRegisteredAsSecure = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsSecure());
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsBypassingContentSecurityPolicy());
    parameters.urlSchemesForWhichDomainRelaxationIsForbidden = copyToVector(m_schemesToSetDomainRelaxationForbiddenFor);
    parameters.urlSchemesRegisteredAsLocal = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsLocal());
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsNoAccess());
    parameters.urlSchemesRegisteredAsDisplayIsolated = copyToVector(m_schemesToRegisterAsDisplayIsolated);
    parameters.urlSchemesRegisteredAsCORSEnabled = copyToVector(m_schemesToRegisterAsCORSEnabled);
    parameters.urlSchemesRegisteredAsAlwaysRevalidated = copyToVector(m_schemesToRegisterAsAlwaysRevalidated);
    parameters.urlSchemesRegisteredAsCachePartitioned = copyToVector(m_schemesToRegisterAsCachePartitioned);
    parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest = copyToVector(m_schemesToRegisterAsCanDisplayOnlyIfCanRequest);

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
        parameters.notificationPermissions = supplement<WebNotificationManagerProxy>()->notificationPermissions();
#endif

    parameters.memoryCacheDisabled = m_memoryCacheDisabled;
    parameters.attrStyleEnabled = m_configuration->attrStyleEnabled();
    parameters.shouldThrowExceptionForGlobalConstantRedeclaration = m_configuration->shouldThrowExceptionForGlobalConstantRedeclaration();
    parameters.crossOriginMode = process.crossOriginMode();
    parameters.isLockdownModeEnabled = process.lockdownMode() == WebProcessProxy::LockdownMode::Enabled;

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

    // Add any platform specific parameters
    platformInitializeWebProcess(process, parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient->getInjectedBundleInitializationUserData(*this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process.transformObjectsToHandles(injectedBundleInitializationUserData.get()));
    
    if (websiteDataStore)
        parameters.websiteDataStoreParameters = webProcessDataStoreParameters(process, *websiteDataStore);

    process.send(Messages::WebProcess::InitializeWebProcess(parameters), 0);

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    setMediaAccessibilityPreferences(process);
#endif

    if (m_automationSession)
        process.send(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()), 0);

    ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

    if (isPrewarmed == WebProcessProxy::IsPrewarmed::Yes) {
        ASSERT(!m_prewarmedProcess);
        m_prewarmedProcess = process;
    }

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    process.send(Messages::WebProcess::BacklightLevelDidChange(displayBrightness()), 0);
#endif

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
    process.sendAudioComponentRegistrations();
#endif

    registerDisplayConfigurationCallback();
    registerHighDynamicRangeChangeCallback();
}

void WebProcessPool::prewarmProcess()
{
    if (m_prewarmedProcess)
        return;

    if (WebProcessProxy::hasReachedProcessCountLimit())
        return;

    WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "prewarmProcess: Prewarming a WebProcess for performance");

    auto lockdownMode = WebProcessProxy::LockdownMode::Disabled;
    if (!m_processes.isEmpty())
        lockdownMode = m_processes.last()->lockdownMode();

    createNewWebProcess(nullptr, lockdownMode, WebProcessProxy::IsPrewarmed::Yes);
}

void WebProcessPool::enableProcessTermination()
{
    WEBPROCESSPOOL_RELEASE_LOG(Process, "enableProcessTermination:");
    m_processTerminationEnabled = true;
    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (auto& process : processes) {
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
        auto sampleLogFilePath = makeString("WebProcess", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()), "pid", process.processIdentifier());
        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite)) {
            sampleLogSandboxHandle = WTFMove(handleAndFilePath->first);
            sampleLogFilePath = WTFMove(handleAndFilePath->second);
        }
        
        process.send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    if (m_configuration->fullySynchronousModeIsAllowedForTesting())
        process.connection()->allowFullySynchronousModeForTesting();

    if (m_configuration->ignoreSynchronousMessagingTimeoutsForTesting())
        process.connection()->ignoreTimeoutsForTesting();

    m_connectionClient.didCreateConnection(this, process.webConnection());
}

void WebProcessPool::disconnectProcess(WebProcessProxy& process)
{
    ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == &process; }));

    if (m_prewarmedProcess == &process) {
        ASSERT(m_prewarmedProcess->isPrewarmed());
        m_prewarmedProcess = nullptr;
    } else if (process.isDummyProcessProxy()) {
        auto removedProcess = m_dummyProcessProxies.take(process.sessionID());
        ASSERT_UNUSED(removedProcess, removedProcess == &process);
    }

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    Ref<WebProcessProxy> protectedProcess(process);

    m_backForwardCache->removeEntriesForProcess(process);

    if (process.isRunningWorkers())
        removeFromRemoteWorkerProcesses(process);

    supplement<WebGeolocationManagerProxy>()->webProcessIsGoingAway(process);

    m_processes.removeFirstMatching([&](auto& item) { return item.ptr() == &process; });

#if ENABLE(GAMEPAD)
    if (m_processesUsingGamepads.contains(process))
        processStoppedUsingGamepads(process);
#endif

    removeProcessFromOriginCacheSet(process);
}

Ref<WebProcessProxy> WebProcessPool::processForRegistrableDomain(WebsiteDataStore& websiteDataStore, const RegistrableDomain& registrableDomain, WebProcessProxy::LockdownMode lockdownMode)
{
    if (!registrableDomain.isEmpty()) {
        if (auto process = webProcessCache().takeProcess(registrableDomain, websiteDataStore, lockdownMode)) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForRegistrableDomain: Using WebProcess from WebProcess cache (process=%p, PID=%i)", process.get(), process->processIdentifier());
            ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
            return process.releaseNonNull();
        }

        // Check if we have a suspended page for the given registrable domain and use its process if we do, for performance reasons.
        if (auto process = SuspendedPageProxy::findReusableSuspendedPageProcess(*this, registrableDomain, websiteDataStore, lockdownMode)) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForRegistrableDomain: Using WebProcess from a SuspendedPage (process=%p, PID=%i)", process.get(), process->processIdentifier());
            ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
            return process.releaseNonNull();
        }
    }

    if (auto process = tryTakePrewarmedProcess(websiteDataStore, lockdownMode)) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForRegistrableDomain: Using prewarmed process (process=%p, PID=%i)", process.get(), process->processIdentifier());
        if (!registrableDomain.isEmpty())
            tryPrewarmWithDomainInformation(*process, registrableDomain);
        ASSERT(m_processes.containsIf([&](auto& item) { return item.ptr() == process; }));
        return process.releaseNonNull();
    }

    if (usesSingleWebProcess()) {
#if PLATFORM(COCOA)
        bool mustMatchDataStore = WebKit::WebsiteDataStore::defaultDataStoreExists() && &websiteDataStore != WebKit::WebsiteDataStore::defaultDataStore().ptr();
#else
        bool mustMatchDataStore = false;
#endif

        for (auto& process : m_processes) {
            if (process.ptr() == m_prewarmedProcess.get() || process->isDummyProcessProxy())
                continue;
#if ENABLE(SERVICE_WORKER)
            if (process->isRunningServiceWorkers())
                continue;
#endif
            if (mustMatchDataStore && process->websiteDataStore() != &websiteDataStore)
                continue;
            return process;
        }
    }
    return createNewWebProcess(&websiteDataStore, lockdownMode);
}

UserContentControllerIdentifier WebProcessPool::userContentControllerIdentifierForRemoteWorkers()
{
    if (!m_userContentControllerForRemoteWorkers)
        m_userContentControllerForRemoteWorkers = WebUserContentControllerProxy::create();

    return m_userContentControllerForRemoteWorkers->identifier();
}

Ref<WebPageProxy> WebProcessPool::createWebPage(PageClient& pageClient, Ref<API::PageConfiguration>&& pageConfiguration)
{
    if (!pageConfiguration->pageGroup())
        pageConfiguration->setPageGroup(m_defaultPageGroup.ptr());
    if (!pageConfiguration->preferences())
        pageConfiguration->setPreferences(&pageConfiguration->pageGroup()->preferences());
    if (!pageConfiguration->userContentController())
        pageConfiguration->setUserContentController(&pageConfiguration->pageGroup()->userContentController());
    if (!pageConfiguration->visitedLinkStore())
        pageConfiguration->setVisitedLinkStore(m_visitedLinkStore.ptr());

    if (!pageConfiguration->websiteDataStore()) {
        // We try to avoid creating the default data store as long as possible.
        // But if there is an attempt to create a web page without any specified data store, then we have to create it.
        pageConfiguration->setWebsiteDataStore(WebKit::WebsiteDataStore::defaultDataStore().ptr());
    }

    RefPtr<WebProcessProxy> process;
    auto lockdownMode = pageConfiguration->lockdownModeEnabled() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;
    auto* relatedPage = pageConfiguration->relatedPage();
    if (relatedPage && !relatedPage->isClosed()) {
        // Sharing processes, e.g. when creating the page via window.open().
        process = &pageConfiguration->relatedPage()->ensureRunningProcess();
        // We do not support several WebsiteDataStores sharing a single process.
        ASSERT(process->isDummyProcessProxy() || pageConfiguration->websiteDataStore() == process->websiteDataStore());
        ASSERT(&pageConfiguration->relatedPage()->websiteDataStore() == pageConfiguration->websiteDataStore());
    } else if (!m_isDelayedWebProcessLaunchDisabled) {
        // In the common case, we delay process launch until something is actually loaded in the page.
        process = dummyProcessProxy(pageConfiguration->websiteDataStore()->sessionID());
        if (!process) {
            process = WebProcessProxy::create(*this, pageConfiguration->websiteDataStore(), lockdownMode, WebProcessProxy::IsPrewarmed::No, CrossOriginMode::Shared, WebProcessProxy::ShouldLaunchProcess::No);
            m_dummyProcessProxies.add(pageConfiguration->websiteDataStore()->sessionID(), *process);
            m_processes.append(*process);
        }
    } else
        process = processForRegistrableDomain(*pageConfiguration->websiteDataStore(), { }, lockdownMode);

    RefPtr<WebUserContentControllerProxy> userContentController = pageConfiguration->userContentController();
    
    ASSERT(process);

    auto page = process->createWebPage(pageClient, WTFMove(pageConfiguration));

    if (!m_remoteWorkerPreferences) {
        m_remoteWorkerPreferences = page->preferencesStore();
        for (auto& workerProcess : remoteWorkerProcesses())
            workerProcess.updateRemoteWorkerPreferencesStore(*m_remoteWorkerPreferences);
    }
    if (userContentController)
        m_userContentControllerForRemoteWorkers = userContentController;

    bool enableProcessSwapOnCrossSiteNavigation = page->preferences().processSwapOnCrossSiteNavigationEnabled();
#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isFirefox() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ProcessSwapOnCrossSiteNavigation))
        enableProcessSwapOnCrossSiteNavigation = false;
#endif

    bool wasProcessSwappingOnNavigationEnabled = m_configuration->processSwapsOnNavigation();
    m_configuration->setProcessSwapsOnNavigationFromExperimentalFeatures(enableProcessSwapOnCrossSiteNavigation);
    if (wasProcessSwappingOnNavigationEnabled != m_configuration->processSwapsOnNavigation())
        m_webProcessCache->updateCapacity(*this);

#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = GPUProcessProxy::singletonIfCreated()) {
        gpuProcess->updatePreferences(*process);
        gpuProcess->updateScreenPropertiesIfNeeded();
    }
#endif

    return page;
}

void WebProcessPool::updateRemoteWorkerUserAgent(const String& userAgent)
{
    if (m_remoteWorkerUserAgent == userAgent)
        return;
    m_remoteWorkerUserAgent = userAgent;
    for (auto& workerProcess : remoteWorkerProcesses())
        workerProcess.setRemoteWorkerUserAgent(m_remoteWorkerUserAgent);
}

void WebProcessPool::pageBeginUsingWebsiteDataStore(WebPageProxyIdentifier pageID, WebsiteDataStore& dataStore)
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    auto result = m_sessionToPageIDsMap.add(dataStore.sessionID(), HashSet<WebPageProxyIdentifier>()).iterator->value.add(pageID);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void WebProcessPool::pageEndUsingWebsiteDataStore(WebPageProxyIdentifier pageID, WebsiteDataStore& dataStore)
{
    RELEASE_ASSERT(RunLoop::isMain());
    auto sessionID = dataStore.sessionID();
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    auto iterator = m_sessionToPageIDsMap.find(sessionID);
    RELEASE_ASSERT(iterator != m_sessionToPageIDsMap.end());

    auto takenPageID = iterator->value.take(pageID);
    ASSERT_UNUSED(takenPageID, takenPageID == pageID);

    if (iterator->value.isEmpty()) {
        m_sessionToPageIDsMap.remove(iterator);

        if (sessionID.isEphemeral())
            m_webProcessCache->clearAllProcessesForSession(sessionID);
    }
}

bool WebProcessPool::hasPagesUsingWebsiteDataStore(WebsiteDataStore& dataStore) const
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionToPageIDsMap.isValidKey(dataStore.sessionID()));
    return m_sessionToPageIDsMap.contains(dataStore.sessionID());
}

DownloadProxy& WebProcessPool::download(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const ResourceRequest& request, const String& suggestedFilename)
{
    auto& downloadProxy = createDownloadProxy(dataStore, request, initiatingPage, { });

    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    if (initiatingPage) {
        initiatingPage->handleDownloadRequest(downloadProxy);
#if ENABLE(APP_BOUND_DOMAINS)
        isAppBound = initiatingPage->isTopFrameNavigatingToAppBoundDomain();
#endif
    }

    ResourceRequest updatedRequest(request);
    // Request's firstPartyForCookies will be used as Original URL of the download request.
    // We set the value to top level document's URL.
    if (initiatingPage) {
        URL initiatingPageURL = URL { initiatingPage->pageLoadState().url() };
        updatedRequest.setFirstPartyForCookies(initiatingPageURL);
        updatedRequest.setIsSameSite(areRegistrableDomainsEqual(initiatingPageURL, request.url()));
        if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
            updatedRequest.setHTTPUserAgent(initiatingPage->userAgentForURL(request.url()));
    } else {
        updatedRequest.setFirstPartyForCookies(URL());
        updatedRequest.setIsSameSite(false);
        if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
            updatedRequest.setHTTPUserAgent(WebPageProxy::standardUserAgent());
    }
    updatedRequest.setIsTopSite(false);
    dataStore.networkProcess().send(Messages::NetworkProcess::DownloadRequest(dataStore.sessionID(), downloadProxy.downloadID(), updatedRequest, isAppBound, suggestedFilename), 0);

    return downloadProxy;
}

DownloadProxy& WebProcessPool::resumeDownload(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path, CallDownloadDidStart callDownloadDidStart)
{
    auto& downloadProxy = createDownloadProxy(dataStore, ResourceRequest(), initiatingPage, { });

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty()) {
        if (auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite))
            sandboxExtensionHandle = WTFMove(*handle);
    }

    dataStore.networkProcess().send(Messages::NetworkProcess::ResumeDownload(dataStore.sessionID(), downloadProxy.downloadID(), resumeData.dataReference(), path, sandboxExtensionHandle, callDownloadDidStart), 0);
    return downloadProxy;
}

void WebProcessPool::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    for (auto& process : m_processes) {
        // FIXME: Return early if the message body contains any references to WKPageRefs/WKFrameRefs etc. since they're local to a process.
        process->send(Messages::WebProcess::HandleInjectedBundleMessage(messageName, UserData(process->transformObjectsToHandles(messageBody).get())), 0);
    }
}

void WebProcessPool::didReachGoodTimeToPrewarm()
{
    if (!configuration().isAutomaticProcessWarmingEnabled() || !configuration().processSwapsOnNavigation() || usesSingleWebProcess())
        return;

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_prewarmedProcess)
            WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "didReachGoodTimeToPrewarm: Not automatically prewarming a WebProcess due to memory pressure");
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

    if (m_prewarmedProcess)
        m_prewarmedProcess->shutDown();
    ASSERT(!m_prewarmedProcess);
}

ProcessID WebProcessPool::prewarmedProcessIdentifier()
{
    return m_prewarmedProcess ? m_prewarmedProcess->processIdentifier() : 0;
}

void WebProcessPool::activePagesOriginsInWebProcessForTesting(ProcessID pid, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    for (auto& process : m_processes) {
        if (process->processIdentifier() == pid)
            return process->activePagesDomainsForTesting(WTFMove(completionHandler));
    }
    completionHandler({ });
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
    WebsiteDataStore::forEachWebsiteDataStore([urlScheme] (WebsiteDataStore& dataStore) {
        if (auto* networkProcess = dataStore.networkProcessIfExists())
            networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsSecure(urlScheme), 0);
    });
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
    WebsiteDataStore::forEachWebsiteDataStore([urlScheme] (WebsiteDataStore& dataStore) {
        if (auto* networkProcess = dataStore.networkProcessIfExists())
            networkProcess->send(Messages::NetworkProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme), 0);
    });
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
    WebsiteDataStore::forEachWebsiteDataStore([urlScheme] (WebsiteDataStore& dataStore) {
        dataStore.networkProcess().send(Messages::NetworkProcess::RegisterURLSchemeAsLocal(urlScheme), 0);
    });
}

void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsNoAccess(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
    WebsiteDataStore::forEachWebsiteDataStore([urlScheme] (WebsiteDataStore& dataStore) {
        dataStore.networkProcess().send(Messages::NetworkProcess::RegisterURLSchemeAsNoAccess(urlScheme), 0);
    });
}

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
    for (auto networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->registerSchemeForLegacyCustomProtocol(urlScheme);
}

void WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    InitializeWebKit2();
    globalURLSchemesWithCustomProtocolHandlers().remove(urlScheme);
    for (auto networkProcess : NetworkProcessProxy::allNetworkProcesses())
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

    m_backForwardCache->setCapacity(backForwardCacheCapacity);
}

void WebProcessPool::setCacheModel(CacheModel cacheModel)
{
    updateBackForwardCacheCapacity();

    sendToAllProcesses(Messages::WebProcess::SetCacheModel(cacheModel));

    WebsiteDataStore::forEachWebsiteDataStore([cacheModel] (WebsiteDataStore& dataStore) {
        dataStore.networkProcess().send(Messages::NetworkProcess::SetCacheModel(cacheModel), 0);
    });
}

void WebProcessPool::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    updateBackForwardCacheCapacity();

    WebsiteDataStore::forEachWebsiteDataStore([cacheModel] (WebsiteDataStore& dataStore) {
        dataStore.networkProcess().sendSync(Messages::NetworkProcess::SetCacheModelSynchronouslyForTesting(cacheModel), 0);
    });
}

void WebProcessPool::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

DownloadProxy& WebProcessPool::createDownloadProxy(WebsiteDataStore& dataStore, const ResourceRequest& request, WebPageProxy* originatingPage, const FrameInfoData& frameInfo)
{
    return dataStore.networkProcess().createDownloadProxy(dataStore, *this, request, frameInfo, originatingPage);
}

void WebProcessPool::addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebProcessPool::addMessageReceiver(IPC::ReceiverName messageReceiverName, UInt128 destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebProcessPool::removeMessageReceiver(IPC::ReceiverName messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void WebProcessPool::removeMessageReceiver(IPC::ReceiverName messageReceiverName, UInt128 destinationID)
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
    
    // For WebProcess
    SandboxExtension::Handle sampleLogSandboxHandle;    
    WallTime now = WallTime::now();
    auto sampleLogFilePath = makeString("WebProcess", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()));
    if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite)) {
        sampleLogSandboxHandle = WTFMove(handleAndFilePath->first);
        sampleLogFilePath = WTFMove(handleAndFilePath->second);
    }
    
    sendToAllProcesses(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, interval));
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

void WebProcessPool::terminateAllWebContentProcesses()
{
    WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "terminateAllWebContentProcesses");
    Vector<Ref<WebProcessProxy>> processes = m_processes;
    for (auto& process : processes)
        process->terminate();
}

void WebProcessPool::terminateServiceWorkers()
{
#if ENABLE(SERVICE_WORKER)
    Ref protectedThis { *this };
    Vector<Ref<WebProcessProxy>> serviceWorkerProcesses;
    remoteWorkerProcesses().forEach([&](auto& process) {
        if (process.isRunningServiceWorkers())
            serviceWorkerProcesses.append(process);
    });
    for (auto& serviceWorkerProcess : serviceWorkerProcesses)
        serviceWorkerProcess->disableRemoteWorkers(RemoteWorkerType::ServiceWorker);
#endif
}

void WebProcessPool::updateAutomationCapabilities() const
{
#if ENABLE(REMOTE_INSPECTOR)
    Inspector::RemoteInspector::singleton().clientCapabilitiesDidChange();
#endif
}

void WebProcessPool::setAutomationSession(RefPtr<WebAutomationSession>&& automationSession)
{
    if (m_automationSession)
        m_automationSession->setProcessPool(nullptr);
    
    m_automationSession = WTFMove(automationSession);

#if ENABLE(REMOTE_INSPECTOR)
    if (m_automationSession) {
        m_automationSession->init();
        m_automationSession->setProcessPool(this);

        sendToAllProcesses(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()));
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

static WebProcessProxy* webProcessProxyFromConnection(IPC::Connection& connection, const Vector<Ref<WebProcessProxy>>& processes)
{
    for (auto& process : processes) {
        if (process->hasConnection(connection))
            return process.ptr();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebProcessPool::handleMessage(IPC::Connection& connection, const String& messageName, const WebKit::UserData& messageBody)
{
    auto* webProcessProxy = webProcessProxyFromConnection(connection, m_processes);
    if (!webProcessProxy)
        return;
    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get());
}

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    auto* webProcessProxy = webProcessProxyFromConnection(connection, m_processes);
    if (!webProcessProxy)
        return completionHandler({ });

    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), [webProcessProxy = Ref { *webProcessProxy }, completionHandler = WTFMove(completionHandler)] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(webProcessProxy->transformObjectsToHandles(returnData.get())));
    });
}

#if ENABLE(GAMEPAD)

void WebProcessPool::startedUsingGamepads(IPC::Connection& connection)
{
    auto* proxy = webProcessProxyFromConnection(connection, m_processes);
    if (!proxy)
        return;

    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.computesEmpty();

    ASSERT(!m_processesUsingGamepads.contains(*proxy));
    m_processesUsingGamepads.add(*proxy);

    if (!wereAnyProcessesUsingGamepads)
        UIGamepadProvider::singleton().processPoolStartedUsingGamepads(*this);

    proxy->send(Messages::WebProcess::SetInitialGamepads(UIGamepadProvider::singleton().snapshotGamepads()), 0);
}

void WebProcessPool::stoppedUsingGamepads(IPC::Connection& connection, CompletionHandler<void()>&& completionHandler)
{
    CompletionHandlerCallingScope callCompletionHandlerOnExit(WTFMove(completionHandler));
    auto* proxy = webProcessProxyFromConnection(connection, m_processes);
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
    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.computesEmpty();

    ASSERT(m_processesUsingGamepads.contains(process));
    m_processesUsingGamepads.remove(process);

    if (wereAnyProcessesUsingGamepads && m_processesUsingGamepads.computesEmpty())
        UIGamepadProvider::singleton().processPoolStoppedUsingGamepads(*this);
}

void WebProcessPool::gamepadConnected(const UIGamepad& gamepad, EventMakesGamepadsVisible eventVisibility)
{
    for (auto& process : m_processesUsingGamepads)
        process.send(Messages::WebProcess::GamepadConnected(gamepad.gamepadData(), eventVisibility), 0);
}

void WebProcessPool::gamepadDisconnected(const UIGamepad& gamepad)
{
    for (auto& process : m_processesUsingGamepads)
        process.send(Messages::WebProcess::GamepadDisconnected(gamepad.index()), 0);
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
            if (API::String* font = array->at<API::String>(i))
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
    sendToAllProcesses(Messages::WebProcess::SetHiddenPageDOMTimerThrottlingIncreaseLimit(limitInMilliseconds));
}

void WebProcessPool::reportWebContentCPUTime(Seconds cpuTime, uint64_t activityState)
{
#if PLATFORM(MAC)
    if (m_perActivityStateCPUUsageSampler)
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
    if (auto& networkProcess = NetworkProcessProxy::defaultNetworkProcess())
        networkProcess->updateProcessAssertion();

#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->updateProcessAssertion();
#endif

    // Check on next run loop since the web process proxy tokens are probably being updated.
    callOnMainRunLoop([] {
        remoteWorkerProcesses().forEach([](auto& workerProcess) {
#if ENABLE(SERVICE_WORKER)
            if (workerProcess.isRunningServiceWorkers())
                workerProcess.updateRemoteWorkerProcessAssertion(RemoteWorkerType::ServiceWorker);
#endif
            if (workerProcess.isRunningSharedWorkers())
                workerProcess.updateRemoteWorkerProcessAssertion(RemoteWorkerType::SharedWorker);
        });
    });
}

bool WebProcessPool::isServiceWorkerPageID(WebPageProxyIdentifier pageID) const
{
#if ENABLE(SERVICE_WORKER)
    // FIXME: This is inefficient.
    return WTF::anyOf(remoteWorkerProcesses(), [pageID](auto& process) {
        return process.hasServiceWorkerPageProxy(pageID);
    });
#endif
    return false;
}

void WebProcessPool::addProcessToOriginCacheSet(WebProcessProxy& process, const URL& url)
{
    auto registrableDomain = WebCore::RegistrableDomain { url };
    auto result = m_swappedProcessesPerRegistrableDomain.add(registrableDomain, &process);
    if (!result.isNewEntry)
        result.iterator->value = &process;

    LOG(ProcessSwapping, "(ProcessSwapping) Registrable domain %s just saved a cached process with pid %i", registrableDomain.string().utf8().data(), process.processIdentifier());
    if (!result.isNewEntry)
        LOG(ProcessSwapping, "(ProcessSwapping) Note: It already had one saved");
}

void WebProcessPool::removeProcessFromOriginCacheSet(WebProcessProxy& process)
{
    LOG(ProcessSwapping, "(ProcessSwapping) Removing process with pid %i from the origin cache set", process.processIdentifier());

    // FIXME: This can be very inefficient as the number of remembered origins and processes grows
    m_swappedProcessesPerRegistrableDomain.removeIf([&](auto& entry) {
        return entry.value == &process;
    });
}

void WebProcessPool::processForNavigation(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient processSwapRequestedByClient, WebProcessProxy::LockdownMode lockdownMode, const FrameInfoData& frameInfo, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&& completionHandler)
{
    processForNavigationInternal(page, navigation, sourceProcess.copyRef(), sourceURL, processSwapRequestedByClient, lockdownMode, frameInfo, WTFMove(dataStore), [this, page = Ref { page }, navigation = Ref { navigation }, sourceProcess = sourceProcess.copyRef(), sourceURL, processSwapRequestedByClient, completionHandler = WTFMove(completionHandler)](Ref<WebProcessProxy>&& process, SuspendedPageProxy* suspendedPage, const String& reason) mutable {
        // We are process-swapping so automatic process prewarming would be beneficial if the client has not explicitly enabled / disabled it.
        bool doingAnAutomaticProcessSwap = processSwapRequestedByClient == ProcessSwapRequestedByClient::No && process.ptr() != sourceProcess.ptr();
        if (doingAnAutomaticProcessSwap && !configuration().wasAutomaticProcessWarmingSetByClient() && !configuration().clientWouldBenefitFromAutomaticProcessPrewarming()) {
            WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "processForNavigation: Automatically turning on process prewarming because the client would benefit from it");
            configuration().setClientWouldBenefitFromAutomaticProcessPrewarming(true);
        }

        if (m_configuration->alwaysKeepAndReuseSwappedProcesses() && process.ptr() != sourceProcess.ptr()) {
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                WTFLogAlways("WARNING: The option to always keep swapped web processes alive is active. This is meant for debugging and testing only.");
            });

            addProcessToOriginCacheSet(sourceProcess, sourceURL);

            LOG(ProcessSwapping, "(ProcessSwapping) Navigating from %s to %s, keeping around old process. Now holding on to old processes for %u origins.", sourceURL.string().utf8().data(), navigation->currentRequest().url().string().utf8().data(), m_swappedProcessesPerRegistrableDomain.size());
        }

        auto processIdentifier = process->coreProcessIdentifier();
        page->websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(processIdentifier, RegistrableDomain(navigation->currentRequest().url())), [completionHandler = WTFMove(completionHandler), process = WTFMove(process), suspendedPage = WTFMove(suspendedPage), reason] () mutable {
            completionHandler(WTFMove(process), suspendedPage, reason);
        });
    });
}

void WebProcessPool::processForNavigationInternal(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& pageSourceURL, ProcessSwapRequestedByClient processSwapRequestedByClient, WebProcessProxy::LockdownMode lockdownMode, const FrameInfoData& frameInfo, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&& completionHandler)
{
    auto& targetURL = navigation.currentRequest().url();
    auto targetRegistrableDomain = WebCore::RegistrableDomain { targetURL };

    auto createNewProcess = [this, protectedThis = Ref { *this }, targetRegistrableDomain, dataStore, lockdownMode] () -> Ref<WebProcessProxy> {
        return processForRegistrableDomain(dataStore, targetRegistrableDomain, lockdownMode);
    };

    if (usesSingleWebProcess())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Single WebProcess mode is enabled"_s);

    if (sourceProcess->lockdownMode() != lockdownMode)
        return completionHandler(createNewProcess(), nullptr, "Process swap due to Lockdown mode change"_s);

    if (processSwapRequestedByClient == ProcessSwapRequestedByClient::Yes)
        return completionHandler(createNewProcess(), nullptr, "Process swap was requested by the client"_s);

    if (!m_configuration->processSwapsOnNavigation())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Feature is disabled"_s);

    if (m_automationSession)
        return completionHandler(WTFMove(sourceProcess), nullptr, "An automation session is active"_s);

    if (!sourceProcess->hasCommittedAnyProvisionalLoads()) {
        tryPrewarmWithDomainInformation(sourceProcess, targetRegistrableDomain);
        return completionHandler(WTFMove(sourceProcess), nullptr, "Process has not yet committed any provisional loads"_s);
    }

    // FIXME: We should support process swap when a window has been opened via window.open() without 'noopener'.
    // The issue is that the opener has a handle to the WindowProxy.
    if (navigation.openedByDOMWithOpener() && !m_configuration->processSwapsOnWindowOpenWithOpener())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Browsing context been opened by DOM without 'noopener'"_s);

    // FIXME: We should support process swap when a window has opened other windows via window.open.
    if (navigation.hasOpenedFrames())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Browsing context has opened other windows"_s);

    if (auto* targetItem = navigation.targetItem()) {
        if (auto* suspendedPage = targetItem->suspendedPage()) {
            return suspendedPage->waitUntilReadyToUnsuspend([createNewProcess = WTFMove(createNewProcess), completionHandler = WTFMove(completionHandler)](SuspendedPageProxy* suspendedPage) mutable {
                if (!suspendedPage)
                    return completionHandler(createNewProcess(), nullptr, "Using new process because target back/forward item's suspended page is not reusable"_s);
                Ref<WebProcessProxy> process = suspendedPage->process();
                completionHandler(WTFMove(process), suspendedPage, "Using target back/forward item's process and suspended page"_s);
            });
        }

        if (auto process = WebProcessProxy::processForIdentifier(targetItem->lastProcessIdentifier())) {
            if (process->state() != WebProcessProxy::State::Terminated) {
                // Make sure we remove the process from the cache if it is in there since we're about to use it.
                if (process->isInProcessCache()) {
                    webProcessCache().removeProcess(*process, WebProcessCache::ShouldShutDownProcess::No);
                    ASSERT(!process->isInProcessCache());
                }

                return completionHandler(process.releaseNonNull(), nullptr, "Using target back/forward item's process"_s);
            }
        }
    }

    // If it is the first navigation in a DOM popup and there is no opener, then force a process swap no matter what since
    // popup windows are originally created in their opener's process.
    // Note that we currently do not process swap if the window popup has a name. In theory, we should be able to swap in this case too
    // but we would need to transfer over the name to the new process. At this point, it is not clear it is worth the extra complexity.
    if (page.openedByDOM() && !navigation.openedByDOMWithOpener() && !page.hasCommittedAnyProvisionalLoads() && frameInfo.frameName.isEmpty() && !targetURL.protocolIsBlob())
        return completionHandler(createNewProcess(), nullptr, "Process swap because this is a first navigation in a DOM popup without opener"_s);

    if (navigation.treatAsSameOriginNavigation())
        return completionHandler(WTFMove(sourceProcess), nullptr, "The treatAsSameOriginNavigation flag is set"_s);

    URL sourceURL;
    if (page.isPageOpenedByDOMShowingInitialEmptyDocument() && !navigation.requesterOrigin().isNull())
        sourceURL = URL { navigation.requesterOrigin().toString() };
    else
        sourceURL = pageSourceURL;

    if (sourceURL.isEmpty() && page.configuration().relatedPage()) {
        sourceURL = URL { page.configuration().relatedPage()->pageLoadState().url() };
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForNavigationInternal: Using related page's URL as source URL for process swap decision (page=%p)", page.configuration().relatedPage());
    }

    // For non-HTTP(s) URLs, we only swap when navigating to a new scheme, unless processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol is set.
    if (!m_configuration->processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol() && !sourceURL.protocolIsInHTTPFamily() && sourceURL.protocol() == targetURL.protocol())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Navigation within the same non-HTTP(s) protocol"_s);

    if (!sourceURL.isValid() || !targetURL.isValid() || sourceURL.isEmpty() || sourceURL.protocolIsAbout() || targetRegistrableDomain.matches(sourceURL))
        return completionHandler(WTFMove(sourceProcess), nullptr, "Navigation is same-site"_s);

    String reason = "Navigation is cross-site"_s;
    
    if (m_configuration->alwaysKeepAndReuseSwappedProcesses()) {
        LOG(ProcessSwapping, "(ProcessSwapping) Considering re-use of a previously cached process for domain %s", targetRegistrableDomain.string().utf8().data());

        if (auto* process = m_swappedProcessesPerRegistrableDomain.get(targetRegistrableDomain)) {
            if (process->websiteDataStore() == dataStore.ptr()) {
                LOG(ProcessSwapping, "(ProcessSwapping) Reusing a previously cached process with pid %i to continue navigation to URL %s", process->processIdentifier(), targetURL.string().utf8().data());

                return completionHandler(*process, nullptr, reason);
            }
        }
    }

    return completionHandler(createNewProcess(), nullptr, reason);
}

void WebProcessPool::addMockMediaDevice(const MockMediaDevice& device)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::addDevice(device);
    sendToAllProcesses(Messages::WebProcess::AddMockMediaDevice { device });
#if ENABLE(GPU_PROCESS)
    ensureGPUProcess().addMockMediaDevice(device);
#endif
#endif
}

void WebProcessPool::clearMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::setDevices({ });
    sendToAllProcesses(Messages::WebProcess::ClearMockMediaDevices { });
#if ENABLE(GPU_PROCESS)
    ensureGPUProcess().clearMockMediaDevices();
#endif
#endif
}

void WebProcessPool::removeMockMediaDevice(const String& persistentId)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::removeDevice(persistentId);
    sendToAllProcesses(Messages::WebProcess::RemoveMockMediaDevice { persistentId });
#if ENABLE(GPU_PROCESS)
    ensureGPUProcess().removeMockMediaDevice(persistentId);
#endif
#endif
}


void WebProcessPool::setMockMediaDeviceIsEphemeral(const String& persistentId, bool isEphemeral)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::setDeviceIsEphemeral(persistentId, isEphemeral);
    sendToAllProcesses(Messages::WebProcess::SetMockMediaDeviceIsEphemeral { persistentId, isEphemeral });
#if ENABLE(GPU_PROCESS)
    ensureGPUProcess().setMockMediaDeviceIsEphemeral(persistentId, isEphemeral);
#endif
#endif
}

void WebProcessPool::resetMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::resetDevices();
    sendToAllProcesses(Messages::WebProcess::ResetMockMediaDevices { });
#if ENABLE(GPU_PROCESS)
    ensureGPUProcess().resetMockMediaDevices();
#endif
#endif
}

void WebProcessPool::sendDisplayConfigurationChangedMessageForTesting()
{
#if PLATFORM(MAC)
    auto display = CGSMainDisplayID();
    CGDisplayChangeSummaryFlags flags = kCGDisplaySetModeFlag | kCGDisplayDesktopShapeChangedFlag;
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, kCGDisplayBeginConfigurationFlag));
        processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, flags));
        if (auto gpuProcess = processPool->gpuProcess())
            gpuProcess->displayConfigurationChanged(display, flags);
    }
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

#if ENABLE(TRACKING_PREVENTION)
void WebProcessPool::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    sendToAllProcesses(Messages::WebProcess::SetDomainsWithUserInteraction(domains));
    m_domainsWithUserInteraction = WTFMove(domains);
}

void WebProcessPool::setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& process : processes())
        process->sendWithAsyncReply(Messages::WebProcess::SetDomainsWithCrossPageStorageAccess(domains), [callbackAggregator] { });

    for (auto& topDomain : domains.keys())
        m_domainsWithCrossPageStorageAccessQuirk.add(topDomain, domains.get(topDomain));
}

void WebProcessPool::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& process : processes())
        process->sendWithAsyncReply(Messages::WebProcess::SeedResourceLoadStatisticsForTesting(firstPartyDomain, thirdPartyDomain, shouldScheduleNotification), [callbackAggregator] { });
}

void WebProcessPool::sendResourceLoadStatisticsDataImmediately(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& process : processes()) {
        if (!process->pageCount())
            continue;

        process->sendWithAsyncReply(Messages::WebProcess::SendResourceLoadStatisticsDataImmediately(), [callbackAggregator] { });
    }
}
#endif

WebProcessWithAudibleMediaToken WebProcessPool::webProcessWithAudibleMediaToken() const
{
    return m_webProcessWithAudibleMediaCounter.count();
}

void WebProcessPool::updateAudibleMediaAssertions()
{
    if (!m_webProcessWithAudibleMediaCounter.value()) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: The number of processes playing audible media now zero. Releasing UI process assertion.");
        m_audibleMediaActivity = std::nullopt;
        return;
    }

    if (m_audibleMediaActivity)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: The number of processes playing audible media is now greater than zero. Taking UI process assertion.");
    m_audibleMediaActivity = AudibleMediaActivity {
        ProcessAssertion::create(getCurrentProcessID(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback)
#if ENABLE(GPU_PROCESS)
        , gpuProcess() ? RefPtr<ProcessAssertion> { ProcessAssertion::create(gpuProcess()->processIdentifier(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback) } : nullptr
#endif
    };
}

void WebProcessPool::setUseSeparateServiceWorkerProcess(bool useSeparateServiceWorkerProcess)
{
    if (s_useSeparateServiceWorkerProcess == useSeparateServiceWorkerProcess)
        return;

    WEBPROCESSPOOL_RELEASE_LOG_STATIC(ServiceWorker, "setUseSeparateServiceWorkerProcess: (useSeparateServiceWorkerProcess=%d)", useSeparateServiceWorkerProcess);

    s_useSeparateServiceWorkerProcess = useSeparateServiceWorkerProcess;
    for (auto& processPool : allProcessPools())
        processPool->terminateServiceWorkers();
}

bool WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion()
{
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        if (processPool->shouldTakeUIBackgroundAssertion())
            return true;
    }
    return false;
}

void WebProcessPool::forEachProcessForSession(PAL::SessionID sessionID, const Function<void(WebProcessProxy&)>& apply)
{
    for (auto& process : m_processes) {
#if ENABLE(WEBCONTENT_CRASH_TESTING)
        if (process->isCrashyProcess())
            continue;
#endif
        if (process->isPrewarmed() || process->sessionID() != sessionID)
            continue;
        apply(process.get());
    }
}

#if ENABLE(SERVICE_WORKER)
size_t WebProcessPool::serviceWorkerProxiesCount() const
{
    unsigned count = 0;
    remoteWorkerProcesses().forEach([&](auto& process) {
        if (process.isRunningServiceWorkers())
            ++count;
    });
    return count;
}

bool WebProcessPool::hasServiceWorkerForegroundActivityForTesting() const
{
    return WTF::anyOf(remoteWorkerProcesses(), [](auto& process) {
        return process.hasServiceWorkerForegroundActivityForTesting();
    });
}

bool WebProcessPool::hasServiceWorkerBackgroundActivityForTesting() const
{
    return WTF::anyOf(remoteWorkerProcesses(), [](auto& process) {
        return process.hasServiceWorkerBackgroundActivityForTesting();
    });
}
#endif

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

} // namespace WebKit
