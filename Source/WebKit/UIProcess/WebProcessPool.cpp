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
#include "APINavigation.h"
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "AuxiliaryProcessMessages.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "GamepadData.h"
#include "HighPerformanceGraphicsUsageSampler.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "PluginProcessManager.h"
#include "SandboxExtension.h"
#include "ServiceWorkerProcessProxy.h"
#include "StatisticsData.h"
#include "TextChecker.h"
#include "UIGamepad.h"
#include "UIGamepadProvider.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
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
#include "WebProcessCache.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <JavaScriptCore/JSCInlines.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <pal/SessionID.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URLParser.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerMessages.h"
#endif

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

#if PLATFORM(COCOA)
#include "VersionChecks.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

namespace WebKit {
using namespace WebCore;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, processPoolCounter, ("WebProcessPool"));

const Seconds serviceWorkerTerminationDelay { 5_s };

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

static Ref<WebsiteDataStoreConfiguration> legacyWebsiteDataStoreConfiguration(API::ProcessPoolConfiguration& processPoolConfiguration)
{
    auto configuration = WebsiteDataStoreConfiguration::create();

    configuration->setCacheStorageDirectory(String(API::WebsiteDataStore::defaultCacheStorageDirectory()));
    configuration->setServiceWorkerRegistrationDirectory(String(API::WebsiteDataStore::defaultServiceWorkerRegistrationDirectory()));
    configuration->setLocalStorageDirectory(String(processPoolConfiguration.localStorageDirectory()));
    configuration->setWebSQLDatabaseDirectory(String(processPoolConfiguration.webSQLDatabaseDirectory()));
    configuration->setApplicationCacheDirectory(String(processPoolConfiguration.applicationCacheDirectory()));
    configuration->setApplicationCacheFlatFileSubdirectoryName(String(processPoolConfiguration.applicationCacheFlatFileSubdirectoryName()));
    configuration->setMediaCacheDirectory(String(processPoolConfiguration.mediaCacheDirectory()));
    configuration->setMediaKeysStorageDirectory(String(processPoolConfiguration.mediaKeysStorageDirectory()));
    configuration->setIndexedDBDatabaseDirectory(String(processPoolConfiguration.indexedDBDatabaseDirectory()));
    configuration->setResourceLoadStatisticsDirectory(String(processPoolConfiguration.resourceLoadStatisticsDirectory()));
    configuration->setNetworkCacheDirectory(String(processPoolConfiguration.diskCacheDirectory()));
    configuration->setJavaScriptConfigurationDirectory(String(processPoolConfiguration.javaScriptConfigurationDirectory()));

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
#if PLATFORM(IOS_FAMILY)
    , m_foregroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
    , m_backgroundWebProcessCounter([this](RefCounterEvent) { updateProcessAssertions(); })
#endif
    , m_webProcessCache(makeUniqueRef<WebProcessCache>(*this))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        WTF::setProcessPrivileges(allPrivileges());
        WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
        Process::setIdentifier(WebCore::ProcessIdentifier::generate());
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

#if OS(LINUX)
    MemoryPressureMonitor::singleton().start();
#endif

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

    updateMaxSuspendedPageCount();
}

WebProcessPool::~WebProcessPool()
{
    m_webProcessCache->clear();

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

void WebProcessPool::setCustomWebContentServiceBundleIdentifier(const String& customWebContentServiceBundleIdentifier)
{
    // Guard against API misuse.
    if (!customWebContentServiceBundleIdentifier.isAllASCII())
        CRASH();

    m_configuration->setCustomWebContentServiceBundleIdentifier(customWebContentServiceBundleIdentifier);
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

#if OS(LINUX)
void WebProcessPool::sendMemoryPressureEvent(bool isCritical)
{
    sendToAllProcesses(Messages::AuxiliaryProcess::DidReceiveMemoryPressureEvent(isCritical));
    sendToNetworkingProcess(Messages::AuxiliaryProcess::DidReceiveMemoryPressureEvent(isCritical));
#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginProcessManager::singleton().sendMemoryPressureEvent(isCritical);
#endif
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
#if PLATFORM(MAC)
    auto screenProperties = WebCore::collectScreenProperties();
    sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));
#endif
}

