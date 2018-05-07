/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "ChildProcessMessages.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "GamepadData.h"
#include "HighPerformanceGraphicsUsageSampler.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerMessages.h"
#endif
#include "LogInitialization.h"
#include "Logging.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "SandboxExtension.h"
#include "ServiceWorkerProcessProxy.h"
#include "StatisticsData.h"
#include "StorageProcessCreationParameters.h"
#include "StorageProcessMessages.h"
#include "TextChecker.h"
#include "UIGamepad.h"
#include "UIGamepadProvider.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebCertificateInfo.h"
#include "WebContextSupplement.h"
#include "WebCookieManagerProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebGeolocationManagerProxy.h"
#include "WebKit2Initialize.h"
#include "WebMemorySampler.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include "WebPreferencesKeys.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <JavaScriptCore/JSCInlines.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/Process.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/URLParser.h>
#include <pal/SessionID.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ServicesController.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if OS(LINUX)
#include "MemoryPressureMonitor.h"
#endif

#if PLATFORM(WAYLAND)
#include "WaylandCompositor.h"
#include <WebCore/PlatformDisplay.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, processPoolCounter, ("WebProcessPool"));

static const Seconds serviceWorkerTerminationDelay { 5_s };

static uint64_t generateListenerIdentifier()
{
    static uint64_t nextIdentifier = 1;
    return nextIdentifier++;
}

static HashMap<uint64_t, Function<void(WebProcessPool&)>>& processPoolCreationListenerFunctionMap()
{
    static NeverDestroyed<HashMap<uint64_t, Function<void(WebProcessPool&)>>> map;
    return map;
}

uint64_t WebProcessPool::registerProcessPoolCreationListener(Function<void(WebProcessPool&)>&& function)
{
    ASSERT(function);

    auto identifier = generateListenerIdentifier();
    processPoolCreationListenerFunctionMap().set(identifier, WTFMove(function));
    return identifier;
}

void WebProcessPool::unregisterProcessPoolCreationListener(uint64_t identifier)
{
    processPoolCreationListenerFunctionMap().remove(identifier);
}

Ref<WebProcessPool> WebProcessPool::create(API::ProcessPoolConfiguration& configuration)
{
    InitializeWebKit2();
    return adoptRef(*new WebProcessPool(configuration));
}

void WebProcessPool::notifyThisWebProcessPoolWasCreated()
{
    auto& listenerMap = processPoolCreationListenerFunctionMap();

    Vector<uint64_t> identifiers;
    identifiers.reserveInitialCapacity(listenerMap.size());
    for (auto identifier : listenerMap.keys())
        identifiers.uncheckedAppend(identifier);

    for (auto identifier : identifiers) {
        auto iterator = listenerMap.find(identifier);
        if (iterator == listenerMap.end())
            continue;

        // To make sure the Function object stays alive until after the function call has been made,
        // we temporarily move it out of the map.
        // This protects it from the Function calling unregisterProcessPoolCreationListener thereby
        // removing itself from the map of listeners.
        // If the identifier still exists in the map later, we move it back in.
        Function<void(WebProcessPool&)> function = WTFMove(iterator->value);
        function(*this);

        iterator = listenerMap.find(identifier);
        if (iterator != listenerMap.end()) {
            ASSERT(!iterator->value);
            iterator->value = WTFMove(function);
        }
    }
}

static Vector<WebProcessPool*>& processPools()
{
    static NeverDestroyed<Vector<WebProcessPool*>> processPools;
    return processPools;
}

const Vector<WebProcessPool*>& WebProcessPool::allProcessPools()
{
    return processPools();
}

static WebsiteDataStore::Configuration legacyWebsiteDataStoreConfiguration(API::ProcessPoolConfiguration& processPoolConfiguration)
{
    WebsiteDataStore::Configuration configuration;

    configuration.cacheStorageDirectory = API::WebsiteDataStore::defaultCacheStorageDirectory();
    configuration.serviceWorkerRegistrationDirectory = API::WebsiteDataStore::defaultServiceWorkerRegistrationDirectory();
    configuration.localStorageDirectory = processPoolConfiguration.localStorageDirectory();
    configuration.webSQLDatabaseDirectory = processPoolConfiguration.webSQLDatabaseDirectory();
    configuration.applicationCacheDirectory = processPoolConfiguration.applicationCacheDirectory();
    configuration.applicationCacheFlatFileSubdirectoryName = processPoolConfiguration.applicationCacheFlatFileSubdirectoryName();
    configuration.mediaCacheDirectory = processPoolConfiguration.mediaCacheDirectory();
    configuration.mediaKeysStorageDirectory = processPoolConfiguration.mediaKeysStorageDirectory();
    configuration.resourceLoadStatisticsDirectory = processPoolConfiguration.resourceLoadStatisticsDirectory();
    configuration.networkCacheDirectory = processPoolConfiguration.diskCacheDirectory();
    configuration.javaScriptConfigurationDirectory = processPoolConfiguration.javaScriptConfigurationDirectory();

    return configuration;
}

static HashSet<String, ASCIICaseInsensitiveHash>& globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> set;
    return set;
}

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundleClient(std::make_unique<API::InjectedBundleClient>())
    , m_automationClient(std::make_unique<API::AutomationClient>())
    , m_downloadClient(std::make_unique<API::DownloadClient>())
    , m_historyClient(std::make_unique<API::LegacyContextHistoryClient>())
    , m_customProtocolManagerClient(std::make_unique<API::CustomProtocolManagerClient>())
    , m_visitedLinkStore(VisitedLinkStore::create())
#if PLATFORM(MAC)
    , m_highPerformanceGraphicsUsageSampler(std::make_unique<HighPerformanceGraphicsUsageSampler>(*this))
    , m_perActivityStateCPUUsageSampler(std::make_unique<PerActivityStateCPUUsageSampler>(*this))
#endif
    , m_alwaysRunsAtBackgroundPriority(m_configuration->alwaysRunsAtBackgroundPriority())
    , m_shouldTakeUIBackgroundAssertion(m_configuration->shouldTakeUIBackgroundAssertion())
    , m_userObservablePageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_hiddenPageThrottlingAutoIncreasesCounter([this](RefCounterEvent) { m_hiddenPageThrottlingTimer.startOneShot(0_s); })
    , m_hiddenPageThrottlingTimer(RunLoop::main(), this, &WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit)
    , m_serviceWorkerProcessesTerminationTimer(RunLoop::main(), this, &WebProcessPool::terminateServiceWorkerProcesses)
#if PLATFORM(IOS)
    , m_foregroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backgroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
#endif
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        WTF::setProcessPrivileges(allPrivileges());
        WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
        Process::setIdentifier(generateObjectIdentifier<WebCore::ProcessIdentifierType>());
    });

    if (m_configuration->shouldHaveLegacyDataStore())
        m_websiteDataStore = API::WebsiteDataStore::createLegacy(legacyWebsiteDataStoreConfiguration(m_configuration));

    if (!m_websiteDataStore && API::WebsiteDataStore::defaultDataStoreExists())
        m_websiteDataStore = API::WebsiteDataStore::defaultDataStore();

    for (auto& scheme : m_configuration->alwaysRevalidatedURLSchemes())
        m_schemesToRegisterAsAlwaysRevalidated.add(scheme);

    for (const auto& urlScheme : m_configuration->cachePartitionedURLSchemes())
        m_schemesToRegisterAsCachePartitioned.add(urlScheme);

    platformInitialize();

    addMessageReceiver(Messages::WebProcessPool::messageReceiverName(), *this);

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    addSupplement<WebCookieManagerProxy>();
    addSupplement<WebGeolocationManagerProxy>();
    addSupplement<WebNotificationManagerProxy>();
#if ENABLE(MEDIA_SESSION)
    addSupplement<WebMediaSessionFocusManager>();
#endif

    processPools().append(this);

    addLanguageChangeObserver(this, languageChanged);

    resolvePathsForSandboxExtensions();

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebCore::initializeLogChannelsIfNecessary();
    WebKit::initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef NDEBUG
    processPoolCounter.increment();
#endif

    notifyThisWebProcessPoolWasCreated();
}

WebProcessPool::~WebProcessPool()
{
    bool removed = processPools().removeFirst(this);
    ASSERT_UNUSED(removed, removed);

    removeLanguageChangeObserver(this);

    m_messageReceiverMap.invalidate();

    for (auto& supplement : m_supplements.values()) {
        supplement->processPoolDestroyed();
        supplement->clearProcessPool();
    }

    invalidateCallbackMap(m_dictionaryCallbacks, CallbackBase::Error::OwnerWasInvalidated);

    platformInvalidateContext();

#ifndef NDEBUG
    processPoolCounter.decrement();
#endif

    if (m_networkProcess)
        m_networkProcess->shutDownProcess();

#if ENABLE(GAMEPAD)
    if (!m_processesUsingGamepads.isEmpty())
        UIGamepadProvider::singleton().processPoolStoppedUsingGamepads(*this);
#endif
}

