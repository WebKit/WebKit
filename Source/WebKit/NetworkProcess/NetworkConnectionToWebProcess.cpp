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
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "NetworkSchemeRegistry.h"
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
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityPolicy.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorProxyMessages.h"
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif

#undef RELEASE_LOG_IF_ALLOWED
#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(m_sessionID.isAlwaysOnLoggingAllowed(), channel, "%p - [webProcessIdentifier=%" PRIu64 "] NetworkConnectionToWebProcess::" fmt, this, webProcessIdentifier().toUInt64(), ##__VA_ARGS__)

#define NETWORK_PROCESS_MESSAGE_CHECK(assertion) NETWORK_PROCESS_MESSAGE_CHECK_COMPLETION(assertion, (void)0)
#define NETWORK_PROCESS_MESSAGE_CHECK_COMPLETION(assertion, completion) do { \
    ASSERT(assertion); \
    if (UNLIKELY(!(assertion))) { \
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(m_webProcessIdentifier), 0); \
        { completion; } \
        return; \
    } \
} while (0)

namespace WebKit {
using namespace WebCore;

Ref<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new NetworkConnectionToWebProcess(networkProcess, webProcessIdentifier, sessionID, connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
    , m_networkProcess(networkProcess)
    , m_sessionID(sessionID)
    , m_networkResourceLoaders([this](bool hasUpload) { hasUploadStateChanged(hasUpload); })
#if ENABLE(WEB_RTC)
    , m_mdnsRegister(*this)
#endif
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_schemeRegistry(NetworkSchemeRegistry::create())
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the Network process, which would be unsafe.
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();

#if ENABLE(SERVICE_WORKER)
    establishSWServerConnection();
#endif
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();

    // This may call hasUploadStateChanged().
    m_networkResourceLoaders.clear();

    for (auto& port : m_processEntangledPorts)
        networkProcess().messagePortChannelRegistry().didCloseMessagePort(port);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (auto* networkStorageSession = storageSession())
        networkStorageSession->stopListeningForCookieChangeNotifications(*this, m_hostsWithCookieListeners);
#endif

#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->close();
#endif

#if ENABLE(SERVICE_WORKER)
    unregisterSWConnection();
#endif
}

void NetworkConnectionToWebProcess::hasUploadStateChanged(bool hasUpload)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "hasUploadStateChanged: (hasUpload=%d)", hasUpload);
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::SetWebProcessHasUploads(m_webProcessIdentifier, hasUpload), 0);
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
    // For security reasons, Messages::NetworkProcess IPC is only supposed to come from the UIProcess.
    ASSERT(decoder.messageReceiverName() != Messages::NetworkProcess::messageReceiverName());

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
        if (auto* socketStream = m_networkSocketStreams.get(makeObjectIdentifier<WebSocketIdentifierType>(decoder.destinationID()))) {
            socketStream->didReceiveMessage(connection, decoder);
            if (decoder.messageName() == Messages::NetworkSocketStream::Close::name())
                m_networkSocketStreams.remove(makeObjectIdentifier<WebSocketIdentifierType>(decoder.destinationID()));
        }
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketChannel::messageReceiverName()) {
        if (auto* channel = m_networkSocketChannels.get(makeObjectIdentifier<WebSocketIdentifierType>(decoder.destinationID())))
            channel->didReceiveMessage(connection, decoder);
        return;
    }

#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::NetworkRTCMonitor::messageReceiverName()) {
        rtcProvider().didReceiveNetworkRTCMonitorMessage(connection, decoder);
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
    
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (m_swConnection)
            m_swConnection->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        if (m_swContextConnection)
            m_swContextConnection->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::ServiceWorkerFetchTask::messageReceiverName()) {
        if (m_swContextConnection)
            m_swContextConnection->didReceiveFetchTaskMessage(connection, decoder);
        return;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName())
        return paymentCoordinator().didReceiveMessage(connection, decoder);
#endif

    WTFLogAlways("Unhandled network process message '%s'", description(decoder.messageName()));
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