NetworkProcessProxy& WebProcessPool::ensureNetworkProcess(WebsiteDataStore* withWebsiteDataStore)
{
    if (m_networkProcess) {
        if (withWebsiteDataStore) {
            m_networkProcess->addSession(makeRef(*withWebsiteDataStore));
            withWebsiteDataStore->clearPendingCookies();
        }
        return *m_networkProcess;
    }

    m_networkProcess = std::make_unique<NetworkProcessProxy>(*this);

    NetworkProcessCreationParameters parameters;

    if (m_websiteDataStore) {
        parameters.defaultDataStoreParameters.pendingCookies = copyToVector(m_websiteDataStore->websiteDataStore().pendingCookies());
        m_websiteDataStore->websiteDataStore().clearPendingCookies();
#if PLATFORM(COCOA)
        parameters.defaultDataStoreParameters.networkSessionParameters.sourceApplicationBundleIdentifier = m_websiteDataStore->websiteDataStore().sourceApplicationBundleIdentifier();
        parameters.defaultDataStoreParameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = m_websiteDataStore->websiteDataStore().sourceApplicationSecondaryIdentifier();
#endif
        m_websiteDataStore->websiteDataStore().finalizeApplicationIdentifiers();
    }

    parameters.cacheModel = cacheModel();
    parameters.canHandleHTTPSServerTrustEvaluation = m_canHandleHTTPSServerTrustEvaluation;

    for (auto& scheme : globalURLSchemesWithCustomProtocolHandlers())
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

    for (auto& scheme : m_urlSchemesRegisteredForCustomProtocols)
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

    parameters.diskCacheDirectory = m_configuration->diskCacheDirectory();
    if (!parameters.diskCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.diskCacheDirectory, parameters.diskCacheDirectoryExtensionHandle);
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    parameters.shouldEnableNetworkCacheSpeculativeRevalidation = m_configuration->diskCacheSpeculativeValidationEnabled();
#endif

#if PLATFORM(IOS_FAMILY)
    String cookieStorageDirectory = this->cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = this->networkingCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String parentBundleDirectory = this->parentBundleDirectory();
    if (!parentBundleDirectory.isEmpty())
        SandboxExtension::createHandle(parentBundleDirectory, SandboxExtension::Type::ReadOnly, parameters.parentBundleDirectoryExtensionHandle);

#if ENABLE(INDEXED_DATABASE)
    SandboxExtension::createHandleForTemporaryFile(emptyString(), SandboxExtension::Type::ReadWrite, parameters.defaultDataStoreParameters.indexedDatabaseTempBlobDirectoryExtensionHandle);
#endif
#endif

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    parameters.urlSchemesRegisteredAsSecure = copyToVector(m_schemesToRegisterAsSecure);
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(m_schemesToRegisterAsBypassingContentSecurityPolicy);
    parameters.urlSchemesRegisteredAsLocal = copyToVector(m_schemesToRegisterAsLocal);
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(m_schemesToRegisterAsNoAccess);
    parameters.urlSchemesRegisteredAsDisplayIsolated = copyToVector(m_schemesToRegisterAsDisplayIsolated);
    parameters.urlSchemesRegisteredAsCORSEnabled = copyToVector(m_schemesToRegisterAsCORSEnabled);
    parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest = copyToVector(m_schemesToRegisterAsCanDisplayOnlyIfCanRequest);

#if ENABLE(INDEXED_DATABASE)
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    parameters.defaultDataStoreParameters.indexedDatabaseDirectory = m_configuration->indexedDBDatabaseDirectory();
    if (parameters.defaultDataStoreParameters.indexedDatabaseDirectory.isEmpty())
        parameters.defaultDataStoreParameters.indexedDatabaseDirectory = API::WebsiteDataStore::defaultDataStore()->websiteDataStore().parameters().indexedDatabaseDirectory;
    
    SandboxExtension::createHandleForReadWriteDirectory(parameters.defaultDataStoreParameters.indexedDatabaseDirectory, parameters.defaultDataStoreParameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    if (m_websiteDataStore)
        parameters.serviceWorkerRegistrationDirectory = m_websiteDataStore->websiteDataStore().resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory)
        parameters.serviceWorkerRegistrationDirectory =  API::WebsiteDataStore::defaultServiceWorkerRegistrationDirectory();
    SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);

    if (!m_schemesServiceWorkersCanHandle.isEmpty())
        parameters.urlSchemesServiceWorkersCanHandle = copyToVector(m_schemesServiceWorkersCanHandle);

    parameters.shouldDisableServiceWorkerProcessTerminationDelay = m_shouldDisableServiceWorkerProcessTerminationDelay;
#endif

    if (m_websiteDataStore)
        parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectory = m_websiteDataStore->websiteDataStore().resolvedResourceLoadStatisticsDirectory();
    if (parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectory.isEmpty())
        parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectory = API::WebsiteDataStore::defaultResourceLoadStatisticsDirectory();

    SandboxExtension::createHandleForReadWriteDirectory(parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectory, parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectoryExtensionHandle);

    bool enableResourceLoadStatistics = false;
    bool shouldIncludeLocalhost = true;
    if (withWebsiteDataStore) {
        enableResourceLoadStatistics = withWebsiteDataStore->resourceLoadStatisticsEnabled();
        shouldIncludeLocalhost = withWebsiteDataStore->parameters().networkSessionParameters.shouldIncludeLocalhostInResourceLoadStatistics;
    } else if (m_websiteDataStore) {
        enableResourceLoadStatistics = m_websiteDataStore->resourceLoadStatisticsEnabled();
        shouldIncludeLocalhost = m_websiteDataStore->websiteDataStore().parameters().networkSessionParameters.shouldIncludeLocalhostInResourceLoadStatistics;
    }

    parameters.defaultDataStoreParameters.networkSessionParameters.enableResourceLoadStatistics = enableResourceLoadStatistics;
    parameters.defaultDataStoreParameters.networkSessionParameters.shouldIncludeLocalhostInResourceLoadStatistics = shouldIncludeLocalhost;

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    m_networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

    if (WebPreferences::anyPagesAreUsingPrivateBrowsing())
        m_networkProcess->send(Messages::NetworkProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters()), 0);

#if PLATFORM(COCOA)
    m_networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif

    if (m_didNetworkProcessCrash) {
        m_didNetworkProcessCrash = false;
        reinstateNetworkProcessAssertionState(*m_networkProcess);
    }

    if (withWebsiteDataStore) {
        m_networkProcess->addSession(makeRef(*withWebsiteDataStore));
        withWebsiteDataStore->clearPendingCookies();
    }

    // Make sure the network process knows about all the sessions that have been registered before it started.
    for (auto& sessionID : m_sessionToPageIDsMap.keys()) {
        if (auto* websiteDataStore = WebsiteDataStore::existingNonDefaultDataStoreForSessionID(sessionID))
            m_networkProcess->addSession(*websiteDataStore);
    }

    return *m_networkProcess;
}

