/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "LoadTrackingTest.h"
#include "WebKitTestServer.h"
#include <WebCore/SoupVersioning.h>
#include <wtf/glib/GRefPtr.h>

static WebKitTestServer* kServer;

static const char authTestUsername[] = "username";
static const char authTestPassword[] = "password";
static const char authExpectedSuccessTitle[] = "WebKit2Gtk+ Authentication test";
static const char authExpectedFailureTitle[] = "401 Authorization Required";
static const char authExpectedAuthorization[] = "Basic dXNlcm5hbWU6cGFzc3dvcmQ="; // Base64 encoding of "username:password".
static const char authSuccessHTMLString[] =
    "<html>"
    "<head><title>WebKit2Gtk+ Authentication test</title></head>"
    "<body></body></html>";
static const char authFailureHTMLString[] =
    "<html>"
    "<head><title>401 Authorization Required</title></head>"
    "<body></body></html>";

class AuthenticationTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(AuthenticationTest);

    AuthenticationTest()
    {
        g_signal_connect(m_webView, "authenticate", G_CALLBACK(runAuthenticationCallback), this);
    }

    ~AuthenticationTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    static int authenticationRetries;

    void loadURI(const char* uri)
    {
        // Reset the retry count of the fake server when a page is loaded.
        authenticationRetries = 0;
        m_authenticationCancelledReceived = false;
        LoadTrackingTest::loadURI(uri);
    }

    static gboolean runAuthenticationCallback(WebKitWebView*, WebKitAuthenticationRequest* request, AuthenticationTest* test)
    {
        g_signal_connect(request, "authenticated", G_CALLBACK(authenticationSucceededCallback), test);
        g_signal_connect(request, "cancelled", G_CALLBACK(authenticationCancelledCallback), test);
        test->runAuthentication(request);
        return TRUE;
    }

    static void authenticationSucceededCallback(WebKitAuthenticationRequest*, WebKitCredential* credential, AuthenticationTest* test)
    {
        g_assert_nonnull(credential);
        g_assert_cmpstr(webkit_credential_get_username(credential), ==, authTestUsername);
        g_assert_cmpstr(webkit_credential_get_password(credential), ==, authTestPassword);
        g_assert_cmpuint(webkit_credential_get_persistence(credential), ==, WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
        g_assert_false(test->m_authenticationCancelledReceived);
        g_assert_false(test->m_authenticationSucceededReceived);
        test->m_authenticationSucceededReceived = true;
    }

    static void authenticationCancelledCallback(WebKitAuthenticationRequest*, AuthenticationTest* test)
    {
        g_assert_false(test->m_authenticationSucceededReceived);
        g_assert_false(test->m_authenticationCancelledReceived);
        test->m_authenticationCancelledReceived = true;
    }

    void runAuthentication(WebKitAuthenticationRequest* request)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));
        m_authenticationRequest = request;
        g_main_loop_quit(m_mainLoop);
    }

    WebKitAuthenticationRequest* waitForAuthenticationRequest()
    {
        g_main_loop_run(m_mainLoop);
        return m_authenticationRequest.get();
    }

    GRefPtr<WebKitAuthenticationRequest> m_authenticationRequest;
    bool m_authenticationSucceededReceived { false };
    bool m_authenticationCancelledReceived { false };
};

class EphemeralAuthenticationTest : public AuthenticationTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(EphemeralAuthenticationTest, setup, teardown);

    static void setup()
    {
        WebViewTest::shouldCreateEphemeralWebView = true;
    }

    static void teardown()
    {
        WebViewTest::shouldCreateEphemeralWebView = false;
    }
};

int AuthenticationTest::authenticationRetries = 0;

static void testWebViewAuthenticationRequest(AuthenticationTest* test, gconstpointer)
{
    // Test authentication request getters match soup authentication header.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    ASSERT_CMP_CSTRING(webkit_authentication_request_get_host(request), ==, kServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_authentication_request_get_port(request), ==, kServer->port());
    g_assert_cmpstr(webkit_authentication_request_get_realm(request), ==, "my realm");
    g_assert_cmpint(webkit_authentication_request_get_scheme(request), ==, WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC);
    g_assert_false(webkit_authentication_request_is_for_proxy(request));
    g_assert_false(webkit_authentication_request_is_retry(request));
    auto* origin = webkit_authentication_request_get_security_origin(request);
    g_assert_nonnull(origin);
    ASSERT_CMP_CSTRING(webkit_security_origin_get_protocol(origin), ==, kServer->baseURL().protocol().toString().utf8());
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, kServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, kServer->port());
    webkit_security_origin_unref(origin);
}

