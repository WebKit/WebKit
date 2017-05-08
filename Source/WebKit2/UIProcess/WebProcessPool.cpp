/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include "APILegacyContextHistoryClient.h"
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "CustomProtocolManagerMessages.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "GamepadData.h"
#include "HighPerformanceGraphicsUsageSampler.h"
#include "LogInitialization.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
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
#include "WebIconDatabase.h"
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
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SessionID.h>
#include <WebCore/URLParser.h>
#include <runtime/JSCInlines.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(DATABASE_PROCESS)
#include "DatabaseProcessCreationParameters.h"
#include "DatabaseProcessMessages.h"
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

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, processPoolCounter, ("WebProcessPool"));

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

const Vector<WebProcessPool*>& WebProcessPool::allProcessPools()
{
    return processPools();
}

static WebsiteDataStore::Configuration legacyWebsiteDataStoreConfiguration(API::ProcessPoolConfiguration& processPoolConfiguration)
{
    WebsiteDataStore::Configuration configuration;

    configuration.localStorageDirectory = processPoolConfiguration.localStorageDirectory();
    configuration.webSQLDatabaseDirectory = processPoolConfiguration.webSQLDatabaseDirectory();
    configuration.applicationCacheDirectory = processPoolConfiguration.applicationCacheDirectory();
    configuration.applicationCacheFlatFileSubdirectoryName = processPoolConfiguration.applicationCacheFlatFileSubdirectoryName();
    configuration.mediaCacheDirectory = processPoolConfiguration.mediaCacheDirectory();
    configuration.mediaKeysStorageDirectory = processPoolConfiguration.mediaKeysStorageDirectory();
    configuration.networkCacheDirectory = processPoolConfiguration.diskCacheDirectory();

    // This is needed to support legacy WK2 clients, which did not have resource load statistics.
    configuration.resourceLoadStatisticsDirectory = API::WebsiteDataStore::defaultResourceLoadStatisticsDirectory();

    return configuration;
}

static HashSet<String, ASCIICaseInsensitiveHash>& globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> set;
    return set;
}

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_haveInitialEmptyProcess(false)
    , m_processWithPageCache(0)
    , m_defaultPageGroup(WebPageGroup::createNonNull())
    , m_automationClient(std::make_unique<API::AutomationClient>())
    , m_downloadClient(std::make_unique<API::DownloadClient>())
    , m_historyClient(std::make_unique<API::LegacyContextHistoryClient>())
    , m_customProtocolManagerClient(std::make_unique<API::CustomProtocolManagerClient>())
    , m_visitedLinkStore(VisitedLinkStore::create())
    , m_visitedLinksPopulated(false)
    , m_plugInAutoStartProvider(this)
    , m_alwaysUsesComplexTextCodePath(false)
    , m_shouldUseFontSmoothing(true)
    , m_memorySamplerEnabled(false)
    , m_memorySamplerInterval(1400.0)
    , m_websiteDataStore(m_configuration->shouldHaveLegacyDataStore() ? API::WebsiteDataStore::create(legacyWebsiteDataStoreConfiguration(m_configuration)).ptr() : nullptr)
#if PLATFORM(MAC)
    , m_highPerformanceGraphicsUsageSampler(std::make_unique<HighPerformanceGraphicsUsageSampler>(*this))
    , m_perActivityStateCPUUsageSampler(std::make_unique<PerActivityStateCPUUsageSampler>(*this))
#endif
    , m_shouldUseTestingNetworkSession(false)
    , m_processTerminationEnabled(true)
    , m_canHandleHTTPSServerTrustEvaluation(true)
    , m_didNetworkProcessCrash(false)
    , m_memoryCacheDisabled(false)
    , m_alwaysRunsAtBackgroundPriority(m_configuration->alwaysRunsAtBackgroundPriority())
    , m_userObservablePageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_hiddenPageThrottlingAutoIncreasesCounter([this](RefCounterEvent) { m_hiddenPageThrottlingTimer.startOneShot(0); })
    , m_hiddenPageThrottlingTimer(RunLoop::main(), this, &WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit)
{
    for (auto& scheme : m_configuration->alwaysRevalidatedURLSchemes())
        m_schemesToRegisterAsAlwaysRevalidated.add(scheme);

    for (const auto& urlScheme : m_configuration->cachePartitionedURLSchemes())
        m_schemesToRegisterAsCachePartitioned.add(urlScheme);

    platformInitialize();

    addMessageReceiver(Messages::WebProcessPool::messageReceiverName(), *this);

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    m_iconDatabase = WebIconDatabase::create(this);

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
}

