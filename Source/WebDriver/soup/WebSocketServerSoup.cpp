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
#include <libsoup/soup.h>
#include <optional>
#include <tuple>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

static bool soupServerListen(SoupServer* server, const std::optional<String>& host, unsigned port, GError** error)
{
    static const auto options = static_cast<SoupServerListenOptions>(0);

    if (!host || host.value() == "local"_s)
        return soup_server_listen_local(server, port, options, error);

    if (host.value() == "all"_s)
        return soup_server_listen_all(server, port, options, error);

    GRefPtr<GSocketAddress> address = adoptGRef(g_inet_socket_address_new_from_string(host.value().utf8().data(), port));
    if (!address) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Invalid host IP address '%s'", host.value().utf8().data());
        return false;
    }

    return soup_server_listen(server, address.get(), options, error);
}

std::optional<String> WebSocketServer::listen(const std::optional<String>& host, unsigned port)
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
    soup_server_add_handler(m_soupServer.get(), nullptr, [](SoupServer* server, SoupServerMessage* message, const char *path, GHashTable*, gpointer userData) {
        if (!soup_websocket_server_check_handshake(message, nullptr, nullptr, nullptr, nullptr)) {
            soup_server_message_set_status(message, 500, "Invalid request");
            return;
        }
        GRefPtr<SoupServerMessage> protectedMessage = message;
        soup_server_pause_message(server, message);
        auto* webSocketServer = static_cast<WebSocketServer*>(userData);
        if (!webSocketServer) {
            soup_server_message_set_status(message, 500, "Invalid server state");
            return;
        }

        auto* requestBody = soup_server_message_get_request_body(message);
        HTTPRequestHandler::Request handshakeMessage = {
            String::fromUTF8(soup_server_message_get_method(message)),
            String::fromUTF8(path),
            requestBody->data,
            static_cast<size_t>(requestBody->length)
        };

        webSocketServer->m_messageHandler.handleHandshake(WTFMove(handshakeMessage), [server, message](std::optional<HTTPRequestHandler::Response>&& response) {
            if (!response) {
                // No error found, follow with handshake procedure
                soup_server_unpause_message(server, message);
            } else {
                // Error found, send the error response
                WTFLogAlways("Error during handshake, sending error response: %s", response->data.data());
                soup_server_message_set_status(message, response->statusCode, nullptr);
                auto* responseHeaders = soup_server_message_get_response_headers(message);
                soup_message_headers_append(responseHeaders, "Content-Type", response->contentType.utf8().data());
                soup_message_headers_append(responseHeaders, "Cache-Control", "no-cache");
                auto* responseBody = soup_server_message_get_response_body(message);
                soup_message_body_append(responseBody, SOUP_MEMORY_COPY, response->data.data(), response->data.length());
            }
        });
    }, this, nullptr);

    // Callback called when the WebSocket handshake has finished and we have a valid WebSocket Connection object
    soup_server_add_websocket_handler(m_soupServer.get(), nullptr, nullptr, nullptr, [](SoupServer*, SoupServerMessage*, const char* path, SoupWebsocketConnection* connection, gpointer userData) {
        WTFLogAlways("websocket connection handler called for path %s", path);

        // Delayed steps from the end of the handshake, as now we have a connection object
        auto webSocketServer = static_cast<WebSocketServer*>(userData);
        if ("/session"_s == path) {
            // 4.2.2  Add the connection to WebSocket connections not associated with a session.
            webSocketServer->addToConnectionsNotAssociatedToASession({ connection });
        } else {
            // 4.6 Otherwise append connection to session’s session WebSocket connections, and proceed with the WebSocket server-side requirements when a server chooses to accept an incoming connection.
            auto resourceName = String::fromLatin1(path);
            auto sessionId = webSocketServer->getSessionIDForWebSocketResource(resourceName);
            auto connectionForKey = WebSocketMessageHandler::Connection(connection);
            webSocketServer->m_connectionToSession.push_back(std::make_pair(connectionForKey, sessionId));
        }

        g_signal_connect(connection, "closed", G_CALLBACK(+[](SoupWebsocketConnection* connection, gpointer userData) {
            auto* uri = soup_websocket_connection_get_uri(connection);
            auto* uriStr = g_uri_to_string(uri);
            WTFLogAlways("websocket connection closed for uri %s", uriStr);
            free(uriStr);

            auto webSocketServer = static_cast<WebSocketServer*>(userData);
            webSocketServer->m_messageHandler.clientDisconnected(connection);
            // FIXME remove the connection from the list of connections not associated to a session
        }), webSocketServer);

        g_signal_connect(connection, "message", G_CALLBACK(+[](SoupWebsocketConnection* connection, SoupWebsocketDataType messageType, GBytes* message, gpointer userData) {

            auto* uri = soup_websocket_connection_get_uri(connection);
            auto* uriStr = g_uri_to_string(uri);
            WTFLogAlways("websocket message handler called for connection on uri %s", uriStr);
            free(uriStr);

            // https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message
            // 1. If type is not text, send an error response given connection, null, and invalid argument, and finally return.
            if (messageType != SOUP_WEBSOCKET_DATA_TEXT) {
                WTFLogAlways("websocket message handler received non-text message. error return");
                auto errorReply = WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, std::nullopt, { "Non-text message received"_s });
                GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(errorReply.data, errorReply.dataLength));
                soup_websocket_connection_send_message(connection, SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
                return;
            }
            auto webSocketServer = static_cast<WebSocketServer*>(userData);

            gsize messageSize;
            gconstpointer messageData = g_bytes_get_data(message, &messageSize);
            webSocketServer->m_messageHandler.handleMessage({ connection, static_cast<const char*>(messageData), static_cast<size_t>(messageSize) },
                [](WebSocketMessageHandler::Message&& message) {
                if (!message.connection) {
                    WTFLogAlways("No connection found when trying to send message: %s", message.data);
                    return;
                }
                GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(message.data, message.dataLength));
                // Using send_message to avoid dealing with null chars in the middle of the message
                soup_websocket_connection_send_message(message.connection->get(), SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
            });

        }), webSocketServer);
    }, this, nullptr);

    // "/session" is the default resource to start bidi-only sessions
    m_listener = WebSocketListener::create(
        host ? *host : "localhost"_s,
        port,
        false,
        { "/session"_s }
    );
    return constructWebSocketURLForListenerAndSession(m_listener, nullString());
}

void WebSocketServer::sendMessageFromSession(const String& session, const String& message)
{
    for (const auto& pair : m_connectionToSession) {
        if (pair.second == session) {
            fprintf(stderr, "%s %s %d sending message %s\n", __FILE__, __FUNCTION__, __LINE__, message.utf8().data());
            GRefPtr<GBytes> rawMessage = adoptGRef(g_bytes_new(message.utf8().data(), message.length()));
            soup_websocket_connection_send_message(pair.first.get(), SOUP_WEBSOCKET_DATA_TEXT, rawMessage.get());
            return;
        }
    }
}

bool WebSocketServer::isListening()
{
    return !!m_soupServer;
}

void WebSocketServer::disconnect()
{
    soup_server_disconnect(m_soupServer.get());
    m_soupServer = nullptr;
}

} // namespace WebDriver