void WebProcessPool::networkProcessCrashed(NetworkProcessProxy& networkProcessProxy, Vector<std::pair<RefPtr<WebProcessProxy>, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>>&& pendingReplies)
{
    ASSERT(m_networkProcess);
    ASSERT(&networkProcessProxy == m_networkProcess.get());
    m_didNetworkProcessCrash = true;

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(&networkProcessProxy);

    m_client.networkProcessDidCrash(this);

    if (m_automationSession)
        m_automationSession->terminate();

    // Leave the process proxy around during client call, so that the client could query the process identifier.
    m_networkProcess = nullptr;

    // Attempt to re-launch.
    if (pendingReplies.isEmpty())
        return;
    auto& newNetworkProcess = ensureNetworkProcess();
    for (auto& reply : pendingReplies)
        newNetworkProcess.getNetworkProcessConnection(*reply.first, WTFMove(reply.second));
}

void WebProcessPool::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&& reply)
{
    ensureNetworkProcess();
    ASSERT(m_networkProcess);

    m_networkProcess->getNetworkProcessConnection(webProcessProxy, WTFMove(reply));
}

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::establishWorkerContextConnectionToNetworkProcess(NetworkProcessProxy& proxy, SecurityOriginData&& securityOrigin, Optional<PAL::SessionID> sessionID)
{
    ASSERT_UNUSED(proxy, &proxy == m_networkProcess.get());

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
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::DisableServiceWorkerProcessTerminationDelay(), 0);
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
    if (privateBrowsingEnabled) {
        sendToNetworkingProcess(Messages::NetworkProcess::AddWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters()));
    } else {
        networkProcess()->removeSession(PAL::SessionID::legacyPrivateSessionID());
    }
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

WebProcessProxy& WebProcessPool::createNewWebProcess(WebsiteDataStore& websiteDataStore, WebProcessProxy::IsPrewarmed isPrewarmed)
{
    auto processProxy = WebProcessProxy::create(*this, websiteDataStore, isPrewarmed);
    auto& process = processProxy.get();
    initializeNewWebProcess(process, websiteDataStore, isPrewarmed);
    m_processes.append(WTFMove(processProxy));

    if (m_serviceWorkerProcessesTerminationTimer.isActive())
        m_serviceWorkerProcessesTerminationTimer.stop();

    return process;
}

RefPtr<WebProcessProxy> WebProcessPool::tryTakePrewarmedProcess(WebsiteDataStore& websiteDataStore)
{
    if (!m_prewarmedProcess)
        return nullptr;

    if (&m_prewarmedProcess->websiteDataStore() != &websiteDataStore)
        return nullptr;

    ASSERT(m_prewarmedProcess->isPrewarmed());
    m_prewarmedProcess->markIsNoLongerInPrewarmedPool();

    return std::exchange(m_prewarmedProcess, nullptr);
}

#if PLATFORM(MAC)
static void displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    auto screenProperties = WebCore::collectScreenProperties();
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        processPool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
        processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, flags));
#endif
    }
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

void WebProcessPool::initializeNewWebProcess(WebProcessProxy& process, WebsiteDataStore& websiteDataStore, WebProcessProxy::IsPrewarmed isPrewarmed)
{
    auto initializationActivityToken = process.throttler().backgroundActivityToken();
    auto scopeExit = makeScopeExit([&process, initializationActivityToken] {
        // Round-trip to the Web Content process before releasing the
        // initialization activity token, so that we're sure that all
        // messages sent from this function have been handled.
        process.isResponsive([initializationActivityToken] (bool) { });
    });

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

#if PLATFORM(IOS_FAMILY)
    setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    if (javaScriptConfigurationFileEnabled()) {
        parameters.javaScriptConfigurationDirectory = websiteDataStore.resolvedJavaScriptConfigurationDirectory();
        if (!parameters.javaScriptConfigurationDirectory.isEmpty())
            SandboxExtension::createHandleWithoutResolvingPath(parameters.javaScriptConfigurationDirectory, SandboxExtension::Type::ReadWrite, parameters.javaScriptConfigurationDirectoryExtensionHandle);
    }

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
    parameters.attrStyleEnabled = m_configuration->attrStyleEnabled();

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
#endif

#if PLATFORM(WAYLAND) && USE(EGL)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        parameters.waylandCompositorDisplayName = WaylandCompositor::singleton().displayName();
#endif

    parameters.resourceLoadStatisticsEnabled = websiteDataStore.resourceLoadStatisticsEnabled();
#if ENABLE(MEDIA_STREAM)
    parameters.shouldCaptureAudioInUIProcess = m_configuration->shouldCaptureAudioInUIProcess();
    parameters.shouldCaptureVideoInUIProcess = m_configuration->shouldCaptureVideoInUIProcess();
    parameters.shouldCaptureDisplayInUIProcess = m_configuration->shouldCaptureDisplayInUIProcess();
#endif

    parameters.presentingApplicationPID = m_configuration->presentingApplicationPID();

#if PLATFORM(COCOA)
    parameters.mediaMIMETypes = process.mediaMIMETypes();
#endif

#if PLATFORM(WPE)
    parameters.isServiceWorkerProcess = process.isServiceWorkerProcess();
#endif

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

    if (m_automationSession)
        process.send(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()), 0);

    ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

    if (isPrewarmed == WebProcessProxy::IsPrewarmed::Yes) {
        ASSERT(!m_prewarmedProcess);
        m_prewarmedProcess = &process;
        process.send(Messages::WebProcess::PrewarmGlobally(), 0);
    }

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton(); 
#endif

#if PLATFORM(MAC)
    registerDisplayConfigurationCallback();
