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
#include "BackgroundFetchState.h"
#include "DataReference.h"
#include "Download.h"
#include "DownloadProxyMessages.h"
#include "ITPThirdPartyData.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif
#include "LoadedWebArchive.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkLoad.h"
#include "NetworkLoadScheduler.h"
#include "NetworkOriginAccessPatterns.h"
#include "NetworkProcessConnectionParameters.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessPlatformStrategies.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "NetworkStorageManager.h"
#include "PreconnectTask.h"
#include "PrivateClickMeasurementPersistentStore.h"
#include "ProcessAssertion.h"
#include "RTCDataChannelRemoteManagerProxy.h"
#include "RemoteWorkerType.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebCookieManager.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebPushMessage.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSharedWorkerServer.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsiteDataType.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/CommonAtomStrings.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/DNS.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStateNotifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotificationData.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SWServer.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/UserContentURLPattern.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>
#include <wtf/UniqueRef.h>
#include <wtf/WTFProcess.h>
#include <wtf/text/AtomString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#include "NetworkCache.h"
#include "NetworkCacheCoders.h"

#if PLATFORM(COCOA)
#include "CookieStorageUtilsCF.h"
#include "LaunchServicesDatabaseObserver.h"
#include "NetworkSessionCocoa.h"
#include <wtf/cocoa/Entitlements.h>
#endif

#if USE(SOUP)
#include "NetworkSessionSoup.h"
#include <WebCore/SoupNetworkSession.h>
#endif

#if USE(CURL)
#include <WebCore/CurlContext.h>
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
        terminateProcess(EXIT_FAILURE);
    });
}

NetworkProcess::NetworkProcess(AuxiliaryProcessInitializationParameters&& parameters)
    : m_downloadManager(*this)
#if ENABLE(CONTENT_EXTENSIONS)
    , m_networkContentRuleListManager(*this)
#endif
#if USE(RUNNINGBOARD)
    , m_webSQLiteDatabaseTracker([this](bool isHoldingLockedFiles) { setIsHoldingLockedFiles(isHoldingLockedFiles); })
#endif
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

    NetworkStateNotifier::singleton().addListener([weakThis = WeakPtr { *this }](bool isOnLine) {
        if (!weakThis)
            return;
        for (auto& webProcessConnection : weakThis->m_webProcessConnections.values())
            webProcessConnection->setOnLineState(isOnLine);
    });

    initialize(WTFMove(parameters));
}

NetworkProcess::~NetworkProcess() = default;

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
    m_allowedFirstPartiesForCookies.remove(connection.webProcessIdentifier());
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

void NetworkProcess::stopRunLoopIfNecessary()
{
    if (m_didSyncCookiesForClose && m_closingStorageManagers.isEmpty())
        stopRunLoop();
}

void NetworkProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([this] {
        ASSERT(RunLoop::isMain());
        m_didSyncCookiesForClose = true;
        stopRunLoopIfNecessary();
    });

    forEachNetworkSession([&](auto& session) {
        platformFlushCookies(session.sessionID(), [callbackAggregator] { });
        session.storageManager().syncLocalStorage([callbackAggregator] { });
    });

#if PLATFORM(COCOA)
    if (m_mediaStreamingActivitityToken != NOTIFY_TOKEN_INVALID)
        notify_cancel(m_mediaStreamingActivitityToken);
#endif
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

    forEachNetworkSession([critical](auto& session) {
        session.lowMemoryHandler(critical);
    });
}

void NetworkProcess::initializeNetworkProcess(NetworkProcessCreationParameters&& parameters, CompletionHandler<void()>&& completionHandler)
{
    CompletionHandlerCallingScope callCompletionHandler(WTFMove(completionHandler));

    applyProcessCreationParameters(parameters.auxiliaryProcessParameters);
#if HAVE(SEC_KEY_PROXY)
    WTF::setProcessPrivileges({ ProcessPrivilege::CanAccessRawCookies });
#else
    WTF::setProcessPrivileges({ ProcessPrivilege::CanAccessRawCookies, ProcessPrivilege::CanAccessCredentials });
#endif
    WebCore::SQLiteDatabase::useFastMalloc();
    WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
    platformInitializeNetworkProcess(parameters);

    WTF::Thread::setCurrentThreadIsUserInitiated();
    WebCore::initializeCommonAtomStrings();

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([weakThis = WeakPtr { *this }] (Critical critical, Synchronous) {
            if (RefPtr process = weakThis.get())
                process->lowMemoryHandler(critical);
        });
        memoryPressureHandler.install();
    }

    setCacheModel(parameters.cacheModel);

    setPrivateClickMeasurementEnabled(parameters.enablePrivateClickMeasurement);
    m_ftpEnabled = parameters.ftpEnabled;
#if ENABLE(BUILT_IN_NOTIFICATIONS)
    m_builtInNotificationsEnabled = parameters.builtInNotificationsEnabled;
#endif

    for (auto [processIdentifier, domain] : parameters.allowedFirstPartiesForCookies) {
        if (RefPtr connection = webProcessConnection(processIdentifier))
            connection->addAllowedFirstPartyForCookies(domain);
        addAllowedFirstPartyForCookies(processIdentifier, WTFMove(domain), LoadedWebArchive::No, [] { });
    }

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

    m_localhostAliasesForTesting = WTFMove(parameters.localhostAliasesForTesting);

    updateStorageAccessPromptQuirks(WTFMove(parameters.storageAccessPromptQuirksData));

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

void NetworkProcess::createNetworkConnectionToWebProcess(ProcessIdentifier identifier, PAL::SessionID sessionID, NetworkProcessConnectionParameters&& parameters, CompletionHandler<void(std::optional<IPC::Connection::Handle>&&, HTTPCookieAcceptPolicy)>&& completionHandler)
{
    auto connectionIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!connectionIdentifiers) {
        completionHandler({ }, HTTPCookieAcceptPolicy::Never);
        return;
    }

    auto newConnection = NetworkConnectionToWebProcess::create(*this, identifier, sessionID, WTFMove(parameters), connectionIdentifiers->server);
    Ref connection = newConnection;

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    auto* storage = storageSession(sessionID);
    completionHandler(WTFMove(connectionIdentifiers->client), storage ? storage->cookieAcceptPolicy() : HTTPCookieAcceptPolicy::Never);

    connection->setOnLineState(NetworkStateNotifier::singleton().onLine());

#if ENABLE(IPC_TESTING_API)
    if (parameters.ignoreInvalidMessageForTesting)
        connection->connection().setIgnoreInvalidMessageForTesting();
#endif

    if (auto* session = networkSession(sessionID))
        session->storageManager().startReceivingMessageFromConnection(Ref { connection->connection() });
}

void NetworkProcess::addAllowedFirstPartyForCookies(WebCore::ProcessIdentifier processIdentifier, WebCore::RegistrableDomain&& firstPartyForCookies, LoadedWebArchive loadedWebArchive, CompletionHandler<void()>&& completionHandler)
{
    if (!HashSet<WebCore::RegistrableDomain>::isValidValue(firstPartyForCookies))
        return completionHandler();

    auto& pair = m_allowedFirstPartiesForCookies.ensure(processIdentifier, [] {
        return std::make_pair(LoadedWebArchive::No, HashSet<RegistrableDomain> { });
    }).iterator->value;

    pair.second.add(WTFMove(firstPartyForCookies));

    if (loadedWebArchive == LoadedWebArchive::Yes)
        pair.first = LoadedWebArchive::Yes;

    completionHandler();
}

void NetworkProcess::webProcessWillLoadWebArchive(WebCore::ProcessIdentifier processIdentifier)
{
    m_allowedFirstPartiesForCookies.ensure(processIdentifier, [] {
        return std::make_pair(LoadedWebArchive::Yes, HashSet<RegistrableDomain> { });
    }).iterator->value.first = LoadedWebArchive::Yes;
}

bool NetworkProcess::allowsFirstPartyForCookies(WebCore::ProcessIdentifier processIdentifier, const URL& firstParty)
{
    return AuxiliaryProcess::allowsFirstPartyForCookies(firstParty, [&] {
        RegistrableDomain firstPartyDomain(firstParty);
        return allowsFirstPartyForCookies(processIdentifier, firstPartyDomain);
    });
}

