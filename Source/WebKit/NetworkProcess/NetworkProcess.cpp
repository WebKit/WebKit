/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "PreconnectTask.h"
#include "RemoteNetworkingContext.h"
#include "ShouldGrandfatherStatistics.h"
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
#include <WebCore/ClientOrigin.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DNS.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStateNotifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageQuotaManager.h>
#include <WebCore/UserContentURLPattern.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/AtomString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#include "NetworkCache.h"
#include "NetworkCacheCoders.h"

#if PLATFORM(COCOA)
#include "LaunchServicesDatabaseObserver.h"
#include "NetworkSessionCocoa.h"
#endif

#if USE(SOUP)
#include "NetworkSessionSoup.h"
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
#if OS(WINDOWS)
        // Calling _exit in non-main threads may cause a deadlock in WTF::Thread::ThreadHolder::~ThreadHolder.
        TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
#else
        _exit(EXIT_FAILURE);
#endif
    });
}

static inline MessagePortChannelRegistry createMessagePortChannelRegistry(NetworkProcess& networkProcess)
{
    return MessagePortChannelRegistry { [&networkProcess](auto& messagePortIdentifier, auto processIdentifier, auto&& completionHandler) {
        auto* connection = networkProcess.webProcessConnection(processIdentifier);
        if (!connection) {
            completionHandler(MessagePortChannelProvider::HasActivity::No);
            return;
        }

        connection->checkProcessLocalPortForActivity(messagePortIdentifier, WTFMove(completionHandler));
    } };
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
    , m_messagePortChannelRegistry(createMessagePortChannelRegistry(*this))
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    addSupplement<LegacyCustomProtocolManager>();
#endif
#if HAVE(LSDATABASECONTEXT)
    addSupplement<LaunchServicesDatabaseObserver>();
#endif
#if PLATFORM(COCOA) && ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    LegacyCustomProtocolManager::networkProcessCreated(*this);
#endif

    NetworkStateNotifier::singleton().addListener([weakThis = makeWeakPtr(*this)](bool isOnLine) {
        if (!weakThis)
            return;
        for (auto& webProcessConnection : weakThis->m_webProcessConnections.values())
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
    ASSERT(m_webProcessConnections.contains(connection.webProcessIdentifier()));
    m_webProcessConnections.remove(connection.webProcessIdentifier());
}

bool NetworkProcess::shouldTerminate()
{
    // Network process keeps session cookies and credentials, so it should never terminate (as long as UI process connection is alive).
    return false;
}

void NetworkProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(parentProcessConnection() == &connection);
    if (parentProcessConnection() != &connection) {
        WTFLogAlways("Ignored message '%s' because it did not come from the UIProcess (destination=%" PRIu64 ")", description(decoder.messageName()), decoder.destinationID());
        ASSERT_NOT_REACHED();
        return;
    }

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

    didReceiveNetworkProcessMessage(connection, decoder);
}

bool NetworkProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    ASSERT(parentProcessConnection() == &connection);
    if (parentProcessConnection() != &connection) {
        WTFLogAlways("Ignored message '%s' because it did not come from the UIProcess (destination=%" PRIu64 ")", description(decoder.messageName()), decoder.destinationID());
        ASSERT_NOT_REACHED();
        return false;
    }

    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;

    return didReceiveSyncNetworkProcessMessage(connection, decoder, replyEncoder);
}

void NetworkProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([this] {
        ASSERT(RunLoop::isMain());
        stopRunLoop();
    });

    // Make sure we flush all cookies to disk before exiting.
    forEachNetworkSession([&] (auto& networkSession) {
        platformFlushCookies(networkSession.sessionID(), [callbackAggregator] { });
    });

    // Make sure references to NetworkProcess in spaceRequester and closeHandler is removed.
    auto servers = std::exchange(m_webIDBServers, { });
    for (auto& server : servers.values())
        server->close();
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

    m_storageManagerSet->handleLowMemoryWarning();

#if ENABLE(SERVICE_WORKER)
    for (auto& swServer : m_swServers.values())
        swServer->handleLowMemoryWarning();
#endif
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

    setCacheModel(parameters.cacheModel);

    setPrivateClickMeasurementEnabled(parameters.enablePrivateClickMeasurement);
    setPrivateClickMeasurementDebugMode(parameters.enablePrivateClickMeasurementDebugMode);

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
    
    for (auto&& websiteDataStoreParameters : WTFMove(parameters.websiteDataStoreParameters))
        addWebsiteDataStore(WTFMove(websiteDataStoreParameters));

    RELEASE_LOG(Process, "%p - NetworkProcess::initializeNetworkProcess: Presenting processPID=%d", this, WebCore::presentingApplicationPID());
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

void NetworkProcess::createNetworkConnectionToWebProcess(ProcessIdentifier identifier, PAL::SessionID sessionID, CompletionHandler<void(Optional<IPC::Attachment>&&, HTTPCookieAcceptPolicy)>&& completionHandler)
{
    auto ipcConnection = createIPCConnectionPair();
    if (!ipcConnection) {
        completionHandler({ }, HTTPCookieAcceptPolicy::Never);
        return;
    }

    auto newConnection = NetworkConnectionToWebProcess::create(*this, identifier, sessionID, ipcConnection->first);
    auto& connection = newConnection.get();

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    auto* storage = storageSession(sessionID);
    completionHandler(WTFMove(ipcConnection->second), storage ? storage->cookieAcceptPolicy() : HTTPCookieAcceptPolicy::Never);

    connection.setOnLineState(NetworkStateNotifier::singleton().onLine());

    m_storageManagerSet->addConnection(connection.connection());
}

void NetworkProcess::clearCachedCredentials(PAL::SessionID sessionID)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* storageSession = networkSession->networkStorageSession())
            storageSession->credentialStorage().clearCredentials();
        networkSession->clearCredentials();
    }
}

