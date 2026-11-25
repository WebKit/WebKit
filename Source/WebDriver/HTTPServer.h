/*
 * Copyright (C) 2017 Igalia S.L.
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

#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupServer SoupServer;
#elif USE(INSPECTOR_SOCKET_SERVER)
// Windows and PlayStation build the remote inspector's socket transport (USE(INSPECTOR_SOCKET_SERVER)),
// and the HTTP server uses its socket endpoint.
#include "HTTPParser.h"
#include <JavaScriptCore/RemoteInspectorSocketEndpoint.h>

using Inspector::ConnectionID;
using Inspector::PlatformSocketType;
using Inspector::RemoteInspectorSocketEndpoint;
#elif PLATFORM(COCOA)
#include <Network/Network.h>
#include <wtf/OSObjectPtr.h>
#endif

namespace WebDriver {

class HTTPRequestHandler
#if USE(INSPECTOR_SOCKET_SERVER)
: public RemoteInspectorSocketEndpoint::Client
#endif
{
public:
    virtual ~HTTPRequestHandler() = default;

    struct Request {
        RefPtr<StringImpl> method;
        RefPtr<StringImpl> path;
        const char* data { nullptr };
        size_t dataLength { 0 };
    };
    struct Response {
        unsigned statusCode { 0 };
        UTF8CString data;
        RefPtr<StringImpl> contentType;
    };
    virtual void handleRequest(Request&&, Function<void (Response&&)>&& replyHandler) = 0;

    void setAllowedHosts(Vector<RefPtr<StringImpl>>&& hosts) { m_allowedHosts = WTF::move(hosts); }
    void setAllowedOrigins(Vector<URL>&& origins) { m_allowedOrigins = WTF::move(origins); }

    std::expected<void, RefPtr<StringImpl>> validateRequestHeaders(StringImpl* hostHeader, StringImpl* originHeader, StringImpl* method = nullptr, StringImpl* contentTypeHeader = nullptr) const;
    static Response rejectedRequestResponse(StringImpl* reason);

#if USE(INSPECTOR_SOCKET_SERVER)
    void connect(ConnectionID);
#endif

private:
    Vector<RefPtr<StringImpl>> m_allowedHosts;
    Vector<URL> m_allowedOrigins;

#if USE(INSPECTOR_SOCKET_SERVER)
    void sendResponse(Response&&);
    void reset();
    RefPtr<StringImpl> packHTTPMessage(Response&&) const;

    void didReceive(RemoteInspectorSocketEndpoint&, ConnectionID, Vector<uint8_t>&&) final;
    void didClose(RemoteInspectorSocketEndpoint&, ConnectionID) final;

    std::optional<ConnectionID> m_client;
    HTTPParser m_parser;
#endif
};

class HTTPServer
#if USE(INSPECTOR_SOCKET_SERVER)
: public RemoteInspectorSocketEndpoint::Listener
#endif
{
public:
    explicit HTTPServer(HTTPRequestHandler&);
    ~HTTPServer() = default;

    bool listen(const std::optional<RefPtr<StringImpl>>& host, unsigned port);
    void disconnect();

    const RefPtr<StringImpl>& visibleHost() const LIFETIME_BOUND { return m_visibleHost; }

private:
#if USE(INSPECTOR_SOCKET_SERVER)
    std::optional<ConnectionID> doAccept(RemoteInspectorSocketEndpoint&, PlatformSocketType) final;
    void didChangeStatus(RemoteInspectorSocketEndpoint&, ConnectionID, RemoteInspectorSocketEndpoint::Listener::Status) final;
#endif

#if PLATFORM(COCOA)
    std::optional<uint16_t> startListener(ASCIILiteral address, uint16_t port);
#endif

    HTTPRequestHandler& m_requestHandler;
    RefPtr<StringImpl> m_visibleHost;

#if USE(SOUP)
    GRefPtr<SoupServer> m_soupServer;
#endif

#if USE(INSPECTOR_SOCKET_SERVER)
    std::optional<ConnectionID> m_server;
#endif

#if PLATFORM(COCOA)
    Vector<OSObjectPtr<nw_listener_t>> m_listeners;
#endif
};

} // namespace WebDriver
