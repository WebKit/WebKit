/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "LoadTrackingTest.h"
#include "WebKitTestServer.h"
#include <WebCore/SoupVersioning.h>

static WebKitTestServer* kHttpsServer;
static WebKitTestServer* kHttpServer;

static const char* indexHTML = "<html><body>Testing WebKit2GTK+ SSL</body></htmll>";
static const char* insecureContentHTML = "<html><script src=\"%s\"></script><body><p>Text + image <img src=\"%s\" align=\"right\"/></p></body></html>";
static const char TLSExpectedSuccessTitle[] = "WebKit2Gtk+ TLS permission test";
static const char TLSSuccessHTMLString[] = "<html><head><title>WebKit2Gtk+ TLS permission test</title></head><body></body></html>";

class SSLTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SSLTest);

    SSLTest()
        : m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
    }

    virtual void provisionalLoadFailed(const gchar* failingURI, GError* error)
    {
        g_assert_error(error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE);
        LoadTrackingTest::provisionalLoadFailed(failingURI, error);
    }

    virtual void loadCommitted()
    {
        GTlsCertificate* certificate = 0;
        webkit_web_view_get_tls_info(m_webView, &certificate, &m_tlsErrors);
        m_certificate = certificate;
        LoadTrackingTest::loadCommitted();
    }

    void waitUntilLoadFinished()
    {
        m_certificate = 0;
        m_tlsErrors = static_cast<GTlsCertificateFlags>(0);
        LoadTrackingTest::waitUntilLoadFinished();
    }

    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
};

static void testSSL(SSLTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    test->loadURI(kHttpsServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_nonnull(test->m_certificate);
    // Self-signed certificate has a nullptr issuer.
    g_assert_null(g_tls_certificate_get_issuer(test->m_certificate.get()));
    // We always expect errors because we are using a self-signed certificate,
    // but only G_TLS_CERTIFICATE_UNKNOWN_CA flags should be present.
    g_assert_cmpuint(test->m_tlsErrors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA);

    // Non HTTPS loads shouldn't have a certificate nor errors.
    test->loadHtml(indexHTML, 0);
    test->waitUntilLoadFinished();
    g_assert_null(test->m_certificate);
    g_assert_cmpuint(test->m_tlsErrors, ==, 0);

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

class InsecureContentTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(InsecureContentTest);

    InsecureContentTest()
        : m_insecureContentRun(false)
        , m_insecureContentDisplayed(false)
    {
        g_signal_connect(m_webView, "insecure-content-detected", G_CALLBACK(insecureContentDetectedCallback), this);
    }

    static void insecureContentDetectedCallback(WebKitWebView* webView, WebKitInsecureContentEvent event, InsecureContentTest* test)
    {
        g_assert_true(webView == test->m_webView);

        if (event == WEBKIT_INSECURE_CONTENT_RUN)
            test->m_insecureContentRun = true;

        if (event == WEBKIT_INSECURE_CONTENT_DISPLAYED)
            test->m_insecureContentDisplayed = true;
    }

    bool m_insecureContentRun;
    bool m_insecureContentDisplayed;
};

static void testInsecureContent(InsecureContentTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    test->loadURI(kHttpsServer->getURIForPath("/insecure-content/").data());
    test->waitUntilLoadFinished();

    g_assert_false(test->m_insecureContentRun);
    // Images are currently always displayed, even bypassing mixed content settings. Check
    // https://bugs.webkit.org/show_bug.cgi?id=142469
    g_assert_true(test->m_insecureContentDisplayed);

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

static bool assertIfSSLRequestProcessed = false;

static void testTLSErrorsPolicy(SSLTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    // TLS errors are treated as transport failures by default.
    g_assert_cmpint(webkit_website_data_manager_get_tls_errors_policy(websiteDataManager), ==, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    assertIfSSLRequestProcessed = true;
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadFailed);
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::LoadCommitted));
    assertIfSSLRequestProcessed = false;

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
    g_assert_cmpint(webkit_website_data_manager_get_tls_errors_policy(websiteDataManager), ==, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    test->m_loadFailed = false;
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_false(test->m_loadFailed);

    // An ephemeral web view should keep the same network settings by default.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "is-ephemeral", TRUE,
        nullptr));
    g_assert_true(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_false(webkit_web_context_is_ephemeral(test->m_webContext.get()));
    auto* webViewDataManager = webkit_web_view_get_website_data_manager(webView.get());
    g_assert_false(websiteDataManager == webViewDataManager);
    g_assert_cmpint(webkit_website_data_manager_get_tls_errors_policy(websiteDataManager), ==, webkit_website_data_manager_get_tls_errors_policy(webViewDataManager));

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);
    g_assert_cmpint(webkit_website_data_manager_get_tls_errors_policy(websiteDataManager), ==, WEBKIT_TLS_ERRORS_POLICY_FAIL);
}