#if !PLATFORM(COCOA)
void WebProcessPool::platformInitialize()
{
}
#endif

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

    m_iconDatabase->invalidate();
    m_iconDatabase->clearProcessPool();
    WebIconDatabase* rawIconDatabase = m_iconDatabase.leakRef();
    rawIconDatabase->derefWhenAppropriate();

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

void WebProcessPool::initializeInjectedBundleClient(const WKContextInjectedBundleClientBase* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebProcessPool::initializeConnectionClient(const WKContextConnectionClientBase* client)
{
    m_connectionClient.initialize(client);
}

void WebProcessPool::setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient> historyClient)
{
    if (!historyClient)
        m_historyClient = std::make_unique<API::LegacyContextHistoryClient>();
    else
        m_historyClient = WTFMove(historyClient);
}

void WebProcessPool::setDownloadClient(std::unique_ptr<API::DownloadClient> downloadClient)
{
    if (!downloadClient)
        m_downloadClient = std::make_unique<API::DownloadClient>();
    else
        m_downloadClient = WTFMove(downloadClient);
}

void WebProcessPool::setAutomationClient(std::unique_ptr<API::AutomationClient> automationClient)
{
    if (!automationClient)
        m_automationClient = std::make_unique<API::AutomationClient>();
    else
        m_automationClient = WTFMove(automationClient);
}