void NetworkConnectionToWebProcess::createRTCProvider(CompletionHandler<void()>&& callback)
{
#if USE(LIBWEBRTC)
    rtcProvider();
#endif
    callback();
}

CacheStorageEngineConnection& NetworkConnectionToWebProcess::cacheStorageConnection()
{
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = CacheStorageEngineConnection::create(*this);
    return *m_cacheStorageConnection;
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& reply)
{
    // For security reasons, Messages::NetworkProcess IPC is only supposed to come from the UIProcess.
    ASSERT(decoder.messageReceiverName() != Messages::NetworkProcess::messageReceiverName());

    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (m_swConnection)
            m_swConnection->didReceiveSyncMessage(connection, decoder, reply);
        return;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName())
        return paymentCoordinator().didReceiveSyncMessage(connection, decoder, reply);
#endif

    WTFLogAlways("Unhandled network process message '%s'", description(decoder.messageName()));
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::updateQuotaBasedOnSpaceUsageForTesting(const ClientOrigin& origin)
{
    auto storageQuotaManager = m_networkProcess->storageQuotaManager(sessionID(), origin);
    storageQuotaManager->resetQuotaUpdatedBasedOnUsageForTesting();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(SERVICE_WORKER)
    m_swContextConnection = nullptr;
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

    m_networkProcess->connectionToWebProcessClosed(connection, m_sessionID);

    m_networkProcess->removeNetworkConnectionToWebProcess(*this);

#if USE(LIBWEBRTC)
    if (m_rtcProvider) {
        m_rtcProvider->close();
        m_rtcProvider = nullptr;
    }
#endif

#if ENABLE(SERVICE_WORKER)
    unregisterSWConnection();
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    m_paymentCoordinator = nullptr;
#endif
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName), m_webProcessIdentifier.toUInt64());
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(m_webProcessIdentifier), 0);
}

void NetworkConnectionToWebProcess::createSocketStream(URL&& url, String cachePartition, WebSocketIdentifier identifier)
{
    ASSERT(!m_networkSocketStreams.contains(identifier));
    WebCore::SourceApplicationAuditToken token = { };
#if PLATFORM(COCOA)
    token = { m_networkProcess->sourceApplicationAuditData() };
#endif
    m_networkSocketStreams.add(identifier, NetworkSocketStream::create(m_networkProcess.get(), WTFMove(url), m_sessionID, cachePartition, identifier, m_connection, WTFMove(token)));
}

void NetworkConnectionToWebProcess::createSocketChannel(const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier)
{
    ASSERT(!m_networkSocketChannels.contains(identifier));
    if (auto channel = NetworkSocketChannel::create(*this, m_sessionID, request, protocol, identifier))
        m_networkSocketChannels.add(identifier, WTFMove(channel));
}

void NetworkConnectionToWebProcess::removeSocketChannel(WebSocketIdentifier identifier)
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

NetworkSession* NetworkConnectionToWebProcess::networkSession()
{
    return networkProcess().networkSession(m_sessionID);
}

Vector<RefPtr<WebCore::BlobDataFileReference>> NetworkConnectionToWebProcess::resolveBlobReferences(const NetworkResourceLoadParameters& loadParameters)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "resolveBlobReferences: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier);

    auto* session = networkSession();
    if (!session)
        return { };

    auto& blobRegistry = session->blobRegistry();

    Vector<RefPtr<WebCore::BlobDataFileReference>> files;
    if (auto* body = loadParameters.request.httpBody()) {
        for (auto& element : body->elements()) {
            if (auto* blobData = WTF::get_if<FormDataElement::EncodedBlobData>(element.data))
                files.appendVector(blobRegistry.filesInBlob(blobData->url));
        }
        const_cast<WebCore::ResourceRequest&>(loadParameters.request).setHTTPBody(body->resolveBlobReferences(&blobRegistry));
    }

    return files;
}

#if ENABLE(SERVICE_WORKER)
std::unique_ptr<ServiceWorkerFetchTask> NetworkConnectionToWebProcess::createFetchTask(NetworkResourceLoader& loader, const ResourceRequest& request)
{
    return swConnection().createFetchTask(loader, request);
}
#endif

