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
#include "RTCDataChannelRemoteManager.h"
#include "StorageAreaMap.h"
#include "StorageAreaMapMessages.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebBroadcastChannelRegistryMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCookieJar.h"
#include "WebCoreArgumentCoders.h"
#include "WebFileSystemStorageConnection.h"
#include "WebFileSystemStorageConnectionMessages.h"
#include "WebFrame.h"
#include "WebIDBConnectionToServer.h"
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
#include "WebResourceLoaderMessages.h"
#include "WebSWClientConnection.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include "WebSharedWorkerContextManagerConnection.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerObjectConnection.h"
#include "WebSharedWorkerObjectConnectionMessages.h"
#include "WebSocketChannel.h"
#include "WebSocketChannelMessages.h"
#include "WebSocketStream.h"
#include "WebSocketStreamMessages.h"
#include <WebCore/CachedResource.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/InspectorInstrumentationWebKit.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MessagePort.h>
#include <WebCore/SharedBuffer.h>
#include <pal/SessionID.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

NetworkProcessConnection::NetworkProcessConnection(IPC::Connection::Identifier connectionIdentifier, HTTPCookieAcceptPolicy cookieAcceptPolicy)
    : m_connection(IPC::Connection::createClientConnection(connectionIdentifier))
    , m_cookieAcceptPolicy(cookieAcceptPolicy)
{
    m_connection->open(*this);

    if (WebRTCProvider::webRTCAvailable())
        WebProcess::singleton().libWebRTCNetwork().setConnection(m_connection.copyRef());
}

NetworkProcessConnection::~NetworkProcessConnection()
{
    m_connection->invalidate();
}

void NetworkProcessConnection::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebResourceLoader::messageReceiverName()) {
        if (auto* webResourceLoader = WebProcess::singleton().webLoaderStrategy().webResourceLoaderForIdentifier(makeObjectIdentifier<WebCore::ResourceLoader>(decoder.destinationID())))
            webResourceLoader->didReceiveWebResourceLoaderMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebBroadcastChannelRegistry::messageReceiverName()) {
        WebProcess::singleton().broadcastChannelRegistry().didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSocketStream::messageReceiverName()) {
        if (auto* stream = WebSocketStream::streamWithIdentifier(makeObjectIdentifier<WebSocketIdentifierType>(decoder.destinationID())))
            stream->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSocketChannel::messageReceiverName()) {
        WebProcess::singleton().webSocketChannelManager().didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebPage::messageReceiverName()) {
        if (auto* webPage = WebProcess::singleton().webPage(makeObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            webPage->didReceiveWebPageMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::StorageAreaMap::messageReceiverName()) {
        if (auto storageAreaMap = WebProcess::singleton().storageAreaMap(makeObjectIdentifier<StorageAreaMapIdentifierType>(decoder.destinationID())))
            storageAreaMap->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebFileSystemStorageConnection::messageReceiverName()) {
        WebProcess::singleton().fileSystemStorageConnection().didReceiveMessage(connection, decoder);
        return;
    }

#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::WebRTCMonitor::messageReceiverName()) {
        auto& network = WebProcess::singleton().libWebRTCNetwork();
        if (network.isActive())
            network.monitor().didReceiveMessage(connection, decoder);
        else
            RELEASE_LOG_ERROR(WebRTC, "Received WebRTCMonitor message while libWebRTCNetwork is not active");
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebRTCResolver::messageReceiverName()) {
        auto& network = WebProcess::singleton().libWebRTCNetwork();
        if (network.isActive())
            network.resolver(makeObjectIdentifier<LibWebRTCResolverIdentifierType>(decoder.destinationID())).didReceiveMessage(connection, decoder);
        else
            RELEASE_LOG_ERROR(WebRTC, "Received WebRTCResolver message while libWebRTCNetwork is not active");
        return;
    }
#endif
#if ENABLE(WEB_RTC)
    if (decoder.messageReceiverName() == Messages::WebMDNSRegister::messageReceiverName()) {
        auto& network = WebProcess::singleton().libWebRTCNetwork();
        if (network.isActive())
            network.mdnsRegister().didReceiveMessage(connection, decoder);
        else
            RELEASE_LOG_ERROR(WebRTC, "Received WebMDNSRegister message while libWebRTCNetwork is not active");
        return;
    }
#endif

    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToServer::messageReceiverName()) {
        if (m_webIDBConnection)
            m_webIDBConnection->didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWClientConnection::messageReceiverName()) {
        serviceWorkerConnection().didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }
#endif
    if (decoder.messageReceiverName() == Messages::WebSharedWorkerObjectConnection::messageReceiverName()) {
        sharedWorkerConnection().didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSharedWorkerContextManagerConnection::messageReceiverName()) {
        ASSERT(SharedWorkerContextManager::singleton().connection());
        if (auto* contextManagerConnection = SharedWorkerContextManager::singleton().connection())
            static_cast<WebSharedWorkerContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(makeObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            webPage->paymentCoordinator()->didReceiveMessage(connection, decoder);
        return;
    }
#endif

    didReceiveNetworkProcessConnectionMessage(connection, decoder);
}