static void testTLSErrorsRedirect(SSLTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    assertIfSSLRequestProcessed = true;
    test->loadURI(kHttpsServer->getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadFailed);
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::LoadCommitted));
    assertIfSSLRequestProcessed = false;

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

static gboolean webViewAuthenticationCallback(WebKitWebView*, WebKitAuthenticationRequest* request)
{
    g_assert_not_reached();
    return TRUE;
}


static void testTLSErrorsHTTPAuth(SSLTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    assertIfSSLRequestProcessed = true;
    g_signal_connect(test->m_webView, "authenticate", G_CALLBACK(webViewAuthenticationCallback), NULL);
    test->loadURI(kHttpsServer->getURIForPath("/auth").data());
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadFailed);
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::LoadCommitted));
    assertIfSSLRequestProcessed = false;

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

class TLSErrorsTest: public SSLTest {
public:
    MAKE_GLIB_TEST_FIXTURE(TLSErrorsTest);

    TLSErrorsTest()
        : m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
    }

    bool loadFailedWithTLSErrors(const char* failingURI, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors) override
    {
        LoadTrackingTest::loadFailedWithTLSErrors(failingURI, certificate, tlsErrors);

        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(certificate));
        m_certificate = certificate;
        m_tlsErrors = tlsErrors;
        m_failingURL = URL(String::fromLatin1(failingURI));
        return true;
    }

    GTlsCertificate* certificate() const { return m_certificate.get(); }
    GTlsCertificateFlags tlsErrors() const { return m_tlsErrors; }
    CString host() const { return m_failingURL.host().toString().utf8(); }

private:
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
    URL m_failingURL;
};

static void testLoadFailedWithTLSErrors(TLSErrorsTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    assertIfSSLRequestProcessed = true;
    // The load-failed-with-tls-errors signal should be emitted when there is a TLS failure.
    test->loadURI(kHttpsServer->getURIForPath("/test-tls/").data());
    test->waitUntilLoadFinished();
    g_assert_true(G_IS_TLS_CERTIFICATE(test->certificate()));
    g_assert_cmpuint(test->tlsErrors(), ==, G_TLS_CERTIFICATE_UNKNOWN_CA);
    ASSERT_CMP_CSTRING(test->host(), ==, kHttpsServer->baseURL().host().toString().utf8());
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadFailedWithTLSErrors);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    assertIfSSLRequestProcessed = false;

    // Test allowing an exception for this certificate on this host.
    webkit_web_context_allow_tls_certificate_for_host(test->m_webContext.get(), test->certificate(), test->host().data());
    // The page should now load without errors.
    test->loadURI(kHttpsServer->getURIForPath("/test-tls/").data());
    test->waitUntilTitleChanged();

    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, TLSExpectedSuccessTitle);

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

class TLSSubresourceTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(TLSSubresourceTest);

    static void resourceLoadStartedCallback(WebKitWebView* webView, WebKitWebResource* resource, WebKitURIRequest* request, TLSSubresourceTest* test)
    {
        if (webkit_web_view_get_main_resource(test->m_webView) == resource)
            return;

        // Ignore favicons.
        if (g_str_has_suffix(webkit_uri_request_get_uri(request), "favicon.ico"))
            return;

        test->subresourceLoadStarted(resource);
    }

    TLSSubresourceTest()
        : m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
        g_signal_connect(m_webView, "resource-load-started", G_CALLBACK(resourceLoadStartedCallback), this);
    }

    static void subresourceFailedCallback(WebKitWebResource*, GError*)
    {
        g_assert_not_reached();
    }

    static void subresourceFailedWithTLSErrorsCallback(WebKitWebResource* resource, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors, TLSSubresourceTest* test)
    {
        test->subresourceFailedWithTLSErrors(resource, certificate, tlsErrors);
    }

    void subresourceLoadStarted(WebKitWebResource* resource)
    {
        g_signal_connect(resource, "failed", G_CALLBACK(subresourceFailedCallback), nullptr);
        g_signal_connect(resource, "failed-with-tls-errors", G_CALLBACK(subresourceFailedWithTLSErrorsCallback), this);
    }

    void subresourceFailedWithTLSErrors(WebKitWebResource* resource, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
    {
        m_certificate = certificate;
        m_tlsErrors = tlsErrors;
        g_main_loop_quit(m_mainLoop);
    }

    void waitUntilSubresourceLoadFail()
    {
        g_main_loop_run(m_mainLoop);
    }

    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
};

