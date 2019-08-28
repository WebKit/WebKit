/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "NetworkProcess.h"

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "AuthenticationManager.h"
#include "AuxiliaryProcessMessages.h"
#include "DataReference.h"
#include "Download.h"
#include "DownloadProxyMessages.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkLoad.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessPlatformStrategies.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoadMap.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "PreconnectTask.h"
#include "RemoteNetworkingContext.h"
#include "ShouldGrandfatherStatistics.h"
#include "StatisticsData.h"
#include "StorageAccessStatus.h"
#include "StorageManagerSet.h"
#include "WebCookieManager.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSWOriginStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerToContextConnection.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteDataType.h"
#include <WebCore/CookieJar.h>
#include <WebCore/DNS.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStateNotifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/AtomString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#include "NetworkCache.h"
#include "NetworkCacheCoders.h"

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif

#if USE(SOUP)
#include "NetworkSessionSoup.h"
#include <WebCore/DNSResolveQueueSoup.h>
#include <WebCore/SoupNetworkSession.h>
#endif

#if USE(CURL)
#include <WebCore/CurlContext.h>
#endif

#if ENABLE(SERVICE_WORKER)
#include "WebSWServerToContextConnectionMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

static void callExitSoon(IPC::Connection*)
{
    // If the connection has been closed and we haven't responded in the main thread for 10 seconds
    // the process will exit forcibly.
    auto watchdogDelay = 10_s;

    WorkQueue::create("com.apple.WebKit.NetworkProcess.WatchDogQueue")->dispatchAfter(watchdogDelay, [] {
        // We use _exit here since the watchdog callback is called from another thread and we don't want
        // global destructors or atexit handlers to be called from this thread while the main thread is busy
        // doing its thing.
        RELEASE_LOG_ERROR(IPC, "Exiting process early due to unacknowledged closed-connection");
        _exit(EXIT_FAILURE);
    });
}

NetworkProcess::NetworkProcess(AuxiliaryProcessInitializationParameters&& parameters)
    : m_downloadManager(*this)
    , m_storageManagerSet(StorageManagerSet::create())
#if ENABLE(CONTENT_EXTENSIONS)
    , m_networkContentRuleListManager(*this)
#endif
#if PLATFORM(IOS_FAMILY)
    , m_webSQLiteDatabaseTracker([this](bool isHoldingLockedFiles) { parentProcessConnection()->send(Messages::NetworkProcessProxy::SetIsHoldingLockedFiles(isHoldingLockedFiles), 0); })
#endif
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    addSupplement<LegacyCustomProtocolManager>();
#endif
#if PLATFORM(COCOA) || USE(SOUP)
    LegacyCustomProtocolManager::networkProcessCreated(*this);
#endif

#if USE(SOUP)
    DNSResolveQueueSoup::setGlobalDefaultSoupSessionAccessor([this]() -> SoupSession* {
        return static_cast<NetworkSessionSoup&>(*networkSession(PAL::SessionID::defaultSessionID())).soupSession();
    });
#endif

    NetworkStateNotifier::singleton().addListener([weakThis = makeWeakPtr(*this)](bool isOnLine) {
        if (!weakThis)
            return;
        for (auto& webProcessConnection : weakThis->m_webProcessConnections)
            webProcessConnection->setOnLineState(isOnLine);
    });

    initialize(WTFMove(parameters));
}

NetworkProcess::~NetworkProcess()
{
    for (auto& callbacks : m_cacheStorageParametersCallbacks.values()) {
        for (auto& callback : callbacks)
            callback(String { });
    }
}

AuthenticationManager& NetworkProcess::authenticationManager()
{
    return *supplement<AuthenticationManager>();
}

DownloadManager& NetworkProcess::downloadManager()
{
    return m_downloadManager;
}

void NetworkProcess::removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess& connection)
{
    auto count = m_webProcessConnections.removeAllMatching([&] (const auto& c) {
        return c.ptr() == &connection;
    });
    ASSERT_UNUSED(count, count == 1);
}

bool NetworkProcess::shouldTerminate()
{
    // Network process keeps session cookies and credentials, so it should never terminate (as long as UI process connection is alive).
    return false;
}

void NetworkProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::AuxiliaryProcess::messageReceiverName()) {
        AuxiliaryProcess::didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (decoder.messageReceiverName() == Messages::NetworkContentRuleListManager::messageReceiverName()) {
        m_networkContentRuleListManager.didReceiveMessage(connection, decoder);
        return;
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        ASSERT(parentProcessHasServiceWorkerEntitlement());
        if (!parentProcessHasServiceWorkerEntitlement())
            return;
        if (auto* webSWConnection = connectionToContextProcessFromIPCConnection(connection)) {
            webSWConnection->didReceiveMessage(connection, decoder);
            return;
        }
    }
#endif
    didReceiveNetworkProcessMessage(connection, decoder);
}

void NetworkProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    didReceiveSyncNetworkProcessMessage(connection, decoder, replyEncoder);
}

void NetworkProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());

    // Make sure we flush all cookies to disk before exiting.
    platformSyncAllCookies([this] {
        stopRunLoop();
    });
}

void NetworkProcess::didCreateDownload()
{
    disableTermination();
}

void NetworkProcess::didDestroyDownload()
{
    enableTermination();
}

IPC::Connection* NetworkProcess::downloadProxyConnection()
{
    return parentProcessConnection();
}

AuthenticationManager& NetworkProcess::downloadsAuthenticationManager()
{
    return authenticationManager();
}

void NetworkProcess::lowMemoryHandler(Critical critical)
{
    if (m_suppressMemoryPressureHandler)
        return;

    WTF::releaseFastMallocFreeMemory();

    forEachNetworkSession([](auto& networkSession) {
        networkSession.clearPrefetchCache();
    });
}

void NetworkProcess::initializeNetworkProcess(NetworkProcessCreationParameters&& parameters)
{
#if HAVE(SEC_KEY_PROXY)
    WTF::setProcessPrivileges({ ProcessPrivilege::CanAccessRawCookies });
#else
    WTF::setProcessPrivileges({ ProcessPrivilege::CanAccessRawCookies, ProcessPrivilege::CanAccessCredentials });
#endif
    WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
    platformInitializeNetworkProcess(parameters);

    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomString::init();

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([this] (Critical critical, Synchronous) {
            lowMemoryHandler(critical);
        });
        memoryPressureHandler.install();
    }

    m_diskCacheIsDisabledForTesting = parameters.shouldUseTestingNetworkSession;

    setCacheModel(parameters.cacheModel);

    setCanHandleHTTPSServerTrustEvaluation(parameters.canHandleHTTPSServerTrustEvaluation);

    if (parameters.shouldUseTestingNetworkSession)
        switchToNewTestingSession();

    WebCore::RuntimeEnabledFeatures::sharedFeatures().setIsITPDatabaseEnabled(parameters.shouldEnableITPDatabase);

    WebCore::RuntimeEnabledFeatures::sharedFeatures().setAdClickAttributionDebugModeEnabled(parameters.enableAdClickAttributionDebugMode);

    SandboxExtension::consumePermanently(parameters.defaultDataStoreParameters.networkSessionParameters.resourceLoadStatisticsDirectoryExtensionHandle);

    auto sessionID = parameters.defaultDataStoreParameters.networkSessionParameters.sessionID;
    setSession(sessionID, NetworkSession::create(*this, WTFMove(parameters.defaultDataStoreParameters.networkSessionParameters)));

#if ENABLE(INDEXED_DATABASE)
    addIndexedDatabaseSession(sessionID, parameters.defaultDataStoreParameters.indexedDatabaseDirectory, parameters.defaultDataStoreParameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    if (parentProcessHasServiceWorkerEntitlement()) {
        addServiceWorkerSession(PAL::SessionID::defaultSessionID(), parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);

        for (auto& scheme : parameters.urlSchemesServiceWorkersCanHandle)
            registerURLSchemeServiceWorkersCanHandle(scheme);

        m_shouldDisableServiceWorkerProcessTerminationDelay = parameters.shouldDisableServiceWorkerProcessTerminationDelay;
    }
#endif
    initializeStorageQuota(parameters.defaultDataStoreParameters);

    m_storageManagerSet->add(sessionID, parameters.defaultDataStoreParameters.localStorageDirectory, parameters.defaultDataStoreParameters.localStorageDirectoryExtensionHandle);

    auto* defaultSession = networkSession(PAL::SessionID::defaultSessionID());
    auto* defaultStorageSession = defaultSession->networkStorageSession();
    for (const auto& cookie : parameters.defaultDataStoreParameters.pendingCookies)
        defaultStorageSession->setCookie(cookie);

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);

    for (auto& scheme : parameters.urlSchemesRegisteredAsSecure)
        registerURLSchemeAsSecure(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy)
        registerURLSchemeAsBypassingContentSecurityPolicy(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsLocal)
        registerURLSchemeAsLocal(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsNoAccess)
        registerURLSchemeAsNoAccess(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsDisplayIsolated)
        registerURLSchemeAsDisplayIsolated(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsCORSEnabled)
        registerURLSchemeAsCORSEnabled(scheme);

    for (auto& scheme : parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest)
        registerURLSchemeAsCanDisplayOnlyIfCanRequest(scheme);
    
    m_downloadMonitorSpeedMultiplier = parameters.downloadMonitorSpeedMultiplier;

    RELEASE_LOG(Process, "%p - NetworkProcess::initializeNetworkProcess: Presenting process = %d", this, WebCore::presentingApplicationPID());
}