bool NetworkProcess::allowsFirstPartyForCookies(WebCore::ProcessIdentifier processIdentifier, const RegistrableDomain& firstPartyDomain)
{
    if (!decltype(m_allowedFirstPartiesForCookies)::isValidKey(processIdentifier)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto iterator = m_allowedFirstPartiesForCookies.find(processIdentifier);
    if (iterator == m_allowedFirstPartiesForCookies.end()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (iterator->value.first == LoadedWebArchive::Yes)
        return true;

    auto& set = iterator->value.second;
    return AuxiliaryProcess::allowsFirstPartyForCookies(firstPartyDomain, set);
}

void NetworkProcess::addStorageSession(PAL::SessionID sessionID, const WebsiteDataStoreParameters& parameters)
{
    auto addResult = m_networkStorageSessions.add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

    if (parameters.networkSessionParameters.shouldUseTestingNetworkSession) {
        addResult.iterator->value = newTestingSession(sessionID);
        return;
    }

#if PLATFORM(COCOA)
    RetainPtr<CFHTTPCookieStorageRef> uiProcessCookieStorage;
    if (!sessionID.isEphemeral() && !parameters.uiProcessCookieStorageIdentifier.isEmpty()) {
        SandboxExtension::consumePermanently(parameters.cookieStoragePathExtensionHandle);
        if (sessionID != PAL::SessionID::defaultSessionID())
            uiProcessCookieStorage = cookieStorageFromIdentifyingData(parameters.uiProcessCookieStorageIdentifier);
    }

    auto identifierBase = makeString(uiProcessBundleIdentifier(), '.', sessionID.toUInt64());
    RetainPtr<CFURLStorageSessionRef> storageSession;
    auto cfIdentifier = makeString(identifierBase, ".PrivateBrowsing."_s, WTF::UUID::createVersion4()).createCFString();
    if (sessionID.isEphemeral())
        storageSession = createPrivateStorageSession(cfIdentifier.get(), std::nullopt, WebCore::NetworkStorageSession::ShouldDisableCFURLCache::Yes);
    else if (sessionID != PAL::SessionID::defaultSessionID())
        storageSession = WebCore::NetworkStorageSession::createCFStorageSessionForIdentifier(cfIdentifier.get(), WebCore::NetworkStorageSession::ShouldDisableCFURLCache::Yes);

    if (NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (!uiProcessCookieStorage && storageSession)
            uiProcessCookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    }

    addResult.iterator->value = makeUnique<NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(uiProcessCookieStorage));
#elif USE(CURL)
    if (!parameters.networkSessionParameters.alternativeServiceDirectory.isEmpty())
        SandboxExtension::consumePermanently(parameters.networkSessionParameters.alternativeServiceDirectoryExtensionHandle);

    addResult.iterator->value = makeUnique<NetworkStorageSession>(sessionID, parameters.networkSessionParameters.alternativeServiceDirectory);
#elif USE(SOUP)
    addResult.iterator->value = makeUnique<NetworkStorageSession>(sessionID);
#endif
}

void NetworkProcess::addWebsiteDataStore(WebsiteDataStoreParameters&& parameters)
{
    auto sessionID = parameters.networkSessionParameters.sessionID;
#if PLATFORM(IOS_FAMILY)
    if (auto& handle = parameters.cookieStorageDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
    if (auto& handle = parameters.containerCachesDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
    if (auto& handle = parameters.parentBundleDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
    if (auto& handle = parameters.tempDirectoryExtensionHandle)
        SandboxExtension::consumePermanently(*handle);
#endif

    addStorageSession(sessionID, parameters);

    auto& session = m_networkSessions.ensure(sessionID, [&]() {
        return NetworkSession::create(*this, parameters.networkSessionParameters);
    }).iterator->value;

    if (m_isSuspended)
        session->storageManager().suspend([] { });
}

void NetworkProcess::forEachNetworkSession(const Function<void(NetworkSession&)>& functor)
{
    for (auto& session : m_networkSessions.values())
        functor(*session);
}

std::unique_ptr<WebCore::NetworkStorageSession> NetworkProcess::newTestingSession(PAL::SessionID sessionID)
{
#if PLATFORM(COCOA)
    // Session name should be short enough for shared memory region name to be under the limit, otherwise sandbox rules won't work (see <rdar://problem/13642852>).
    auto session = WebCore::createPrivateStorageSession(makeString("WebKit Test-", getCurrentProcessID()).createCFString().get(), std::nullopt, NetworkStorageSession::ShouldDisableCFURLCache::Yes);
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

void NetworkProcess::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    for (auto& connection : m_webProcessConnections.values())
        connection->cookieAcceptPolicyChanged(newPolicy);
}

WebCore::NetworkStorageSession* NetworkProcess::storageSession(PAL::SessionID sessionID) const
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

void NetworkProcess::destroySession(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
#if !USE(SOUP) && !USE(CURL)
    // cURL and Soup based ports destroy the default session right before the process exits to avoid leaking
    // network resources like the cookies database.
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());
#endif

    if (auto session = m_networkSessions.take(sessionID)) {
        session->invalidateAndCancel();
        Ref storageManager = session->storageManager();
        m_closingStorageManagers.add(storageManager.copyRef());
        storageManager->close([this, protectedThis = Ref { *this }, storageManager, completionHandler = std::exchange(completionHandler, { })]() mutable {
            m_closingStorageManagers.remove(storageManager.ptr());
            completionHandler();
            stopRunLoopIfNecessary();
        });
    }
    m_networkStorageSessions.remove(sessionID);
    m_sessionsControlledByAutomation.remove(sessionID);
    if (completionHandler)
        completionHandler();
}

void NetworkProcess::registrableDomainsWithLastAccessedTime(PAL::SessionID sessionID, CompletionHandler<void(std::optional<HashMap<RegistrableDomain, WallTime>>)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
            resourceLoadStatistics->registrableDomainsWithLastAccessedTime(WTFMove(completionHandler));
            return;
        }
    }
    completionHandler(std::nullopt);
}

void NetworkProcess::registrableDomainsExemptFromWebsiteDataDeletion(PAL::SessionID sessionID, CompletionHandler<void(HashSet<RegistrableDomain>)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
            resourceLoadStatistics->registrableDomainsExemptFromWebsiteDataDeletion(WTFMove(completionHandler));
            return;
        }
    }
    completionHandler({ });
}