static void testSubresourceLoadFailedWithTLSErrors(TLSSubresourceTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    assertIfSSLRequestProcessed = true;
    test->loadURI(kHttpServer->getURIForPath("/").data());
    test->waitUntilSubresourceLoadFail();
    g_assert_true(G_IS_TLS_CERTIFICATE(test->m_certificate.get()));
    g_assert_cmpuint(test->m_tlsErrors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA);
    assertIfSSLRequestProcessed = false;
}

class WebSocketTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebSocketTest);

    enum EventFlags {
        None = 0,
        DidServerCompleteHandshake = 1 << 0,
        DidOpen = 1 << 1,
        DidClose = 1 << 2
    };

    WebSocketTest()
    {
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "event");
        g_signal_connect(m_userContentManager.get(), "script-message-received::event", G_CALLBACK(webSocketTestResultCallback), this);
    }

    virtual ~WebSocketTest()
    {
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "event");
        g_signal_handlers_disconnect_by_data(m_userContentManager.get(), this);
    }

    static constexpr const char* webSocketTestJSFormat =
        "var socket = new WebSocket('%s');"
        "socket.addEventListener('open', onOpen);"
        "socket.addEventListener('close', onClose);"
        "function onOpen() {"
        "    window.webkit.messageHandlers.event.postMessage('open');"
        "    socket.removeEventListener('close', onClose);"
        "}"
        "function onClose() {"
        "    window.webkit.messageHandlers.event.postMessage('close');"
        "    socket.removeEventListener('open', onOpen);"
        "}";

#if USE(SOUP2)
    static void serverWebSocketCallback(SoupServer*, SoupWebsocketConnection*, const char*, SoupClientContext*, gpointer userData)
#else
    static void serverWebSocketCallback(SoupServer*, SoupServerMessage*, const char*, SoupWebsocketConnection*, gpointer userData)
#endif
    {
        static_cast<WebSocketTest*>(userData)->m_events |= WebSocketTest::EventFlags::DidServerCompleteHandshake;
    }

    static void webSocketTestResultCallback(WebKitUserContentManager*, WebKitJavascriptResult* javascriptResult, WebSocketTest* test)
    {
        GUniquePtr<char> event(WebViewTest::javascriptResultToCString(javascriptResult));
        if (!g_strcmp0(event.get(), "open"))
            test->m_events |= WebSocketTest::EventFlags::DidOpen;
        else if (!g_strcmp0(event.get(), "close"))
            test->m_events |= WebSocketTest::EventFlags::DidClose;
        else
            g_assert_not_reached();
        test->quitMainLoop();
    }

    unsigned connectToServerAndWaitForEvents(WebKitTestServer* server)
    {
        m_events = 0;

        server->addWebSocketHandler(serverWebSocketCallback, this);
        GUniquePtr<char> createWebSocketJS(g_strdup_printf(webSocketTestJSFormat, server->getWebSocketURIForPath("/foo").data()));
        webkit_web_view_run_javascript(m_webView, createWebSocketJS.get(), nullptr, nullptr, nullptr);
        g_main_loop_run(m_mainLoop);
        server->removeWebSocketHandler();

        return m_events;
    }

    unsigned m_events { 0 };
};

static void testWebSocketTLSErrors(WebSocketTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    // First, check that insecure ws:// web sockets work fine.
    unsigned events = test->connectToServerAndWaitForEvents(kHttpServer);
    g_assert_true(events);
    g_assert_true(events & WebSocketTest::EventFlags::DidServerCompleteHandshake);
    g_assert_true(events & WebSocketTest::EventFlags::DidOpen);
    g_assert_false(events & WebSocketTest::EventFlags::DidClose);

    // Try again using wss:// this time. It should be blocked because the
    // server certificate is self-signed.
    events = test->connectToServerAndWaitForEvents(kHttpsServer);
    g_assert_true(events);
    g_assert_false(events & WebSocketTest::EventFlags::DidServerCompleteHandshake);
    g_assert_false(events & WebSocketTest::EventFlags::DidOpen);
    g_assert_true(events & WebSocketTest::EventFlags::DidClose);

    // Now try wss:// again, this time ignoring TLS errors.
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
    events = test->connectToServerAndWaitForEvents(kHttpsServer);
    g_assert_true(events & WebSocketTest::EventFlags::DidServerCompleteHandshake);
    g_assert_true(events & WebSocketTest::EventFlags::DidOpen);
    g_assert_false(events & WebSocketTest::EventFlags::DidClose);

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

class EphemeralSSLTest : public SSLTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(EphemeralSSLTest, setup, teardown);

    static void setup()
    {
        WebViewTest::shouldCreateEphemeralWebView = true;
    }

    static void teardown()
    {
        WebViewTest::shouldCreateEphemeralWebView = false;
    }
};

