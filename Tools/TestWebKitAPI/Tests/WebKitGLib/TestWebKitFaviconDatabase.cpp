/*
 * Copyright (C) 2012, 2017 Igalia S.L.
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

#if PLATFORM(GTK)

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <WebCore/SoupVersioning.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <wtf/glib/GUniquePtr.h>

static WebKitTestServer* kServer;

class FaviconDatabaseTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FaviconDatabaseTest);

    FaviconDatabaseTest()
#if ENABLE(2022_GLIB_API)
        : m_database(webkit_website_data_manager_get_favicon_database(webkit_network_session_get_website_data_manager(m_networkSession.get())))
#else
        : m_database(webkit_web_context_get_favicon_database(m_webContext.get()))
#endif
    {
#if ENABLE(2022_GLIB_API)
        // In 2022 API when favicons are disabled, the database is nullptr.
        g_assert_null(m_database.get());
#else
        g_assert_true(WEBKIT_IS_FAVICON_DATABASE(m_database.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_database.get()));
        g_signal_connect(m_database.get(), "favicon-changed", G_CALLBACK(faviconChangedCallback), this);
#endif
    }

    ~FaviconDatabaseTest()
    {
#if !USE(GTK4)
        if (m_favicon)
            cairo_surface_destroy(m_favicon);
#endif

        g_signal_handlers_disconnect_matched(m_database.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
#if ENABLE(2022_GLIB_API)
        GUniquePtr<char> databaseDirectory(g_build_filename(Test::dataDirectory(), "icondatabase", nullptr));
        if (GDir* directory = g_dir_open(databaseDirectory.get(), 0, nullptr)) {
            const char* fileName;
            while ((fileName = g_dir_read_name(directory))) {
                GUniquePtr<char> filePath(g_build_filename(databaseDirectory.get(), fileName, nullptr));
                g_unlink(filePath.get());
            }
            g_dir_close(directory);
        }
        g_rmdir(databaseDirectory.get());
#endif
    }

    void open(const char* directory)
    {
#if ENABLE(2022_GLIB_API)
        g_assert_null(m_database.get());
        auto* manager = webkit_network_session_get_website_data_manager(m_networkSession.get());
        g_assert_false(webkit_website_data_manager_get_favicons_enabled(manager));
        webkit_website_data_manager_set_favicons_enabled(manager, TRUE);
        m_database = webkit_website_data_manager_get_favicon_database(manager);
        g_assert_true(WEBKIT_IS_FAVICON_DATABASE(m_database.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_database.get()));
        g_signal_connect(m_database.get(), "favicon-changed", G_CALLBACK(faviconChangedCallback), this);
#else
        GUniquePtr<char> databaseDirectory(g_build_filename(dataDirectory(), directory, nullptr));
        webkit_web_context_set_favicon_database_directory(m_webContext.get(), databaseDirectory.get());
        g_assert_cmpstr(databaseDirectory.get(), ==, webkit_web_context_get_favicon_database_directory(m_webContext.get()));
#endif
    }

#if ENABLE(2022_GLIB_API)
    void close()
    {
        auto* manager = webkit_network_session_get_website_data_manager(m_networkSession.get());
        webkit_website_data_manager_set_favicons_enabled(manager, FALSE);
        g_assert_null(webkit_website_data_manager_get_favicon_database(manager));
    }
#endif

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
#if USE(GTK4)
        test->m_favicon = webkit_web_view_get_favicon(webView);
#else
        test->m_favicon = cairo_surface_reference(webkit_web_view_get_favicon(webView));
#endif
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
#if USE(GTK4)
        test->m_favicon = adoptGRef(webkit_favicon_database_get_favicon_finish(test->m_database.get(), result, &test->m_error.outPtr()));
#else
        test->m_favicon = webkit_favicon_database_get_favicon_finish(test->m_database.get(), result, &test->m_error.outPtr());
#endif
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
#if !USE(GTK4)
        if (m_favicon)
            cairo_surface_destroy(m_favicon);
#endif
        m_favicon = nullptr;
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
#if !USE(GTK4)
        if (m_favicon)
            cairo_surface_destroy(m_favicon);
#endif
        m_favicon = nullptr;

        webkit_favicon_database_get_favicon(m_database.get(), pageURI, 0, getFaviconCallback, this);
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

    GRefPtr<WebKitFaviconDatabase> m_database;
#if USE(GTK4)
    GRefPtr<GdkTexture> m_favicon;
#else
    cairo_surface_t* m_favicon { nullptr };
#endif
    CString m_faviconURI;
    GUniqueOutPtr<GError> m_error;
    bool m_faviconNotificationReceived { false };
    bool m_loadFinished { false };
    bool m_waitingForFaviconURI { false };
};

#if USE(SOUP2)
static void serverCallback(SoupServer*, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable* query, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/favicon.ico")) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
        soup_message_body_complete(responseBody);
        return;
    }

    char* contents;
    gsize length;
    if (g_str_equal(path, "/icon/favicon.ico")) {
        GUniquePtr<char> pathToFavicon(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
        g_file_get_contents(pathToFavicon.get(), &contents, &length, 0);
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/nofavicon")) {
        static const char* noFaviconHTML = "<html><head><body>test</body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, noFaviconHTML, strlen(noFaviconHTML));
    } else {
        static const char* contentsHTML = "<html><head><link rel='icon' href='/icon/favicon.ico' type='image/x-ico; charset=binary'></head><body>test</body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, contentsHTML, strlen(contentsHTML));
    }

    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    soup_message_body_complete(responseBody);
}

static void testFaviconDatabaseInitialization(FaviconDatabaseTest* test, gconstpointer)
{
    // In 2022 API favicon database is nullptr until favicons are enabled.
#if !ENABLE(2022_GLIB_API)
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_null(test->m_favicon);
    g_assert_error(test->m_error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED);
#endif

    test->open("testFaviconDatabaseInitialization");

#if ENABLE(2022_GLIB_API)
    GUniquePtr<char> databaseFile(g_build_filename(Test::dataDirectory(), "icondatabase", "WebpageIcons.db", nullptr));
#else
    GUniquePtr<char> databaseFile(g_build_filename(webkit_web_context_get_favicon_database_directory(test->m_webContext.get()), "WebpageIcons.db", nullptr));
#endif
    g_assert_true(g_file_test(databaseFile.get(), G_FILE_TEST_IS_REGULAR));

#if ENABLE(2022_GLIB_API)
    test->close();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_null(test->m_favicon);
    g_assert_error(test->m_error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED);
#endif
}

static void testFaviconDatabaseGetFavicon(FaviconDatabaseTest* test, gconstpointer)
{
    test->open("testFaviconDatabaseGetFavicon");

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    CString faviconURI = kServer->getURIForPath("/icon/favicon.ico");

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_nonnull(test->m_favicon);
#if USE(GTK4)
    g_assert_cmpint(gdk_texture_get_width(test->m_favicon.get()), ==, 16);
    g_assert_cmpint(gdk_texture_get_height(test->m_favicon.get()), ==, 16);
#else
    g_assert_cmpint(cairo_image_surface_get_width(test->m_favicon), ==, 16);
    g_assert_cmpint(cairo_image_surface_get_height(test->m_favicon), ==, 16);
#endif
    g_assert_cmpstr(test->m_faviconURI.data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());

#if !USE(GTK4)
    // Check that another page with the same favicon returns the same icon.
    cairo_surface_t* favicon = cairo_surface_reference(test->m_favicon);
    test->loadURI(kServer->getURIForPath("/bar").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    // favicon changes twice, first to reset it and then when the new icon is loaded.
    if (!test->m_favicon)
        test->waitUntilFaviconChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/bar").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_true(test->m_favicon == favicon);
    g_assert_cmpstr(test->m_faviconURI.data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());
    cairo_surface_destroy(favicon);
#endif

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
    // If the session is ephemeral, the database is not created.
#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> ephemeralSession = adoptGRef(webkit_network_session_new_ephemeral());
    auto* manager = webkit_network_session_get_website_data_manager(ephemeralSession.get());
    webkit_website_data_manager_set_favicons_enabled(manager, TRUE);
    GUniquePtr<char> databaseFile(g_build_filename(Test::dataDirectory(), "icondatabase", "WebpageIcons.db", nullptr));
    g_assert_false(g_file_test(databaseFile.get(), G_FILE_TEST_EXISTS));
#else
    // If the context is ephemeral, the database is not created.
    GRefPtr<WebKitWebContext> ephemeralContext = adoptGRef(webkit_web_context_new_ephemeral());
    GUniquePtr<char> databaseDirectory(g_build_filename(Test::dataDirectory(), "testFaviconDatabaseEphemeral", nullptr));
    webkit_web_context_set_favicon_database_directory(ephemeralContext.get(), databaseDirectory.get());
    g_assert_cmpstr(databaseDirectory.get(), ==, webkit_web_context_get_favicon_database_directory(ephemeralContext.get()));
    GUniquePtr<char> databaseFile(g_build_filename(databaseDirectory.get(), "WebpageIcons.db", nullptr));
    g_assert_false(g_file_test(databaseFile.get(), G_FILE_TEST_EXISTS));
    ephemeralContext = nullptr;
#endif

    test->open("testFaviconDatabaseEphemeral");
    g_assert_true(g_file_test(databaseFile.get(), G_FILE_TEST_EXISTS));

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(test->m_database.get(), kServer->getURIForPath("/foo").data()));

    // An ephemeral web view doesn't write to the database.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", test->m_webContext.get(),
#if ENABLE(2022_GLIB_API)
        "network-session", ephemeralSession.get(),
#else
        "is-ephemeral", TRUE,
#endif
        nullptr));
    g_signal_connect(webView.get(), "notify::favicon", G_CALLBACK(ephemeralViewFaviconChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/bar").data());
    g_main_loop_run(test->m_mainLoop);

#if ENABLE(2022_GLIB_API)
    auto* database = webkit_website_data_manager_get_favicon_database(webkit_network_session_get_website_data_manager(ephemeralSession.get()));
    // Persistent session database is not modified by ephemeral web views.
    g_assert_null(webkit_favicon_database_get_favicon_uri(test->m_database.get(), kServer->getURIForPath("/bar").data()));
#else
    auto* database = test->m_database.get();
#endif
    // We get a favicon, but it's only in database memory cache.
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(database, kServer->getURIForPath("/bar").data()));

#if !ENABLE(2022_GLIB_API)
    // If the database exists, it's used in read only mode for ephemeral contexts.
    ephemeralContext = adoptGRef(webkit_web_context_new_ephemeral());
    webkit_web_context_set_favicon_database_directory(ephemeralContext.get(), databaseDirectory.get());
    auto* ephemeralDatabase = webkit_web_context_get_favicon_database(ephemeralContext.get());
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(ephemeralDatabase, kServer->getURIForPath("/foo").data()));

    // Page URL loaded in ephemeral web view is not in the database.
    g_assert_null(webkit_favicon_database_get_favicon_uri(ephemeralDatabase, kServer->getURIForPath("/bar").data()));
    ephemeralContext = nullptr;
#endif
}

void testFaviconDatabaseClear(FaviconDatabaseTest* test, gconstpointer)
{
    test->open("testFaviconDatabaseClear");

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_nonnull(webkit_favicon_database_get_favicon_uri(test->m_database.get(), kServer->getURIForPath("/foo").data()));

    webkit_favicon_database_clear(test->m_database.get());

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    g_assert_null(test->m_favicon);
    g_assert_null(webkit_favicon_database_get_favicon_uri(test->m_database.get(), kServer->getURIForPath("/foo").data()));
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