void WebProcessPool::initializeClient(const WKContextClientBase* client)
{
    m_client.initialize(client);
}

void WebProcessPool::setInjectedBundleClient(std::unique_ptr<API::InjectedBundleClient>&& client)
{
    if (!client)
        m_injectedBundleClient = std::make_unique<API::InjectedBundleClient>();
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
        m_historyClient = std::make_unique<API::LegacyContextHistoryClient>();
    else
        m_historyClient = WTFMove(historyClient);
}

void WebProcessPool::setDownloadClient(std::unique_ptr<API::DownloadClient>&& downloadClient)
{
    if (!downloadClient)
        m_downloadClient = std::make_unique<API::DownloadClient>();
    else
        m_downloadClient = WTFMove(downloadClient);
}

void WebProcessPool::setAutomationClient(std::unique_ptr<API::AutomationClient>&& automationClient)
{
    if (!automationClient)
        m_automationClient = std::make_unique<API::AutomationClient>();
    else
        m_automationClient = WTFMove(automationClient);
}

void WebProcessPool::setLegacyCustomProtocolManagerClient(std::unique_ptr<API::CustomProtocolManagerClient>&& customProtocolManagerClient)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (!customProtocolManagerClient)
        m_customProtocolManagerClient = std::make_unique<API::CustomProtocolManagerClient>();
    else
        m_customProtocolManagerClient = WTFMove(customProtocolManagerClient);
#endif
}

void WebProcessPool::setMaximumNumberOfProcesses(unsigned maximumNumberOfProcesses)
{
    // Guard against API misuse.
    if (!m_processes.isEmpty())
        CRASH();

    m_configuration->setMaximumProcessCount(maximumNumberOfProcesses);
}

IPC::Connection* WebProcessPool::networkingProcessConnection()
{
    return m_networkProcess->connection();
}

void WebProcessPool::languageChanged(void* context)
{
    static_cast<WebProcessPool*>(context)->languageChanged();
}

void WebProcessPool::languageChanged()
{
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(userPreferredLanguages()));
#if USE(SOUP)
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::UserPreferredLanguagesChanged(userPreferredLanguages()), 0);
#endif
}

void WebProcessPool::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    sendToAllProcesses(Messages::WebProcess::FullKeyboardAccessModeChanged(fullKeyboardAccessEnabled));
}

void WebProcessPool::textCheckerStateChanged()
{
    sendToAllProcesses(Messages::WebProcess::SetTextCheckerState(TextChecker::state()));
}

NetworkProcessProxy& WebProcessPool::ensureNetworkProcess(WebsiteDataStore* withWebsiteDataStore)
{
    if (m_networkProcess) {
        if (withWebsiteDataStore)
            m_networkProcess->send(Messages::NetworkProcess::AddWebsiteDataStore(withWebsiteDataStore->parameters()), 0);
        return *m_networkProcess;
    }

    if (m_websiteDataStore)
        m_websiteDataStore->websiteDataStore().resolveDirectoriesIfNecessary();

    m_networkProcess = NetworkProcessProxy::create(*this);

    NetworkProcessCreationParameters parameters;

    if (withWebsiteDataStore) {
        auto websiteDataStoreParameters = withWebsiteDataStore->parameters();
        parameters.defaultSessionParameters = websiteDataStoreParameters.networkSessionParameters;

        // FIXME: This isn't conceptually correct, but it's needed to preserve behavior introduced in r213241.
        // We should separate the concept of the default session from the currently used persistent session.
        parameters.defaultSessionParameters.sessionID = PAL::SessionID::defaultSessionID();
    }
    
    parameters.privateBrowsingEnabled = WebPreferences::anyPagesAreUsingPrivateBrowsing();

    parameters.cacheModel = cacheModel();
    parameters.diskCacheSizeOverride = m_configuration->diskCacheSizeOverride();
    parameters.canHandleHTTPSServerTrustEvaluation = m_canHandleHTTPSServerTrustEvaluation;

    for (auto& scheme : globalURLSchemesWithCustomProtocolHandlers())
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

    for (auto& scheme : m_urlSchemesRegisteredForCustomProtocols)
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

    parameters.cacheStorageDirectory = m_websiteDataStore ? m_websiteDataStore->websiteDataStore().cacheStorageDirectory() : API::WebsiteDataStore::defaultCacheStorageDirectory();
    if (!parameters.cacheStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.cacheStorageDirectory, parameters.cacheStorageDirectoryExtensionHandle);
    parameters.cacheStoragePerOriginQuota = m_websiteDataStore ? m_websiteDataStore->websiteDataStore().cacheStoragePerOriginQuota() : WebsiteDataStore::defaultCacheStoragePerOriginQuota;

    parameters.diskCacheDirectory = m_configuration->diskCacheDirectory();
    if (!parameters.diskCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.diskCacheDirectory, parameters.diskCacheDirectoryExtensionHandle);
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    parameters.shouldEnableNetworkCacheSpeculativeRevalidation = m_configuration->diskCacheSpeculativeValidationEnabled();
#endif

#if PLATFORM(IOS)
    String cookieStorageDirectory = this->cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = this->networkingCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String parentBundleDirectory = this->parentBundleDirectory();
    if (!parentBundleDirectory.isEmpty())
        SandboxExtension::createHandle(parentBundleDirectory, SandboxExtension::Type::ReadOnly, parameters.parentBundleDirectoryExtensionHandle);
#endif

#if OS(LINUX)
    if (MemoryPressureMonitor::isEnabled())
        parameters.memoryPressureMonitorHandle = MemoryPressureMonitor::singleton().createHandle();
#endif

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;
    parameters.presentingApplicationPID = m_configuration->presentingApplicationPID();

    parameters.urlSchemesRegisteredAsSecure = copyToVector(m_schemesToRegisterAsSecure);
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(m_schemesToRegisterAsBypassingContentSecurityPolicy);
    parameters.urlSchemesRegisteredAsLocal = copyToVector(m_schemesToRegisterAsLocal);
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(m_schemesToRegisterAsNoAccess);
    parameters.urlSchemesRegisteredAsDisplayIsolated = copyToVector(m_schemesToRegisterAsDisplayIsolated);
    parameters.urlSchemesRegisteredAsCORSEnabled = copyToVector(m_schemesToRegisterAsCORSEnabled);
    parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest = copyToVector(m_schemesToRegisterAsCanDisplayOnlyIfCanRequest);

    parameters.trackNetworkActivity = m_configuration->trackNetworkActivity();

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    m_networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

#if PLATFORM(COCOA)
    m_networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif

    if (m_didNetworkProcessCrash) {
        m_didNetworkProcessCrash = false;
        reinstateNetworkProcessAssertionState(*m_networkProcess);
        if (m_websiteDataStore)
            m_websiteDataStore->websiteDataStore().networkProcessDidCrash();
    }

    if (withWebsiteDataStore)
        m_networkProcess->send(Messages::NetworkProcess::AddWebsiteDataStore(withWebsiteDataStore->parameters()), 0);

    return *m_networkProcess;
}

void WebProcessPool::networkProcessCrashed(NetworkProcessProxy& networkProcessProxy, Vector<Ref<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>>&& pendingReplies)
{
    networkProcessFailedToLaunch(networkProcessProxy);
    ASSERT(!m_networkProcess);
    if (pendingReplies.isEmpty())
        return;
    auto& newNetworkProcess = ensureNetworkProcess();
    for (auto& reply : pendingReplies)
        newNetworkProcess.getNetworkProcessConnection(WTFMove(reply));
}

void WebProcessPool::networkProcessFailedToLaunch(NetworkProcessProxy& networkProcessProxy)
{
    ASSERT(m_networkProcess);
    ASSERT(&networkProcessProxy == m_networkProcess.get());
    m_didNetworkProcessCrash = true;

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(&networkProcessProxy);

    m_client.networkProcessDidCrash(this);

    // Leave the process proxy around during client call, so that the client could query the process identifier.
    m_networkProcess = nullptr;
}

void WebProcessPool::getNetworkProcessConnection(Ref<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>&& reply)
{
    ensureNetworkProcess();
    ASSERT(m_networkProcess);

    m_networkProcess->getNetworkProcessConnection(WTFMove(reply));
}

