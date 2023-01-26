

/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebViewTest.h"
#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

static WebKitTestServer* kServer;

static void testNetworkSessionDefault(Test* test, gconstpointer)
{
    // Check there's a single instance of the default network session.
    g_assert_true(webkit_network_session_get_default() == webkit_network_session_get_default());
    g_assert_true(webkit_network_session_get_default() != test->m_networkSession.get());
}

static void testNetworkSessionEphemeral(Test* test, gconstpointer)
{
    // By default network sessions are not ephemeral.
    g_assert_false(webkit_network_session_is_ephemeral(webkit_network_session_get_default()));
    g_assert_false(webkit_network_session_is_ephemeral(test->m_networkSession.get()));

    WebKitWebsiteDataManager* manager = webkit_network_session_get_website_data_manager(webkit_network_session_get_default());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));
    manager = webkit_network_session_get_website_data_manager(test->m_networkSession.get());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));

    auto webView = Test::adoptView(Test::createWebView());
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == webkit_network_session_get_default());

    webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "network-session", test->m_networkSession.get(),
        nullptr));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == test->m_networkSession.get());

    GRefPtr<WebKitNetworkSession> session = adoptGRef(webkit_network_session_new_ephemeral());
    g_assert_true(webkit_network_session_is_ephemeral(session.get()));
    manager = webkit_network_session_get_website_data_manager(session.get());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_true(webkit_website_data_manager_is_ephemeral(manager));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) != session.get());

    webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "network-session", session.get(),
        nullptr));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == session.get());
}

static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/echoPort")) {
        char* port = g_strdup_printf("%u", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(soup_server_message_get_local_address(message))));
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, port, strlen(port));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

class ProxyTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ProxyTest);

    enum class WebSocketServerType {
        Unknown,
        NoProxy,
        Proxy
    };

    static void webSocketProxyServerCallback(SoupServer*, SoupServerMessage*, const char* path, SoupWebsocketConnection*, gpointer userData)
    {
        static_cast<ProxyTest*>(userData)->webSocketConnected(ProxyTest::WebSocketServerType::Proxy);
    }

    ProxyTest()
    {
        // This "proxy server" is actually just a different instance of the main
        // test server (kServer), listening on a different port. Requests
        // will not actually be proxied to kServer because proxyServer is not
        // actually a proxy server. We're testing whether the proxy settings
        // work, not whether we can write a soup proxy server.
        m_proxyServer.run(serverCallback);
        g_assert_false(m_proxyServer.baseURL().isNull());

        m_proxyServer.addWebSocketHandler(webSocketProxyServerCallback, this);
        g_assert_false(m_proxyServer.baseWebSocketURL().isNull());
    }

    CString loadURIAndGetMainResourceData(const char* uri)
    {
        loadURI(uri);
        waitUntilLoadFinished();
        size_t dataSize = 0;
        const char* data = mainResourceData(dataSize);
        return CString(data, dataSize);
    }

    GUniquePtr<char> proxyServerPortAsString()
    {
        GUniquePtr<char> port(g_strdup_printf("%u", m_proxyServer.port()));
        return port;
    }

    void webSocketConnected(WebSocketServerType serverType)
    {
        m_webSocketRequestReceived = serverType;
        quitMainLoop();
    }

    WebSocketServerType createWebSocketAndWaitUntilConnected()
    {
        m_webSocketRequestReceived = WebSocketServerType::Unknown;
        GUniquePtr<char> createWebSocket(g_strdup_printf("var ws = new WebSocket('%s');", kServer->getWebSocketURIForPath("/foo").data()));
        runJavaScriptAndWaitUntilFinished(createWebSocket.get(), nullptr);
        if (m_webSocketRequestReceived == WebSocketServerType::Unknown)
            g_main_loop_run(m_mainLoop);
        return m_webSocketRequestReceived;
    }

    WebKitTestServer m_proxyServer;
    WebSocketServerType m_webSocketRequestReceived { WebSocketServerType::Unknown };
};

static void webSocketServerCallback(SoupServer*, SoupServerMessage*, const char*, SoupWebsocketConnection*, gpointer userData)
{
    static_cast<ProxyTest*>(userData)->webSocketConnected(ProxyTest::WebSocketServerType::NoProxy);
}