void NetworkProcess::dumpResourceLoadStatistics(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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

void NetworkProcess::isGrandfathered(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isGrandfathered(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isPrevalentResource(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isPrevalentResource(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isVeryPrevalentResource(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isVeryPrevalentResource(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setGrandfathered(PAL::SessionID sessionID, RegistrableDomain&& domain, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setGrandfathered(WTFMove(domain), isGrandfathered, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setPrevalentResource(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setPrevalentResource(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setPrevalentResourceForDebugMode(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setPrevalentResourceForDebugMode(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setVeryPrevalentResource(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setVeryPrevalentResource(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::clearPrevalentResource(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->clearPrevalentResource(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::scheduleCookieBlockingUpdate(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->scheduleCookieBlockingUpdate(WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, std::optional<WallTime> modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        session->clearIsolatedSessions();
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
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

void NetworkProcess::getResourceLoadStatisticsDataSummary(PAL::SessionID sessionID, CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        session->resetFirstPartyDNSData();
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->statisticsDatabaseHasAllTables(WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setNotifyPagesWhenDataRecordsWereScanned(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setResourceLoadStatisticsTimeAdvanceForTesting(PAL::SessionID sessionID, Seconds time, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            return resourceLoadStatistics->setTimeAdvanceForTesting(time, WTFMove(completionHandler));
    }
    completionHandler();
}

void NetworkProcess::setIsRunningResourceLoadStatisticsTest(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setIsRunningTest(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubframeUnderTopFrameDomain(PAL::SessionID sessionID, RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setSubframeUnderTopFrameDomain(WTFMove(subFrameDomain), WTFMove(topFrameDomain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::isRegisteredAsRedirectingTo(PAL::SessionID sessionID, RegistrableDomain&& domainRedirectedFrom, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsRedirectingTo(WTFMove(domainRedirectedFrom), WTFMove(domainRedirectedTo), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isRegisteredAsSubFrameUnder(PAL::SessionID sessionID, RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsSubFrameUnder(WTFMove(subFrameDomain), WTFMove(topFrameDomain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setSubresourceUnderTopFrameDomain(PAL::SessionID sessionID, RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUnderTopFrameDomain(WTFMove(subresourceDomain), WTFMove(topFrameDomain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubresourceUniqueRedirectTo(PAL::SessionID sessionID, RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUniqueRedirectTo(WTFMove(subresourceDomain), WTFMove(domainRedirectedTo), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setSubresourceUniqueRedirectFrom(PAL::SessionID sessionID, RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setSubresourceUniqueRedirectFrom(WTFMove(subresourceDomain), WTFMove(domainRedirectedFrom), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::isRegisteredAsSubresourceUnder(PAL::SessionID sessionID, RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isRegisteredAsSubresourceUnder(WTFMove(subresourceDomain), WTFMove(topFrameDomain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::setTopFrameUniqueRedirectTo(PAL::SessionID sessionID, RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setTopFrameUniqueRedirectTo(WTFMove(topFrameDomain), WTFMove(domainRedirectedTo), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setTopFrameUniqueRedirectFrom(PAL::SessionID sessionID, RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setTopFrameUniqueRedirectFrom(WTFMove(topFrameDomain), WTFMove(domainRedirectedFrom), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}
    
    
void NetworkProcess::setLastSeen(PAL::SessionID sessionID, RegistrableDomain&& domain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setLastSeen(WTFMove(domain), seconds, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::domainIDExistsInDatabase(PAL::SessionID sessionID, int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->domainIDExistsInDatabase(domainID, WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::mergeStatisticForTesting(PAL::SessionID sessionID, RegistrableDomain&& domain, RegistrableDomain&& topFrameDomain1, RegistrableDomain&& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, uint64_t dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->mergeStatisticForTesting(WTFMove(domain), WTFMove(topFrameDomain1), WTFMove(topFrameDomain2), lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, unsigned(dataRecordsRemoved), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::insertExpiredStatisticForTesting(PAL::SessionID sessionID, RegistrableDomain&& domain, uint64_t numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->insertExpiredStatisticForTesting(WTFMove(domain), unsigned(numberOfOperatingDaysPassed), hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, WTFMove(completionHandler));
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

void NetworkProcess::logFrameNavigation(PAL::SessionID sessionID, RegistrableDomain&& targetDomain, RegistrableDomain&& topFrameDomain, RegistrableDomain&& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->logFrameNavigation(WTFMove(targetDomain), WTFMove(topFrameDomain), WTFMove(sourceDomain), isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
    } else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::logUserInteraction(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->logUserInteraction(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::hadUserInteraction(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->hasHadUserInteraction(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::isRelationshipOnlyInDatabaseOnce(PAL::SessionID sessionID, RegistrableDomain&& subDomain, RegistrableDomain&& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->isRelationshipOnlyInDatabaseOnce(WTFMove(subDomain), WTFMove(topDomain), WTFMove(completionHandler));
        else
            completionHandler(false);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(false);
    }
}

void NetworkProcess::clearUserInteraction(PAL::SessionID sessionID, RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->clearUserInteraction(WTFMove(domain), WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::hasLocalStorage(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* session = networkSession(sessionID);
    if (!session)
        return completionHandler(false);

    auto types = OptionSet<WebsiteDataType> { WebsiteDataType::LocalStorage };
    session->storageManager().fetchData(types, NetworkStorageManager::ShouldComputeSize::No, [domain, completionHandler = WTFMove(completionHandler)](auto entries) mutable {
        completionHandler(WTF::anyOf(entries, [&domain](auto& entry) {
            return domain.matches(entry.origin);
        }));
    });
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->setShouldClassifyResourcesBeforeDataRecordsRemoval(value, WTFMove(completionHandler));
        else
            completionHandler();
    } else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void NetworkProcess::setTrackingPreventionEnabled(PAL::SessionID sessionID, bool enabled)
{
    if (auto* session = networkSession(sessionID))
        session->setTrackingPreventionEnabled(enabled);
}

void NetworkProcess::updateStorageAccessPromptQuirks(Vector<WebCore::OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirks)
{
    NetworkStorageSession::updateStorageAccessPromptQuirks(WTFMove(organizationStorageAccessPromptQuirks));
}

void NetworkProcess::setResourceLoadStatisticsLogTestingEvent(bool enabled)
{
    forEachNetworkSession([enabled](auto& session) {
        session.setResourceLoadStatisticsLogTestingEvent(enabled);
    });
}

void NetworkProcess::setResourceLoadStatisticsDebugMode(PAL::SessionID sessionID, bool debugMode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
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

void NetworkProcess::didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID sessionID, RegistrableDomain&& fromDomain, RegistrableDomain&& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID)
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
        if (auto* session = networkSession(sessionID)) {
            if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
                resourceLoadStatistics->logCrossSiteLoadWithLinkDecoration(WTFMove(fromDomain), WTFMove(toDomain), [] { });
        } else
            ASSERT_NOT_REACHED();
    }
}

void NetworkProcess::setCrossSiteLoadWithLinkDecorationForTesting(PAL::SessionID sessionID, RegistrableDomain&& fromDomain, RegistrableDomain&& toDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->logCrossSiteLoadWithLinkDecoration(WTFMove(fromDomain), WTFMove(toDomain), WTFMove(completionHandler));
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

void NetworkProcess::grantStorageAccessForTesting(PAL::SessionID sessionID, Vector<RegistrableDomain>&& subFrameDomains, RegistrableDomain&& topFrameDomain, CompletionHandler<void(void)>&& completionHandler)
{
    HashSet allowedDomains { "site1.example"_str, "site2.example"_str, "site3.example"_str, "site4.example"_str };
    if (!allowedDomains.contains(topFrameDomain.string())) {
        completionHandler();
        return;
    }
    if (auto* networkStorageSession = storageSession(sessionID)) {
        for (auto&& subFrameDomain : subFrameDomains)
            networkStorageSession->grantCrossPageStorageAccess(WTFMove(topFrameDomain), WTFMove(subFrameDomain));
    } else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::hasIsolatedSession(PAL::SessionID sessionID, const WebCore::RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    bool result = false;
    if (auto* session = networkSession(sessionID))
        result = session->hasIsolatedSession(domain);
    completionHandler(result);
}

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcess::setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID sessionID, HashSet<WebCore::RegistrableDomain>&& appBoundDomains, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
            resourceLoadStatistics->setAppBoundDomains(WTFMove(appBoundDomains), WTFMove(completionHandler));
            return;
        }
    }
    ASSERT_NOT_REACHED();
    completionHandler();
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void NetworkProcess::setManagedDomainsForResourceLoadStatistics(PAL::SessionID sessionID, HashSet<WebCore::RegistrableDomain>&& managedDomains, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        supplement<WebCookieManager>()->setHTTPCookieAcceptPolicy(sessionID, WebCore::HTTPCookieAcceptPolicy::AlwaysAccept, [session = WeakPtr { *session }, managedDomains = WTFMove(managedDomains), completionHandler = WTFMove(completionHandler)]() mutable {
            if (session) {
                if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
                    resourceLoadStatistics->setManagedDomains(WTFMove(managedDomains), WTFMove(completionHandler));
                    return;
                }
            }
            completionHandler();
        });
        return;
    }
    ASSERT_NOT_REACHED();
    completionHandler();
}
#endif

void NetworkProcess::setShouldDowngradeReferrerForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    forEachNetworkSession([enabled](auto& session) {
        session.setShouldDowngradeReferrerForTesting(enabled);
    });
    completionHandler();
}

void NetworkProcess::setThirdPartyCookieBlockingMode(PAL::SessionID sessionID, WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setThirdPartyCookieBlockingMode(blockingMode);
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID sessionID, WebCore::SameSiteStrictEnforcementEnabled enabled, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setShouldEnbleSameSiteStrictEnforcement(enabled);
    else
        ASSERT_NOT_REACHED();
    completionHandler();
}

void NetworkProcess::setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID sessionID, WebCore::FirstPartyWebsiteDataRemovalMode mode, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
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
    if (auto* session = networkSession(sessionID))
        session->setFirstPartyHostCNAMEDomain(WTFMove(firstPartyHost), WTFMove(cnameDomain));
    completionHandler();
}

void NetworkProcess::setThirdPartyCNAMEDomainForTesting(PAL::SessionID sessionID, WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->setThirdPartyCNAMEDomainForTesting(WTFMove(domain));
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementEnabled(bool enabled)
{
    m_privateClickMeasurementEnabled = enabled;
}

bool NetworkProcess::privateClickMeasurementEnabled() const
{
    return m_privateClickMeasurementEnabled;
}

void NetworkProcess::notifyMediaStreamingActivity(bool activity)
{
#if PLATFORM(COCOA)
    static const char* notifyMediaStreamingName = "com.apple.WebKit.mediaStreamingActivity";

    if (m_mediaStreamingActivitityToken == NOTIFY_TOKEN_INVALID) {
        auto status = notify_register_check(notifyMediaStreamingName, &m_mediaStreamingActivitityToken);
        if (status != NOTIFY_STATUS_OK || m_mediaStreamingActivitityToken == NOTIFY_TOKEN_INVALID) {
            RELEASE_LOG_ERROR(IPC, "notify_register_check() for %s failed with status (%d) 0x%X", notifyMediaStreamingName, status, status);
            m_mediaStreamingActivitityToken = NOTIFY_TOKEN_INVALID;
            return;
        }
    }
    auto status = notify_set_state(m_mediaStreamingActivitityToken, activity ? 1 : 0);
    if (status != NOTIFY_STATUS_OK) {
        RELEASE_LOG_ERROR(IPC, "notify_set_state() for %s failed with status (%d) 0x%X", notifyMediaStreamingName, status, status);
        return;
    }
    status = notify_post(notifyMediaStreamingName);
    RELEASE_LOG_ERROR_IF(status != NOTIFY_STATUS_OK, IPC, "notify_post() for %s failed with status (%d) 0x%X", notifyMediaStreamingName, status, status);
#else
    UNUSED_PARAM(activity);
#endif
}

void NetworkProcess::setPrivateClickMeasurementDebugMode(PAL::SessionID sessionID, bool enabled)
{
    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementDebugMode(enabled);
}

void NetworkProcess::setBlobRegistryTopOriginPartitioningEnabled(PAL::SessionID sessionID, bool enabled) const
{
    if (auto* session = networkSession(sessionID))
        session->setBlobRegistryTopOriginPartitioningEnabled(enabled);
}

void NetworkProcess::preconnectTo(PAL::SessionID sessionID, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID, WebCore::ResourceRequest&& request, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    auto url = request.url();
    auto userAgent = request.httpUserAgent();

    LOG(Network, "(NetworkProcess) Preconnecting to URL %s (storedCredentialsPolicy %i)", url.string().utf8().data(), (int)storedCredentialsPolicy);

#if ENABLE(SERVER_PRECONNECT)
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (supplement<LegacyCustomProtocolManager>()->supportsScheme(url.protocol().toString()))
        return;
#endif

    auto* session = networkSession(sessionID);
    if (!session)
        return;

    NetworkLoadParameters parameters;
    parameters.request = WTFMove(request);
    parameters.webPageProxyID = webPageProxyID;
    parameters.webPageID = webPageID;
    parameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;

    NetworkLoadParameters parametersForAdditionalPreconnect = parameters;

    session->networkLoadScheduler().startedPreconnectForMainResource(url, userAgent);
    auto task = new PreconnectTask(*session, WTFMove(parameters), [session = WeakPtr { *session }, url, userAgent, parametersForAdditionalPreconnect = WTFMove(parametersForAdditionalPreconnect)](const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& metrics) mutable {
        if (session) {
            session->networkLoadScheduler().finishedPreconnectForMainResource(url, userAgent, error);
#if ENABLE(ADDITIONAL_PRECONNECT_ON_HTTP_1X)
            if (equalLettersIgnoringASCIICase(metrics.protocol, "http/1.1"_s)) {
                auto parameters = parametersForAdditionalPreconnect;
                auto task = new PreconnectTask(*session, WTFMove(parameters), [](const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& metrics) { });
                task->start();
            }
#endif // ENABLE(ADDITIONAL_PRECONNECT_ON_HTTP_1X)
        }
    });
    task->setTimeout(10_s);
    task->start();
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

void NetworkProcess::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, CompletionHandler<void(WebsiteData&&)>&& completionHandler)
{
    RELEASE_LOG(Storage, "NetworkProcess::fetchWebsiteData started to fetch data for session %" PRIu64, sessionID.toUInt64());
    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(WebsiteData&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), websiteData = WTFMove(m_websiteData)] () mutable {
                completionHandler(WTFMove(websiteData));
                RELEASE_LOG(Storage, "NetworkProcess::fetchWebsiteData finished fetching data");
            });
        }

        CompletionHandler<void(WebsiteData&&)> m_completionHandler;
        WebsiteData m_websiteData;
    };

    auto callbackAggregator = adoptRef(*new CallbackAggregator(WTFMove(completionHandler)));
    auto* session = networkSession(sessionID);

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
        if (session) {
            for (auto origin : session->originsWithCredentials())
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::Credentials, 0 });
        }
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        callbackAggregator->m_websiteData.hostNamesWithHSTSCache = hostNamesWithHSTSCache(sessionID);
#endif

    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && session && session->hasServiceWorkerDatabasePath()) {
        session->ensureSWServer().getOriginsWithRegistrations([callbackAggregator](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& origin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && session) {
        if (auto* cache = session->cache()) {
            cache->fetchData(fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes), [callbackAggregator](auto entries) mutable {
                callbackAggregator->m_websiteData.entries.appendVector(entries);
            });
        }
    }

#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices) && session) {
        for (auto& origin : session->hostNamesWithAlternativeServices())
            callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::AlternativeServices, 0 });
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics) && session) {
        if (auto* resourceLoadStatistics = session->resourceLoadStatistics()) {
            resourceLoadStatistics->registrableDomains([callbackAggregator](auto&& domains) mutable {
                while (!domains.isEmpty())
                    callbackAggregator->m_websiteData.registrableDomainsWithResourceLoadStatistics.add(domains.takeLast());
            });
        }
    }

    if (NetworkStorageManager::canHandleTypes(websiteDataTypes) && session) {
        auto shouldComputeSize = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? NetworkStorageManager::ShouldComputeSize::Yes : NetworkStorageManager::ShouldComputeSize::No;
        session->storageManager().fetchData(websiteDataTypes, shouldComputeSize, [callbackAggregator](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(WTFMove(entries));
        });
    }
}

void NetworkProcess::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    auto clearTasksHandler = WTF::CallbackAggregator::create([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
        RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteData finished deleting modified data");
    });

    RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteData started to delete data modified since %f for session %" PRIu64, modifiedSince.secondsSinceEpoch().value(), sessionID.toUInt64());
    auto* session = networkSession(sessionID);

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        clearHSTSCache(sessionID, modifiedSince);
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteAllCookiesModifiedSince(modifiedSince, [clearTasksHandler] { });
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* storage = storageSession(sessionID))
            storage->credentialStorage().clearCredentials();
        if (session)
            session->clearCredentials(modifiedSince);
    }

    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !sessionID.isEphemeral() && session) {
        session->ensureSWServer().clearAll([clearTasksHandler] { });

#if ENABLE(BUILT_IN_NOTIFICATIONS)
        session->notificationManager().removeAllPushSubscriptions([clearTasksHandler](auto&&) { });
#endif
    }

    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (auto* resourceLoadStatistics = session ? session->resourceLoadStatistics() : nullptr) {
            // If we are deleting all of the data types that the resource load statistics store monitors
            // we do not need to re-grandfather old data.
            auto shouldGrandfather = websiteDataTypes.containsAll(WebResourceLoadStatisticsStore::monitoredDataTypes()) ? ShouldGrandfatherStatistics::No : ShouldGrandfatherStatistics::Yes;
            resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince, shouldGrandfather, [clearTasksHandler] { });
        }
    }

    if (session)
        session->removeNetworkWebsiteData(modifiedSince, std::nullopt, [clearTasksHandler] { });

    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache))
        CrossOriginPreflightResultCache::singleton().clear();

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && session) {
        if (auto cache = session->cache())
            cache->clear(modifiedSince, [clearTasksHandler] { });
    }

    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements) && session)
        session->clearPrivateClickMeasurement([clearTasksHandler] { });

#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices) && session)
        session->clearAlternativeServices(modifiedSince);
#endif

    if (NetworkStorageManager::canHandleTypes(websiteDataTypes) && session)
        session->storageManager().deleteDataModifiedSince(websiteDataTypes, modifiedSince, [clearTasksHandler] { });
}

void NetworkProcess::deleteWebsiteDataForOrigin(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    auto clearTasksHandler = WTF::CallbackAggregator::create([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
        RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteDataForOrigin finished deleting data");
    });
    RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteDataForOrigin started to delete data for session %" PRIu64, sessionID.toUInt64());

    auto* session = networkSession(sessionID);
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteCookies(origin, [clearTasksHandler] { });
    }
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral()) {
        if (RefPtr cache = session->cache()) {
            Vector<NetworkCache::Key> cacheKeysToDelete;
            String cachePartition = origin.clientOrigin == origin.topOrigin ? emptyString() : ResourceRequest::partitionName(origin.topOrigin.host());
            bool shouldClearAllEntriesInPartition = origin.clientOrigin == origin.topOrigin;
            cache->traverse(cachePartition, [cache, clearTasksHandler, shouldClearAllEntriesInPartition, origin = origin.clientOrigin, cachePartition, cacheKeysToDelete = WTFMove(cacheKeysToDelete)](auto* traversalEntry) mutable {
                if (traversalEntry) {
                    ASSERT_UNUSED(cachePartition, equalIgnoringNullity(traversalEntry->entry.key().partition(), cachePartition));
                    if (shouldClearAllEntriesInPartition || SecurityOriginData::fromURLWithoutStrictOpaqueness(traversalEntry->entry.response().url()) == origin)
                        cacheKeysToDelete.append(traversalEntry->entry.key());
                    return;
                }

                cache->remove(cacheKeysToDelete, [clearTasksHandler] { });
                return;
            });
        }
    }
    if (NetworkStorageManager::canHandleTypes(websiteDataTypes) && session)
        session->storageManager().deleteData(websiteDataTypes, origin, [clearTasksHandler] { });

    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !sessionID.isEphemeral() && session)
        session->ensureSWServer().clear(origin, [clearTasksHandler] { });
}

void NetworkProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& originDatas, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, const Vector<RegistrableDomain>& registrableDomains, CompletionHandler<void()>&& completionHandler)
{
    auto clearTasksHandler = WTF::CallbackAggregator::create([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
        RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteDataForOrigins finished deleting data");
    });

    RELEASE_LOG(Storage, "NetworkProcess::deleteWebsiteDataForOrigins started to delete data for session %" PRIu64, sessionID.toUInt64());
    auto* session = networkSession(sessionID);

    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID))
            networkStorageSession->deleteCookiesForHostnames(cookieHostNames, [clearTasksHandler] { });
    }

#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        deleteHSTSCacheForHostNames(sessionID, HSTSCacheHostNames);
#endif

#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices) && session) {
        auto hosts = originDatas.map([](auto& originData) {
            return originData.host();
        });
        session->deleteAlternativeServicesForHostNames(hosts);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements) && session) {
        for (auto& originData : originDatas)
            session->clearPrivateClickMeasurementForRegistrableDomain(RegistrableDomain::uncheckedCreateFromHost(originData.host()), [clearTasksHandler] { });
    }

    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && !sessionID.isEphemeral() && session) {
        auto& server = session->ensureSWServer();
        for (auto& originData : originDatas) {
            server.clear(originData, [clearTasksHandler] { });

#if ENABLE(BUILT_IN_NOTIFICATIONS)
            session->notificationManager().removePushSubscriptionsForOrigin(SecurityOriginData { originData }, [clearTasksHandler](auto&&) { });
#endif
        }
    }

    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache))
        CrossOriginPreflightResultCache::singleton().clear();

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && session) {
        if (auto cache = session->cache())
            cache->deleteData(originDatas, [clearTasksHandler] { });
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* storage = storageSession(sessionID)) {
            for (auto& originData : originDatas)
                storage->credentialStorage().removeCredentialsWithOrigin(originData);
        }
        if (session)
            session->removeCredentialsForOrigins(originDatas);
    }

    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics) && session) {
        for (auto& domain : registrableDomains) {
            if (auto* resourceLoadStatistics = session->resourceLoadStatistics())
                resourceLoadStatistics->removeDataForDomain(domain, [clearTasksHandler] { });
        }
    }

    if (NetworkStorageManager::canHandleTypes(websiteDataTypes) && session)
        session->storageManager().deleteData(websiteDataTypes, originDatas, [clearTasksHandler] { });

    if (session) {
        HashSet<WebCore::RegistrableDomain> domainsToDeleteNetworkDataFor;
        for (auto& originData : originDatas)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(originData.host()));
        for (auto& cookieHostName : cookieHostNames)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(cookieHostName));
        for (auto& cacheHostName : HSTSCacheHostNames)
            domainsToDeleteNetworkDataFor.add(WebCore::RegistrableDomain::uncheckedCreateFromHost(cacheHostName));
        for (auto& domain : registrableDomains)
            domainsToDeleteNetworkDataFor.add(domain);

        session->removeNetworkWebsiteData(std::nullopt, WTFMove(domainsToDeleteNetworkDataFor), [clearTasksHandler] { });
    }
}

static Vector<String> filterForRegistrableDomains(const Vector<RegistrableDomain>& registrableDomains, const HashSet<String>& foundValues)
{
    Vector<String> result;
    for (const auto& value : foundValues) {
        if (registrableDomains.contains(RegistrableDomain::uncheckedCreateFromHost(value)))
            result.append(value);
    }
    
    return result;
}

static Vector<WebCore::SecurityOriginData> filterForRegistrableDomains(const HashSet<WebCore::SecurityOriginData>& origins, const Vector<RegistrableDomain>& domainsToDelete, HashSet<RegistrableDomain>& domainsDeleted)
{
    Vector<SecurityOriginData> originsDeleted;
    for (const auto& origin : origins) {
        auto domain = RegistrableDomain::uncheckedCreateFromHost(origin.host());
        if (!domainsToDelete.contains(domain))
            continue;
        originsDeleted.append(origin);
        domainsDeleted.add(domain);
    }

    return originsDeleted;
}

void NetworkProcess::deleteAndRestrictWebsiteDataForRegistrableDomains(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domains, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    RELEASE_LOG(Storage, "NetworkProcess::deleteAndRestrictWebsiteDataForRegistrableDomains started to delete and restrict data for session %" PRIu64 " - %zu domainsToDeleteAllCookiesFor, %zu domainsToDeleteAllButHttpOnlyCookiesFor, %zu domainsToDeleteAllScriptWrittenStorageFor", sessionID.toUInt64(), domains.domainsToDeleteAllCookiesFor.size(), domains.domainsToDeleteAllButHttpOnlyCookiesFor.size(), domains.domainsToDeleteAllScriptWrittenStorageFor.size());
    auto* session = networkSession(sessionID);

    OptionSet<WebsiteDataFetchOption> fetchOptions = WebsiteDataFetchOption::DoNotCreateProcesses;

    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }
        
        ~CallbackAggregator()
        {
            RunLoop::main().dispatch([completionHandler = WTFMove(m_completionHandler), domains = WTFMove(m_domains)] () mutable {
                RELEASE_LOG(Storage, "NetworkProcess::deleteAndRestrictWebsiteDataForRegistrableDomains finished deleting and restricting data");
                completionHandler(WTFMove(domains));
            });
        }
        
        CompletionHandler<void(HashSet<RegistrableDomain>&&)> m_completionHandler;
        HashSet<RegistrableDomain> m_domains;
    };
    
    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, completionHandler = WTFMove(completionHandler), shouldNotifyPage] (HashSet<RegistrableDomain>&& domainsWithData) mutable {
        if (shouldNotifyPage)
            parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyWebsiteDataDeletionForRegistrableDomainsFinished(), 0);
        
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), domainsWithData = crossThreadCopy(WTFMove(domainsWithData))] () mutable {
            completionHandler(WTFMove(domainsWithData));
        });
    }));

    HashSet<String> hostNamesWithCookies;
    HashSet<String> hostNamesWithHSTSCache;
    Vector<String> hostnamesWithCookiesToDelete;
    Vector<String> hostnamesWithScriptWrittenCookiesToDelete;
    auto domainsToDeleteAllScriptWrittenStorageFor = domains.domainsToDeleteAllScriptWrittenStorageFor;
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = storageSession(sessionID)) {
            networkStorageSession->getHostnamesWithCookies(hostNamesWithCookies);

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domains.domainsToDeleteAllCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::Yes, ScriptWrittenCookiesOnly::No, [callbackAggregator] { });

#if ENABLE(JS_COOKIE_CHECKING)
            hostnamesWithScriptWrittenCookiesToDelete = filterForRegistrableDomains(domains.domainsToDeleteAllScriptWrittenStorageFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithScriptWrittenCookiesToDelete, WebCore::IncludeHttpOnlyCookies::No, ScriptWrittenCookiesOnly::Yes, [callbackAggregator] { });
#endif
            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

            hostnamesWithCookiesToDelete = filterForRegistrableDomains(domains.domainsToDeleteAllButHttpOnlyCookiesFor, hostNamesWithCookies);
            networkStorageSession->deleteCookiesForHostnames(hostnamesWithCookiesToDelete, WebCore::IncludeHttpOnlyCookies::No, ScriptWrittenCookiesOnly::No, [callbackAggregator] { });

            for (const auto& host : hostnamesWithCookiesToDelete)
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));
        }
    }

    Vector<String> hostnamesWithHSTSToDelete;