void WebProcessPool::ensureStorageProcessAndWebsiteDataStore(WebsiteDataStore* relevantDataStore)
{
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********

    if (!m_storageProcess) {
        auto parameters = m_websiteDataStore ? m_websiteDataStore->websiteDataStore().storageProcessParameters() : (relevantDataStore ? relevantDataStore->storageProcessParameters() : API::WebsiteDataStore::defaultDataStore()->websiteDataStore().storageProcessParameters());

        ASSERT(parameters.sessionID.isValid());

#if ENABLE(INDEXED_DATABASE)
        if (parameters.indexedDatabaseDirectory.isEmpty()) {
            parameters.indexedDatabaseDirectory = m_configuration->indexedDBDatabaseDirectory();
            SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
        }
#endif
#if ENABLE(SERVICE_WORKER)
        if (parameters.serviceWorkerRegistrationDirectory.isEmpty()) {
            parameters.serviceWorkerRegistrationDirectory = API::WebsiteDataStore::defaultServiceWorkerRegistrationDirectory();
            SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
        }

        if (!m_schemesServiceWorkersCanHandle.isEmpty())
            parameters.urlSchemesServiceWorkersCanHandle = copyToVector(m_schemesServiceWorkersCanHandle);

        parameters.shouldDisableServiceWorkerProcessTerminationDelay = m_shouldDisableServiceWorkerProcessTerminationDelay;
#endif

        m_storageProcess = StorageProcessProxy::create(*this);
        m_storageProcess->send(Messages::StorageProcess::InitializeWebsiteDataStore(parameters), 0);
    }

    if (!relevantDataStore || !m_websiteDataStore || relevantDataStore == &m_websiteDataStore->websiteDataStore())
        return;

    m_storageProcess->send(Messages::StorageProcess::InitializeWebsiteDataStore(relevantDataStore->storageProcessParameters()), 0);
}

void WebProcessPool::getStorageProcessConnection(WebProcessProxy& webProcessProxy, PAL::SessionID initialSessionID, Ref<Messages::WebProcessProxy::GetStorageProcessConnection::DelayedReply>&& reply)
{
    ensureStorageProcessAndWebsiteDataStore(WebsiteDataStore::existingNonDefaultDataStoreForSessionID(initialSessionID));

    m_storageProcess->getStorageProcessConnection(webProcessProxy, WTFMove(reply));
}

void WebProcessPool::storageProcessCrashed(StorageProcessProxy* storageProcessProxy)
{
    ASSERT(m_storageProcess);
    ASSERT(storageProcessProxy == m_storageProcess.get());

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(storageProcessProxy);

    m_client.storageProcessDidCrash(this);
    m_storageProcess = nullptr;
}

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::establishWorkerContextConnectionToStorageProcess(StorageProcessProxy& proxy, SecurityOriginData&& securityOrigin, std::optional<PAL::SessionID> sessionID)
{
    ASSERT_UNUSED(proxy, &proxy == m_storageProcess);

    if (m_serviceWorkerProcesses.contains(securityOrigin))
        return;

    m_mayHaveRegisteredServiceWorkers.clear();

    WebsiteDataStore* websiteDataStore = nullptr;
    if (sessionID)
        websiteDataStore = WebsiteDataStore::existingNonDefaultDataStoreForSessionID(*sessionID);

    if (!websiteDataStore) {
        if (!m_websiteDataStore)
            m_websiteDataStore = API::WebsiteDataStore::defaultDataStore().ptr();
        websiteDataStore = &m_websiteDataStore->websiteDataStore();
    }

    if (m_serviceWorkerProcesses.isEmpty())
        sendToAllProcesses(Messages::WebProcess::RegisterServiceWorkerClients { });

    auto serviceWorkerProcessProxy = ServiceWorkerProcessProxy::create(*this, securityOrigin, *websiteDataStore);
    m_serviceWorkerProcesses.add(WTFMove(securityOrigin), serviceWorkerProcessProxy.ptr());

    updateProcessAssertions();
    initializeNewWebProcess(serviceWorkerProcessProxy, *websiteDataStore);

    auto* serviceWorkerProcessProxyPtr = serviceWorkerProcessProxy.ptr();
    m_processes.append(WTFMove(serviceWorkerProcessProxy));

    serviceWorkerProcessProxyPtr->start(m_serviceWorkerPreferences ? m_serviceWorkerPreferences.value() : m_defaultPageGroup->preferences().store(), sessionID);
    if (!m_serviceWorkerUserAgent.isNull())
        serviceWorkerProcessProxyPtr->setUserAgent(m_serviceWorkerUserAgent);
}
#endif

void WebProcessPool::disableServiceWorkerProcessTerminationDelay()
{
#if ENABLE(SERVICE_WORKER)
    if (m_shouldDisableServiceWorkerProcessTerminationDelay)
        return;

    m_shouldDisableServiceWorkerProcessTerminationDelay = true;
    if (m_storageProcess)
        m_storageProcess->send(Messages::StorageProcess::DisableServiceWorkerProcessTerminationDelay(), 0);
#endif
}

void WebProcessPool::willStartUsingPrivateBrowsing()
{
    for (auto* processPool : allProcessPools())
        processPool->setAnyPageGroupMightHavePrivateBrowsingEnabled(true);
}

void WebProcessPool::willStopUsingPrivateBrowsing()
{
    for (auto* processPool : allProcessPools())
        processPool->setAnyPageGroupMightHavePrivateBrowsingEnabled(false);
}

void WebProcessPool::windowServerConnectionStateChanged()
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i)
        m_processes[i]->windowServerConnectionStateChanged();
}

void WebProcessPool::setAnyPageGroupMightHavePrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    if (networkProcess()) {
        if (privateBrowsingEnabled)
            networkProcess()->send(Messages::NetworkProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters()), 0);
        else
            networkProcess()->send(Messages::NetworkProcess::DestroySession(PAL::SessionID::legacyPrivateSessionID()), 0);
    }

    if (privateBrowsingEnabled)
        sendToAllProcesses(Messages::WebProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters()));
    else
        sendToAllProcesses(Messages::WebProcess::DestroySession(PAL::SessionID::legacyPrivateSessionID()));
}

void (*s_invalidMessageCallback)(WKStringRef messageName);

void WebProcessPool::setInvalidMessageCallback(void (*invalidMessageCallback)(WKStringRef messageName))
{
    s_invalidMessageCallback = invalidMessageCallback;
}

void WebProcessPool::didReceiveInvalidMessage(const IPC::StringReference& messageReceiverName, const IPC::StringReference& messageName)
{
    if (!s_invalidMessageCallback)
        return;

    StringBuilder messageNameStringBuilder;
    messageNameStringBuilder.append(messageReceiverName.data(), messageReceiverName.size());
    messageNameStringBuilder.append('.');
    messageNameStringBuilder.append(messageName.data(), messageName.size());

    s_invalidMessageCallback(toAPI(API::String::create(messageNameStringBuilder.toString()).ptr()));
}

void WebProcessPool::processDidCachePage(WebProcessProxy* process)
{
    if (m_processWithPageCache && m_processWithPageCache != process)
        m_processWithPageCache->releasePageCache();
    m_processWithPageCache = process;
}

void WebProcessPool::resolvePathsForSandboxExtensions()
{
    m_resolvedPaths.injectedBundlePath = resolvePathForSandboxExtension(injectedBundlePath());
    m_resolvedPaths.applicationCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->applicationCacheDirectory());
    m_resolvedPaths.webSQLDatabaseDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->webSQLDatabaseDirectory());
    m_resolvedPaths.mediaCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaCacheDirectory());
    m_resolvedPaths.mediaKeyStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaKeysStorageDirectory());
    m_resolvedPaths.indexedDatabaseDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->indexedDBDatabaseDirectory());

    m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.reserveCapacity(m_configuration->additionalReadAccessAllowedPaths().size());
    for (const auto& path : m_configuration->additionalReadAccessAllowedPaths())
        m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.uncheckedAppend(resolvePathForSandboxExtension(path.data()));

    platformResolvePathsForSandboxExtensions();
}

WebProcessProxy& WebProcessPool::createNewWebProcess(WebsiteDataStore& websiteDataStore, WebProcessProxy::IsInPrewarmedPool isInPrewarmedPool)
{
    auto processProxy = WebProcessProxy::create(*this, websiteDataStore, isInPrewarmedPool);
    auto& process = processProxy.get();
    initializeNewWebProcess(process, websiteDataStore);
    m_processes.append(WTFMove(processProxy));
    if (isInPrewarmedPool == WebProcessProxy::IsInPrewarmedPool::Yes)
        ++m_prewarmedProcessCount;

    if (m_serviceWorkerProcessesTerminationTimer.isActive())
        m_serviceWorkerProcessesTerminationTimer.stop();

    return process;
}

RefPtr<WebProcessProxy> WebProcessPool::tryTakePrewarmedProcess(WebsiteDataStore& websiteDataStore)
{
    if (!m_prewarmedProcessCount)
        return nullptr;

    for (const auto& process : m_processes) {
        if (process->isInPrewarmedPool()) {
            --m_prewarmedProcessCount;
            process->setIsInPrewarmedPool(false);
            if (&process->websiteDataStore() != &websiteDataStore)
                process->send(Messages::WebProcess::AddWebsiteDataStore(websiteDataStore.parameters()), 0);
            return process.get();
        }
    }

    ASSERT_NOT_REACHED();
    m_prewarmedProcessCount = 0;
    return nullptr;
}

#if PLATFORM(MAC)
static void displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    auto screenProperties = WebCore::getScreenProperties();
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties.first, screenProperties.second));
}

static void registerDisplayConfigurationCallback()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, nullptr);
        });
}
#endif