void WebProcessPool::setCustomProtocolManagerClient(std::unique_ptr<API::CustomProtocolManagerClient>&& customProtocolManagerClient)
{
    if (!customProtocolManagerClient)
        m_customProtocolManagerClient = std::make_unique<API::CustomProtocolManagerClient>();
    else
        m_customProtocolManagerClient = WTFMove(customProtocolManagerClient);
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

NetworkProcessProxy& WebProcessPool::ensureNetworkProcess()
{
    if (m_networkProcess)
        return *m_networkProcess;

    m_networkProcess = NetworkProcessProxy::create(*this);

    NetworkProcessCreationParameters parameters;

    parameters.privateBrowsingEnabled = WebPreferences::anyPagesAreUsingPrivateBrowsing();

    parameters.cacheModel = cacheModel();
    parameters.diskCacheSizeOverride = m_configuration->diskCacheSizeOverride();
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

#if PLATFORM(IOS)
    String cookieStorageDirectory = this->cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = this->networkingCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String parentBundleDirectory = this->parentBundleDirectory();
    if (!parentBundleDirectory.isEmpty())
        SandboxExtension::createHandle(parentBundleDirectory, SandboxExtension::ReadOnly, parameters.parentBundleDirectoryExtensionHandle);
#endif

#if OS(LINUX)
    if (MemoryPressureMonitor::isEnabled())
        parameters.memoryPressureMonitorHandle = MemoryPressureMonitor::singleton().createHandle();
#endif

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    m_networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

#if PLATFORM(COCOA)
    m_networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif

    if (m_didNetworkProcessCrash) {
        m_didNetworkProcessCrash = false;
        for (auto& process : m_processes)
            process->reinstateNetworkProcessAssertionState(*m_networkProcess);
    }

    return *m_networkProcess;
}

void WebProcessPool::networkProcessCrashed(NetworkProcessProxy* networkProcessProxy)
{
    ASSERT(m_networkProcess);
    ASSERT(networkProcessProxy == m_networkProcess.get());
    m_didNetworkProcessCrash = true;

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(networkProcessProxy);

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

#if ENABLE(DATABASE_PROCESS)
void WebProcessPool::ensureDatabaseProcess()
{
    if (m_databaseProcess)
        return;

    m_databaseProcess = DatabaseProcessProxy::create(this);

    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    DatabaseProcessCreationParameters parameters;
#if ENABLE(INDEXED_DATABASE)
    ASSERT(!m_configuration->indexedDBDatabaseDirectory().isEmpty());
    parameters.indexedDatabaseDirectory = m_configuration->indexedDBDatabaseDirectory();

    SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

    m_databaseProcess->send(Messages::DatabaseProcess::InitializeDatabaseProcess(parameters), 0);
}

void WebProcessPool::getDatabaseProcessConnection(Ref<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>&& reply)
{
    ensureDatabaseProcess();

    m_databaseProcess->getDatabaseProcessConnection(WTFMove(reply));
}

void WebProcessPool::databaseProcessCrashed(DatabaseProcessProxy* databaseProcessProxy)
{
    ASSERT(m_databaseProcess);
    ASSERT(databaseProcessProxy == m_databaseProcess.get());

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(databaseProcessProxy);

    m_client.databaseProcessDidCrash(this);
    m_databaseProcess = nullptr;
}
#endif

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
    m_iconDatabase->setPrivateBrowsingEnabled(privateBrowsingEnabled);

    if (networkProcess()) {
        if (privateBrowsingEnabled)
            networkProcess()->send(Messages::NetworkProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
        else
            networkProcess()->send(Messages::NetworkProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
    }

    if (privateBrowsingEnabled)
        sendToAllProcesses(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
    else
        sendToAllProcesses(Messages::WebProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
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

    platformResolvePathsForSandboxExtensions();
}

WebProcessProxy& WebProcessPool::createNewWebProcess()
{
    ensureNetworkProcess();

    Ref<WebProcessProxy> process = WebProcessProxy::create(*this);

    WebProcessCreationParameters parameters;

    parameters.injectedBundlePath = m_resolvedPaths.injectedBundlePath;
    if (!parameters.injectedBundlePath.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.injectedBundlePath, SandboxExtension::ReadOnly, parameters.injectedBundlePathExtensionHandle);

    parameters.applicationCacheDirectory = m_resolvedPaths.applicationCacheDirectory;
    if (!parameters.applicationCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.applicationCacheDirectory, SandboxExtension::ReadWrite, parameters.applicationCacheDirectoryExtensionHandle);

    parameters.applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName();

    parameters.webSQLDatabaseDirectory = m_resolvedPaths.webSQLDatabaseDirectory;
    if (!parameters.webSQLDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.webSQLDatabaseDirectory, SandboxExtension::ReadWrite, parameters.webSQLDatabaseDirectoryExtensionHandle);

    parameters.mediaCacheDirectory = m_resolvedPaths.mediaCacheDirectory;
    if (!parameters.mediaCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaCacheDirectory, SandboxExtension::ReadWrite, parameters.mediaCacheDirectoryExtensionHandle);

    parameters.mediaKeyStorageDirectory = m_resolvedPaths.mediaKeyStorageDirectory;
    if (!parameters.mediaKeyStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaKeyStorageDirectory, SandboxExtension::ReadWrite, parameters.mediaKeyStorageDirectoryExtensionHandle);

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    parameters.cacheModel = cacheModel();
    parameters.languages = userPreferredLanguages();

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegisteredAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToRegisterAsBypassingContentSecurityPolicy, parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);
    copyToVector(m_schemesToRegisterAsLocal, parameters.urlSchemesRegisteredAsLocal);
    copyToVector(m_schemesToRegisterAsNoAccess, parameters.urlSchemesRegisteredAsNoAccess);
    copyToVector(m_schemesToRegisterAsDisplayIsolated, parameters.urlSchemesRegisteredAsDisplayIsolated);
    copyToVector(m_schemesToRegisterAsCORSEnabled, parameters.urlSchemesRegisteredAsCORSEnabled);
    copyToVector(m_schemesToRegisterAsAlwaysRevalidated, parameters.urlSchemesRegisteredAsAlwaysRevalidated);
    copyToVector(m_schemesToRegisterAsCachePartitioned, parameters.urlSchemesRegisteredAsCachePartitioned);

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;

    parameters.iconDatabaseEnabled = !iconDatabasePath().isEmpty();

    parameters.terminationTimeout = 0;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    supplement<WebNotificationManagerProxy>()->populateCopyOfNotificationPermissions(parameters.notificationPermissions);
#endif

    parameters.plugInAutoStartOriginHashes = m_plugInAutoStartProvider.autoStartOriginHashesCopy();
    copyToVector(m_plugInAutoStartProvider.autoStartOrigins(), parameters.plugInAutoStartOrigins);

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

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient.getInjectedBundleInitializationUserData(this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process->transformObjectsToHandles(injectedBundleInitializationUserData.get()));

    process->send(Messages::WebProcess::InitializeWebProcess(parameters), 0);

#if PLATFORM(COCOA)
    process->send(Messages::WebProcess::SetQOS(webProcessLatencyQOS(), webProcessThroughputQOS()), 0);
#endif

    if (WebPreferences::anyPagesAreUsingPrivateBrowsing())
        process->send(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);

    if (m_automationSession)
        process->send(Messages::WebProcess::EnsureAutomationSessionProxy(m_automationSession->sessionIdentifier()), 0);

    m_processes.append(process.ptr());

    ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton(); 
#endif

    return process;
}

void WebProcessPool::warmInitialProcess()  
{
    if (m_haveInitialEmptyProcess) {
        ASSERT(!m_processes.isEmpty());
        return;
    }

    if (m_processes.size() >= maximumNumberOfProcesses())
        return;

    createNewWebProcess();
    m_haveInitialEmptyProcess = true;
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

    if (!m_processTerminationEnabled)
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
        double now = WTF::currentTime();
        String sampleLogFilePath = String::format("WebProcess%llupid%d", static_cast<unsigned long long>(now), process->processIdentifier());
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
        
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

    if (m_haveInitialEmptyProcess && process == m_processes.last())
        m_haveInitialEmptyProcess = false;

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    RefPtr<WebProcessProxy> protect(process);
    if (m_processWithPageCache == process)
        m_processWithPageCache = nullptr;

    static_cast<WebContextSupplement*>(supplement<WebGeolocationManagerProxy>())->processDidClose(process);

    m_processes.removeFirst(process);

#if ENABLE(GAMEPAD)
    if (m_processesUsingGamepads.contains(process))
        processStoppedUsingGamepads(*process);
#endif
}

WebProcessProxy& WebProcessPool::createNewWebProcessRespectingProcessCountLimit()
{
    if (m_processes.size() < maximumNumberOfProcesses())
        return createNewWebProcess();

    // Choose the process with fewest pages.
    auto& process = *std::min_element(m_processes.begin(), m_processes.end(), [](const RefPtr<WebProcessProxy>& a, const RefPtr<WebProcessProxy>& b) {
        return a->pageCount() < b->pageCount();
    });

    return *process;
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
        ASSERT(!pageConfiguration->sessionID().isValid());
        pageConfiguration->setWebsiteDataStore(m_websiteDataStore.get());
        pageConfiguration->setSessionID(pageConfiguration->preferences()->privateBrowsingEnabled() ? SessionID::legacyPrivateSessionID() : SessionID::defaultSessionID());
    }

    RefPtr<WebProcessProxy> process;
    if (m_haveInitialEmptyProcess) {
        process = m_processes.last();
        m_haveInitialEmptyProcess = false;
    } else if (pageConfiguration->relatedPage()) {
        // Sharing processes, e.g. when creating the page via window.open().
        process = &pageConfiguration->relatedPage()->process();
    } else
        process = &createNewWebProcessRespectingProcessCountLimit();

    return process->createWebPage(pageClient, WTFMove(pageConfiguration));
}

DownloadProxy* WebProcessPool::download(WebPageProxy* initiatingPage, const ResourceRequest& request, const String& suggestedFilename)
{
    DownloadProxy* downloadProxy = createDownloadProxy(request);
    SessionID sessionID = initiatingPage ? initiatingPage->sessionID() : SessionID::defaultSessionID();

    if (initiatingPage)
        initiatingPage->handleDownloadRequest(downloadProxy);

    if (networkProcess()) {
        ResourceRequest updatedRequest(request);
        // Request's firstPartyForCookies will be used as Original URL of the download request.
        // We set the value to top level document's URL.
        if (initiatingPage)
            updatedRequest.setFirstPartyForCookies(URL(URL(), initiatingPage->pageLoadState().url()));
        else
            updatedRequest.setFirstPartyForCookies(URL());
        networkProcess()->send(Messages::NetworkProcess::DownloadRequest(sessionID, downloadProxy->downloadID(), updatedRequest, suggestedFilename), 0);
        return downloadProxy;
    }

    return downloadProxy;
}

DownloadProxy* WebProcessPool::resumeDownload(const API::Data* resumeData, const String& path)
{
    DownloadProxy* downloadProxy = createDownloadProxy(ResourceRequest());

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty())
        SandboxExtension::createHandle(path, SandboxExtension::ReadWrite, sandboxExtensionHandle);

    if (networkProcess()) {
        // FIXME: If we started a download in an ephemeral session and that session still exists, we should find a way to use that same session.
        networkProcess()->send(Messages::NetworkProcess::ResumeDownload(SessionID::defaultSessionID(), downloadProxy->downloadID(), resumeData->dataReference(), path, sandboxExtensionHandle), 0);
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

pid_t WebProcessPool::networkProcessIdentifier()
{
    if (!m_networkProcess)
        return 0;

    return m_networkProcess->processIdentifier();
}

pid_t WebProcessPool::databaseProcessIdentifier()
{
#if ENABLE(DATABASE_PROCESS)
    if (!m_databaseProcess)
        return 0;

    return m_databaseProcess->processIdentifier();
#else
    return 0;
#endif
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

void WebProcessPool::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme));
}

void WebProcessPool::registerURLSchemeAsSecure(const String& urlScheme)
{
    m_schemesToRegisterAsSecure.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    m_schemesToRegisterAsBypassingContentSecurityPolicy.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
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

void WebProcessPool::registerURLSchemeAsLocal(const String& urlScheme)
{
    m_schemesToRegisterAsLocal.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsLocal(urlScheme));
}

void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    m_schemesToRegisterAsNoAccess.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
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

DownloadProxy* WebProcessPool::createDownloadProxy(const ResourceRequest& request)
{
    return ensureNetworkProcess().createDownloadProxy(request);
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
    double now = WTF::currentTime();
    String sampleLogFilePath = String::format("WebProcess%llu", static_cast<unsigned long long>(now));
    sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
    
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

void WebProcessPool::setIconDatabasePath(const String& path)
{
    m_overrideIconDatabasePath = path;
    if (!m_overrideIconDatabasePath.isEmpty()) {
        // This implicitly enables the database on UI process side.
        m_iconDatabase->setDatabasePath(path);
    }
}

String WebProcessPool::iconDatabasePath() const
{
    if (!m_overrideIconDatabasePath.isNull())
        return m_overrideIconDatabasePath;

    return platformDefaultIconDatabasePath();
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

void WebProcessPool::clearCachedCredentials()
{
    sendToAllProcesses(Messages::WebProcess::ClearCachedCredentials());
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::ClearCachedCredentials(), 0);
}

void WebProcessPool::terminateDatabaseProcess()
{
#if ENABLE(DATABASE_PROCESS)
    if (!m_databaseProcess)
        return;

    m_databaseProcess->terminate();
    m_databaseProcess = nullptr;
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

void WebProcessPool::getStatistics(uint32_t statisticsMask, std::function<void (API::Dictionary*, CallbackBase::Error)> callbackFunction)
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
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get());
}

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, UserData& returnUserData)
{
    auto* webProcessProxy = webProcessProxyFromConnection(connection, m_processes);
    if (!webProcessProxy)
        return;

    RefPtr<API::Object> returnData;
    m_injectedBundleClient.didReceiveSynchronousMessageFromInjectedBundle(this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), returnData);
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
    gamepadDatas.resize(gamepads.size());
    for (size_t i = 0; i < gamepads.size(); ++i) {
        if (!gamepads[i])
            continue;
        gamepadDatas[i] = gamepads[i]->fullGamepadData();
    }

    for (auto& process : m_processesUsingGamepads)
        process->send(Messages::WebProcess::SetInitialGamepads(gamepadDatas), 0);
}

