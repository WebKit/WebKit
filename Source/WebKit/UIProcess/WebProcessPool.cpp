/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "GPUProcessConnectionInfo.h"
#include "GamepadData.h"
#include "HighPerformanceGraphicsUsageSampler.h"
#include "LegacyGlobalSettings.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "PerActivityStateCPUUsageSampler.h"
#include "PluginProcessManager.h"
#include "SandboxExtension.h"
#include "TextChecker.h"
#include "UIGamepad.h"
#include "UIGamepadProvider.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContextSupplement.h"
#include "WebCookieManagerProxy.h"
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
#include <WebCore/LogInitialization.h>
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
#include "VersionChecks.h"
#endif

#if PLATFORM(MAC)
#include <pal/spi/cg/CoreGraphicsSPI.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebProcessPoolAdditions.h>
#else
#define WEB_PROCESS_POOL_ADDITIONS
#define WEB_PROCESS_POOL_ADDITIONS_2
#define WEB_PROCESS_POOL_ADDITIONS_3
#define WEB_PROCESS_POOL_ADDITIONS_4
#endif

#define WEBPROCESSPOOL_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)
#define WEBPROCESSPOOL_RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), channel, "%p - WebProcessPool::" fmt, this, ##__VA_ARGS__)

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

static HashSet<String, ASCIICaseInsensitiveHash>& globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> set;
    return set;
}

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_defaultPageGroup(WebPageGroup::create())
    , m_injectedBundleClient(makeUnique<API::InjectedBundleClient>())
    , m_automationClient(makeUnique<API::AutomationClient>())
    , m_downloadClient(makeUniqueRef<API::DownloadClient>())
    , m_historyClient(makeUnique<API::LegacyContextHistoryClient>())
    , m_customProtocolManagerClient(makeUnique<API::CustomProtocolManagerClient>())
    , m_visitedLinkStore(VisitedLinkStore::create())
#if PLATFORM(MAC)
    , m_highPerformanceGraphicsUsageSampler(makeUnique<HighPerformanceGraphicsUsageSampler>(*this))
    , m_perActivityStateCPUUsageSampler(makeUnique<PerActivityStateCPUUsageSampler>(*this))
#endif
    , m_alwaysRunsAtBackgroundPriority(m_configuration->alwaysRunsAtBackgroundPriority())
    , m_shouldTakeUIBackgroundAssertion(m_configuration->shouldTakeUIBackgroundAssertion())
    WEB_PROCESS_POOL_ADDITIONS
    , m_userObservablePageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](RefCounterEvent) { updateProcessSuppressionState(); })
    , m_hiddenPageThrottlingAutoIncreasesCounter([this](RefCounterEvent) { m_hiddenPageThrottlingTimer.startOneShot(0_s); })
    , m_hiddenPageThrottlingTimer(RunLoop::main(), this, &WebProcessPool::updateHiddenPageThrottlingAutoIncreaseLimit)
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
        WEB_PROCESS_POOL_ADDITIONS_4
    });

    if (!m_websiteDataStore && WebKit::WebsiteDataStore::defaultDataStoreExists())
        m_websiteDataStore = WebKit::WebsiteDataStore::defaultDataStore();

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

    ASSERT(RunLoop::isMain());
    RunLoop::main().dispatch([weakPtr = makeWeakPtr(*this)] {
        if (!weakPtr)
            return;

        weakPtr->notifyThisWebProcessPoolWasCreated();
    });

    updateBackForwardCacheCapacity();
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

void WebProcessPool::setDownloadClient(UniqueRef<API::DownloadClient>&& downloadClient)
{
    m_downloadClient = WTFMove(downloadClient);
}

void WebProcessPool::setAutomationClient(std::unique_ptr<API::AutomationClient>&& automationClient)
{
    if (!automationClient)
        m_automationClient = makeUnique<API::AutomationClient>();
    else
        m_automationClient = WTFMove(automationClient);
}

void WebProcessPool::setLegacyCustomProtocolManagerClient(std::unique_ptr<API::CustomProtocolManagerClient>&& customProtocolManagerClient)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (!customProtocolManagerClient)
        m_customProtocolManagerClient = makeUnique<API::CustomProtocolManagerClient>();
    else
        m_customProtocolManagerClient = WTFMove(customProtocolManagerClient);
#endif
}

void WebProcessPool::setStorageAccessAPIEnabled(bool enabled)
{
    m_storageAccessAPIEnabled = enabled;
#if PLATFORM(COCOA)
    sendToNetworkingProcess(Messages::NetworkProcess::SetStorageAccessAPIEnabled(enabled));
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
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged());
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
#if PLATFORM(COCOA)
    auto screenProperties = WebCore::collectScreenProperties();
    sendToAllProcesses(Messages::WebProcess::SetScreenProperties(screenProperties));
#endif
}

NetworkProcessProxy& WebProcessPool::ensureNetworkProcess(WebsiteDataStore* withWebsiteDataStore)
{
    ASSERT(RunLoop::isMain());
    
    // FIXME: This is a temporary workaround for apps using WebKit API on non-main threads.
    // We should remove this once we enforce threading violation check on our APIs.
    // https://bugs.webkit.org/show_bug.cgi?id=200246.
    if (!RunLoop::isMain()) {
        callOnMainRunLoopAndWait([this, protectedThis = makeRef(*this)] {
            ensureNetworkProcess();
        });
        return *m_networkProcess;
    }

    if (m_networkProcess) {
        if (withWebsiteDataStore) {
            m_networkProcess->addSession(makeRef(*withWebsiteDataStore));
            withWebsiteDataStore->clearPendingCookies();
        }
        return *m_networkProcess;
    }

    auto networkProcess = makeUnique<NetworkProcessProxy>(*this);

    NetworkProcessCreationParameters parameters;

    if (m_websiteDataStore) {
        parameters.defaultDataStoreParameters.pendingCookies = copyToVector(m_websiteDataStore->pendingCookies());
        m_websiteDataStore->clearPendingCookies();
#if PLATFORM(COCOA)
        parameters.defaultDataStoreParameters.networkSessionParameters.sourceApplicationBundleIdentifier = m_websiteDataStore->configuration().sourceApplicationBundleIdentifier();
        parameters.defaultDataStoreParameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = m_websiteDataStore->configuration().sourceApplicationSecondaryIdentifier();
#endif
    }

    parameters.cacheModel = LegacyGlobalSettings::singleton().cacheModel();

    for (auto& scheme : globalURLSchemesWithCustomProtocolHandlers())
        parameters.urlSchemesRegisteredForCustomProtocols.append(scheme);

#if PLATFORM(IOS_FAMILY)
    String cookieStorageDirectory = WebProcessPool::cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = WebProcessPool::networkingCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String parentBundleDirectory = WebProcessPool::parentBundleDirectory();
    if (!parentBundleDirectory.isEmpty())
        SandboxExtension::createHandle(parentBundleDirectory, SandboxExtension::Type::ReadOnly, parameters.parentBundleDirectoryExtensionHandle);

#if ENABLE(INDEXED_DATABASE)
    SandboxExtension::createHandleForTemporaryFile(emptyString(), SandboxExtension::Type::ReadWrite, parameters.defaultDataStoreParameters.indexedDatabaseTempBlobDirectoryExtensionHandle);
#endif
#endif

    parameters.hstsStorageDirectory = m_configuration->hstsStorageDirectory();
    if (!parameters.hstsStorageDirectory.isNull())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.hstsStorageDirectory, parameters.hstsStorageDirectoryExtensionHandle);

    parameters.urlSchemesRegisteredAsSecure = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsSecure());
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsBypassingContentSecurityPolicy());
    parameters.urlSchemesRegisteredAsLocal = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsLocal());
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsNoAccess());

