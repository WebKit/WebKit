/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "CommandResult.h"
#include "HTTPServer.h"
#include <map>
#include <optional>
#include <vector>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupServer SoupServer;
typedef struct _SoupWebsocketConnection SoupWebsocketConnection;
#elif PLATFORM(COCOA)
#include <Network/Network.h>
#include <wtf/HashSet.h>
#include <wtf/OSObjectPtr.h>
#endif

namespace WebDriver {

class Session;
class WebDriverService;

struct Command {
    // Defined as js-uint, but in practice opaque for the remote end.
    String id;
    String method;
    RefPtr<JSON::Object> parameters;

    static std::optional<Command> fromData(const char* data, size_t dataLength);
};

// https://w3c.github.io/webdriver-bidi/#websocket-listener
// A WebSocket listener is a network endpoint that is able to accept incoming WebSocket connections.
struct WebSocketListener : public RefCounted<WebSocketListener> {
    static RefPtr<WebSocketListener> create(const String& host, unsigned port, bool secure, const std::vector<String>& resources = { })
    {
        return adoptRef(*new WebSocketListener(host, port, secure, resources));
    }

    String host;
    unsigned port { 0 };
    bool isSecure { false };
    std::vector<String> resources { };

private:
    WebSocketListener(const String& host, unsigned port, bool isSecure, const std::vector<String>& resources)
        : host(host)
        , port(port)
        , isSecure(isSecure)
        , resources(resources)
    {
    }
};


class WebSocketMessageHandler {
public:
    virtual ~WebSocketMessageHandler() = default;

#if USE(SOUP)
    using Connection = GRefPtr<SoupWebsocketConnection>;
#elif PLATFORM(COCOA)
    using Connection = OSObjectPtr<nw_connection_t>;
#endif

    struct Message {
        // Optional connection, as the message might be generated without a connection object available (e.g. inside a method handler).
        // In this case, it gets associated to the connection when sending the message back to the client.
        Connection connection;
        // WebSocket text frames are UTF-8 by RFC 6455.
        const UTF8CString payload;

        static Message fail(CommandResult::ErrorCode, std::optional<Connection>, std::optional<String> errorMessage = std::nullopt, std::optional<int> commandId = std::nullopt);
        static Message reply(const String& type, unsigned id, Ref<JSON::Value>&& result);
    };

    // Same semantics as HTTPRequestHandler::validateRequestHeaders()
    virtual std::expected<void, String> validateHandshakeHeaders(const String& hostHeader, const String& originHeader) const = 0;
    virtual bool acceptHandshake(HTTPRequestHandler::Request&&) = 0;
    virtual void handleMessage(Message&&, Function<void(Message&&)>&& replyHandler) = 0;
    virtual void clientDisconnected(const Connection&) = 0;
private:
};

class WebSocketServer : public RefCountedAndCanMakeWeakPtr<WebSocketServer> {
public:
    static Ref<WebSocketServer> create(WebSocketMessageHandler& messageHandler)
    {
        return adoptRef(*new WebSocketServer(messageHandler));
    }
    virtual ~WebSocketServer() = default;

    std::optional<String> listen(const String& host, unsigned port);
    void disconnect();

    WebSocketMessageHandler& messageHandler() { return m_messageHandler; }

    const RefPtr<WebSocketListener>& listener() const { return m_listener; }

    void addStaticConnection(WebSocketMessageHandler::Connection&&);
    bool isStaticConnection(const WebSocketMessageHandler::Connection&);
    void removeStaticConnection(const WebSocketMessageHandler::Connection&);

    void addConnection(WebSocketMessageHandler::Connection&&, const String& sessionId);
    String sessionID(const WebSocketMessageHandler::Connection&) const;
    std::optional<WebSocketMessageHandler::Connection> connection(const String& sessionId);
    void removeConnection(const WebSocketMessageHandler::Connection&);

    RefPtr<WebSocketListener> startListening(const String& sessionId);
    String getResourceName(const String& sessionId);
    String getWebSocketURL(const RefPtr<WebSocketListener>, const String& sessionId);
    String getSessionID(const String& resource);
    void sendMessage(const String& sessionId, const String& message);
    void sendErrorResponse(const String& sessionId, std::optional<unsigned> commandId, CommandResult::ErrorCode, const String& errorMessage, std::optional<String> stacktrace = std::nullopt);

    // Non-spec method
    void removeResourceForSession(const String& sessionId);
    void disconnectSession(const String& sessionId);

private:
    explicit WebSocketServer(WebSocketMessageHandler&);

    void sendMessage(WebSocketMessageHandler::Connection, const String& message);
    void sendErrorResponse(WebSocketMessageHandler::Connection, std::optional<unsigned> commandId, CommandResult::ErrorCode, const String& errorMessage, std::optional<String> stacktrace = std::nullopt);

#if PLATFORM(COCOA)
    void startConnection(WebSocketMessageHandler::Connection&&);
    void didOpenConnection(const WebSocketMessageHandler::Connection&, const String& resourcePath);
    void receiveMessage(const WebSocketMessageHandler::Connection&);
    void didReceiveMessage(const WebSocketMessageHandler::Connection&, dispatch_data_t, nw_ws_opcode_t);
    void didCloseConnection(const WebSocketMessageHandler::Connection&);
#endif

    WebSocketMessageHandler& m_messageHandler;
    RefPtr<WebSocketListener> m_listener;
    HashMap<WebSocketMessageHandler::Connection, String> m_connectionToSession;

#if USE(SOUP)
    GRefPtr<SoupServer> m_soupServer;
    std::vector<GRefPtr<SoupWebsocketConnection>> m_staticConnections;
#elif PLATFORM(COCOA)
    OSObjectPtr<nw_listener_t> m_networkListener;
    std::vector<WebSocketMessageHandler::Connection> m_staticConnections;
    HashSet<WebSocketMessageHandler::Connection> m_openConnections;
#endif
};

}