void NetworkConnectionToWebProcess::scheduleResourceLoad(NetworkResourceLoadParameters&& loadParameters)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "scheduleResourceLoad: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier);

#if ENABLE(SERVICE_WORKER)
    auto& server = m_networkProcess->swServerForSession(m_sessionID);
    if (!server.isImportCompleted()) {
        server.whenImportIsCompleted([this, protectedThis = makeRef(*this), loadParameters = WTFMove(loadParameters)]() mutable {
            if (!m_networkProcess->webProcessConnection(webProcessIdentifier()))
                return;
            ASSERT(m_networkProcess->swServerForSession(m_sessionID).isImportCompleted());
            scheduleResourceLoad(WTFMove(loadParameters));
        });
        return;
    }
#endif
    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto& loader = m_networkResourceLoaders.add(identifier, NetworkResourceLoader::create(WTFMove(loadParameters), *this)).iterator->value;

#if ENABLE(SERVICE_WORKER)
    loader->startWithServiceWorker();
#else
    loader->start();
#endif
}

void NetworkConnectionToWebProcess::performSynchronousLoad(NetworkResourceLoadParameters&& loadParameters, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply&& reply)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "performSynchronousLoad: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier);

    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this, WTFMove(reply));
    m_networkResourceLoaders.add(identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier pageID, Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply::DelayedReply&& reply)
{
    bool handled = false;
    if (!m_networkProcess->parentProcessConnection()->sendSync(Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(pageID), Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply::Reply(handled), 0))
        return reply(false);
    reply(handled);
}

void NetworkConnectionToWebProcess::loadPing(NetworkResourceLoadParameters&& loadParameters)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "loadPing: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier);

    auto completionHandler = [connection = m_connection, identifier = loadParameters.identifier] (const ResourceError& error, const ResourceResponse& response) {
        connection->send(Messages::NetworkProcessConnection::DidFinishPingLoad(identifier, error, response), 0);
    };

    // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingLoad(*this, WTFMove(loadParameters), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::setOnLineState(bool isOnLine)
{
    m_connection->send(Messages::NetworkProcessConnection::SetOnLineState(isOnLine), 0);
}

void NetworkConnectionToWebProcess::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    m_connection->send(Messages::NetworkProcessConnection::CookieAcceptPolicyChanged(newPolicy), 0);
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

void NetworkConnectionToWebProcess::browsingContextRemoved(WebPageProxyIdentifier webPageProxyID, PageIdentifier webPageID, FrameIdentifier webFrameID)
{
    if (auto* session = networkProcess().networkSession(sessionID())) {
        if (auto* cache = session->cache())
            cache->browsingContextRemoved(webPageProxyID, webPageID, webFrameID);
    }
}

void NetworkConnectionToWebProcess::prefetchDNS(const String& hostname)
{
    m_networkProcess->prefetchDNS(hostname);
}

void NetworkConnectionToWebProcess::sendH2Ping(NetworkResourceLoadParameters&& parameters, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
#if ENABLE(SERVER_PRECONNECT)
    auto* networkSession = this->networkSession();
    if (!networkSession)
        return completionHandler(makeUnexpected(internalError(parameters.request.url())));

    URL url = parameters.request.url();
    auto* task = new PreconnectTask(*networkSession, WTFMove(parameters), [] (const ResourceError&) { });
    task->setH2PingCallback(url, WTFMove(completionHandler));
    task->start();
#else
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(internalError(parameters.request.url())));
#endif
}