#if ENABLE(INDEXED_DATABASE)
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    if (m_websiteDataStore)
        parameters.defaultDataStoreParameters.indexedDatabaseDirectory = m_websiteDataStore->resolvedIndexedDatabaseDirectory();
    else if (WebKit::WebsiteDataStore::defaultDataStoreExists())
        parameters.defaultDataStoreParameters.indexedDatabaseDirectory = WebKit::WebsiteDataStore::defaultDataStore()->parameters().indexedDatabaseDirectory;
    
    if (!parameters.defaultDataStoreParameters.indexedDatabaseDirectory.isEmpty()) {
        SandboxExtension::createHandleForReadWriteDirectory(parameters.defaultDataStoreParameters.indexedDatabaseDirectory, parameters.defaultDataStoreParameters.indexedDatabaseDirectoryExtensionHandle);
        networkProcess->createSymLinkForFileUpgrade(parameters.defaultDataStoreParameters.indexedDatabaseDirectory);
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (m_websiteDataStore)
        parameters.serviceWorkerRegistrationDirectory = m_websiteDataStore->resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory)
        parameters.serviceWorkerRegistrationDirectory =  WebKit::WebsiteDataStore::defaultServiceWorkerRegistrationDirectory();
    SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
#endif

    auto localStorageDirectory = m_websiteDataStore ? m_websiteDataStore->resolvedLocalStorageDirectory() : nullString();
    if (!localStorageDirectory)
        localStorageDirectory = WebKit::WebsiteDataStore::defaultLocalStorageDirectory();
    parameters.defaultDataStoreParameters.localStorageDirectory = localStorageDirectory;
    SandboxExtension::createHandleForReadWriteDirectory(localStorageDirectory, parameters.defaultDataStoreParameters.localStorageDirectoryExtensionHandle);

    auto cacheStorageDirectory = m_websiteDataStore ? m_websiteDataStore->cacheStorageDirectory() : nullString();
    if (!cacheStorageDirectory.isEmpty()) {
        SandboxExtension::createHandleForReadWriteDirectory(cacheStorageDirectory, parameters.defaultDataStoreParameters.cacheStorageDirectoryExtensionHandle);
        parameters.defaultDataStoreParameters.cacheStorageDirectory = WTFMove(cacheStorageDirectory);
    }

    String resourceLoadStatisticsDirectory;
    SandboxExtension::Handle resourceLoadStatisticsDirectoryExtensionHandle;
    if (m_websiteDataStore)
        resourceLoadStatisticsDirectory = m_websiteDataStore->resolvedResourceLoadStatisticsDirectory();
    else if (WebKit::WebsiteDataStore::defaultDataStoreExists())
        resourceLoadStatisticsDirectory = WebKit::WebsiteDataStore::defaultDataStore()->parameters().networkSessionParameters.resourceLoadStatisticsParameters.directory;
    
    if (!resourceLoadStatisticsDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(resourceLoadStatisticsDirectory, resourceLoadStatisticsDirectoryExtensionHandle);

    bool enableResourceLoadStatistics = false;
    bool enableResourceLoadStatisticsLogTestingEvent = false;
    bool shouldIncludeLocalhost = true;
    bool enableResourceLoadStatisticsDebugMode = false;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    WebCore::SameSiteStrictEnforcementEnabled sameSiteStrictEnforcementEnabled = WebCore::SameSiteStrictEnforcementEnabled::No;
#endif
    WebCore::FirstPartyWebsiteDataRemovalMode firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> appBoundDomains;
    WebCore::RegistrableDomain manualPrevalentResource;
    WEB_PROCESS_POOL_ADDITIONS_2
    if (withWebsiteDataStore) {
        enableResourceLoadStatistics = withWebsiteDataStore->resourceLoadStatisticsEnabled();
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        enableResourceLoadStatisticsLogTestingEvent = withWebsiteDataStore->hasStatisticsTestingCallback();
#endif
        if (enableResourceLoadStatistics) {
            auto networkSessionParameters = withWebsiteDataStore->parameters().networkSessionParameters;
            shouldIncludeLocalhost = networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost;
            enableResourceLoadStatisticsDebugMode = networkSessionParameters.resourceLoadStatisticsParameters.enableDebugMode;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
            thirdPartyCookieBlockingMode = networkSessionParameters.resourceLoadStatisticsParameters.thirdPartyCookieBlockingMode;
            sameSiteStrictEnforcementEnabled = networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled;
#endif
            firstPartyWebsiteDataRemovalMode = networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode;
            standaloneApplicationDomain = networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain;
            appBoundDomains = networkSessionParameters.resourceLoadStatisticsParameters.appBoundDomains;
            manualPrevalentResource = networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource;
        }

        parameters.defaultDataStoreParameters.perOriginStorageQuota = withWebsiteDataStore->perOriginStorageQuota();
        parameters.defaultDataStoreParameters.perThirdPartyOriginStorageQuota = withWebsiteDataStore->perThirdPartyOriginStorageQuota();
        
        const auto& networkCacheDirectory = withWebsiteDataStore->resolvedNetworkCacheDirectory();
        if (!networkCacheDirectory.isNull()) {
            parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectory = networkCacheDirectory;
            SandboxExtension::createHandle(networkCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectoryExtensionHandle);
        }
    } else if (m_websiteDataStore) {
        enableResourceLoadStatistics = m_websiteDataStore->resourceLoadStatisticsEnabled();
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        enableResourceLoadStatisticsLogTestingEvent = m_websiteDataStore->hasStatisticsTestingCallback();
#endif
        if (enableResourceLoadStatistics) {
            auto networkSessionParameters = m_websiteDataStore->parameters().networkSessionParameters;
            shouldIncludeLocalhost = networkSessionParameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost;
            enableResourceLoadStatisticsDebugMode = networkSessionParameters.resourceLoadStatisticsParameters.enableDebugMode;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
            thirdPartyCookieBlockingMode = networkSessionParameters.resourceLoadStatisticsParameters.thirdPartyCookieBlockingMode;
            sameSiteStrictEnforcementEnabled = networkSessionParameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled;
#endif
            firstPartyWebsiteDataRemovalMode = networkSessionParameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode;
            standaloneApplicationDomain = networkSessionParameters.resourceLoadStatisticsParameters.standaloneApplicationDomain;
            appBoundDomains = networkSessionParameters.resourceLoadStatisticsParameters.appBoundDomains;
            manualPrevalentResource = networkSessionParameters.resourceLoadStatisticsParameters.manualPrevalentResource;
        }

        parameters.defaultDataStoreParameters.perOriginStorageQuota = m_websiteDataStore->perOriginStorageQuota();
        parameters.defaultDataStoreParameters.perThirdPartyOriginStorageQuota = m_websiteDataStore->perThirdPartyOriginStorageQuota();

        const auto& networkCacheDirectory = m_websiteDataStore->resolvedNetworkCacheDirectory();
        if (!networkCacheDirectory.isNull()) {
            parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectory = networkCacheDirectory;
            SandboxExtension::createHandle(networkCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectoryExtensionHandle);
        }
    } else {
        if (WebsiteDataStore::defaultDataStoreExists())
            enableResourceLoadStatistics = WebsiteDataStore::defaultDataStore()->resourceLoadStatisticsEnabled();
        auto networkCacheDirectory = WebsiteDataStore::defaultNetworkCacheDirectory();
        parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectory = networkCacheDirectory;
        SandboxExtension::createHandle(networkCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.defaultDataStoreParameters.networkSessionParameters.networkCacheDirectoryExtensionHandle);
    }

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    parameters.defaultDataStoreParameters.networkSessionParameters.alternativeServiceDirectory = WebsiteDataStore::defaultAlternativeServicesDirectory();
    if (!parameters.defaultDataStoreParameters.networkSessionParameters.alternativeServiceDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.defaultDataStoreParameters.networkSessionParameters.alternativeServiceDirectory, parameters.defaultDataStoreParameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle);
    parameters.defaultDataStoreParameters.networkSessionParameters.http3Enabled = WebsiteDataStore::http3Enabled();
#endif
    bool isItpStateExplicitlySet = false;
    parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsParameters = ResourceLoadStatisticsParameters {
        WTFMove(resourceLoadStatisticsDirectory),
        WTFMove(resourceLoadStatisticsDirectoryExtensionHandle),
        enableResourceLoadStatistics,
        isItpStateExplicitlySet,
        enableResourceLoadStatisticsLogTestingEvent,
        shouldIncludeLocalhost,
        enableResourceLoadStatisticsDebugMode,
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        thirdPartyCookieBlockingMode,
        sameSiteStrictEnforcementEnabled,
#endif
        firstPartyWebsiteDataRemovalMode,
        standaloneApplicationDomain,
        appBoundDomains,
        manualPrevalentResource,
    };

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

#if PLATFORM(COCOA)
    networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif

    networkProcess->updateProcessAssertion();

    if (withWebsiteDataStore) {
        networkProcess->addSession(makeRef(*withWebsiteDataStore));
        withWebsiteDataStore->clearPendingCookies();
    }

    // Make sure the network process knows about all the sessions that have been registered before it started.
    for (auto& sessionID : m_sessionToPageIDsMap.keys()) {
        if (auto* websiteDataStore = WebsiteDataStore::existingNonDefaultDataStoreForSessionID(sessionID))
            networkProcess->addSession(*websiteDataStore);
    }

    m_networkProcess = WTFMove(networkProcess);
    return *m_networkProcess;
}