#endif
}

void WebProcessPool::prewarmProcess(WebsiteDataStore* websiteDataStore, MayCreateDefaultDataStore mayCreateDefaultDataStore)
{
    if (m_prewarmedProcess && websiteDataStore && &m_prewarmedProcess->websiteDataStore() != websiteDataStore) {
        RELEASE_LOG(PerformanceLogging, "Shutting down prewarmed process %i because we needed a prewarmed process with a different data store", m_prewarmedProcess->processIdentifier());
        m_prewarmedProcess->shutDown();
        ASSERT(!m_prewarmedProcess);
    }

    if (m_prewarmedProcess)
        return;

    if (!websiteDataStore) {
        websiteDataStore = m_websiteDataStore ? &m_websiteDataStore->websiteDataStore() : nullptr;
        if (!websiteDataStore) {
            if (!m_processes.isEmpty())
                websiteDataStore = &m_processes.last()->websiteDataStore();
            else if (mayCreateDefaultDataStore == MayCreateDefaultDataStore::Yes || API::WebsiteDataStore::defaultDataStoreExists())
                websiteDataStore = &API::WebsiteDataStore::defaultDataStore()->websiteDataStore();
            else {
                RELEASE_LOG(PerformanceLogging, "Unable to prewarming a WebProcess because we could not find a usable data store");
                return;
            }
        }
    }

    ASSERT(websiteDataStore);

    RELEASE_LOG(PerformanceLogging, "Prewarming a WebProcess for performance");
    createNewWebProcess(*websiteDataStore, WebProcessProxy::IsPrewarmed::Yes);
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
        String sampleLogFilePath = makeString("WebProcess", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()), "pid", process->processIdentifier());
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::Type::ReadWrite, sampleLogSandboxHandle);
        
        process->send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    if (m_configuration->fullySynchronousModeIsAllowedForTesting())
        process->connection()->allowFullySynchronousModeForTesting();

    if (m_configuration->ignoreSynchronousMessagingTimeoutsForTesting())
        process->connection()->ignoreTimeoutsForTesting();

    m_connectionClient.didCreateConnection(this, process->webConnection());

    if (m_websiteDataStore)
        m_websiteDataStore->websiteDataStore().didCreateNetworkProcess();
}

void WebProcessPool::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (m_prewarmedProcess == process) {
        ASSERT(m_prewarmedProcess->isPrewarmed());
        m_prewarmedProcess = nullptr;
    }

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    RefPtr<WebProcessProxy> protect(process);
    if (m_processWithPageCache == process)
        m_processWithPageCache = nullptr;

    m_suspendedPages.removeAllMatching([process](auto& suspendedPage) {
        return &suspendedPage->process() == process;
    });

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
    if (!usesSingleWebProcess())
        return createNewWebProcess(websiteDataStore);

#if PLATFORM(COCOA)
    bool mustMatchDataStore = API::WebsiteDataStore::defaultDataStoreExists() && &websiteDataStore != &API::WebsiteDataStore::defaultDataStore()->websiteDataStore();
#else
    bool mustMatchDataStore = false;
#endif

    for (auto& process : m_processes) {
        if (mustMatchDataStore && &process->websiteDataStore() != &websiteDataStore)
            continue;
#if ENABLE(SERVICE_WORKER)
        if (is<ServiceWorkerProcessProxy>(*process))
            continue;
#endif
        return *process;
    }
    return createNewWebProcess(websiteDataStore);
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

    auto page = process->createWebPage(pageClient, WTFMove(pageConfiguration));

#if ENABLE(SERVICE_WORKER)
    ASSERT(!is<ServiceWorkerProcessProxy>(*process));

    if (!m_serviceWorkerPreferences) {
        m_serviceWorkerPreferences = page->preferencesStore();
        for (auto* serviceWorkerProcess : m_serviceWorkerProcesses.values())
            serviceWorkerProcess->updatePreferencesStore(*m_serviceWorkerPreferences);
    }
#endif

    bool enableProcessSwapOnCrossSiteNavigation = page->preferences().processSwapOnCrossSiteNavigationEnabled();
#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isFirefox() && !linkedOnOrAfter(WebKit::SDKVersion::FirstWithProcessSwapOnCrossSiteNavigation))
        enableProcessSwapOnCrossSiteNavigation = false;
#endif

    bool wasProcessSwappingOnNavigationEnabled = m_configuration->processSwapsOnNavigation();
    m_configuration->setProcessSwapsOnNavigationFromExperimentalFeatures(enableProcessSwapOnCrossSiteNavigation);
    if (wasProcessSwappingOnNavigationEnabled != m_configuration->processSwapsOnNavigation())
        m_webProcessCache->updateCapacity(*this);

    m_configuration->setShouldCaptureAudioInUIProcess(page->preferences().captureAudioInUIProcessEnabled());
    m_configuration->setShouldCaptureVideoInUIProcess(page->preferences().captureVideoInUIProcessEnabled());

    return page;
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
        serviceWorkerRegistrationDirectory = API::WebsiteDataStore::defaultDataStoreConfiguration()->serviceWorkerRegistrationDirectory();

    return m_mayHaveRegisteredServiceWorkers.ensure(serviceWorkerRegistrationDirectory, [&] {
        // FIXME: Make this computation on a background thread.
        return ServiceWorkerProcessProxy::hasRegisteredServiceWorkers(serviceWorkerRegistrationDirectory);
    }).iterator->value;
}
#endif