void NetworkConnectionToWebProcess::preconnectTo(Optional<uint64_t> preconnectionIdentifier, NetworkResourceLoadParameters&& loadParameters)
{
    RELEASE_LOG_IF_ALLOWED(Loading, "preconnectTo: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier);

    ASSERT(!loadParameters.request.httpBody());

    auto completionHandler = [this, protectedThis = makeRef(*this), preconnectionIdentifier = WTFMove(preconnectionIdentifier)](const ResourceError& error) {
        if (preconnectionIdentifier)
            didFinishPreconnection(*preconnectionIdentifier, error);
    };

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (networkProcess().supplement<LegacyCustomProtocolManager>()->supportsScheme(loadParameters.request.url().protocol().toString())) {
        completionHandler(internalError(loadParameters.request.url()));
        return;
    }
#endif

#if ENABLE(SERVER_PRECONNECT)
    auto* session = networkSession();
    if (session && session->allowsServerPreconnect()) {
        (new PreconnectTask(*session, WTFMove(loadParameters), [completionHandler = WTFMove(completionHandler)] (const ResourceError& error) {
            completionHandler(error);
        }))->start();
        return;
    }
#endif
    completionHandler(internalError(loadParameters.request.url()));
}

void NetworkConnectionToWebProcess::didFinishPreconnection(uint64_t preconnectionIdentifier, const ResourceError& error)
{
    if (!m_connection->isValid())
        return;

    m_connection->send(Messages::NetworkProcessConnection::DidFinishPreconnection(preconnectionIdentifier, error), 0);
}

NetworkStorageSession* NetworkConnectionToWebProcess::storageSession()
{
    auto* session = networkProcess().storageSession(m_sessionID);
    if (!session)
        LOG_ERROR("Non-default storage session was requested, but there was no session for it. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    return session;
}

void NetworkConnectionToWebProcess::startDownload(DownloadID downloadID, const ResourceRequest& request, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedName)
{
    m_networkProcess->downloadManager().startDownload(m_sessionID, downloadID, request, isNavigatingToAppBoundDomain, suggestedName);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(uint64_t mainResourceLoadIdentifier, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // In case a response is served from service worker, we do not have yet the ability to convert the load.
    if (!mainResourceLoadIdentifier || response.source() == ResourceResponse::Source::ServiceWorker) {
        m_networkProcess->downloadManager().startDownload(m_sessionID, downloadID, request, isNavigatingToAppBoundDomain);
        return;
    }

    NetworkResourceLoader* loader = m_networkResourceLoaders.get(mainResourceLoadIdentifier);
    if (!loader) {
        // If we're trying to download a blob here loader can be null.
        return;
    }

    loader->convertToDownload(downloadID, request, response);
}

void NetworkConnectionToWebProcess::registerURLSchemesAsCORSEnabled(Vector<String>&& schemes)
{
    for (auto&& scheme : WTFMove(schemes))
        m_schemeRegistry->registerURLSchemeAsCORSEnabled(WTFMove(scheme));
}

void NetworkConnectionToWebProcess::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, IncludeSecureCookies includeSecureCookies, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&& completionHandler)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ }, false);
    auto result = networkStorageSession->cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto* session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::cookiesForDOM", reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, WTF::nullopt);
    }
#endif
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, WebCore::FrameIdentifier frameID, PageIdentifier pageID, ShouldAskITP shouldAskITP, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return;
    networkStorageSession->setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, shouldAskITP, cookieString, shouldRelaxThirdPartyCookieBlocking);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto* session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::setCookiesFromDOM", reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, WTF::nullopt);
    }
#endif
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String, bool)>&& completionHandler)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ }, false);
    auto result = networkStorageSession->cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ });
    Vector<WebCore::Cookie> result;
    networkStorageSession->getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, shouldAskITP, shouldRelaxThirdPartyCookieBlocking, result);
    completionHandler(WTFMove(result));
}

void NetworkConnectionToWebProcess::setRawCookie(const WebCore::Cookie& cookie)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return;

    networkStorageSession->setCookie(cookie);
}

void NetworkConnectionToWebProcess::deleteCookie(const URL& url, const String& cookieName)
{
    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return;
    networkStorageSession->deleteCookie(url, cookieName);
}

void NetworkConnectionToWebProcess::domCookiesForHost(const String& host, bool subscribeToCookieChangeNotifications, CompletionHandler<void(const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    NETWORK_PROCESS_MESSAGE_CHECK_COMPLETION(HashSet<String>::isValidValue(host), completionHandler({ }));

    auto* networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ });

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (subscribeToCookieChangeNotifications) {
        ASSERT(!m_hostsWithCookieListeners.contains(host));
        m_hostsWithCookieListeners.add(host);
        networkStorageSession->startListeningForCookieChangeNotifications(*this, host);
    }