static void testWebViewAuthenticationCancel(AuthenticationTest* test, gconstpointer)
{
    // Test cancel.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    webkit_authentication_request_cancel(request);
    // Server doesn't ask for new credentials.
    test->waitUntilLoadFinished();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);

    g_assert_error(test->m_error.get(), WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED);
    g_assert_true(test->m_authenticationCancelledReceived);
    g_assert_false(test->m_authenticationSucceededReceived);
}

static void testWebViewAuthenticationLoadCancelled(AuthenticationTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    test->waitForAuthenticationRequest();
    webkit_web_view_stop_loading(test->m_webView);
    // Expect empty page.
    test->waitUntilLoadFinished();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);

    g_assert_error(test->m_error.get(), WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED);
    g_assert_true(test->m_authenticationCancelledReceived);
    g_assert_false(test->m_authenticationSucceededReceived);
}

static void testWebViewAuthenticationFailure(AuthenticationTest* test, gconstpointer)
{
    // Test authentication failures.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    g_assert_false(webkit_authentication_request_is_retry(request));
    WebKitCredential* credential = webkit_credential_new(authTestUsername, "wrongpassword", WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    // Expect a second authentication request.
    request = test->waitForAuthenticationRequest();
    g_assert_true(webkit_authentication_request_is_retry(request));
    // Test second failure.
    credential = webkit_credential_new(authTestUsername, "wrongpassword2", WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    // Expect authentication failed page.
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, authExpectedFailureTitle);
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_false(test->m_authenticationSucceededReceived);
}

static void testWebViewAuthenticationNoCredential(AuthenticationTest* test, gconstpointer)
{
    // Test continue without credentials.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    webkit_authentication_request_authenticate(request, 0);
    // Server doesn't ask for new credentials.
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, authExpectedFailureTitle);
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_false(test->m_authenticationSucceededReceived);
}

static void testWebViewAuthenticationEphemeral(EphemeralAuthenticationTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    auto* request = test->waitForAuthenticationRequest();
    g_assert_null(webkit_authentication_request_get_proposed_credential(request));
    g_assert_false(webkit_authentication_request_can_save_credentials(request));
    webkit_authentication_request_set_can_save_credentials(request, TRUE);
    g_assert_false(webkit_authentication_request_can_save_credentials(request));
}

