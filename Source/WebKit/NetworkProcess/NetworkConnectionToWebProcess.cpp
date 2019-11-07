/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "NetworkConnectionToWebProcess.h"

#include "BlobDataFileReferenceWithSandboxExtension.h"
#include "CacheStorageEngineConnectionMessages.h"
#include "DataReference.h"
#include "Logging.h"
#include "NetworkCache.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkRTCSocketMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "NetworkSession.h"
#include "NetworkSocketChannel.h"
#include "NetworkSocketChannelMessages.h"
#include "NetworkSocketStream.h"
#include "NetworkSocketStreamMessages.h"
#include "PingLoad.h"
#include "PreconnectTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebIDBConnectionToClient.h"
#include "WebIDBConnectionToClientMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityPolicy.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorProxyMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(NetworkProcess& networkProcess, IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new NetworkConnectionToWebProcess(networkProcess, connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(NetworkProcess& networkProcess, IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
    , m_networkProcess(networkProcess)
#if ENABLE(WEB_RTC)
    , m_mdnsRegister(*this)
#endif
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the Network process, which would be unsafe.
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();
#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->close();
#endif

#if ENABLE(SERVICE_WORKER)
    unregisterSWConnections();
#endif
}

void NetworkConnectionToWebProcess::didCleanupResourceLoader(NetworkResourceLoader& loader)
{
    RELEASE_ASSERT(loader.identifier());
    RELEASE_ASSERT(RunLoop::isMain());

    if (loader.isKeptAlive()) {
        networkProcess().removeKeptAliveLoad(loader);
        return;
    }

    ASSERT(m_networkResourceLoaders.get(loader.identifier()) == &loader);
    m_networkResourceLoaders.remove(loader.identifier());
}

void NetworkConnectionToWebProcess::transferKeptAliveLoad(NetworkResourceLoader& loader)
{
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(loader.isKeptAlive());
    ASSERT(m_networkResourceLoaders.get(loader.identifier()) == &loader);
    if (auto takenLoader = m_networkResourceLoaders.take(loader.identifier()))
        m_networkProcess->addKeptAliveLoad(takenLoader.releaseNonNull());
}

void NetworkConnectionToWebProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveNetworkConnectionToWebProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkResourceLoader::messageReceiverName()) {
        RELEASE_ASSERT(RunLoop::isMain());
        RELEASE_ASSERT(decoder.destinationID());
        if (auto* loader = m_networkResourceLoaders.get(decoder.destinationID()))
            loader->didReceiveNetworkResourceLoaderMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketStream::messageReceiverName()) {
        if (auto* socketStream = m_networkSocketStreams.get(decoder.destinationID())) {
            socketStream->didReceiveMessage(connection, decoder);
            if (decoder.messageName() == Messages::NetworkSocketStream::Close::name())
                m_networkSocketStreams.remove(decoder.destinationID());
        }
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketChannel::messageReceiverName()) {
        if (auto* channel = m_networkSocketChannels.get(decoder.destinationID()))
            channel->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkProcess::messageReceiverName()) {
        m_networkProcess->didReceiveNetworkProcessMessage(connection, decoder);
        return;
    }


#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::NetworkRTCSocket::messageReceiverName()) {
        rtcProvider().didReceiveNetworkRTCSocketMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::NetworkRTCMonitor::messageReceiverName()) {
        rtcProvider().didReceiveNetworkRTCMonitorMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::NetworkRTCProvider::messageReceiverName()) {
        rtcProvider().didReceiveMessage(connection, decoder);
        return;
    }
#endif
#if ENABLE(WEB_RTC)
    if (decoder.messageReceiverName() == Messages::NetworkMDNSRegister::messageReceiverName()) {
        mdnsRegister().didReceiveMessage(connection, decoder);
        return;
    }
#endif

    if (decoder.messageReceiverName() == Messages::CacheStorageEngineConnection::messageReceiverName()) {
        cacheStorageConnection().didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(INDEXED_DATABASE)
    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToClient::messageReceiverName()) {
        auto iterator = m_webIDBConnections.find(decoder.destinationID());
        if (iterator != m_webIDBConnections.end())
            iterator->value->didReceiveMessage(connection, decoder);
        return;
    }
