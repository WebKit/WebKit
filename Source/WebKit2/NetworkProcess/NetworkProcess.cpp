/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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
#include "CustomProtocolManager.h"
#include "DataReference.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessPlatformStrategies.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "RemoteNetworkingContext.h"
#include "SessionTracker.h"
#include "StatisticsData.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataType.h"
#include <WebCore/DNS.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SessionID.h>
#include <WebCore/URLParser.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#if ENABLE(NETWORK_CACHE)
#include "NetworkCache.h"
#include "NetworkCacheCoders.h"
#endif

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
    addSupplement<CustomProtocolManager>();
#if USE(NETWORK_SESSION) && PLATFORM(COCOA)
    NetworkSessionCocoa::setCustomProtocolManager(supplement<CustomProtocolManager>());
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
    platformInitializeNetworkProcess(parameters);

    WTF::setCurrentThreadIsUserInitiated();

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
        RemoteNetworkingContext::ensurePrivateBrowsingSession(SessionID::legacyPrivateSessionID());

    if (parameters.shouldUseTestingNetworkSession)
        NetworkStorageSession::switchToNewTestingSession();

    for (auto& supplement : m_supplements.values())
        supplement->initialize(parameters);
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
#if USE(NETWORK_SESSION)
    NetworkSession::defaultSession().clearCredentials();
#endif
}

void NetworkProcess::ensurePrivateBrowsingSession(SessionID sessionID)
{
    RemoteNetworkingContext::ensurePrivateBrowsingSession(sessionID);
}

void NetworkProcess::destroyPrivateBrowsingSession(SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
}

void NetworkProcess::grantSandboxExtensionsToDatabaseProcessForBlobs(const Vector<String>& filenames, Function<void ()>&& completionHandler)
{
    static uint64_t lastRequestID;

    uint64_t requestID = ++lastRequestID;
    m_sandboxExtensionForBlobsCompletionHandlers.set(requestID, WTFMove(completionHandler));
    parentProcessConnection()->send(Messages::NetworkProcessProxy::GrantSandboxExtensionsToDatabaseProcessForBlobs(requestID, filenames), 0);
}

void NetworkProcess::didGrantSandboxExtensionsToDatabaseProcessForBlobs(uint64_t requestID)
{
    if (auto handler = m_sandboxExtensionForBlobsCompletionHandlers.take(requestID))
        handler();
}

static void fetchDiskCacheEntries(SessionID sessionID, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void (Vector<WebsiteData::Entry>)>&& completionHandler)
{
#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        HashMap<SecurityOriginData, uint64_t> originsAndSizes;
        NetworkCache::singleton().traverse([fetchOptions, completionHandler = WTFMove(completionHandler), originsAndSizes = WTFMove(originsAndSizes)](auto* traversalEntry) mutable {
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
#endif

    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
        completionHandler({ });
    });
}