#else
    UNUSED_PARAM(subscribeToCookieChangeNotifications);
#endif

    completionHandler(networkStorageSession->domCookiesForHost(host));
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)

void NetworkConnectionToWebProcess::unsubscribeFromCookieChangeNotifications(const HashSet<String>& hosts)
{
    bool removed = m_hostsWithCookieListeners.remove(hosts.begin(), hosts.end());
    ASSERT_UNUSED(removed, removed);

    if (auto* networkStorageSession = storageSession())
        networkStorageSession->stopListeningForCookieChangeNotifications(*this, hosts);
}

void NetworkConnectionToWebProcess::cookiesAdded(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    connection().send(Messages::NetworkProcessConnection::CookiesAdded(host, cookies), 0);
}

void NetworkConnectionToWebProcess::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    connection().send(Messages::NetworkProcessConnection::CookiesDeleted(host, cookies), 0);
}

void NetworkConnectionToWebProcess::allCookiesDeleted()
{
    connection().send(Messages::NetworkProcessConnection::AllCookiesDeleted(), 0);
}

#endif

void NetworkConnectionToWebProcess::registerFileBlobURL(const URL& url, const String& path, const String& replacementPath, SandboxExtension::Handle&& extensionHandle, const String& contentType)
{
    NETWORK_PROCESS_MESSAGE_CHECK(!url.isEmpty());

    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().registerFileBlobURL(url, BlobDataFileReferenceWithSandboxExtension::create(path, replacementPath, SandboxExtension::create(WTFMove(extensionHandle))), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().registerBlobURL(url, WTFMove(blobParts), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(const URL& url, const URL& srcURL)
{
    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().registerBlobURL(url, srcURL);
}

void NetworkConnectionToWebProcess::registerBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, const String& fileBackedPath, const String& contentType)
{
    NETWORK_PROCESS_MESSAGE_CHECK(!url.isEmpty() && !srcURL.isEmpty() && !fileBackedPath.isEmpty());

    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().registerBlobURLOptionallyFileBacked(url, srcURL, BlobDataFileReferenceWithSandboxExtension::create(fileBackedPath), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLForSlice(const URL& url, const URL& srcURL, int64_t start, int64_t end)
{
    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().registerBlobURLForSlice(url, srcURL, start, end);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const URL& url)
{
    auto* session = networkSession();
    if (!session)
        return;

    session->blobRegistry().unregisterBlobURL(url);
}

void NetworkConnectionToWebProcess::blobSize(const URL& url, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    auto* session = networkSession();
    completionHandler(session ? session->blobRegistry().blobSize(url) : 0);
}

void NetworkConnectionToWebProcess::writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto* session = networkSession();
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkConnectionToWebProcess::removeStorageAccessForFrame(FrameIdentifier frameID, PageIdentifier pageID)
{
    if (auto* storageSession = networkProcess().storageSession(m_sessionID))
        storageSession->removeStorageAccessForFrame(frameID, pageID);
}

void NetworkConnectionToWebProcess::clearPageSpecificDataForResourceLoadStatistics(PageIdentifier pageID)
{
    if (auto* storageSession = networkProcess().storageSession(m_sessionID))
        storageSession->clearPageSpecificDataForResourceLoadStatistics(pageID);
}

void NetworkConnectionToWebProcess::logUserInteraction(const RegistrableDomain& domain)
{
    if (auto* networkSession = this->networkSession()) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logUserInteraction(domain, [] { });
    }
}

void NetworkConnectionToWebProcess::resourceLoadStatisticsUpdated(Vector<ResourceLoadStatistics>&& statistics, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkSession = this->networkSession()) {
        if (networkSession->sessionID().isEphemeral()) {
            completionHandler();
            return;
        }
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->resourceLoadStatisticsUpdated(WTFMove(statistics), WTFMove(completionHandler));
            return;
        }
    }
    completionHandler();
}

void NetworkConnectionToWebProcess::hasStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession()) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(completionHandler));
            return;
        } else {
            storageSession()->hasCookies(subFrameDomain, WTFMove(completionHandler));
            return;
        }
    }

    completionHandler(false);
}

