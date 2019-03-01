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
#include "DownloadProxyMessages.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessPlatformStrategies.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkProximityManager.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "PreconnectTask.h"
#include "RemoteNetworkingContext.h"
#include "ShouldGrandfatherStatistics.h"
#include "StatisticsData.h"
#include "StorageAccessStatus.h"
#include "WebCookieManager.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSWOriginStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerToContextConnection.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteDataType.h"
#include <WebCore/DNS.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStateNotifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#include "NetworkCache.h"
#include "NetworkCacheCoders.h"

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif

#if USE(SOUP)
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
#if ENABLE(CONTENT_EXTENSIONS)
    , m_networkContentRuleListManager(*this)
#endif
#if PLATFORM(IOS_FAMILY)
    , m_webSQLiteDatabaseTracker(*this)
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
#if ENABLE(PROXIMITY_NETWORKING)
    addSupplement<NetworkProximityManager>();
#endif

#if USE(SOUP)
    DNSResolveQueueSoup::setGlobalDefaultNetworkStorageSessionAccessor([this]() -> NetworkStorageSession& {
        return defaultStorageSession();
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
            callback(String { }, 0);
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

#if ENABLE(PROXIMITY_NETWORKING)
NetworkProximityManager& NetworkProcess::proximityManager()
{
    return *supplement<NetworkProximityManager>();
}
#endif

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
    AtomicString::init();

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

    auto* defaultSession = networkSession(PAL::SessionID::defaultSessionID());
    for (const auto& cookie : parameters.defaultDataStoreParameters.pendingCookies)
        defaultSession->networkStorageSession().setCookie(cookie);

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

void NetworkProcess::createNetworkConnectionToWebProcess(bool isServiceWorkerProcess, WebCore::SecurityOriginData&& securityOrigin)
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
        auto contextConnection = WebSWServerToContextConnection::create(*this, securityOrigin, m_webProcessConnections.last()->connection());
        auto addResult = m_serverToContextConnections.add(WTFMove(securityOrigin), contextConnection.copyRef());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);

        m_waitingForServerToContextProcessConnection = false;

        for (auto* server : SWServer::allServers())
            server->serverToContextConnectionCreated(contextConnection);
    }
#else
    UNUSED_PARAM(isServiceWorkerProcess);
    UNUSED_PARAM(securityOrigin);
#endif
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

    RemoteNetworkingContext::ensureWebsiteDataStoreSession(*this, WTFMove(parameters));
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

    m_defaultNetworkStorageSession = std::make_unique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID(), WTFMove(session), WTFMove(cookieStorage));
#elif USE(SOUP)
    m_defaultNetworkStorageSession = std::make_unique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID(), std::make_unique<WebCore::SoupNetworkSession>());
#elif USE(CURL)
    m_defaultNetworkStorageSession = std::make_unique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID());
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

    addResult.iterator->value = std::make_unique<NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(cookieStorage));
#elif USE(SOUP)
    addResult.iterator->value = std::make_unique<NetworkStorageSession>(sessionID, std::make_unique<SoupNetworkSession>(sessionID));
#elif USE(CURL)
    addResult.iterator->value = std::make_unique<NetworkStorageSession>(sessionID);
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

NetworkSession* NetworkProcess::networkSession(const PAL::SessionID& sessionID) const
{
    return m_networkSessions.get(sessionID);
}

void NetworkProcess::setSession(const PAL::SessionID& sessionID, Ref<NetworkSession>&& session)
{
    m_networkSessions.set(sessionID, WTFMove(session));
}

void NetworkProcess::destroySession(const PAL::SessionID& sessionID)
{
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());

    if (auto session = m_networkSessions.take(sessionID))
        session->get().invalidateAndCancel();
    m_networkStorageSessions.remove(sessionID);
    m_sessionsControlledByAutomation.remove(sessionID);
    CacheStorage::Engine::destroyEngine(*this, sessionID);

#if ENABLE(SERVICE_WORKER)
    m_swServers.remove(sessionID);
    m_swDatabasePaths.remove(sessionID);
#endif
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
        networkStorageSession->setPrevalentDomainsToBlockCookiesFor(domainsToBlock);
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