void NetworkProcess::fetchWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, uint64_t callbackID)
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

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache)) {
        fetchDiskCacheEntries(sessionID, fetchOptions, [callbackAggregator = WTFMove(callbackAggregator)](auto entries) mutable {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
}

void NetworkProcess::deleteWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
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

    auto completionHandler = [this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    };

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral()) {
        clearDiskCache(modifiedSince, WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

static void clearDiskCacheEntries(const Vector<SecurityOriginData>& origins, Function<void ()>&& completionHandler)
{
#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        HashSet<RefPtr<SecurityOrigin>> originsToDelete;
        for (auto& origin : origins)
            originsToDelete.add(origin.securityOrigin());

        Vector<NetworkCache::Key> cacheKeysToDelete;
        NetworkCache::singleton().traverse([completionHandler = WTFMove(completionHandler), originsToDelete = WTFMove(originsToDelete), cacheKeysToDelete = WTFMove(cacheKeysToDelete)](auto* traversalEntry) mutable {
            if (traversalEntry) {
                if (originsToDelete.contains(SecurityOrigin::create(traversalEntry->entry.response().url())))
                    cacheKeysToDelete.append(traversalEntry->entry.key());
                return;
            }

            for (auto& key : cacheKeysToDelete)
                NetworkCache::singleton().remove(key);

            RunLoop::main().dispatch(WTFMove(completionHandler));
            return;
        });

        return;
    }
#endif

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void NetworkProcess::deleteWebsiteDataForOrigins(SessionID sessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<SecurityOriginData>& origins, const Vector<String>& cookieHostNames, uint64_t callbackID)
{
    if (websiteDataTypes.contains(WebsiteDataType::Cookies)) {
        if (auto* networkStorageSession = NetworkStorageSession::storageSession(sessionID))
            deleteCookiesForHostnames(*networkStorageSession, cookieHostNames);
    }

    auto completionHandler = [this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    };

    if (websiteDataTypes.contains(WebsiteDataType::DiskCache) && !sessionID.isEphemeral()) {
        clearDiskCacheEntries(origins, WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

void NetworkProcess::downloadRequest(SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedFilename)
{
    downloadManager().startDownload(nullptr, sessionID, downloadID, request, suggestedFilename);
}

void NetworkProcess::resumeDownload(SessionID sessionID, DownloadID downloadID, const IPC::DataReference& resumeData, const String& path, const WebKit::SandboxExtension::Handle& sandboxExtensionHandle)
{
    downloadManager().resumeDownload(sessionID, downloadID, resumeData, path, sandboxExtensionHandle);
}

void NetworkProcess::cancelDownload(DownloadID downloadID)
{
    downloadManager().cancelDownload(downloadID);
}
    
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkProcess::canAuthenticateAgainstProtectionSpace(NetworkResourceLoader& loader, const WebCore::ProtectionSpace& protectionSpace)
{
    static uint64_t lastLoaderID = 0;
    uint64_t loaderID = ++lastLoaderID;
    m_waitingNetworkResourceLoaders.set(lastLoaderID, loader);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::CanAuthenticateAgainstProtectionSpace(loaderID, loader.pageID(), loader.frameID(), protectionSpace), 0);
}

void NetworkProcess::continueCanAuthenticateAgainstProtectionSpace(uint64_t loaderID, bool canAuthenticate)
{
    m_waitingNetworkResourceLoaders.take(loaderID).value()->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
}
#endif

#if USE(NETWORK_SESSION)
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkProcess::continueCanAuthenticateAgainstProtectionSpaceDownload(DownloadID downloadID, bool canAuthenticate)
{
    downloadManager().continueCanAuthenticateAgainstProtectionSpace(downloadID, canAuthenticate);
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
#endif

void NetworkProcess::continueDecidePendingDownloadDestination(DownloadID downloadID, String destination, const SandboxExtension::Handle& sandboxExtensionHandle, bool allowOverwrite)
{
    if (destination.isEmpty())
        downloadManager().cancelDownload(downloadID);
    else
        downloadManager().continueDecidePendingDownloadDestination(downloadID, destination, sandboxExtensionHandle, allowOverwrite);
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
    if (WebCore::getVolumeFreeSpace(m_diskCacheDirectory, diskFreeSize)) {
        // As a fudge factor, use 1000 instead of 1024, in case the reported byte
        // count doesn't align exactly to a megabyte boundary.
        diskFreeSize /= KB * 1000;
        calculateURLCacheSizes(cacheModel, diskFreeSize, urlCacheMemoryCapacity, urlCacheDiskCapacity);
    }

    if (m_diskCacheSizeOverride >= 0)
        urlCacheDiskCapacity = m_diskCacheSizeOverride;

#if ENABLE(NETWORK_CACHE)
    auto& networkCache = NetworkCache::singleton();
    if (networkCache.isEnabled()) {
        networkCache.setCapacity(urlCacheDiskCapacity);
        return;
    }
#endif

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

void NetworkProcess::processWillSuspendImminently(bool& handled)
{
    lowMemoryHandler(Critical::Yes);
    handled = true;
}

void NetworkProcess::prepareToSuspend()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend()", this);
    lowMemoryHandler(Critical::Yes);

    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::prepareToSuspend() Sending ProcessReadyToSuspend IPC message", this);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::ProcessReadyToSuspend(), 0);
}

void NetworkProcess::cancelPrepareToSuspend()
{
    // Although it is tempting to send a NetworkProcessProxy::DidCancelProcessSuspension message from here
    // we do not because prepareToSuspend() already replied with a NetworkProcessProxy::ProcessReadyToSuspend
    // message. And NetworkProcessProxy expects to receive either a NetworkProcessProxy::ProcessReadyToSuspend-
    // or NetworkProcessProxy::DidCancelProcessSuspension- message, but not both.
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::cancelPrepareToSuspend()", this);
}

void NetworkProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcess::processDidResume()", this);
}

void NetworkProcess::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
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
#endif

} // namespace WebKit
