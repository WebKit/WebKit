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

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <glib/gstdio.h>

static WebKitTestServer* kServer;

static const char* kFirstPartyDomain = "127.0.0.1";
static const char* kThirdPartyDomain = "localhost";
static const char* kIndexHtmlFormat =
    "<html><body>"
    " <p>WebKitGTK+ Cookie Manager test</p>"
    " <img src='http://localhost:%u/image.png' width=5 height=5></img>"
    "</body></html>";

class CookieManagerTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(CookieManagerTest);

    static void cookiesChangedCallback(WebKitCookieManager*, CookieManagerTest* test)
    {
        test->m_cookiesChanged = true;
        if (test->m_finishLoopWhenCookiesChange && !(--test->m_cookiesExpectedToChangeCount))
            g_main_loop_quit(test->m_mainLoop);
    }

    CookieManagerTest()
        : WebViewTest()
        , m_cookieManager(webkit_web_context_get_cookie_manager(webkit_web_view_get_context(m_webView)))
    {
        g_assert(webkit_website_data_manager_get_cookie_manager(webkit_web_context_get_website_data_manager(webkit_web_view_get_context(m_webView))) == m_cookieManager);
        g_signal_connect(m_cookieManager, "changed", G_CALLBACK(cookiesChangedCallback), this);
    }

    ~CookieManagerTest()
    {
        g_strfreev(m_domains);
        g_signal_handlers_disconnect_matched(m_cookieManager, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        if (m_cookiesTextFile)
            g_unlink(m_cookiesTextFile.get());
        if (m_cookiesSQLiteFile)
            g_unlink(m_cookiesSQLiteFile.get());
    }

    void setPersistentStorage(WebKitCookiePersistentStorage storage)
    {
        const char* filename = 0;
        switch (storage) {
        case WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT:
            if (!m_cookiesTextFile)
                m_cookiesTextFile.reset(g_build_filename(Test::dataDirectory(), "cookies.txt", nullptr));
            filename = m_cookiesTextFile.get();
            break;
        case WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE:
            if (!m_cookiesSQLiteFile)
                m_cookiesSQLiteFile.reset(g_build_filename(Test::dataDirectory(), "cookies.db", nullptr));
            filename = m_cookiesSQLiteFile.get();
            break;
        default:
            g_assert_not_reached();
        }
        webkit_cookie_manager_set_persistent_storage(m_cookieManager, filename, storage);
    }

    static void getAcceptPolicyReadyCallback(GObject* object, GAsyncResult* result, gpointer userData)
    {
        GUniqueOutPtr<GError> error;
        WebKitCookieAcceptPolicy policy = webkit_cookie_manager_get_accept_policy_finish(WEBKIT_COOKIE_MANAGER(object), result, &error.outPtr());
        g_assert(!error.get());

        CookieManagerTest* test = static_cast<CookieManagerTest*>(userData);
        test->m_acceptPolicy = policy;
        g_main_loop_quit(test->m_mainLoop);
    }

    WebKitCookieAcceptPolicy getAcceptPolicy()
    {
        m_acceptPolicy = WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
        webkit_cookie_manager_get_accept_policy(m_cookieManager, 0, getAcceptPolicyReadyCallback, this);
        g_main_loop_run(m_mainLoop);

        return m_acceptPolicy;
    }

    void setAcceptPolicy(WebKitCookieAcceptPolicy policy)
    {
        webkit_cookie_manager_set_accept_policy(m_cookieManager, policy);
    }

    static void getDomainsReadyCallback(GObject* object, GAsyncResult* result, gpointer userData)
    {
        GUniqueOutPtr<GError> error;
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        char** domains = webkit_cookie_manager_get_domains_with_cookies_finish(WEBKIT_COOKIE_MANAGER(object), result, &error.outPtr());
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_assert(!error.get());

        CookieManagerTest* test = static_cast<CookieManagerTest*>(userData);
        test->m_domains = domains;
        g_main_loop_quit(test->m_mainLoop);
    }

    char** getDomains()
    {
        g_strfreev(m_domains);
        m_domains = 0;
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        webkit_cookie_manager_get_domains_with_cookies(m_cookieManager, 0, getDomainsReadyCallback, this);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_main_loop_run(m_mainLoop);

        return m_domains;
    }

    bool hasDomain(const char* domain)
    {
        if (!m_domains)
            return false;

        for (size_t i = 0; m_domains[i]; ++i) {
            if (g_str_equal(m_domains[i], domain))
                return true;
        }
        return false;
    }

    void deleteCookiesForDomain(const char* domain)
    {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        webkit_cookie_manager_delete_cookies_for_domain(m_cookieManager, domain);
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }

    void deleteAllCookies()
    {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        webkit_cookie_manager_delete_all_cookies(m_cookieManager);
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }

    void waitUntilCookiesChanged(int cookiesExpectedToChangeCount = 1)
    {
        m_cookiesChanged = false;
        m_cookiesExpectedToChangeCount = cookiesExpectedToChangeCount;
        m_finishLoopWhenCookiesChange = true;
        g_main_loop_run(m_mainLoop);
        m_finishLoopWhenCookiesChange = false;
    }

    WebKitCookieManager* m_cookieManager { nullptr };
    WebKitCookieAcceptPolicy m_acceptPolicy { WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY };
    char** m_domains { nullptr };
    bool m_cookiesChanged { false };
    int m_cookiesExpectedToChangeCount { 0 };
    bool m_finishLoopWhenCookiesChange { false };
    GUniquePtr<char> m_cookiesTextFile;
    GUniquePtr<char> m_cookiesSQLiteFile;
};