static void testTLSErrorsEphemeral(EphemeralSSLTest* test, gconstpointer)
{
    auto* websiteDataManager = webkit_web_view_get_website_data_manager(test->m_webView);
    g_assert_true(webkit_website_data_manager_is_ephemeral(websiteDataManager));
    g_assert_cmpint(webkit_website_data_manager_get_tls_errors_policy(websiteDataManager), ==, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    test->loadURI(kHttpsServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadFailed);
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::LoadCommitted));
}

#if !USE(SOUP2)
class ClientSideCertificateTestBase {
public:
    static gboolean acceptCertificateCallback(SoupServerMessage* message, GTlsCertificate* certificate, GTlsCertificateFlags errors, ClientSideCertificateTestBase* test)
    {
        bool acceptedCertificate = test->acceptCertificate(errors);
        g_object_set_data_full(G_OBJECT(message), acceptedCertificate ? "accepted-certificate" : "rejected-certificate", g_object_ref(certificate), g_object_unref);
        return acceptedCertificate;
    }

    static void requestStartedCallback(SoupServer*, SoupServerMessage* message, ClientSideCertificateTestBase* test)
    {
        g_signal_connect(message, "accept-certificate", G_CALLBACK(acceptCertificateCallback), test);
    }

    static gboolean authenticateCallback(WebKitWebView*, WebKitAuthenticationRequest* request, ClientSideCertificateTestBase* test)
    {
        test->authenticate(request);
        return TRUE;
    }

    ClientSideCertificateTestBase(WebKitWebView* webView)
    {
        CString resourcesDir = Test::getResourcesDir();
        GUniquePtr<char> sslCertificateFile(g_build_filename(resourcesDir.data(), "test-cert.pem", nullptr));
        GUniquePtr<char> sslKeyFile(g_build_filename(resourcesDir.data(), "test-key.pem", nullptr));
        GUniqueOutPtr<GError> error;
        m_clientCertificate = adoptGRef(g_tls_certificate_new_from_files(sslCertificateFile.get(), sslKeyFile.get(), &error.outPtr()));
        g_assert_no_error(error.get());

        soup_server_set_tls_auth_mode(kHttpsServer->soupServer(), G_TLS_AUTHENTICATION_REQUIRED);
        g_signal_connect(kHttpsServer->soupServer(), "request-started", G_CALLBACK(requestStartedCallback), this);
        g_signal_connect(webView, "authenticate", G_CALLBACK(authenticateCallback), this);
    }

    virtual ~ClientSideCertificateTestBase()
    {
        soup_server_set_tls_auth_mode(kHttpsServer->soupServer(), G_TLS_AUTHENTICATION_NONE);
        g_signal_handlers_disconnect_by_data(kHttpsServer->soupServer(), this);
    }

    virtual void authenticate(WebKitAuthenticationRequest*) = 0;

    bool acceptCertificate(GTlsCertificateFlags errors)
    {
        if (m_rejectClientCertificates)
            return false;

        // We always expect errors because we are using a self-signed certificate,
        // but only G_TLS_CERTIFICATE_UNKNOWN_CA flags should be present.
        return !errors || errors == G_TLS_CERTIFICATE_UNKNOWN_CA;
    }

    GRefPtr<GTlsCertificate> m_clientCertificate;
    bool m_rejectClientCertificates { false };
};

class ClientSideCertificateTest final : public LoadTrackingTest, public ClientSideCertificateTestBase {
public:
    MAKE_GLIB_TEST_FIXTURE(ClientSideCertificateTest);

    ClientSideCertificateTest()
        : LoadTrackingTest()
        , ClientSideCertificateTestBase(m_webView)
    {
    }