void WebProcessPool::pageBeginUsingWebsiteDataStore(uint64_t pageID, WebsiteDataStore& dataStore)
{
    auto result = m_sessionToPageIDsMap.add(dataStore.sessionID(), HashSet<uint64_t>()).iterator->value.add(pageID);
    ASSERT_UNUSED(result, result.isNewEntry);

    auto sessionID = dataStore.sessionID();
    if (sessionID.isEphemeral()) {
        ASSERT(dataStore.parameters().networkSessionParameters.sessionID == sessionID);
        if (m_networkProcess)
            m_networkProcess->addSession(makeRef(dataStore));
        dataStore.clearPendingCookies();
    } else if (sessionID != PAL::SessionID::defaultSessionID()) {
        if (m_networkProcess)
            m_networkProcess->addSession(makeRef(dataStore));
        dataStore.clearPendingCookies();
    }
}

void WebProcessPool::pageEndUsingWebsiteDataStore(uint64_t pageID, WebsiteDataStore& dataStore)
{
    auto sessionID = dataStore.sessionID();
    auto iterator = m_sessionToPageIDsMap.find(sessionID);
    ASSERT(iterator != m_sessionToPageIDsMap.end());

    auto takenPageID = iterator->value.take(pageID);
    ASSERT_UNUSED(takenPageID, takenPageID == pageID);

    if (iterator->value.isEmpty()) {
        m_sessionToPageIDsMap.remove(iterator);

        if (sessionID == PAL::SessionID::defaultSessionID())
            return;

        // The last user of this non-default PAL::SessionID is gone, so clean it up in the child processes.
        if (networkProcess())
            networkProcess()->removeSession(sessionID);

        m_webProcessCache->clearAllProcessesForSession(sessionID);
    }
}

bool WebProcessPool::hasPagesUsingWebsiteDataStore(WebsiteDataStore& dataStore) const
{
    return m_sessionToPageIDsMap.contains(dataStore.sessionID());
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

DownloadProxy* WebProcessPool::resumeDownload(WebPageProxy* initiatingPage, const API::Data* resumeData, const String& path)
{
    auto* downloadProxy = createDownloadProxy(ResourceRequest(), initiatingPage);
    PAL::SessionID sessionID = initiatingPage ? initiatingPage->sessionID() : PAL::SessionID::defaultSessionID();

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty())
        SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite, sandboxExtensionHandle);

    if (networkProcess()) {
        networkProcess()->send(Messages::NetworkProcess::ResumeDownload(sessionID, downloadProxy->downloadID(), resumeData->dataReference(), path, sandboxExtensionHandle), 0);
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

void WebProcessPool::didReachGoodTimeToPrewarm(WebsiteDataStore& dataStore)
{
    if (!configuration().isAutomaticProcessWarmingEnabled() || !configuration().processSwapsOnNavigation() || usesSingleWebProcess())
        return;

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_prewarmedProcess)
            RELEASE_LOG(PerformanceLogging, "Not automatically prewarming a WebProcess due to memory pressure");
        return;
    }

    prewarmProcess(&dataStore, MayCreateDefaultDataStore::No);
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
    RELEASE_LOG(PerformanceLogging, "%p - WebProcessPool::handleMemoryPressureWarning", this);


    clearSuspendedPages(AllowProcessCaching::No);
    m_webProcessCache->clear();

    if (m_prewarmedProcess)
        m_prewarmedProcess->shutDown();
    ASSERT(!m_prewarmedProcess);
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

void WebProcessPool::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    m_shouldUseFontSmoothing = useFontSmoothing;
    sendToAllProcesses(Messages::WebProcess::SetShouldUseFontSmoothing(useFontSmoothing));
}

void WebProcessPool::setResourceLoadStatisticsEnabled(bool enabled)
{
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
    sendToAllProcesses(Messages::AuxiliaryProcess::RegisterURLSchemeServiceWorkersCanHandle(urlScheme));
    if (m_networkProcess)
        m_networkProcess->send(Messages::AuxiliaryProcess::RegisterURLSchemeServiceWorkersCanHandle(urlScheme), 0);
}

void WebProcessPool::registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String& urlScheme)
{
    m_schemesToRegisterAsCanDisplayOnlyIfCanRequest.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCanDisplayOnlyIfCanRequest(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsCanDisplayOnlyIfCanRequest(urlScheme));
}

void WebProcessPool::updateMaxSuspendedPageCount()
{
    unsigned dummy = 0;
    Seconds dummyInterval;
    unsigned pageCacheSize = 0;
    calculateMemoryCacheSizes(m_configuration->cacheModel(), dummy, dummy, dummy, dummyInterval, pageCacheSize);

    m_maxSuspendedPageCount = pageCacheSize;

    while (m_suspendedPages.size() > m_maxSuspendedPageCount)
        m_suspendedPages.removeFirst();
}

void WebProcessPool::setCacheModel(CacheModel cacheModel)
{
    m_configuration->setCacheModel(cacheModel);
    updateMaxSuspendedPageCount();

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
    String sampleLogFilePath = makeString("WebProcess", static_cast<unsigned long long>(now.secondsSinceEpoch().seconds()));
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
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::ClearCachedCredentials(), 0);
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
    ensureNetworkProcess().syncAllCookies();
}

void WebProcessPool::setIDBPerOriginQuota(uint64_t quota)
{
#if ENABLE(INDEXED_DATABASE)
    ensureNetworkProcess().send(Messages::NetworkProcess::SetIDBPerOriginQuota(quota), 0);
#endif
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

    auto request = StatisticsRequest::create(DictionaryCallback::create(WTFMove(callbackFunction)));

    if (statisticsMask & StatisticsRequestTypeWebContent)
        requestWebContentStatistics(request.get());
    
    if (statisticsMask & StatisticsRequestTypeNetworking)
        requestNetworkingStatistics(request.get());
}

