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

#if PLATFORM(GTK) || ENABLE(2022_GLIB_API)

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
#if PLATFORM(GTK) && !USE(GTK4)
        if (m_favicon)
            cairo_surface_destroy(m_favicon);
#endif

        g_signal_handlers_disconnect_matched(m_database.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
#if ENABLE(2022_GLIB_API)
        g_clear_pointer(&m_pageIcons, webkit_image_list_unref);

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

        if (m_database)
            close();
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
#if PLATFORM(GTK)
        g_signal_connect(m_database.get(), "favicon-changed", G_CALLBACK(faviconChangedCallback), this);
#endif
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

#if PLATFORM(GTK)
    static void faviconChangedCallback(WebKitFaviconDatabase* database, const char* pageURI, const char* faviconURI, FaviconDatabaseTest* test)
    {
        if (!g_strcmp0(webkit_web_view_get_uri(test->webView()), pageURI)) {
            test->m_faviconURI = faviconURI;
            if (test->m_waitingForFaviconURI) {
                test->m_waitingForFaviconURI = false;
                test->quitMainLoop();
            }
        }
    }

    static void viewFaviconChangedCallback(WebKitWebView* webView, GParamSpec* pspec, FaviconDatabaseTest* test)
    {
        g_assert_true(test->webView() == webView);
        test->m_pageFaviconNotificationCount++;
#if USE(GTK4)
        test->m_favicon = webkit_web_view_get_favicon(webView);
#else
        test->m_favicon = cairo_surface_reference(webkit_web_view_get_favicon(webView));
#endif
        if (test->m_loadFinished)
            test->quitMainLoop();
    }
#endif // PLATFORM(GTK)

#if ENABLE(2022_GLIB_API)
    static void viewPageIconsChangedCallback(WebKitWebView* webView, GParamSpec*, FaviconDatabaseTest* test)
    {
        g_clear_pointer(&test->m_pageIcons, webkit_image_list_unref);
        g_assert_true(test->webView() == webView);
        test->m_pageIconsNoticationCount++;
        test->m_pageIcons = webkit_image_list_ref(webkit_web_view_get_page_icons(webView));
        if (test->m_loadFinished)
            test->quitMainLoop();
    }
#endif

    static void viewLoadChangedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, FaviconDatabaseTest* test)
    {
        g_assert_true(test->webView() == webView);
        if (loadEvent != WEBKIT_LOAD_FINISHED)
            return;

        test->m_loadFinished = true;
#if PLATFORM(GTK)
        const bool shouldQuitMainLoop = test->m_pageFaviconNotificationCount > 0;
#else
        const bool shouldQuitMainLoop = test->m_pageIconsNoticationCount > 0;
#endif
        if (shouldQuitMainLoop)
            test->quitMainLoop();
    }

#if PLATFORM(GTK)
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
        m_pageFaviconNotificationCount = 0;
        unsigned long handlerID = g_signal_connect(m_webView.get(), "notify::favicon", G_CALLBACK(viewFaviconChangedCallback), this);
        g_main_loop_run(m_mainLoop);
        g_signal_handler_disconnect(m_webView.get(), handlerID);
    }

    void waitUntilLoadFinishedAndFaviconChanged()
    {
#if !USE(GTK4)
        if (m_favicon)
            cairo_surface_destroy(m_favicon);
#endif
        m_favicon = nullptr;
        m_pageFaviconNotificationCount = 0;
        m_loadFinished = false;
        unsigned long faviconChangedID = g_signal_connect(m_webView.get(), "notify::favicon", G_CALLBACK(viewFaviconChangedCallback), this);
        unsigned long loadChangedID = g_signal_connect(m_webView.get(), "load-changed", G_CALLBACK(viewLoadChangedCallback), this);
        g_main_loop_run(m_mainLoop);
        g_signal_handler_disconnect(m_webView.get(), faviconChangedID);
        g_signal_handler_disconnect(m_webView.get(), loadChangedID);
    }

    static void faviconChangedCountCallback(WebKitWebView* webView, GParamSpec*, FaviconDatabaseTest* test)
    {
        g_assert_true(test->webView() == webView);
        test->m_pageFaviconNotificationCount++;
    }
#endif // PLATFORM(GTK)