void WebProcessPool::initializeNewWebProcess(WebProcessProxy& process, WebsiteDataStore& websiteDataStore)
{
    ensureNetworkProcess();

    WebProcessCreationParameters parameters;

    websiteDataStore.resolveDirectoriesIfNecessary();

    parameters.injectedBundlePath = m_resolvedPaths.injectedBundlePath;
    if (!parameters.injectedBundlePath.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.injectedBundlePath, SandboxExtension::Type::ReadOnly, parameters.injectedBundlePathExtensionHandle);

    parameters.additionalSandboxExtensionHandles.allocate(m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.size());
    for (size_t i = 0, size = m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.size(); i < size; ++i)
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.additionalWebProcessSandboxExtensionPaths[i], SandboxExtension::Type::ReadOnly, parameters.additionalSandboxExtensionHandles[i]);

    parameters.applicationCacheDirectory = websiteDataStore.resolvedApplicationCacheDirectory();
    if (parameters.applicationCacheDirectory.isEmpty())
        parameters.applicationCacheDirectory = m_resolvedPaths.applicationCacheDirectory;
    if (!parameters.applicationCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.applicationCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.applicationCacheDirectoryExtensionHandle);

    parameters.applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName();

    parameters.webSQLDatabaseDirectory = websiteDataStore.resolvedDatabaseDirectory();
    if (parameters.webSQLDatabaseDirectory.isEmpty())
        parameters.webSQLDatabaseDirectory = m_resolvedPaths.webSQLDatabaseDirectory;
    if (!parameters.webSQLDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.webSQLDatabaseDirectory, SandboxExtension::Type::ReadWrite, parameters.webSQLDatabaseDirectoryExtensionHandle);

    parameters.mediaCacheDirectory = websiteDataStore.resolvedMediaCacheDirectory();
    if (parameters.mediaCacheDirectory.isEmpty())
        parameters.mediaCacheDirectory = m_resolvedPaths.mediaCacheDirectory;
    if (!parameters.mediaCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.mediaCacheDirectoryExtensionHandle);

    parameters.mediaKeyStorageDirectory = websiteDataStore.resolvedMediaKeysDirectory();
    if (parameters.mediaKeyStorageDirectory.isEmpty())
        parameters.mediaKeyStorageDirectory = m_resolvedPaths.mediaKeyStorageDirectory;
    if (!parameters.mediaKeyStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaKeyStorageDirectory, SandboxExtension::Type::ReadWrite, parameters.mediaKeyStorageDirectoryExtensionHandle);

#if PLATFORM(IOS)
    setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    if (javaScriptConfigurationFileEnabled()) {
        parameters.javaScriptConfigurationDirectory = websiteDataStore.resolvedJavaScriptConfigurationDirectory();
        if (!parameters.javaScriptConfigurationDirectory.isEmpty())
            SandboxExtension::createHandleWithoutResolvingPath(parameters.javaScriptConfigurationDirectory, SandboxExtension::Type::ReadWrite, parameters.javaScriptConfigurationDirectoryExtensionHandle);
    }

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    parameters.cacheModel = cacheModel();
    parameters.languages = userPreferredLanguages();

    parameters.urlSchemesRegisteredAsEmptyDocument = copyToVector(m_schemesToRegisterAsEmptyDocument);
    parameters.urlSchemesRegisteredAsSecure = copyToVector(m_schemesToRegisterAsSecure);
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(m_schemesToRegisterAsBypassingContentSecurityPolicy);
    parameters.urlSchemesForWhichDomainRelaxationIsForbidden = copyToVector(m_schemesToSetDomainRelaxationForbiddenFor);
    parameters.urlSchemesRegisteredAsLocal = copyToVector(m_schemesToRegisterAsLocal);
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(m_schemesToRegisterAsNoAccess);
    parameters.urlSchemesRegisteredAsDisplayIsolated = copyToVector(m_schemesToRegisterAsDisplayIsolated);
    parameters.urlSchemesRegisteredAsCORSEnabled = copyToVector(m_schemesToRegisterAsCORSEnabled);
    parameters.urlSchemesRegisteredAsAlwaysRevalidated = copyToVector(m_schemesToRegisterAsAlwaysRevalidated);
    parameters.urlSchemesRegisteredAsCachePartitioned = copyToVector(m_schemesToRegisterAsCachePartitioned);
    parameters.urlSchemesServiceWorkersCanHandle = copyToVector(m_schemesServiceWorkersCanHandle);
    parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest = copyToVector(m_schemesToRegisterAsCanDisplayOnlyIfCanRequest);

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;

    parameters.terminationTimeout = 0_s;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    parameters.notificationPermissions = supplement<WebNotificationManagerProxy>()->notificationPermissions();
#endif

    parameters.plugInAutoStartOriginHashes = m_plugInAutoStartProvider.autoStartOriginHashesCopy();
    parameters.plugInAutoStartOrigins = copyToVector(m_plugInAutoStartProvider.autoStartOrigins());

    parameters.memoryCacheDisabled = m_memoryCacheDisabled;

#if ENABLE(SERVICE_CONTROLS)
    auto& serviceController = ServicesController::singleton();
    parameters.hasImageServices = serviceController.hasImageServices();
    parameters.hasSelectionServices = serviceController.hasSelectionServices();
    parameters.hasRichContentServices = serviceController.hasRichContentServices();
    serviceController.refreshExistingServices();
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    parameters.pluginLoadClientPolicies = m_pluginLoadClientPolicies;
#endif

#if OS(LINUX)
    parameters.shouldEnableMemoryPressureReliefLogging = true;
    if (MemoryPressureMonitor::isEnabled())
        parameters.memoryPressureMonitorHandle = MemoryPressureMonitor::singleton().createHandle();
#endif

#if PLATFORM(WAYLAND) && USE(EGL)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        parameters.waylandCompositorDisplayName = WaylandCompositor::singleton().displayName();
#endif

    parameters.resourceLoadStatisticsEnabled = resourceLoadStatisticsEnabled();
#if ENABLE(MEDIA_STREAM)
    parameters.shouldCaptureAudioInUIProcess = m_configuration->shouldCaptureAudioInUIProcess();
#endif

    parameters.presentingApplicationPID = m_configuration->presentingApplicationPID();

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient->getInjectedBundleInitializationUserData(*this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process.transformObjectsToHandles(injectedBundleInitializationUserData.get()));

    process.send(Messages::WebProcess::InitializeWebProcess(parameters), 0);

#if PLATFORM(COCOA)
    process.send(Messages::WebProcess::SetQOS(webProcessLatencyQOS(), webProcessThroughputQOS()), 0);
#endif

    if (WebPreferences::anyPagesAreUsingPrivateBrowsing())
        process.send(Messages::WebProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters()), 0);

    if (m_automationSession)
        process.send(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()), 0);

    ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton(); 
#endif

#if PLATFORM(MAC)
    registerDisplayConfigurationCallback();

    auto screenProperties = WebCore::getScreenProperties();
    process.send(Messages::WebProcess::SetScreenProperties(screenProperties.first, screenProperties.second), 0);
#endif
}

void WebProcessPool::warmInitialProcess()
{
    if (m_prewarmedProcessCount) {
        ASSERT(!m_processes.isEmpty());
        return;
    }

    if (m_processes.size() >= maximumNumberOfProcesses())
        return;

    if (!m_websiteDataStore)
        m_websiteDataStore = API::WebsiteDataStore::defaultDataStore().ptr();
    createNewWebProcess(m_websiteDataStore->websiteDataStore(), WebProcessProxy::IsInPrewarmedPool::Yes);
}

void WebProcessPool::enableProcessTermination()
{
    m_processTerminationEnabled = true;
    Vector<RefPtr<WebProcessProxy>> processes = m_processes;
    for (size_t i = 0; i < processes.size(); ++i) {
        if (shouldTerminate(processes[i].get()))
            processes[i]->terminate();
    }
}

bool WebProcessPool::shouldTerminate(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (!m_processTerminationEnabled || m_configuration->alwaysKeepAndReuseSwappedProcesses())
        return false;

    return true;
}

void WebProcessPool::processDidFinishLaunching(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (!m_visitedLinksPopulated) {
        populateVisitedLinks();
        m_visitedLinksPopulated = true;
    }

    // Sometimes the memorySampler gets initialized after process initialization has happened but before the process has finished launching
    // so check if it needs to be started here
    if (m_memorySamplerEnabled) {
        SandboxExtension::Handle sampleLogSandboxHandle;        
        WallTime now = WallTime::now();
        String sampleLogFilePath = String::format("WebProcess%llupid%d", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()), process->processIdentifier());
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite, sampleLogSandboxHandle);
        
        process->send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    if (m_configuration->fullySynchronousModeIsAllowedForTesting())
        process->connection()->allowFullySynchronousModeForTesting();

    if (m_configuration->ignoreSynchronousMessagingTimeoutsForTesting())
        process->connection()->ignoreTimeoutsForTesting();

    m_connectionClient.didCreateConnection(this, process->webConnection());
}