void NetworkConnectionToWebProcess::requestStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, CompletionHandler<void(WebCore::RequestStorageAccessResult result)>&& completionHandler)
{
    if (auto* networkSession = this->networkSession()) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->requestStorageAccess(subFrameDomain, topFrameDomain, frameID, webPageID, webPageProxyID, scope, WTFMove(completionHandler));
            return;
        }
    }

    completionHandler({ WebCore::StorageAccessWasGranted::Yes, WebCore::StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
}

void NetworkConnectionToWebProcess::requestStorageAccessUnderOpener(WebCore::RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, WebCore::RegistrableDomain&& openerDomain)
{
    if (auto* networkSession = this->networkSession()) {
        if (auto* resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));
    }
}
#endif

void NetworkConnectionToWebProcess::addOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::removeOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::resetOriginAccessAllowLists()
{
    SecurityPolicy::resetOriginAccessAllowlists();
}

Optional<NetworkActivityTracker> NetworkConnectionToWebProcess::startTrackingResourceLoad(PageIdentifier pageID, ResourceLoadIdentifier resourceID, bool isTopResource)
{
    if (m_sessionID.isEphemeral())
        return WTF::nullopt;

    // Either get the existing root activity tracker for this page or create a
    // new one if this is the main resource.

    size_t rootActivityIndex;
    if (isTopResource) {
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
        activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::Cancel);

    m_networkActivityTrackers.clear();
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTrackingForPage(PageIdentifier pageID)
{
    for (auto& activityTracker : m_networkActivityTrackers) {
        if (activityTracker.pageID == pageID)
            activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::Cancel);
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
    
#if ENABLE(SERVICE_WORKER)
void NetworkConnectionToWebProcess::unregisterSWConnection()
{
    if (m_swConnection)
        m_swConnection->server().removeConnection(m_swConnection->identifier());
}

void NetworkConnectionToWebProcess::establishSWServerConnection()
{
    if (m_swConnection)
        return;

    auto& server = m_networkProcess->swServerForSession(m_sessionID);
    auto connection = makeUnique<WebSWServerConnection>(m_networkProcess, server, m_connection.get(), m_webProcessIdentifier);

    m_swConnection = makeWeakPtr(*connection);
    server.addConnection(WTFMove(connection));
}

void NetworkConnectionToWebProcess::establishSWContextConnection(RegistrableDomain&& registrableDomain, CompletionHandler<void()>&& completionHandler)
{
    if (auto* server = m_networkProcess->swServerForSessionIfExists(m_sessionID))
        m_swContextConnection = makeUnique<WebSWServerToContextConnection>(*this, WTFMove(registrableDomain), *server);
    completionHandler();
}

void NetworkConnectionToWebProcess::closeSWContextConnection()
{
    m_swContextConnection = nullptr;
}

void NetworkConnectionToWebProcess::serverToContextConnectionNoLongerNeeded()
{
    RELEASE_LOG_IF_ALLOWED(ServiceWorker, "serverToContextConnectionNoLongerNeeded: WebProcess no longer useful for running service workers");
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::WorkerContextConnectionNoLongerNeeded { webProcessIdentifier() }, 0);

    m_swContextConnection = nullptr;
}

WebSWServerConnection& NetworkConnectionToWebProcess::swConnection()
{
    if (!m_swConnection)
        establishSWServerConnection();
    return *m_swConnection;
}
#endif

void NetworkConnectionToWebProcess::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    m_processEntangledPorts.add(port1);
    m_processEntangledPorts.add(port2);
    networkProcess().messagePortChannelRegistry().didCreateMessagePortChannel(port1, port2);
}

void NetworkConnectionToWebProcess::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    m_processEntangledPorts.add(local);
    networkProcess().messagePortChannelRegistry().didEntangleLocalToRemote(local, remote, m_webProcessIdentifier);

    auto* channel = networkProcess().messagePortChannelRegistry().existingChannelContainingPort(local);
    if (channel && channel->hasAnyMessagesPendingOrInFlight())
        connection().send(Messages::NetworkProcessConnection::MessagesAvailableForPort(local), 0);
}