static void testWebViewAuthenticationStorage(AuthenticationTest* test, gconstpointer)
{
    WebKitAuthenticationRequest* request = nullptr;
#if USE(LIBSECRET)
    // If WebKit has been compiled with libsecret, and private browsing is disabled
    // then check that credentials can be saved.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    request = test->waitForAuthenticationRequest();
    g_assert_null(webkit_authentication_request_get_proposed_credential(request));
    g_assert_true(webkit_authentication_request_can_save_credentials(request));
    webkit_authentication_request_set_can_save_credentials(request, FALSE);
    g_assert_false(webkit_authentication_request_can_save_credentials(request));
    webkit_authentication_request_cancel(request);
    test->waitUntilLoadFinished();
    g_assert_true(test->m_authenticationCancelledReceived);
    g_assert_false(test->m_authenticationSucceededReceived);
#endif

#if ENABLE(2022_GLIB_API)
    auto* networkSession = webkit_web_view_get_network_session(test->m_webView);
    g_assert_true(webkit_network_session_get_persistent_credential_storage_enabled(networkSession));
    webkit_network_session_set_persistent_credential_storage_enabled(networkSession, FALSE);
    g_assert_false(webkit_network_session_get_persistent_credential_storage_enabled(networkSession));
#else
    auto* websiteDataManager = webkit_web_view_get_website_data_manager(test->m_webView);
    g_assert_true(webkit_website_data_manager_get_persistent_credential_storage_enabled(websiteDataManager));
    webkit_website_data_manager_set_persistent_credential_storage_enabled(websiteDataManager, FALSE);
    g_assert_false(webkit_website_data_manager_get_persistent_credential_storage_enabled(websiteDataManager));
#endif

    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    request = test->waitForAuthenticationRequest();
    g_assert_null(webkit_authentication_request_get_proposed_credential(request));
    webkit_authentication_request_set_proposed_credential(request, nullptr);
    g_assert_null(webkit_authentication_request_get_proposed_credential(request));
    auto* credential = webkit_credential_new(authTestUsername, authTestPassword, WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
    webkit_authentication_request_set_proposed_credential(request, credential);
    auto* proposedCredential = webkit_authentication_request_get_proposed_credential(request);
    g_assert_nonnull(proposedCredential);
    g_assert_cmpstr(webkit_credential_get_username(credential), ==, webkit_credential_get_username(proposedCredential));
    g_assert_cmpstr(webkit_credential_get_password(credential), ==, webkit_credential_get_password(proposedCredential));
    g_assert_cmpuint(webkit_credential_get_persistence(credential), ==, webkit_credential_get_persistence(proposedCredential));
    webkit_credential_free(proposedCredential);
    g_assert_false(webkit_authentication_request_can_save_credentials(request));
    webkit_authentication_request_set_can_save_credentials(request, TRUE);
    g_assert_true(webkit_authentication_request_can_save_credentials(request));
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilLoadFinished();
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_true(test->m_authenticationSucceededReceived);
#if ENABLE(2022_GLIB_API)
    webkit_network_session_set_persistent_credential_storage_enabled(networkSession, TRUE);
#else
    webkit_website_data_manager_set_persistent_credential_storage_enabled(websiteDataManager, TRUE);
#endif
}

static void testWebViewAuthenticationSuccess(AuthenticationTest* test, gconstpointer)
{
    // Test correct authentication.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    WebKitCredential* credential = webkit_credential_new(authTestUsername, authTestPassword, WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, authExpectedSuccessTitle);
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_true(test->m_authenticationSucceededReceived);

    // Test loading the same (authorized) page again.
    test->loadURI(kServer->getURIForPath("/auth-test.html").data());
    // There is no authentication challenge.
    test->waitUntilLoadFinished();
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, authExpectedSuccessTitle);
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_true(test->m_authenticationSucceededReceived);
}

static void testWebViewAuthenticationEmptyRealm(AuthenticationTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/empty-realm.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
    WebKitCredential* credential = webkit_credential_new(authTestUsername, authTestPassword, WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, authExpectedSuccessTitle);
    g_assert_false(test->m_authenticationCancelledReceived);
    g_assert_true(test->m_authenticationSucceededReceived);
}

class Tunnel {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(SOUP2)
    Tunnel(SoupServer* server, SoupMessage* message)
#else
    Tunnel(SoupServer* server, SoupServerMessage* message)
#endif
        : m_server(server)
        , m_message(message)
    {
        soup_server_pause_message(m_server.get(), m_message.get());
    }

    ~Tunnel()
    {
        soup_server_unpause_message(m_server.get(), m_message.get());
    }

    void connect(Function<void (const char*)>&& completionHandler)
    {
        m_completionHandler = WTFMove(completionHandler);
        GRefPtr<GSocketClient> client = adoptGRef(g_socket_client_new());
        auto* uri = soup_server_message_get_uri(m_message.get());
#if USE(SOUP2)
        const char* host = uri->host;
        int port = uri->port;
#else
        const char* host = g_uri_get_host(uri);
        int port = std::max<int>(g_uri_get_port(uri), 0);
#endif
        g_socket_client_connect_to_host_async(client.get(), host, port, nullptr, [](GObject* source, GAsyncResult* result, gpointer userData) {
            auto* tunnel = static_cast<Tunnel*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source), result, &error.outPtr()));
            tunnel->connected(!connection ? error->message : nullptr);
        }, this);
    }

    void connected(const char* errorMessage)
    {
        auto completionHandler = std::exchange(m_completionHandler, nullptr);
        completionHandler(errorMessage);
    }

    GRefPtr<SoupServer> m_server;
#if USE(SOUP2)
    GRefPtr<SoupMessage> m_message;
#else
    GRefPtr<SoupServerMessage> m_message;
#endif
    Function<void (const char*)> m_completionHandler;
};

