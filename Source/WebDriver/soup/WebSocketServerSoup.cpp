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

#include "config.h"
#include "WebSocketServer.h"

#include "CommandResult.h"
#include "HTTPServer.h"
#include <cstdio>
#include <libsoup/soup-websocket-connection.h>
#include <libsoup/soup.h>
#include <optional>
#include <span>
#include <tuple>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

static bool soupServerListen(SoupServer* server, const String& host, unsigned port, GError** error)
{
    static const auto options = static_cast<SoupServerListenOptions>(0);

    if (!host || host == "local"_s)
        return soup_server_listen_local(server, port, options, error);

    if (host == "all"_s)
        return soup_server_listen_all(server, port, options, error);

    GRefPtr<GSocketAddress> address = adoptGRef(g_inet_socket_address_new_from_string(host.utf8().data(), port));
    if (!address) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Invalid host IP address '%s'", host.utf8().data());
        return false;
    }

    return soup_server_listen(server, address.get(), options, error);
}

static void handleIncomingHandshake(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer userData)
{
    if (!soup_websocket_server_check_handshake(message, nullptr, nullptr, nullptr, nullptr)) {
        soup_server_message_set_status(message, 500, "Invalid request");
        return;
    }
    GRefPtr<SoupServerMessage> protectedMessage = message;
    auto* requestBody = soup_server_message_get_request_body(message);
    HTTPRequestHandler::Request handshakeMessage = {
        String::fromUTF8(soup_server_message_get_method(message)),
        String::fromUTF8(path),
        requestBody->data,
        static_cast<size_t>(requestBody->length)
    };

    auto* webSocketServer = static_cast<WebSocketServer*>(userData);
    if (webSocketServer->messageHandler().acceptHandshake(WTFMove(handshakeMessage))) // Follow with handshake procedure
        return;

    HTTPRequestHandler::Response errorResponse = { 503, "Service Unavailable", "text/plain"_s };
    WTFLogAlways("Error during handshake, sending error response: %s", errorResponse.data.data());
    soup_server_message_set_status(message, errorResponse.statusCode, nullptr);
    auto* responseHeaders = soup_server_message_get_response_headers(message);
    soup_message_headers_append(responseHeaders, "Content-Type", errorResponse.contentType.utf8().data());
    soup_message_headers_append(responseHeaders, "Cache-Control", "no-cache");
    auto* responseBody = soup_server_message_get_response_body(message);
    soup_message_body_append(responseBody, SOUP_MEMORY_COPY, errorResponse.data.data(), errorResponse.data.length());
}

static void handleWebSocketMessage(SoupWebsocketConnection* connection, SoupWebsocketDataType messageType, GBytes* message, gpointer userData)
{
    // https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message
    if (messageType != SOUP_WEBSOCKET_DATA_TEXT) {
        WTFLogAlways("websocket message handler received non-text message. error return");
        auto errorReply = WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, std::nullopt, { "Non-text message received"_s });
        GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(errorReply.payload.data(), errorReply.payload.length()));
        soup_websocket_connection_send_message(connection, SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
        return;
    }
    auto webSocketServer = static_cast<WebSocketServer*>(userData);

    gsize messageSize;
    gconstpointer messageData = g_bytes_get_data(message, &messageSize);
    WebSocketMessageHandler::Message messageObj = { connection, { std::span<const char>(static_cast<const char*>(messageData), messageSize) } };
    webSocketServer->messageHandler().handleMessage(WTFMove(messageObj), [](WebSocketMessageHandler::Message&& message) {
        if (!message.connection) {
            WTFLogAlways("No connection found when trying to send message: %s", message.payload.data());
            return;
        }
        GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(message.payload.data(), message.payload.length()));
        // Using send_message to avoid dealing with null chars in the middle of the message
        soup_websocket_connection_send_message(message.connection.get(), SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
    });
}

static void handleWebSocketConnection(SoupServer*, SoupServerMessage*, const char* path, SoupWebsocketConnection* connection, gpointer userData)
{
    // Delayed steps from the end of the handshake, as now we have a connection object
    auto webSocketServer = static_cast<WebSocketServer*>(userData);
    if ("/session"_s == path)
        webSocketServer->addStaticConnection({ connection });
    else {
        auto sessionId = webSocketServer->getSessionID(String::fromLatin1(path));
        auto connectionForKey = WebSocketMessageHandler::Connection(connection);
        webSocketServer->addConnection({ connection }, sessionId);
    }

    g_signal_connect(connection, "closed", G_CALLBACK(+[](SoupWebsocketConnection* connection, gpointer userData) {
        auto webSocketServer = static_cast<WebSocketServer*>(userData);
        webSocketServer->messageHandler().clientDisconnected(connection);
    }), webSocketServer);

    g_signal_connect(connection, "message", G_CALLBACK(handleWebSocketMessage), webSocketServer);
}

std::optional<String> WebSocketServer::listen(const String& host, unsigned port)
{
#if USE(SOUP2)
    WTFLogAlways("WebSockets support not implemented yet with libsoup2");
    return stf::nullopt;
#endif

    m_soupServer = adoptGRef(soup_server_new("server-header", "WebKitWebDriver-WSS", nullptr));
    GUniqueOutPtr<GError> error;
    if (!soupServerListen(m_soupServer.get(), host, port, &error.outPtr())) {
        WTFLogAlways("Failed to start WebSocket server at port %u: %s", port, error->message);
        return std::nullopt;
    }

    // Callback for handling incoming WebSocket handshake requests
    soup_server_add_handler(m_soupServer.get(), nullptr, handleIncomingHandshake, this, nullptr);

    // Callback called when the WebSocket handshake has finished and we have a valid WebSocket Connection object
    soup_server_add_websocket_handler(m_soupServer.get(), nullptr, nullptr, nullptr, handleWebSocketConnection, this, nullptr);

    // "/session" is the default resource to start bidi-only sessions
    m_listener = WebSocketListener::create(
        host.isNull() ? "localhost"_s : host,
        port,
        false,
        { "/session"_s }
    );
    return getWebSocketURL(m_listener, nullString());
}

void WebSocketServer::sendMessage(const String& session, const String& message)
{
    for (const auto& pair : m_connectionToSession) {
        if (pair.value == session) {
            GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(message.utf8().data(), message.utf8().length()));
            soup_websocket_connection_send_message(pair.key.get(), SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
            return;
        }
    }
}

void WebSocketServer::disconnect()
{
    soup_server_disconnect(m_soupServer.get());
    m_soupServer = nullptr;
}

void WebSocketServer::disconnectSession(const String& sessionId)
{
    auto connection = this->connection(sessionId);
    if (!connection)
        return;

    soup_websocket_connection_close(connection->get(), SOUP_WEBSOCKET_CLOSE_NORMAL, nullptr);
}

} // namespace WebDriver