#endif
    
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (auto swConnection = m_swConnections.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID())))
            swConnection->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        if (auto* contextConnection = m_networkProcess->connectionToContextProcessFromIPCConnection(connection)) {
            contextConnection->didReceiveMessage(connection, decoder);
            return;
        }
    }

    if (decoder.messageReceiverName() == Messages::ServiceWorkerFetchTask::messageReceiverName()) {
        if (auto* contextConnection = m_networkProcess->connectionToContextProcessFromIPCConnection(connection)) {
            contextConnection->didReceiveFetchTaskMessage(connection, decoder);
            return;
        }
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName())
        return paymentCoordinator().didReceiveMessage(connection, decoder);
#endif

    LOG_ERROR("Unhandled network process message '%s:%s'", decoder.messageReceiverName().toString().data(), decoder.messageName().toString().data());

    ASSERT_NOT_REACHED();
}

#if USE(LIBWEBRTC)
NetworkRTCProvider& NetworkConnectionToWebProcess::rtcProvider()
{
    if (!m_rtcProvider)
        m_rtcProvider = NetworkRTCProvider::create(*this);
    return *m_rtcProvider;
}
#endif

CacheStorageEngineConnection& NetworkConnectionToWebProcess::cacheStorageConnection()
{
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = CacheStorageEngineConnection::create(*this);
    return *m_cacheStorageConnection;
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (auto swConnection = m_swConnections.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID())))
            swConnection->didReceiveSyncMessage(connection, decoder, reply);
        return;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName())
        return paymentCoordinator().didReceiveSyncMessage(connection, decoder, reply);
#endif

    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(SERVICE_WORKER)
    if (RefPtr<WebSWServerToContextConnection> serverToContextConnection = m_networkProcess->connectionToContextProcessFromIPCConnection(connection)) {
        // Service Worker process exited.
        m_networkProcess->connectionToContextProcessWasClosed(serverToContextConnection.releaseNonNull());
        return;
    }
#else
    UNUSED_PARAM(connection);
#endif

    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

    while (!m_networkResourceLoaders.isEmpty())
        m_networkResourceLoaders.begin()->value->abort();

    // All trackers of resources that were in the middle of being loaded were
    // stopped with the abort() calls above, but we still need to sweep up the
    // root activity trackers.
    stopAllNetworkActivityTracking();

    m_networkProcess->connectionToWebProcessClosed(connection);

    m_networkProcess->removeNetworkConnectionToWebProcess(*this);