unsigned gProxyServerPort;

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext* context, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
#if USE(SOUP2)
    unsigned port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(soup_client_context_get_local_address(context)));
#else
    unsigned port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(soup_server_message_get_local_address(message)));
#endif

    if (soup_server_message_get_method(message) == SOUP_METHOD_CONNECT) {
        g_assert_cmpuint(port, ==, gProxyServerPort);
        auto tunnel = makeUnique<Tunnel>(server, message);
        auto* tunnelPtr = tunnel.get();
        tunnelPtr->connect([tunnel = WTFMove(tunnel)](const char* errorMessage) {
            if (errorMessage) {
                soup_server_message_set_status(tunnel->m_message.get(), SOUP_STATUS_BAD_GATEWAY, nullptr);
                soup_server_message_set_response(tunnel->m_message.get(), "text/plain", SOUP_MEMORY_COPY, errorMessage, strlen(errorMessage));
            } else {
                soup_message_headers_append(soup_server_message_get_response_headers(tunnel->m_message.get()), "Proxy-Authenticate", "Basic realm=\"Proxy realm\"");
                soup_server_message_set_status(tunnel->m_message.get(), SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED, nullptr);
            }
        });
        return;
    }

    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_has_suffix(path, "/auth-test.html") || g_str_has_suffix(path, "/empty-realm.html")) {
        bool isProxy = g_str_has_prefix(path, "/proxy");
        if (isProxy)
            g_assert_cmpuint(port, ==, gProxyServerPort);

        const char* authorization = soup_message_headers_get_one(soup_server_message_get_request_headers(message), "Authorization");
        // Require authentication.
        if (!g_strcmp0(authorization, authExpectedAuthorization)) {
            // Successful authentication.
            soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
            soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, authSuccessHTMLString, strlen(authSuccessHTMLString));
            AuthenticationTest::authenticationRetries = 0;
        } else if (++AuthenticationTest::authenticationRetries < 3) {
            // No or invalid authorization header provided by the client, request authentication twice then fail.
            soup_server_message_set_status(message, isProxy ? SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED : SOUP_STATUS_UNAUTHORIZED, nullptr);
            if (!strcmp(path, "/empty-realm.html"))
                soup_message_headers_append(soup_server_message_get_response_headers(message), "WWW-Authenticate", "Basic");
            else
                soup_message_headers_append(soup_server_message_get_response_headers(message), isProxy ? "Proxy-Authenticate" : "WWW-Authenticate", isProxy ? "Basic realm=\"Proxy realm\"" : "Basic realm=\"my realm\"");
            // Include a failure message in case the user attempts to proceed without authentication.
            soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, authFailureHTMLString, strlen(authFailureHTMLString));
        } else {
            // Authentication not successful, display a "401 Authorization Required" page.
            soup_server_message_set_status(message, isProxy ? SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED : SOUP_STATUS_UNAUTHORIZED, nullptr);
            soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, authFailureHTMLString, strlen(authFailureHTMLString));
        }
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);

    soup_message_body_complete(responseBody);
}

class ProxyAuthenticationTest : public AuthenticationTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ProxyAuthenticationTest);

    ProxyAuthenticationTest()
    {
        m_proxyServer.run(serverCallback);
        g_assert_false(m_proxyServer.baseURL().isNull());
        gProxyServerPort = m_proxyServer.port();
        WebKitNetworkProxySettings* settings = webkit_network_proxy_settings_new(m_proxyServer.baseURL().string().utf8().data(), nullptr);
#if ENABLE(2022_GLIB_API)
        webkit_network_session_set_proxy_settings(m_networkSession.get(), WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
#else
        auto* websiteDataManager = webkit_web_context_get_website_data_manager(m_webContext.get());
        webkit_website_data_manager_set_network_proxy_settings(websiteDataManager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
#endif
        webkit_network_proxy_settings_free(settings);
    }

    ~ProxyAuthenticationTest()
    {
        gProxyServerPort = 0;
    }

    GUniquePtr<char> proxyServerPortAsString()
    {
        GUniquePtr<char> port(g_strdup_printf("%u", m_proxyServer.port()));
        return port;
    }

    WebKitTestServer m_proxyServer;
};

static void testWebViewAuthenticationProxy(ProxyAuthenticationTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/proxy/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
#if USE(SOUP2)
    // FIXME: the uri and host should the proxy ones, not the requested ones.
    ASSERT_CMP_CSTRING(webkit_authentication_request_get_host(request), ==, kServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_authentication_request_get_port(request), ==, kServer->port());
#else
    ASSERT_CMP_CSTRING(webkit_authentication_request_get_host(request), ==, test->m_proxyServer.baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_authentication_request_get_port(request), ==, test->m_proxyServer.port());
#endif
    g_assert_cmpstr(webkit_authentication_request_get_realm(request), ==, "Proxy realm");
    g_assert_cmpint(webkit_authentication_request_get_scheme(request), ==, WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC);
    g_assert_true(webkit_authentication_request_is_for_proxy(request));
    g_assert_false(webkit_authentication_request_is_retry(request));
    auto* origin = webkit_authentication_request_get_security_origin(request);
    g_assert_nonnull(origin);
    ASSERT_CMP_CSTRING(webkit_security_origin_get_protocol(origin), ==, kServer->baseURL().protocol().toString().utf8());
#if USE(SOUP2)
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, kServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, kServer->port());
#else
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, test->m_proxyServer.baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, test->m_proxyServer.port());
#endif
    webkit_security_origin_unref(origin);
}