#endif // ENABLE(GAMEPAD)

void WebProcessPool::garbageCollectJavaScriptObjects()
{
    sendToAllProcesses(Messages::WebProcess::GarbageCollectJavaScriptObjects());
}

void WebProcessPool::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetJavaScriptGarbageCollectorTimerEnabled(flag));
}

void WebProcessPool::addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, SessionID sessionID)
{
    m_plugInAutoStartProvider.addAutoStartOriginHash(pageOrigin, plugInOriginHash, sessionID);
}

void WebProcessPool::plugInDidReceiveUserInteraction(unsigned plugInOriginHash, SessionID sessionID)
{
    m_plugInAutoStartProvider.didReceiveUserInteraction(plugInOriginHash, sessionID);
}

PassRefPtr<API::Dictionary> WebProcessPool::plugInAutoStartOriginHashes() const
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

void WebProcessPool::setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary& dictionary, double time)
{
    m_plugInAutoStartProvider.setAutoStartOriginsFilteringOutEntriesAddedAfterTime(dictionary, time);
}

void WebProcessPool::registerSchemeForCustomProtocol(const String& scheme)
{
    if (!globalURLSchemesWithCustomProtocolHandlers().contains(scheme))
        m_urlSchemesRegisteredForCustomProtocols.add(scheme);
    sendToNetworkingProcess(Messages::CustomProtocolManager::RegisterScheme(scheme));
}