#if ENABLE(2022_GLIB_API)
    void waitUntilLoadFinishedAndPageIconsChanged()
    {
        g_clear_pointer(&m_pageIcons, webkit_image_list_unref);
        m_pageIconsNoticationCount = 0;
        m_loadFinished = false;

#if PLATFORM(GTK)
        m_pageFaviconNotificationCount = 0;
        const auto faviconID = g_signal_connect(m_webView.get(), "notify::favicon", G_CALLBACK(faviconChangedCountCallback), this);
#endif

        const auto pageIconsID = g_signal_connect(m_webView.get(), "notify::page-icons", G_CALLBACK(viewPageIconsChangedCallback), this);
        const auto loadChangedID = g_signal_connect(m_webView.get(), "load-changed", G_CALLBACK(viewLoadChangedCallback), this);
        g_main_loop_run(m_mainLoop);

#if PLATFORM(GTK)
        g_signal_handler_disconnect(m_webView.get(), faviconID);
#endif

        g_signal_handler_disconnect(m_webView.get(), pageIconsID);
        g_signal_handler_disconnect(m_webView.get(), loadChangedID);
    }
#endif // ENABLE(2022_GLIB_API)

#if PLATFORM(GTK)
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

    void willWaitForFaviconURIChanged()
    {
        m_waitingForFaviconURI = false;
        m_faviconURI.reset();
    }

    void waitUntilFaviconURIChangedIfNeeded(unsigned timeoutMilliseconds = 0)
    {
        g_assert_false(m_waitingForFaviconURI);

        if (m_faviconURI)
            g_assert_false(m_waitingForFaviconURI);
        else {
            m_waitingForFaviconURI = true;
            if (timeoutMilliseconds) {
                g_assert_cmpint(m_waitingForFaviconURITimeoutID, ==, 0);
                m_waitingForFaviconURITimeoutID = g_timeout_add(timeoutMilliseconds, [](void* data) -> gboolean {
                    auto* test = static_cast<FaviconDatabaseTest*>(data);
                    test->m_waitingForFaviconURITimeoutID = 0;
                    test->quitMainLoop();
                    return G_SOURCE_REMOVE;
                }, this);
            }
            g_main_loop_run(m_mainLoop);
            g_clear_handle_id(&m_waitingForFaviconURITimeoutID, g_source_remove);
        }

        if (!timeoutMilliseconds) {
            g_assert_false(m_waitingForFaviconURI);
            g_assert_true(m_faviconURI.has_value());
        }
    }

    void waitUntilFaviconChangedTimes(unsigned numTimes, unsigned timeoutMilliseconds = 0)
    {
        if (m_pageFaviconNotificationCount >= numTimes)
            return;

        GTimer* timer = g_timer_new();
        auto handlerID = g_signal_connect(m_webView.get(), "notify::favicon", G_CALLBACK(faviconChangedCountCallback), this);
        while (m_pageFaviconNotificationCount < numTimes && g_timer_elapsed(timer, nullptr) * 1000.0 <= timeoutMilliseconds) {
            g_main_context_iteration(g_main_loop_get_context(m_mainLoop), FALSE);
            g_usleep(10000);
        }
        g_clear_pointer(&timer, g_timer_destroy);
        g_signal_handler_disconnect(m_webView.get(), handlerID);
    }

    unsigned m_faviconChangedID { 0 };
    unsigned m_pageFaviconNotificationTimeoutID { 0 };
    size_t m_pageFaviconNotificationCount { 0 };
    std::optional<CString> m_faviconURI { std::nullopt };
    GUniqueOutPtr<GError> m_error;
    bool m_waitingForFaviconURI { false };
    unsigned m_waitingForFaviconURITimeoutID { 0 };

#if USE(GTK4)
    GRefPtr<GdkTexture> m_favicon;
#else
    cairo_surface_t* m_favicon { nullptr };
#endif
#endif // PLATFORM(GTK)

#if ENABLE(2022_GLIB_API)
    size_t m_pageIconsNoticationCount { 0 };
    WebKitImageList* m_pageIcons;
#endif

    GRefPtr<WebKitFaviconDatabase> m_database;
    bool m_loadFinished { false };
};