void NetworkProcess::addWebsiteDataStore(WebsiteDataStoreParameters&& parameters)
{
    auto sessionID = parameters.networkSessionParameters.sessionID;

    addSessionStorageQuotaManager(sessionID, parameters.perOriginStorageQuota, parameters.perThirdPartyOriginStorageQuota, parameters.cacheStorageDirectory, parameters.cacheStorageDirectoryExtensionHandle);

    addIndexedDatabaseSession(sessionID, parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);

#if ENABLE(SERVICE_WORKER)
    addServiceWorkerSession(sessionID, parameters.serviceWorkerProcessTerminationDelayEnabled, WTFMove(parameters.serviceWorkerRegistrationDirectory), parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
#endif

    m_storageManagerSet->add(sessionID, parameters.localStorageDirectory, parameters.localStorageDirectoryExtensionHandle);

    RemoteNetworkingContext::ensureWebsiteDataStoreSession(*this, WTFMove(parameters));
}

void NetworkProcess::addSessionStorageQuotaManager(PAL::SessionID sessionID, uint64_t defaultQuota, uint64_t defaultThirdPartyQuota, const String& cacheRootPath, SandboxExtension::Handle& cacheRootPathHandle)
{
    LockHolder locker(m_sessionStorageQuotaManagersLock);
    auto isNewEntry = m_sessionStorageQuotaManagers.ensure(sessionID, [defaultQuota, defaultThirdPartyQuota, &cacheRootPath] {
        return makeUnique<SessionStorageQuotaManager>(cacheRootPath, defaultQuota, defaultThirdPartyQuota);
    }).isNewEntry;
    if (isNewEntry)
        SandboxExtension::consumePermanently(cacheRootPathHandle);
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::removeSessionStorageQuotaManager(PAL::SessionID sessionID)
{
    LockHolder locker(m_sessionStorageQuotaManagersLock);
    ASSERT(m_sessionStorageQuotaManagers.contains(sessionID));
    m_sessionStorageQuotaManagers.remove(sessionID);
}

void NetworkProcess::forEachNetworkSession(const Function<void(NetworkSession&)>& functor)
{
    for (auto& session : m_networkSessions.values())
        functor(*session);
}

std::unique_ptr<WebCore::NetworkStorageSession> NetworkProcess::newTestingSession(const PAL::SessionID& sessionID)
{
#if PLATFORM(COCOA)
    // Session name should be short enough for shared memory region name to be under the limit, otherwise sandbox rules won't work (see <rdar://problem/13642852>).
    String sessionName = makeString("WebKit Test-", getCurrentProcessID());

    auto session = WebCore::createPrivateStorageSession(sessionName.createCFString().get());

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (WebCore::NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (session)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, session.get()));
    }

    return makeUnique<WebCore::NetworkStorageSession>(sessionID, WTFMove(session), WTFMove(cookieStorage));
#elif USE(CURL) || USE(SOUP)
    return makeUnique<WebCore::NetworkStorageSession>(sessionID);
#endif
}

#if PLATFORM(COCOA)
void NetworkProcess::ensureSession(const PAL::SessionID& sessionID, bool shouldUseTestingNetworkSession, const String& identifierBase, RetainPtr<CFHTTPCookieStorageRef>&& cookieStorage)
#else
void NetworkProcess::ensureSession(const PAL::SessionID& sessionID, bool shouldUseTestingNetworkSession, const String& identifierBase)
#endif
{
    auto addResult = m_networkStorageSessions.add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

    if (shouldUseTestingNetworkSession) {
        addResult.iterator->value = newTestingSession(sessionID);
        return;
    }
    
#if PLATFORM(COCOA)
    RetainPtr<CFURLStorageSessionRef> storageSession;
    RetainPtr<CFStringRef> cfIdentifier = makeString(identifierBase, ".PrivateBrowsing.", createCanonicalUUIDString()).createCFString();
    if (sessionID.isEphemeral())
        storageSession = createPrivateStorageSession(cfIdentifier.get());
    else if (sessionID != PAL::SessionID::defaultSessionID())
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

void NetworkProcess::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    for (auto& connection : m_webProcessConnections.values())
        connection->cookieAcceptPolicyChanged(newPolicy);
}

WebCore::NetworkStorageSession* NetworkProcess::storageSession(const PAL::SessionID& sessionID) const
{
    return m_networkStorageSessions.get(sessionID);
}

void NetworkProcess::forEachNetworkStorageSession(const Function<void(WebCore::NetworkStorageSession&)>& functor)
{
    for (auto& storageSession : m_networkStorageSessions.values())
        functor(*storageSession);
}

NetworkSession* NetworkProcess::networkSession(PAL::SessionID sessionID) const
{
    ASSERT(RunLoop::isMain());
    return m_networkSessions.get(sessionID);
}

void NetworkProcess::setSession(PAL::SessionID sessionID, std::unique_ptr<NetworkSession>&& session)
{
    ASSERT(RunLoop::isMain());
    m_networkSessions.set(sessionID, WTFMove(session));
}

void NetworkProcess::destroySession(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());
#if !USE(SOUP) && !USE(CURL)
    // cURL and Soup based ports destroy the default session right before the process exits to avoid leaking
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
    m_serviceWorkerInfo.remove(sessionID);
#endif

    m_storageManagerSet->remove(sessionID);
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

void NetworkProcess::getResourceLoadStatisticsDataSummary(PAL::SessionID sessionID, CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->aggregatedThirdPartyData(WTFMove(completionHandler));
        else
            completionHandler({ });
    } else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
    }
}