#if PLATFORM(COCOA) || USE(SOUP)
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache)) {
        hostNamesWithHSTSCache = this->hostNamesWithHSTSCache(sessionID);
        hostnamesWithHSTSToDelete = filterForRegistrableDomains(domainsToDeleteAllScriptWrittenStorageFor, hostNamesWithHSTSCache);

        for (const auto& host : hostnamesWithHSTSToDelete)
            callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(host));

        deleteHSTSCacheForHostNames(sessionID, hostnamesWithHSTSToDelete);
    }
#endif

#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices) && session) {
        auto registrableDomainsToDelete = domainsToDeleteAllScriptWrittenStorageFor.map([](auto& domain) {
            return domain.string();
        });
        session->deleteAlternativeServicesForHostNames(registrableDomainsToDelete);
    }
#endif

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (auto* session = storageSession(sessionID)) {
            auto origins = session->credentialStorage().originsWithCredentials();
            auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllScriptWrittenStorageFor, callbackAggregator->m_domains);
            for (auto& origin : originsToDelete)
                session->credentialStorage().removeCredentialsWithOrigin(origin);
        }

        if (session) {
            auto origins = session->originsWithCredentials();
            auto originsToDelete = filterForRegistrableDomains(origins, domainsToDeleteAllScriptWrittenStorageFor, callbackAggregator->m_domains);
            session->removeCredentialsForOrigins(originsToDelete);
        }
    }
    
    bool clearServiceWorkers = websiteDataTypes.contains(WebsiteDataType::DOMCache) || websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations);
    if (clearServiceWorkers && session && session->hasServiceWorkerDatabasePath()) {
        session->ensureSWServer().getOriginsWithRegistrations([domainsToDeleteAllScriptWrittenStorageFor, callbackAggregator, session = WeakPtr { *session }](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins) {
                if (!domainsToDeleteAllScriptWrittenStorageFor.contains(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host())))
                    continue;
                callbackAggregator->m_domains.add(RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host()));
                if (session) {
                    session->ensureSWServer().clear(securityOrigin, [callbackAggregator] { });

#if ENABLE(BUILT_IN_NOTIFICATIONS)
                    session->notificationManager().removePushSubscriptionsForOrigin(SecurityOriginData { securityOrigin }, [callbackAggregator](auto&&) { });
#endif
                }
            }
        });
    }

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && session) {
        if (auto cache = session->cache()) {
            cache->deleteDataForRegistrableDomains(domainsToDeleteAllScriptWrittenStorageFor, [callbackAggregator](auto&& deletedDomains) mutable {
                for (auto domain : deletedDomains)
                    callbackAggregator->m_domains.add(WTFMove(domain));
            });
        }
    }

    if (NetworkStorageManager::canHandleTypes(websiteDataTypes) && session) {
        session->storageManager().deleteDataForRegistrableDomains(websiteDataTypes, domainsToDeleteAllScriptWrittenStorageFor, [callbackAggregator](auto&& deletedDomains) mutable {
            for (auto domain : deletedDomains)
                callbackAggregator->m_domains.add(WTFMove(domain));
        });
    }

    auto dataTypesForUIProcess = WebsiteData::filter(websiteDataTypes, WebsiteDataProcessType::UI);
    if (!dataTypesForUIProcess.isEmpty() && !domainsToDeleteAllScriptWrittenStorageFor.isEmpty()) {
        CompletionHandler<void(const HashSet<RegistrableDomain>&)> completionHandler = [callbackAggregator] (const HashSet<RegistrableDomain>& domains) {
            for (auto& domain : domains)
                callbackAggregator->m_domains.add(domain);
        };
        parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::DeleteWebsiteDataInUIProcessForRegistrableDomains(sessionID, dataTypesForUIProcess, fetchOptions, domainsToDeleteAllScriptWrittenStorageFor), WTFMove(completionHandler));
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

    deleteAndRestrictWebsiteDataForRegistrableDomains(sessionID, cookieType, WTFMove(toDeleteFor), true, [completionHandler = WTFMove(completionHandler)] (HashSet<RegistrableDomain>&& domainsDeletedFor) mutable {
        UNUSED_PARAM(domainsDeletedFor);
        completionHandler();
    });
}