void WebProcessPool::requestWebContentStatistics(StatisticsRequest& request)
{
    // FIXME (Multi-WebProcess) <rdar://problem/13200059>: Make getting statistics from multiple WebProcesses work.
}

void WebProcessPool::requestNetworkingStatistics(StatisticsRequest& request)
{
    if (!m_networkProcess) {
        LOG_ERROR("Attempt to get NetworkProcess statistics but the NetworkProcess is unavailable");
        return;
    }

    uint64_t requestID = request.addOutstandingRequest();
    m_statisticsRequests.set(requestID, &request);
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

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    auto* webProcessProxy = webProcessProxyFromConnection(connection, m_processes);
    if (!webProcessProxy)
        return completionHandler({ });

    RefPtr<API::Object> returnData;
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), returnData);
    completionHandler(UserData(webProcessProxy->transformObjectsToHandles(returnData.get())));
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
#if PLATFORM(IOS_FAMILY)
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
        auto& networkProcess = ensureNetworkProcess();

        if (m_foregroundWebProcessCounter.value()) {
            if (!m_foregroundTokenForNetworkProcess) {
                m_foregroundTokenForNetworkProcess = networkProcess.throttler().foregroundActivityToken();
                networkProcess.sendProcessDidTransitionToForeground();
            }
            m_backgroundTokenForNetworkProcess = nullptr;
            return;
        }
        if (m_backgroundWebProcessCounter.value()) {
            if (!m_backgroundTokenForNetworkProcess) {
                m_backgroundTokenForNetworkProcess = networkProcess.throttler().backgroundActivityToken();
                networkProcess.sendProcessDidTransitionToBackground();
            }
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
    sendToNetworkingProcessRelaunchingIfNecessary(Messages::NetworkProcess::PostMessageToServiceWorkerClient(destination, WTFMove(message), source, sourceOrigin));
}

void WebProcessPool::postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& source, SWServerConnectionIdentifier connectionIdentifier)
{
    sendToNetworkingProcessRelaunchingIfNecessary(Messages::NetworkProcess::PostMessageToServiceWorker(destination, WTFMove(message), source, connectionIdentifier));
}
#endif