#if USE(LIBWEBRTC)
    if (m_rtcProvider) {
        m_rtcProvider->close();
        m_rtcProvider = nullptr;
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    auto idbConnections = std::exchange(m_webIDBConnections, { });
    for (auto& connection : idbConnections.values())
        connection->disconnectedFromWebProcess();
#endif
    
#if ENABLE(SERVICE_WORKER)
    unregisterSWConnections();
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    m_paymentCoordinator = nullptr;
#endif
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkConnectionToWebProcess::createSocketStream(URL&& url, PAL::SessionID sessionID, String cachePartition, uint64_t identifier)
{
    ASSERT(!m_networkSocketStreams.contains(identifier));
    WebCore::SourceApplicationAuditToken token = { };
#if PLATFORM(COCOA)
    token = { m_networkProcess->sourceApplicationAuditData() };
#endif
    m_networkSocketStreams.set(identifier, NetworkSocketStream::create(m_networkProcess.get(), WTFMove(url), sessionID, cachePartition, identifier, m_connection, WTFMove(token)));
}

void NetworkConnectionToWebProcess::createSocketChannel(PAL::SessionID sessionID, const ResourceRequest& request, const String& protocol, uint64_t identifier)
{
    ASSERT(!m_networkSocketChannels.contains(identifier));
    if (auto channel = NetworkSocketChannel::create(*this, sessionID, request, protocol, identifier))
        m_networkSocketChannels.add(identifier, WTFMove(channel));
}

void NetworkConnectionToWebProcess::removeSocketChannel(uint64_t identifier)
{
    ASSERT(m_networkSocketChannels.contains(identifier));
    m_networkSocketChannels.remove(identifier);
}

void NetworkConnectionToWebProcess::cleanupForSuspension(Function<void()>&& completionHandler)
{
#if USE(LIBWEBRTC)
    if (m_rtcProvider) {
        m_rtcProvider->closeListeningSockets(WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler();
}

void NetworkConnectionToWebProcess::endSuspension()
{
#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->authorizeListeningSockets();
#endif
}

Vector<RefPtr<WebCore::BlobDataFileReference>> NetworkConnectionToWebProcess::resolveBlobReferences(const NetworkResourceLoadParameters& parameters)
{
    auto* session = networkProcess().networkSession(parameters.sessionID);
    if (!session)
        return { };

    auto& blobRegistry = session->blobRegistry();

    Vector<RefPtr<WebCore::BlobDataFileReference>> files;
    if (auto* body = parameters.request.httpBody()) {
        for (auto& element : body->elements()) {
            if (auto* blobData = WTF::get_if<FormDataElement::EncodedBlobData>(element.data))
                files.appendVector(blobRegistry.filesInBlob(blobData->url));
        }
        const_cast<WebCore::ResourceRequest&>(parameters.request).setHTTPBody(body->resolveBlobReferences(&blobRegistry));
    }

    return files;
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(NetworkResourceLoadParameters&& loadParameters)
{
    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this);
    m_networkResourceLoaders.add(identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::performSynchronousLoad(NetworkResourceLoadParameters&& loadParameters, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply&& reply)
{
    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this, WTFMove(reply));
    m_networkResourceLoaders.add(identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebCore::PageIdentifier webPageID, Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply::DelayedReply&& reply)
{
    bool handled = false;
    if (!m_networkProcess->parentProcessConnection()->sendSync(Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(webPageID), Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply::Reply(handled), 0))
        return reply(false);
    reply(handled);
}

void NetworkConnectionToWebProcess::loadPing(NetworkResourceLoadParameters&& loadParameters)
{
    auto completionHandler = [connection = m_connection.copyRef(), identifier = loadParameters.identifier] (const ResourceError& error, const ResourceResponse& response) {
        connection->send(Messages::NetworkProcessConnection::DidFinishPingLoad(identifier, error, response), 0);
    };

    // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingLoad(*this, networkProcess(), WTFMove(loadParameters), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::setOnLineState(bool isOnLine)
{
    m_connection->send(Messages::NetworkProcessConnection::SetOnLineState(isOnLine), 0);
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());

    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    // Abort the load now, as the WebProcess won't be able to respond to messages any more which might lead
    // to leaked loader resources (connections, threads, etc).
    loader->abort();
    ASSERT(!m_networkResourceLoaders.contains(identifier));
}

void NetworkConnectionToWebProcess::pageLoadCompleted(PageIdentifier webPageID)
{
    stopAllNetworkActivityTrackingForPage(webPageID);
}

void NetworkConnectionToWebProcess::prefetchDNS(const String& hostname)
{
    m_networkProcess->prefetchDNS(hostname);
}

void NetworkConnectionToWebProcess::preconnectTo(uint64_t preconnectionIdentifier, NetworkResourceLoadParameters&& parameters)
{
    ASSERT(!parameters.request.httpBody());
    
#if ENABLE(SERVER_PRECONNECT)
    new PreconnectTask(networkProcess(), WTFMove(parameters), [this, protectedThis = makeRef(*this), identifier = preconnectionIdentifier] (const ResourceError& error) {
        didFinishPreconnection(identifier, error);
    });
#else
    UNUSED_PARAM(parameters);
    didFinishPreconnection(preconnectionIdentifier, internalError(parameters.request.url()));
#endif
}

void NetworkConnectionToWebProcess::didFinishPreconnection(uint64_t preconnectionIdentifier, const ResourceError& error)
{
    if (!m_connection->isValid())
        return;

    m_connection->send(Messages::NetworkProcessConnection::DidFinishPreconnection(preconnectionIdentifier, error), 0);
}

static NetworkStorageSession& storageSession(const NetworkProcess& networkProcess, PAL::SessionID sessionID)
{
    if (sessionID != PAL::SessionID::defaultSessionID()) {
        if (auto* storageSession = networkProcess.storageSession(sessionID))
            return *storageSession;

        // Some requests with private browsing mode requested may still be coming shortly after NetworkProcess was told to destroy its session.
        // FIXME: Find a way to track private browsing sessions more rigorously.
        LOG_ERROR("Non-default storage session was requested, but there was no session for it. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    }
    return networkProcess.defaultStorageSession();
}

void NetworkConnectionToWebProcess::startDownload(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
{
    m_networkProcess->downloadManager().startDownload(sessionID, downloadID, request, suggestedName);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(PAL::SessionID sessionID, uint64_t mainResourceLoadIdentifier, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // In case a response is served from service worker, we do not have yet the ability to convert the load.
    if (!mainResourceLoadIdentifier || response.source() == ResourceResponse::Source::ServiceWorker) {
        m_networkProcess->downloadManager().startDownload(sessionID, downloadID, request);
        return;
    }

    NetworkResourceLoader* loader = m_networkResourceLoaders.get(mainResourceLoadIdentifier);
    if (!loader) {
        // If we're trying to download a blob here loader can be null.
        return;
    }

    loader->convertToDownload(downloadID, request, response);
}

void NetworkConnectionToWebProcess::cookiesForDOM(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&& completionHandler)
{
    auto& networkStorageSession = storageSession(networkProcess(), sessionID);
    auto result = networkStorageSession.cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto* session = networkProcess().networkSession(sessionID)) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::cookiesForDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, WTF::nullopt);
    }
#endif
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<WebCore::FrameIdentifier> frameID, Optional<PageIdentifier> pageID, const String& cookieString)
{
    auto& networkStorageSession = storageSession(networkProcess(), sessionID);
    networkStorageSession.setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, cookieString);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto* session = networkProcess().networkSession(sessionID)) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::setCookiesFromDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, WTF::nullopt);
    }
#endif
}

void NetworkConnectionToWebProcess::cookiesEnabled(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(storageSession(networkProcess(), sessionID).cookiesEnabled());
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, CompletionHandler<void(String, bool)>&& completionHandler)
{
    auto result = storageSession(networkProcess(), sessionID).cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::getRawCookies(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    Vector<WebCore::Cookie> result;
    storageSession(networkProcess(), sessionID).getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, result);
    completionHandler(WTFMove(result));
}

void NetworkConnectionToWebProcess::deleteCookie(PAL::SessionID sessionID, const URL& url, const String& cookieName)
{
    storageSession(networkProcess(), sessionID).deleteCookie(url, cookieName);
}

void NetworkConnectionToWebProcess::registerFileBlobURL(PAL::SessionID sessionID, const URL& url, const String& path, SandboxExtension::Handle&& extensionHandle, const String& contentType)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().registerFileBlobURL(url, BlobDataFileReferenceWithSandboxExtension::create(path, SandboxExtension::create(WTFMove(extensionHandle))), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURL(PAL::SessionID sessionID, const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().registerBlobURL(url, WTFMove(blobParts), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(PAL::SessionID sessionID, const URL& url, const URL& srcURL)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().registerBlobURL(url, srcURL);
}

void NetworkConnectionToWebProcess::registerBlobURLOptionallyFileBacked(PAL::SessionID sessionID, const URL& url, const URL& srcURL, const String& fileBackedPath, const String& contentType)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().registerBlobURLOptionallyFileBacked(url, srcURL, BlobDataFileReferenceWithSandboxExtension::create(fileBackedPath, nullptr), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLForSlice(PAL::SessionID sessionID, const URL& url, const URL& srcURL, int64_t start, int64_t end)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().registerBlobURLForSlice(url, srcURL, start, end);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(PAL::SessionID sessionID, const URL& url)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return;

    session->blobRegistry().unregisterBlobURL(url);
}

void NetworkConnectionToWebProcess::blobSize(PAL::SessionID sessionID, const URL& url, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    auto* session = networkProcess().networkSession(sessionID);
    completionHandler(session ? session->blobRegistry().blobSize(url) : 0);
}

void NetworkConnectionToWebProcess::writeBlobsToTemporaryFiles(PAL::SessionID sessionID, const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto* session = networkProcess().networkSession(sessionID);
    if (!session)
        return completionHandler({ });

    Vector<RefPtr<BlobDataFileReference>> fileReferences;
    for (auto& url : blobURLs)
        fileReferences.appendVector(session->blobRegistry().filesInBlob({ { }, url }));

    for (auto& file : fileReferences)
        file->prepareForFileAccess();

    session->blobRegistry().writeBlobsToTemporaryFiles(blobURLs, [fileReferences = WTFMove(fileReferences), completionHandler = WTFMove(completionHandler)](auto&& fileNames) mutable {
        for (auto& file : fileReferences)
            file->revokeFileAccess();
        completionHandler(WTFMove(fileNames));
    });
}

void NetworkConnectionToWebProcess::setCaptureExtraNetworkLoadMetricsEnabled(bool enabled)
{
    m_captureExtraNetworkLoadMetricsEnabled = enabled;
    if (m_captureExtraNetworkLoadMetricsEnabled)
        return;

    m_networkLoadInformationByID.clear();
    for (auto& loader : m_networkResourceLoaders.values())
        loader->disableExtraNetworkLoadMetricsCapture();
}

void NetworkConnectionToWebProcess::ensureLegacyPrivateBrowsingSession()
{
    m_networkProcess->addWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters());
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkConnectionToWebProcess::removeStorageAccessForFrame(PAL::SessionID sessionID, FrameIdentifier frameID, PageIdentifier pageID)
{
    if (auto* storageSession = networkProcess().storageSession(sessionID))
        storageSession->removeStorageAccessForFrame(frameID, pageID);
}

void NetworkConnectionToWebProcess::clearPageSpecificDataForResourceLoadStatistics(PAL::SessionID sessionID, PageIdentifier pageID)
{
    if (auto* storageSession = networkProcess().storageSession(sessionID))
        storageSession->clearPageSpecificDataForResourceLoadStatistics(pageID);
}

void NetworkConnectionToWebProcess::logUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& domain)
{
    if (auto* networkSession = networkProcess().networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logUserInteraction(domain, [] { });
    }
}

void NetworkConnectionToWebProcess::resourceLoadStatisticsUpdated(ResourceLoadObserver::PerSessionResourceLoadData&& statistics)
{
    for (auto& iter : statistics) {
        if (auto* networkSession = networkProcess().networkSession(iter.first)) {
            if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
                resourceLoadStatistics->resourceLoadStatisticsUpdated(WTFMove(iter.second));
        }
    }
}

void NetworkConnectionToWebProcess::hasStorageAccess(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = networkProcess().networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(completionHandler));
            return;
        }
    }

    completionHandler(true);
}

