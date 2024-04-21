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

WebKitTestServer::WebKitTestServer(ServerOptionsBitSet options)
{
    if (options[ServerRunInThread]) {
        WTF::initialize();
        m_queue = WorkQueue::create("WebKitTestServer"_s);
    }

    GRefPtr<GTlsCertificate> certificate;
    if (options[ServerHTTPS]) {
        GUniqueOutPtr<GError> error;
        CString resourcesDir = Test::getResourcesDir();
        GUniquePtr<char> sslCertificateFile(g_build_filename(resourcesDir.data(), "test-cert.pem", nullptr));
        GUniquePtr<char> sslKeyFile(g_build_filename(resourcesDir.data(), "test-key.pem", nullptr));
        certificate = adoptGRef(g_tls_certificate_new_from_files(sslCertificateFile.get(), sslKeyFile.get(), &error.outPtr()));
        g_assert_no_error(error.get());
    }

    m_soupServer = adoptGRef(soup_server_new("server-header", "WebKitTestServer ", "tls-certificate", certificate.get(), nullptr));
}

void WebKitTestServer::run(SoupServerCallback serverCallback)
{
    soup_server_add_handler(m_soupServer.get(), nullptr, serverCallback, nullptr, nullptr);

    unsigned options = SOUP_SERVER_LISTEN_IPV4_ONLY;
    if (soup_server_is_https(m_soupServer.get()))
        options |= SOUP_SERVER_LISTEN_HTTPS;

    if (m_queue) {
        m_queue->dispatchSync([&] {
            g_assert_true(soup_server_listen_local(m_soupServer.get(), 0, static_cast<SoupServerListenOptions>(options), nullptr));
        });
    } else
        g_assert_true(soup_server_listen_local(m_soupServer.get(), 0, static_cast<SoupServerListenOptions>(options), nullptr));

    GSList* uris = soup_server_get_uris(m_soupServer.get());
    g_assert_nonnull(uris);
#if USE(SOUP2)
    GUniquePtr<gchar> urlString(soup_uri_to_string(static_cast<SoupURI*>(uris->data), FALSE));
    m_baseURL = URL({ }, String::fromUTF8(urlString.get()));
    g_slist_free_full(uris, reinterpret_cast<GDestroyNotify>(soup_uri_free));
#else
    m_baseURL = static_cast<GUri*>(uris->data);
    g_slist_free_full(uris, reinterpret_cast<GDestroyNotify>(g_uri_unref));
#endif
}

void WebKitTestServer::addWebSocketHandler(SoupServerWebsocketCallback callback, gpointer userData)
{
    m_baseWebSocketURL = URL(m_baseURL, "/websocket/"_s);
    m_baseWebSocketURL.setProtocol(m_baseWebSocketURL.protocolIs("http"_s) ? "ws"_s : "wss"_s);

    if (m_queue) {
        m_queue->dispatch([this, callback, userData] {
            soup_server_add_websocket_handler(m_soupServer.get(), "/websocket", nullptr, nullptr, callback, userData, nullptr);
        });
    } else
        soup_server_add_websocket_handler(m_soupServer.get(), "/websocket", nullptr, nullptr, callback, userData, nullptr);
}

void WebKitTestServer::removeWebSocketHandler()
{
    m_baseWebSocketURL = { };

    if (m_queue) {
        m_queue->dispatch([this] {
            soup_server_remove_handler(m_soupServer.get(), "/websocket");
        });
    } else
        soup_server_remove_handler(m_soupServer.get(), "/websocket");
}

CString WebKitTestServer::getWebSocketURIForPath(const char* path) const
{
    g_assert_false(m_baseWebSocketURL.isNull());
    g_assert_true(path && *path == '/');
    return URL(m_baseWebSocketURL, String::fromLatin1(path + 1)).string().utf8(); // Ignore the leading slash.
}

CString WebKitTestServer::getURIForPath(const char* path) const
{
    return URL(m_baseURL, String::fromLatin1(path)).string().utf8();
}

unsigned WebKitTestServer::port() const
{
    return m_baseURL.port().value_or(0);
}