void NetworkProcess::registrableDomainsWithWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    auto* session = networkSession(sessionID);
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
                    domains.add(RegistrableDomain::uncheckedCreateFromHost(entry.origin.host()));

                completionHandler(WTFMove(domains));
            });
        }
        
        CompletionHandler<void(HashSet<RegistrableDomain>&&)> m_completionHandler;
        WebsiteData m_websiteData;
    };
    
    auto callbackAggregator = adoptRef(*new CallbackAggregator([this, completionHandler = WTFMove(completionHandler), shouldNotifyPage] (HashSet<RegistrableDomain>&& domainsWithData) mutable {
        if (shouldNotifyPage)
            parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyWebsiteDataScanForRegistrableDomainsFinished(), 0);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), domainsWithData = crossThreadCopy(WTFMove(domainsWithData))] () mutable {
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

        if (session) {
            for (auto origin : session->originsWithCredentials())
                callbackAggregator->m_websiteData.entries.append({ origin, WebsiteDataType::Credentials, 0 });
        }
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && session && session->hasServiceWorkerDatabasePath()) {
        session->ensureSWServer().getOriginsWithRegistrations([callbackAggregator](const HashSet<SecurityOriginData>& securityOrigins) mutable {
            for (auto& securityOrigin : securityOrigins)
                callbackAggregator->m_websiteData.entries.append({ securityOrigin, WebsiteDataType::ServiceWorkerRegistrations, 0 });
        });
    }
    
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && session) {
        if (auto cache = session->cache()) {
            cache->fetchData(false, [callbackAggregator](auto entries) mutable {
                callbackAggregator->m_websiteData.entries.appendVector(entries);
            });
        }
    }

    if (session) {
        session->storageManager().fetchData(websiteDataTypes, NetworkStorageManager::ShouldComputeSize::No, [callbackAggregator](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(WTFMove(entries));
        });
    }
}

