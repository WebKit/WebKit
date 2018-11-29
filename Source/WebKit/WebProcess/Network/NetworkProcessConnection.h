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
#include "WebIDBConnectionToServer.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class DataReference;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class WebSWClientConnection;

typedef uint64_t ResourceLoadIdentifier;

class NetworkProcessConnection : public RefCounted<NetworkProcessConnection>, IPC::Connection::Client {
public:
    static Ref<NetworkProcessConnection> create(IPC::Connection::Identifier connectionIdentifier)
    {
        return adoptRef(*new NetworkProcessConnection(connectionIdentifier));
    }
    ~NetworkProcessConnection();
    
    IPC::Connection& connection() { return m_connection.get(); }

    void didReceiveNetworkProcessConnectionMessage(IPC::Connection&, IPC::Decoder&);

    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&);

#if ENABLE(INDEXED_DATABASE)
    WebIDBConnectionToServer* existingIDBConnectionToServerForIdentifier(uint64_t identifier) const { return m_webIDBConnectionsByIdentifier.get(identifier); };
    WebIDBConnectionToServer& idbConnectionToServerForSession(PAL::SessionID);
#endif

#if ENABLE(SERVICE_WORKER)
    WebSWClientConnection* existingServiceWorkerConnectionForSession(PAL::SessionID sessionID) { return m_swConnectionsBySession.get(sessionID); }
    WebSWClientConnection& serviceWorkerConnectionForSession(PAL::SessionID);
#endif

private:
    NetworkProcessConnection(IPC::Connection::Identifier);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    void didFinishPingLoad(uint64_t pingLoadIdentifier, WebCore::ResourceError&&, WebCore::ResourceResponse&&);
    void didFinishPreconnection(uint64_t preconnectionIdentifier, WebCore::ResourceError&&);
    void setOnLineState(bool isOnLine);

#if ENABLE(SHAREABLE_RESOURCE)
    // Message handlers.
    void didCacheResource(const WebCore::ResourceRequest&, const ShareableResource::Handle&, PAL::SessionID);
#endif

    // The connection from the web process to the network process.
    Ref<IPC::Connection> m_connection;

#if ENABLE(INDEXED_DATABASE)
    HashMap<PAL::SessionID, RefPtr<WebIDBConnectionToServer>> m_webIDBConnectionsBySession;
    HashMap<uint64_t, RefPtr<WebIDBConnectionToServer>> m_webIDBConnectionsByIdentifier;
#endif

#if ENABLE(SERVICE_WORKER)
    HashMap<PAL::SessionID, RefPtr<WebSWClientConnection>> m_swConnectionsBySession;
    HashMap<WebCore::SWServerConnectionIdentifier, WebSWClientConnection*> m_swConnectionsByIdentifier;
#endif
};

} // namespace WebKit

#endif // NetworkProcessConnection_h