void NetworkProcess::hasStorageAccessForFrame(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    bool hasStorageAccess = false;
    if (auto* networkStorageSession = storageSession(sessionID))
        hasStorageAccess = networkStorageSession->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID);
    else
        ASSERT_NOT_REACHED();

    completionHandler(hasStorageAccess);
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

void NetworkProcess::hasStorageAccess(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::requestStorageAccess(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->requestStorageAccess(subFrameDomain, topFrameDomain, frameID.value(), pageID, promptEnabled, WTFMove(completionHandler));
        else
            completionHandler(StorageAccessStatus::CannotRequestAccess);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(StorageAccessStatus::CannotRequestAccess);
    }
}

void NetworkProcess::requestStorageAccessGranted(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->requestStorageAccessGranted(subFrameDomain, topFrameDomain, frameID, pageID, promptEnabled, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::grantStorageAccess(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, bool userWasPrompted, CompletionHandler<void(bool)>&& completionHandler)
{
    // FIXME: We should not accept an optional frame ID since we call frameID.value() unconditionally.
    if (!frameID) {
        completionHandler(false);
        return;
    }

    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->grantStorageAccess(subFrameDomain, topFrameDomain, frameID.value(), pageID, userWasPrompted, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
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

void NetworkProcess::removeAllStorageAccess(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->removeAllStorageAccess();
    else
        ASSERT_NOT_REACHED();
    completionHandler();
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
    for (auto& networkSession : m_networkSessions.values())
        networkSession.get().setResourceLoadStatisticsEnabled(enabled);
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

void NetworkProcess::committedCrossSiteLoadWithLinkDecoration(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, uint64_t pageID)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->committedCrossSiteLoadWithLinkDecoration(fromDomain, toDomain, pageID);
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::resetCrossSiteLoadsWithLinkDecorationForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->resetCrossSiteLoadsWithLinkDecorationForTesting();
    else
        ASSERT_NOT_REACHED();
    completionHandler();
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
        if (storageSession(sessionID))
            callbackAggregator->m_websiteData.originsWithCredentials = storageSession(sessionID)->credentialStorage().originsWithCredentials();
    }

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }

#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            getHostNamesWithHSTSCache(*networkStorageSession, callbackAggregator->m_websiteData.hostNamesWithHSTSCache);
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, callbackAggregator = callbackAggregator.copyRef(), path = WTFMove(path)]() mutable {
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
        fetchDiskCacheEntries(cache(), sessionID, fetchOptions, [callbackAggregator = WTFMove(callbackAggregator)](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
}

void NetworkProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, uint64_t callbackID)
{
#if PLATFORM(COCOA)
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
    }

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        CacheStorage::Engine::clearAllCaches(*this, sessionID, [clearTasksHandler = clearTasksHandler.copyRef()] { });

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

#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            deleteHSTSCacheForHostNames(*networkStorageSession, HSTSCacheHostNames);
    }
#endif

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        for (auto& originData : originDatas)
            CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { originData }, [clearTasksHandler = clearTasksHandler.copyRef()] { });
    }

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

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral())
        clearDiskCacheEntries(cache(), originDatas, [clearTasksHandler = WTFMove(clearTasksHandler)] { });
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
static Vector<String> filterForRegistrableDomains(const Vector<RegistrableDomain>& registrableDomains, const HashSet<String>& foundValues)
{
    Vector<String> result;
    for (const auto& domain : registrableDomains) {
        if (foundValues.contains(domain.string()))
            result.append(domain.string());
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

void NetworkProcess::deleteWebsiteDataForRegistrableDomainsInAllPersistentDataStores(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, Vector<RegistrableDomain>&& domains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
{
    OptionSet<WebsiteDataFetchOption> fetchOptions = WebsiteDataFetchOption::DoNotCreateProcesses;

    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
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

                completionHandler(domains);
            });
        }
        
        CompletionHandler<void(const HashSet<RegistrableDomain>&)> m_completionHandler;
        WebsiteData m_websiteData;
    };
    
    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, completionHandler = WTFMove(completionHandler), shouldNotifyPage] (const HashSet<RegistrableDomain>& domainsWithData) mutable {
        if (shouldNotifyPage)
            parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyWebsiteDataDeletionForRegistrableDomainsFinished(), 0);
        
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), domainsWithData = crossThreadCopy(domainsWithData)] () mutable {
            completionHandler(domainsWithData);
        });
    }));

    auto& websiteDataStore = callbackAggregator->m_websiteData;

    Vector<String> hostnamesWithCookiesToDelete;
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            networkStorageSession->getHostnamesWithCookies(websiteDataStore.hostNamesWithCookies);
            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domains, websiteDataStore.hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete);
        }
    }

    Vector<String> hostnamesWithHSTSToDelete;