void NetworkProcess::initializeConnection(IPC::Connection* connection)
{
    AuxiliaryProcess::initializeConnection(connection);

    // We give a chance for didClose() to get called on the main thread but forcefully call _exit() after a delay
    // in case the main thread is unresponsive or didClose() takes too long.
    connection->setDidCloseOnConnectionWorkQueueCallback(callExitSoon);

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);
}

void NetworkProcess::createNetworkConnectionToWebProcess(bool isServiceWorkerProcess, WebCore::RegistrableDomain&& registrableDomain)
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();

    auto connection = NetworkConnectionToWebProcess::create(*this, socketPair.server);
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientSocket(socketPair.client);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientSocket), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(Process, "NetworkProcess::createNetworkConnectionToWebProcess: Could not allocate mach port, error %x", kr);
        CRASH();
    }
    if (!MACH_PORT_VALID(listeningPort)) {
        RELEASE_LOG_ERROR(Process, "NetworkProcess::createNetworkConnectionToWebProcess: Could not allocate mach port, returned port was invalid");
        CRASH();
    }

    // Create a listening connection.
    auto connection = NetworkConnectionToWebProcess::create(*this, IPC::Connection::Identifier(listeningPort));
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientPort), 0);
#elif OS(WINDOWS)
    IPC::Connection::Identifier serverIdentifier, clientIdentifier;
    if (!IPC::Connection::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        LOG_ERROR("Failed to create server and client identifiers");
        CRASH();
    }

    auto connection = NetworkConnectionToWebProcess::create(*this, serverIdentifier);
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientSocket(clientIdentifier);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientSocket), 0);
#else
    notImplemented();
#endif

    if (!m_webProcessConnections.isEmpty())
        m_webProcessConnections.last()->setOnLineState(NetworkStateNotifier::singleton().onLine());
    
#if ENABLE(SERVICE_WORKER)
    if (isServiceWorkerProcess && !m_webProcessConnections.isEmpty()) {
        ASSERT(parentProcessHasServiceWorkerEntitlement());
        ASSERT(m_waitingForServerToContextProcessConnection);
        auto contextConnection = WebSWServerToContextConnection::create(*this, registrableDomain, m_webProcessConnections.last()->connection());
        auto addResult = m_serverToContextConnections.add(WTFMove(registrableDomain), contextConnection.copyRef());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);

        m_waitingForServerToContextProcessConnection = false;

        for (auto* server : SWServer::allServers())
            server->serverToContextConnectionCreated(contextConnection);
    }
#else
    UNUSED_PARAM(isServiceWorkerProcess);
    UNUSED_PARAM(registrableDomain);
#endif

    m_storageManagerSet->addConnection(m_webProcessConnections.last()->connection());
}

void NetworkProcess::clearCachedCredentials()
{
    defaultStorageSession().credentialStorage().clearCredentials();
    if (auto* networkSession = this->networkSession(PAL::SessionID::defaultSessionID()))
        networkSession->clearCredentials();
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::addWebsiteDataStore(WebsiteDataStoreParameters&& parameters)
{
#if ENABLE(INDEXED_DATABASE)
    addIndexedDatabaseSession(parameters.networkSessionParameters.sessionID, parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    if (parentProcessHasServiceWorkerEntitlement())
        addServiceWorkerSession(parameters.networkSessionParameters.sessionID, parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
#endif

    m_storageManagerSet->add(parameters.networkSessionParameters.sessionID, parameters.localStorageDirectory, parameters.localStorageDirectoryExtensionHandle);

    initializeStorageQuota(parameters);

    RemoteNetworkingContext::ensureWebsiteDataStoreSession(*this, WTFMove(parameters));
}

void NetworkProcess::initializeStorageQuota(const WebsiteDataStoreParameters& parameters)
{
    auto& managers =  m_storageQuotaManagers.ensure(parameters.networkSessionParameters.sessionID, [] {
        return StorageQuotaManagers { };
    }).iterator->value;
    managers.setDefaultQuotas(parameters.perOriginStorageQuota, parameters.perThirdPartyOriginStorageQuota);
}

void NetworkProcess::forEachNetworkSession(const Function<void(NetworkSession&)>& functor)
{
    for (auto& session : m_networkSessions.values())
        functor(*session);
}

void NetworkProcess::switchToNewTestingSession()
{
#if PLATFORM(COCOA)
    // Session name should be short enough for shared memory region name to be under the limit, otherwise sandbox rules won't work (see <rdar://problem/13642852>).
    String sessionName = makeString("WebKit Test-", getCurrentProcessID());

    auto session = adoptCF(WebCore::createPrivateStorageSession(sessionName.createCFString().get()));

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (WebCore::NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (session)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, session.get()));
    }

    m_defaultNetworkStorageSession = makeUnique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID(), WTFMove(session), WTFMove(cookieStorage));
#elif USE(CURL) || USE(SOUP)
    m_defaultNetworkStorageSession = makeUnique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID());
#endif
}

#if PLATFORM(COCOA)
void NetworkProcess::ensureSession(const PAL::SessionID& sessionID, const String& identifierBase, RetainPtr<CFHTTPCookieStorageRef>&& cookieStorage)
#else
void NetworkProcess::ensureSession(const PAL::SessionID& sessionID, const String& identifierBase)
#endif
{
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());

    auto addResult = m_networkStorageSessions.add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

#if PLATFORM(COCOA)
    RetainPtr<CFURLStorageSessionRef> storageSession;
    RetainPtr<CFStringRef> cfIdentifier = String(identifierBase + ".PrivateBrowsing").createCFString();
    if (sessionID.isEphemeral())
        storageSession = adoptCF(createPrivateStorageSession(cfIdentifier.get()));
    else
        storageSession = WebCore::NetworkStorageSession::createCFStorageSessionForIdentifier(cfIdentifier.get());

    if (NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (!cookieStorage && storageSession)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    }

    addResult.iterator->value = makeUnique<NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(cookieStorage));
#elif USE(CURL) || USE(SOUP)
    addResult.iterator->value = makeUnique<NetworkStorageSession>(sessionID);
#endif
}

WebCore::NetworkStorageSession* NetworkProcess::storageSession(const PAL::SessionID& sessionID) const
{
    if (sessionID == PAL::SessionID::defaultSessionID())
        return &defaultStorageSession();
    return m_networkStorageSessions.get(sessionID);
}

WebCore::NetworkStorageSession& NetworkProcess::defaultStorageSession() const
{
    if (!m_defaultNetworkStorageSession)
        m_defaultNetworkStorageSession = platformCreateDefaultStorageSession();
    return *m_defaultNetworkStorageSession;
}

void NetworkProcess::forEachNetworkStorageSession(const Function<void(WebCore::NetworkStorageSession&)>& functor)
{
    functor(defaultStorageSession());
    for (auto& storageSession : m_networkStorageSessions.values())
        functor(*storageSession);
}

NetworkSession* NetworkProcess::networkSession(PAL::SessionID sessionID) const
{
    ASSERT(RunLoop::isMain());
    return m_networkSessions.get(sessionID);
}

NetworkSession* NetworkProcess::networkSessionByConnection(IPC::Connection& connection) const
{
    auto iterator = m_sessionByConnection.find(connection.uniqueID());
    return iterator != m_sessionByConnection.end() ? networkSession(iterator->value) : nullptr;
}

void NetworkProcess::setSession(PAL::SessionID sessionID, std::unique_ptr<NetworkSession>&& session)
{
    ASSERT(RunLoop::isMain());
    m_networkSessions.set(sessionID, WTFMove(session));
}

void NetworkProcess::destroySession(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());
#if !USE(SOUP)
    // Soup based ports destroy the default session right before the process exits to avoid leaking
    // network resources like the cookies database.
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());
#endif

    if (auto session = m_networkSessions.take(sessionID))
        session->invalidateAndCancel();
    m_networkStorageSessions.remove(sessionID);
    m_sessionsControlledByAutomation.remove(sessionID);
    CacheStorage::Engine::destroyEngine(*this, sessionID);

#if ENABLE(SERVICE_WORKER)
    m_swServers.remove(sessionID);
    m_swDatabasePaths.remove(sessionID);
#endif

    m_storageManagerSet->remove(sessionID);

    m_storageQuotaManagers.remove(sessionID);
}

BlobRegistryImpl* NetworkProcess::blobRegistry(NetworkConnectionToWebProcess& connection)
{
    // FIXME: Deprecate this method and use sessionID -> NetworkSession -> blob registry.
    auto* session = networkSessionByConnection(connection.connection());
    return session ? &session->blobRegistry() : nullptr;
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkProcess::dumpResourceLoadStatistics(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->dumpResourceLoadStatistics(WTFMove(completionHandler));
        else
            completionHandler({ });
    } else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
    }
}

void NetworkProcess::updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID sessionID, const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->setPrevalentDomainsToBlockAndDeleteCookiesFor(domainsToBlock);
    completionHandler();
}

