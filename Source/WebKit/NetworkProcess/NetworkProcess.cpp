/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "ChildProcessMessages.h"
#include "DataReference.h"
#include "DownloadProxyMessages.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif
#include "Logging.h"
#include "NetworkBlobRegistry.h"
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
#include "SessionTracker.h"
#include "StatisticsData.h"
#include "WebCookieManager.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
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

    WorkQueue::create("com.apple.WebKit.ChildProcess.WatchDogQueue")->dispatchAfter(watchdogDelay, [] {
        // We use _exit here since the watchdog callback is called from another thread and we don't want
        // global destructors or atexit handlers to be called from this thread while the main thread is busy
        // doing its thing.
        RELEASE_LOG_ERROR(IPC, "Exiting process early due to unacknowledged closed-connection");
        _exit(EXIT_FAILURE);
    });
}

NetworkProcess& NetworkProcess::singleton()
{
    static NeverDestroyed<NetworkProcess> networkProcess;
    return networkProcess;
}

NetworkProcess::NetworkProcess()
    : m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_diskCacheIsDisabledForTesting(false)
    , m_canHandleHTTPSServerTrustEvaluation(true)
#if PLATFORM(COCOA)
    , m_clearCacheDispatchGroup(0)
#endif
    , m_storageTaskQueue(WorkQueue::create("com.apple.WebKit.StorageTask"))
#if ENABLE(INDEXED_DATABASE)
    , m_idbPerOriginQuota(IDBServer::defaultPerOriginQuota)
#endif
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    addSupplement<LegacyCustomProtocolManager>();
#endif
#if ENABLE(PROXIMITY_NETWORKING)
    addSupplement<NetworkProximityManager>();
#endif

    NetworkStateNotifier::singleton().addListener([this](bool isOnLine) {
        auto webProcessConnections = m_webProcessConnections;
        for (auto& webProcessConnection : webProcessConnections)
            webProcessConnection->setOnLineState(isOnLine);
    });
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
    static NeverDestroyed<DownloadManager> downloadManager(*this);
    return downloadManager;
}

#if ENABLE(PROXIMITY_NETWORKING)
NetworkProximityManager& NetworkProcess::proximityManager()
{
    return *supplement<NetworkProximityManager>();
}
#endif

void NetworkProcess::removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess* connection)
{
    size_t vectorIndex = m_webProcessConnections.find(connection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);
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

    if (decoder.messageReceiverName() == Messages::ChildProcess::messageReceiverName()) {
        ChildProcess::didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (decoder.messageReceiverName() == Messages::NetworkContentRuleListManager::messageReceiverName()) {
        m_NetworkContentRuleListManager.didReceiveMessage(connection, decoder);
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

    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    setCanHandleHTTPSServerTrustEvaluation(parameters.canHandleHTTPSServerTrustEvaluation);

    if (parameters.shouldUseTestingNetworkSession)
        NetworkStorageSession::switchToNewTestingSession();

    auto sessionID = parameters.defaultDataStoreParameters.networkSessionParameters.sessionID;
    SessionTracker::setSession(sessionID, NetworkSession::create(WTFMove(parameters.defaultDataStoreParameters.networkSessionParameters)));

#if ENABLE(INDEXED_DATABASE)
    addIndexedDatabaseSession(PAL::SessionID::defaultSessionID(), parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    if (parentProcessHasServiceWorkerEntitlement()) {
        addServiceWorkerSession(PAL::SessionID::defaultSessionID(), parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);

        for (auto& scheme : parameters.urlSchemesServiceWorkersCanHandle)
            registerURLSchemeServiceWorkersCanHandle(scheme);

        m_shouldDisableServiceWorkerProcessTerminationDelay = parameters.shouldDisableServiceWorkerProcessTerminationDelay;
    }
#endif

    auto* defaultSession = SessionTracker::networkSession(PAL::SessionID::defaultSessionID());
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
    ChildProcess::initializeConnection(connection);

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

    auto connection = NetworkConnectionToWebProcess::create(socketPair.server);
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
    auto connection = NetworkConnectionToWebProcess::create(IPC::Connection::Identifier(listeningPort));
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientPort), 0);
#elif OS(WINDOWS)
    IPC::Connection::Identifier serverIdentifier, clientIdentifier;
    if (!IPC::Connection::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        LOG_ERROR("Failed to create server and client identifiers");
        CRASH();
    }

    auto connection = NetworkConnectionToWebProcess::create(serverIdentifier);
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
        auto contextConnection = WebSWServerToContextConnection::create(securityOrigin, m_webProcessConnections.last()->connection());
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
    NetworkStorageSession::defaultStorageSession().credentialStorage().clearCredentials();
    if (auto* networkSession = SessionTracker::networkSession(PAL::SessionID::defaultSessionID()))
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

    RemoteNetworkingContext::ensureWebsiteDataStoreSession(WTFMove(parameters));
}

void NetworkProcess::destroySession(PAL::SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
    m_sessionsControlledByAutomation.remove(sessionID);
    CacheStorage::Engine::destroyEngine(sessionID);

#if ENABLE(SERVICE_WORKER)
    m_swServers.remove(sessionID);
    m_swDatabasePaths.remove(sessionID);
#endif
}

void NetworkProcess::writeBlobToFilePath(const URL& url, const String& path, SandboxExtension::Handle&& handleForWriting, CompletionHandler<void(bool)>&& completionHandler)
{
    auto extension = SandboxExtension::create(WTFMove(handleForWriting));
    if (!extension) {
        completionHandler(false);
        return;
    }

    extension->consume();
    NetworkBlobRegistry::singleton().writeBlobToFilePath(url, path, [extension = WTFMove(extension), completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        extension->revoke();
        completionHandler(success);
    });
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkProcess::updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID sessionID, const Vector<String>& domainsToBlock, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->setPrevalentDomainsToBlockCookiesFor(domainsToBlock);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidUpdateBlockCookies(contextId), 0);
}