#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            getHostNamesWithHSTSCache(*networkStorageSession, websiteDataStore.hostNamesWithHSTSCache);
            hostnamesWithHSTSToDelete = filterForRegistrableDomains(domains, websiteDataStore.hostNamesWithHSTSCache);
            deleteHSTSCacheForHostNames(*networkStorageSession, hostnamesWithHSTSToDelete);
        }
    }
#endif

    /*
    // FIXME: No API to delete credentials by origin
    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (storageSession(sessionID))
            websiteDataStore.originsWithCredentials = storageSession(sessionID)->credentialStorage().originsWithCredentials();
    }
    */
    
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [this, domains, sessionID, callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            
            auto entriesToDelete = filterForRegistrableDomains(domains, entries);

            callbackAggregator->m_websiteData.entries.appendVector(entriesToDelete);
            
            for (auto& entry : entriesToDelete)
                CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { entry.origin }, [callbackAggregator = callbackAggregator.copyRef()] { });
        });
    }
    
#if ENABLE(INDEXED_DATABASE)
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        // FIXME: Pick the right database store based on the session ID.
        postStorageTask(CrossThreadTask([this, sessionID, callbackAggregator = callbackAggregator.copyRef(), path = WTFMove(path), domains]() mutable {
            RunLoop::main().dispatch([this, sessionID, domains = crossThreadCopy(domains), callbackAggregator = callbackAggregator.copyRef(), securityOrigins = indexedDatabaseOrigins(path)] {
                Vector<SecurityOriginData> entriesToDelete;
                for (const auto& securityOrigin : securityOrigins) {
                    if (!domains.contains(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host)))
                        continue;

                    entriesToDelete.append(securityOrigin);
                    callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::IndexedDBDatabases, 0 });
                }

                idbServer(sessionID).closeAndDeleteDatabasesForOrigins(entriesToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
            });
        }));
    }
#endif
    
#if ENABLE(SERVICE_WORKER)
    path = m_swDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([this, sessionID, domains, callbackAggregator = callbackAggregator.copyRef()](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins) {
                if (!domains.contains(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host)))
                    continue;
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
                swServerForSession(sessionID).clear(securityOrigin, [callbackAggregator = callbackAggregator.copyRef()] { });
            }
        });
    }