void NetworkProcess::isGrandfathered(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isGrandfathered(domain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isPrevalentResource(domain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isVeryPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isVeryPrevalentResource(domain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setAgeCapForClientSideCookies(PAL::SessionID sessionID, Optional<Seconds> seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->setAgeCapForClientSideCookies(seconds);
    completionHandler();
}

void NetworkProcess::setGrandfathered(PAL::SessionID sessionID, const RegistrableDomain& domain, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setGrandfathered(domain, isGrandfathered, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setPrevalentResource(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setPrevalentResourceForDebugMode(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setPrevalentResourceForDebugMode(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setVeryPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setVeryPrevalentResource(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::clearPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->clearPrevalentResource(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::submitTelemetry(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->submitTelemetry(WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::scheduleCookieBlockingUpdate(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->scheduleCookieBlockingUpdate(WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, Optional<WallTime> modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        networkSession->clearIsolatedSessions();
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            if (modifiedSince)
                resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince.value(), shouldGrandfather, WTFMove(completionHandler));
            else
                resourceLoadStatistics->scheduleClearInMemoryAndPersistent(shouldGrandfather, WTFMove(completionHandler));
        } else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::resetParametersToDefaultValues(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->resetParametersToDefaultValues(WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::scheduleStatisticsAndDataRecordsProcessing(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->scheduleStatisticsAndDataRecordsProcessing(WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setNotifyPagesWhenDataRecordsWereScanned(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setIsRunningResourceLoadStatisticsTest(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setIsRunningTest(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setNotifyPagesWhenTelemetryWasCaptured(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setNotifyPagesWhenTelemetryWasCaptured(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubframeUnderTopFrameDomain(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setSubframeUnderTopFrameDomain(subFrameDomain, topFrameDomain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::isRegisteredAsRedirectingTo(PAL::SessionID sessionID, const RegistrableDomain& domainRedirectedFrom, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsRedirectingTo(domainRedirectedFrom, domainRedirectedTo, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isRegisteredAsSubFrameUnder(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsSubFrameUnder(subFrameDomain, topFrameDomain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setSubresourceUnderTopFrameDomain(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUnderTopFrameDomain(subresourceDomain, topFrameDomain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubresourceUniqueRedirectTo(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUniqueRedirectTo(subresourceDomain, domainRedirectedTo, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubresourceUniqueRedirectFrom(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUniqueRedirectFrom(subresourceDomain, domainRedirectedFrom, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::isRegisteredAsSubresourceUnder(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsSubresourceUnder(subresourceDomain, topFrameDomain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setTopFrameUniqueRedirectTo(PAL::SessionID sessionID, const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setTopFrameUniqueRedirectTo(topFrameDomain, domainRedirectedTo, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setTopFrameUniqueRedirectFrom(PAL::SessionID sessionID, const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setTopFrameUniqueRedirectFrom(topFrameDomain, domainRedirectedFrom, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}
    
    
void NetworkProcess::setLastSeen(PAL::SessionID sessionID, const RegistrableDomain& domain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setLastSeen(domain, seconds, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::getAllStorageAccessEntries(PAL::SessionID sessionID, CompletionHandler<void(Vector<String> domains)>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        completionHandler(networkStorageSession->getAllStorageAccessEntries());
    else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
    }
}

void NetworkProcess::logFrameNavigation(PAL::SessionID sessionID, const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame);
    } else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::logUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logUserInteraction(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::hadUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->hasHadUserInteraction(domain, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::clearUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->clearUserInteraction(domain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::hasLocalStorage(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_storageManagerSet->contains(sessionID)) {
        m_storageManagerSet->getLocalStorageOrigins(sessionID, [domain, completionHandler = WTFMove(completionHandler)](auto&& origins) mutable {
            completionHandler(WTF::anyOf(origins, [&domain](auto& origin) {
                return domain.matches(origin);
            }));
        });
        return;
    }
    completionHandler(false);
}

void NetworkProcess::removePrevalentDomains(PAL::SessionID sessionID, const Vector<RegistrableDomain>& domains)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->removePrevalentDomains(domains);
}

void NetworkProcess::setCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->setCacheMaxAgeCapForPrevalentResources(Seconds { seconds });
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::setGrandfatheringTime(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setGrandfatheringTime(seconds, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setMaxStatisticsEntries(PAL::SessionID sessionID, uint64_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setMaxStatisticsEntries(maximumEntryCount, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setMinimumTimeBetweenDataRecordsRemoval(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setMinimumTimeBetweenDataRecordsRemoval(seconds, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setPruneEntriesDownTo(PAL::SessionID sessionID, uint64_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setPruneEntriesDownTo(pruneTargetCount, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setTimeToLiveUserInteraction(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setTimeToLiveUserInteraction(seconds, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setShouldClassifyResourcesBeforeDataRecordsRemoval(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setShouldClassifyResourcesBeforeDataRecordsRemoval(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setResourceLoadStatisticsEnabled(bool enabled)
{
    forEachNetworkSession([enabled](auto& networkSession) {
        networkSession.setResourceLoadStatisticsEnabled(enabled);
    });
}

void NetworkProcess::setResourceLoadStatisticsLogTestingEvent(bool enabled)
{
    forEachNetworkSession([enabled](auto& networkSession) {
        networkSession.setResourceLoadStatisticsLogTestingEvent(enabled);
    });
}

void NetworkProcess::setResourceLoadStatisticsDebugMode(PAL::SessionID sessionID, bool debugMode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setResourceLoadStatisticsDebugMode(debugMode, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->resetCacheMaxAgeCapForPrevalentResources();
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer, WebCore::PageIdentifier pageID)
{
    ASSERT(!navigationDataTransfer.isEmpty());

    if (auto* networkStorageSession = storageSession(sessionID)) {
        if (!networkStorageSession->shouldBlockThirdPartyCookies(fromDomain))
            return;

        if (navigationDataTransfer.contains(CrossSiteNavigationDataTransfer::Flag::DestinationLinkDecoration))
            networkStorageSession->didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(toDomain, pageID);

        if (navigationDataTransfer.contains(CrossSiteNavigationDataTransfer::Flag::ReferrerLinkDecoration))
            parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCommitCrossSiteLoadWithDataTransferFromPrevalentResource(pageID), 0);
    } else
        ASSERT_NOT_REACHED();

    if (navigationDataTransfer.contains(CrossSiteNavigationDataTransfer::Flag::DestinationLinkDecoration)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
                resourceLoadStatistics->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain, [] { });
        } else
            ASSERT_NOT_REACHED();
    }
}

void NetworkProcess::setCrossSiteLoadWithLinkDecorationForTesting(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::resetCrossSiteLoadsWithLinkDecorationForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->resetCrossSiteLoadsWithLinkDecorationForTesting();
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::hasIsolatedSession(PAL::SessionID sessionID, const WebCore::RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    bool result = false;
    if (auto* networkSession = this->networkSession(sessionID))
        result = networkSession->hasIsolatedSession(domain);
    completionHandler(result);
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

bool NetworkProcess::sessionIsControlledByAutomation(PAL::SessionID sessionID) const
{
    return m_sessionsControlledByAutomation.contains(sessionID);
}

void NetworkProcess::setSessionIsControlledByAutomation(PAL::SessionID sessionID, bool controlled)
{
    if (controlled)
        m_sessionsControlledByAutomation.add(sessionID);
    else
        m_sessionsControlledByAutomation.remove(sessionID);
}

static void fetchDiskCacheEntries(NetworkCache::Cache* cache, PAL::SessionID sessionID, OptionSet<WebsiteDataFetchOption> fetchOptions, CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    if (!cache) {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler({ });
        });
        return;
    }
    
    HashMap<SecurityOriginData, uint64_t> originsAndSizes;
    cache->traverse([fetchOptions, completionHandler = WTFMove(completionHandler), originsAndSizes = WTFMove(originsAndSizes)](auto* traversalEntry) mutable {
        if (!traversalEntry) {
            Vector<WebsiteData::Entry> entries;

            for (auto& originAndSize : originsAndSizes)
                entries.append(WebsiteData::Entry { originAndSize.key, WebsiteDataType::DiskCache, originAndSize.value });

            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), entries = WTFMove(entries)] () mutable {
                completionHandler(entries);
            });

            return;
        }

        auto url = traversalEntry->entry.response().url();
        auto result = originsAndSizes.add({url.protocol().toString(), url.host().toString(), url.port()}, 0);

        if (fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes))
            result.iterator->value += traversalEntry->entry.sourceStorageRecord().header.size() + traversalEntry->recordInfo.bodySize;
    });
}

void NetworkProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, uint64_t callbackID)
{
    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(Function<void (WebsiteData)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), websiteData = WTFMove(m_websiteData)] () mutable {
                completionHandler(websiteData);
            });
        }

        CompletionHandler<void(WebsiteData)> m_completionHandler;
        WebsiteData m_websiteData;
    };

    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, callbackID] (WebsiteData websiteData) {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    }));

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->getHostnamesWithCookies(callbackAggregator->m_websiteData.hostNamesWithCookies);
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (storageSession(sessionID)) {
            auto securityOrigins = storageSession(sessionID)->credentialStorage().originsWithCredentials();
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::Credentials, 0 });
        }
        auto securityOrigins = WebCore::CredentialStorage::originsWithSessionCredentials();
        for (auto& securityOrigin : securityOrigins)
            callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::Credentials, 0 });
    }

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID)) {
        m_storageManagerSet->getSessionStorageOrigins(sessionID, [callbackAggregator = callbackAggregator.copyRef()](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::SessionStorage, 0 });
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID)) {
        m_storageManagerSet->getLocalStorageOrigins(sessionID, [callbackAggregator = callbackAggregator.copyRef()](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::LocalStorage, 0 });
        });
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            getHostNamesWithHSTSCache(*networkStorageSession, callbackAggregator->m_websiteData.hostNamesWithHSTSCache);
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, callbackAggregator = callbackAggregator.copyRef(), path = crossThreadCopy(path)]() mutable {
            RunLoop::main().dispatch([callbackAggregator = WTFMove(callbackAggregator), securityOrigins = indexedDatabaseOrigins(path)] {
                for (const auto& securityOrigin : securityOrigins)
                    callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });
            });
        }));
    }
#endif

#if ENABLE(SERVICE_WORKER)
    path = m_swDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([callbackAggregator = callbackAggregator.copyRef()](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& origin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
#endif
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
                callbackAggregator->m_websiteData.entries.appendVector(entries);
            });
        });
    }
}

void NetworkProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, uint64_t callbackID)
{
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            clearHSTSCache(*networkStorageSession, modifiedSince);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteAllCookiesModifiedSince(modifiedSince);
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID))
            session->credentialStorage().clearCredentials();
        WebCore::CredentialStorage::clearSessionCredentials();
    }

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        CacheStorage::Engine::clearAllCaches(*this, sessionID, [clearTasksHandler = clearTasksHandler.copyRef()] { });

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteSessionStorage(sessionID, [clearTasksHandler = clearTasksHandler.copyRef()] { });

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteLocalStorageModifiedSince(sessionID, modifiedSince, [clearTasksHandler = clearTasksHandler.copyRef()] { });

#if ENABLE(INDEXED_DATABASE)
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !sessionID.isEphemeral())
        idbServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, [clearTasksHandler = clearTasksHandler.copyRef()] { });
#endif

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && !sessionID.isEphemeral())
        swServerForSession(sessionID).clearAll([clearTasksHandler = clearTasksHandler.copyRef()] { });
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
                auto deletedTypesRaw = websiteDataTypes.toRaw();
                auto monitoredTypesRaw = WebResourceLoadStatisticsStore::monitoredDataTypes().toRaw();
                
                // If we are deleting all of the data types that the resource load statistics store monitors
                // we do not need to re-grandfather old data.
                auto shouldGrandfather = ((monitoredTypesRaw & deletedTypesRaw) == monitoredTypesRaw) ? ShouldGrandfatherStatistics::No : ShouldGrandfatherStatistics::Yes;
                
                resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince, shouldGrandfather, [clearTasksHandler = clearTasksHandler.copyRef()] { });
            }
        }
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral())
        clearDiskCache(modifiedSince, [clearTasksHandler = WTFMove(clearTasksHandler)] { });

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) || websiteDataTypes.contains(WebsiteDataType::DOMCache))
        clearStorageQuota(sessionID);

    if (websiteDataTypes.contains(WebsiteDataType::AdClickAttributions)) {
        if (auto* networkSession = this->networkSession(sessionID))
            networkSession->clearAdClickAttribution();
    }
}

static void clearDiskCacheEntries(NetworkCache::Cache* cache, const Vector<SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    if (!cache) {
        RunLoop::main().dispatch(WTFMove(completionHandler));
        return;
    }

    HashSet<RefPtr<SecurityOrigin>> originsToDelete;
    for (auto& origin : origins)
        originsToDelete.add(origin.securityOrigin());

    Vector<NetworkCache::Key> cacheKeysToDelete;
    cache->traverse([cache, completionHandler = WTFMove(completionHandler), originsToDelete = WTFMove(originsToDelete), cacheKeysToDelete = WTFMove(cacheKeysToDelete)](auto* traversalEntry) mutable {
        if (traversalEntry) {
            if (originsToDelete.contains(SecurityOrigin::create(traversalEntry->entry.response().url())))
                cacheKeysToDelete.append(traversalEntry->entry.key());
            return;
        }

        cache->remove(cacheKeysToDelete, WTFMove(completionHandler));
        return;
    });
}

void NetworkProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& originDatas, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, uint64_t callbackID)
{
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteCookiesForHostnames(cookieHostNames);
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            deleteHSTSCacheForHostNames(*networkStorageSession, HSTSCacheHostNames);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::AdClickAttributions)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            for (auto& originData : originDatas)
                networkSession->clearAdClickAttributionForRegistrableDomain(RegistrableDomain::uncheckedCreateFromHost(originData.host));
        }
    }
    
    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        for (auto& originData : originDatas)
            CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { originData }, [clearTasksHandler = clearTasksHandler.copyRef()] { });
    }

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteSessionStorageForOrigins(sessionID, originDatas, [clearTasksHandler = clearTasksHandler.copyRef()] { });

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteLocalStorageForOrigins(sessionID, originDatas, [clearTasksHandler = clearTasksHandler.copyRef()] { });

#if ENABLE(INDEXED_DATABASE)
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !sessionID.isEphemeral())
        idbServer(sessionID).closeAndDeleteDatabasesForOrigins(originDatas, [clearTasksHandler = clearTasksHandler.copyRef()] { });
#endif

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && !sessionID.isEphemeral()) {
        auto& server = swServerForSession(sessionID);
        for (auto& originData : originDatas)
            server.clear(originData, [clearTasksHandler = clearTasksHandler.copyRef()] { });
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral()) {
        forEachNetworkSession([originDatas, &clearTasksHandler](auto& session) {
            clearDiskCacheEntries(session.cache(), originDatas, [clearTasksHandler = clearTasksHandler.copyRef()] { });
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID)) {
            for (auto& originData : originDatas)
                session->credentialStorage().removeCredentialsWithOrigin(originData);
        }
        WebCore::CredentialStorage::removeSessionCredentialsWithOrigins(originDatas);
    }

    // FIXME: Implement storage quota clearing for these origins.
}