static void serverCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable* query, gpointer)
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
    } else if (g_str_equal(path, "/icon/red-32x32.png")) {
        GUniquePtr<char> pathToFavicon(g_build_filename(Test::getResourcesDir().data(), "red-32x32.png", nullptr));
        g_file_get_contents(pathToFavicon.get(), &contents, &length, nullptr);
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/icon/blue-48x48.png")) {
        GUniquePtr<char> pathToFavicon(g_build_filename(Test::getResourcesDir().data(), "blue-48x48.png", nullptr));
        g_file_get_contents(pathToFavicon.get(), &contents, &length, nullptr);
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/nofavicon")) {
        static const char* noFaviconHTML = "<html><head><body>test</body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, noFaviconHTML, strlen(noFaviconHTML));
    } else if (g_str_equal(path, "/multipleicons")) {
        static constexpr ASCIILiteral multipleIconsHTML = "<html><head>"
            "<link rel='icon' type='image/x-ico' href='/icon/favicon.ico'>"
            "<link rel='icon' type='image/png' href='/icon/red-32x32.png' sizes='32x32'>"
            "<link rel='icon' type='image/png' href='/icon/blue-48x48.png' sizes='48x48'>"
            "</head><body>test</body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, multipleIconsHTML, multipleIconsHTML.length());
    } else {
        static const char* contentsHTML = "<html><head><link rel='icon' href='/icon/favicon.ico' type='image/x-ico; charset=binary'></head><body>test</body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, contentsHTML, strlen(contentsHTML));
    }

    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    soup_message_body_complete(responseBody);
}

#if PLATFORM(GTK)
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

    test->willWaitForFaviconURIChanged();

    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    CString faviconURI = kServer->getURIForPath("/icon/favicon.ico");

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/foo").data());
    test->waitUntilFaviconURIChangedIfNeeded();

    g_assert_nonnull(test->m_favicon);
#if USE(GTK4)
    g_assert_cmpint(gdk_texture_get_width(test->m_favicon.get()), ==, 16);
    g_assert_cmpint(gdk_texture_get_height(test->m_favicon.get()), ==, 16);
#else
    g_assert_cmpint(cairo_image_surface_get_width(test->m_favicon), ==, 16);
    g_assert_cmpint(cairo_image_surface_get_height(test->m_favicon), ==, 16);
#endif
    g_assert_true(test->m_faviconURI.has_value());
    g_assert_cmpstr(test->m_faviconURI->data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());

#if !USE(GTK4)
    // Check that another page with the same favicon returns the same icon.
    cairo_surface_t* favicon = cairo_surface_reference(test->m_favicon);
    test->willWaitForFaviconURIChanged();
    test->loadURI(kServer->getURIForPath("/bar").data());
    test->waitUntilLoadFinishedAndFaviconChanged();
    test->waitUntilFaviconURIChangedIfNeeded();
    // favicon changes twice, first to reset it and then when the new icon is loaded.
    if (!test->m_favicon)
        test->waitUntilFaviconChanged();
    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/bar").data());
    g_assert_nonnull(test->m_favicon);
    g_assert_true(test->m_favicon == favicon);
    g_assert_true(test->m_faviconURI.has_value());
    g_assert_cmpstr(test->m_faviconURI->data(), ==, faviconURI.data());
    g_assert_no_error(test->m_error.get());
    cairo_surface_destroy(favicon);
#endif

    test->willWaitForFaviconURIChanged();
    faviconURI = kServer->getURIForPath("/favicon.ico");
    test->loadURI(kServer->getURIForPath("/nofavicon").data());
    test->waitUntilLoadFinishedAndFaviconChanged();

    // Note that /favicon.icon results in HTTP 404 Not Found, and when an icon
    // is not loaded, there will be no WebKitFaviconDatabase:favicon-changed signal.
    test->waitUntilFaviconURIChangedIfNeeded(1000);
    // Still waiting for the icon that was never fetched...
    g_assert_true(test->m_waitingForFaviconURI);

    test->getFaviconForPageURIAndWaitUntilReady(kServer->getURIForPath("/nofavicon").data());
    g_assert_null(test->m_favicon);
    g_assert_false(test->m_faviconURI.has_value());
    g_assert_error(test->m_error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN);

    // Loading an icon that is already in the database should emit
    // WebKitWebView::notify::favicon, but not WebKitFaviconDatabase::icon-changed.
    g_assert_null(webkit_web_view_get_favicon(test->webView()));
    test->m_faviconURI.reset();
    test->loadURI(kServer->getURIForPath("/foo").data());
    test->waitUntilFaviconChanged();
    g_assert_false(test->m_faviconURI.has_value());
    g_assert_nonnull(webkit_web_view_get_favicon(test->webView()));
}
#endif // PLATFORM(GTK)