void NetworkProcess::resetParametersToDefaultValues(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        networkSession->resetCNAMEDomainData();
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

void NetworkProcess::statisticsDatabaseHasAllTables(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->statisticsDatabaseHasAllTables(WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::statisticsDatabaseColumnsForTable(PAL::SessionID sessionID, const String& tableName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->statisticsDatabaseColumnsForTable(tableName, WTFMove(completionHandler));
        else
            completionHandler({ });
    } else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
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

void NetworkProcess::domainIDExistsInDatabase(PAL::SessionID sessionID, int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->domainIDExistsInDatabase(domainID, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::mergeStatisticForTesting(PAL::SessionID sessionID, const RegistrableDomain& domain, const RegistrableDomain& topFrameDomain1, const RegistrableDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->mergeStatisticForTesting(domain, topFrameDomain1, topFrameDomain2, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::insertExpiredStatisticForTesting(PAL::SessionID sessionID, const RegistrableDomain& domain, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->insertExpiredStatisticForTesting(domain, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, WTFMove(completionHandler));
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

void NetworkProcess::logFrameNavigation(PAL::SessionID sessionID, const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
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

void NetworkProcess::isRelationshipOnlyInDatabaseOnce(PAL::SessionID sessionID, const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->isRelationshipOnlyInDatabaseOnce(subDomain, topDomain, WTFMove(completionHandler));
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

void NetworkProcess::setResourceLoadStatisticsEnabled(PAL::SessionID sessionID, bool enabled)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->setResourceLoadStatisticsEnabled(enabled);
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

void NetworkProcess::isResourceLoadStatisticsEphemeral(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler) const
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            completionHandler(resourceLoadStatistics->isEphemeral());
            return;
        }
    } else
        ASSERT_NOT_REACHED();
    completionHandler(false);
}

void NetworkProcess::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->resetCacheMaxAgeCapForPrevalentResources();
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID)
{
    ASSERT(!navigationDataTransfer.isEmpty());

    if (auto* networkStorageSession = storageSession(sessionID)) {
        if (!networkStorageSession->shouldBlockThirdPartyCookies(fromDomain))
            return;

        if (navigationDataTransfer.contains(CrossSiteNavigationDataTransfer::Flag::DestinationLinkDecoration))
            networkStorageSession->didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(toDomain, webPageID);

        if (navigationDataTransfer.contains(CrossSiteNavigationDataTransfer::Flag::ReferrerLinkDecoration))
            parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCommitCrossSiteLoadWithDataTransferFromPrevalentResource(webPageProxyID), 0);
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

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcess::setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID sessionID, HashSet<WebCore::RegistrableDomain>&& appBoundDomains, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->setAppBoundDomains(WTFMove(appBoundDomains), WTFMove(completionHandler));
            return;
        }
    }
    ASSERT_NOT_REACHED();
    completionHandler();
}
#endif

void NetworkProcess::setShouldDowngradeReferrerForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    forEachNetworkSession([enabled](auto& networkSession) {
        networkSession.setShouldDowngradeReferrerForTesting(enabled);
    });
    completionHandler();
}

void NetworkProcess::setThirdPartyCookieBlockingMode(PAL::SessionID sessionID, WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->setThirdPartyCookieBlockingMode(blockingMode);
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID sessionID, WebCore::SameSiteStrictEnforcementEnabled enabled, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->setShouldEnbleSameSiteStrictEnforcement(enabled);
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID sessionID, WebCore::FirstPartyWebsiteDataRemovalMode mode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->setFirstPartyWebsiteDataRemovalMode(mode, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setToSameSiteStrictCookiesForTesting(PAL::SessionID sessionID, const WebCore::RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkStorageSession = storageSession(sessionID))
        networkStorageSession->setAllCookiesToSameSiteStrict(domain, WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setFirstPartyHostCNAMEDomainForTesting(PAL::SessionID sessionID, String&& firstPartyHost, WebCore::RegistrableDomain&& cnameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->setFirstPartyHostCNAMEDomain(WTFMove(firstPartyHost), WTFMove(cnameDomain));
    completionHandler();
}

void NetworkProcess::setThirdPartyCNAMEDomainForTesting(PAL::SessionID sessionID, WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->setThirdPartyCNAMEDomainForTesting(WTFMove(domain));
    completionHandler();
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void NetworkProcess::setPrivateClickMeasurementEnabled(bool enabled)
{
    m_privateClickMeasurementEnabled = enabled;
}

bool NetworkProcess::privateClickMeasurementEnabled() const
{
    return m_privateClickMeasurementEnabled;
}

void NetworkProcess::setPrivateClickMeasurementDebugMode(bool enabled)
{
    if (m_privateClickMeasurementDebugModeEnabled == enabled)
        return;

    m_privateClickMeasurementDebugModeEnabled = enabled;

    String message = enabled ? "[Private Click Measurement] Turned Debug Mode on."_s : "[Private Click Measurement] Turned Debug Mode off."_s;
    for (auto& networkConnectionToWebProcess : m_webProcessConnections.values()) {
        if (networkConnectionToWebProcess->sessionID().isEphemeral())
            continue;
        networkConnectionToWebProcess->broadcastConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Info, message);
    }
}

bool NetworkProcess::privateClickMeasurementDebugModeEnabled() const
{
    return m_privateClickMeasurementDebugModeEnabled;
}

void NetworkProcess::preconnectTo(PAL::SessionID sessionID, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID, const URL& url, const String& userAgent, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, LastNavigationWasAppBound lastNavigationWasAppBound)
{
    LOG(Network, "(NetworkProcess) Preconnecting to URL %s (storedCredentialsPolicy %i)", url.string().utf8().data(), (int)storedCredentialsPolicy);

#if ENABLE(SERVER_PRECONNECT)
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (supplement<LegacyCustomProtocolManager>()->supportsScheme(url.protocol().toString()))
        return;
#endif

    auto* networkSession = this->networkSession(sessionID);
    if (!networkSession)
        return;

    NetworkLoadParameters parameters;
    parameters.request = ResourceRequest { url };
    parameters.request.setIsAppBound(lastNavigationWasAppBound == LastNavigationWasAppBound::Yes);
    parameters.request.setFirstPartyForCookies(url);
    parameters.webPageProxyID = webPageProxyID;
    parameters.webPageID = webPageID;
    parameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    if (!userAgent.isEmpty()) {
        // FIXME: we add user-agent to the preconnect request because otherwise the preconnect
        // gets thrown away by CFNetwork when using an HTTPS proxy (<rdar://problem/59434166>).
        parameters.request.setHTTPUserAgent(userAgent);
    }
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;

    (new PreconnectTask(*networkSession, WTFMove(parameters), [](const WebCore::ResourceError&) { }))->start();
#else
    UNUSED_PARAM(url);
    UNUSED_PARAM(userAgent);
    UNUSED_PARAM(storedCredentialsPolicy);
#endif
}

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

void NetworkProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, CompletionHandler<void(WebsiteData&&)>&& completionHandler)
{
    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(WebsiteData&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), websiteData = WTFMove(m_websiteData)] () mutable {
                completionHandler(WTFMove(websiteData));
            });
        }

        CompletionHandler<void(WebsiteData&&)> m_completionHandler;
        WebsiteData m_websiteData;
    };

    auto callbackAggregator = adoptRef(*new CallbackAggregator(WTFMove(completionHandler)));

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
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID)) {
        m_storageManagerSet->getSessionStorageOrigins(sessionID, [callbackAggregator](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::SessionStorage, 0 });
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID)) {
        m_storageManagerSet->getLocalStorageOrigins(sessionID, [callbackAggregator](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::LocalStorage, 0 });
        });
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        callbackAggregator->m_websiteData.hostNamesWithHSTSCache = hostNamesWithHSTSCache(sessionID);
#endif

    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        webIDBServer(sessionID).getOrigins([callbackAggregator](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append({ origins.takeAny(), WebsiteDataType::IndexedDBDatabases, 0 });
        });
    }

#if ENABLE(SERVICE_WORKER)
    path = m_serviceWorkerInfo.get(sessionID).databasePath;
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([callbackAggregator](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& origin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
#endif
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [callbackAggregator](auto entries) mutable {
                callbackAggregator->m_websiteData.entries.appendVector(entries);
            });
        });
    }

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices)) {
        if (auto* session = networkSession(sessionID)) {
            for (auto& origin : session->hostNamesWithAlternativeServices())
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::AlternativeServices, 0 });
        }
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (auto* session = networkSession(sessionID)) {
            if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
                resourceLoadStatistics->registrableDomains([callbackAggregator](auto&& domains) mutable {
                    while (!domains.isEmpty())
                        callbackAggregator->m_websiteData.registrableDomainsWithResourceLoadStatistics.add(domains.takeLast());
                });
            }
        }
    }
