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

#ifndef NetworkProcessConnection_h
#define NetworkProcessConnection_h

#include "Connection.h"
#include "ShareableResource.h"
#include <WebCore/MessagePortChannelProvider.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class DataReference;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
struct MessagePortIdentifier;
struct MessageWithMessagePorts;
enum class HTTPCookieAcceptPolicy : uint8_t;
}

namespace WebKit {

class WebIDBConnectionToServer;
class WebSWClientConnection;

typedef uint64_t ResourceLoadIdentifier;

class NetworkProcessConnection : public RefCounted<NetworkProcessConnection>, IPC::Connection::Client {
public:
    static Ref<NetworkProcessConnection> create(IPC::Connection::Identifier connectionIdentifier, WebCore::HTTPCookieAcceptPolicy httpCookieAcceptPolicy)
    {
        return adoptRef(*new NetworkProcessConnection(connectionIdentifier, httpCookieAcceptPolicy));
    }
    ~NetworkProcessConnection();
    
    IPC::Connection& connection() { return m_connection.get(); }

    void didReceiveNetworkProcessConnectionMessage(IPC::Connection&, IPC::Decoder&);

    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&);

#if ENABLE(INDEXED_DATABASE)
    WebIDBConnectionToServer* existingIDBConnectionToServer() const { return m_webIDBConnection.get(); };
    WebIDBConnectionToServer& idbConnectionToServer();
#endif

#if ENABLE(SERVICE_WORKER)
    WebSWClientConnection& serviceWorkerConnection();
#endif

#if HAVE(AUDIT_TOKEN)
    void setNetworkProcessAuditToken(Optional<audit_token_t> auditToken) { m_networkProcessAuditToken = auditToken; }
    Optional<audit_token_t> networkProcessAuditToken() const { return m_networkProcessAuditToken; }
#endif

    bool cookiesEnabled() const;

private:
    NetworkProcessConnection(IPC::Connection::Identifier, WebCore::HTTPCookieAcceptPolicy);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    void didFinishPingLoad(uint64_t pingLoadIdentifier, WebCore::ResourceError&&, WebCore::ResourceResponse&&);
    void didFinishPreconnection(uint64_t preconnectionIdentifier, WebCore::ResourceError&&);
    void setOnLineState(bool isOnLine);
    void cookieAcceptPolicyChanged(WebCore::HTTPCookieAcceptPolicy);

    void checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier&, CompletionHandler<void(WebCore::MessagePortChannelProvider::HasActivity)>&&);
    void messagesAvailableForPort(const WebCore::MessagePortIdentifier&);

#if ENABLE(SHAREABLE_RESOURCE)
    // Message handlers.
    void didCacheResource(const WebCore::ResourceRequest&, const ShareableResource::Handle&);
#endif

    // The connection from the web process to the network process.
    Ref<IPC::Connection> m_connection;
#if HAVE(AUDIT_TOKEN)
    Optional<audit_token_t> m_networkProcessAuditToken;
#endif

#if ENABLE(INDEXED_DATABASE)
    RefPtr<WebIDBConnectionToServer> m_webIDBConnection;
#endif

#if ENABLE(SERVICE_WORKER)
    RefPtr<WebSWClientConnection> m_swConnection;
#endif
    WebCore::HTTPCookieAcceptPolicy m_cookieAcceptPolicy;
};

} // namespace WebKit

#endif // NetworkProcessConnection_h