    void authenticate(WebKitAuthenticationRequest* request) override
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));
        m_authenticationRequest = request;
        g_main_loop_quit(m_mainLoop);
    }

    WebKitAuthenticationRequest* waitForAuthenticationRequest()
    {
        m_authenticationRequest = nullptr;
        g_main_loop_run(m_mainLoop);
        return m_authenticationRequest.get();
    }

    GRefPtr<WebKitAuthenticationRequest> m_authenticationRequest;
};

static void testClientSideCertificate(ClientSideCertificateTest* test, gconstpointer)
{
    // Ignore server certificate errors.
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    // Cancel the authentiation request.
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    auto* request = test->waitForAuthenticationRequest();
    g_assert_cmpstr(webkit_authentication_request_get_realm(request), ==, "");
    g_assert_cmpint(webkit_authentication_request_get_scheme(request), ==, WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED);
    g_assert_false(webkit_authentication_request_is_for_proxy(request));
    g_assert_false(webkit_authentication_request_is_retry(request));
    auto* origin = webkit_authentication_request_get_security_origin(request);
    g_assert_nonnull(origin);
    ASSERT_CMP_CSTRING(webkit_security_origin_get_protocol(origin), ==, kHttpsServer->baseURL().protocol().toString().utf8());
    ASSERT_CMP_CSTRING(webkit_security_origin_get_host(origin), ==, kHttpsServer->baseURL().host().toString().utf8());
    g_assert_cmpuint(webkit_security_origin_get_port(origin), ==, kHttpsServer->port());
    webkit_security_origin_unref(origin);
    webkit_authentication_request_cancel(request);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    g_assert_error(test->m_error.get(), WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED);
    test->m_loadEvents.clear();

    // Complete the request with no credential.
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    request = test->waitForAuthenticationRequest();
    webkit_authentication_request_authenticate(request, nullptr);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    // Sometimes glib-networking fails to report the error as certificate required and we end up
    // with connection reset by peer because the server closes the connection.
    if (!g_error_matches(test->m_error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
        g_assert_error(test->m_error.get(), G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED);
    test->m_loadEvents.clear();

    // Complete the request with a credential with no certificate.
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    request = test->waitForAuthenticationRequest();
    WebKitCredential* credential = webkit_credential_new_for_certificate(nullptr, WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    if (!g_error_matches(test->m_error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
        g_assert_error(test->m_error.get(), G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED);
    test->m_loadEvents.clear();

    // Complete the request with a credential with an invalid certificate.
    test->m_rejectClientCertificates = true;
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    request = test->waitForAuthenticationRequest();
    credential = webkit_credential_new_for_certificate(test->m_clientCertificate.get(), WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    if (!g_error_matches(test->m_error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
        g_assert_error(test->m_error.get(), G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED);
    test->m_loadEvents.clear();
    test->m_rejectClientCertificates = false;

    // Complete the request with a credential with a valid certificate.
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    request = test->waitForAuthenticationRequest();
    credential = webkit_credential_new_for_certificate(test->m_clientCertificate.get(), WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(request, credential);
    webkit_credential_free(credential);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    test->m_loadEvents.clear();

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}

class WebSocketClientSideCertificateTest final : public WebSocketTest, public ClientSideCertificateTestBase {
public:
    MAKE_GLIB_TEST_FIXTURE(WebSocketClientSideCertificateTest);

    WebSocketClientSideCertificateTest()
        : WebSocketTest()
        , ClientSideCertificateTestBase(m_webView)
    {
    }

    void authenticate(WebKitAuthenticationRequest* request) override
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        WebKitCredential* credential = webkit_credential_new_for_certificate(m_clientCertificate.get(), WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
        webkit_authentication_request_authenticate(request, credential);
        webkit_credential_free(credential);
    }
};

static void testWebSocketClientSideCertificate(WebSocketClientSideCertificateTest* test, gconstpointer)
{
    // Ignore server certificate errors.
    auto* websiteDataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    WebKitTLSErrorsPolicy originalPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    // Try first without having the certificate in credential storage.
    auto events = test->connectToServerAndWaitForEvents(kHttpsServer);
    g_assert_true(events);
    g_assert_false(events & WebSocketTest::EventFlags::DidServerCompleteHandshake);
    g_assert_false(events & WebSocketTest::EventFlags::DidOpen);
    g_assert_true(events & WebSocketTest::EventFlags::DidClose);

    // Load the page to ensure the certificate is stored in session credential sotorage.
    test->loadURI(kHttpsServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();

    // And try to connect again now with the certificate in credential storage.
    events = test->connectToServerAndWaitForEvents(kHttpsServer);
    g_assert_true(events);
    g_assert_true(events & WebSocketTest::EventFlags::DidServerCompleteHandshake);
    g_assert_true(events & WebSocketTest::EventFlags::DidOpen);
    g_assert_false(events & WebSocketTest::EventFlags::DidClose);

    webkit_website_data_manager_set_tls_errors_policy(websiteDataManager, originalPolicy);
}
#endif

#if USE(SOUP2)
static void httpsServerCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void httpsServerCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    g_assert_false(assertIfSSLRequestProcessed);

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, indexHTML, strlen(indexHTML));
        soup_message_body_complete(responseBody);
    } else if (g_str_equal(path, "/insecure-content/")) {
        GUniquePtr<char> responseHTML(g_strdup_printf(insecureContentHTML, kHttpServer->getURIForPath("/test-script").data(), kHttpServer->getURIForPath("/test-image").data()));
        soup_message_body_append(responseBody, SOUP_MEMORY_COPY, responseHTML.get(), strlen(responseHTML.get()));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/test-tls/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, TLSSuccessHTMLString, strlen(TLSSuccessHTMLString));
        soup_message_body_complete(responseBody);
    } else if (g_str_equal(path, "/redirect")) {
        soup_server_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY, nullptr);
        soup_message_headers_append(soup_server_message_get_response_headers(message), "Location", kHttpServer->getURIForPath("/test-image").data());
    } else if (g_str_equal(path, "/auth")) {
        soup_server_message_set_status(message, SOUP_STATUS_UNAUTHORIZED, nullptr);
        soup_message_headers_append(soup_server_message_get_response_headers(message), "WWW-Authenticate", "Basic realm=\"HTTPS auth\"");
    } else if (g_str_equal(path, "/style.css")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        static const char* styleCSS = "body { color: black; }";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, styleCSS, strlen(styleCSS));
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

#if USE(SOUP2)
static void httpServerCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void httpServerCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/test-script")) {
        GUniquePtr<char> pathToFile(g_build_filename(Test::getResourcesDir().data(), "link-title.js", nullptr));
        char* contents;
        gsize length;
        g_file_get_contents(pathToFile.get(), &contents, &length, 0);

        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, length);
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/test-image")) {
        GUniquePtr<char> pathToFile(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
        char* contents;
        gsize length;
        g_file_get_contents(pathToFile.get(), &contents, &length, 0);

        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, length);
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        char* responseHTML = g_strdup_printf("<html><head><link rel='stylesheet' href='%s' type='text/css'></head><body>SSL subresource test</body></html>",
            kHttpsServer->getURIForPath("/style.css").data());
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, responseHTML, strlen(responseHTML));
        soup_message_body_complete(responseBody);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

void beforeAll()
{
    kHttpsServer = new WebKitTestServer(WebKitTestServer::ServerHTTPS);
    kHttpsServer->run(httpsServerCallback);

    kHttpServer = new WebKitTestServer();
    kHttpServer->run(httpServerCallback);

    SSLTest::add("WebKitWebView", "ssl", testSSL);
    InsecureContentTest::add("WebKitWebView", "insecure-content", testInsecureContent);
    SSLTest::add("WebKitWebView", "tls-errors-policy", testTLSErrorsPolicy);
    SSLTest::add("WebKitWebView", "tls-errors-redirect-to-http", testTLSErrorsRedirect);
    SSLTest::add("WebKitWebView", "tls-http-auth", testTLSErrorsHTTPAuth);
    TLSSubresourceTest::add("WebKitWebView", "tls-subresource", testSubresourceLoadFailedWithTLSErrors);
    TLSErrorsTest::add("WebKitWebView", "load-failed-with-tls-errors", testLoadFailedWithTLSErrors);
    WebSocketTest::add("WebKitWebView", "web-socket-tls-errors", testWebSocketTLSErrors);
    EphemeralSSLTest::add("WebKitWebView", "ephemeral-tls-errors", testTLSErrorsEphemeral);
#if !USE(SOUP2)
    ClientSideCertificateTest::add("WebKitWebView", "client-side-certificate", testClientSideCertificate);
    WebSocketClientSideCertificateTest::add("WebKitWebView", "web-socket-client-side-certificate", testWebSocketClientSideCertificate);
#endif
}

void afterAll()
{
    delete kHttpsServer;
    delete kHttpServer;
}