void WebProcessPool::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (process->isInPrewarmedPool())
        --m_prewarmedProcessCount;

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    RefPtr<WebProcessProxy> protect(process);
    if (m_processWithPageCache == process)
        m_processWithPageCache = nullptr;

#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerProcessProxy>(*process)) {
        auto* removedProcess = m_serviceWorkerProcesses.take(downcast<ServiceWorkerProcessProxy>(*process).securityOrigin());
        ASSERT_UNUSED(removedProcess, removedProcess == process);
        updateProcessAssertions();
    }
#endif

    static_cast<WebContextSupplement*>(supplement<WebGeolocationManagerProxy>())->processDidClose(process);

    m_processes.removeFirst(process);

#if ENABLE(GAMEPAD)
    if (m_processesUsingGamepads.contains(process))
        processStoppedUsingGamepads(*process);
#endif

    removeProcessFromOriginCacheSet(*process);

#if ENABLE(SERVICE_WORKER)
    // FIXME: We should do better than this. For now, we just destroy the ServiceWorker process
    // whenever there is no regular WebContent process remaining.
    if (m_processes.size() == m_serviceWorkerProcesses.size()) {
        if (!m_serviceWorkerProcessesTerminationTimer.isActive())
            m_serviceWorkerProcessesTerminationTimer.startOneShot(serviceWorkerTerminationDelay);
    }
#endif
}

WebProcessProxy& WebProcessPool::createNewWebProcessRespectingProcessCountLimit(WebsiteDataStore& websiteDataStore)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=168676
    // Once WebsiteDataStores are truly per-view instead of per-process, remove this nonsense.

#if PLATFORM(COCOA)
    bool mustMatchDataStore = API::WebsiteDataStore::defaultDataStoreExists() && &websiteDataStore != &API::WebsiteDataStore::defaultDataStore()->websiteDataStore();
#else
    bool mustMatchDataStore = false;
#endif

    if (m_processes.size() < maximumNumberOfProcesses())
        return createNewWebProcess(websiteDataStore);

    WebProcessProxy* processToReuse = nullptr;
    for (auto& process : m_processes) {
        if (mustMatchDataStore && &process->websiteDataStore() != &websiteDataStore)
            continue;
#if ENABLE(SERVICE_WORKER)
        if (is<ServiceWorkerProcessProxy>(*process))
            continue;
#endif
        // Choose the process with fewest pages.
        if (!processToReuse || processToReuse->pageCount() > process->pageCount())
            processToReuse = process.get();
    }
    return processToReuse ? *processToReuse : createNewWebProcess(websiteDataStore);
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
        if (!m_websiteDataStore)
            m_websiteDataStore = API::WebsiteDataStore::defaultDataStore().ptr();

        ASSERT(!pageConfiguration->sessionID().isValid());
        pageConfiguration->setWebsiteDataStore(m_websiteDataStore.get());
        pageConfiguration->setSessionID(pageConfiguration->preferences()->privateBrowsingEnabled() ? PAL::SessionID::legacyPrivateSessionID() : m_websiteDataStore->websiteDataStore().sessionID());
    }

    RefPtr<WebProcessProxy> process;
    if (pageConfiguration->relatedPage()) {
        // Sharing processes, e.g. when creating the page via window.open().
        process = &pageConfiguration->relatedPage()->process();
    } else {
        process = tryTakePrewarmedProcess(pageConfiguration->websiteDataStore()->websiteDataStore());
        if (!process)
            process = &createNewWebProcessRespectingProcessCountLimit(pageConfiguration->websiteDataStore()->websiteDataStore());
    }

#if ENABLE(SERVICE_WORKER)
    ASSERT(!is<ServiceWorkerProcessProxy>(*process));
#endif

    return process->createWebPage(pageClient, WTFMove(pageConfiguration));
}

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::updateServiceWorkerUserAgent(const String& userAgent)
{
    if (m_serviceWorkerUserAgent == userAgent)
        return;
    m_serviceWorkerUserAgent = userAgent;
    for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values())
        serviceWorkerProcess->setUserAgent(m_serviceWorkerUserAgent);
}

bool WebProcessPool::mayHaveRegisteredServiceWorkers(const WebsiteDataStore& store)
{
    if (!m_serviceWorkerProcesses.isEmpty())
        return true;

    String serviceWorkerRegistrationDirectory = store.resolvedServiceWorkerRegistrationDirectory();
    if (serviceWorkerRegistrationDirectory.isEmpty())
        serviceWorkerRegistrationDirectory = API::WebsiteDataStore::defaultDataStoreConfiguration().serviceWorkerRegistrationDirectory;

    return m_mayHaveRegisteredServiceWorkers.ensure(serviceWorkerRegistrationDirectory, [&] {
        // FIXME: Make this computation on a background thread.
        return ServiceWorkerProcessProxy::hasRegisteredServiceWorkers(serviceWorkerRegistrationDirectory);
    }).iterator->value;
}
#endif

void WebProcessPool::pageBeginUsingWebsiteDataStore(WebPageProxy& page)
{
    auto result = m_sessionToPagesMap.add(page.sessionID(), HashSet<WebPageProxy*>()).iterator->value.add(&page);
    ASSERT_UNUSED(result, result.isNewEntry);

    auto sessionID = page.sessionID();
    if (sessionID.isEphemeral()) {
        ASSERT(page.websiteDataStore().parameters().networkSessionParameters.sessionID == sessionID);
        sendToNetworkingProcess(Messages::NetworkProcess::AddWebsiteDataStore(page.websiteDataStore().parameters()));
        page.process().send(Messages::WebProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::privateSessionParameters(sessionID)), 0);
    } else if (sessionID != PAL::SessionID::defaultSessionID()) {
        sendToNetworkingProcess(Messages::NetworkProcess::AddWebsiteDataStore(page.websiteDataStore().parameters()));
        page.process().send(Messages::WebProcess::AddWebsiteDataStore(page.websiteDataStore().parameters()), 0);

#if ENABLE(INDEXED_DATABASE)
        if (!page.websiteDataStore().resolvedIndexedDatabaseDirectory().isEmpty())
            ensureStorageProcessAndWebsiteDataStore(&page.websiteDataStore());
#endif
    }

#if ENABLE(SERVICE_WORKER)
    if (!m_serviceWorkerPreferences) {
        m_serviceWorkerPreferences = page.preferencesStore();
        for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values())
            serviceWorkerProcess->updatePreferencesStore(*m_serviceWorkerPreferences);
    }
#endif
}

void WebProcessPool::pageEndUsingWebsiteDataStore(WebPageProxy& page)
{
    auto sessionID = page.sessionID();
    auto iterator = m_sessionToPagesMap.find(sessionID);
    ASSERT(iterator != m_sessionToPagesMap.end());

    auto takenPage = iterator->value.take(&page);
    ASSERT_UNUSED(takenPage, takenPage == &page);

    if (iterator->value.isEmpty()) {
        m_sessionToPagesMap.remove(iterator);

        if (sessionID == PAL::SessionID::defaultSessionID())
            return;

        // The last user of this non-default PAL::SessionID is gone, so clean it up in the child processes.
        if (networkProcess())
            networkProcess()->send(Messages::NetworkProcess::DestroySession(sessionID), 0);
        page.process().send(Messages::WebProcess::DestroySession(sessionID), 0);
    }
}

DownloadProxy* WebProcessPool::download(WebPageProxy* initiatingPage, const ResourceRequest& request, const String& suggestedFilename)
{
    auto* downloadProxy = createDownloadProxy(request, initiatingPage);
    PAL::SessionID sessionID = initiatingPage ? initiatingPage->sessionID() : PAL::SessionID::defaultSessionID();

    if (initiatingPage)
        initiatingPage->handleDownloadRequest(downloadProxy);

    if (networkProcess()) {
        ResourceRequest updatedRequest(request);
        // Request's firstPartyForCookies will be used as Original URL of the download request.
        // We set the value to top level document's URL.
        if (initiatingPage) {
            URL initiatingPageURL = URL { URL { }, initiatingPage->pageLoadState().url() };
            updatedRequest.setFirstPartyForCookies(initiatingPageURL);
            updatedRequest.setIsSameSite(registrableDomainsAreEqual(initiatingPageURL, request.url()));
            if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
                updatedRequest.setHTTPUserAgent(initiatingPage->userAgent());
        } else {
            updatedRequest.setFirstPartyForCookies(URL());
            updatedRequest.setIsSameSite(false);
            if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
                updatedRequest.setHTTPUserAgent(WebPageProxy::standardUserAgent());
        }
        updatedRequest.setIsTopSite(false);
        networkProcess()->send(Messages::NetworkProcess::DownloadRequest(sessionID, downloadProxy->downloadID(), updatedRequest, suggestedFilename), 0);
        return downloadProxy;
    }

    return downloadProxy;
}

