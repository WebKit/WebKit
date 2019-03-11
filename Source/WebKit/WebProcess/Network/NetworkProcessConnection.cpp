/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
#include "NetworkProcessConnection.h"

#include "DataReference.h"
#include "LibWebRTCNetwork.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "ServiceWorkerClientFetchMessages.h"
#include "WebCacheStorageConnection.h"
#include "WebCacheStorageConnectionMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebLoaderStrategy.h"
#include "WebMDNSRegisterMessages.h"
#include "WebPage.h"
#include "WebPageMessages.h"
#include "WebPaymentCoordinator.h"
#include "WebProcess.h"
#include "WebRTCMonitor.h"
#include "WebRTCMonitorMessages.h"
#include "WebRTCResolverMessages.h"
#include "WebRTCSocketMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWClientConnection.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include "WebSocketStream.h"
#include "WebSocketStreamMessages.h"
#include <WebCore/CachedResource.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/SharedBuffer.h>
#include <pal/SessionID.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

NetworkProcessConnection::NetworkProcessConnection(IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createClientConnection(connectionIdentifier, *this))
{
    m_connection->open();
}

NetworkProcessConnection::~NetworkProcessConnection()
{
    m_connection->invalidate();
}

void NetworkProcessConnection::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebResourceLoader::messageReceiverName()) {
        if (auto* webResourceLoader = WebProcess::singleton().webLoaderStrategy().webResourceLoaderForIdentifier(decoder.destinationID()))
            webResourceLoader->didReceiveWebResourceLoaderMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSocketStream::messageReceiverName()) {
        if (auto* stream = WebSocketStream::streamWithIdentifier(decoder.destinationID()))
            stream->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebPage::messageReceiverName()) {
        if (auto* webPage = WebProcess::singleton().webPage(decoder.destinationID()))
            webPage->didReceiveWebPageMessage(connection, decoder);
        return;
    }

#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::WebRTCSocket::messageReceiverName()) {
        WebProcess::singleton().libWebRTCNetwork().socket(decoder.destinationID()).didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebRTCMonitor::messageReceiverName()) {
        WebProcess::singleton().libWebRTCNetwork().monitor().didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebRTCResolver::messageReceiverName()) {
        WebProcess::singleton().libWebRTCNetwork().resolver(decoder.destinationID()).didReceiveMessage(connection, decoder);
        return;
    }
#endif
#if ENABLE(WEB_RTC)
    if (decoder.messageReceiverName() == Messages::WebMDNSRegister::messageReceiverName()) {
        WebProcess::singleton().libWebRTCNetwork().mdnsRegister().didReceiveMessage(connection, decoder);
        return;
    }
#endif
    if (decoder.messageReceiverName() == Messages::WebCacheStorageConnection::messageReceiverName()) {
        WebProcess::singleton().cacheStorageProvider().process(connection, decoder);
        return;
    }

#if ENABLE(INDEXED_DATABASE)
    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToServer::messageReceiverName()) {
        if (auto idbConnection = m_webIDBConnectionsByIdentifier.get(decoder.destinationID()))
            idbConnection->didReceiveMessage(connection, decoder);
        return;
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWClientConnection::messageReceiverName()) {
        auto serviceWorkerConnection = m_swConnectionsByIdentifier.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID()));
        if (serviceWorkerConnection)
            serviceWorkerConnection->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::ServiceWorkerClientFetch::messageReceiverName()) {
        WebServiceWorkerProvider::singleton().didReceiveServiceWorkerClientFetchMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(decoder.destinationID()))
            webPage->paymentCoordinator()->didReceiveMessage(connection, decoder);
        return;
    }
#endif

    didReceiveNetworkProcessConnectionMessage(connection, decoder);
}

void NetworkProcessConnection::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveSyncMessage(connection, decoder, replyEncoder);
        return;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(decoder.destinationID()))
            webPage->paymentCoordinator()->didReceiveSyncMessage(connection, decoder, replyEncoder);
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

void NetworkProcessConnection::didClose(IPC::Connection&)
{
    // The NetworkProcess probably crashed.
    Ref<NetworkProcessConnection> protector(*this);
    WebProcess::singleton().networkProcessConnectionClosed(this);

#if ENABLE(INDEXED_DATABASE)
    for (auto& connection : m_webIDBConnectionsByIdentifier.values())
        connection->connectionToServerLost();
    
    m_webIDBConnectionsByIdentifier.clear();
    m_webIDBConnectionsBySession.clear();
#endif

#if ENABLE(SERVICE_WORKER)
    for (auto& connection : m_swConnectionsBySession.values())
        connection->connectionToServerLost();
    
    m_swConnectionsByIdentifier.clear();
    m_swConnectionsBySession.clear();
#endif
}

void NetworkProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkProcessConnection::writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::WriteBlobsToTemporaryFiles(blobURLs), WTFMove(completionHandler));
}

void NetworkProcessConnection::didFinishPingLoad(uint64_t pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    WebProcess::singleton().webLoaderStrategy().didFinishPingLoad(pingLoadIdentifier, WTFMove(error), WTFMove(response));
}

void NetworkProcessConnection::didFinishPreconnection(uint64_t preconnectionIdentifier, ResourceError&& error)
{
    WebProcess::singleton().webLoaderStrategy().didFinishPreconnection(preconnectionIdentifier, WTFMove(error));
}

void NetworkProcessConnection::setOnLineState(bool isOnLine)
{
    WebProcess::singleton().webLoaderStrategy().setOnLineState(isOnLine);
}

#if ENABLE(SHAREABLE_RESOURCE)
void NetworkProcessConnection::didCacheResource(const ResourceRequest& request, const ShareableResource::Handle& handle, PAL::SessionID sessionID)
{
    CachedResource* resource = MemoryCache::singleton().resourceForRequest(request, sessionID);
    if (!resource)
        return;
    
    RefPtr<SharedBuffer> buffer = handle.tryWrapInSharedBuffer();
    if (!buffer) {
        LOG_ERROR("Unable to create SharedBuffer from ShareableResource handle for resource url %s", request.url().string().utf8().data());
        return;
    }

    resource->tryReplaceEncodedData(*buffer);
}
#endif

#if ENABLE(INDEXED_DATABASE)
WebIDBConnectionToServer& NetworkProcessConnection::idbConnectionToServerForSession(PAL::SessionID sessionID)
{
    return *m_webIDBConnectionsBySession.ensure(sessionID, [&] {
        auto connection = WebIDBConnectionToServer::create(sessionID);
        
        auto result = m_webIDBConnectionsByIdentifier.add(connection->identifier(), connection.copyRef());
        ASSERT_UNUSED(result, result.isNewEntry);
        
        return connection;
    }).iterator->value;
}
#endif

#if ENABLE(SERVICE_WORKER)
WebSWClientConnection& NetworkProcessConnection::serviceWorkerConnectionForSession(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    return *m_swConnectionsBySession.ensure(sessionID, [&] {
        auto connection = WebSWClientConnection::create(m_connection, sessionID);
        
        auto result = m_swConnectionsByIdentifier.add(connection->serverConnectionIdentifier(), connection.ptr());
        ASSERT_UNUSED(result, result.isNewEntry);
        
        return connection;
    }).iterator->value;
}
#endif
} // namespace WebKit