void NetworkProcess::clearStorageQuota(PAL::SessionID sessionID)
{
    auto iterator = m_storageQuotaManagers.find(sessionID);
    if (iterator == m_storageQuotaManagers.end())
        return;

    auto& managers = iterator->value;
    for (auto& manager : managers.managersPerOrigin())
        manager.value->resetQuota(managers.defaultQuota(manager.key));
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
static Vector<String> filterForRegistrableDomains(const Vector<RegistrableDomain>& registrableDomains, const HashSet<String>& foundValues)
{
    Vector<String> result;
    for (const auto& value : foundValues) {
        if (registrableDomains.contains(RegistrableDomain::uncheckedCreateFromHost(value)))
            result.append(value);
    }
    
    return result;
}

static Vector<WebsiteData::Entry> filterForRegistrableDomains(const Vector<RegistrableDomain>& registrableDomains, const Vector<WebsiteData::Entry>& foundValues)
{
    Vector<WebsiteData::Entry> result;
    for (const auto& value : foundValues) {
        if (registrableDomains.contains(RegistrableDomain::uncheckedCreateFromHost(value.origin.host)))
            result.append(value);
    }
    
    return result;
}

static Vector<WebCore::SecurityOriginData> filterForRegistrableDomains(const HashSet<WebCore::SecurityOriginData>& origins, const Vector<RegistrableDomain>& domainsToDelete, HashSet<RegistrableDomain>& domainsDeleted)
{
    Vector<SecurityOriginData> originsDeleted;
    for (const auto& origin : origins) {
        auto domain = RegistrableDomain::uncheckedCreateFromHost(origin.host);
        if (!domainsToDelete.contains(domain))
            continue;
        originsDeleted.append(origin);
        domainsDeleted.add(domain);
    }

    return originsDeleted;
}

void NetworkProcess::deleteWebsiteDataForRegistrableDomains(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>>&& domains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
{
    OptionSet<WebsiteDataFetchOption> fetchOptions = WebsiteDataFetchOption::DoNotCreateProcesses;

    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }
        
        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), domains = WTFMove(m_domains)] () mutable {
                completionHandler(domains);
            });
        }
        
        CompletionHandler<void(const HashSet<RegistrableDomain>&)> m_completionHandler;
        HashSet<RegistrableDomain> m_domains;
    };
    
    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, completionHandler = WTFMove(completionHandler), shouldNotifyPage] (const HashSet<RegistrableDomain>& domainsWithData) mutable {
        if (shouldNotifyPage)
            parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyWebsiteDataDeletionForRegistrableDomainsFinished(), 0);
        
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), domainsWithData = crossThreadCopy(domainsWithData)] () mutable {
            completionHandler(domainsWithData);
        });
    }));

    HashSet<String> hostNamesWithCookies;
    HashSet<String> hostNamesWithHSTSCache;

    Vector<RegistrableDomain> domainsToDeleteCookiesFor;
    Vector<RegistrableDomain> domainsToDeleteAllButHttpOnlyCookiesFor;
    Vector<RegistrableDomain> domainsToDeleteAllButCookiesFor;
    Vector<String> hostnamesWithCookiesToDelete;
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        for (auto& pair : domains) {
            auto& domain = pair.first;
            auto& dataToRemove = pair.second;
            domainsToDeleteAllButCookiesFor.append(domain);
            switch (dataToRemove) {
            case WebsiteDataToRemove::All:
                domainsToDeleteCookiesFor.append(domain);
                break;
            case WebsiteDataToRemove::AllButHttpOnlyCookies:
                domainsToDeleteAllButHttpOnlyCookiesFor.append(domain);
                break;
            case WebsiteDataToRemove::AllButCookies:
                // Already added.
                break;
            }
        }
        if (auto* networkStorageSession = storageSession(sessionID)) {
            networkStorageSession->getHostnamesWithCookies(hostNamesWithCookies);

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domainsToDeleteCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::Yes);

            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domainsToDeleteAllButHttpOnlyCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::No);

            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));
        }
    } else {
        for (auto& domain : domains)
            domainsToDeleteAllButCookiesFor.append(domain.first);
    }

    Vector<String> hostnamesWithHSTSToDelete;
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            getHostNamesWithHSTSCache(*networkStorageSession, hostNamesWithHSTSCache);
            hostnamesWithHSTSToDelete = filterForRegistrableDomains(domainsToDeleteAllButCookiesFor, hostNamesWithHSTSCache);

            for (const auto& host : hostnamesWithHSTSToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

            deleteHSTSCacheForHostNames(*networkStorageSession, hostnamesWithHSTSToDelete);
        }
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID)) {
            auto origins = session->credentialStorage().originsWithCredentials();
            auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllButCookiesFor, callbackAggregator->m_domains);
            for (auto& origin : originsToDelete)
                session->credentialStorage().removeCredentialsWithOrigin(origin);
        }

        auto origins = WebCore::CredentialStorage::originsWithSessionCredentials();
        auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllButCookiesFor, callbackAggregator->m_domains);
        WebCore::CredentialStorage::removeSessionCredentialsWithOrigins(originsToDelete);
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [this, domainsToDeleteAllButCookiesFor, sessionID, callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            
            auto entriesToDelete = filterForRegistrableDomains(domainsToDeleteAllButCookiesFor, entries);

            for (const auto& entry : entriesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host));

            for (auto& entry : entriesToDelete)
                CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { entry.origin }, [callbackAggregator = callbackAggregator.copyRef()] { });
        });
    }

    if (m_storageManagerSet->contains(sessionID)) {
        if (websiteDataTypes.contains(WebsiteDataType::SessionStorage)) {
            m_storageManagerSet->getSessionStorageOrigins(sessionID, [protectedThis = makeRef(*this), this, sessionID, callbackAggregator = callbackAggregator.copyRef(), domainsToDeleteAllButCookiesFor](auto&& origins) {
                auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllButCookiesFor, callbackAggregator->m_domains);
                m_storageManagerSet->deleteSessionStorageForOrigins(sessionID, originsToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
            });
        }

        if (websiteDataTypes.contains(WebsiteDataType::LocalStorage)) {
            m_storageManagerSet->getLocalStorageOrigins(sessionID, [protectedThis = makeRef(*this), this, sessionID, callbackAggregator = callbackAggregator.copyRef(), domainsToDeleteAllButCookiesFor](auto&& origins) {
                auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllButCookiesFor, callbackAggregator->m_domains);
                m_storageManagerSet->deleteLocalStorageForOrigins(sessionID, originsToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
            });
        }
    }