void WebProcessPool::networkProcessCrashed(NetworkProcessProxy& networkProcessProxy)
{
    ASSERT(m_networkProcess);
    ASSERT(&networkProcessProxy == m_networkProcess.get());

    for (auto& supplement : m_supplements.values())
        supplement->processDidClose(&networkProcessProxy);

    m_client.networkProcessDidCrash(this);

    if (m_automationSession)
        m_automationSession->terminate();

    // Leave the process proxy around during client call, so that the client could query the process identifier.
    m_networkProcess = nullptr;

    terminateServiceWorkers();
}

void WebProcessPool::serviceWorkerProcessCrashed(WebProcessProxy& proxy)
{
#if ENABLE(SERVICE_WORKER)
    m_client.serviceWorkerProcessDidCrash(this, proxy.processIdentifier());
#endif
}

void WebProcessPool::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&& reply)
{
    ensureNetworkProcess();
    m_networkProcess->getNetworkProcessConnection(webProcessProxy, [this, weakThis = makeWeakPtr(*this), webProcessProxy = makeWeakPtr(webProcessProxy), reply = WTFMove(reply)] (auto& connectionInfo) mutable {
        if (UNLIKELY(!IPC::Connection::identifierIsValid(connectionInfo.identifier()) && webProcessProxy && weakThis)) {
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed first attempt, retrying");
            ensureNetworkProcess();
            m_networkProcess->getNetworkProcessConnection(*webProcessProxy, WTFMove(reply));
            return;
        }
        reply(connectionInfo);
    });
}

#if ENABLE(GPU_PROCESS)
void WebProcessPool::gpuProcessCrashed(ProcessID identifier)
{
    m_client.gpuProcessDidCrash(this, identifier);
    Vector<RefPtr<WebProcessProxy>> processes = m_processes;
    for (auto& process : processes)
        process->gpuProcessCrashed();
    terminateAllWebContentProcesses();
}

