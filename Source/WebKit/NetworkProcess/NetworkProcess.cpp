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
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "PreconnectTask.h"
#include "RemoteNetworkingContext.h"
#include "SessionTracker.h"
#include "StatisticsData.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
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
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/Settings.h>
#include <WebCore/URLParser.h>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#include "NetworkCache.h"
#include "NetworkCacheCoders.h"

#if ENABLE(NETWORK_CAPTURE)
#include "NetworkCaptureManager.h"
#endif

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif

using namespace WebCore;

namespace WebKit {

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
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    addSupplement<LegacyCustomProtocolManager>();
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
    WebCore::setPresentingApplicationPID(parameters.presentingApplicationPID);
    platformInitializeNetworkProcess(parameters);

    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomicString::init();

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    m_loadThrottleLatency = parameters.loadThrottleLatency;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([this] (Critical critical, Synchronous) {
            lowMemoryHandler(critical);
        });
        memoryPressureHandler.install();
    }

#if ENABLE(NETWORK_CAPTURE)
    NetworkCapture::Manager::singleton().initialize(
        parameters.recordReplayMode,
        parameters.recordReplayCacheLocation);
#endif

    m_diskCacheIsDisabledForTesting = parameters.shouldUseTestingNetworkSession;

    m_diskCacheSizeOverride = parameters.diskCacheSizeOverride;
    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    setCanHandleHTTPSServerTrustEvaluation(parameters.canHandleHTTPSServerTrustEvaluation);

    // FIXME: instead of handling this here, a message should be sent later (scales to multiple sessions)
    if (parameters.privateBrowsingEnabled)
        RemoteNetworkingContext::ensureWebsiteDataStoreSession(WebsiteDataStoreParameters::legacyPrivateSessionParameters());

    if (parameters.shouldUseTestingNetworkSession)
        NetworkStorageSession::switchToNewTestingSession();

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    m_logCookieInformation = parameters.logCookieInformation;
#endif

    SessionTracker::setSession(PAL::SessionID::defaultSessionID(), NetworkSession::create(WTFMove(parameters.defaultSessionParameters)));

    auto* defaultSession = SessionTracker::networkSession(PAL::SessionID::defaultSessionID());
    for (const auto& cookie : parameters.defaultSessionPendingCookies)
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

    for (auto& supplement : m_supplements.values())
        supplement->initializeConnection(connection);
}

void NetworkProcess::createNetworkConnectionToWebProcess()
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
    RemoteNetworkingContext::ensureWebsiteDataStoreSession(WTFMove(parameters));
}

void NetworkProcess::destroySession(PAL::SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
    m_sessionsControlledByAutomation.remove(sessionID);
    CacheStorage::Engine::destroyEngine(sessionID);
}

void NetworkProcess::grantSandboxExtensionsToStorageProcessForBlobs(const Vector<String>& filenames, Function<void ()>&& completionHandler)
{
    static uint64_t lastRequestID;

    uint64_t requestID = ++lastRequestID;
    m_sandboxExtensionForBlobsCompletionHandlers.set(requestID, WTFMove(completionHandler));
    parentProcessConnection()->send(Messages::NetworkProcessProxy::GrantSandboxExtensionsToStorageProcessForBlobs(requestID, filenames), 0);
}

void NetworkProcess::didGrantSandboxExtensionsToStorageProcessForBlobs(uint64_t requestID)
{
    if (auto handler = m_sandboxExtensionForBlobsCompletionHandlers.take(requestID))
        handler();
}

void NetworkProcess::writeBlobToFilePath(const WebCore::URL& url, const String& path, SandboxExtension::Handle&& handleForWriting, uint64_t requestID)
{
    auto extension = SandboxExtension::create(WTFMove(handleForWriting));
    if (!extension) {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidWriteBlobToFilePath(false, requestID), 0);
        return;
    }

    extension->consume();
    NetworkBlobRegistry::singleton().writeBlobToFilePath(url, path, [this, extension = WTFMove(extension), requestID] (bool success) {
        extension->revoke();
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidWriteBlobToFilePath(success, requestID), 0);
    });
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void NetworkProcess::updatePrevalentDomainsToPartitionOrBlockCookies(PAL::SessionID sessionID, const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, bool shouldClearFirst, uint64_t callbackId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->setPrevalentDomainsToPartitionOrBlockCookies(domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidUpdatePartitionOrBlockCookies(callbackId), 0);
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

void NetworkProcess::grantStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID, uint64_t contextId)
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

void NetworkProcess::removeAllStorageAccess(PAL::SessionID sessionID)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->removeAllStorageAccess();
    else
        ASSERT_NOT_REACHED();
}

void NetworkProcess::removePrevalentDomains(PAL::SessionID sessionID, const Vector<String>& domains)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->removePrevalentDomains(domains);
}
#endif

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
            getHostnamesWithCookies(*networkStorageSession, callbackAggregator->m_websiteData.hostNamesWithCookies);
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
            deleteAllCookiesModifiedSince(*networkStorageSession, modifiedSince);
    }

    if (websiteDataTypes.contains(WebsiteDataType::Credentials)) {
        if (NetworkStorageSession::storageSession(sessionID))
            NetworkStorageSession::storageSession(sessionID)->credentialStorage().clearCredentials();
    }

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        CacheStorage::Engine::clearAllCaches(sessionID, clearTasksHandler.copyRef());

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

void NetworkProcess::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& originDatas, const Vector<String>& cookieHostNames, uint64_t callbackID)
{
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            deleteCookiesForHostnames(*networkStorageSession, cookieHostNames);
    }

    auto clearTasksHandler = WTF::CallbackAggregator::create([this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    });

    if (websiteDataTypes.contains(WebsiteDataType::DOMCache)) {
        for (auto& originData : originDatas)
            CacheStorage::Engine::clearCachesForOrigin(sessionID, SecurityOriginData { originData }, clearTasksHandler.copyRef());
    }

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
    
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkProcess::canAuthenticateAgainstProtectionSpace(const WebCore::ProtectionSpace& protectionSpace, uint64_t pageID, uint64_t frameID, CompletionHandler<void(bool)>&& completionHandler)
{
    static uint64_t lastCompletionHandlerID = 0;
    uint64_t completionHandlerID = ++lastCompletionHandlerID;
    m_canAuthenticateAgainstProtectionSpaceCompletionHandlers.add(completionHandlerID, WTFMove(completionHandler));
    parentProcessConnection()->send(Messages::NetworkProcessProxy::CanAuthenticateAgainstProtectionSpace(completionHandlerID, pageID, frameID, protectionSpace), 0);
}

void NetworkProcess::continueCanAuthenticateAgainstProtectionSpace(uint64_t completionHandlerID, bool canAuthenticate)
{
    if (auto completionHandler = m_canAuthenticateAgainstProtectionSpaceCompletionHandlers.take(completionHandlerID)) {
        completionHandler(canAuthenticate);
        return;
    }
    ASSERT_NOT_REACHED();
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

    if (m_diskCacheSizeOverride >= 0)
        urlCacheDiskCapacity = m_diskCacheSizeOverride;

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
#if ENABLE(NETWORK_CAPTURE)
    NetworkCapture::Manager::singleton().terminate();
#endif

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

void NetworkProcess::preconnectTo(const WebCore::URL& url, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
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
