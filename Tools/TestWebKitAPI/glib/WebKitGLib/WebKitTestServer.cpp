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
#include <wtf/threads/BinarySemaphore.h>

WebKitTestServer::WebKitTestServer(ServerOptions options)
{
    if (options & ServerRunInThread) {
        WTF::initialize();
        m_queue = WorkQueue::create("WebKitTestServer");
    }

    m_soupServer = adoptGRef(soup_server_new("server-header", "WebKitTestServer ", nullptr));

    if (options & ServerHTTPS) {
        CString resourcesDir = Test::getResourcesDir();
        GUniquePtr<char> sslCertificateFile(g_build_filename(resourcesDir.data(), "test-cert.pem", nullptr));
        GUniquePtr<char> sslKeyFile(g_build_filename(resourcesDir.data(), "test-key.pem", nullptr));
        g_assert_true(soup_server_set_ssl_cert_file(m_soupServer.get(), sslCertificateFile.get(), sslKeyFile.get(), nullptr));
    }
}

WebKitTestServer::~WebKitTestServer()
{
    soup_uri_free(m_baseURI);
    if (m_baseWebSocketURI)
        soup_uri_free(m_baseWebSocketURI);
}

void WebKitTestServer::run(SoupServerCallback serverCallback)
{
    soup_server_add_handler(m_soupServer.get(), nullptr, serverCallback, nullptr, nullptr);

    unsigned options = SOUP_SERVER_LISTEN_IPV4_ONLY;
    if (soup_server_is_https(m_soupServer.get()))
        options |= SOUP_SERVER_LISTEN_HTTPS;

    if (m_queue) {
        BinarySemaphore semaphore;
        m_queue->dispatch([&] {
            g_assert_true(soup_server_listen_local(m_soupServer.get(), SOUP_ADDRESS_ANY_PORT, static_cast<SoupServerListenOptions>(options), nullptr));
            semaphore.signal();
        });
        semaphore.wait();
    } else
        g_assert_true(soup_server_listen_local(m_soupServer.get(), SOUP_ADDRESS_ANY_PORT, static_cast<SoupServerListenOptions>(options), nullptr));

    GSList* uris = soup_server_get_uris(m_soupServer.get());
    g_assert_nonnull(uris);
    m_baseURI = soup_uri_copy(static_cast<SoupURI*>(uris->data));
    g_slist_free_full(uris, reinterpret_cast<GDestroyNotify>(soup_uri_free));
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

unsigned WebKitTestServer::port() const
{
    return soup_uri_get_port(m_baseURI);
}