DownloadProxy* WebProcessPool::resumeDownload(const API::Data* resumeData, const String& path)
{
    auto* downloadProxy = createDownloadProxy(ResourceRequest(), nullptr);

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty())
        SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite, sandboxExtensionHandle);

    if (networkProcess()) {
        // FIXME: If we started a download in an ephemeral session and that session still exists, we should find a way to use that same session.
        networkProcess()->send(Messages::NetworkProcess::ResumeDownload(PAL::SessionID::defaultSessionID(), downloadProxy->downloadID(), resumeData->dataReference(), path, sandboxExtensionHandle), 0);
        return downloadProxy;
    }

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
    if (!m_configuration->processSwapsOnNavigation())
        return;
    if (!m_websiteDataStore)
        m_websiteDataStore = API::WebsiteDataStore::defaultDataStore().ptr();
    static constexpr size_t maxPrewarmCount = 1;
    while (m_prewarmedProcessCount < maxPrewarmCount)
        createNewWebProcess(m_websiteDataStore->websiteDataStore(), WebProcessProxy::IsInPrewarmedPool::Yes);
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

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::setAdditionalPluginsDirectory(const String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}

void WebProcessPool::refreshPlugins()
{
    m_pluginInfoStore.refresh();
    sendToAllProcesses(Messages::WebProcess::RefreshPlugins());
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)

ProcessID WebProcessPool::networkProcessIdentifier()
{
    if (!m_networkProcess)
        return 0;

    return m_networkProcess->processIdentifier();
}

ProcessID WebProcessPool::storageProcessIdentifier()
{
    if (!m_storageProcess)
        return 0;

    return m_storageProcess->processIdentifier();
}

void WebProcessPool::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    m_alwaysUsesComplexTextCodePath = alwaysUseComplexText;
    sendToAllProcesses(Messages::WebProcess::SetAlwaysUsesComplexTextCodePath(alwaysUseComplexText));
}

void WebProcessPool::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    m_shouldUseFontSmoothing = useFontSmoothing;
    sendToAllProcesses(Messages::WebProcess::SetShouldUseFontSmoothing(useFontSmoothing));
}

void WebProcessPool::setResourceLoadStatisticsEnabled(bool enabled)
{
    m_resourceLoadStatisticsEnabled = enabled;
    sendToAllProcesses(Messages::WebProcess::SetResourceLoadStatisticsEnabled(enabled));
}

void WebProcessPool::clearResourceLoadStatistics()
{
    sendToAllProcesses(Messages::WebProcess::ClearResourceLoadStatistics());
}

void WebProcessPool::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme));
}

void WebProcessPool::registerURLSchemeAsSecure(const String& urlScheme)
{
    m_schemesToRegisterAsSecure.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsSecure(urlScheme));
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    m_schemesToRegisterAsBypassingContentSecurityPolicy.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
}

void WebProcessPool::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme)
{
    m_schemesToSetDomainRelaxationForbiddenFor.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::SetDomainRelaxationForbiddenForURLScheme(urlScheme));
}

void WebProcessPool::setCanHandleHTTPSServerTrustEvaluation(bool value)
{
    m_canHandleHTTPSServerTrustEvaluation = value;
    if (m_networkProcess) {
        m_networkProcess->send(Messages::NetworkProcess::SetCanHandleHTTPSServerTrustEvaluation(value), 0);
        return;
    }
}

void WebProcessPool::preconnectToServer(const URL& url)
{
    if (!url.isValid() || !url.protocolIsInHTTPFamily())
        return;

    ensureNetworkProcess().send(Messages::NetworkProcess::PreconnectTo(url, StoredCredentialsPolicy::Use), 0);
}

void WebProcessPool::registerURLSchemeAsLocal(const String& urlScheme)
{
    m_schemesToRegisterAsLocal.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsLocal(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsLocal(urlScheme));
}

void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    m_schemesToRegisterAsNoAccess.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsNoAccess(urlScheme));
}

void WebProcessPool::registerURLSchemeAsDisplayIsolated(const String& urlScheme)
{
    m_schemesToRegisterAsDisplayIsolated.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsDisplayIsolated(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsDisplayIsolated(urlScheme));
}

void WebProcessPool::registerURLSchemeAsCORSEnabled(const String& urlScheme)
{
    m_schemesToRegisterAsCORSEnabled.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCORSEnabled(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsCORSEnabled(urlScheme));
}

void WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    globalURLSchemesWithCustomProtocolHandlers().add(urlScheme);
    for (auto* processPool : allProcessPools())
        processPool->registerSchemeForCustomProtocol(urlScheme);
}

void WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    globalURLSchemesWithCustomProtocolHandlers().remove(urlScheme);
    for (auto* processPool : allProcessPools())
        processPool->unregisterSchemeForCustomProtocol(urlScheme);
}

void WebProcessPool::registerURLSchemeAsCachePartitioned(const String& urlScheme)
{
    m_schemesToRegisterAsCachePartitioned.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCachePartitioned(urlScheme));
}

void WebProcessPool::registerURLSchemeServiceWorkersCanHandle(const String& urlScheme)
{
    m_schemesServiceWorkersCanHandle.add(urlScheme);
    sendToAllProcesses(Messages::ChildProcess::RegisterURLSchemeServiceWorkersCanHandle(urlScheme));
    if (m_storageProcess)
        m_storageProcess->send(Messages::ChildProcess::RegisterURLSchemeServiceWorkersCanHandle(urlScheme), 0);
}

void WebProcessPool::registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String& urlScheme)
{
    m_schemesToRegisterAsCanDisplayOnlyIfCanRequest.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCanDisplayOnlyIfCanRequest(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsCanDisplayOnlyIfCanRequest(urlScheme));
}

void WebProcessPool::setCacheModel(CacheModel cacheModel)
{
    m_configuration->setCacheModel(cacheModel);
    sendToAllProcesses(Messages::WebProcess::SetCacheModel(cacheModel));

    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::SetCacheModel(cacheModel), 0);
}

void WebProcessPool::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

DownloadProxy* WebProcessPool::createDownloadProxy(const ResourceRequest& request, WebPageProxy* originatingPage)
{
    auto downloadProxy = ensureNetworkProcess().createDownloadProxy(request);
    downloadProxy->setOriginatingPage(originatingPage);
    return downloadProxy;
}

void WebProcessPool::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebProcessPool::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebProcessPool::removeMessageReceiver(IPC::StringReference messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void WebProcessPool::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool WebProcessPool::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool WebProcessPool::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
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
    String sampleLogFilePath = String::format("WebProcess%llu", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()));
    sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite, sampleLogSandboxHandle);
    
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

void WebProcessPool::useTestingNetworkSession()
{
    ASSERT(m_processes.isEmpty());
    ASSERT(!m_networkProcess);

    if (m_networkProcess)
        return;

    if (!m_processes.isEmpty())
        return;

    m_shouldUseTestingNetworkSession = true;
}

template<typename T, typename U>
void WebProcessPool::sendSyncToNetworkingProcess(T&& message, U&& reply)
{
    if (m_networkProcess && m_networkProcess->canSendMessage())
        m_networkProcess->sendSync(std::forward<T>(message), std::forward<U>(reply), 0);
}

void WebProcessPool::setAllowsAnySSLCertificateForWebSocket(bool allows)
{
    sendSyncToNetworkingProcess(Messages::NetworkProcess::SetAllowsAnySSLCertificateForWebSocket(allows), Messages::NetworkProcess::SetAllowsAnySSLCertificateForWebSocket::Reply());
}

void WebProcessPool::clearCachedCredentials()
{
    sendToAllProcesses(Messages::WebProcess::ClearCachedCredentials());
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::ClearCachedCredentials(), 0);
}

void WebProcessPool::terminateStorageProcess()
{
    if (!m_storageProcess)
        return;

    m_storageProcess->terminate();
    m_storageProcess = nullptr;
}

void WebProcessPool::terminateNetworkProcess()
{
    if (!m_networkProcess)
        return;
    
    m_networkProcess->terminate();
    m_networkProcess = nullptr;
    m_didNetworkProcessCrash = true;
}

void WebProcessPool::terminateServiceWorkerProcesses()
{
#if ENABLE(SERVICE_WORKER)
    auto protectedThis = makeRef(*this);
    while (!m_serviceWorkerProcesses.isEmpty())
        m_serviceWorkerProcesses.begin()->value->requestTermination(ProcessTerminationReason::RequestedByClient);
#endif
}

void WebProcessPool::syncNetworkProcessCookies()
{
    sendSyncToNetworkingProcess(Messages::NetworkProcess::SyncAllCookies(), Messages::NetworkProcess::SyncAllCookies::Reply());
}