static void testWebViewAuthenticationProxyHTTPS(ProxyAuthenticationTest* test, gconstpointer)
{
    auto httpsServer = makeUnique<WebKitTestServer>(WebKitTestServer::ServerHTTPS);
    httpsServer->run(serverCallback);

    test->loadURI(httpsServer->getURIForPath("/proxy/auth-test.html").data());
    WebKitAuthenticationRequest* request = test->waitForAuthenticationRequest();
#if USE(SOUP2)
    // FIXME: the uri and host should the proxy ones, not the requested ones.
    ASSERT_CMP_CSTRING(webkit_authentication_request_get_host(request), ==, httpsServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_authentication_request_get_port(request), ==, httpsServer->port());
#else
    ASSERT_CMP_CSTRING(webkit_authentication_request_get_host(request), ==, test->m_proxyServer.baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_authentication_request_get_port(request), ==, test->m_proxyServer.port());
#endif
    g_assert_cmpstr(webkit_authentication_request_get_realm(request), ==, "Proxy realm");
    g_assert_cmpint(webkit_authentication_request_get_scheme(request), ==, WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC);
    g_assert_true(webkit_authentication_request_is_for_proxy(request));
    g_assert_false(webkit_authentication_request_is_retry(request));
    auto* origin = webkit_authentication_request_get_security_origin(request);
    g_assert_nonnull(origin);
    ASSERT_CMP_CSTRING(webkit_security_origin_get_protocol(origin), ==, httpsServer->baseURL().protocol().toString().utf8());
#if USE(SOUP2)
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, httpsServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, httpsServer->port());
#else
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, test->m_proxyServer.baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, test->m_proxyServer.port());
#endif
    webkit_security_origin_unref(origin);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    AuthenticationTest::add("Authentication", "authentication-request", testWebViewAuthenticationRequest);
    AuthenticationTest::add("Authentication", "authentication-cancel", testWebViewAuthenticationCancel);
    AuthenticationTest::add("Authentication", "authentication-load-cancelled", testWebViewAuthenticationLoadCancelled);
    AuthenticationTest::add("Authentication", "authentication-success", testWebViewAuthenticationSuccess);
    AuthenticationTest::add("Authentication", "authentication-failure", testWebViewAuthenticationFailure);
    AuthenticationTest::add("Authentication", "authentication-no-credential", testWebViewAuthenticationNoCredential);
    EphemeralAuthenticationTest::add("Authentication", "authentication-ephemeral", testWebViewAuthenticationEphemeral);
    AuthenticationTest::add("Authentication", "authentication-storage", testWebViewAuthenticationStorage);
    AuthenticationTest::add("Authentication", "authentication-empty-realm", testWebViewAuthenticationEmptyRealm);
    ProxyAuthenticationTest::add("Authentication", "authentication-proxy", testWebViewAuthenticationProxy);
    ProxyAuthenticationTest::add("Authentication", "authentication-proxy-https", testWebViewAuthenticationProxyHTTPS);
}

void afterAll()
{
    delete kServer;
}