static void ephemeralViewloadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, ProxyTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewloadChanged), test);
    test->quitMainLoop();
}

static void testNetworkSessionProxySettings(ProxyTest* test, gconstpointer)
{
    // Proxy URI is unset by default. Requests to kServer should be received by kServer.
    GUniquePtr<char> serverPortAsString(g_strdup_printf("%u", kServer->port()));
    auto mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    // WebSocket requests should also be received by kServer.
    kServer->addWebSocketHandler(webSocketServerCallback, test);
    auto serverType = test->createWebSocketAndWaitUntilConnected();
    g_assert_true(serverType == ProxyTest::WebSocketServerType::NoProxy);

    // Set default proxy URI to point to proxyServer. Requests to kServer should be received by proxyServer instead.
    WebKitNetworkProxySettings* settings = webkit_network_proxy_settings_new(test->m_proxyServer.baseURL().string().utf8().data(), nullptr);
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    GUniquePtr<char> proxyServerPortAsString = test->proxyServerPortAsString();
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());

    // WebSocket requests should also be received by proxyServer.
    serverType = test->createWebSocketAndWaitUntilConnected();
    g_assert_true(serverType == ProxyTest::WebSocketServerType::Proxy);

    // Proxy settings also affect ephemeral web views.
    GRefPtr<WebKitNetworkSession> ephemeralSession = adoptGRef(webkit_network_session_new_ephemeral());
    webkit_network_session_set_proxy_settings(ephemeralSession.get(), WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    webkit_network_proxy_settings_free(settings);
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "network-session", ephemeralSession.get(),
        nullptr));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == ephemeralSession.get());

    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(ephemeralViewloadChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/echoPort").data());
    g_main_loop_run(test->m_mainLoop);
    WebKitWebResource* resource = webkit_web_view_get_main_resource(webView.get());
    g_assert_true(WEBKIT_IS_WEB_RESOURCE(resource));
    webkit_web_resource_get_data(resource, nullptr, [](GObject* object, GAsyncResult* result, gpointer userData) {
        size_t dataSize;
        GUniquePtr<char> data(reinterpret_cast<char*>(webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(object), result, &dataSize, nullptr)));
        g_assert_nonnull(data);
        auto* test = static_cast<ProxyTest*>(userData);
        GUniquePtr<char> proxyServerPortAsString = test->proxyServerPortAsString();
        ASSERT_CMP_CSTRING(CString(data.get(), dataSize), ==, proxyServerPortAsString.get());
        test->quitMainLoop();
        }, test);
    g_main_loop_run(test->m_mainLoop);

    // Remove the proxy. Requests to kServer should be received by kServer again.
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_NO_PROXY, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    // Use a default proxy uri, but ignoring requests to localhost.
    static const char* ignoreHosts[] = { "localhost", nullptr };
    settings = webkit_network_proxy_settings_new(test->m_proxyServer.baseURL().string().utf8().data(), ignoreHosts);
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());
    GUniquePtr<char> localhostEchoPortURI(g_strdup_printf("http://localhost:%s/echoPort", serverPortAsString.get()));
    mainResourceData = test->loadURIAndGetMainResourceData(localhostEchoPortURI.get());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());
    webkit_network_proxy_settings_free(settings);

    // Remove the proxy again to ensure next test is not using any previous values.
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_NO_PROXY, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    // Use scheme specific proxy instead of the default.
    settings = webkit_network_proxy_settings_new(nullptr, nullptr);
    webkit_network_proxy_settings_add_proxy_for_scheme(settings, "http", test->m_proxyServer.baseURL().string().utf8().data());
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());
    webkit_network_proxy_settings_free(settings);

    // Reset to use the default resolver.
    webkit_network_session_set_proxy_settings(test->m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_DEFAULT, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    kServer->removeWebSocketHandler();
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    Test::add("WebKitNetworkSession", "default-session", testNetworkSessionDefault);
    Test::add("WebKitNetworkSession", "ephemeral", testNetworkSessionEphemeral);
    ProxyTest::add("WebKitNetworkSession", "proxy", testNetworkSessionProxySettings);
}

void afterAll()
{
    delete kServer;
}