#if ENABLE(2022_GLIB_API)
static void testFaviconDatabaseGetPageIcons(FaviconDatabaseTest *test, gconstpointer)
{
    test->open("testFaviconDatabaseGetPageIcons");

    test->loadURI(kServer->getURIForPath("/multipleicons").data());
    test->waitUntilLoadFinishedAndPageIconsChanged();

#if PLATFORM(GTK)
    test->waitUntilFaviconChangedTimes(3, 1500);
    g_assert_cmpint(test->m_pageFaviconNotificationCount, ==, 3);
#endif

    g_assert_cmpint(test->m_pageIconsNoticationCount, ==, 1);
    g_assert_nonnull(test->m_pageIcons);
    g_assert_cmpint(webkit_image_list_get_length(test->m_pageIcons), ==, 3);

    static const auto getIconWithSize = [](WebKitImageList* imageList, int size) -> WebKitImage* {
        for (unsigned i = 0; i < webkit_image_list_get_length(imageList); ++i) {
            auto* image = webkit_image_list_get(imageList, i);
            if (webkit_image_get_width(image) == size && webkit_image_get_height(image) == size)
                return image;
        }
        return nullptr;
    };

    static const auto getARGBColorAtOrigin = [](WebKitImage* image) -> std::optional<uint32_t> {
        auto* imageBytes = webkit_image_as_bytes(image);

        size_t imageDataSize = 0;
        auto* imageData = reinterpret_cast<const uint32_t*>(g_bytes_get_data(imageBytes, &imageDataSize));
        if (imageDataSize >= sizeof(uint32_t))
            return *imageData;

        return std::nullopt;
    };

    auto* whiteIcon = getIconWithSize(test->m_pageIcons, 16);
    g_assert_nonnull(whiteIcon);
    auto whiteIconPixel = getARGBColorAtOrigin(whiteIcon);
    g_assert_true(whiteIconPixel.has_value());
    g_assert_cmpint(*whiteIconPixel, ==, 0xFFFFFFFF);

    auto* redIcon = getIconWithSize(test->m_pageIcons, 32);
    g_assert_nonnull(redIcon);
    auto redIconPixel = getARGBColorAtOrigin(redIcon);
    g_assert_true(redIconPixel.has_value());
    g_assert_cmpint(*redIconPixel, ==, 0xFFFF0000);

    auto* blueIcon = getIconWithSize(test->m_pageIcons, 48);
    g_assert_nonnull(blueIcon);
    auto blueIconPixel = getARGBColorAtOrigin(blueIcon);
    g_assert_true(blueIconPixel.has_value());
    g_assert_cmpint(*blueIconPixel, ==, 0xFF0000FF);
}
#endif // ENABLE(2022_GLIB_API)

#if PLATFORM(GTK)
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
    auto webView = test->createWebView(
#if ENABLE(2022_GLIB_API)
        "network-session", ephemeralSession.get(),
#else
        "is-ephemeral", TRUE,
#endif
        nullptr);
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
#endif // PLATFORM(GTK)


void beforeAll()
{
    // Start a soup server for testing.
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

#if PLATFORM(GTK)
    // FIXME(311208): Enable these test cases for the WPE port.
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "initialization", testFaviconDatabaseInitialization);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "get-favicon", testFaviconDatabaseGetFavicon);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "ephemeral", testFaviconDatabaseEphemeral);
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "clear", testFaviconDatabaseClear);
#endif
#if ENABLE(2022_GLIB_API)
    FaviconDatabaseTest::add("WebKitFaviconDatabase", "get-page-icons", testFaviconDatabaseGetPageIcons);
#endif
}

void afterAll()
{
    delete kServer;
}

#endif