void WebProcessPool::getGPUProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetGPUProcessConnection::DelayedReply&& reply)
{
    GPUProcessProxy::singleton().getGPUProcessConnection(webProcessProxy, [this, weakThis = makeWeakPtr(*this), webProcessProxy = makeWeakPtr(webProcessProxy), reply = WTFMove(reply)] (auto& connectionInfo) mutable {
        if (UNLIKELY(!IPC::Connection::identifierIsValid(connectionInfo.identifier()) && webProcessProxy && weakThis)) {
            WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "getGPUProcessConnection: Failed first attempt, retrying");
            GPUProcessProxy::singleton().getGPUProcessConnection(*webProcessProxy, WTFMove(reply));
            return;
        }
        reply(connectionInfo);
    });
}
#endif

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::establishWorkerContextConnectionToNetworkProcess(NetworkProcessProxy& proxy, RegistrableDomain&& registrableDomain, PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(proxy, &proxy == m_networkProcess.get());

    auto* websiteDataStore = WebsiteDataStore::existingNonDefaultDataStoreForSessionID(sessionID);

    if (!websiteDataStore) {
        if (!m_websiteDataStore)
            m_websiteDataStore = WebKit::WebsiteDataStore::defaultDataStore().ptr();
        websiteDataStore = m_websiteDataStore.get();
    }

    WebProcessProxy* serviceWorkerProcessProxy { nullptr };
    if (!m_useSeparateServiceWorkerProcess) {
        for (auto& process : m_processes) {
            if (process == m_prewarmedProcess || process->isDummyProcessProxy())
                continue;
            if (&process->websiteDataStore() != websiteDataStore)
                continue;
            if (!process->isMatchingRegistrableDomain(registrableDomain))
                continue;

            serviceWorkerProcessProxy = process.get();
            serviceWorkerProcessProxy->enableServiceWorkers(userContentControllerIdentifierForServiceWorkers());

            WEBPROCESSPOOL_RELEASE_LOG_IF_ALLOWED(ServiceWorker, "establishWorkerContextConnectionToNetworkProcess reusing an existing web process (process=%p, PID=%d)", serviceWorkerProcessProxy, serviceWorkerProcessProxy->processIdentifier());
            break;
        }
    }

    if (!serviceWorkerProcessProxy) {
        auto newProcessProxy = WebProcessProxy::createForServiceWorkers(*this, WTFMove(registrableDomain), *websiteDataStore);
        serviceWorkerProcessProxy = newProcessProxy.ptr();

        WEBPROCESSPOOL_RELEASE_LOG_IF_ALLOWED(ServiceWorker, "establishWorkerContextConnectionToNetworkProcess creating a new service worker process (proces=%p, PID=%d)", serviceWorkerProcessProxy, serviceWorkerProcessProxy->processIdentifier());

        initializeNewWebProcess(newProcessProxy, websiteDataStore);
        m_processes.append(WTFMove(newProcessProxy));
    }

    ASSERT(!m_serviceWorkerProcesses.contains(*serviceWorkerProcessProxy));
    m_serviceWorkerProcesses.add(*serviceWorkerProcessProxy);

    serviceWorkerProcessProxy->establishServiceWorkerContext(m_serviceWorkerPreferences ? m_serviceWorkerPreferences.value() : m_defaultPageGroup->preferences().store(), WTFMove(completionHandler));
    if (!m_serviceWorkerUserAgent.isNull())
        serviceWorkerProcessProxy->setServiceWorkerUserAgent(m_serviceWorkerUserAgent);
}

void WebProcessPool::removeFromServiceWorkerProcesses(WebProcessProxy& process)
{
    ASSERT(m_serviceWorkerProcesses.contains(process));
    m_serviceWorkerProcesses.remove(process);
}
#endif

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

    s_invalidMessageCallback(toAPI(API::String::create(description(messageName)).ptr()));
}

void WebProcessPool::resolvePathsForSandboxExtensions()
{
    m_resolvedPaths.injectedBundlePath = resolvePathForSandboxExtension(injectedBundlePath());

    m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.reserveCapacity(m_configuration->additionalReadAccessAllowedPaths().size());
    for (const auto& path : m_configuration->additionalReadAccessAllowedPaths())
        m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.uncheckedAppend(resolvePathForSandboxExtension(path.data()));

    platformResolvePathsForSandboxExtensions();
}

WebProcessProxy& WebProcessPool::createNewWebProcess(WebsiteDataStore* websiteDataStore, WebProcessProxy::IsPrewarmed isPrewarmed)
{
    WEB_PROCESS_POOL_ADDITIONS_3
    auto processProxy = WebProcessProxy::create(*this, websiteDataStore, isPrewarmed);
    auto& process = processProxy.get();
    initializeNewWebProcess(process, websiteDataStore, isPrewarmed);
    m_processes.append(WTFMove(processProxy));

    return process;
}