#endif
    
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        fetchDiskCacheEntries(cache(), sessionID, fetchOptions, [this, domains, callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {

            Vector<SecurityOriginData> entriesToDelete;
            for (auto& entry : entries) {
                if (!domains.contains(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host)))
                    continue;
                entriesToDelete.append(entry.origin);
                callbackAggregator->m_websiteData.entries.append(entry);
            }
            clearDiskCacheEntries(cache(), entriesToDelete, [callbackAggregator = callbackAggregator.copyRef()] { });
        });
    }
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
#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            getHostNamesWithHSTSCache(*networkStorageSession, websiteDataStore.hostNamesWithHSTSCache);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            websiteDataStore.originsWithCredentials = networkStorageSession->credentialStorage().originsWithCredentials();
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
        postStorageTask(CrossThreadTask([this, callbackAggregator = callbackAggregator.copyRef(), path = WTFMove(path)]() mutable {
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
        fetchDiskCacheEntries(cache(), sessionID, fetchOptions, [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
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

    if (m_cache)
        m_cache->setCapacity(urlCacheDiskCapacity);
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

void NetworkProcess::logDiagnosticMessage(uint64_t webPageID, const String& message, const String& description, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessage(webPageID, message, description, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithResult(uint64_t webPageID, const String& message, const String& description, DiagnosticLoggingResultType result, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessageWithResult(webPageID, message, description, result, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithValue(uint64_t webPageID, const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
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

// FIXME: We can remove this one by adapting RefCounter.
class TaskCounter : public RefCounted<TaskCounter> {
public:
    explicit TaskCounter(Function<void()>&& callback) : m_callback(WTFMove(callback)) { }
    ~TaskCounter() { m_callback(); };

private:
    Function<void()> m_callback;
};

void NetworkProcess::actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend shouldAcknowledgeWhenReadyToSuspend)
{
    lowMemoryHandler(Critical::Yes);

    RefPtr<TaskCounter> delayedTaskCounter;
    if (shouldAcknowledgeWhenReadyToSuspend == ShouldAcknowledgeWhenReadyToSuspend::Yes) {
        delayedTaskCounter = adoptRef(new TaskCounter([this] {
            RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::notifyProcessReadyToSuspend() Sending ProcessReadyToSuspend IPC message", this);
            if (parentProcessConnection())
                parentProcessConnection()->send(Messages::NetworkProcessProxy::ProcessReadyToSuspend(), 0);
        }));
    }

    platformPrepareToSuspend([delayedTaskCounter] { });
    platformSyncAllCookies([delayedTaskCounter] { });

    for (auto& connection : m_webProcessConnections)
        connection->cleanupForSuspension([delayedTaskCounter] { });
}

void NetworkProcess::processWillSuspendImminently(CompletionHandler<void(bool)>&& completionHandler)
{
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::No);
    completionHandler(true);
}

void NetworkProcess::prepareToSuspend()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend()", this);
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::Yes);
}

void NetworkProcess::cancelPrepareToSuspend()
{
    // Although it is tempting to send a NetworkProcessProxy::DidCancelProcessSuspension message from here
    // we do not because prepareToSuspend() already replied with a NetworkProcessProxy::ProcessReadyToSuspend
    // message. And NetworkProcessProxy expects to receive either a NetworkProcessProxy::ProcessReadyToSuspend-
    // or NetworkProcessProxy::DidCancelProcessSuspension- message, but not both.
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::cancelPrepareToSuspend()", this);
    platformProcessDidResume();
    for (auto& connection : m_webProcessConnections)
        connection->endSuspension();
}

void NetworkProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processDidResume()", this);
    platformProcessDidResume();
    for (auto& connection : m_webProcessConnections)
        connection->endSuspension();
}

void NetworkProcess::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

void NetworkProcess::cacheStorageParameters(PAL::SessionID sessionID, CacheStorageParametersCallback&& callback)
{
    m_cacheStorageParametersCallbacks.ensure(sessionID, [&] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::RetrieveCacheStorageParameters { sessionID }, 0);
        return Vector<CacheStorageParametersCallback> { };
    }).iterator->value.append(WTFMove(callback));
}

void NetworkProcess::setCacheStorageParameters(PAL::SessionID sessionID, uint64_t quota, String&& cacheStorageDirectory, SandboxExtension::Handle&& handle)
{
    auto iterator = m_cacheStorageParametersCallbacks.find(sessionID);
    if (iterator == m_cacheStorageParametersCallbacks.end())
        return;

    SandboxExtension::consumePermanently(handle);
    auto callbacks = WTFMove(iterator->value);
    m_cacheStorageParametersCallbacks.remove(iterator);
    for (auto& callback : callbacks)
        callback(String { cacheStorageDirectory }, quota);
}

void NetworkProcess::preconnectTo(const URL& url, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
#if ENABLE(SERVER_PRECONNECT)
    NetworkLoadParameters parameters;
    parameters.request = ResourceRequest { url };
    parameters.sessionID = PAL::SessionID::defaultSessionID();
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
IDBServer::IDBServer& NetworkProcess::idbServer(PAL::SessionID sessionID)
{
    auto addResult = m_idbServers.add(sessionID, nullptr);
    if (!addResult.isNewEntry) {
        ASSERT(addResult.iterator->value);
        return *addResult.iterator->value;
    }
    
    auto path = m_idbDatabasePaths.get(sessionID);
    // There should already be a registered path for this PAL::SessionID.
    // If there's not, then where did this PAL::SessionID come from?
    ASSERT(!path.isEmpty());
    
    addResult.iterator->value = IDBServer::IDBServer::create(path, *this, [this](bool isHoldingLockedFiles) {
        notifyHoldingLockedFiles(isHoldingLockedFiles);
    });
    addResult.iterator->value->setPerOriginQuota(m_idbPerOriginQuota);
    return *addResult.iterator->value;
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

HashSet<WebCore::SecurityOriginData> NetworkProcess::indexedDatabaseOrigins(const String& path)
{
    if (path.isEmpty())
        return { };
    
    HashSet<WebCore::SecurityOriginData> securityOrigins;
    for (auto& topOriginPath : FileSystem::listDirectory(path, "*")) {
        auto databaseIdentifier = FileSystem::pathGetFileName(topOriginPath);
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            securityOrigins.add(WTFMove(*securityOrigin));
        
        for (auto& originPath : FileSystem::listDirectory(topOriginPath, "*")) {
            databaseIdentifier = FileSystem::pathGetFileName(originPath);
            if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
                securityOrigins.add(WTFMove(*securityOrigin));
        }
    }

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

void NetworkProcess::setIDBPerOriginQuota(uint64_t quota)
{
    m_idbPerOriginQuota = quota;
    
    for (auto& server : m_idbServers.values())
        server->setPerOriginQuota(quota);
}
#endif // ENABLE(INDEXED_DATABASE)

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
    auto& securityOrigin = serverToContextConnection->securityOrigin();
    
    serverToContextConnection->connectionClosed();
    m_serverToContextConnections.remove(securityOrigin);
    
    for (auto& swServer : m_swServers.values())
        swServer->markAllWorkersForOriginAsTerminated(securityOrigin);
    
    if (needsServerToContextConnectionForOrigin(securityOrigin)) {
        RELEASE_LOG(ServiceWorker, "Connection to service worker process was closed but is still needed, relaunching it");
        createServerToContextConnection(securityOrigin, WTF::nullopt);
    }
}

bool NetworkProcess::needsServerToContextConnectionForOrigin(const SecurityOriginData& securityOrigin) const
{
    return WTF::anyOf(m_swServers.values(), [&](auto& swServer) {
        return swServer->needsServerToContextConnectionForOrigin(securityOrigin);
    });
}

SWServer& NetworkProcess::swServerForSession(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    
    auto result = m_swServers.ensure(sessionID, [&] {
        auto path = m_swDatabasePaths.get(sessionID);
        // There should already be a registered path for this PAL::SessionID.
        // If there's not, then where did this PAL::SessionID come from?
        ASSERT(sessionID.isEphemeral() || !path.isEmpty());
        
        auto value = std::make_unique<SWServer>(makeUniqueRef<WebSWOriginStore>(), WTFMove(path), sessionID);
        if (m_shouldDisableServiceWorkerProcessTerminationDelay)
            value->disableServiceWorkerProcessTerminationDelay();
        return value;
    });

    return *result.iterator->value;
}

WebSWOriginStore& NetworkProcess::swOriginStoreForSession(PAL::SessionID sessionID)
{
    return static_cast<WebSWOriginStore&>(swServerForSession(sessionID).originStore());
}

WebSWOriginStore* NetworkProcess::existingSWOriginStoreForSession(PAL::SessionID sessionID) const
{
    auto* swServer = m_swServers.get(sessionID);
    if (!swServer)
        return nullptr;
    return &static_cast<WebSWOriginStore&>(swServer->originStore());
}

WebSWServerToContextConnection* NetworkProcess::serverToContextConnectionForOrigin(const SecurityOriginData& securityOrigin)
{
    return m_serverToContextConnections.get(securityOrigin);
}

void NetworkProcess::createServerToContextConnection(const SecurityOriginData& securityOrigin, Optional<PAL::SessionID> sessionID)
{
    if (m_waitingForServerToContextProcessConnection)
        return;
    
    m_waitingForServerToContextProcessConnection = true;
    if (sessionID)
        parentProcessConnection()->send(Messages::NetworkProcessProxy::EstablishWorkerContextConnectionToNetworkProcessForExplicitSession(securityOrigin, *sessionID), 0);
    else
        parentProcessConnection()->send(Messages::NetworkProcessProxy::EstablishWorkerContextConnectionToNetworkProcess(securityOrigin), 0);
}

void NetworkProcess::didFailFetch(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const ResourceError& error)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFailFetch(fetchIdentifier, error);
}

void NetworkProcess::didNotHandleFetch(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didNotHandleFetch(fetchIdentifier);
}

void NetworkProcess::didReceiveFetchRedirectResponse(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const WebCore::ResourceResponse& response)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchRedirectResponse(fetchIdentifier, response);
}

void NetworkProcess::didReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const WebCore::ResourceResponse& response, bool needsContinueDidReceiveResponseMessage)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchResponse(fetchIdentifier, response, needsContinueDidReceiveResponseMessage);
}

void NetworkProcess::didReceiveFetchData(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const IPC::DataReference& data, int64_t encodedDataLength)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchData(fetchIdentifier, data, encodedDataLength);
}

