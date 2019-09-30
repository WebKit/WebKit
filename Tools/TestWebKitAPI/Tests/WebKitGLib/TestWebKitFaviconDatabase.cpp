/*
 * Copyright (C) 2012, 2017 Igalia S.L.
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

#if PLATFORM(GTK)

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <wtf/glib/GUniquePtr.h>

static WebKitTestServer* kServer;

class FaviconDatabaseTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FaviconDatabaseTest);

    FaviconDatabaseTest()
        : m_database(webkit_web_context_get_favicon_database(m_webContext.get()))
    {
        g_assert_true(WEBKIT_IS_FAVICON_DATABASE(m_database));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_database));
        g_signal_connect(m_database, "favicon-changed", G_CALLBACK(faviconChangedCallback), this);
    }

    ~FaviconDatabaseTest()
    {
        if (m_favicon)
            cairo_surface_destroy(m_favicon);

        g_signal_handlers_disconnect_matched(m_database, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void open(const char* directory)
    {
        GUniquePtr<char> databaseDirectory(g_build_filename(dataDirectory(), directory, nullptr));
        webkit_web_context_set_favicon_database_directory(m_webContext.get(), databaseDirectory.get());
        g_assert_cmpstr(databaseDirectory.get(), ==, webkit_web_context_get_favicon_database_directory(m_webContext.get()));
    }

    static void faviconChangedCallback(WebKitFaviconDatabase* database, const char* pageURI, const char* faviconURI, FaviconDatabaseTest* test)
    {
        if (!g_strcmp0(webkit_web_view_get_uri(test->m_webView), pageURI)) {
            test->m_faviconURI = faviconURI;
            if (test->m_waitingForFaviconURI)
                test->quitMainLoop();
        }
    }

    static void viewFaviconChangedCallback(WebKitWebView* webView, GParamSpec* pspec, FaviconDatabaseTest* test)
    {
        g_assert_true(test->m_webView == webView);
        test->m_faviconNotificationReceived = true;
        if (test->m_loadFinished)
            test->quitMainLoop();
    }

    static void viewLoadChangedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, FaviconDatabaseTest* test)
    {
        g_assert_true(test->m_webView == webView);
        if (loadEvent != WEBKIT_LOAD_FINISHED)
            return;

        test->m_loadFinished = true;
        if (test->m_faviconNotificationReceived)
            test->quitMainLoop();
    }

    static void getFaviconCallback(GObject* sourceObject, GAsyncResult* result, void* data)
    {
        FaviconDatabaseTest* test = static_cast<FaviconDatabaseTest*>(data);
        test->m_favicon = webkit_favicon_database_get_favicon_finish(test->m_database, result, &test->m_error.outPtr());
        test->quitMainLoop();
    }

    void waitUntilFaviconChanged()
    {
        m_faviconNotificationReceived = false;
        unsigned long handlerID = g_signal_connect(m_webView, "notify::favicon", G_CALLBACK(viewFaviconChangedCallback), this);
        g_main_loop_run(m_mainLoop);
        g_signal_handler_disconnect(m_webView, handlerID);
    }

    void waitUntilLoadFinishedAndFaviconChanged()
    {
        m_faviconNotificationReceived = false;
        m_loadFinished = false;
        unsigned long faviconChangedID = g_signal_connect(m_webView, "notify::favicon", G_CALLBACK(viewFaviconChangedCallback), this);
        unsigned long loadChangedID = g_signal_connect(m_webView, "load-changed", G_CALLBACK(viewLoadChangedCallback), this);
        g_main_loop_run(m_mainLoop);
        g_signal_handler_disconnect(m_webView, faviconChangedID);
        g_signal_handler_disconnect(m_webView, loadChangedID);
    }

    void getFaviconForPageURIAndWaitUntilReady(const char* pageURI)
    {
        if (m_favicon) {
            cairo_surface_destroy(m_favicon);
            m_favicon = nullptr;
        }

        webkit_favicon_database_get_favicon(m_database, pageURI, 0, getFaviconCallback, this);
        g_main_loop_run(m_mainLoop);
    }

    void waitUntilFaviconURIChanged()
    {
        g_assert_false(m_waitingForFaviconURI);
        m_faviconURI = CString();
        m_waitingForFaviconURI = true;
        g_main_loop_run(m_mainLoop);
        m_waitingForFaviconURI = false;
    }

    WebKitFaviconDatabase* m_database { nullptr };
    cairo_surface_t* m_favicon { nullptr };
    CString m_faviconURI;
    GUniqueOutPtr<GError> m_error;
    bool m_faviconNotificationReceived { false };
    bool m_loadFinished { false };
    bool m_waitingForFaviconURI { false };
};

static void
serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable* query, SoupClientContext* context, void* data)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (g_str_equal(path, "/favicon.ico")) {
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
        soup_message_body_complete(message->response_body);
        return;
    }

    char* contents;
    gsize length;
    if (g_str_equal(path, "/icon/favicon.ico")) {
        GUniquePtr<char> pathToFavicon(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
        g_file_get_contents(pathToFavicon.get(), &contents, &length, 0);
        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/nofavicon")) {
        static const char* noFaviconHTML = "<html><head><body>test</body></html>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, noFaviconHTML, strlen(noFaviconHTML));
    } else {
        static const char* contentsHTML = "<html><head><link rel='icon' href='/icon/favicon.ico' type='image/x-ico; charset=binary'></head><body>test</body></html>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, contentsHTML, strlen(contentsHTML));
    }

    soup_message_set_status(message, SOUP_STATUS_OK);
    soup_message_body_complete(message->response_body);
}

static void testFaviconDatabaseInitialization(FaviconDatabaseTest* test, gconstpointer)
{
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_null(test->m_favicon);
    g_assert_error(test->m_error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED);

    test->open("testFaviconDatabaseInitialization");
    GUniquePtr<char> databaseFile(g_build_filename(webkit_web_context_get_favicon_database_directory(test->m_webContext.get()), "WebpageIcons.db", nullptr));
    g_assert_true(g_file_test(databaseFile.get(), G_FILE_TEST_IS_REGULAR));
}

static void testFaviconDatabaseGetFavicon(FaviconDatabaseTest* test, gconstpointer)
{
    test->open("testFaviconDatabaseGetFavicon");

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    CString faviconURI = kServer->getURIForPath("/icon/favicon.ico");

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_cmpint(cairo_image_surface_get_width(test->m_favicon), ==, 16);
    g_assert_cmpint(cairo_image_surface_get_height(test->m_favicon), ==, 16);
    g_assert_cmpstr(test->m_faviconURI.data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());

    // Check that another page with the same favicon returns the same icon.
    cairo_surface_t* favicon = cairo_surface_reference(test->m_favicon);
    test->loadURI(kServer->getURIForPath("/bar").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    // favicon changes twice, first to reset it and then when the new icon is loaded.
    test->waitUntilFaviconChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/bar").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_true(test->m_favicon == favicon);
    g_assert_cmpstr(test->m_faviconURI.data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());
    cairo_surface_destroy(favicon);

    faviconURI = kServer->getURIForPath("/favicon.ico");
    test->loadURI(kServer->getURIForPath("/nofavicon").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    test->waitUntilFaviconURIChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/nofavicon").data());
    g_assert_null(test->m_favicon);
    g_assert_cmpstr(test->m_faviconURI.data(), ==, faviconURI.data());
    g_assert_error(test->m_error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN);

    // Loading an icon that is already in the database should emit
    // WebKitWebView::notify::favicon, but not WebKitFaviconDatabase::icon-changed.
    g_assert_null(webkit_web_view_get_favicon(test->m_webView));
    test->m_faviconURI = { };
    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilFaviconChanged();
    g_assert_true(test->m_faviconURI.isNull());
    g_assert_nonnull(webkit_web_view_get_favicon(test->m_webView));
}

static void ephemeralViewFaviconChanged(WebKitWebView* webView, GParamSpec*, WebViewTest* test)
{
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewFaviconChanged), test);
    test->quitMainLoop();
}

static void testFaviconDatabaseEphemeral(FaviconDatabaseTest* test, gconstpointer)
{
    // If the context is ephemeral, the database is not created if it doesn't exist.
    GRefPtr<WebKitWebContext> ephemeralContext = adoptGRef(webkit_web_context_new_ephemeral());
    GUniquePtr<char> databaseDirectory(g_build_filename(Test::dataDirectory(), "testFaviconDatabaseEphemeral", nullptr));
    webkit_web_context_set_favicon_database_directory(ephemeralContext.get(), databaseDirectory.get());
    g_assert_cmpstr(databaseDirectory.get(), ==, webkit_web_context_get_favicon_database_directory(ephemeralContext.get()));
    GUniquePtr<char> databaseFile(g_build_filename(databaseDirectory.get(), "WebpageIcons.db", nullptr));
    g_assert_false(g_file_test(databaseFile.get(), G_FILE_TEST_EXISTS));
    ephemeralContext = nullptr;

    test->open("testFaviconDatabaseEphemeral");
    g_assert_true(g_file_test(databaseFile.get(), G_FILE_TEST_EXISTS));

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();

    // An ephemeral web view doesn't write to the database.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", test->m_webContext.get(), "is-ephemeral", TRUE, nullptr));
    g_signal_connect(webView.get(), "notify::favicon", G_CALLBACK(ephemeralViewFaviconChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/bar").data());
    g_main_loop_run(test->m_mainLoop);

    // We get a favicon, but it's only in database memory cache.
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(test->m_database, kServer->getURIForPath("/bar").data()));

    // If the database exists, it's used in read only mode for epeheral contexts.
    ephemeralContext = adoptGRef(webkit_web_context_new_ephemeral());
    webkit_web_context_set_favicon_database_directory(ephemeralContext.get(), databaseDirectory.get());
    auto* ephemeralDatabase = webkit_web_context_get_favicon_database(ephemeralContext.get());
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(ephemeralDatabase, kServer->getURIForPath("/foo").data()));

    // Page URL loaded in ephemeral web view is not in the database.
    g_assert_null(webkit_favicon_database_get_favicon_uri(ephemeralDatabase, kServer->getURIForPath("/bar").data()));
    ephemeralContext = nullptr;
}

void testFaviconDatabaseClear(FaviconDatabaseTest* test, gconstpointer)
{
    test->open("testFaviconDatabaseClear");
    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(test->m_database, kServer->getURIForPath("/foo").data()));

    webkit_favicon_database_clear(test->m_database);

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_null(test->m_favicon);
    g_assert_null(webkit_favicon_database_get_favicon_uri(test->m_database, kServer->getURIForPath("/foo").data()));
}

void beforeAll()
{
    // Start a soup server for testing.
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    FaviconDatabaseTest::add("WebKitFaviconDatabase", "initialization", testFaviconDatabaseInitialization);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "get-favicon", testFaviconDatabaseGetFavicon);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "ephemeral", testFaviconDatabaseEphemeral);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "clear", testFaviconDatabaseClear);
}

void afterAll()
{
    delete kServer;
}

#endif