static void testCookieManagerAcceptPolicy(CookieManagerTest* test, gconstpointer)
{
    // Default policy is NO_THIRD_PARTY.
    g_assert_cmpint(test->getAcceptPolicy(), ==, WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);
    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    char** domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 1);
    g_assert_cmpstr(domains[0], ==, kFirstPartyDomain);
    test->deleteAllCookies();

    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    g_assert_cmpint(test->getAcceptPolicy(), ==, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 2);
    g_assert(test->hasDomain(kFirstPartyDomain));
    g_assert(test->hasDomain(kThirdPartyDomain));
    test->deleteAllCookies();

    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_NEVER);
    g_assert_cmpint(test->getAcceptPolicy(), ==, WEBKIT_COOKIE_POLICY_ACCEPT_NEVER);
    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);
}

static void testCookieManagerDeleteCookies(CookieManagerTest* test, gconstpointer)
{
    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 2);

    // Delete first party cookies.
    test->deleteCookiesForDomain(kFirstPartyDomain);
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 1);

    // Delete third party cookies.
    test->deleteCookiesForDomain(kThirdPartyDomain);
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 0);

    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 2);

    // Delete all cookies.
    test->deleteAllCookies();
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 0);
}

static void testCookieManagerCookiesChanged(CookieManagerTest* test, gconstpointer)
{
    g_assert(!test->m_cookiesChanged);
    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    g_assert(test->m_cookiesChanged);

    test->deleteCookiesForDomain(kFirstPartyDomain);
    test->waitUntilCookiesChanged();
    g_assert(test->m_cookiesChanged);

    test->deleteAllCookies();
    test->waitUntilCookiesChanged();
    g_assert(test->m_cookiesChanged);
}

static void testCookieManagerPersistentStorage(CookieManagerTest* test, gconstpointer)
{
    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);

    // Text storage using a new file.
    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
    char** domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);

    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    g_assert(test->m_cookiesChanged);
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 2);


    // SQLite storage using a new file.
    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);

    test->loadURI(kServer->getURIForPath("/index.html").data());
    test->waitUntilLoadFinished();
    g_assert(test->m_cookiesChanged);
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 2);

    // Text storage using an existing file.
    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 2);
    test->deleteAllCookies();
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 0);

    // SQLite storage with an existing file.
    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 2);
    test->deleteAllCookies();
    g_assert_cmpint(g_strv_length(test->getDomains()), ==, 0);
}