void NetworkProcess::didReceiveFetchFormData(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const IPC::FormDataReference& formData)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchFormData(fetchIdentifier, formData);
}

void NetworkProcess::didFinishFetch(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didFinishFetch(fetchIdentifier);
}

void NetworkProcess::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, MessageWithMessagePorts&& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    if (auto* connection = m_swServerConnections.get(destinationIdentifier.serverConnectionIdentifier))
        connection->postMessageToServiceWorkerClient(destinationIdentifier.contextIdentifier, WTFMove(message), sourceIdentifier, sourceOrigin);
}

void NetworkProcess::postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&& message, const WebCore::ServiceWorkerOrClientIdentifier& source, SWServerConnectionIdentifier connectionIdentifier)
{
    if (auto* connection = m_swServerConnections.get(connectionIdentifier))
        connection->postMessageToServiceWorker(destination, WTFMove(message), source);
}

void NetworkProcess::registerSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(parentProcessHasServiceWorkerEntitlement());
    ASSERT(!m_swServerConnections.contains(connection.identifier()));
    m_swServerConnections.add(connection.identifier(), &connection);
    swOriginStoreForSession(connection.sessionID()).registerSWServerConnection(connection);
}

void NetworkProcess::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    ASSERT(m_swServerConnections.get(connection.identifier()) == &connection);
    m_swServerConnections.remove(connection.identifier());
    if (auto* store = existingSWOriginStoreForSession(connection.sessionID()))
        store->unregisterSWServerConnection(connection);
}

