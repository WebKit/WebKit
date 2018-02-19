/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/Settings.h>
#include <WebCore/URLParser.h>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
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
#if PLATFORM(IOS)
    , m_webSQLiteDatabaseTracker(*this)
#endif
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    addSupplement<LegacyCustomProtocolManager>();
#endif
}

NetworkProcess::~NetworkProcess()
{
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
    // The UIProcess just exited.
    stopRunLoop();
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
    WebCore::setPresentingApplicationPID(parameters.presentingApplicationPID);
    platformInitializeNetworkProcess(parameters);

    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomicString::init();

    m_suppressMemoryPressureHandler = parameters.shouldSuppressMemoryPressureHandler;
    m_loadThrottleLatency = parameters.loadThrottleLatency;
    if (!m_suppressMemoryPressureHandler) {
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
#if OS(LINUX)
        if (parameters.memoryPressureMonitorHandle.fileDescriptor() != -1)
            memoryPressureHandler.setMemoryPressureMonitorHandle(parameters.memoryPressureMonitorHandle.releaseFileDescriptor());
#endif
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

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    parameters.defaultSessionParameters.legacyCustomProtocolManager = supplement<LegacyCustomProtocolManager>();
#endif
    SessionTracker::setSession(PAL::SessionID::defaultSessionID(), NetworkSession::create(WTFMove(parameters.defaultSessionParameters)));

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);

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
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    auto connection = NetworkConnectionToWebProcess::create(IPC::Connection::Identifier(listeningPort));
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientPort), 0);
#else
    notImplemented();
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
    RemoteNetworkingContext::ensureWebsiteDataStoreSession(WTFMove(parameters));
}

void NetworkProcess::destroySession(PAL::SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
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
void NetworkProcess::updatePrevalentDomainsToPartitionOrBlockCookies(PAL::SessionID sessionID, const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, bool shouldClearFirst)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->setPrevalentDomainsToPartitionOrBlockCookies(domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst);
}

void NetworkProcess::hasStorageAccessForFrame(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, uint64_t contextId)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        parentProcessConnection()->send(Messages::NetworkProcessProxy::StorageAccessRequestResult(networkStorageSession->hasStorageAccessForFrame(resourceDomain, firstPartyDomain, frameID, pageID), contextId), 0);
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

void NetworkProcess::grantStorageAccessForFrame(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, uint64_t contextId)
{
    bool isStorageGranted = false;
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID)) {
        networkStorageSession->grantStorageAccessForFrame(resourceDomain, firstPartyDomain, frameID, pageID);
        ASSERT(networkStorageSession->hasStorageAccessForFrame(resourceDomain, firstPartyDomain, frameID, pageID));
        isStorageGranted = true;
    } else
        ASSERT_NOT_REACHED();

    parentProcessConnection()->send(Messages::NetworkProcessProxy::StorageAccessRequestResult(isStorageGranted, contextId), 0);
}

void NetworkProcess::removePrevalentDomains(PAL::SessionID sessionID, const Vector<String>& domains)
{
    if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
        networkStorageSession->removePrevalentDomains(domains);
}
#endif

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
            auto result = originsAndSizes.add({url.protocol().toString(), url.host(), url.port()}, 0);

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
        CacheStorage::Engine::from(sessionID).clearAllCaches(clearTasksHandler);

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
            CacheStorage::Engine::from(sessionID).clearCachesForOrigin(originData, clearTasksHandler);
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
static uint64_t generateCanAuthenticateIdentifier()
{
    static uint64_t lastLoaderID = 0;
    return ++lastLoaderID;
}

void NetworkProcess::canAuthenticateAgainstProtectionSpace(NetworkResourceLoader& loader, const WebCore::ProtectionSpace& protectionSpace)
{
    uint64_t loaderID = generateCanAuthenticateIdentifier();
    m_waitingNetworkResourceLoaders.set(loaderID, loader);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::CanAuthenticateAgainstProtectionSpace(loaderID, loader.pageID(), loader.frameID(), protectionSpace), 0);
}

#if ENABLE(SERVER_PRECONNECT)
void NetworkProcess::canAuthenticateAgainstProtectionSpace(PreconnectTask& preconnectTask, const WebCore::ProtectionSpace& protectionSpace)
{
    uint64_t loaderID = generateCanAuthenticateIdentifier();
    m_waitingPreconnectTasks.set(loaderID, preconnectTask.createWeakPtr());
    parentProcessConnection()->send(Messages::NetworkProcessProxy::CanAuthenticateAgainstProtectionSpace(loaderID, preconnectTask.pageID(), preconnectTask.frameID(), protectionSpace), 0);
}
#endif

void NetworkProcess::continueCanAuthenticateAgainstProtectionSpace(uint64_t loaderID, bool canAuthenticate)
{
    if (auto resourceLoader = m_waitingNetworkResourceLoaders.take(loaderID)) {
        resourceLoader.value()->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
        return;
    }
#if ENABLE(SERVER_PRECONNECT)
    if (auto preconnectTask = m_waitingPreconnectTasks.take(loaderID)) {
        preconnectTask->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
        return;
    }
#endif
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

    if (m_cache) {
        m_cache->setCapacity(urlCacheDiskCapacity);
        return;
    }

    platformSetURLCacheSize(urlCacheMemoryCapacity, urlCacheDiskCapacity);
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
    for (auto& connection : m_webProcessConnections)
        connection->endSuspension();
}

void NetworkProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processDidResume()", this);
    for (auto& connection : m_webProcessConnections)
        connection->endSuspension();
}

void NetworkProcess::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

String NetworkProcess::cacheStorageDirectory(PAL::SessionID sessionID) const
{
    if (sessionID.isEphemeral())
        return { };

    if (sessionID == PAL::SessionID::defaultSessionID())
        return m_cacheStorageDirectory;

    auto* session = NetworkStorageSession::storageSession(sessionID);
    if (!session)
        return { };

    return session->cacheStorageDirectory();
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

uint64_t NetworkProcess::cacheStoragePerOriginQuota() const
{
    return m_cacheStoragePerOriginQuota;
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

#endif

} // namespace WebKit
