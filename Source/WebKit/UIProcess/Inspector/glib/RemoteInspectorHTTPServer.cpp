/*
 * Copyright (C) 2022 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteInspectorHTTPServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorClient.h"
#include <WebCore/SoupVersioning.h>
#include <wtf/FileSystem.h>
#include <wtf/URL.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

RemoteInspectorHTTPServer& RemoteInspectorHTTPServer::singleton()
{
    static RemoteInspectorHTTPServer server;
    return server;
}

bool RemoteInspectorHTTPServer::start(GRefPtr<GSocketAddress>&& socketAddress, unsigned inspectorPort)
{
    m_server = adoptGRef(soup_server_new("server-header", "WebKitInspectorHTTPServer ", nullptr));

    GUniqueOutPtr<GError> error;
    if (!soup_server_listen(m_server.get(), socketAddress.get(), static_cast<SoupServerListenOptions>(0), &error.outPtr())) {
        GUniquePtr<char> address(g_socket_connectable_to_string(G_SOCKET_CONNECTABLE(socketAddress.get())));
        g_warning("Failed to start remote inspector HTTP server on %s: %s", address.get(), error->message);
        return false;
    }

    soup_server_add_handler(m_server.get(), nullptr,
#if USE(SOUP2)
        [](SoupServer*, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer userData) {
#else
        [](SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer userData) {
#endif
            auto& httpServer = *static_cast<RemoteInspectorHTTPServer*>(userData);
            auto status = httpServer.handleRequest(path, soup_server_message_get_response_headers(message), soup_server_message_get_response_body(message));
            soup_server_message_set_status(message, status, nullptr);
        }, this, nullptr);

    soup_server_add_websocket_handler(m_server.get(), "/socket", nullptr, nullptr,
#if USE(SOUP2)
        [](SoupServer*, SoupWebsocketConnection* connection, const char* path, SoupClientContext*, gpointer userData) {
#else
        [](SoupServer*, SoupServerMessage*, const char* path, SoupWebsocketConnection* connection, gpointer userData) {
#endif
            auto& httpServer = *static_cast<RemoteInspectorHTTPServer*>(userData);
            httpServer.handleWebSocket(path, connection);
        }, this, nullptr);

    auto* inetAddress = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(socketAddress.get()));
    GUniquePtr<char> host(g_inet_address_to_string(inetAddress));
    GUniquePtr<char> inspectorServerAddress;
    if (g_inet_address_get_family(inetAddress) == G_SOCKET_FAMILY_IPV6)
        inspectorServerAddress.reset(g_strdup_printf("[%s]:%u", host.get(), inspectorPort));
    else
        inspectorServerAddress.reset(g_strdup_printf("%s:%u", host.get(), inspectorPort));
    m_client = makeUnique<RemoteInspectorClient>(String::fromUTF8(inspectorServerAddress.get()), *this);

    return true;
}

const String& RemoteInspectorHTTPServer::inspectorServerAddress() const
{
    return m_client ? m_client->hostAndPort() : emptyString();
}

unsigned RemoteInspectorHTTPServer::handleRequest(const char* path, SoupMessageHeaders* responseHeaders, SoupMessageBody* responseBody) const
{
    if (g_str_equal(path, "/")) {
        auto* html = m_client->buildTargetListPage(RemoteInspectorClient::InspectorType::HTTP);
        soup_message_headers_append(responseHeaders, "Content-Type", "text/html");
        gsize bodyLength = html->len;
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, g_string_free(html, FALSE), bodyLength);
        return SOUP_STATUS_OK;
    }

    GUniquePtr<char> resourcePath(g_build_filename("/org/webkit/inspector/UserInterface", path, nullptr));

    GUniqueOutPtr<GError> error;
    GRefPtr<GBytes> bytes = adoptGRef(g_resources_lookup_data(resourcePath.get(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error.outPtr()));
    if (bytes) {
        gsize dataSize;
        const auto* data = static_cast<const guchar*>(g_bytes_get_data(bytes.get(), &dataSize));
        GUniquePtr<char> fileName(g_path_get_basename(resourcePath.get()));
        GUniquePtr<char> contentType(g_content_type_guess(fileName.get(), data, dataSize, nullptr));
        soup_message_headers_append(responseHeaders, "Content-Type", contentType.get());
        soup_message_body_append(responseBody, SOUP_MEMORY_COPY, data, dataSize);
        return SOUP_STATUS_OK;
    }

    g_warning("Failed to load inspector resource %s: %s", resourcePath.get(), error->message);

    return SOUP_STATUS_NOT_FOUND;
}

void RemoteInspectorHTTPServer::handleWebSocket(const char* path, SoupWebsocketConnection* connection)
{
    auto items = String::fromUTF8(path).split('/');
    if (items.size() != 4)
        return;

    uint64_t connectionID = items[1].toDouble();
    uint64_t targetID = items[2].toDouble();
    m_webSocketConnectionMap.set(std::make_pair(connectionID, targetID), connection);
    m_webSocketConnectionToTargetMap.set(connection, std::make_pair(connectionID, targetID));
    g_signal_connect(connection, "message", G_CALLBACK(+[](SoupWebsocketConnection* connection, SoupWebsocketDataType messageType, GBytes* message, gpointer userData) {
        auto& httpServer = *static_cast<RemoteInspectorHTTPServer*>(userData);
        httpServer.sendMessageToBackend(connection, String::fromUTF8(span(message)));
    }), this);
    g_signal_connect(connection, "closed", G_CALLBACK(+[](SoupWebsocketConnection* connection, gpointer userData) {
        auto& httpServer = *static_cast<RemoteInspectorHTTPServer*>(userData);
        httpServer.didCloseWebSocket(connection);
    }), this);

    m_client->inspect(connectionID, targetID, items[3], RemoteInspectorClient::InspectorType::HTTP);
}

void RemoteInspectorHTTPServer::sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const String& message) const
{
    auto* webSocketConnection = m_webSocketConnectionMap.get(std::make_pair(connectionID, targetID));
    if (!webSocketConnection)
        return;

    auto utf8 = message.utf8();
    // Soup is going to copy the data immediately, so we can use g_bytes_new_static() here to avoid more data copies.
#if SOUP_CHECK_VERSION(2, 67, 3)
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_static(utf8.data(), utf8.length()));
    soup_websocket_connection_send_message(webSocketConnection, SOUP_WEBSOCKET_DATA_TEXT, bytes.get());
#else
    soup_websocket_connection_send_text(webSocketConnection, CString(utf8).data());
#endif
}

void RemoteInspectorHTTPServer::targetDidClose(uint64_t connectionID, uint64_t targetID)
{
    auto* webSocketConnection = m_webSocketConnectionMap.get(std::make_pair(connectionID, targetID));
    if (!webSocketConnection)
        return;

    soup_websocket_connection_close(webSocketConnection, SOUP_WEBSOCKET_CLOSE_GOING_AWAY, "Target closed");
}

void RemoteInspectorHTTPServer::sendMessageToBackend(SoupWebsocketConnection* connection, const String& message) const
{
    auto [connectionID, targetID] = m_webSocketConnectionToTargetMap.get(connection);
    if (connectionID && targetID)
        m_client->sendMessageToBackend(connectionID, targetID, message);
}

void RemoteInspectorHTTPServer::didCloseWebSocket(SoupWebsocketConnection* connection)
{
    auto connectionAndTarget = m_webSocketConnectionToTargetMap.take(connection);
    if (!connectionAndTarget.first || !connectionAndTarget.second)
        return;

    g_signal_handlers_disconnect_by_data(connection, this);
    m_webSocketConnectionMap.remove(connectionAndTarget);
    m_client->closeFromFrontend(connectionAndTarget.first, connectionAndTarget.second);
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