void WebProcessPool::reinstateNetworkProcessAssertionState(NetworkProcessProxy& newNetworkProcessProxy)
{
#if PLATFORM(IOS_FAMILY)
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

void WebProcessPool::addProcessToOriginCacheSet(WebProcessProxy& process, const URL& url)
{
    auto registrableDomain = toRegistrableDomain(url);
    auto result = m_swappedProcessesPerRegistrableDomain.add(registrableDomain, &process);
    if (!result.isNewEntry)
        result.iterator->value = &process;

    LOG(ProcessSwapping, "(ProcessSwapping) Registrable domain %s just saved a cached process with pid %i", registrableDomain.utf8().data(), process.processIdentifier());
    if (!result.isNewEntry)
        LOG(ProcessSwapping, "(ProcessSwapping) Note: It already had one saved");
}

void WebProcessPool::removeProcessFromOriginCacheSet(WebProcessProxy& process)
{
    LOG(ProcessSwapping, "(ProcessSwapping) Removing process with pid %i from the origin cache set", process.processIdentifier());

    // FIXME: This can be very inefficient as the number of remembered origins and processes grows
    Vector<String> registrableDomainsToRemove;
    for (auto entry : m_swappedProcessesPerRegistrableDomain) {
        if (entry.value == &process)
            registrableDomainsToRemove.append(entry.key);
    }

    for (auto& registrableDomain : registrableDomainsToRemove)
        m_swappedProcessesPerRegistrableDomain.remove(registrableDomain);
}

void WebProcessPool::processForNavigation(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& sourceURL, ProcessSwapRequestedByClient processSwapRequestedByClient, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&& completionHandler)
{
    processForNavigationInternal(page, navigation, sourceProcess.copyRef(), sourceURL, processSwapRequestedByClient, WTFMove(dataStore), [this, page = makeRefPtr(page), navigation = makeRef(navigation), sourceProcess = sourceProcess.copyRef(), sourceURL, processSwapRequestedByClient, completionHandler = WTFMove(completionHandler)](Ref<WebProcessProxy>&& process, SuspendedPageProxy* suspendedPage, const String& reason) mutable {
        // We are process-swapping so automatic process prewarming would be beneficial if the client has not explicitly enabled / disabled it.
        bool doingAnAutomaticProcessSwap = processSwapRequestedByClient == ProcessSwapRequestedByClient::No && process.ptr() != sourceProcess.ptr();
        if (doingAnAutomaticProcessSwap && !configuration().wasAutomaticProcessWarmingSetByClient() && !configuration().clientWouldBenefitFromAutomaticProcessPrewarming()) {
            RELEASE_LOG(PerformanceLogging, "Automatically turning on process prewarming because the client would benefit from it");
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

        completionHandler(WTFMove(process), suspendedPage, reason);
    });
}

void WebProcessPool::processForNavigationInternal(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& pageSourceURL, ProcessSwapRequestedByClient processSwapRequestedByClient, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&& completionHandler)
{
    auto& targetURL = navigation.currentRequest().url();
    auto registrableDomain = toRegistrableDomain(targetURL);

    auto createNewProcess = [this, protectedThis = makeRef(*this), page = makeRef(page), targetURL, registrableDomain, dataStore = dataStore.copyRef()] () -> Ref<WebProcessProxy> {
        if (auto process = webProcessCache().takeProcess(registrableDomain, dataStore))
            return process.releaseNonNull();

        // Check if we have a suspended page for the given registrable domain and use its process if we do, for performance reasons.
        if (auto process = findReusableSuspendedPageProcess(registrableDomain, page, dataStore)) {
            RELEASE_LOG(ProcessSwapping, "Using WebProcess %i from a SuspendedPage", process->processIdentifier());
            return process.releaseNonNull();
        }

        if (auto process = tryTakePrewarmedProcess(dataStore)) {
            RELEASE_LOG(ProcessSwapping, "Using prewarmed process %i", process->processIdentifier());
            tryPrewarmWithDomainInformation(*process, targetURL);
            return process.releaseNonNull();
        }

        RELEASE_LOG(ProcessSwapping, "Launching a new process");
        return createNewWebProcess(dataStore);
    };

    if (usesSingleWebProcess())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Single WebProcess mode is enabled"_s);

    if (processSwapRequestedByClient == ProcessSwapRequestedByClient::Yes)
        return completionHandler(createNewProcess(), nullptr, "Process swap was requested by the client"_s);

    if (!m_configuration->processSwapsOnNavigation())
        return completionHandler(WTFMove(sourceProcess), nullptr, "Feature is disabled"_s);

    if (m_automationSession)
        return completionHandler(WTFMove(sourceProcess), nullptr, "An automation session is active"_s);

    if (!sourceProcess->hasCommittedAnyProvisionalLoads()) {
        tryPrewarmWithDomainInformation(sourceProcess, targetURL);
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

        if (RefPtr<WebProcessProxy> process = WebProcessProxy::processForIdentifier(targetItem->lastProcessIdentifier())) {
            // FIXME: Architecturally we do not currently support multiple WebPage's with the same ID in a given WebProcess.
            // In the case where this WebProcess has a SuspendedPageProxy for this WebPage, we can throw it away to support
            // WebProcess re-use.
            removeAllSuspendedPagesForPage(page, process.get());

            // Make sure we remove the process from the cache if it is in there since we're about to use it.
            if (process->isInProcessCache()) {
                webProcessCache().removeProcess(*process, WebProcessCache::ShouldShutDownProcess::No);
                ASSERT(!process->isInProcessCache());
            }

            return completionHandler(process.releaseNonNull(), nullptr, "Using target back/forward item's process"_s);
        }
    }

    if (navigation.treatAsSameOriginNavigation())
        return completionHandler(WTFMove(sourceProcess), nullptr, "The treatAsSameOriginNavigation flag is set"_s);

    URL sourceURL;
    if (page.isPageOpenedByDOMShowingInitialEmptyDocument() && !navigation.requesterOrigin().isEmpty())
        sourceURL = URL { URL(), navigation.requesterOrigin().toString() };
    else
        sourceURL = pageSourceURL;

    if (sourceURL.isEmpty() && page.configuration().relatedPage()) {
        sourceURL = URL { { }, page.configuration().relatedPage()->pageLoadState().url() };
        RELEASE_LOG(ProcessSwapping, "Using related page %p's URL as source URL for process swap decision", page.configuration().relatedPage());
    }

    if (!sourceURL.isValid() || !targetURL.isValid() || sourceURL.isEmpty() || sourceURL.protocolIsAbout() || registrableDomainsAreEqual(sourceURL, targetURL))
        return completionHandler(WTFMove(sourceProcess), nullptr, "Navigation is same-site"_s);

    String reason = "Navigation is cross-site"_s;
    
    if (m_configuration->alwaysKeepAndReuseSwappedProcesses()) {
        LOG(ProcessSwapping, "(ProcessSwapping) Considering re-use of a previously cached process for domain %s", registrableDomain.utf8().data());

        if (auto* process = m_swappedProcessesPerRegistrableDomain.get(registrableDomain)) {
            if (&process->websiteDataStore() == dataStore.ptr()) {
                LOG(ProcessSwapping, "(ProcessSwapping) Reusing a previously cached process with pid %i to continue navigation to URL %s", process->processIdentifier(), targetURL.string().utf8().data());

                // FIXME: Architecturally we do not currently support multiple WebPage's with the same ID in a given WebProcess.
                // In the case where this WebProcess has a SuspendedPageProxy for this WebPage, we can throw it away to support
                // WebProcess re-use.
                // In the future it would be great to refactor-out this limitation (https://bugs.webkit.org/show_bug.cgi?id=191166).
                removeAllSuspendedPagesForPage(page, process);

                return completionHandler(makeRef(*process), nullptr, reason);
            }
        }
    }

    return completionHandler(createNewProcess(), nullptr, reason);
}

RefPtr<WebProcessProxy> WebProcessPool::findReusableSuspendedPageProcess(const String& registrableDomain, WebPageProxy& page, WebsiteDataStore& dataStore)
{
    auto it = m_suspendedPages.findIf([&](auto& suspendedPage) {
        return suspendedPage->registrableDomain() == registrableDomain && &suspendedPage->process().websiteDataStore() == &dataStore;
    });
    if (it == m_suspendedPages.end())
        return nullptr;

    Ref<WebProcessProxy> process = (*it)->process();

    // FIXME: If the SuspendedPage is for this page, then we need to destroy the suspended page as we do not support having
    // multiple WebPages with the same ID in a given WebProcess currently (https://bugs.webkit.org/show_bug.cgi?id=191166).
    if (&(*it)->page() == &page)
        m_suspendedPages.remove(it);


    return WTFMove(process);
}

void WebProcessPool::addSuspendedPage(std::unique_ptr<SuspendedPageProxy>&& suspendedPage)
{
    if (!m_maxSuspendedPageCount)
        return;

    if (m_suspendedPages.size() >= m_maxSuspendedPageCount)
        m_suspendedPages.removeFirst();

    m_suspendedPages.append(WTFMove(suspendedPage));
}

void WebProcessPool::removeAllSuspendedPagesForPage(WebPageProxy& page, WebProcessProxy* process)
{
    m_suspendedPages.removeAllMatching([&page, process](auto& suspendedPage) {
        return &suspendedPage->page() == &page && (!process || &suspendedPage->process() == process);
    });
}

void WebProcessPool::closeFailedSuspendedPagesForPage(WebPageProxy& page)
{
    for (auto& suspendedPage : m_suspendedPages) {
        if (&suspendedPage->page() == &page && suspendedPage->failedToSuspend())
            suspendedPage->close();
    }
}

std::unique_ptr<SuspendedPageProxy> WebProcessPool::takeSuspendedPage(SuspendedPageProxy& suspendedPage)
{
    return m_suspendedPages.takeFirst([&suspendedPage](auto& item) {
        return item.get() == &suspendedPage;
    });
}

void WebProcessPool::removeSuspendedPage(SuspendedPageProxy& suspendedPage)
{
    auto it = m_suspendedPages.findIf([&suspendedPage](auto& item) {
        return item.get() == &suspendedPage;
    });
    if (it != m_suspendedPages.end())
        m_suspendedPages.remove(it);
}

bool WebProcessPool::hasSuspendedPageFor(WebProcessProxy& process, WebPageProxy& page) const
{
    return m_suspendedPages.findIf([&process, &page](auto& suspendedPage) {
        return &suspendedPage->process() == &process && &suspendedPage->page() == &page;
    }) != m_suspendedPages.end();
}

void WebProcessPool::clearSuspendedPages(AllowProcessCaching allowProcessCaching)
{
    HashSet<RefPtr<WebProcessProxy>> processes;
    for (auto& suspendedPage : m_suspendedPages)
        processes.add(&suspendedPage->process());

    m_suspendedPages.clear();

    for (auto& process : processes)
        process->maybeShutDown(allowProcessCaching);
}

void WebProcessPool::addMockMediaDevice(const MockMediaDevice& device)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::addDevice(device);
    sendToAllProcesses(Messages::WebProcess::AddMockMediaDevice { device });
#endif
}

void WebProcessPool::clearMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::setDevices({ });
    sendToAllProcesses(Messages::WebProcess::ClearMockMediaDevices { });
#endif
}

