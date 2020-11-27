/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitTestServer.h"

#include "TestMain.h"
#include <wtf/Threading.h>
#include <wtf/glib/GUniquePtr.h>

WebKitTestServer::WebKitTestServer(ServerOptionsBitSet options)
{
    if (options[ServerRunInThread]) {
        WTF::initialize();
        m_queue = WorkQueue::create("WebKitTestServer");
    }

    GUniquePtr<char> sslCertificateFile;
    GUniquePtr<char> sslKeyFile;
    if (options[ServerHTTPS]) {
        CString resourcesDir = Test::getResourcesDir();
        sslCertificateFile.reset(g_build_filename(resourcesDir.data(), "test-cert.pem", NULL));
        sslKeyFile.reset(g_build_filename(resourcesDir.data(), "test-key.pem", NULL));
    }

    m_soupServer = adoptGRef(soup_server_new(
        SOUP_SERVER_ASYNC_CONTEXT, m_queue ? m_queue->runLoop().mainContext() : nullptr,
        SOUP_SERVER_SSL_CERT_FILE, sslCertificateFile.get(),
        SOUP_SERVER_SSL_KEY_FILE, sslKeyFile.get(), nullptr));

    GUniqueOutPtr<GError> error;
    SoupServerListenOptions serverOptions = static_cast<SoupServerListenOptions>(options[ServerHTTPS] ? SOUP_SERVER_LISTEN_IPV4_ONLY : SOUP_SERVER_LISTEN_IPV4_ONLY | SOUP_SERVER_LISTEN_HTTPS);
    bool serverStarted = false;
    if (options[ServerNonLoopback]) {
        GRefPtr<SoupAddress> address = adoptGRef(soup_address_new("localhost", SOUP_ADDRESS_ANY_PORT));
        soup_address_resolve_sync(address.get(), nullptr);
        serverStarted = soup_server_listen(m_soupServer.get(), soup_address_get_gsockaddr(address.get()), serverOptions, &error.outPtr());
    } else
        serverStarted = soup_server_listen_local(m_soupServer.get(), SOUP_ADDRESS_ANY_PORT, serverOptions, &error.outPtr());
    if (!serverStarted) {
        WTFLogAlways("Failed to start HTTP server: %s", error->message);
        CRASH();
    }

    GSList* uris = soup_server_get_uris(m_soupServer.get());
    ASSERT(uris);
    m_baseURI = static_cast<SoupURI*>(g_object_ref(uris->data));
    g_slist_free_full(uris, g_object_unref);
}

WebKitTestServer::~WebKitTestServer()
{
    soup_uri_free(m_baseURI);
    if (m_baseWebSocketURI)
        soup_uri_free(m_baseWebSocketURI);
}

void WebKitTestServer::run(SoupServerCallback serverCallback)
{
    if (m_queue) {
        m_queue->dispatch([this, serverCallback] {
            soup_server_run_async(m_soupServer.get());
            soup_server_add_handler(m_soupServer.get(), nullptr, serverCallback, nullptr, nullptr);
        });
    } else {
        soup_server_run_async(m_soupServer.get());
        soup_server_add_handler(m_soupServer.get(), nullptr, serverCallback, nullptr, nullptr);
    }
}

void WebKitTestServer::addWebSocketHandler(SoupServerWebsocketCallback callback, gpointer userData)
{
    m_baseWebSocketURI = soup_uri_new_with_base(m_baseURI, "/websocket/");
    m_baseWebSocketURI->scheme = m_baseWebSocketURI->scheme == SOUP_URI_SCHEME_HTTP ? SOUP_URI_SCHEME_WS : SOUP_URI_SCHEME_WSS;

    if (m_queue) {
        m_queue->dispatch([this, callback, userData] {
            soup_server_add_websocket_handler(m_soupServer.get(), "/websocket", nullptr, nullptr, callback, userData, nullptr);
        });
    } else
        soup_server_add_websocket_handler(m_soupServer.get(), "/websocket", nullptr, nullptr, callback, userData, nullptr);
}

void WebKitTestServer::removeWebSocketHandler()
{
    soup_uri_free(m_baseWebSocketURI);
    m_baseWebSocketURI = nullptr;

    if (m_queue) {
        m_queue->dispatch([this] {
            soup_server_remove_handler(m_soupServer.get(), "/websocket");
        });
    } else
        soup_server_remove_handler(m_soupServer.get(), "/websocket");
}

CString WebKitTestServer::getWebSocketURIForPath(const char* path) const
{
    g_assert_nonnull(m_baseWebSocketURI);
    g_assert_true(path && *path == '/');
    SoupURI* uri = soup_uri_new_with_base(m_baseWebSocketURI, path + 1); // Ignore the leading slash.
    GUniquePtr<gchar> uriString(soup_uri_to_string(uri, FALSE));
    soup_uri_free(uri);
    return uriString.get();
}

CString WebKitTestServer::getURIForPath(const char* path) const
{
    SoupURI* uri = soup_uri_new_with_base(m_baseURI, path);
    GUniquePtr<gchar> uriString(soup_uri_to_string(uri, FALSE));
    soup_uri_free(uri);
    return uriString.get();
}