void NetworkProcess::closeITPDatabase(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        session->destroyResourceLoadStatistics(WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

void NetworkProcess::downloadRequest(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedFilename)
{
    downloadManager().startDownload(sessionID, downloadID, request, topOrigin, isNavigatingToAppBoundDomain, suggestedFilename);
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

void NetworkProcess::findPendingDownloadLocation(NetworkDataTask& networkDataTask, ResponseCompletionHandler&& completionHandler, const ResourceResponse& response)
{
    uint64_t destinationID = networkDataTask.pendingDownloadID().toUInt64();

    String suggestedFilename = networkDataTask.suggestedFilename();

    downloadProxyConnection()->sendWithAsyncReply(Messages::DownloadProxy::DecideDestinationWithSuggestedFilename(response, suggestedFilename), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), networkDataTask = Ref { networkDataTask }] (String&& destination, SandboxExtension::Handle&& sandboxExtensionHandle, AllowOverwrite allowOverwrite) mutable {
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

void NetworkProcess::dataTaskWithRequest(WebPageProxyIdentifier pageID, PAL::SessionID sessionID, WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, IPC::FormDataReference&& httpBody, CompletionHandler<void(DataTaskIdentifier)>&& completionHandler)
{
    request.setHTTPBody(httpBody.takeData());
    networkSession(sessionID)->dataTaskWithRequest(pageID, WTFMove(request), topOrigin, WTFMove(completionHandler));
}

void NetworkProcess::cancelDataTask(DataTaskIdentifier identifier, PAL::SessionID sessionID)
{
    if (auto* session = networkSession(sessionID))
        session->cancelDataTask(identifier);
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

void NetworkProcess::allowTLSCertificateChainForLocalPCMTesting(PAL::SessionID sessionID, const WebCore::CertificateInfo& certificateInfo)
{
    if (auto* session = networkSession(sessionID))
        session->allowTLSCertificateChainForLocalPCMTesting(certificateInfo);
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

void NetworkProcess::processWillSuspendImminentlyForTestingSync(CompletionHandler<void()>&& completionHandler)
{
    prepareToSuspend(true, MonotonicTime::now(), WTFMove(completionHandler));
}

void NetworkProcess::terminateRemoteWorkerContextConnectionWhenPossible(RemoteWorkerType workerType, PAL::SessionID sessionID, const WebCore::RegistrableDomain& registrableDomain, WebCore::ProcessIdentifier processIdentifier)
{
    auto* session = networkSession(sessionID);
    if (!session)
        return;

    switch (workerType) {
    case RemoteWorkerType::ServiceWorker:
        if (auto* swServer = session->swServer())
            swServer->terminateContextConnectionWhenPossible(registrableDomain, processIdentifier);
        break;
    case RemoteWorkerType::SharedWorker:
        if (auto* sharedWorkerServer = session->sharedWorkerServer())
            sharedWorkerServer->terminateContextConnectionWhenPossible(registrableDomain, processIdentifier);
        break;
    }
}

void NetworkProcess::prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&& completionHandler)
{
#if !RELEASE_LOG_DISABLED
    auto nowTime = MonotonicTime::now();
    double remainingRunTime = estimatedSuspendTime > nowTime ? (estimatedSuspendTime - nowTime).value() : 0.0;
#endif
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend(), isSuspensionImminent=%d, remainingRunTime=%fs", this, isSuspensionImminent, remainingRunTime);

    m_isSuspended = true;
    lowMemoryHandler(Critical::Yes);

    RefPtr<CallbackAggregator> callbackAggregator = CallbackAggregator::create([this, completionHandler = WTFMove(completionHandler)]() mutable {
        RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend() Process is ready to suspend", this);
        UNUSED_VARIABLE(this);
        completionHandler();
    });
    
    WebResourceLoadStatisticsStore::suspend([callbackAggregator] { });
    PCM::PersistentStore::prepareForProcessToSuspend([callbackAggregator] { });

    forEachNetworkSession([&] (auto& session) {
        platformFlushCookies(session.sessionID(), [callbackAggregator] { });
        session.storageManager().suspend([callbackAggregator] { });
    });

    for (auto& storageManager : m_closingStorageManagers)
        storageManager->suspend([callbackAggregator] { });
}

void NetworkProcess::applicationDidEnterBackground()
{
    m_downloadManager.applicationDidEnterBackground();
}

void NetworkProcess::applicationWillEnterForeground()
{
    m_downloadManager.applicationWillEnterForeground();
}

void NetworkProcess::processDidResume(bool forForegroundActivity)
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processDidResume() forForegroundActivity=%d", this, forForegroundActivity);

    m_isSuspended = false;

    for (auto& connection : m_webProcessConnections.values())
        connection->endSuspension();

    WebResourceLoadStatisticsStore::resume();
    PCM::PersistentStore::processDidResume();

    forEachNetworkSession([](auto& session) {
        session.storageManager().resume();
    });

    for (auto& storageManager : m_closingStorageManagers)
        storageManager->resume();
}

void NetworkProcess::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
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

void NetworkProcess::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachNetworkSession([&](auto& session) {
        session.storageManager().syncLocalStorage([aggregator] { });
    });
}

void NetworkProcess::storeServiceWorkerRegistrations(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    auto* session = networkSession(sessionID);
    auto* server = session ? session->swServer() : nullptr;
    if (!server)
        return completionHandler();

    server->storeRegistrationsOnDisk(WTFMove(completionHandler));
}

void NetworkProcess::resetQuota(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        return session->storageManager().resetQuotaForTesting(WTFMove(completionHandler));

    completionHandler();
}

void NetworkProcess::setOriginQuotaRatioEnabledForTesting(PAL::SessionID sessionID, bool enabled, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        return session->storageManager().setOriginQuotaRatioEnabledForTesting(enabled, WTFMove(completionHandler));

    completionHandler();
}

void NetworkProcess::resetStoragePersistedState(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->storageManager().resetStoragePersistedState(WTFMove(completionHandler));
    else
        completionHandler();
}

void NetworkProcess::cloneSessionStorageForWebPage(PAL::SessionID sessionID, WebPageProxyIdentifier sourcePage, WebPageProxyIdentifier destinationPage)
{
    if (auto* session = networkSession(sessionID))
        session->storageManager().cloneSessionStorageForWebPage(sourcePage, destinationPage);
}

void NetworkProcess::didIncreaseQuota(PAL::SessionID sessionID, ClientOrigin&& origin, QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    if (auto* session = networkSession(sessionID))
        session->storageManager().didIncreaseQuota(WTFMove(origin), identifier, newQuota);
}

void NetworkProcess::renameOriginInWebsiteData(PAL::SessionID sessionID, SecurityOriginData&& oldOrigin, SecurityOriginData&& newOrigin, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void()>&& completionHandler)
{
    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));

    if (oldOrigin.isNull() || newOrigin.isNull())
        return;

    if (auto* session = networkSession(sessionID))
        session->storageManager().moveData(dataTypes, WTFMove(oldOrigin), WTFMove(newOrigin), [aggregator] { });
}

void NetworkProcess::websiteDataOriginDirectoryForTesting(PAL::SessionID sessionID, ClientOrigin&& origin, OptionSet<WebsiteDataType> dataType, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!dataType.hasExactlyOneBitSet()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    auto* session = networkSession(sessionID);
    if (!session)
        return completionHandler({ });

    session->storageManager().getOriginDirectory(WTFMove(origin), *dataType.toSingleValue(), WTFMove(completionHandler));
}

void NetworkProcess::processNotificationEvent(NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    auto* session = networkSession(data.sourceSession);
    if (!session) {
        callback(false);
        return;
    }

    session->ensureSWServer().processNotificationEvent(WTFMove(data), eventType, WTFMove(callback));
}

void NetworkProcess::getAllBackgroundFetchIdentifiers(PAL::SessionID sessionID, CompletionHandler<void(Vector<String>&&)>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback({ });
        return;
    }

    session->getAllBackgroundFetchIdentifiers(WTFMove(callback));
}