#endif
}

void NetworkProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        clearHSTSCache(sessionID, modifiedSince);
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

    auto clearTasksHandler = WTF::CallbackAggregator::create(WTFMove(completionHandler));

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        CacheStorage::Engine::clearAllCaches(*this, sessionID, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteSessionStorage(sessionID, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteLocalStorageModifiedSince(sessionID, modifiedSince, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !sessionID.isEphemeral())
        webIDBServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, [clearTasksHandler] { });

#if ENABLE(SERVICE_WORKER)
    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !sessionID.isEphemeral())
        swServerForSession(sessionID).clearAll([clearTasksHandler] { });
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
                
                resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince, shouldGrandfather, [clearTasksHandler] { });
            }
        }
    }
#endif

    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->removeNetworkWebsiteData(modifiedSince, WTF::nullopt, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral())
        clearDiskCache(modifiedSince, [clearTasksHandler = WTFMove(clearTasksHandler)] { });

    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements)) {
        if (auto* networkSession = this->networkSession(sessionID))
            networkSession->clearPrivateClickMeasurement();
    }

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices)) {
        if (auto* networkSession = this->networkSession(sessionID))
            networkSession->clearAlternativeServices(modifiedSince);
    }
#endif
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

void NetworkProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& originDatas, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, const Vector<RegistrableDomain>& registrableDomains, CompletionHandler<void()>&& completionHandler)
{
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteCookiesForHostnames(cookieHostNames);
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        deleteHSTSCacheForHostNames(sessionID, HSTSCacheHostNames);
#endif

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            Vector<String> hosts;
            hosts.reserveInitialCapacity(originDatas.size());
            for (auto& origin : originDatas)
                hosts.uncheckedAppend(origin.host);
            networkSession->deleteAlternativeServicesForHostNames(hosts);
        }
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            for (auto& originData : originDatas)
                networkSession->clearPrivateClickMeasurementForRegistrableDomain(RegistrableDomain::uncheckedCreateFromHost(originData.host));
        }
    }
    
    auto clearTasksHandler = WTF::CallbackAggregator::create(WTFMove(completionHandler));

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        for (auto& originData : originDatas)
            CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { originData }, [clearTasksHandler] { });
    }

    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteSessionStorageForOrigins(sessionID, originDatas, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManagerSet->contains(sessionID))
        m_storageManagerSet->deleteLocalStorageForOrigins(sessionID, originDatas, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !sessionID.isEphemeral())
        webIDBServer(sessionID).closeAndDeleteDatabasesForOrigins(originDatas, [clearTasksHandler] { });

#if ENABLE(SERVICE_WORKER)
    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !sessionID.isEphemeral()) {
        auto& server = swServerForSession(sessionID);
        for (auto& originData : originDatas)
            server.clear(originData, [clearTasksHandler] { });
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral()) {
        forEachNetworkSession([originDatas, &clearTasksHandler](auto& session) {
            clearDiskCacheEntries(session.cache(), originDatas, [clearTasksHandler] { });
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID)) {
            for (auto& originData : originDatas)
                session->credentialStorage().removeCredentialsWithOrigin(originData);
        }
        WebCore::CredentialStorage::removeSessionCredentialsWithOrigins(originDatas);
    }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            for (auto& domain : registrableDomains) {
                if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
                    resourceLoadStatistics->removeDataForDomain(domain, [clearTasksHandler] { });
            }
        }
    }
#endif

    if (auto* networkSession = this->networkSession(sessionID)) {
        HashSet<WebCore::RegistrableDomain> domainsToDeleteNetworkDataFor;
        for (auto& originData : originDatas)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(originData.host));
        for (auto& cookieHostName : cookieHostNames)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(cookieHostName));
        for (auto& cacheHostName : HSTSCacheHostNames)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(cacheHostName));
        for (auto& domain : registrableDomains)
            domainsToDeleteNetworkDataFor.add(domain);

        networkSession->removeNetworkWebsiteData(WTF::nullopt, WTFMove(domainsToDeleteNetworkDataFor), [clearTasksHandler] { });
    }
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

void NetworkProcess::deleteAndRestrictWebsiteDataForRegistrableDomains(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
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
    Vector<String> hostnamesWithCookiesToDelete;
    auto domainsToDeleteAllNonCookieWebsiteDataFor = domains.domainsToDeleteAllNonCookieWebsiteDataFor;
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            networkStorageSession->getHostnamesWithCookies(hostNamesWithCookies);

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domains.domainsToDeleteAllCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::Yes);

            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domains.domainsToDeleteAllButHttpOnlyCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::No);

            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));
        }
    }

    Vector<String> hostnamesWithHSTSToDelete;
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        hostNamesWithHSTSCache = this->hostNamesWithHSTSCache(sessionID);
        hostnamesWithHSTSToDelete = filterForRegistrableDomains(domainsToDeleteAllNonCookieWebsiteDataFor, hostNamesWithHSTSCache);

        for (const auto& host : hostnamesWithHSTSToDelete)
            callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

        deleteHSTSCacheForHostNames(sessionID, hostnamesWithHSTSToDelete);
    }
#endif

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices)) {
        if (auto* networkSession = this->networkSession(sessionID)) {
            Vector<String> registrableDomainsToDelete;
            registrableDomainsToDelete.reserveInitialCapacity(domainsToDeleteAllNonCookieWebsiteDataFor.size());
            for (auto& domain : domainsToDeleteAllNonCookieWebsiteDataFor)
                registrableDomainsToDelete.uncheckedAppend(domain.string());
            networkSession->deleteAlternativeServicesForHostNames(registrableDomainsToDelete);
        }
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID)) {
            auto origins = session->credentialStorage().originsWithCredentials();
            auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator->m_domains);
            for (auto& origin : originsToDelete)
                session->credentialStorage().removeCredentialsWithOrigin(origin);
        }

        auto origins = WebCore::CredentialStorage::originsWithSessionCredentials();
        auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator->m_domains);
        WebCore::CredentialStorage::removeSessionCredentialsWithOrigins(originsToDelete);
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [this, domainsToDeleteAllNonCookieWebsiteDataFor, sessionID, callbackAggregator](auto entries) mutable {
            
            auto entriesToDelete = filterForRegistrableDomains(domainsToDeleteAllNonCookieWebsiteDataFor, entries);

            for (const auto& entry : entriesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host));

            for (auto& entry : entriesToDelete)
                CacheStorage::Engine::clearCachesForOrigin(*this, sessionID, SecurityOriginData { entry.origin }, [callbackAggregator] { });
        });
    }

    if (m_storageManagerSet->contains(sessionID)) {
        if (websiteDataTypes.contains(WebsiteDataType::SessionStorage)) {
            m_storageManagerSet->getSessionStorageOrigins(sessionID, [protectedThis = makeRef(*this), this, sessionID, callbackAggregator, domainsToDeleteAllNonCookieWebsiteDataFor](auto&& origins) {
                if (!m_storageManagerSet->contains(sessionID))
                    return;

                auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator->m_domains);
                m_storageManagerSet->deleteSessionStorageForOrigins(sessionID, originsToDelete, [callbackAggregator] { });
            });
        }

        if (websiteDataTypes.contains(WebsiteDataType::LocalStorage)) {
            m_storageManagerSet->getLocalStorageOrigins(sessionID, [protectedThis = makeRef(*this), this, sessionID, callbackAggregator, domainsToDeleteAllNonCookieWebsiteDataFor](auto&& origins) {
                if (!m_storageManagerSet->contains(sessionID))
                    return;

                auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator->m_domains);
                m_storageManagerSet->deleteLocalStorageForOrigins(sessionID, originsToDelete, [callbackAggregator] { });
            });
        }
    }

    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        webIDBServer(sessionID).getOrigins([this, protectedThis = makeRef(*this), sessionID, callbackAggregator, domainsToDeleteAllNonCookieWebsiteDataFor](auto&& origins) {
            if (!m_webIDBServers.contains(sessionID))
                return;

            auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator->m_domains);
            webIDBServer(sessionID).closeAndDeleteDatabasesForOrigins(originsToDelete, [callbackAggregator] { });
        });
    }
    