void WebProcessPool::removeMockMediaDevice(const String& persistentId)
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::removeDevice(persistentId);
    sendToAllProcesses(Messages::WebProcess::RemoveMockMediaDevice { persistentId });
#endif
}

void WebProcessPool::resetMockMediaDevices()
{
#if ENABLE(MEDIA_STREAM)
    MockRealtimeMediaSourceCenter::resetDevices();
    sendToAllProcesses(Messages::WebProcess::ResetMockMediaDevices { });
#endif
}

void WebProcessPool::sendDisplayConfigurationChangedMessageForTesting()
{
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    auto display = CGSMainDisplayID();

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, kCGDisplayBeginConfigurationFlag));
        processPool->sendToAllProcesses(Messages::WebProcess::DisplayConfigurationChanged(display, kCGDisplaySetModeFlag | kCGDisplayDesktopShapeChangedFlag));
    }
#endif
}

void WebProcessPool::didCollectPrewarmInformation(const String& registrableDomain, const WebCore::PrewarmInformation& prewarmInformation)
{
    static const size_t maximumSizeToPreventUnlimitedGrowth = 100;
    if (m_prewarmInformationPerRegistrableDomain.size() == maximumSizeToPreventUnlimitedGrowth)
        m_prewarmInformationPerRegistrableDomain.remove(m_prewarmInformationPerRegistrableDomain.random());

    auto& value = m_prewarmInformationPerRegistrableDomain.ensure(registrableDomain, [] {
        return std::make_unique<WebCore::PrewarmInformation>();
    }).iterator->value;

    *value = prewarmInformation;
}

void WebProcessPool::tryPrewarmWithDomainInformation(WebProcessProxy& process, const URL& url)
{
    auto* prewarmInformation = m_prewarmInformationPerRegistrableDomain.get(toRegistrableDomain(url));
    if (!prewarmInformation)
        return;
    process.send(Messages::WebProcess::PrewarmWithDomainInformation(*prewarmInformation), 0);
}

void WebProcessPool::clearCurrentModifierStateForTesting()
{
    sendToAllProcesses(Messages::WebProcess::ClearCurrentModifierStateForTesting());
}

void WebProcessPool::dumpAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_networkProcess) {
        completionHandler(emptyString());
        return;
    }

    m_networkProcess->dumpAdClickAttribution(sessionID, WTFMove(completionHandler));
}

void WebProcessPool::clearAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!m_networkProcess) {
        completionHandler();
        return;
    }
    
    m_networkProcess->clearAdClickAttribution(sessionID, WTFMove(completionHandler));
}
    
void WebProcessPool::committedCrossSiteLoadWithLinkDecoration(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, uint64_t pageID)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!m_networkProcess)
        return;

    m_networkProcess->committedCrossSiteLoadWithLinkDecoration(sessionID, fromDomain, toDomain, pageID, [] { });
#else
    UNUSED_PARAM(sessionID);
    UNUSED_PARAM(fromDomain);
    UNUSED_PARAM(toDomain);
    UNUSED_PARAM(pageID);
#endif
}

} // namespace WebKit