void NetworkProcess::getBackgroundFetchState(PAL::SessionID sessionID, const String& identifier, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback({ });
        return;
    }

    session->getBackgroundFetchState(identifier, WTFMove(callback));
}

void NetworkProcess::abortBackgroundFetch(PAL::SessionID sessionID, const String& identifier, CompletionHandler<void()>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback();
        return;
    }

    session->abortBackgroundFetch(identifier, WTFMove(callback));
}

void NetworkProcess::pauseBackgroundFetch(PAL::SessionID sessionID, const String& identifier, CompletionHandler<void()>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback();
        return;
    }

    session->pauseBackgroundFetch(identifier, WTFMove(callback));
}

void NetworkProcess::resumeBackgroundFetch(PAL::SessionID sessionID, const String& identifier, CompletionHandler<void()>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback();
        return;
    }

    session->resumeBackgroundFetch(identifier, WTFMove(callback));
}

void NetworkProcess::clickBackgroundFetch(PAL::SessionID sessionID, const String& identifier, CompletionHandler<void()>&& callback)
{
    auto* session = networkSession(sessionID);
    if (!session) {
        callback();
        return;
    }

    session->clickBackgroundFetch(identifier, WTFMove(callback));
}
#if ENABLE(BUILT_IN_NOTIFICATIONS)

void NetworkProcess::getPendingPushMessages(PAL::SessionID sessionID, CompletionHandler<void(const Vector<WebPushMessage>&)>&& callback)
{
    if (auto* session = networkSession(sessionID)) {
        LOG(Notifications, "NetworkProcess getting pending push messages for session ID %" PRIu64, sessionID.toUInt64());
        session->notificationManager().getPendingPushMessages(WTFMove(callback));
        return;
    } else
        LOG(Notifications, "NetworkProcess could not find session for ID %llu to get pending push messages", sessionID.toUInt64());
}

void NetworkProcess::processPushMessage(PAL::SessionID sessionID, WebPushMessage&& pushMessage, PushPermissionState permissionState, CompletionHandler<void(bool, std::optional<WebCore::NotificationPayload>&&)>&& callback)
{
    if (auto* session = networkSession(sessionID)) {
        RELEASE_LOG(Push, "Networking process handling a push message from UI process in session %llu", sessionID.toUInt64());
        auto origin = SecurityOriginData::fromURL(pushMessage.registrationURL);

        if (permissionState == PushPermissionState::Prompt) {
            RELEASE_LOG(Push, "Push message from %" SENSITIVE_LOG_STRING " won't be processed since permission is in the prompt state; removing push subscription", origin.toString().utf8().data());
            session->notificationManager().removePushSubscriptionsForOrigin(SecurityOriginData { origin }, [callback = WTFMove(callback)](auto&&) mutable {
                callback(false, std::nullopt);
            });
            return;
        }

        if (permissionState == PushPermissionState::Denied) {
            RELEASE_LOG(Push, "Push message from %" SENSITIVE_LOG_STRING " won't be processed since permission is in the denied state", origin.toString().utf8().data());
            // FIXME: move topic to ignore list in webpushd if permission is denied.
            callback(false, std::nullopt);
            return;
        }

        ASSERT(permissionState == PushPermissionState::Granted);
        auto scope = pushMessage.registrationURL.string();
        bool isDeclarative = !!pushMessage.notificationPayload;
        session->ensureSWServer().processPushMessage(WTFMove(pushMessage.pushData), WTFMove(pushMessage.notificationPayload), WTFMove(pushMessage.registrationURL), [this, protectedThis = Ref { *this }, sessionID, origin = WTFMove(origin), scope = WTFMove(scope), callback = WTFMove(callback), isDeclarative](bool result, std::optional<WebCore::NotificationPayload>&& resultPayload) mutable {
            if (!isDeclarative && !result) {
                if (auto* session = networkSession(sessionID)) {
                    session->notificationManager().incrementSilentPushCount(WTFMove(origin), [scope = WTFMove(scope), callback = WTFMove(callback), result](unsigned newSilentPushCount) mutable {
                        RELEASE_LOG_ERROR(Push, "Push message for scope %" SENSITIVE_LOG_STRING " not handled properly; new silent push count: %u", scope.utf8().data(), newSilentPushCount);
                        callback(result, std::nullopt);
                    });
                    return;
                }
            }

            callback(result, WTFMove(resultPayload));
        });
    } else {
        RELEASE_LOG_ERROR(Push, "Networking process asked to handle a push message from UI process in session %llu, but that session doesn't exist", sessionID.toUInt64());
        callback(false, WTFMove(pushMessage.notificationPayload));
    }
}

#else

void NetworkProcess::getPendingPushMessages(PAL::SessionID, CompletionHandler<void(const Vector<WebPushMessage>&)>&& callback)
{
    callback({ });
}

void NetworkProcess::processPushMessage(PAL::SessionID, WebPushMessage&&, PushPermissionState, CompletionHandler<void(bool, std::optional<WebCore::NotificationPayload>&&)>&& callback)
{
    callback(false, std::nullopt);
}

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)

void NetworkProcess::setPushAndNotificationsEnabledForOrigin(PAL::SessionID sessionID, const SecurityOriginData& origin, bool enabled, CompletionHandler<void()>&& callback)
{
#if ENABLE(BUILT_IN_NOTIFICATIONS)
    if (auto* session = networkSession(sessionID)) {
        session->notificationManager().setPushAndNotificationsEnabledForOrigin(origin, enabled, WTFMove(callback));
        return;
    }
#endif
    callback();
}

void NetworkProcess::deletePushAndNotificationRegistration(PAL::SessionID sessionID, const SecurityOriginData& origin, CompletionHandler<void(const String&)>&& callback)
{
#if ENABLE(BUILT_IN_NOTIFICATIONS)
    if (auto* session = networkSession(sessionID)) {
        session->notificationManager().deletePushAndNotificationRegistration(origin, WTFMove(callback));
        return;
    }
#endif
    callback("Cannot find network session"_s);
}