void NetworkProcess::setAgeCapForClientSideCookies(PAL::SessionID sessionID, Optional<Seconds> seconds, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->setAgeCapForClientSideCookies(seconds);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidSetAgeCapForClientSideCookies(contextId), 0);
}

void NetworkProcess::hasStorageAccessForFrame(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        parentProcessConnection()->send(Messages::NetworkProcessProxy::StorageAccessRequestResult(networkStorageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID), contextId), 0);
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::getAllStorageAccessEntries(PAL::SessionID sessionID, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        parentProcessConnection()->send(Messages::NetworkProcessProxy::AllStorageAccessEntriesResult(networkStorageSession->getAllStorageAccessEntries(), contextId), 0);
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::grantStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, uint64_t contextId)
{
    bool isStorageGranted = false;
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID)) {
        networkStorageSession->grantStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID);
        ASSERT(networkStorageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
        isStorageGranted = true;
    } else
        ASSERT_NOT_REACHED();

    parentProcessConnection()->send(Messages::NetworkProcessProxy::StorageAccessRequestResult(isStorageGranted, contextId), 0);
}

void NetworkProcess::removeAllStorageAccess(PAL::SessionID sessionID, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->removeAllStorageAccess();
    else
        ASSERT_NOT_REACHED();
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidRemoveAllStorageAccess(contextId), 0);
}

void NetworkProcess::removePrevalentDomains(PAL::SessionID sessionID, const Vector<String>& domains)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->removePrevalentDomains(domains);
}

void NetworkProcess::setCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, Seconds seconds, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->setCacheMaxAgeCapForPrevalentResources(Seconds { seconds });
    else
        ASSERT_NOT_REACHED();
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidSetCacheMaxAgeCapForPrevalentResources(contextId), 0);
}

void NetworkProcess::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->resetCacheMaxAgeCapForPrevalentResources();
    else
        ASSERT_NOT_REACHED();
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidResetCacheMaxAgeCapForPrevalentResources(contextId), 0);
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

static void fetchDiskCacheEntries(PAL::SessionID sessionID, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void (Vector<WebsiteData::Entry>)>&& completionHandler)
{
    if (auto* cache = NetworkProcess::singleton().cache()) {
        HashMap<SecurityOriginData, uint64_t> originsAndSizes;
        cache->traverse([fetchOptions, completionHandler = WTFMove(completionHandler), originsAndSizes = WTFMove(originsAndSizes)](auto* traversalEntry) mutable {
            if (!traversalEntry) {
                Vector<WebsiteData::Entry> entries;

                for (auto& originAndSize : originsAndSizes)
                    entries.append(WebsiteData::Entry { originAndSize.key, WebsiteDataType::DiskCache, originAndSize.value });

                RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), entries = WTFMove(entries)] {
                    completionHandler(entries);
                });

                return;
            }

            auto url = traversalEntry->entry.response().url();
            auto result = originsAndSizes.add({url.protocol().toString(), url.host().toString(), url.port()}, 0);

            if (fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes))
                result.iterator->value += traversalEntry->entry.sourceStorageRecord().header.size() + traversalEntry->recordInfo.bodySize;
        });

        return;
    }

    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
        completionHandler({ });
    });
}

void NetworkProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, uint64_t callbackID)
{
    struct CallbackAggregator final : public RefCounted<CallbackAggregator> {
        explicit CallbackAggregator(Function<void (WebsiteData)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), websiteData = WTFMove(m_websiteData)] {
                completionHandler(websiteData);
            });
        }

        Function<void (WebsiteData)> m_completionHandler;
        WebsiteData m_websiteData;
    };

    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, callbackID] (WebsiteData websiteData) {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    }));

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            networkStorageSession->getHostnamesWithCookies(callbackAggregator->m_websiteData.hostNamesWithCookies);
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (NetworkStorageSession::storageSession(sessionID))
            callbackAggregator->m_websiteData.originsWithCredentials = NetworkStorageSession::storageSession(sessionID)->credentialStorage().originsWithCredentials();
    }

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        CacheStorage::Engine::fetchEntries(sessionID, fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator = callbackAggregator.copyRef()](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }

#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
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
        fetchDiskCacheEntries(sessionID, fetchOptions, [callbackAggregator = WTFMove(callbackAggregator)](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
}

void NetworkProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, uint64_t callbackID)
{
#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            clearHSTSCache(*networkStorageSession, modifiedSince);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            networkStorageSession->deleteAllCookiesModifiedSince(modifiedSince);
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (NetworkStorageSession::storageSession(sessionID))
            NetworkStorageSession::storageSession(sessionID)->credentialStorage().clearCredentials();
    }

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        CacheStorage::Engine::clearAllCaches(sessionID, [clearTasksHandler = clearTasksHandler.copyRef()] { });

#if ENABLE(INDEXED_DATABASE)
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !sessionID.isEphemeral())
        idbServer(sessionID).closeAndDeleteDatabasesModifiedSince(modifiedSince, [clearTasksHandler = clearTasksHandler.copyRef()] { });
#endif

#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && !sessionID.isEphemeral())
        swServerForSession(sessionID).clearAll([clearTasksHandler = clearTasksHandler.copyRef()] { });
#endif

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral())
        clearDiskCache(modifiedSince, [clearTasksHandler = WTFMove(clearTasksHandler)] { });
}

static void clearDiskCacheEntries(const Vector<SecurityOriginData>& origins, Function<void ()>&& completionHandler)
{
    if (auto* cache = NetworkProcess::singleton().cache()) {
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

        return;
    }

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void NetworkProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& originDatas, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, uint64_t callbackID)
{
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            networkStorageSession->deleteCookiesForHostnames(cookieHostNames);
    }

#if PLATFORM(COCOA)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            deleteHSTSCacheForHostNames(*networkStorageSession, HSTSCacheHostNames);
    }
#endif

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        for (auto& originData : originDatas)
            CacheStorage::Engine::clearCachesForOrigin(sessionID, SecurityOriginData { originData }, [clearTasksHandler = clearTasksHandler.copyRef()] { });
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
        clearDiskCacheEntries(originDatas, [clearTasksHandler = WTFMove(clearTasksHandler)] { });
}

void NetworkProcess::downloadRequest(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedFilename)
{
    downloadManager().startDownload(nullptr, sessionID, downloadID, request, suggestedFilename);
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

void NetworkProcess::setCacheModel(uint32_t cm)
{
    CacheModel cacheModel = static_cast<CacheModel>(cm);

    if (m_hasSetCacheModel && (cacheModel == m_cacheModel))
        return;

    m_hasSetCacheModel = true;
    m_cacheModel = cacheModel;

    unsigned urlCacheMemoryCapacity = 0;
    uint64_t urlCacheDiskCapacity = 0;
    uint64_t diskFreeSize = 0;
    if (WebCore::FileSystem::getVolumeFreeSpace(m_diskCacheDirectory, diskFreeSize)) {
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

    auto& networkProcess = NetworkProcess::singleton();
    data.statisticsNumbers.set("DownloadsActiveCount", networkProcess.downloadManager().activeDownloadCount());
    data.statisticsNumbers.set("OutstandingAuthenticationChallengesCount", networkProcess.authenticationManager().outstandingAuthenticationChallengeCount());

    parentProcessConnection()->send(Messages::WebProcessPool::DidGetStatistics(data, callbackID), 0);
}

void NetworkProcess::setAllowsAnySSLCertificateForWebSocket(bool allows)
{
    DeprecatedGlobalSettings::setAllowsAnySSLCertificate(allows);
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
    ChildProcess::terminate();
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

void NetworkProcess::processWillSuspendImminently(bool& handled)
{
    actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend::No);
    handled = true;
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

    new PreconnectTask(WTFMove(parameters));
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
    
    addResult.iterator->value = IDBServer::IDBServer::create(path, NetworkProcess::singleton());
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

void NetworkProcess::didReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, const WebCore::ResourceResponse& response)
{
    if (auto* connection = m_swServerConnections.get(serverConnectionIdentifier))
        connection->didReceiveFetchResponse(fetchIdentifier, response);
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

#if !PLATFORM(COCOA)
void NetworkProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
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

} // namespace WebKit