void WebProcessPool::allowSpecificHTTPSCertificateForHost(const WebCertificateInfo* certificate, const String& host)
{
    ensureNetworkProcess();
    m_networkProcess->send(Messages::NetworkProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
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

void WebProcessPool::getStatistics(uint32_t statisticsMask, Function<void (API::Dictionary*, CallbackBase::Error)>&& callbackFunction)
{
    if (!statisticsMask) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    RefPtr<StatisticsRequest> request = StatisticsRequest::create(DictionaryCallback::create(WTFMove(callbackFunction)));

    if (statisticsMask & StatisticsRequestTypeWebContent)
        requestWebContentStatistics(request.get());
    
    if (statisticsMask & StatisticsRequestTypeNetworking)
        requestNetworkingStatistics(request.get());
}

void WebProcessPool::requestWebContentStatistics(StatisticsRequest* request)
{
    // FIXME (Multi-WebProcess) <rdar://problem/13200059>: Make getting statistics from multiple WebProcesses work.
}

void WebProcessPool::requestNetworkingStatistics(StatisticsRequest* request)
{
    if (!m_networkProcess) {
        LOG_ERROR("Attempt to get NetworkProcess statistics but the NetworkProcess is unavailable");
        return;
    }

    uint64_t requestID = request->addOutstandingRequest();
    m_statisticsRequests.set(requestID, request);
    m_networkProcess->send(Messages::NetworkProcess::GetNetworkProcessStatistics(requestID), 0);
}

static WebProcessProxy* webProcessProxyFromConnection(IPC::Connection& connection, const Vector<RefPtr<WebProcessProxy>>& processes)
{
    for (auto& process : processes) {
        if (process->hasConnection(connection))
            return process.get();
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

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, UserData& returnUserData)
{
    auto* webProcessProxy = webProcessProxyFromConnection(connection, m_processes);
    if (!webProcessProxy)
        return;

    RefPtr<API::Object> returnData;
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), returnData);
    returnUserData = UserData(webProcessProxy->transformObjectsToHandles(returnData.get()));
}

void WebProcessPool::didGetStatistics(const StatisticsData& statisticsData, uint64_t requestID)
{
    RefPtr<StatisticsRequest> request = m_statisticsRequests.take(requestID);
    if (!request) {
        LOG_ERROR("Cannot report networking statistics.");
        return;
    }

    request->completedRequest(requestID, statisticsData);
}

#if ENABLE(GAMEPAD)

void WebProcessPool::startedUsingGamepads(IPC::Connection& connection)
{
    auto* proxy = webProcessProxyFromConnection(connection, m_processes);
    if (!proxy)
        return;

    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.isEmpty();

    ASSERT(!m_processesUsingGamepads.contains(proxy));
    m_processesUsingGamepads.add(proxy);

    if (!wereAnyProcessesUsingGamepads)
        UIGamepadProvider::singleton().processPoolStartedUsingGamepads(*this);

    proxy->send(Messages::WebProcess::SetInitialGamepads(UIGamepadProvider::singleton().snapshotGamepads()), 0);
}

void WebProcessPool::stoppedUsingGamepads(IPC::Connection& connection)
{
    auto* proxy = webProcessProxyFromConnection(connection, m_processes);
    if (!proxy)
        return;

    ASSERT(m_processesUsingGamepads.contains(proxy));
    processStoppedUsingGamepads(*proxy);
}

void WebProcessPool::processStoppedUsingGamepads(WebProcessProxy& process)
{
    bool wereAnyProcessesUsingGamepads = !m_processesUsingGamepads.isEmpty();

    ASSERT(m_processesUsingGamepads.contains(&process));
    m_processesUsingGamepads.remove(&process);

    if (wereAnyProcessesUsingGamepads && m_processesUsingGamepads.isEmpty())
        UIGamepadProvider::singleton().processPoolStoppedUsingGamepads(*this);
}

void WebProcessPool::gamepadConnected(const UIGamepad& gamepad)
{
    for (auto& process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::GamepadConnected(gamepad.fullGamepadData()), 0);
}

void WebProcessPool::gamepadDisconnected(const UIGamepad& gamepad)
{
    for (auto& process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::GamepadDisconnected(gamepad.index()), 0);
}

void WebProcessPool::setInitialConnectedGamepads(const Vector<std::unique_ptr<UIGamepad>>& gamepads)
{
    Vector<GamepadData> gamepadDatas;
    gamepadDatas.grow(gamepads.size());
    for (size_t i = 0; i < gamepads.size(); ++i) {
        if (!gamepads[i])
            continue;
        gamepadDatas[i] = gamepads[i]->fullGamepadData();
    }

    for (auto& process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::SetInitialGamepads(gamepadDatas), 0);
}

#endif // ENABLE(GAMEPAD)

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

void WebProcessPool::addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, PAL::SessionID sessionID)
{
    m_plugInAutoStartProvider.addAutoStartOriginHash(pageOrigin, plugInOriginHash, sessionID);
}

void WebProcessPool::plugInDidReceiveUserInteraction(unsigned plugInOriginHash, PAL::SessionID sessionID)
{
    m_plugInAutoStartProvider.didReceiveUserInteraction(plugInOriginHash, sessionID);
}

Ref<API::Dictionary> WebProcessPool::plugInAutoStartOriginHashes() const
{
    return m_plugInAutoStartProvider.autoStartOriginsTableCopy();
}

void WebProcessPool::setPlugInAutoStartOriginHashes(API::Dictionary& dictionary)
{
    m_plugInAutoStartProvider.setAutoStartOriginsTable(dictionary);
}

void WebProcessPool::setPlugInAutoStartOrigins(API::Array& array)
{
    m_plugInAutoStartProvider.setAutoStartOriginsArray(array);
}

void WebProcessPool::setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary& dictionary, WallTime time)
{
    m_plugInAutoStartProvider.setAutoStartOriginsFilteringOutEntriesAddedAfterTime(dictionary, time);
}

void WebProcessPool::registerSchemeForCustomProtocol(const String& scheme)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (!globalURLSchemesWithCustomProtocolHandlers().contains(scheme))
        m_urlSchemesRegisteredForCustomProtocols.add(scheme);
    sendToNetworkingProcess(Messages::LegacyCustomProtocolManager::RegisterScheme(scheme));
#endif
}

void WebProcessPool::unregisterSchemeForCustomProtocol(const String& scheme)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    m_urlSchemesRegisteredForCustomProtocols.remove(scheme);
    sendToNetworkingProcess(Messages::LegacyCustomProtocolManager::UnregisterScheme(scheme));
#endif
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    auto& policiesForHost = m_pluginLoadClientPolicies.ensure(host, [] { return HashMap<String, HashMap<String, uint8_t>>(); }).iterator->value;
    auto& versionsToPolicies = policiesForHost.ensure(bundleIdentifier, [] { return HashMap<String, uint8_t>(); }).iterator->value;
    versionsToPolicies.set(versionString, policy);

    sendToAllProcesses(Messages::WebProcess::SetPluginLoadClientPolicy(policy, host, bundleIdentifier, versionString));
}

void WebProcessPool::resetPluginLoadClientPolicies(HashMap<String, HashMap<String, HashMap<String, uint8_t>>>&& pluginLoadClientPolicies)
{
    m_pluginLoadClientPolicies = WTFMove(pluginLoadClientPolicies);
    sendToAllProcesses(Messages::WebProcess::ResetPluginLoadClientPolicies(m_pluginLoadClientPolicies));
}

void WebProcessPool::clearPluginClientPolicies()
{
    m_pluginLoadClientPolicies.clear();
    sendToAllProcesses(Messages::WebProcess::ClearPluginClientPolicies());
}
#endif

void WebProcessPool::addSupportedPlugin(String&& matchingDomain, String&& name, HashSet<String>&& mimeTypes, HashSet<String> extensions)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.addSupportedPlugin(WTFMove(matchingDomain), WTFMove(name), WTFMove(mimeTypes), WTFMove(extensions));
#else
    UNUSED_PARAM(matchingDomain);
    UNUSED_PARAM(name);
    UNUSED_PARAM(mimeTypes);
    UNUSED_PARAM(extensions);
#endif
}

void WebProcessPool::clearSupportedPlugins()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.clearSupportedPlugins();
#endif
}

void WebProcessPool::setMemoryCacheDisabled(bool disabled)
{
    m_memoryCacheDisabled = disabled;
    sendToAllProcesses(Messages::WebProcess::SetMemoryCacheDisabled(disabled));
}