#if ENABLE(SERVICE_WORKER)
    path = m_serviceWorkerInfo.get(sessionID).databasePath;
    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !path.isEmpty()) {
        swServerForSession(sessionID).getOriginsWithRegistrations([this, sessionID, domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins) {
                if (!domainsToDeleteAllNonCookieWebsiteDataFor.contains(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host)))
                    continue;
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host));
                swServerForSession(sessionID).clear(securityOrigin, [callbackAggregator] { });
            }
        });
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &domainsToDeleteAllNonCookieWebsiteDataFor, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [domainsToDeleteAllNonCookieWebsiteDataFor, callbackAggregator, session = makeWeakPtr(&session)](auto entries) mutable {
                if (!session)
                    return;

                Vector<SecurityOriginData> entriesToDelete;
                for (auto& entry : entries) {
                    if (!domainsToDeleteAllNonCookieWebsiteDataFor.contains(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host)))
                        continue;
                    entriesToDelete.append(entry.origin);
                    callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host));
                }
                clearDiskCacheEntries(session->cache(), entriesToDelete, [callbackAggregator] { });
            });
        });
    }

    auto dataTypesForUIProcess = WebsiteData::filter(websiteDataTypes, WebsiteDataProcessType::UI);
    if (!dataTypesForUIProcess.isEmpty() && !domainsToDeleteAllNonCookieWebsiteDataFor.isEmpty()) {
        CompletionHandler<void(const HashSet<RegistrableDomain>&)> completionHandler = [callbackAggregator] (const HashSet<RegistrableDomain>& domains) {
            for (auto& domain : domains)
                callbackAggregator->m_domains.add(domain);
        };
        parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::DeleteWebsiteDataInUIProcessForRegistrableDomains(sessionID, dataTypesForUIProcess, fetchOptions, domainsToDeleteAllNonCookieWebsiteDataFor), WTFMove(completionHandler));
    }
}

void NetworkProcess::deleteCookiesForTesting(PAL::SessionID sessionID, RegistrableDomain domain, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    OptionSet<WebsiteDataType> cookieType = WebsiteDataType::Cookies;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor toDeleteFor;
    if (includeHttpOnlyCookies)
        toDeleteFor.domainsToDeleteAllCookiesFor.append(domain);
    else
        toDeleteFor.domainsToDeleteAllButHttpOnlyCookiesFor.append(domain);

    deleteAndRestrictWebsiteDataForRegistrableDomains(sessionID, cookieType, WTFMove(toDeleteFor), true, [completionHandler = WTFMove(completionHandler)] (const HashSet<RegistrableDomain>& domainsDeletedFor) mutable {
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
    
    auto& websiteData = callbackAggregator->m_websiteData;
    
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->getHostnamesWithCookies(websiteData.hostNamesWithCookies);
    }
    
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        websiteData.hostNamesWithHSTSCache = hostNamesWithHSTSCache(sessionID);
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            auto securityOrigins = networkStorageSession->credentialStorage().originsWithCredentials();
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::Credentials, 0 });
        }
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(*this, sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
    
    auto path = m_idbDatabasePaths.get(sessionID);
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        webIDBServer(sessionID).getOrigins([callbackAggregator](auto&& origins) {
            while (!origins.isEmpty())
                callbackAggregator->m_websiteData.entries.append({ origins.takeAny(), WebsiteDataType::IndexedDBDatabases, 0 });
        });
    }
    