RefPtr<WebProcessProxy> WebProcessPool::tryTakePrewarmedProcess(WebsiteDataStore& websiteDataStore)
{
    if (!m_prewarmedProcess)
        return nullptr;
    
    // There is sometimes a delay until we get notified that a prewarmed process has been terminated (e.g. after resuming
    // from suspension) so make sure the process is still running here before deciding to use it.
    if (m_prewarmedProcess->wasTerminated()) {
        WEBPROCESSPOOL_RELEASE_LOG_ERROR(Process, "tryTakePrewarmedProcess: Not using prewarmed process because it has been terminated (process=%p, PID=%d)", m_prewarmedProcess, m_prewarmedProcess->processIdentifier());
        m_prewarmedProcess = nullptr;
        return nullptr;
    }

#if PLATFORM(GTK) || PLATFORM(WPE)
    // In platforms using Bubblewrap for sandboxing, prewarmed process is launched using the WebProcessPool primary WebsiteDataStore,
    // so we don't use it in case of using a different WebsiteDataStore.
    if (m_sandboxEnabled && m_websiteDataStore && m_websiteDataStore.get() != &websiteDataStore)
        return nullptr;
#endif

    ASSERT(m_prewarmedProcess->isPrewarmed());
    m_prewarmedProcess->markIsNoLongerInPrewarmedPool();
    m_prewarmedProcess->setWebsiteDataStore(websiteDataStore);

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

#if !PLATFORM(MAC)
void WebProcessPool::registerHighDynamicRangeChangeCallback()
{
}
#endif

WebProcessDataStoreParameters WebProcessPool::webProcessDataStoreParameters(WebProcessProxy& process, WebsiteDataStore& websiteDataStore)
{
    websiteDataStore.resolveDirectoriesIfNecessary();

    String applicationCacheDirectory = websiteDataStore.resolvedApplicationCacheDirectory();
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    if (!applicationCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(applicationCacheDirectory, SandboxExtension::Type::ReadWrite, applicationCacheDirectoryExtensionHandle);

    String applicationCacheFlatFileSubdirectoryName = websiteDataStore.applicationCacheFlatFileSubdirectoryName();

    String webSQLDatabaseDirectory = websiteDataStore.resolvedDatabaseDirectory();
    SandboxExtension::Handle webSQLDatabaseDirectoryExtensionHandle;
    if (!webSQLDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(webSQLDatabaseDirectory, SandboxExtension::Type::ReadWrite, webSQLDatabaseDirectoryExtensionHandle);

    String mediaCacheDirectory = websiteDataStore.resolvedMediaCacheDirectory();
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!mediaCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(mediaCacheDirectory, SandboxExtension::Type::ReadWrite, mediaCacheDirectoryExtensionHandle);

    String mediaKeyStorageDirectory = websiteDataStore.resolvedMediaKeysDirectory();
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    if (!mediaKeyStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(mediaKeyStorageDirectory, SandboxExtension::Type::ReadWrite, mediaKeyStorageDirectoryExtensionHandle);

    String javaScriptConfigurationDirectory;
    if (!m_javaScriptConfigurationDirectory.isEmpty())
        javaScriptConfigurationDirectory = m_javaScriptConfigurationDirectory;
    else if (javaScriptConfigurationFileEnabled())
        javaScriptConfigurationDirectory = websiteDataStore.resolvedJavaScriptConfigurationDirectory();

    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
    if (!javaScriptConfigurationDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(javaScriptConfigurationDirectory, SandboxExtension::Type::ReadWrite, javaScriptConfigurationDirectoryExtensionHandle);
        
    auto plugInAutoStartOriginHashes = m_plugInAutoStartProvider.autoStartOriginHashesCopy(websiteDataStore.sessionID());

    return WebProcessDataStoreParameters {
        websiteDataStore.sessionID(),
        WTFMove(applicationCacheDirectory),
        WTFMove(applicationCacheDirectoryExtensionHandle),
        WTFMove(applicationCacheFlatFileSubdirectoryName),
        WTFMove(webSQLDatabaseDirectory),
        WTFMove(webSQLDatabaseDirectoryExtensionHandle),
        WTFMove(mediaCacheDirectory),
        WTFMove(mediaCacheDirectoryExtensionHandle),
        WTFMove(mediaKeyStorageDirectory),
        WTFMove(mediaKeyStorageDirectoryExtensionHandle),
        WTFMove(javaScriptConfigurationDirectory),
        WTFMove(javaScriptConfigurationDirectoryExtensionHandle),
        WTFMove(plugInAutoStartOriginHashes),
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        websiteDataStore.thirdPartyCookieBlockingMode(),
#endif
        websiteDataStore.resourceLoadStatisticsEnabled()
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

    ensureNetworkProcess();

    WebProcessCreationParameters parameters;

    parameters.injectedBundlePath = m_resolvedPaths.injectedBundlePath;
    if (!parameters.injectedBundlePath.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.injectedBundlePath, SandboxExtension::Type::ReadOnly, parameters.injectedBundlePathExtensionHandle);

    parameters.additionalSandboxExtensionHandles.allocate(m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.size());
    for (size_t i = 0, size = m_resolvedPaths.additionalWebProcessSandboxExtensionPaths.size(); i < size; ++i)
        SandboxExtension::createHandleWithoutResolvingPath(m_resolvedPaths.additionalWebProcessSandboxExtensionPaths[i], SandboxExtension::Type::ReadOnly, parameters.additionalSandboxExtensionHandles[i]);

#if PLATFORM(IOS_FAMILY)
    setJavaScriptConfigurationFileEnabledFromDefaults();
#endif

    parameters.cacheModel = LegacyGlobalSettings::singleton().cacheModel();
    parameters.overrideLanguages = configuration().overrideLanguages();

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
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;

    parameters.terminationTimeout = 0_s;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

    parameters.backForwardCacheCapacity = backForwardCache().capacity();

#if ENABLE(NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    parameters.notificationPermissions = supplement<WebNotificationManagerProxy>()->notificationPermissions();
#endif

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

    parameters.presentingApplicationPID = m_configuration->presentingApplicationPID();

    // Add any platform specific parameters
    platformInitializeWebProcess(process, parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient->getInjectedBundleInitializationUserData(*this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process.transformObjectsToHandles(injectedBundleInitializationUserData.get()));
    
    if (websiteDataStore)
        parameters.websiteDataStoreParameters = webProcessDataStoreParameters(process, *websiteDataStore);

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

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    process.send(Messages::WebProcess::BacklightLevelDidChange(displayBrightness()), 0);
#endif

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton();
#endif

#if PLATFORM(MAC)
    registerDisplayConfigurationCallback();
#endif

    registerHighDynamicRangeChangeCallback();
}

void WebProcessPool::prewarmProcess()
{
    if (m_prewarmedProcess)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(PerformanceLogging, "prewarmProcess: Prewarming a WebProcess for performance");
    createNewWebProcess(nullptr, WebProcessProxy::IsPrewarmed::Yes);
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
        m_websiteDataStore->didCreateNetworkProcess();
}

void WebProcessPool::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (m_prewarmedProcess == process) {
        ASSERT(m_prewarmedProcess->isPrewarmed());
        m_prewarmedProcess = nullptr;
    } else if (process->isDummyProcessProxy()) {
        auto removedProcess = m_dummyProcessProxies.take(process->sessionID());
        ASSERT_UNUSED(removedProcess, removedProcess == process);
    }

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls of the other supplements are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    RefPtr<WebProcessProxy> protect(process);

    m_backForwardCache->removeEntriesForProcess(*process);

#if ENABLE(SERVICE_WORKER)
    if (process->isRunningServiceWorkers())
        removeFromServiceWorkerProcesses(*process);
#endif

    static_cast<WebContextSupplement*>(supplement<WebGeolocationManagerProxy>())->processDidClose(process);

    m_processes.removeFirst(process);

#if ENABLE(GAMEPAD)
    if (m_processesUsingGamepads.contains(process))
        processStoppedUsingGamepads(*process);
#endif

    removeProcessFromOriginCacheSet(*process);
}

WebProcessProxy& WebProcessPool::processForRegistrableDomain(WebsiteDataStore& websiteDataStore, WebPageProxy* page, const RegistrableDomain& registrableDomain)
{
    if (!registrableDomain.isEmpty()) {
        if (auto process = webProcessCache().takeProcess(registrableDomain, websiteDataStore))
            return *process;

        // Check if we have a suspended page for the given registrable domain and use its process if we do, for performance reasons.
        if (auto process = SuspendedPageProxy::findReusableSuspendedPageProcess(*this, registrableDomain, websiteDataStore)) {
            WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForRegistrableDomain: Using WebProcess from a SuspendedPage (process=%p, PID=%i)", process.get(), process->processIdentifier());
            return *process;
        }
    }

    if (auto process = tryTakePrewarmedProcess(websiteDataStore)) {
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForRegistrableDomain: Using prewarmed process (process=%p, PID=%i)", process.get(), process->processIdentifier());
        if (!registrableDomain.isEmpty())
            tryPrewarmWithDomainInformation(*process, registrableDomain);
        return *process;
    }

    if (!usesSingleWebProcess())
        return createNewWebProcess(&websiteDataStore);

#if PLATFORM(COCOA)
    bool mustMatchDataStore = WebKit::WebsiteDataStore::defaultDataStoreExists() && &websiteDataStore != WebKit::WebsiteDataStore::defaultDataStore().ptr();
#else
    bool mustMatchDataStore = false;
#endif

    for (auto& process : m_processes) {
        if (process == m_prewarmedProcess || process->isDummyProcessProxy())
            continue;
#if ENABLE(SERVICE_WORKER)
        if (process->isRunningServiceWorkers())
            continue;
#endif
        if (mustMatchDataStore && &process->websiteDataStore() != &websiteDataStore)
            continue;
        return *process;
    }
    return createNewWebProcess(&websiteDataStore);
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
            m_websiteDataStore = WebKit::WebsiteDataStore::defaultDataStore().ptr();

        pageConfiguration->setWebsiteDataStore(m_websiteDataStore.get());
    }

    RefPtr<WebProcessProxy> process;
    auto* relatedPage = pageConfiguration->relatedPage();
    if (relatedPage && !relatedPage->isClosed()) {
        // Sharing processes, e.g. when creating the page via window.open().
        process = &pageConfiguration->relatedPage()->ensureRunningProcess();
        // We do not support several WebsiteDataStores sharing a single process.
        ASSERT(process->isDummyProcessProxy() || pageConfiguration->websiteDataStore() == &process->websiteDataStore());
        ASSERT(&pageConfiguration->relatedPage()->websiteDataStore() == pageConfiguration->websiteDataStore());
    } else if (!m_isDelayedWebProcessLaunchDisabled) {
        // In the common case, we delay process launch until something is actually loaded in the page.
        process = dummyProcessProxy(pageConfiguration->websiteDataStore()->sessionID());
        if (!process) {
            process = WebProcessProxy::create(*this, pageConfiguration->websiteDataStore(), WebProcessProxy::IsPrewarmed::No, WebProcessProxy::ShouldLaunchProcess::No);
            m_dummyProcessProxies.add(pageConfiguration->websiteDataStore()->sessionID(), makeWeakPtr(*process));
            m_processes.append(process.copyRef());
        }
    } else
        process = &processForRegistrableDomain(*pageConfiguration->websiteDataStore(), nullptr, { });

    RefPtr<WebUserContentControllerProxy> userContentController = pageConfiguration->userContentController();
    
    ASSERT(process);

    auto page = process->createWebPage(pageClient, WTFMove(pageConfiguration));

#if ENABLE(SERVICE_WORKER)
    if (!m_serviceWorkerPreferences) {
        m_serviceWorkerPreferences = page->preferencesStore();
        for (auto& serviceWorkerProcess : m_serviceWorkerProcesses)
            serviceWorkerProcess.updateServiceWorkerPreferencesStore(*m_serviceWorkerPreferences);
    }
    if (userContentController)
        m_userContentControllerIDForServiceWorker = userContentController->identifier();
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

    return page;
}

#if ENABLE(SERVICE_WORKER)
void WebProcessPool::updateServiceWorkerUserAgent(const String& userAgent)
{
    if (m_serviceWorkerUserAgent == userAgent)
        return;
    m_serviceWorkerUserAgent = userAgent;
    for (auto& serviceWorkerProcess : m_serviceWorkerProcesses)
        serviceWorkerProcess.setServiceWorkerUserAgent(m_serviceWorkerUserAgent);
}
#endif

void WebProcessPool::pageBeginUsingWebsiteDataStore(WebPageProxyIdentifier pageID, WebsiteDataStore& dataStore)
{
    auto result = m_sessionToPageIDsMap.add(dataStore.sessionID(), HashSet<WebPageProxyIdentifier>()).iterator->value.add(pageID);
    ASSERT_UNUSED(result, result.isNewEntry);

    auto sessionID = dataStore.sessionID();
    if (sessionID != PAL::SessionID::defaultSessionID()) {
        ASSERT(!sessionID.isEphemeral() || dataStore.parameters().networkSessionParameters.sessionID == sessionID);
        if (m_networkProcess) {
            m_networkProcess->addSession(makeRef(dataStore));
            dataStore.clearPendingCookies();
        }
    }
}

void WebProcessPool::pageEndUsingWebsiteDataStore(WebPageProxyIdentifier pageID, WebsiteDataStore& dataStore)
{
    auto sessionID = dataStore.sessionID();
    auto iterator = m_sessionToPageIDsMap.find(sessionID);
    ASSERT(iterator != m_sessionToPageIDsMap.end());

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
    return m_sessionToPageIDsMap.contains(dataStore.sessionID());
}

DownloadProxy& WebProcessPool::download(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const ResourceRequest& request, const String& suggestedFilename)
{
    auto& downloadProxy = createDownloadProxy(dataStore, request, initiatingPage, { });
    PAL::SessionID sessionID = dataStore.sessionID();

    Optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    if (initiatingPage) {
        initiatingPage->handleDownloadRequest(downloadProxy);
        isAppBound = initiatingPage->isNavigatingToAppBoundDomain();
    }

    if (networkProcess()) {
        ResourceRequest updatedRequest(request);
        // Request's firstPartyForCookies will be used as Original URL of the download request.
        // We set the value to top level document's URL.
        if (initiatingPage) {
            URL initiatingPageURL = URL { URL { }, initiatingPage->pageLoadState().url() };
            updatedRequest.setFirstPartyForCookies(initiatingPageURL);
            updatedRequest.setIsSameSite(areRegistrableDomainsEqual(initiatingPageURL, request.url()));
            if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
                updatedRequest.setHTTPUserAgent(initiatingPage->userAgent());
        } else {
            updatedRequest.setFirstPartyForCookies(URL());
            updatedRequest.setIsSameSite(false);
            if (!updatedRequest.hasHTTPHeaderField(HTTPHeaderName::UserAgent))
                updatedRequest.setHTTPUserAgent(WebPageProxy::standardUserAgent());
        }
        updatedRequest.setIsTopSite(false);
        networkProcess()->send(Messages::NetworkProcess::DownloadRequest(sessionID, downloadProxy.downloadID(), updatedRequest, isAppBound, suggestedFilename), 0);
        return downloadProxy;
    }

    return downloadProxy;
}

DownloadProxy& WebProcessPool::resumeDownload(WebsiteDataStore& dataStore, WebPageProxy* initiatingPage, const API::Data& resumeData, const String& path)
{
    auto& downloadProxy = createDownloadProxy(dataStore, ResourceRequest(), initiatingPage, { });
    PAL::SessionID sessionID = dataStore.sessionID();

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty())
        SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite, sandboxExtensionHandle);

    if (networkProcess()) {
        networkProcess()->send(Messages::NetworkProcess::ResumeDownload(sessionID, downloadProxy.downloadID(), resumeData.dataReference(), path, sandboxExtensionHandle), 0);
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
    return m_networkProcess ? m_networkProcess->processIdentifier() : 0;
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

void WebProcessPool::setServiceWorkerTimeoutForTesting(Seconds seconds)
{
    sendSyncToNetworkingProcess(Messages::NetworkProcess::SetServiceWorkerFetchTimeoutForTesting(seconds), Messages::NetworkProcess::SetServiceWorkerFetchTimeoutForTesting::Reply());
}

void WebProcessPool::resetServiceWorkerTimeoutForTesting()
{
    sendSyncToNetworkingProcess(Messages::NetworkProcess::ResetServiceWorkerFetchTimeoutForTesting(), Messages::NetworkProcess::ResetServiceWorkerFetchTimeoutForTesting::Reply());
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
    LegacyGlobalSettings::singleton().registerURLSchemeAsSecure(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsSecure(urlScheme));
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
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
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsLocal(urlScheme));
}

void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    LegacyGlobalSettings::singleton().registerURLSchemeAsNoAccess(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
    sendToNetworkingProcess(Messages::NetworkProcess::RegisterURLSchemeAsNoAccess(urlScheme));
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
    for (auto* processPool : allProcessPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->registerSchemeForLegacyCustomProtocol(urlScheme);
    }
}

void WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    globalURLSchemesWithCustomProtocolHandlers().remove(urlScheme);
    for (auto* processPool : allProcessPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->unregisterSchemeForLegacyCustomProtocol(urlScheme);
    }
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

    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::SetCacheModel(cacheModel), 0);
}

void WebProcessPool::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    updateBackForwardCacheCapacity();

    if (m_networkProcess)
        m_networkProcess->sendSync(Messages::NetworkProcess::SetCacheModelSynchronouslyForTesting(cacheModel), { }, { });
}

void WebProcessPool::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

DownloadProxy& WebProcessPool::createDownloadProxy(WebsiteDataStore& dataStore, const ResourceRequest& request, WebPageProxy* originatingPage, const FrameInfoData& frameInfo)
{
    return ensureNetworkProcess().createDownloadProxy(dataStore, request, frameInfo, originatingPage);
}

void WebProcessPool::synthesizeAppIsBackground(bool background)
{
    ensureNetworkProcess().synthesizeAppIsBackground(background);
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
    terminateServiceWorkers();

    if (!m_networkProcess)
        return;
    
    m_networkProcess->terminate();
    m_networkProcess = nullptr;
}

void WebProcessPool::terminateAllWebContentProcesses()
{
    Vector<RefPtr<WebProcessProxy>> processes = m_processes;
    for (auto& process : processes)
        process->terminate();
}

void WebProcessPool::sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&& completionHandler)
{
    if (!m_networkProcess)
        return completionHandler();

    m_networkProcess->sendPrepareToSuspend(IsSuspensionImminent::No, WTFMove(completionHandler));
}