void WebProcessPool::setFontWhitelist(API::Array* array)
{
    m_fontWhitelist.clear();
    if (array) {
        for (size_t i = 0; i < array->size(); ++i) {
            if (API::String* font = array->at<API::String>(i))
                m_fontWhitelist.append(font->string());
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

void WebProcessPool::updateProcessAssertions()
{
#if PLATFORM(IOS)
#if ENABLE(SERVICE_WORKER)
    auto updateServiceWorkerProcessAssertion = [&] {
        if (!m_serviceWorkerProcesses.isEmpty() && m_foregroundWebProcessCounter.value()) {
            // FIXME: We can do better than this once we have process per origin.
            for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values()) {
                auto& securityOrigin = serviceWorkerProcess->securityOrigin();
                if (!m_foregroundTokensForServiceWorkerProcesses.contains(securityOrigin))
                    m_foregroundTokensForServiceWorkerProcesses.add(securityOrigin, serviceWorkerProcess->throttler().foregroundActivityToken());
            }
            m_backgroundTokensForServiceWorkerProcesses.clear();
            return;
        }
        if (!m_serviceWorkerProcesses.isEmpty() && m_backgroundWebProcessCounter.value()) {
            // FIXME: We can do better than this once we have process per origin.
            for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values()) {
                auto& securityOrigin = serviceWorkerProcess->securityOrigin();
                if (!m_backgroundTokensForServiceWorkerProcesses.contains(securityOrigin))
                    m_backgroundTokensForServiceWorkerProcesses.add(securityOrigin, serviceWorkerProcess->throttler().backgroundActivityToken());
            }
            m_foregroundTokensForServiceWorkerProcesses.clear();
            return;
        }
        m_foregroundTokensForServiceWorkerProcesses.clear();
        m_backgroundTokensForServiceWorkerProcesses.clear();
    };
    updateServiceWorkerProcessAssertion();
#endif

    auto updateNetworkProcessAssertion = [&] {
        if (m_foregroundWebProcessCounter.value()) {
            if (!m_foregroundTokenForNetworkProcess)
                m_foregroundTokenForNetworkProcess = ensureNetworkProcess().throttler().foregroundActivityToken();
            m_backgroundTokenForNetworkProcess = nullptr;
            return;
        }
        if (m_backgroundWebProcessCounter.value()) {
            if (!m_backgroundTokenForNetworkProcess)
                m_backgroundTokenForNetworkProcess = ensureNetworkProcess().throttler().backgroundActivityToken();
            m_foregroundTokenForNetworkProcess = nullptr;
            return;
        }
        m_foregroundTokenForNetworkProcess = nullptr;
        m_backgroundTokenForNetworkProcess = nullptr;
    };
    updateNetworkProcessAssertion();
#endif
}

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destination, MessageWithMessagePorts&& message, ServiceWorkerIdentifier source, const String& sourceOrigin)
{
    sendToStorageProcessRelaunchingIfNecessary(Messages::StorageProcess::PostMessageToServiceWorkerClient(destination, WTFMove(message), source, sourceOrigin));
}

void WebProcessPool::postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& source, SWServerConnectionIdentifier connectionIdentifier)
{
    sendToStorageProcessRelaunchingIfNecessary(Messages::StorageProcess::PostMessageToServiceWorker(destination, WTFMove(message), source, connectionIdentifier));
}
#endif

void WebProcessPool::reinstateNetworkProcessAssertionState(NetworkProcessProxy& newNetworkProcessProxy)
{
#if PLATFORM(IOS)
    // The network process crashed; take new tokens for the new network process.
    if (m_backgroundTokenForNetworkProcess)
        m_backgroundTokenForNetworkProcess = newNetworkProcessProxy.throttler().backgroundActivityToken();
    else if (m_foregroundTokenForNetworkProcess)
        m_foregroundTokenForNetworkProcess = newNetworkProcessProxy.throttler().foregroundActivityToken();
#else
    UNUSED_PARAM(newNetworkProcessProxy);
#endif
}

#if ENABLE(SERVICE_WORKER)
ServiceWorkerProcessProxy* WebProcessPool::serviceWorkerProcessProxyFromPageID(uint64_t pageID) const
{
    // FIXME: This is inefficient.
    for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values()) {
        if (serviceWorkerProcess->pageID() == pageID)
            return serviceWorkerProcess;
    }
    return nullptr;
}
#endif

void WebProcessPool::addProcessToOriginCacheSet(WebPageProxy& page)
{
    auto origin = SecurityOriginData::fromURL({ ParsedURLString, page.pageLoadState().url() });
    auto result = m_swappedProcesses.add(origin, &page.process());
    if (!result.isNewEntry)
        result.iterator->value = &page.process();

    LOG(ProcessSwapping, "(ProcessSwapping) Security origin %s just saved a cached process with pid %i", origin.debugString().utf8().data(), page.process().processIdentifier());
    if (!result.isNewEntry)
        LOG(ProcessSwapping, "(ProcessSwapping) Note: It already had one saved");
}

void WebProcessPool::removeProcessFromOriginCacheSet(WebProcessProxy& process)
{
    LOG(ProcessSwapping, "(ProcessSwapping) Removing process with pid %i from the origin cache set", process.processIdentifier());

    // FIXME: This can be very inefficient as the number of remembered origins and processes grows
    Vector<SecurityOriginData> originsToRemove;
    for (auto entry : m_swappedProcesses) {
        if (entry.value == &process)
            originsToRemove.append(entry.key);
    }

    for (auto& origin : originsToRemove)
        m_swappedProcesses.remove(origin);
}

Ref<WebProcessProxy> WebProcessPool::processForNavigation(WebPageProxy& page, const API::Navigation& navigation, PolicyAction& action)
{
    auto process = processForNavigationInternal(page, navigation, action);

    if (m_configuration->alwaysKeepAndReuseSwappedProcesses() && process.ptr() != &page.process()) {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            WTFLogAlways("WARNING: The option to always keep swapped web processes alive is active. This is meant for debugging and testing only.");
        });

        addProcessToOriginCacheSet(page);

        LOG(ProcessSwapping, "(ProcessSwapping) Navigating from %s to %s, keeping around old process. Now holding on to old processes for %u origins.", page.currentURL().utf8().data(), navigation.currentRequest().url().string().utf8().data(), m_swappedProcesses.size());
    }

    return process;
}

Ref<WebProcessProxy> WebProcessPool::processForNavigationInternal(WebPageProxy& page, const API::Navigation& navigation, PolicyAction& action)
{
    if (!m_configuration->processSwapsOnNavigation())
        return page.process();

    if (page.inspectorFrontendCount() > 0)
        return page.process();

    if (!page.process().hasCommittedAnyProvisionalLoads())
        return page.process();

    if (navigation.isCrossOriginWindowOpenNavigation()) {
        if (navigation.opener() && !m_configuration->processSwapsOnWindowOpenWithOpener())
            return page.process();

        action = PolicyAction::Ignore;
        return createNewWebProcess(page.websiteDataStore());
    }

    // FIXME: We should support process swap when a window has an opener.
    if (navigation.opener())
        return page.process();

    if (auto* backForwardListItem = navigation.targetItem()) {
        if (auto* suspendedPage = backForwardListItem->suspendedPage()) {
            ASSERT(suspendedPage->process());
            action = PolicyAction::Suspend;
            return *suspendedPage->process();
        }
    }

    if (navigation.treatAsSameOriginNavigation())
        return page.process();

    auto targetURL = navigation.currentRequest().url();
    auto url = URL { ParsedURLString, page.pageLoadState().url() };
    if (!url.isValid() || !targetURL.isValid() || url.isEmpty() || url.isBlankURL() || protocolHostAndPortAreEqual(url, targetURL))
        return page.process();

    if (m_configuration->alwaysKeepAndReuseSwappedProcesses()) {
        auto origin = SecurityOriginData::fromURL(targetURL);
        LOG(ProcessSwapping, "(ProcessSwapping) Considering re-use of a previously cached process to URL %s", origin.debugString().utf8().data());

        if (auto* process = m_swappedProcesses.get(origin)) {
            if (&process->websiteDataStore() == &page.websiteDataStore()) {
                LOG(ProcessSwapping, "(ProcessSwapping) Reusing a previously cached process with pid %i to continue navigation to URL %s", process->processIdentifier(), targetURL.string().utf8().data());

                // FIXME: Architecturally we do not currently support multiple WebPage's with the same ID in a given WebProcess.
                // In the case where this WebProcess has a SuspendedPageProxy for this WebPage, we can throw it away to support
                // WebProcess re-use.
                // In the future it would be great to refactor-out this limitation.
                if (auto* suspendedPage = page.suspendedPage()) {
                    LOG(ProcessSwapping, "(ProcessSwapping) Destroying suspended page for that swap");
                    suspendedPage->destroyWebPageInWebProcess();
                }

                return makeRef(*process);
            }
        }
    }

    action = PolicyAction::Suspend;
    if (RefPtr<WebProcessProxy> process = tryTakePrewarmedProcess(page.websiteDataStore()))
        return process.releaseNonNull();
    return createNewWebProcess(page.websiteDataStore());
}

void WebProcessPool::registerSuspendedPageProxy(SuspendedPageProxy& page)
{
    auto& vector = m_suspendedPages.ensure(page.origin(), [] {
        return Vector<SuspendedPageProxy*> { };
    }).iterator->value;

    vector.append(&page);

#if !LOG_DISABLED
    if (vector.size() > 5)
        LOG(ProcessSwapping, "Security origin %s now has %zu suspended pages (this seems unexpected)", page.origin().debugString().utf8().data(), vector.size());
#endif
}

void WebProcessPool::unregisterSuspendedPageProxy(SuspendedPageProxy& page)
{
    auto iterator = m_suspendedPages.find(page.origin());
    ASSERT(iterator != m_suspendedPages.end());

    auto result = iterator->value.removeFirst(&page);
    ASSERT_UNUSED(result, result);

    if (iterator->value.isEmpty())
        m_suspendedPages.remove(iterator);
}

} // namespace WebKit