#if ENABLE(SERVICE_WORKER)
    path = m_serviceWorkerInfo.get(sessionID).databasePath;
    if (!path.isEmpty() && websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)) {
        swServerForSession(sessionID).getOriginsWithRegistrations([callbackAggregator](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
#endif
    
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        forEachNetworkSession([sessionID, fetchOptions, &callbackAggregator](auto& session) {
            fetchDiskCacheEntries(session.cache(), sessionID, fetchOptions, [callbackAggregator](auto entries) mutable {
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

void NetworkProcess::downloadRequest(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedFilename)
{
    downloadManager().startDownload(sessionID, downloadID, request, isNavigatingToAppBoundDomain, suggestedFilename);
}

void NetworkProcess::resumeDownload(PAL::SessionID sessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, WebKit::SandboxExtension::Handle&& sandboxExtensionHandle, CallDownloadDidStart callDownloadDidStart)
{
    downloadManager().resumeDownload(sessionID, downloadID, resumeData, path, WTFMove(sandboxExtensionHandle), callDownloadDidStart);
}

void NetworkProcess::cancelDownload(DownloadID downloadID, CompletionHandler<void(const IPC::DataReference&)>&& completionHandler)
{
    downloadManager().cancelDownload(downloadID, WTFMove(completionHandler));
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

void NetworkProcess::findPendingDownloadLocation(NetworkDataTask& networkDataTask, ResponseCompletionHandler&& completionHandler, const ResourceResponse& response)
{
    uint64_t destinationID = networkDataTask.pendingDownloadID().toUInt64();

    String suggestedFilename = networkDataTask.suggestedFilename();

    downloadProxyConnection()->sendWithAsyncReply(Messages::DownloadProxy::DecideDestinationWithSuggestedFilename(response, suggestedFilename), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), networkDataTask = makeRef(networkDataTask)] (String&& destination, SandboxExtension::Handle&& sandboxExtensionHandle, AllowOverwrite allowOverwrite) mutable {
        auto downloadID = networkDataTask->pendingDownloadID();
        if (destination.isEmpty())
            return completionHandler(PolicyAction::Ignore);
        networkDataTask->setPendingDownloadLocation(destination, WTFMove(sandboxExtensionHandle), allowOverwrite == AllowOverwrite::Yes);
        completionHandler(PolicyAction::Download);
        if (networkDataTask->state() == NetworkDataTask::State::Canceling || networkDataTask->state() == NetworkDataTask::State::Completed)
            return;

        if (downloadManager().download(downloadID)) {
            // The completion handler already called dataTaskBecameDownloadTask().
            return;
        }

        downloadManager().downloadDestinationDecided(downloadID, WTFMove(networkDataTask));
    }, destinationID);
}

void NetworkProcess::setCacheModelSynchronouslyForTesting(CacheModel cacheModel, CompletionHandler<void()>&& completionHandler)
{
    setCacheModel(cacheModel);
    completionHandler();
}

void NetworkProcess::setCacheModel(CacheModel cacheModel)
{
    if (m_hasSetCacheModel && (cacheModel == m_cacheModel))
        return;

    m_hasSetCacheModel = true;
    m_cacheModel = cacheModel;

    forEachNetworkSession([](auto& session) {
        if (auto* cache = session.cache())
            cache->updateCapacity();
    });
}

void NetworkProcess::setAllowsAnySSLCertificateForWebSocket(bool allows, CompletionHandler<void()>&& completionHandler)
{
    DeprecatedGlobalSettings::setAllowsAnySSLCertificate(allows);
    completionHandler();
}

void NetworkProcess::logDiagnosticMessage(WebPageProxyIdentifier webPageProxyID, const String& message, const String& description, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessage(webPageProxyID, message, description, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithResult(WebPageProxyIdentifier webPageProxyID, const String& message, const String& description, DiagnosticLoggingResultType result, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessageWithResult(webPageProxyID, message, description, result, ShouldSample::No), 0);
}

void NetworkProcess::logDiagnosticMessageWithValue(WebPageProxyIdentifier webPageProxyID, const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogDiagnosticMessageWithValue(webPageProxyID, message, description, value, significantFigures, ShouldSample::No), 0);
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

void NetworkProcess::processWillSuspendImminentlyForTestingSync(CompletionHandler<void()>&& completionHandler)
{
    prepareToSuspend(true, WTFMove(completionHandler));
}

void NetworkProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend(), isSuspensionImminent=%d", this, isSuspensionImminent);

#if PLATFORM(IOS_FAMILY)
    for (auto& server : m_webIDBServers.values())
        server->suspend();
#endif

#if PLATFORM(IOS_FAMILY)
    m_webSQLiteDatabaseTracker.setIsSuspended(true);
#endif

    lowMemoryHandler(Critical::Yes);

    RefPtr<CallbackAggregator> callbackAggregator = CallbackAggregator::create([this, completionHandler = WTFMove(completionHandler)]() mutable {
        RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend() Process is ready to suspend", this);
        UNUSED_VARIABLE(this);
        completionHandler();
    });
    
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebResourceLoadStatisticsStore::suspend([callbackAggregator] { });
#endif

    forEachNetworkSession([&] (auto& networkSession) {
        platformFlushCookies(networkSession.sessionID(), [callbackAggregator] { });
    });

    for (auto& connection : m_webProcessConnections.values())
        connection->cleanupForSuspension([callbackAggregator] { });

#if ENABLE(SERVICE_WORKER)
    for (auto& server : m_swServers.values()) {
        ASSERT(m_swServers.get(server->sessionID()) == server.get());
        server->startSuspension([callbackAggregator] { });
    }
#endif

    m_storageManagerSet->suspend([callbackAggregator] { });
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

    for (auto& connection : m_webProcessConnections.values())
        connection->endSuspension();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebResourceLoadStatisticsStore::resume();
#endif
    
#if ENABLE(SERVICE_WORKER)
    for (auto& server : m_swServers.values())
        server->endSuspension();
#endif
#if PLATFORM(IOS_FAMILY)
    for (auto& server : m_webIDBServers.values())
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

void NetworkProcess::registerURLSchemeAsSecure(const String& scheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsSecure(scheme);
}

void NetworkProcess::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void NetworkProcess::registerURLSchemeAsLocal(const String& scheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsLocal(scheme);
}

void NetworkProcess::registerURLSchemeAsNoAccess(const String& scheme) const
{
    LegacySchemeRegistry::registerURLSchemeAsNoAccess(scheme);
}

Ref<WebIDBServer> NetworkProcess::createWebIDBServer(PAL::SessionID sessionID)
{
    String path;
    if (!sessionID.isEphemeral()) {
        ASSERT(m_idbDatabasePaths.contains(sessionID));
        path = m_idbDatabasePaths.get(sessionID);
    }

    auto spaceRequester = [protectedThis = makeRef(*this), sessionID](const auto& origin, uint64_t spaceRequested) {
        return protectedThis->storageQuotaManager(sessionID, origin)->requestSpaceOnBackgroundThread(spaceRequested);
    };
    auto closeHandler = [protectedThis = makeRef(*this), sessionID]() {
        protectedThis->m_webIDBServers.remove(sessionID);
    };
    return WebIDBServer::create(sessionID, path, WTFMove(spaceRequester), WTFMove(closeHandler));
}

WebIDBServer& NetworkProcess::webIDBServer(PAL::SessionID sessionID)
{
    return *m_webIDBServers.ensure(sessionID, [this, sessionID] {
        return this->createWebIDBServer(sessionID);
    }).iterator->value;
}

void NetworkProcess::addIndexedDatabaseSession(PAL::SessionID sessionID, String& indexedDatabaseDirectory, SandboxExtension::Handle& handle)
{
    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    auto addResult = m_idbDatabasePaths.add(sessionID, indexedDatabaseDirectory);
    if (addResult.isNewEntry) {
        SandboxExtension::consumePermanently(handle);
        setSessionStorageQuotaManagerIDBRootPath(sessionID, indexedDatabaseDirectory);
    }
}

void NetworkProcess::setSessionStorageQuotaManagerIDBRootPath(PAL::SessionID sessionID, const String& idbRootPath)
{
    LockHolder locker(m_sessionStorageQuotaManagersLock);
    auto* sessionStorageQuotaManager = m_sessionStorageQuotaManagers.get(sessionID);
    ASSERT(sessionStorageQuotaManager);
    sessionStorageQuotaManager->setIDBRootPath(idbRootPath);
}

void NetworkProcess::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    m_storageManagerSet->waitUntilSyncingLocalStorageFinished();
    completionHandler();
}

void NetworkProcess::resetQuota(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    LockHolder locker(m_sessionStorageQuotaManagersLock);
    if (auto* sessionStorageQuotaManager = m_sessionStorageQuotaManagers.get(sessionID)) {
        for (auto storageQuotaManager : sessionStorageQuotaManager->existingStorageQuotaManagers())
            storageQuotaManager->resetQuotaForTesting();
    }
    completionHandler();
}

void NetworkProcess::renameOriginInWebsiteData(PAL::SessionID sessionID, const URL& oldName, const URL& newName, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void()>&& completionHandler)
{
    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto oldOrigin = WebCore::SecurityOriginData::fromURL(oldName);
    auto newOrigin = WebCore::SecurityOriginData::fromURL(newName);

    if (oldOrigin.isEmpty() || newOrigin.isEmpty())
        return;

    if (dataTypes.contains(WebsiteDataType::LocalStorage)) {
        if (m_storageManagerSet->contains(sessionID))
            m_storageManagerSet->renameOrigin(sessionID, oldName, newName, [aggregator] { });
    }

    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases)) {
        auto path = m_idbDatabasePaths.get(sessionID);
        if (!path.isEmpty())
            webIDBServer(sessionID).renameOrigin(oldOrigin, newOrigin, [aggregator] { });
    }
}

#if ENABLE(SERVICE_WORKER)
void NetworkProcess::forEachSWServer(const Function<void(SWServer&)>& callback)
{
    for (auto& server : m_swServers.values())
        callback(*server);
}

SWServer& NetworkProcess::swServerForSession(PAL::SessionID sessionID)
{
    auto result = m_swServers.ensure(sessionID, [&] {
        auto info = m_serviceWorkerInfo.get(sessionID);
        auto path = info.databasePath;
        // There should already be a registered path for this PAL::SessionID.
        // If there's not, then where did this PAL::SessionID come from?
        ASSERT(sessionID.isEphemeral() || !path.isEmpty());
        
#if ENABLE(APP_BOUND_DOMAINS)
        auto appBoundDomainsCallback = [this, sessionID](auto&& completionHandler) {
            parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::GetAppBoundDomains { sessionID }, WTFMove(completionHandler), 0);
        };
#else
        auto appBoundDomainsCallback = [] (auto&& completionHandler) {
            completionHandler({ });
        };
#endif
        return makeUnique<SWServer>(makeUniqueRef<WebSWOriginStore>(), info.processTerminationDelayEnabled, WTFMove(path), sessionID, parentProcessHasServiceWorkerEntitlement(), [this, sessionID](auto&& jobData, bool shouldRefreshCache, auto&& request, auto&& completionHandler) mutable {
            ServiceWorkerSoftUpdateLoader::start(networkSession(sessionID), WTFMove(jobData), shouldRefreshCache, WTFMove(request), WTFMove(completionHandler));
        }, [this, sessionID](auto& registrableDomain, auto&& completionHandler) {
            ASSERT(!registrableDomain.isEmpty());
            parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::EstablishWorkerContextConnectionToNetworkProcess { registrableDomain, sessionID }, WTFMove(completionHandler), 0);
        }, WTFMove(appBoundDomainsCallback));
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

void NetworkProcess::registerSWServerConnection(WebSWServerConnection& connection)
{
    auto* store = existingSWOriginStoreForSession(connection.sessionID());
    ASSERT(store);
    if (store)
        store->registerSWServerConnection(connection);
}

void NetworkProcess::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    if (auto* store = existingSWOriginStoreForSession(connection.sessionID()))
        store->unregisterSWServerConnection(connection);
}

void NetworkProcess::addServiceWorkerSession(PAL::SessionID sessionID, bool processTerminationDelayEnabled, String&& serviceWorkerRegistrationDirectory, const SandboxExtension::Handle& handle)
{
    ServiceWorkerInfo info {
        WTFMove(serviceWorkerRegistrationDirectory),
        processTerminationDelayEnabled
    };
    auto addResult = m_serviceWorkerInfo.add(sessionID, WTFMove(info));
    if (addResult.isNewEntry)
        SandboxExtension::consumePermanently(handle);
}
#endif // ENABLE(SERVICE_WORKER)

void NetworkProcess::requestStorageSpace(PAL::SessionID sessionID, const ClientOrigin& origin, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(Optional<uint64_t>)>&& callback)
{
    parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageSpace { sessionID, origin, quota, currentSize, spaceRequired }, WTFMove(callback), 0);
}

RefPtr<StorageQuotaManager> NetworkProcess::storageQuotaManager(PAL::SessionID sessionID, const ClientOrigin& origin)
{
    LockHolder locker(m_sessionStorageQuotaManagersLock);
    auto* sessionStorageQuotaManager = m_sessionStorageQuotaManagers.get(sessionID);
    if (!sessionStorageQuotaManager)
        return nullptr;

    String idbRootPath = sessionStorageQuotaManager->idbRootPath();
    StorageQuotaManager::UsageGetter usageGetter = [cacheRootPath = sessionStorageQuotaManager->cacheRootPath().isolatedCopy(), idbRootPath = idbRootPath.isolatedCopy(), origin = origin.isolatedCopy()]() {
        ASSERT(!isMainRunLoop());

        return CacheStorage::Engine::diskUsage(cacheRootPath, origin) + IDBServer::IDBServer::diskUsage(idbRootPath, origin);
    };
    StorageQuotaManager::QuotaIncreaseRequester quotaIncreaseRequester = [this, weakThis = makeWeakPtr(*this), sessionID, origin] (uint64_t currentQuota, uint64_t currentSpace, uint64_t requestedIncrease, auto&& callback) {
        ASSERT(isMainRunLoop());
        if (!weakThis)
            callback({ });
        requestStorageSpace(sessionID, origin, currentQuota, currentSpace, requestedIncrease, WTFMove(callback));
    };

    auto originStorageQuotaManager = sessionStorageQuotaManager->ensureOriginStorageQuotaManager(origin, sessionStorageQuotaManager->defaultQuota(origin), WTFMove(usageGetter), WTFMove(quotaIncreaseRequester)).ptr();
    return originStorageQuotaManager;
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

void NetworkProcess::flushCookies(const PAL::SessionID&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void NetworkProcess::platformFlushCookies(const PAL::SessionID&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

#endif

void NetworkProcess::storePrivateClickMeasurement(PAL::SessionID sessionID, WebCore::PrivateClickMeasurement&& privateClickMeasurement)
{
    if (auto* session = networkSession(sessionID))
        session->storePrivateClickMeasurement(WTFMove(privateClickMeasurement));
}

void NetworkProcess::dumpPrivateClickMeasurement(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        return session->dumpPrivateClickMeasurement(WTFMove(completionHandler));

    completionHandler({ });
}

void NetworkProcess::clearPrivateClickMeasurement(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->clearPrivateClickMeasurement();
    
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementOverrideTimerForTesting(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementOverrideTimerForTesting(value);
    
    completionHandler();
}

void NetworkProcess::firePrivateClickMeasurementTimerImmediately(PAL::SessionID sessionID)
{
    if (auto* session = networkSession(sessionID))
        session->firePrivateClickMeasurementTimerImmediately();
}

void NetworkProcess::simulateResourceLoadStatisticsSessionRestart(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* session = networkSession(sessionID)) {
        session->recreateResourceLoadStatisticStore([this, sessionID, completionHandler = WTFMove(completionHandler)] () mutable {
            firePrivateClickMeasurementTimerImmediately(sessionID);
            completionHandler();
        });
        return;
    }
#endif
    completionHandler();
}

void NetworkProcess::markAttributedPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        session->markAttributedPrivateClickMeasurementsAsExpiredForTesting(WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementTokenPublicKeyURLForTesting(PAL::SessionID sessionID, URL&& url, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementTokenPublicKeyURLForTesting(WTFMove(url));

    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementTokenSignatureURLForTesting(PAL::SessionID sessionID, URL&& url, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementTokenSignatureURLForTesting(WTFMove(url));
    
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementAttributionReportURLsForTesting(PAL::SessionID sessionID, URL&& sourceURL, URL&& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementAttributionReportURLsForTesting(WTFMove(sourceURL), WTFMove(destinationURL));

    completionHandler();
}

void NetworkProcess::markPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->markPrivateClickMeasurementsAsExpiredForTesting();

    completionHandler();
}

void NetworkProcess::setPCMFraudPreventionValuesForTesting(PAL::SessionID sessionID, String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setPCMFraudPreventionValuesForTesting(WTFMove(unlinkableToken), WTFMove(secretToken), WTFMove(signature), WTFMove(keyID));

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
        completionHandler({ });
        return;
    }

    m_storageManagerSet->getLocalStorageOriginDetails(sessionID, WTFMove(completionHandler));
}

void NetworkProcess::connectionToWebProcessClosed(IPC::Connection& connection, PAL::SessionID sessionID)
{
    m_storageManagerSet->removeConnection(connection);

    if (auto* server = m_webIDBServers.get(sessionID))
        server->removeConnection(connection);
}

NetworkConnectionToWebProcess* NetworkProcess::webProcessConnection(ProcessIdentifier identifier) const
{
    return m_webProcessConnections.get(identifier);
}

const Seconds NetworkProcess::defaultServiceWorkerFetchTimeout = 70_s;
void NetworkProcess::setServiceWorkerFetchTimeoutForTesting(Seconds timeout, CompletionHandler<void()>&& completionHandler)
{
    m_serviceWorkerFetchTimeout = timeout;
    completionHandler();
}

void NetworkProcess::resetServiceWorkerFetchTimeoutForTesting(CompletionHandler<void()>&& completionHandler)
{
    m_serviceWorkerFetchTimeout = defaultServiceWorkerFetchTimeout;
    completionHandler();
}

static constexpr auto delayMax = 100_ms;
static constexpr auto delayMin = 10_ms;
Seconds NetworkProcess::randomClosedPortDelay()
{
    return delayMin + Seconds::fromMilliseconds(static_cast<double>(cryptographicallyRandomNumber())) % delayMax;
}

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcess::hasAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler) const
{
    bool result = false;
    if (auto* networkSession = this->networkSession(sessionID))
        result = networkSession->hasAppBoundSession();
    completionHandler(result);
}

void NetworkProcess::clearAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        networkSession->clearAppBoundSession();
        completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}
#endif

void NetworkProcess::broadcastConsoleMessage(PAL::SessionID sessionID, JSC::MessageSource source, JSC::MessageLevel level, const String& message)
{
    for (auto& networkConnectionToWebProcess : m_webProcessConnections.values()) {
        if (networkConnectionToWebProcess->sessionID() == sessionID)
            networkConnectionToWebProcess->broadcastConsoleMessage(source, level, message);
    }
}

void NetworkProcess::updateBundleIdentifier(String&& bundleIdentifier, CompletionHandler<void()>&& completionHandler)
{
#if PLATFORM(COCOA)
    WebCore::clearApplicationBundleIdentifierTestingOverride();
    WebCore::setApplicationBundleIdentifier(bundleIdentifier);
#endif
    completionHandler();
}

void NetworkProcess::clearBundleIdentifier(CompletionHandler<void()>&& completionHandler)
{
#if PLATFORM(COCOA)
    WebCore::clearApplicationBundleIdentifierTestingOverride();
#endif
    completionHandler();
}

bool NetworkProcess::shouldDisableCORSForRequestTo(PageIdentifier pageIdentifier, const URL& url) const
{
    return WTF::anyOf(m_extensionCORSDisablingPatterns.get(pageIdentifier), [&] (const auto& pattern) {
        return pattern.matches(url);
    });
}

void NetworkProcess::setCORSDisablingPatterns(PageIdentifier pageIdentifier, Vector<String>&& patterns)
{
    Vector<UserContentURLPattern> parsedPatterns;
    parsedPatterns.reserveInitialCapacity(patterns.size());
    for (auto&& pattern : WTFMove(patterns)) {
        UserContentURLPattern parsedPattern(WTFMove(pattern));
        if (parsedPattern.isValid())
            parsedPatterns.uncheckedAppend(WTFMove(parsedPattern));
    }
    parsedPatterns.shrinkToFit();
    if (parsedPatterns.isEmpty()) {
        m_extensionCORSDisablingPatterns.remove(pageIdentifier);
        return;
    }
    m_extensionCORSDisablingPatterns.set(pageIdentifier, WTFMove(parsedPatterns));
}

#if PLATFORM(COCOA)
void NetworkProcess::appBoundNavigationData(PAL::SessionID sessionID, CompletionHandler<void(const AppBoundNavigationTestingData&)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID)) {
        completionHandler(networkSession->appBoundNavigationTestingData());
        return;
    }
    completionHandler({ });
}

void NetworkProcess::clearAppBoundNavigationData(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession(sessionID))
        networkSession->appBoundNavigationTestingData().clearAppBoundNavigationDataTesting();

    completionHandler();
}
#endif

#if ENABLE(WEB_RTC)
RTCDataChannelRemoteManagerProxy& NetworkProcess::rtcDataChannelProxy()
{
    ASSERT(isMainRunLoop());
    if (!m_rtcDataChannelProxy)
        m_rtcDataChannelProxy = RTCDataChannelRemoteManagerProxy::create();
    return *m_rtcDataChannelProxy;
}
#endif

} // namespace WebKit