static void testCookieManagerPersistentStorageDeleteAll(CookieManagerTest* test, gconstpointer)
{
    // This checks that we can remove all the cookies of an existing file before a web process is created.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=175265.
    static const char cookiesFileFormat[] = "127.0.0.1\tFALSE\t/\tFALSE\t%ld\tfoo\tbar\nlocalhost\tFALSE\t/\tFALSE\t%ld\tbaz\tqux\n";
    time_t expires = time(nullptr) + 60;
    GUniquePtr<char> cookiesFileContents(g_strdup_printf(cookiesFileFormat, expires, expires));
    GUniquePtr<char> cookiesFile(g_build_filename(Test::dataDirectory(), "cookies.txt", nullptr));
    g_assert(g_file_set_contents(cookiesFile.get(), cookiesFileContents.get(), -1, nullptr));

    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
    test->deleteAllCookies();
    // Changed signal is emitted for every deleted cookie, twice in this case.
    test->waitUntilCookiesChanged(2);

    // Ensure the web process is created and load something without cookies.
    test->m_cookiesChanged = false;
    test->loadURI(kServer->getURIForPath("/no-cookies.html").data());
    test->waitUntilLoadFinished();
    g_assert(!test->m_cookiesChanged);
    char** domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);
}

static void ephemeralViewloadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewloadChanged), test);
    test->quitMainLoop();
}

static void testCookieManagerEphemeral(CookieManagerTest* test, gconstpointer)
{
    test->setAcceptPolicy(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    test->setPersistentStorage(WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
    char** domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);

    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(test->m_webView),
        "is-ephemeral", TRUE,
        nullptr));
    g_assert(webkit_web_view_is_ephemeral(webView.get()));
    g_assert(!webkit_web_context_is_ephemeral(webkit_web_view_get_context(webView.get())));

    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(ephemeralViewloadChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/index.html").data());
    g_main_loop_run(test->m_mainLoop);

    domains = test->getDomains();
    g_assert(domains);
    g_assert_cmpint(g_strv_length(domains), ==, 0);

    auto* viewDataManager = webkit_web_view_get_website_data_manager(webView.get());
    g_assert(WEBKIT_IS_WEBSITE_DATA_MANAGER(viewDataManager));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(viewDataManager));
    g_assert(viewDataManager != webkit_web_context_get_website_data_manager(webkit_web_view_get_context(test->m_webView)));
    auto* cookieManager = webkit_website_data_manager_get_cookie_manager(viewDataManager);
    g_assert(WEBKIT_IS_COOKIE_MANAGER(cookieManager));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(cookieManager));
    g_assert(cookieManager != test->m_cookieManager);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    webkit_cookie_manager_get_domains_with_cookies(cookieManager, nullptr, [](GObject* object, GAsyncResult* result, gpointer userData) {
        auto* test = static_cast<CookieManagerTest*>(userData);
        GUniquePtr<char*> domains(webkit_cookie_manager_get_domains_with_cookies_finish(WEBKIT_COOKIE_MANAGER(object), result, nullptr));
        g_assert(domains);
        g_assert_cmpint(g_strv_length(domains.get()), ==, 1);
        g_assert_cmpstr(domains.get()[0], ==, kFirstPartyDomain);
        test->quitMainLoop();
    }, test);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_main_loop_run(test->m_mainLoop);
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);
    if (g_str_equal(path, "/index.html")) {
        char* indexHtml = g_strdup_printf(kIndexHtmlFormat, soup_server_get_port(server));
        soup_message_headers_replace(message->response_headers, "Set-Cookie", "foo=bar; Max-Age=60");
        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, indexHtml, strlen(indexHtml));
    } else if (g_str_equal(path, "/image.png"))
        soup_message_headers_replace(message->response_headers, "Set-Cookie", "baz=qux; Max-Age=60");
    else if (g_str_equal(path, "/no-cookies.html")) {
        static const char* indexHtml = "<html><body><p>No cookies</p></body></html>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, indexHtml, strlen(indexHtml));
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
    soup_message_body_complete(message->response_body);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    CookieManagerTest::add("WebKitCookieManager", "accept-policy", testCookieManagerAcceptPolicy);
    CookieManagerTest::add("WebKitCookieManager", "delete-cookies", testCookieManagerDeleteCookies);
    CookieManagerTest::add("WebKitCookieManager", "cookies-changed", testCookieManagerCookiesChanged);
    CookieManagerTest::add("WebKitCookieManager", "persistent-storage", testCookieManagerPersistentStorage);
    CookieManagerTest::add("WebKitCookieManager", "persistent-storage-delete-all", testCookieManagerPersistentStorageDeleteAll);
    CookieManagerTest::add("WebKitCookieManager", "ephemeral", testCookieManagerEphemeral);
}

void afterAll()
{
    delete kServer;
}
