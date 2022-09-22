/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#pragma once

#include "Connection.h"
#include "ShareableResource.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/MessagePortChannelProvider.h>
#include <WebCore/RTCDataChannelIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
struct Cookie;
struct MessagePortIdentifier;
struct MessageWithMessagePorts;
struct SecurityOriginData;
enum class HTTPCookieAcceptPolicy : uint8_t;
}

namespace WebKit {

class WebIDBConnectionToServer;
class WebSharedWorkerObjectConnection;
class WebSWClientConnection;
class WebSharedWorkerObjectConnection;

enum class WebsiteDataType : uint32_t;

class NetworkProcessConnection : public RefCounted<NetworkProcessConnection>, IPC::Connection::Client {
public:
    static Ref<NetworkProcessConnection> create(IPC::Connection::Identifier&& connectionIdentifier, WebCore::HTTPCookieAcceptPolicy httpCookieAcceptPolicy)
    {
        return adoptRef(*new NetworkProcessConnection(connectionIdentifier, httpCookieAcceptPolicy));
    }
    ~NetworkProcessConnection();
    
    IPC::Connection& connection() { return m_connection.get(); }

    void didReceiveNetworkProcessConnectionMessage(IPC::Connection&, IPC::Decoder&);

    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&);

    WebIDBConnectionToServer* existingIDBConnectionToServer() const { return m_webIDBConnection.get(); };
    WebIDBConnectionToServer& idbConnectionToServer();

#if ENABLE(SERVICE_WORKER)
    WebSWClientConnection& serviceWorkerConnection();
#endif
    WebSharedWorkerObjectConnection& sharedWorkerConnection();

#if HAVE(AUDIT_TOKEN)
    void setNetworkProcessAuditToken(std::optional<audit_token_t> auditToken) { m_networkProcessAuditToken = auditToken; }
    std::optional<audit_token_t> networkProcessAuditToken() const { return m_networkProcessAuditToken; }
#endif

    WebCore::HTTPCookieAcceptPolicy cookieAcceptPolicy() const { return m_cookieAcceptPolicy; }
    bool cookiesEnabled() const;

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&);
    void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&);
    void allCookiesDeleted();
#endif

private:
    NetworkProcessConnection(IPC::Connection::Identifier, WebCore::HTTPCookieAcceptPolicy);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    void didFinishPingLoad(WebCore::ResourceLoaderIdentifier pingLoadIdentifier, WebCore::ResourceError&&, WebCore::ResourceResponse&&);
    void didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, WebCore::ResourceError&&);
    void setOnLineState(bool isOnLine);
    void cookieAcceptPolicyChanged(WebCore::HTTPCookieAcceptPolicy);

    void checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier&, CompletionHandler<void(WebCore::MessagePortChannelProvider::HasActivity)>&&);
    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

#if ENABLE(SHAREABLE_RESOURCE)
    // Message handlers.
    void didCacheResource(const WebCore::ResourceRequest&, const ShareableResource::Handle&);
#endif
#if ENABLE(WEB_RTC)
    void connectToRTCDataChannelRemoteSource(WebCore::RTCDataChannelIdentifier source, WebCore::RTCDataChannelIdentifier handler, CompletionHandler<void(std::optional<bool>)>&&);
#endif

    void deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);

    void broadcastConsoleMessage(MessageSource, MessageLevel, const String& message);

    // The connection from the web process to the network process.
    Ref<IPC::Connection> m_connection;
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> m_networkProcessAuditToken;
#endif

    RefPtr<WebIDBConnectionToServer> m_webIDBConnection;

#if ENABLE(SERVICE_WORKER)
    RefPtr<WebSWClientConnection> m_swConnection;
#endif
    RefPtr<WebSharedWorkerObjectConnection> m_sharedWorkerConnection;
    WebCore::HTTPCookieAcceptPolicy m_cookieAcceptPolicy;
};

} // namespace WebKit