#if ENABLE(INDEXED_DATABASE)
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, sessionID, callbackAggregator = callbackAggregator.copyRef(), path = crossThreadCopy(path), domainsToDeleteAllButCookiesFor = crossThreadCopy(domainsToDeleteAllButCookiesFor)]() mutable {
            RunLoop::main().dispatch([this, sessionID, domainsToDeleteAllButCookiesFor = WTFMove(domainsToDeleteAllButCookiesFor), callbackAggregator = callbackAggregator.copyRef(), securityOrigins = indexedDatabaseOrigins(path)] {
                Vector<SecurityOriginData> entriesToDelete;
                for (const auto& securityOrigin : securityOrigins) {
                    auto domain = RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host);
                    if (!domainsToDeleteAllButCookiesFor.contains(domain))
                        continue;

                    entriesToDelete.append(securityOrigin);
                    callbackAggregator->m_domains.add(domain);
                }

                idbServer(sessionID).closeAndDeleteDatabasesForOrigins(entriesToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
            });
        }));
    }
#endif
    
#if ENABLE(SERVICE_WORKER)
    path = m_swDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([this, sessionID, domainsToDeleteAllButCookiesFor, callbackAggregator = callbackAggregator.copyRef()](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins) {
                if (!domainsToDeleteAllButCookiesFor.contains(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host)))
                    continue;
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host));
                swServerForSession(sessionID).clear(securityOrigin, [callbackAggregator = callbackAggregator.copyRef()] { });
            }
        });
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &domainsToDeleteAllButCookiesFor, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [domainsToDeleteAllButCookiesFor, callbackAggregator = callbackAggregator.copyRef(), session = makeWeakPtr(&session)](auto entries) mutable {
                if (!session)
                    return;

                Vector<SecurityOriginData> entriesToDelete;
                for (auto& entry : entries) {
                    if (!domainsToDeleteAllButCookiesFor.contains(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host)))
                        continue;
                    entriesToDelete.append(entry.origin);
                    callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host));
                }
                clearDiskCacheEntries(session->cache(), entriesToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
            });
        });
    }

    auto dataTypesForUIProcess = WebsiteData::filter(websiteDataTypes, WebsiteDataProcessType::UI);
    if (!dataTypesForUIProcess.isEmpty() && !domainsToDeleteAllButCookiesFor.isEmpty()) {
        CompletionHandler<void(const HashSet<RegistrableDomain>&)> completionHandler = [callbackAggregator = callbackAggregator.copyRef()] (const HashSet<RegistrableDomain>& domains) {
            for (auto& domain : domains)
                callbackAggregator->m_domains.add(domain);
        };
        parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::DeleteWebsiteDataInUIProcessForRegistrableDomains(sessionID, dataTypesForUIProcess, fetchOptions, domainsToDeleteAllButCookiesFor), WTFMove(completionHandler));
    }
}

void NetworkProcess::deleteCookiesForTesting(PAL::SessionID sessionID, RegistrableDomain domain, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    OptionSet<WebsiteDataType> cookieType = WebsiteDataType::Cookies;
    Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>> toDeleteFor = {
        std::make_pair(domain, includeHttpOnlyCookies ? WebsiteDataToRemove::All : WebsiteDataToRemove::AllButHttpOnlyCookies)
    };
    deleteWebsiteDataForRegistrableDomains(sessionID, cookieType, WTFMove(toDeleteFor), true, [completionHandler = WTFMove(completionHandler)] (const HashSet<RegistrableDomain>& domainsDeletedFor) mutable {
        UNUSED_PARAM(domainsDeletedFor);
        completionHandler();
    });
}

void NetworkProcess::registrableDomainsWithWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    OptionSet<WebsiteDataFetchOption> fetchOptions = WebsiteDataFetchOption::DoNotCreateProcesses;
    
    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }
        
        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), websiteData = WTFMove(m_websiteData)] () mutable {
                HashSet<RegistrableDomain> domains;
                for (const auto& hostnameWithCookies : websiteData.hostNamesWithCookies)
                    domains.add(RegistrableDomain::uncheckedCreateFromHost(hostnameWithCookies));

                for (const auto& hostnameWithHSTS : websiteData.hostNamesWithHSTSCache)
                    domains.add(RegistrableDomain::uncheckedCreateFromHost(hostnameWithHSTS));

                for (const auto& entry : websiteData.entries)
                    domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host));

                completionHandler(WTFMove(domains));
            });
        }
        
        CompletionHandler<void(HashSet<RegistrableDomain>&&)> m_completionHandler;
        WebsiteData m_websiteData;
    };
    
    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, completionHandler = WTFMove(completionHandler), shouldNotifyPage] (HashSet<RegistrableDomain>&& domainsWithData) mutable {
        if (shouldNotifyPage)
            parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyWebsiteDataScanForRegistrableDomainsFinished(), 0);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), domainsWithData = crossThreadCopy(domainsWithData)] () mutable {
            completionHandler(WTFMove(domainsWithData));
        });
    }));
    
    auto& websiteDataStore = callbackAggregator->m_websiteData;
    
    Vector<String> hostnamesWithCookiesToDelete;
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->getHostnamesWithCookies(websiteDataStore.hostNamesWithCookies);
    }
    
    Vector<String> hostnamesWithHSTSToDelete;
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            getHostNamesWithHSTSCache(*networkStorageSession, websiteDataStore.hostNamesWithHSTSCache);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            auto securityOrigins = networkStorageSession->credentialStorage().originsWithCredentials();
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::Credentials, 0 });
        }
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
    
#if ENABLE(INDEXED_DATABASE)
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, callbackAggregator = callbackAggregator.copyRef(), path = crossThreadCopy(path)]() mutable {
            RunLoop::main().dispatch([callbackAggregator = callbackAggregator.copyRef(), securityOrigins = indexedDatabaseOrigins(path)] {
                for (const auto& securityOrigin : securityOrigins)
                    callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });
            });
        }));
    }
#endif
    
#if ENABLE(SERVICE_WORKER)
    path = m_swDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([callbackAggregator = callbackAggregator.copyRef()](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
#endif
    
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
                callbackAggregator->m_websiteData.entries.appendVector(entries);
            });
        });
    }
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