void WebProcessPool::unregisterSchemeForCustomProtocol(const String& scheme)
{
    m_urlSchemesRegisteredForCustomProtocols.remove(scheme);
    sendToNetworkingProcess(Messages::CustomProtocolManager::UnregisterScheme(scheme));
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    HashMap<String, HashMap<String, uint8_t>> policiesByIdentifier;
    if (m_pluginLoadClientPolicies.contains(host))
        policiesByIdentifier = m_pluginLoadClientPolicies.get(host);

    HashMap<String, uint8_t> versionsToPolicies;
    if (policiesByIdentifier.contains(bundleIdentifier))
        versionsToPolicies = policiesByIdentifier.get(bundleIdentifier);

    versionsToPolicies.set(versionString, policy);
    policiesByIdentifier.set(bundleIdentifier, versionsToPolicies);
    m_pluginLoadClientPolicies.set(host, policiesByIdentifier);

    sendToAllProcesses(Messages::WebProcess::SetPluginLoadClientPolicy(policy, host, bundleIdentifier, versionString));
}

void WebProcessPool::clearPluginClientPolicies()
{
    m_pluginLoadClientPolicies.clear();
    sendToAllProcesses(Messages::WebProcess::ClearPluginClientPolicies());
}
#endif

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
    sendToAllProcesses(Messages::WebProcess::SetHiddenPageTimerThrottlingIncreaseLimit(limitInMilliseconds));
}

void WebProcessPool::reportWebContentCPUTime(int64_t cpuTime, uint64_t activityState)
{
#if PLATFORM(MAC)
    if (m_perActivityStateCPUUsageSampler)
        m_perActivityStateCPUUsageSampler->reportWebContentCPUTime(cpuTime, static_cast<WebCore::ActivityStateForCPUSampling>(activityState));
#else
    UNUSED_PARAM(cpuTime);
    UNUSED_PARAM(activityState);
#endif
}

} // namespace WebKit