void NetworkConnectionToWebProcess::requestStorageAccess(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(WebCore::StorageAccessWasGranted wasGranted, WebCore::StorageAccessPromptWasShown promptWasShown)>&& completionHandler)
{
    if (auto* networkSession = networkProcess().networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->requestStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(completionHandler));
            return;
        }
    }

    completionHandler(WebCore::StorageAccessWasGranted::Yes, WebCore::StorageAccessPromptWasShown::No);
}

void NetworkConnectionToWebProcess::requestStorageAccessUnderOpener(PAL::SessionID sessionID, WebCore::RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, WebCore::RegistrableDomain&& openerDomain)
{
    if (auto* networkSession = networkProcess().networkSession(sessionID)) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));
    }
}
#endif

void NetworkConnectionToWebProcess::addOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::removeOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::resetOriginAccessWhitelists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
}

Optional<NetworkActivityTracker> NetworkConnectionToWebProcess::startTrackingResourceLoad(PageIdentifier pageID, ResourceLoadIdentifier resourceID, bool isMainResource, const PAL::SessionID& sessionID)
{
    if (sessionID.isEphemeral())
        return WTF::nullopt;

    // Either get the existing root activity tracker for this page or create a
    // new one if this is the main resource.

    size_t rootActivityIndex;
    if (isMainResource) {
        // If we're loading a page from the top, make sure any tracking of
        // previous activity for this page is stopped.

        stopAllNetworkActivityTrackingForPage(pageID);

        rootActivityIndex = m_networkActivityTrackers.size();
        m_networkActivityTrackers.constructAndAppend(pageID);
        m_networkActivityTrackers[rootActivityIndex].networkActivity.start();

#if HAVE(NW_ACTIVITY)
        ASSERT(m_networkActivityTrackers[rootActivityIndex].networkActivity.getPlatformObject());
#endif
    } else {
        rootActivityIndex = findRootNetworkActivity(pageID);

        // This could happen if the Networking process crashes, taking its
        // previous state with it.
        if (rootActivityIndex == notFound)
            return WTF::nullopt;

#if HAVE(NW_ACTIVITY)
        ASSERT(m_networkActivityTrackers[rootActivityIndex].networkActivity.getPlatformObject());
#endif
    }

    // Create a tracker for the loading of the new resource, setting the root
    // activity tracker as its parent.

    size_t newActivityIndex = m_networkActivityTrackers.size();
    m_networkActivityTrackers.constructAndAppend(pageID, resourceID);
#if HAVE(NW_ACTIVITY)
    ASSERT(m_networkActivityTrackers[newActivityIndex].networkActivity.getPlatformObject());
#endif

    auto& newActivityTracker = m_networkActivityTrackers[newActivityIndex];
    newActivityTracker.networkActivity.setParent(m_networkActivityTrackers[rootActivityIndex].networkActivity);
    newActivityTracker.networkActivity.start();

    return newActivityTracker.networkActivity;
}