CacheStorage::Engine* NetworkProcess::findCacheEngine(const PAL::SessionID& sessionID)
{
    return m_cacheEngines.get(sessionID);
}

CacheStorage::Engine& NetworkProcess::ensureCacheEngine(const PAL::SessionID& sessionID, Function<Ref<CacheStorage::Engine>()>&& functor)
{
    return m_cacheEngines.ensure(sessionID, WTFMove(functor)).iterator->value;
}

void NetworkProcess::removeCacheEngine(const PAL::SessionID& sessionID)
{
    m_cacheEngines.remove(sessionID);
}

void NetworkProcess::downloadRequest(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedFilename)
{
    downloadManager().startDownload(sessionID, downloadID, request, suggestedFilename);
}

void NetworkProcess::resumeDownload(PAL::SessionID sessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, WebKit::SandboxExtension::Handle&& sandboxExtensionHandle)
{
    downloadManager().resumeDownload(sessionID, downloadID, resumeData, path, WTFMove(sandboxExtensionHandle));
}

void NetworkProcess::cancelDownload(DownloadID downloadID)
{
    downloadManager().cancelDownload(downloadID);
}

#if PLATFORM(COCOA)
void NetworkProcess::publishDownloadProgress(DownloadID downloadID, const URL& url, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    downloadManager().publishDownloadProgress(downloadID, url, WTFMove(sandboxExtensionHandle));
}
#endif

void NetworkProcess::continueWillSendRequest(DownloadID downloadID, WebCore::ResourceRequest&& request)
{
    downloadManager().continueWillSendRequest(downloadID, WTFMove(request));
}

void NetworkProcess::pendingDownloadCanceled(DownloadID downloadID)
{
    downloadProxyConnection()->send(Messages::DownloadProxy::DidCancel({ }), downloadID.downloadID());
}

void NetworkProcess::findPendingDownloadLocation(NetworkDataTask& networkDataTask, ResponseCompletionHandler&& completionHandler, const ResourceResponse& response)
{
    uint64_t destinationID = networkDataTask.pendingDownloadID().downloadID();
    downloadProxyConnection()->send(Messages::DownloadProxy::DidReceiveResponse(response), destinationID);

    downloadManager().willDecidePendingDownloadDestination(networkDataTask, WTFMove(completionHandler));

    // As per https://html.spec.whatwg.org/#as-a-download (step 2), the filename from the Content-Disposition header
    // should override the suggested filename from the download attribute.
    String suggestedFilename = response.isAttachmentWithFilename() ? response.suggestedFilename() : networkDataTask.suggestedFilename();
    suggestedFilename = MIMETypeRegistry::appendFileExtensionIfNecessary(suggestedFilename, response.mimeType());

    downloadProxyConnection()->send(Messages::DownloadProxy::DecideDestinationWithSuggestedFilenameAsync(networkDataTask.pendingDownloadID(), suggestedFilename), destinationID);
}

void NetworkProcess::continueDecidePendingDownloadDestination(DownloadID downloadID, String destination, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    if (destination.isEmpty())
        downloadManager().cancelDownload(downloadID);
    else
        downloadManager().continueDecidePendingDownloadDestination(downloadID, destination, WTFMove(sandboxExtensionHandle), allowOverwrite);
}

void NetworkProcess::setCacheModel(CacheModel cacheModel)
{
    if (m_hasSetCacheModel && (cacheModel == m_cacheModel))
        return;

    m_hasSetCacheModel = true;
    m_cacheModel = cacheModel;

    unsigned urlCacheMemoryCapacity = 0;
    uint64_t urlCacheDiskCapacity = 0;
    uint64_t diskFreeSize = 0;
    if (FileSystem::getVolumeFreeSpace(m_diskCacheDirectory, diskFreeSize)) {
        // As a fudge factor, use 1000 instead of 1024, in case the reported byte
        // count doesn't align exactly to a megabyte boundary.
        diskFreeSize /= KB * 1000;
        calculateURLCacheSizes(cacheModel, diskFreeSize, urlCacheMemoryCapacity, urlCacheDiskCapacity);
    }

    forEachNetworkSession([urlCacheDiskCapacity](auto& session) {
        if (auto* cache = session.cache())
            cache->setCapacity(urlCacheDiskCapacity);
    });
}

void NetworkProcess::setCanHandleHTTPSServerTrustEvaluation(bool value)
{
    m_canHandleHTTPSServerTrustEvaluation = value;
}

void NetworkProcess::getNetworkProcessStatistics(uint64_t callbackID)
{
    StatisticsData data;

    data.statisticsNumbers.set("DownloadsActiveCount", downloadManager().activeDownloadCount());
    data.statisticsNumbers.set("OutstandingAuthenticationChallengesCount", authenticationManager().outstandingAuthenticationChallengeCount());

    parentProcessConnection()->send(Messages::WebProcessPool::DidGetStatistics(data, callbackID), 0);
}

void NetworkProcess::setAllowsAnySSLCertificateForWebSocket(bool allows, CompletionHandler<void()>&& completionHandler)
{
    DeprecatedGlobalSettings::setAllowsAnySSLCertificate(allows);
    completionHandler();
}

void NetworkProcess::logDiagnosticMessage(PageIdentifier webPageID, const String& message, const String& description, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessage(webPageID, message, description, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithResult(PageIdentifier webPageID, const String& message, const String& description, DiagnosticLoggingResultType result, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessageWithResult(webPageID, message, description, result, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithValue(PageIdentifier webPageID, const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessageWithValue(webPageID, message, description, value, significantFigures, ShouldSample::No), 0);
}

void NetworkProcess::terminate()
{
    platformTerminate();
    AuxiliaryProcess::terminate();
}

void NetworkProcess::processDidTransitionToForeground()
{
    platformProcessDidTransitionToForeground();
}

void NetworkProcess::processDidTransitionToBackground()
{
    platformProcessDidTransitionToBackground();
}

void NetworkProcess::actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend shouldAcknowledgeWhenReadyToSuspend)
{
#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(true);
#endif

    lowMemoryHandler(Critical::Yes);

    RefPtr<CallbackAggregator> callbackAggregator;
    if (shouldAcknowledgeWhenReadyToSuspend == ShouldAcknowledgeWhenReadyToSuspend::Yes) {
        callbackAggregator = CallbackAggregator::create([this] {
            RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::notifyProcessReadyToSuspend() Sending ProcessReadyToSuspend IPC message", this);
            if (parentProcessConnection())
                parentProcessConnection()->send(Messages::NetworkProcessProxy::ProcessReadyToSuspend(), 0);
        });
    }

    platformPrepareToSuspend([callbackAggregator] { });
    platformSyncAllCookies([callbackAggregator] { });

    for (auto& connection : m_webProcessConnections)
        connection->cleanupForSuspension([callbackAggregator] { });

#if ENABLE(SERVICE_WORKER)
    for (auto& server : m_swServers.values()) {
        ASSERT(m_swServers.get(server->sessionID()) == server.get());
        server->startSuspension([callbackAggregator] { });
    }
#endif

    m_storageManagerSet->suspend([callbackAggregator] { });
}

void NetworkProcess::processWillSuspendImminently()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processWillSuspendImminently() BEGIN", this);
#if PLATFORM(IOS_FAMILY) && ENABLE(INDEXED_DATABASE)
    for (auto& server : m_idbServers.values())
        server->tryStop(IDBServer::ShouldForceStop::Yes);
#endif
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::No);
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processWillSuspendImminently() END", this);
}

void NetworkProcess::processWillSuspendImminentlyForTestingSync(CompletionHandler<void()>&& completionHandler)
{
    processWillSuspendImminently();
    completionHandler();
}

void NetworkProcess::prepareToSuspend()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend()", this);

#if PLATFORM(IOS_FAMILY) && ENABLE(INDEXED_DATABASE)
    for (auto& server : m_idbServers.values())
        server->tryStop(IDBServer::ShouldForceStop::No);
#endif
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::Yes);
}

void NetworkProcess::cancelPrepareToSuspend()
{
    // Although it is tempting to send a NetworkProcessProxy::DidCancelProcessSuspension message from here
    // we do not because prepareToSuspend() already replied with a NetworkProcessProxy::ProcessReadyToSuspend
    // message. And NetworkProcessProxy expects to receive either a NetworkProcessProxy::ProcessReadyToSuspend-
    // or NetworkProcessProxy::DidCancelProcessSuspension- message, but not both.
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::cancelPrepareToSuspend()", this);
    resume();
}

void NetworkProcess::applicationDidEnterBackground()
{
    m_downloadManager.applicationDidEnterBackground();
}

void NetworkProcess::applicationWillEnterForeground()
{
    m_downloadManager.applicationWillEnterForeground();
}

void NetworkProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processDidResume()", this);
    resume();
}

void NetworkProcess::resume()
{
#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(false);
#endif

    platformProcessDidResume();
    for (auto& connection : m_webProcessConnections)
        connection->endSuspension();

#if ENABLE(SERVICE_WORKER)
    for (auto& server : m_swServers.values())
        server->endSuspension();
#endif
#if PLATFORM(IOS_FAMILY) && ENABLE(INDEXED_DATABASE)
    for (auto& server : m_idbServers.values())
        server->resume();
#endif

    m_storageManagerSet->resume();
}