void NetworkConnectionToWebProcess::messagePortDisentangled(const MessagePortIdentifier& port)
{
    auto result = m_processEntangledPorts.remove(port);
    ASSERT_UNUSED(result, result);

    networkProcess().messagePortChannelRegistry().didDisentangleMessagePort(port);
}

void NetworkConnectionToWebProcess::messagePortClosed(const MessagePortIdentifier& port)
{
    networkProcess().messagePortChannelRegistry().didCloseMessagePort(port);
}

uint64_t NetworkConnectionToWebProcess::nextMessageBatchIdentifier(Function<void()>&& deliveryCallback)
{
    static uint64_t currentMessageBatchIdentifier;
    ASSERT(!m_messageBatchDeliveryCompletionHandlers.contains(currentMessageBatchIdentifier + 1));
    m_messageBatchDeliveryCompletionHandlers.add(++currentMessageBatchIdentifier, WTFMove(deliveryCallback));
    return currentMessageBatchIdentifier;
}

void NetworkConnectionToWebProcess::takeAllMessagesForPort(const MessagePortIdentifier& port, CompletionHandler<void(Vector<MessageWithMessagePorts>&&, uint64_t)>&& callback)
{
    networkProcess().messagePortChannelRegistry().takeAllMessagesForPort(port, [this, protectedThis = makeRef(*this), callback = WTFMove(callback)](auto&& messages, Function<void()>&& deliveryCallback) mutable {
        callback(WTFMove(messages), nextMessageBatchIdentifier(WTFMove(deliveryCallback)));
    });
}

void NetworkConnectionToWebProcess::didDeliverMessagePortMessages(uint64_t messageBatchIdentifier)
{
    // Null check only necessary for rare condition where network process crashes during message port connection establishment.
    if (auto callback = m_messageBatchDeliveryCompletionHandlers.take(messageBatchIdentifier))
        callback();
}

void NetworkConnectionToWebProcess::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& port)
{
    if (networkProcess().messagePortChannelRegistry().didPostMessageToRemote(WTFMove(message), port)) {
        // Look up the process for that port
        auto* channel = networkProcess().messagePortChannelRegistry().existingChannelContainingPort(port);
        ASSERT(channel);
        auto processIdentifier = channel->processForPort(port);
        if (processIdentifier) {
            if (auto* connectionToWebProcess = networkProcess().webProcessConnection(*processIdentifier))
                connectionToWebProcess->connection().send(Messages::NetworkProcessConnection::MessagesAvailableForPort(port), 0);
        }
    }
}

void NetworkConnectionToWebProcess::checkRemotePortForActivity(const WebCore::MessagePortIdentifier port, CompletionHandler<void(bool)>&& callback)
{
    networkProcess().messagePortChannelRegistry().checkRemotePortForActivity(port, [callback = WTFMove(callback)](auto hasActivity) mutable {
        callback(hasActivity == MessagePortChannelProvider::HasActivity::Yes);
    });
}

void NetworkConnectionToWebProcess::checkProcessLocalPortForActivity(const MessagePortIdentifier& port, CompletionHandler<void(MessagePortChannelProvider::HasActivity)>&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkProcessConnection::CheckProcessLocalPortForActivity { port }, WTFMove(callback), 0);
}

void NetworkConnectionToWebProcess::broadcastConsoleMessage(JSC::MessageSource source, JSC::MessageLevel level, const String& message)
{
    connection().send(Messages::NetworkProcessConnection::BroadcastConsoleMessage(source, level, message), 0);
}

void NetworkConnectionToWebProcess::setCORSDisablingPatterns(WebCore::PageIdentifier pageIdentifier, Vector<String>&& patterns)
{
    networkProcess().setCORSDisablingPatterns(pageIdentifier, WTFMove(patterns));
}

} // namespace WebKit

#undef NETWORK_PROCESS_MESSAGE_CHECK_COMPLETION
#undef NETWORK_PROCESS_MESSAGE_CHECK