bool NetworkProcessConnection::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            return static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveSyncMessage(connection, decoder, replyEncoder);
        return false;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(makeObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            return webPage->paymentCoordinator()->didReceiveSyncMessage(connection, decoder, replyEncoder);
        return false;
    }
#endif

    ASSERT_NOT_REACHED();
    return false;
}

void NetworkProcessConnection::didClose(IPC::Connection&)
{
    // The NetworkProcess probably crashed.
    Ref<NetworkProcessConnection> protector(*this);
    WebProcess::singleton().networkProcessConnectionClosed(this);

    if (auto idbConnection = std::exchange(m_webIDBConnection, nullptr))
        idbConnection->connectionToServerLost();

#if ENABLE(SERVICE_WORKER)
    if (auto swConnection = std::exchange(m_swConnection, nullptr))
        swConnection->connectionToServerLost();
#endif
}

void NetworkProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

void NetworkProcessConnection::writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler)
{
    connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::WriteBlobsToTemporaryFilesForIndexedDB(blobURLs), WTFMove(completionHandler));
}

void NetworkProcessConnection::didFinishPingLoad(WebCore::ResourceLoaderIdentifier pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    WebProcess::singleton().webLoaderStrategy().didFinishPingLoad(pingLoadIdentifier, WTFMove(error), WTFMove(response));
}

void NetworkProcessConnection::didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, ResourceError&& error)
{
    WebProcess::singleton().webLoaderStrategy().didFinishPreconnection(preconnectionIdentifier, WTFMove(error));
}

void NetworkProcessConnection::setOnLineState(bool isOnLine)
{
    WebProcess::singleton().webLoaderStrategy().setOnLineState(isOnLine);
}

bool NetworkProcessConnection::cookiesEnabled() const
{
    return m_cookieAcceptPolicy != HTTPCookieAcceptPolicy::Never;
}

void NetworkProcessConnection::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    m_cookieAcceptPolicy = newPolicy;
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
void NetworkProcessConnection::cookiesAdded(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    WebProcess::singleton().cookieJar().cookiesAdded(host, cookies);
}

void NetworkProcessConnection::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    WebProcess::singleton().cookieJar().cookiesDeleted(host, cookies);
}

void NetworkProcessConnection::allCookiesDeleted()
{
    WebProcess::singleton().cookieJar().allCookiesDeleted();
}
#endif

#if ENABLE(SHAREABLE_RESOURCE)
void NetworkProcessConnection::didCacheResource(const ResourceRequest& request, const ShareableResource::Handle& handle)
{
    auto* resource = MemoryCache::singleton().resourceForRequest(request, WebProcess::singleton().sessionID());
    if (!resource)
        return;
    
    auto buffer = handle.tryWrapInSharedBuffer();
    if (!buffer) {
        LOG_ERROR("Unable to create FragmentedSharedBuffer from ShareableResource handle for resource url %s", request.url().string().utf8().data());
        return;
    }

    resource->tryReplaceEncodedData(*buffer);
}
#endif

WebIDBConnectionToServer& NetworkProcessConnection::idbConnectionToServer()
{
    if (!m_webIDBConnection)
        m_webIDBConnection = WebIDBConnectionToServer::create();
    return *m_webIDBConnection;
}

#if ENABLE(SERVICE_WORKER)
WebSWClientConnection& NetworkProcessConnection::serviceWorkerConnection()
{
    if (!m_swConnection)
        m_swConnection = WebSWClientConnection::create();
    return *m_swConnection;
}
#endif

WebSharedWorkerObjectConnection& NetworkProcessConnection::sharedWorkerConnection()
{
    if (!m_sharedWorkerConnection)
        m_sharedWorkerConnection = WebSharedWorkerObjectConnection::create();
    return *m_sharedWorkerConnection;
}

void NetworkProcessConnection::messagesAvailableForPort(const WebCore::MessagePortIdentifier& messagePortIdentifier)
{
    WebProcess::singleton().messagesAvailableForPort(messagePortIdentifier);
}

void NetworkProcessConnection::checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier& messagePortIdentifier, CompletionHandler<void(MessagePortChannelProvider::HasActivity)>&& callback)
{
    callback(WebCore::MessagePort::isExistingMessagePortLocallyReachable(messagePortIdentifier) ? MessagePortChannelProvider::HasActivity::Yes : MessagePortChannelProvider::HasActivity::No);
}

void NetworkProcessConnection::deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    WebProcess::singleton().deleteWebsiteDataForOrigins(dataTypes, origins, WTFMove(completionHandler));
}

void NetworkProcessConnection::broadcastConsoleMessage(MessageSource source, MessageLevel level, const String& message)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    Page::forEachPage([&] (Page& page) {
        WebPage::fromCorePage(page).mainWebFrame().addConsoleMessage(source, level, message);
    });
}

#if ENABLE(WEB_RTC)
void NetworkProcessConnection::connectToRTCDataChannelRemoteSource(WebCore::RTCDataChannelIdentifier localIdentifier, WebCore::RTCDataChannelIdentifier remoteIdentifier, CompletionHandler<void(std::optional<bool>)>&& callback)
{
    callback(RTCDataChannelRemoteManager::sharedManager().connectToRemoteSource(localIdentifier, remoteIdentifier));
}
#endif

} // namespace WebKit