void NetworkConnectionToWebProcess::stopTrackingResourceLoad(ResourceLoadIdentifier resourceID, NetworkActivityTracker::CompletionCode code)
{
    auto itemIndex = findNetworkActivityTracker(resourceID);
    if (itemIndex == notFound)
        return;

    m_networkActivityTrackers[itemIndex].networkActivity.complete(code);
    m_networkActivityTrackers.remove(itemIndex);
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTracking()
{
    for (auto& activityTracker : m_networkActivityTrackers)
        activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::None);

    m_networkActivityTrackers.clear();
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTrackingForPage(PageIdentifier pageID)
{
    for (auto& activityTracker : m_networkActivityTrackers) {
        if (activityTracker.pageID == pageID)
            activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::None);
    }

    m_networkActivityTrackers.removeAllMatching([&](const auto& activityTracker) {
        return activityTracker.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findRootNetworkActivity(PageIdentifier pageID)
{
    return m_networkActivityTrackers.findMatching([&](const auto& item) {
        return item.isRootActivity && item.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findNetworkActivityTracker(ResourceLoadIdentifier resourceID)
{
    return m_networkActivityTrackers.findMatching([&](const auto& item) {
        return item.resourceID == resourceID;
    });
}

#if ENABLE(INDEXED_DATABASE)
static uint64_t generateIDBConnectionToServerIdentifier()
{
    ASSERT(RunLoop::isMain());
    static uint64_t identifier = 0;
    return ++identifier;
}

void NetworkConnectionToWebProcess::establishIDBConnectionToServer(PAL::SessionID sessionID, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    uint64_t serverConnectionIdentifier = generateIDBConnectionToServerIdentifier();
    LOG(IndexedDB, "NetworkConnectionToWebProcess::establishIDBConnectionToServer - %" PRIu64, serverConnectionIdentifier);
    ASSERT(!m_webIDBConnections.contains(serverConnectionIdentifier));
    
    m_webIDBConnections.set(serverConnectionIdentifier, WebIDBConnectionToClient::create(m_networkProcess, m_connection.get(), serverConnectionIdentifier, sessionID));
    completionHandler(serverConnectionIdentifier);
}
#endif
    
#if ENABLE(SERVICE_WORKER)
void NetworkConnectionToWebProcess::unregisterSWConnections()
{
    auto swConnections = WTFMove(m_swConnections);
    for (auto& swConnection : swConnections.values()) {
        if (swConnection)
            swConnection->server().removeConnection(swConnection->identifier());
    }
}

void NetworkConnectionToWebProcess::establishSWServerConnection(PAL::SessionID sessionID, CompletionHandler<void(WebCore::SWServerConnectionIdentifier&&)>&& completionHandler)
{
    auto& server = m_networkProcess->swServerForSession(sessionID);
    auto connection = makeUnique<WebSWServerConnection>(m_networkProcess, server, m_connection.get(), sessionID);
    
    SWServerConnectionIdentifier serverConnectionIdentifier = connection->identifier();
    LOG(ServiceWorker, "NetworkConnectionToWebProcess::establishSWServerConnection - %s", serverConnectionIdentifier.loggingString().utf8().data());

    ASSERT(!m_swConnections.contains(serverConnectionIdentifier));
    m_swConnections.add(serverConnectionIdentifier, makeWeakPtr(*connection));
    server.addConnection(WTFMove(connection));
    completionHandler(WTFMove(serverConnectionIdentifier));
}
#endif

} // namespace WebKit