void NetworkProcess::hasPushSubscriptionForTesting(PAL::SessionID sessionID, URL&& scopeURL, CompletionHandler<void(bool)>&& callback)
{
#if ENABLE(BUILT_IN_NOTIFICATIONS)
    if (auto* session = networkSession(sessionID)) {
        session->notificationManager().getPushSubscription(WTFMove(scopeURL), [callback = WTFMove(callback)](auto &&result) mutable {
            callback(result && result->has_value());
        });
        return;
    }
#endif

    callback(false);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkProcess::setEmulatedConditions(PAL::SessionID sessionID, std::optional<int64_t>&& bytesPerSecondLimit)
{
    if (auto* session = networkSession(sessionID))
        session->setEmulatedConditions(WTFMove(bytesPerSecondLimit));
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

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

void NetworkProcess::flushCookies(PAL::SessionID, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void NetworkProcess::platformFlushCookies(PAL::SessionID, CompletionHandler<void()>&& completionHandler)
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
        session->clearPrivateClickMeasurement(WTFMove(completionHandler));
    else
        completionHandler();
}

bool NetworkProcess::allowsPrivateClickMeasurementTestFunctionality() const
{
#if !PLATFORM(COCOA) || !USE(APPLE_INTERNAL_SDK)
    return true;
#else
    auto auditToken = sourceApplicationAuditToken();
    if (!auditToken)
        return false;
    return WTF::hasEntitlement(*auditToken, "com.apple.private.webkit.adattributiond.testing"_s);
#endif
}

void NetworkProcess::setPrivateClickMeasurementOverrideTimerForTesting(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementOverrideTimerForTesting(value);
    
    completionHandler();
}

void NetworkProcess::closePCMDatabase(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        session->destroyPrivateClickMeasurementStore(WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

void NetworkProcess::simulatePrivateClickMeasurementSessionRestart(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID)) {
        session->destroyPrivateClickMeasurementStore([session = WeakPtr { *session }, completionHandler = WTFMove(completionHandler)] () mutable {
            if (session)
                session->firePrivateClickMeasurementTimerImmediatelyForTesting();
            completionHandler();
        });
        return;
    }
    completionHandler();
}

void NetworkProcess::markAttributedPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID)) {
        session->markAttributedPrivateClickMeasurementsAsExpiredForTesting(WTFMove(completionHandler));
        return;
    }
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementEphemeralMeasurementForTesting(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementEphemeralMeasurementForTesting(value);
    
    completionHandler();
}


void NetworkProcess::setPrivateClickMeasurementTokenPublicKeyURLForTesting(PAL::SessionID sessionID, URL&& url, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementTokenPublicKeyURLForTesting(WTFMove(url));

    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementTokenSignatureURLForTesting(PAL::SessionID sessionID, URL&& url, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementTokenSignatureURLForTesting(WTFMove(url));
    
    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementAttributionReportURLsForTesting(PAL::SessionID sessionID, URL&& sourceURL, URL&& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementAttributionReportURLsForTesting(WTFMove(sourceURL), WTFMove(destinationURL));

    completionHandler();
}

void NetworkProcess::markPrivateClickMeasurementsAsExpiredForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->markPrivateClickMeasurementsAsExpiredForTesting();

    completionHandler();
}

void NetworkProcess::setPCMFraudPreventionValuesForTesting(PAL::SessionID sessionID, String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPCMFraudPreventionValuesForTesting(WTFMove(unlinkableToken), WTFMove(secretToken), WTFMove(signature), WTFMove(keyID));

    completionHandler();
}

void NetworkProcess::setPrivateClickMeasurementAppBundleIDForTesting(PAL::SessionID sessionID, String&& appBundleIDForTesting, CompletionHandler<void()>&& completionHandler)
{
    if (!allowsPrivateClickMeasurementTestFunctionality())
        return completionHandler();

    if (auto* session = networkSession(sessionID))
        session->setPrivateClickMeasurementAppBundleIDForTesting(WTFMove(appBundleIDForTesting));

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

void NetworkProcess::connectionToWebProcessClosed(IPC::Connection& connection, PAL::SessionID sessionID)
{
    if (auto* session = networkSession(sessionID))
        session->storageManager().stopReceivingMessageFromConnection(connection);
}

WebCore::ProcessIdentifier NetworkProcess::webProcessIdentifierForConnection(IPC::Connection& connection) const
{
    for (auto& [processIdentifier, webConnection] : m_webProcessConnections) {
        if (&webConnection->connection() == &connection)
            return processIdentifier;
    }
    return { };
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

Seconds NetworkProcess::randomClosedPortDelay()
{
    // Random delay in the range [10ms, 110ms).
    return 10_ms + Seconds { cryptographicallyRandomUnitInterval() * (100_ms).value() };
}

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcess::hasAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler) const
{
    bool result = false;
    if (auto* session = networkSession(sessionID))
        result = session->hasAppBoundSession();
    completionHandler(result);
}

void NetworkProcess::clearAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        session->clearAppBoundSession();
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
    WebCore::setApplicationBundleIdentifierOverride(bundleIdentifier);
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

void NetworkProcess::setCORSDisablingPatterns(NetworkConnectionToWebProcess& connection, PageIdentifier pageIdentifier, Vector<String>&& patterns)
{
    auto parsedPatterns = WTF::compactMap(WTFMove(patterns), [&](auto&& pattern) -> std::optional<UserContentURLPattern> {
        UserContentURLPattern parsedPattern(WTFMove(pattern));
        if (parsedPattern.isValid()) {
            connection.originAccessPatterns().allowAccessTo(parsedPattern);
            return parsedPattern;
        }
        return std::nullopt;
    });

    parsedPatterns.shrinkToFit();

    if (parsedPatterns.isEmpty()) {
        m_extensionCORSDisablingPatterns.remove(pageIdentifier);
        return;
    }

    m_extensionCORSDisablingPatterns.set(pageIdentifier, WTFMove(parsedPatterns));
}

#if PLATFORM(COCOA)
void NetworkProcess::appPrivacyReportTestingData(PAL::SessionID sessionID, CompletionHandler<void(const AppPrivacyReportTestingData&)>&& completionHandler)
{
    if (auto* session = networkSession(sessionID)) {
        completionHandler(session->appPrivacyReportTestingData());
        return;
    }
    completionHandler({ });
}

void NetworkProcess::clearAppPrivacyReportTestingData(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = networkSession(sessionID))
        session->appPrivacyReportTestingData().clearAppPrivacyReportTestingData();

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

void NetworkProcess::addWebPageNetworkParameters(PAL::SessionID sessionID, WebPageProxyIdentifier pageID, WebPageNetworkParameters&& parameters)
{
    if (auto* session = networkSession(sessionID))
        session->addWebPageNetworkParameters(pageID, WTFMove(parameters));
}

void NetworkProcess::removeWebPageNetworkParameters(PAL::SessionID sessionID, WebPageProxyIdentifier pageID)
{
    if (auto* session = networkSession(sessionID)) {
        session->removeWebPageNetworkParameters(pageID);
        session->storageManager().clearStorageForWebPage(pageID);
    }
}

void NetworkProcess::countNonDefaultSessionSets(PAL::SessionID sessionID, CompletionHandler<void(size_t)>&& completionHandler)
{
    auto* session = networkSession(sessionID);
    completionHandler(session ? session->countNonDefaultSessionSets() : 0);
}

void NetworkProcess::requestBackgroundFetchPermission(PAL::SessionID sessionID, const ClientOrigin& origin, CompletionHandler<void(bool)>&& callback)
{
    parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestBackgroundFetchPermission(sessionID, origin), WTFMove(callback));
}

#if USE(RUNNINGBOARD)
void NetworkProcess::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
#if PLATFORM(MAC)
    // The sandbox doesn't allow the network process to talk to runningboardd on macOS.
    UNUSED_PARAM(isHoldingLockedFiles);
#else
    if (!isHoldingLockedFiles) {
        m_holdingLockedFileAssertion = nullptr;
#if USE(EXTENSIONKIT)
        invalidateGrant();
#endif
        return;
    }

    if (m_holdingLockedFileAssertion && m_holdingLockedFileAssertion->isValid())
        return;

#if USE(EXTENSIONKIT)
    if (hasAcquiredGrant())
        return;
    if (aqcuireLockedFileGrant())
        return;
#endif

    // We synchronously take a process assertion when beginning a SQLite transaction so that we don't get suspended
    // while holding a locked file. We would get killed if suspended while holding locked files.
    m_holdingLockedFileAssertion = ProcessAssertion::create(getCurrentProcessID(), "Network Process is holding locked files"_s, ProcessAssertionType::FinishTaskInterruptable, ProcessAssertion::Mode::Sync);
#endif
}
#endif

void NetworkProcess::setInspectionForServiceWorkersAllowed(PAL::SessionID sessionID, bool inspectable)
{
    if (auto* session = networkSession(sessionID))
        session->setInspectionForServiceWorkersAllowed(inspectable);
}

} // namespace WebKit
