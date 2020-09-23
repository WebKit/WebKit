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
        g_assert_error(error, SOUP_HTTP_ERROR, SOUP_STATUS_SSL_FAILED);
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
        , m_failingURI(nullptr)
    {
    }

    ~TLSErrorsTest()
    {
        if (m_failingURI)
            soup_uri_free(m_failingURI);
    }

    bool loadFailedWithTLSErrors(const char* failingURI, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors) override
    {
        LoadTrackingTest::loadFailedWithTLSErrors(failingURI, certificate, tlsErrors);

        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(certificate));
        m_certificate = certificate;
        m_tlsErrors = tlsErrors;
        if (m_failingURI)
            soup_uri_free(m_failingURI);
        m_failingURI = soup_uri_new(failingURI);
        return true;
    }

    GTlsCertificate* certificate() const { return m_certificate.get(); }
    GTlsCertificateFlags tlsErrors() const { return m_tlsErrors; }
    const char* host() const { return m_failingURI->host; }

private:
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
    SoupURI* m_failingURI;
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
    g_assert_cmpstr(test->host(), ==, soup_uri_get_host(kHttpsServer->baseURI()));
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadFailedWithTLSErrors);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
    assertIfSSLRequestProcessed = false;

    // Test allowing an exception for this certificate on this host.
    webkit_web_context_allow_tls_certificate_for_host(test->m_webContext.get(), test->certificate(), test->host());
    // The page should now load without errors.
    test->loadURI(kHttpsServer->getURIForPath("/test-tls/").data());
    test->waitUntilLoadFinished();

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

    static void serverWebSocketCallback(SoupServer*, SoupWebsocketConnection*, const char*, SoupClientContext*, gpointer userData)
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

static void httpsServerCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    g_assert_false(assertIfSSLRequestProcessed);

    if (g_str_equal(path, "/")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, indexHTML, strlen(indexHTML));
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/insecure-content/")) {
        GUniquePtr<char> responseHTML(g_strdup_printf(insecureContentHTML, kHttpServer->getURIForPath("/test-script").data(), kHttpServer->getURIForPath("/test-image").data()));
        soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, responseHTML.get(), strlen(responseHTML.get()));
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/test-tls/")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, TLSSuccessHTMLString, strlen(TLSSuccessHTMLString));
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/redirect")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", kHttpServer->getURIForPath("/test-image").data());
    } else if (g_str_equal(path, "/auth")) {
        soup_message_set_status(message, SOUP_STATUS_UNAUTHORIZED);
        soup_message_headers_append(message->response_headers, "WWW-Authenticate", "Basic realm=\"HTTPS auth\"");
    } else if (g_str_equal(path, "/style.css")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        static const char* styleCSS = "body { color: black; }";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, styleCSS, strlen(styleCSS));
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
}

static void httpServerCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (g_str_equal(path, "/test-script")) {
        GUniquePtr<char> pathToFile(g_build_filename(Test::getResourcesDir().data(), "link-title.js", nullptr));
        char* contents;
        gsize length;
        g_file_get_contents(pathToFile.get(), &contents, &length, 0);

        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, contents, length);
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/test-image")) {
        GUniquePtr<char> pathToFile(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
        char* contents;
        gsize length;
        g_file_get_contents(pathToFile.get(), &contents, &length, 0);

        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, contents, length);
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        char* responseHTML = g_strdup_printf("<html><head><link rel='stylesheet' href='%s' type='text/css'></head><body>SSL subresource test</body></html>",
            kHttpsServer->getURIForPath("/style.css").data());
        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, responseHTML, strlen(responseHTML));
        soup_message_body_complete(message->response_body);
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
}

void beforeAll()
{
    kHttpsServer = new WebKitTestServer(WebKitTestServer::ServerHTTPS);
    kHttpsServer->run(httpsServerCallback);

    kHttpServer = new WebKitTestServer(WebKitTestServer::ServerHTTP);
    kHttpServer->run(httpServerCallback);

    SSLTest::add("WebKitWebView", "ssl", testSSL);
    InsecureContentTest::add("WebKitWebView", "insecure-content", testInsecureContent);
    SSLTest::add("WebKitWebView", "tls-errors-policy", testTLSErrorsPolicy);
    SSLTest::add("WebKitWebView", "tls-errors-redirect-to-http", testTLSErrorsRedirect);
    SSLTest::add("WebKitWebView", "tls-http-auth", testTLSErrorsHTTPAuth);
    TLSSubresourceTest::add("WebKitWebView", "tls-subresource", testSubresourceLoadFailedWithTLSErrors);
    TLSErrorsTest::add("WebKitWebView", "load-failed-with-tls-errors", testLoadFailedWithTLSErrors);
    WebSocketTest::add("WebKitWebView", "web-socket-tls-errors", testWebSocketTLSErrors);
}

void afterAll()
{
    delete kHttpsServer;
    delete kHttpServer;
}