void WebProcessPool::sendNetworkProcessWillSuspendImminentlyForTesting()
{
    if (m_networkProcess)
        m_networkProcess->sendProcessWillSuspendImminentlyForTesting();
}

void WebProcessPool::sendNetworkProcessDidResume()
{
    if (m_networkProcess)
        m_networkProcess->sendProcessDidResume();
}

void WebProcessPool::terminateServiceWorkers()
{
#if ENABLE(SERVICE_WORKER)
    auto protectedThis = makeRef(*this);
    while (m_serviceWorkerProcesses.computeSize())
        m_serviceWorkerProcesses.begin()->disableServiceWorkers();
#endif
}

void WebProcessPool::syncNetworkProcessCookies()
{
    ensureNetworkProcess().syncAllCookies();
}

void WebProcessPool::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    sendSyncToNetworkingProcess(Messages::NetworkProcess::SyncLocalStorage(), Messages::NetworkProcess::SyncLocalStorage::Reply());
    completionHandler();
}

void WebProcessPool::clearLegacyPrivateBrowsingLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::ClearLegacyPrivateBrowsingLocalStorage(), 0);
    completionHandler();
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

    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(*this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), [webProcessProxy = makeRef(*webProcessProxy), completionHandler = WTFMove(completionHandler)] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(webProcessProxy->transformObjectsToHandles(returnData.get())));
    });
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

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    auto& policiesForHost = m_pluginLoadClientPolicies.ensure(host, [] {
        return HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>();
    }).iterator->value;
    auto& versionsToPolicies = policiesForHost.ensure(bundleIdentifier, [] {
        return HashMap<String, WebCore::PluginLoadClientPolicy>();
    }).iterator->value;
    versionsToPolicies.set(versionString, policy);

    sendToAllProcesses(Messages::WebProcess::SetPluginLoadClientPolicy(policy, host, bundleIdentifier, versionString));
}