void NetworkProcess::swContextConnectionMayNoLongerBeNeeded(WebSWServerToContextConnection& serverToContextConnection)
{
    auto& securityOrigin = serverToContextConnection.securityOrigin();
    if (needsServerToContextConnectionForOrigin(securityOrigin))
        return;
    
    RELEASE_LOG(ServiceWorker, "Service worker process is no longer needed, terminating it");
    serverToContextConnection.terminate();
    
    for (auto& swServer : m_swServers.values())
        swServer->markAllWorkersForOriginAsTerminated(securityOrigin);
    
    serverToContextConnection.connectionClosed();
    m_serverToContextConnections.remove(securityOrigin);
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

void NetworkProcess::requestCacheStorageSpace(PAL::SessionID sessionID, const ClientOrigin& origin, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(Optional<uint64_t>)>&& callback)
{
    parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestCacheStorageSpace { sessionID, origin, quota, currentSize, spaceRequired }, WTFMove(callback), 0);
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
    if (auto session = networkSession(sessionID))
        session->storeAdClickAttribution(WTFMove(adClickAttribution));
}

void NetworkProcess::dumpAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (auto session = networkSession(sessionID))
        return session->dumpAdClickAttribution(WTFMove(completionHandler));

    completionHandler({ });
}

void NetworkProcess::clearAdClickAttribution(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto session = networkSession(sessionID))
        return session->clearAdClickAttribution(WTFMove(completionHandler));
    
    completionHandler();
}

void NetworkProcess::notifyHoldingLockedFiles(bool isIDBDatabaseHoldingLockedFiles)
{
    parentProcessConnection()->send(Messages::NetworkProcessProxy::SetIsIDBDatabaseHoldingLockedFiles(isIDBDatabaseHoldingLockedFiles), 0);
}

} // namespace WebKit