void NetworkProcess::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

void NetworkProcess::cacheStorageRootPath(PAL::SessionID sessionID, CacheStorageRootPathCallback&& callback)
{
    m_cacheStorageParametersCallbacks.ensure(sessionID, [&] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::RetrieveCacheStorageParameters { sessionID }, 0);
        return Vector<CacheStorageRootPathCallback> { };
    }).iterator->value.append(WTFMove(callback));
}

void NetworkProcess::setCacheStorageParameters(PAL::SessionID sessionID, String&& cacheStorageDirectory, SandboxExtension::Handle&& handle)
{
    auto iterator = m_cacheStorageParametersCallbacks.find(sessionID);
    if (iterator == m_cacheStorageParametersCallbacks.end())
        return;

    SandboxExtension::consumePermanently(handle);
    auto callbacks = WTFMove(iterator->value);
    m_cacheStorageParametersCallbacks.remove(iterator);
    for (auto& callback : callbacks)
        callback(String { cacheStorageDirectory });
}

void NetworkProcess::preconnectTo(const URL& url, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
#if ENABLE(SERVER_PRECONNECT)
    NetworkLoadParameters parameters { PAL::SessionID::defaultSessionID() };
    parameters.request = ResourceRequest { url };
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;

    new PreconnectTask(*this, WTFMove(parameters));
#else
    UNUSED_PARAM(url);
    UNUSED_PARAM(storedCredentialsPolicy);
#endif
}

void NetworkProcess::registerURLSchemeAsSecure(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsSecure(scheme);
}

void NetworkProcess::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void NetworkProcess::registerURLSchemeAsLocal(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsLocal(scheme);
}

void NetworkProcess::registerURLSchemeAsNoAccess(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsNoAccess(scheme);
}

void NetworkProcess::registerURLSchemeAsDisplayIsolated(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsDisplayIsolated(scheme);
}

void NetworkProcess::registerURLSchemeAsCORSEnabled(const String& scheme) const
{
    SchemeRegistry::registerURLSchemeAsCORSEnabled(scheme);
}

void NetworkProcess::registerURLSchemeAsCanDisplayOnlyIfCanRequest(const String& scheme) const
{
    SchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(scheme);
}

void NetworkProcess::didSyncAllCookies()
{
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidSyncAllCookies(), 0);
}

#if ENABLE(INDEXED_DATABASE)
Ref<IDBServer::IDBServer> NetworkProcess::createIDBServer(PAL::SessionID sessionID)
{
    String path;
    if (!sessionID.isEphemeral()) {
        ASSERT(m_idbDatabasePaths.contains(sessionID));
        path = m_idbDatabasePaths.get(sessionID);
    }

    return IDBServer::IDBServer::create(sessionID, path, *this, [this, weakThis = makeWeakPtr(this)](PAL::SessionID sessionID, const auto& origin) -> StorageQuotaManager* {
        if (!weakThis)
            return nullptr;
        return &this->storageQuotaManager(sessionID, origin);
    });
}

IDBServer::IDBServer& NetworkProcess::idbServer(PAL::SessionID sessionID)
{
    return *m_idbServers.ensure(sessionID, [this, sessionID] {
        return this->createIDBServer(sessionID);
    }).iterator->value;
}

void NetworkProcess::ensurePathExists(const String& path)
{
    ASSERT(!RunLoop::isMain());
    
    if (!FileSystem::makeAllDirectories(path))
        LOG_ERROR("Failed to make all directories for path '%s'", path.utf8().data());
}

void NetworkProcess::postStorageTask(CrossThreadTask&& task)
{
    ASSERT(RunLoop::isMain());
    
    LockHolder locker(m_storageTaskMutex);
    
    m_storageTasks.append(WTFMove(task));
    
    m_storageTaskQueue->dispatch([this] {
        performNextStorageTask();
    });
}

void NetworkProcess::performNextStorageTask()
{
    ASSERT(!RunLoop::isMain());
    
    CrossThreadTask task;
    {
        LockHolder locker(m_storageTaskMutex);
        ASSERT(!m_storageTasks.isEmpty());
        task = m_storageTasks.takeFirst();
    }
    
    task.performTask();
}

void NetworkProcess::accessToTemporaryFileComplete(const String& path)
{
    // We've either hard linked the temporary blob file to the database directory, copied it there,
    // or the transaction is being aborted.
    // In any of those cases, we can delete the temporary blob file now.
    FileSystem::deleteFile(path);
}

void NetworkProcess::collectIndexedDatabaseOriginsForVersion(const String& path, HashSet<WebCore::SecurityOriginData>& securityOrigins)
{
    if (path.isEmpty())
        return;

    for (auto& topOriginPath : FileSystem::listDirectory(path, "*")) {
        auto databaseIdentifier = FileSystem::pathGetFileName(topOriginPath);
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier)) {
            securityOrigins.add(WTFMove(*securityOrigin));
        
            for (auto& originPath : FileSystem::listDirectory(topOriginPath, "*")) {
                databaseIdentifier = FileSystem::pathGetFileName(originPath);
                if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
                    securityOrigins.add(WTFMove(*securityOrigin));
            }
        }
    }
}

HashSet<WebCore::SecurityOriginData> NetworkProcess::indexedDatabaseOrigins(const String& path)
{
    if (path.isEmpty())
        return { };
    
    HashSet<WebCore::SecurityOriginData> securityOrigins;
    collectIndexedDatabaseOriginsForVersion(FileSystem::pathByAppendingComponent(path, "v0"), securityOrigins);
    collectIndexedDatabaseOriginsForVersion(FileSystem::pathByAppendingComponent(path, "v1"), securityOrigins);

    return securityOrigins;
}

void NetworkProcess::addIndexedDatabaseSession(PAL::SessionID sessionID, String& indexedDatabaseDirectory, SandboxExtension::Handle& handle)
{
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    auto addResult = m_idbDatabasePaths.add(sessionID, indexedDatabaseDirectory);
    if (addResult.isNewEntry) {
        SandboxExtension::consumePermanently(handle);
        if (!indexedDatabaseDirectory.isEmpty())
            postStorageTask(createCrossThreadTask(*this, &NetworkProcess::ensurePathExists, indexedDatabaseDirectory));
    }
}
#endif // ENABLE(INDEXED_DATABASE)

void NetworkProcess::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    m_storageManagerSet->waitUntilSyncingLocalStorageFinished();
    completionHandler();
}

void NetworkProcess::clearLegacyPrivateBrowsingLocalStorage()
{
    if (m_storageManagerSet->contains(PAL::SessionID::legacyPrivateSessionID()))
        m_storageManagerSet->deleteLocalStorageModifiedSince(PAL::SessionID::legacyPrivateSessionID(), -WallTime::infinity(), []() { });
}

void NetworkProcess::updateQuotaBasedOnSpaceUsageForTesting(PAL::SessionID sessionID, const ClientOrigin& origin)
{
    auto& manager = storageQuotaManager(sessionID, origin);
    manager.resetQuota(m_storageQuotaManagers.find(sessionID)->value.defaultQuota(origin));
    manager.updateQuotaBasedOnSpaceUsage();
}

#if ENABLE(SANDBOX_EXTENSIONS)
void NetworkProcess::getSandboxExtensionsForBlobFiles(const Vector<String>& filenames, CompletionHandler<void(SandboxExtension::HandleArray&&)>&& completionHandler)
{
    parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::GetSandboxExtensionsForBlobFiles(filenames), WTFMove(completionHandler));
}
#endif // ENABLE(SANDBOX_EXTENSIONS)

#if ENABLE(SERVICE_WORKER)
WebSWServerToContextConnection* NetworkProcess::connectionToContextProcessFromIPCConnection(IPC::Connection& connection)
{
    for (auto& serverToContextConnection : m_serverToContextConnections.values()) {
        if (serverToContextConnection->ipcConnection() == &connection)
            return serverToContextConnection.get();
    }
    return nullptr;
}

void NetworkProcess::connectionToContextProcessWasClosed(Ref<WebSWServerToContextConnection>&& serverToContextConnection)
{
    auto& registrableDomain = serverToContextConnection->registrableDomain();
    
    serverToContextConnection->connectionClosed();
    m_serverToContextConnections.remove(registrableDomain);
    
    for (auto& swServer : m_swServers.values())
        swServer->markAllWorkersForRegistrableDomainAsTerminated(registrableDomain);
    
    if (needsServerToContextConnectionForRegistrableDomain(registrableDomain)) {
        RELEASE_LOG(ServiceWorker, "Connection to service worker process was closed but is still needed, relaunching it");
        createServerToContextConnection(registrableDomain, WTF::nullopt);
    }
}

bool NetworkProcess::needsServerToContextConnectionForRegistrableDomain(const RegistrableDomain& registrableDomain) const
{
    return WTF::anyOf(m_swServers.values(), [&](auto& swServer) {
        return swServer->needsServerToContextConnectionForRegistrableDomain(registrableDomain);
    });
}