void WebProcessPool::resetPluginLoadClientPolicies(HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>>&& pluginLoadClientPolicies)
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

void WebProcessPool::setFontAllowList(API::Array* array)
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
    ensureNetworkProcess().updateProcessAssertion();
#if ENABLE(SERVICE_WORKER)
    // Check on next run loop since the web process proxy tokens are probably being updated.
    callOnMainRunLoop([this, weakThis = makeWeakPtr(this)] {
        if (!weakThis)
            return;
        for (auto& serviceWorkerProcess : m_serviceWorkerProcesses)
            serviceWorkerProcess.updateServiceWorkerProcessAssertion();
    });
#endif
}

bool WebProcessPool::isServiceWorkerPageID(WebPageProxyIdentifier pageID) const
{
#if ENABLE(SERVICE_WORKER)
    // FIXME: This is inefficient.
    return WTF::anyOf(m_serviceWorkerProcesses, [pageID](auto& process) {
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
    Vector<WebCore::RegistrableDomain> registrableDomainsToRemove;
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

        completionHandler(WTFMove(process), suspendedPage, reason);
    });
}

void WebProcessPool::processForNavigationInternal(WebPageProxy& page, const API::Navigation& navigation, Ref<WebProcessProxy>&& sourceProcess, const URL& pageSourceURL, ProcessSwapRequestedByClient processSwapRequestedByClient, Ref<WebsiteDataStore>&& dataStore, CompletionHandler<void(Ref<WebProcessProxy>&&, SuspendedPageProxy*, const String&)>&& completionHandler)
{
    auto& targetURL = navigation.currentRequest().url();
    auto targetRegistrableDomain = WebCore::RegistrableDomain { targetURL };

    auto createNewProcess = [this, protectedThis = makeRef(*this), page = makeRef(page), targetRegistrableDomain, dataStore = dataStore.copyRef()] () -> Ref<WebProcessProxy> {
        return processForRegistrableDomain(dataStore, page.ptr(), targetRegistrableDomain);
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

        if (RefPtr<WebProcessProxy> process = WebProcessProxy::processForIdentifier(targetItem->lastProcessIdentifier())) {
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

    if (navigation.treatAsSameOriginNavigation())
        return completionHandler(WTFMove(sourceProcess), nullptr, "The treatAsSameOriginNavigation flag is set"_s);

    URL sourceURL;
    if (page.isPageOpenedByDOMShowingInitialEmptyDocument() && !navigation.requesterOrigin().isEmpty())
        sourceURL = URL { URL(), navigation.requesterOrigin().toString() };
    else
        sourceURL = pageSourceURL;

    if (sourceURL.isEmpty() && page.configuration().relatedPage()) {
        sourceURL = URL { { }, page.configuration().relatedPage()->pageLoadState().url() };
        WEBPROCESSPOOL_RELEASE_LOG(ProcessSwapping, "processForNavigationInternal: Using related page's URL as source URL for process swap decision (page=%p)", page.configuration().relatedPage());
    }

    if (!sourceURL.isValid() || !targetURL.isValid() || sourceURL.isEmpty() || sourceURL.protocolIsAbout() || targetRegistrableDomain.matches(sourceURL))
        return completionHandler(WTFMove(sourceProcess), nullptr, "Navigation is same-site"_s);

    String reason = "Navigation is cross-site"_s;
    
    if (m_configuration->alwaysKeepAndReuseSwappedProcesses()) {
        LOG(ProcessSwapping, "(ProcessSwapping) Considering re-use of a previously cached process for domain %s", targetRegistrableDomain.string().utf8().data());

        if (auto* process = m_swappedProcessesPerRegistrableDomain.get(targetRegistrableDomain)) {
            if (&process->websiteDataStore() == dataStore.ptr()) {
                LOG(ProcessSwapping, "(ProcessSwapping) Reusing a previously cached process with pid %i to continue navigation to URL %s", process->processIdentifier(), targetURL.string().utf8().data());

                return completionHandler(makeRef(*process), nullptr, reason);
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebProcessPool::didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, OptionSet<CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer, WebPageProxyIdentifier webPageProxyID, PageIdentifier webPageID)
{
    if (!m_networkProcess)
        return;

    m_networkProcess->didCommitCrossSiteLoadWithDataTransfer(sessionID, fromDomain, toDomain, navigationDataTransfer, webPageProxyID, webPageID);
}

void WebProcessPool::seedResourceLoadStatisticsForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& process : processes())
        process->sendWithAsyncReply(Messages::WebProcess::SeedResourceLoadStatisticsForTesting(firstPartyDomain, thirdPartyDomain, shouldScheduleNotification), [callbackAggregator = callbackAggregator.copyRef()] { });
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
        m_audibleMediaActivity = WTF::nullopt;
        return;
    }

    if (m_audibleMediaActivity)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ProcessSuspension, "updateAudibleMediaAssertions: The number of processes playing audible media is now greater than zero. Taking UI process assertion.");
    m_audibleMediaActivity = AudibleMediaActivity {
        makeUniqueRef<ProcessAssertion>(getCurrentProcessID(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback)
#if ENABLE(GPU_PROCESS)
        , GPUProcessProxy::singletonIfCreated() ? makeUnique<ProcessAssertion>(GPUProcessProxy::singleton().processIdentifier(), "WebKit Media Playback"_s, ProcessAssertionType::MediaPlayback) : nullptr
#endif
    };
}

void WebProcessPool::setUseSeparateServiceWorkerProcess(bool useSeparateServiceWorkerProcess)
{
    if (m_useSeparateServiceWorkerProcess == useSeparateServiceWorkerProcess)
        return;

    WEBPROCESSPOOL_RELEASE_LOG(ServiceWorker, "setUseSeparateServiceWorkerProcess: (useSeparateServiceWorkerProcess=%d)", useSeparateServiceWorkerProcess);

    m_useSeparateServiceWorkerProcess = useSeparateServiceWorkerProcess;
    terminateServiceWorkers();
    terminateNetworkProcess();
}

#if ENABLE(SERVICE_WORKER)
bool WebProcessPool::hasServiceWorkerForegroundActivityForTesting() const
{
    return WTF::anyOf(m_serviceWorkerProcesses, [](auto& process) {
        return process.hasServiceWorkerForegroundActivityForTesting();
    });
}

bool WebProcessPool::hasServiceWorkerBackgroundActivityForTesting() const
{
    return WTF::anyOf(m_serviceWorkerProcesses, [](auto& process) {
        return process.hasServiceWorkerBackgroundActivityForTesting();
    });
}
#endif

} // namespace WebKit