SWServer& NetworkProcess::swServerForSession(PAL::SessionID sessionID)
{
    auto result = m_swServers.ensure(sessionID, [&] {
        auto path = m_swDatabasePaths.get(sessionID);
        // There should already be a registered path for this PAL::SessionID.
        // If there's not, then where did this PAL::SessionID come from?
        ASSERT(sessionID.isEphemeral() || !path.isEmpty());
        
        auto value = makeUnique<SWServer>(makeUniqueRef<WebSWOriginStore>(), WTFMove(path), sessionID);
        if (m_shouldDisableServiceWorkerProcessTerminationDelay)
            value->disableServiceWorkerProcessTerminationDelay();
        return value;
    });

    return *result.iterator->value;
}

WebSWOriginStore* NetworkProcess::existingSWOriginStoreForSession(PAL::SessionID sessionID) const
{
    auto* swServer = m_swServers.get(sessionID);
    if (!swServer)
        return nullptr;
    return &static_cast<WebSWOriginStore&>(swServer->originStore());
}

WebSWServerToContextConnection* NetworkProcess::serverToContextConnectionForRegistrableDomain(const RegistrableDomain& registrableDomain)
{
    return m_serverToContextConnections.get(registrableDomain);
}

void NetworkProcess::createServerToContextConnection(const RegistrableDomain& registrableDomain, Optional<PAL::SessionID> sessionID)
{
    if (m_waitingForServerToContextProcessConnection)
        return;
    
    m_waitingForServerToContextProcessConnection = true;
    if (sessionID)
        parentProcessConnection()->send(Messages::NetworkProcessProxy::EstablishWorkerContextConnectionToNetworkProcessForExplicitSession(registrableDomain, *sessionID), 0);
    else
        parentProcessConnection()->send(Messages::NetworkProcessProxy::EstablishWorkerContextConnectionToNetworkProcess(registrableDomain), 0);
}

void NetworkProcess::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, MessageWithMessagePorts&& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    if (auto connection = m_swServerConnections.get(destinationIdentifier.serverConnectionIdentifier))
        connection->postMessageToServiceWorkerClient(destinationIdentifier.contextIdentifier, WTFMove(message), sourceIdentifier, sourceOrigin);
}

void NetworkProcess::postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&& message, const WebCore::ServiceWorkerOrClientIdentifier& source, SWServerConnectionIdentifier connectionIdentifier)
{
    if (auto connection = m_swServerConnections.get(connectionIdentifier))
        connection->postMessageToServiceWorker(destination, WTFMove(message), source);
}

void NetworkProcess::registerSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(parentProcessHasServiceWorkerEntitlement());
    ASSERT(!m_swServerConnections.contains(connection.identifier()));
    m_swServerConnections.add(connection.identifier(), makeWeakPtr(connection));
    auto* store = existingSWOriginStoreForSession(connection.sessionID());
    ASSERT(store);
    if (store)
        store->registerSWServerConnection(connection);
}

void NetworkProcess::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(m_swServerConnections.get(connection.identifier()).get() == &connection);
    m_swServerConnections.remove(connection.identifier());
    if (auto* store = existingSWOriginStoreForSession(connection.sessionID()))
        store->unregisterSWServerConnection(connection);
}

void NetworkProcess::swContextConnectionMayNoLongerBeNeeded(WebSWServerToContextConnection& serverToContextConnection)
{
    auto& registrableDomain = serverToContextConnection.registrableDomain();
    if (needsServerToContextConnectionForRegistrableDomain(registrableDomain))
        return;
    
    RELEASE_LOG(ServiceWorker, "Service worker process is no longer needed, terminating it");
    serverToContextConnection.terminate();
    
    for (auto& swServer : m_swServers.values())
        swServer->markAllWorkersForRegistrableDomainAsTerminated(registrableDomain);
    
    serverToContextConnection.connectionClosed();
    m_serverToContextConnections.remove(registrableDomain);
}

void NetworkProcess::disableServiceWorkerProcessTerminationDelay()
{
    if (m_shouldDisableServiceWorkerProcessTerminationDelay)
        return;
    
    m_shouldDisableServiceWorkerProcessTerminationDelay = true;
    for (auto& swServer : m_swServers.values())
        swServer->disableServiceWorkerProcessTerminationDelay();
}

void NetworkProcess::addServiceWorkerSession(PAL::SessionID sessionID, String& serviceWorkerRegistrationDirectory, const SandboxExtension::Handle& handle)
{
    auto addResult = m_swDatabasePaths.add(sessionID, serviceWorkerRegistrationDirectory);
    if (addResult.isNewEntry) {
        SandboxExtension::consumePermanently(handle);
        if (!serviceWorkerRegistrationDirectory.isEmpty())
            postStorageTask(createCrossThreadTask(*this, &NetworkProcess::ensurePathExists, serviceWorkerRegistrationDirectory));
    }
}
#endif // ENABLE(SERVICE_WORKER)

void NetworkProcess::requestStorageSpace(PAL::SessionID sessionID, const ClientOrigin& origin, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(Optional<uint64_t>)>&& callback)
{
    parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageSpace { sessionID, origin, quota, currentSize, spaceRequired }, WTFMove(callback), 0);
}

class QuotaUserInitializer final : public WebCore::StorageQuotaUser {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit QuotaUserInitializer(StorageQuotaManager& manager)
        : m_manager(makeWeakPtr(manager))
    {
        manager.addUser(*this);
    }

    ~QuotaUserInitializer()
    {
        if (m_manager)
            m_manager->removeUser(*this);
        if (m_callback)
            m_callback();
    }

private:
    // StorageQuotaUser API.
    uint64_t spaceUsed() const final
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    void whenInitialized(CompletionHandler<void()>&& callback) final
    {
        m_callback = WTFMove(callback);
    }

    WeakPtr<StorageQuotaManager> m_manager;
    CompletionHandler<void()> m_callback;
};

void NetworkProcess::initializeQuotaUsers(StorageQuotaManager& manager, PAL::SessionID sessionID, const ClientOrigin& origin)
{
    RunLoop::main().dispatch([this, weakThis = makeWeakPtr(this), sessionID, origin, user = makeUnique<QuotaUserInitializer>(manager)]() mutable {
        if (!weakThis)
            return;
        this->idbServer(sessionID).initializeQuotaUser(origin);
        CacheStorage::Engine::initializeQuotaUser(*this, sessionID, origin, [user = WTFMove(user)] { });
    });
}

StorageQuotaManager& NetworkProcess::storageQuotaManager(PAL::SessionID sessionID, const ClientOrigin& origin)
{
    auto& storageQuotaManagers = m_storageQuotaManagers.ensure(sessionID, [] {
        return StorageQuotaManagers { };
    }).iterator->value;
    return *storageQuotaManagers.managersPerOrigin().ensure(origin, [this, &storageQuotaManagers, sessionID, &origin] {
        auto manager = makeUnique<StorageQuotaManager>(storageQuotaManagers.defaultQuota(origin), [this, sessionID, origin](uint64_t quota, uint64_t currentSpace, uint64_t spaceIncrease, auto callback) {
            this->requestStorageSpace(sessionID, origin, quota, currentSpace, spaceIncrease, WTFMove(callback));
        });
        initializeQuotaUsers(*manager, sessionID, origin);
        return manager;
    }).iterator->value;
}

#if !PLATFORM(COCOA)
void NetworkProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
}

void NetworkProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
}

void NetworkProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void NetworkProcess::syncAllCookies()
{
}

void NetworkProcess::platformSyncAllCookies(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

#endif

void NetworkProcess::storeAdClickAttribution(PAL::SessionID sessionID, WebCore::AdClickAttribution&& adClickAttribution)
{
    if (auto* session = networkSession(sessionID))
        session->storeAdClickAttribution(WTFMove(adClickAttribution));
}

void NetworkProcess::dumpAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        return session->dumpAdClickAttribution(WTFMove(completionHandler));

    completionHandler({ });
}

void NetworkProcess::clearAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->clearAdClickAttribution();
    
    completionHandler();
}

void NetworkProcess::setAdClickAttributionOverrideTimerForTesting(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setAdClickAttributionOverrideTimerForTesting(value);
    
    completionHandler();
}

void NetworkProcess::setAdClickAttributionConversionURLForTesting(PAL::SessionID sessionID, URL&& url, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setAdClickAttributionConversionURLForTesting(WTFMove(url));
    
    completionHandler();
}

void NetworkProcess::markAdClickAttributionsAsExpiredForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->markAdClickAttributionsAsExpiredForTesting();

    completionHandler();
}

void NetworkProcess::addKeptAliveLoad(Ref<NetworkResourceLoader>&& loader)
{
    if (auto* session = networkSession(loader->sessionID()))
        session->addKeptAliveLoad(WTFMove(loader));
}

void NetworkProcess::removeKeptAliveLoad(NetworkResourceLoader& loader)
{
    if (auto* session = networkSession(loader.sessionID()))
        session->removeKeptAliveLoad(loader);
}

void NetworkProcess::getLocalStorageOriginDetails(PAL::SessionID sessionID, CompletionHandler<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&& completionHandler)
{
    if (!m_storageManagerSet->contains(sessionID)) {
        LOG_ERROR("Cannot get local storage information for an unknown session");
        return;
    }

    m_storageManagerSet->getLocalStorageOriginDetails(sessionID, [completionHandler = WTFMove(completionHandler)](auto&& details) mutable {
        completionHandler(WTFMove(details));
    });
}

void NetworkProcess::connectionToWebProcessClosed(IPC::Connection& connection)
{
    m_storageManagerSet->removeConnection(connection);
}

} // namespace WebKit
