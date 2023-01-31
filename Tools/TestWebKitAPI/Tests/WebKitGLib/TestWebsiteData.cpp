/*
 * Copyright (C) 2017 Igalia S.L.
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
#include <WebCore/GUniquePtrSoup.h>
#include <WebCore/SoupVersioning.h>
#include <glib/gstdio.h>

static WebKitTestServer* kServer;

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/")) {
        const char* html = "<html><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, html, strlen(html));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/empty")) {
        auto* responseHeaders = soup_server_message_get_response_headers(message);
        soup_message_headers_replace(responseHeaders, "Set-Cookie", "foo=bar; Max-Age=60");
        soup_message_headers_replace(responseHeaders, "Strict-Transport-Security", "max-age=3600");
        static const char* emptyHTML = "<html><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, emptyHTML, strlen(emptyHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/appcache")) {
        static const char* appcacheHTML = "<html manifest=appcache.manifest><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, appcacheHTML, strlen(appcacheHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/appcache.manifest")) {
        static const char* appcacheManifest = "CACHE MANIFEST\nCACHE:\nappcache/foo.txt\n";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, appcacheManifest, strlen(appcacheManifest));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/appcache/foo.txt")) {
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, "foo", 3);
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/sessionstorage")) {
        static const char* sessionStorageHTML = "<html><body onload=\"sessionStorage.foo = 'bar';\"></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, sessionStorageHTML, strlen(sessionStorageHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/localstorage")) {
        static const char* localStorageHTML = "<html><body onload=\"localStorage.foo = 'bar';\"></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, localStorageHTML, strlen(localStorageHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/enumeratedevices")) {
        static const char* enumerateDevicesHTML = "<html><body onload=\"navigator.mediaDevices.enumerateDevices().then(function(devices) { document.title = 'Finished'; })\"></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, enumerateDevicesHTML, strlen(enumerateDevicesHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/service/register.html")) {
        static const char* swRegisterHTML = "<html><body><script src=\"/service/register.js\"></script></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, swRegisterHTML, strlen(swRegisterHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/service/register.js")) {
        static const char* swRegisterJS =
            "async function test() { await navigator.serviceWorker.register(\"/service/empty-worker.js\"); } test();";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, swRegisterJS, strlen(swRegisterJS));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/service/empty-worker.js")) {
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

class WebsiteDataTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebsiteDataTest);

    WebsiteDataTest()
#if ENABLE(2022_GLIB_API)
        : m_manager(webkit_network_session_get_website_data_manager(webkit_web_view_get_network_session(m_webView)))
#else
        : m_manager(webkit_web_context_get_website_data_manager(webkit_web_view_get_context(m_webView)))
#endif
    {
        g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(m_manager));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_manager));
        // WebsiteDataStore creates a new WebProcessPool when used before any secondary process has been created.
        // Ensure we have a web process by always loading about:blank here.
        loadURI("about:blank");
        waitUntilLoadFinished();
    }

    ~WebsiteDataTest()
    {
        g_list_free_full(m_dataList, reinterpret_cast<GDestroyNotify>(webkit_website_data_unref));
    }

    GList* fetch(WebKitWebsiteDataTypes types)
    {
        if (m_dataList) {
            g_list_free_full(m_dataList, reinterpret_cast<GDestroyNotify>(webkit_website_data_unref));
            m_dataList = nullptr;
        }
        webkit_website_data_manager_fetch(m_manager, types, nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
            WebsiteDataTest* test = static_cast<WebsiteDataTest*>(userData);
            test->m_dataList = webkit_website_data_manager_fetch_finish(test->m_manager, result, nullptr);
            test->quitMainLoop();
        }, this);
        g_main_loop_run(m_mainLoop);
        return m_dataList;
    }

    void remove(WebKitWebsiteDataTypes types, GList* dataList)
    {
        webkit_website_data_manager_remove(m_manager, types, dataList, nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
            WebsiteDataTest* test = static_cast<WebsiteDataTest*>(userData);
            g_assert_true(webkit_website_data_manager_remove_finish(test->m_manager, result, nullptr));
            test->quitMainLoop();
        }, this);
        g_main_loop_run(m_mainLoop);
    }

    void clear(WebKitWebsiteDataTypes types, GTimeSpan timeSpan)
    {
        webkit_website_data_manager_clear(m_manager, types, timeSpan, nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
            WebsiteDataTest* test = static_cast<WebsiteDataTest*>(userData);
            g_assert_true(webkit_website_data_manager_clear_finish(test->m_manager, result, nullptr));
            test->quitMainLoop();
        }, this);
        g_main_loop_run(m_mainLoop);
    }

    WebKitWebsiteDataManager* m_manager;
    GList* m_dataList { nullptr };
};

static void testWebsiteDataConfiguration(WebsiteDataTest* test, gconstpointer)
{
#if !ENABLE(2022_GLIB_API)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#endif

    g_assert_cmpstr(webkit_website_data_manager_get_base_data_directory(test->m_manager), ==, Test::dataDirectory());
    g_assert_cmpstr(webkit_website_data_manager_get_base_cache_directory(test->m_manager), ==, Test::dataDirectory());

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();
    test->runJavaScriptAndWaitUntilFinished("window.localStorage.myproperty = 42;", nullptr);
    GUniquePtr<char> localStorageDirectory(g_build_filename(Test::dataDirectory(), "localstorage", nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(localStorageDirectory.get(), ==, webkit_website_data_manager_get_local_storage_directory(test->m_manager));
#endif
    test->assertFileIsCreated(localStorageDirectory.get());
    g_assert_true(g_file_test(localStorageDirectory.get(), G_FILE_TEST_IS_DIR));
    test->runJavaScriptAndWaitUntilFinished("window.localStorage.clear();", nullptr);

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();
    test->runJavaScriptAndWaitUntilFinished("window.indexedDB.open('TestDatabase');", nullptr);
    GUniquePtr<char> indexedDBDirectory(g_build_filename(Test::dataDirectory(), "databases", "indexeddb", nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(indexedDBDirectory.get(), ==, webkit_website_data_manager_get_indexeddb_directory(test->m_manager));
#endif
    test->assertFileIsCreated(indexedDBDirectory.get());
    g_assert_true(g_file_test(indexedDBDirectory.get(), G_FILE_TEST_IS_DIR));

    test->loadURI(kServer->getURIForPath("/appcache").data());
    test->waitUntilLoadFinished();
    GUniquePtr<char> applicationCacheDirectory(g_build_filename(Test::dataDirectory(), "applications", nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(applicationCacheDirectory.get(), ==, webkit_website_data_manager_get_offline_application_cache_directory(test->m_manager));
#endif
    GUniquePtr<char> applicationCacheDatabase(g_build_filename(applicationCacheDirectory.get(), "ApplicationCache.db", nullptr));
    test->assertFileIsCreated(applicationCacheDatabase.get());

    GUniquePtr<char> diskCacheDirectory(g_build_filename(Test::dataDirectory(), nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(diskCacheDirectory.get(), ==, webkit_website_data_manager_get_disk_cache_directory(test->m_manager));
#endif
    g_assert_true(g_file_test(diskCacheDirectory.get(), G_FILE_TEST_IS_DIR));

    GUniquePtr<char> hstsCacheDirectory(g_build_filename(Test::dataDirectory(), nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(hstsCacheDirectory.get(), ==, webkit_website_data_manager_get_hsts_cache_directory(test->m_manager));
#endif
    g_assert_true(g_file_test(hstsCacheDirectory.get(), G_FILE_TEST_IS_DIR));

#if !ENABLE(2022_GLIB_API)
    GUniquePtr<char> itpDirectory(g_build_filename(Test::dataDirectory(), "itp", nullptr));
    g_assert_cmpstr(itpDirectory.get(), ==, webkit_website_data_manager_get_itp_directory(test->m_manager));
#endif

    test->runJavaScriptAndWaitUntilFinished("navigator.serviceWorker.register('./some-dummy.js');", nullptr);
    GUniquePtr<char> swRegistrationsDirectory(g_build_filename(Test::dataDirectory(), "serviceworkers", nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(swRegistrationsDirectory.get(), ==, webkit_website_data_manager_get_service_worker_registrations_directory(test->m_manager));
#endif
    test->assertFileIsCreated(swRegistrationsDirectory.get());
    g_assert_true(g_file_test(swRegistrationsDirectory.get(), G_FILE_TEST_IS_DIR));

    test->runJavaScriptAndWaitUntilFinished("let domCacheOpened = false; caches.open('my-cache').then(() => { domCacheOpened = true});", nullptr);
    GUniquePtr<char> domCacheDirectory(g_build_filename(Test::dataDirectory(), "CacheStorage", nullptr));
#if !ENABLE(2022_GLIB_API)
    g_assert_cmpstr(domCacheDirectory.get(), ==, webkit_website_data_manager_get_dom_cache_directory(test->m_manager));
#endif
    test->assertFileIsCreated(domCacheDirectory.get());
    g_assert_true(g_file_test(domCacheDirectory.get(), G_FILE_TEST_IS_DIR));

    // Make sure the cache was opened before asking to clear it to avoid it being re-created behind our backs.
    test->assertJavaScriptBecomesTrue("domCacheOpened");

    // Clear all persistent caches, since the data dir is common to all test cases. Note: not cleaning the HSTS cache here as its data
    // is needed for the HSTS tests, where data cleaning will be tested.
    static const WebKitWebsiteDataTypes persistentCaches = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_DISK_CACHE | WEBKIT_WEBSITE_DATA_LOCAL_STORAGE
        | WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES | WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE | WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT | WEBKIT_WEBSITE_DATA_ITP
        | WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS | WEBKIT_WEBSITE_DATA_DOM_CACHE);
    test->clear(persistentCaches, 0);
    g_assert_null(test->fetch(persistentCaches));

    // The default context or session should have a different manager with different configuration.
#if ENABLE(2022_GLIB_API)
    auto* defaultManager = webkit_network_session_get_website_data_manager(webkit_network_session_get_default());
#else
    auto* defaultManager = webkit_web_context_get_website_data_manager(webkit_web_context_get_default());
#endif
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(defaultManager));
    g_assert_true(test->m_manager != defaultManager);
    g_assert_cmpstr(webkit_website_data_manager_get_base_data_directory(test->m_manager), !=, webkit_website_data_manager_get_base_data_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_base_cache_directory(test->m_manager), !=, webkit_website_data_manager_get_base_cache_directory(defaultManager));
#if ENABLE(2022_GLIB_API)
    g_assert_nonnull(webkit_website_data_manager_get_base_data_directory(defaultManager));
    g_assert_nonnull(webkit_website_data_manager_get_base_cache_directory(defaultManager));
#else
    g_assert_null(webkit_website_data_manager_get_base_data_directory(defaultManager));
    g_assert_null(webkit_website_data_manager_get_base_cache_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_local_storage_directory(test->m_manager), !=, webkit_website_data_manager_get_local_storage_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_indexeddb_directory(test->m_manager), !=, webkit_website_data_manager_get_indexeddb_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_disk_cache_directory(test->m_manager), !=, webkit_website_data_manager_get_disk_cache_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_offline_application_cache_directory(test->m_manager), !=, webkit_website_data_manager_get_offline_application_cache_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_hsts_cache_directory(test->m_manager), !=, webkit_website_data_manager_get_hsts_cache_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_itp_directory(test->m_manager), !=, webkit_website_data_manager_get_itp_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_service_worker_registrations_directory(test->m_manager), !=, webkit_website_data_manager_get_service_worker_registrations_directory(defaultManager));
    g_assert_cmpstr(webkit_website_data_manager_get_dom_cache_directory(test->m_manager), !=, webkit_website_data_manager_get_dom_cache_directory(defaultManager));
#endif

#if !ENABLE(2022_GLIB_API)
    // Any specific configuration provided takes precedence over base dirs.
    indexedDBDirectory.reset(g_build_filename(Test::dataDirectory(), "mycustomindexeddb", nullptr));
    applicationCacheDirectory.reset(g_build_filename(Test::dataDirectory(), "mycustomappcache", nullptr));
    GRefPtr<WebKitWebsiteDataManager> baseDataManager = adoptGRef(webkit_website_data_manager_new(
        "base-data-directory", Test::dataDirectory(), "base-cache-directory", Test::dataDirectory(),
        "indexeddb-directory", indexedDBDirectory.get(), "offline-application-cache-directory", applicationCacheDirectory.get(),
        nullptr));
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(baseDataManager.get()));

    g_assert_cmpstr(webkit_website_data_manager_get_indexeddb_directory(baseDataManager.get()), ==, indexedDBDirectory.get());
    g_assert_cmpstr(webkit_website_data_manager_get_offline_application_cache_directory(baseDataManager.get()), ==, applicationCacheDirectory.get());
    // The result should be the same as previous manager.
    g_assert_cmpstr(webkit_website_data_manager_get_local_storage_directory(baseDataManager.get()), ==, localStorageDirectory.get());
    g_assert_cmpstr(webkit_website_data_manager_get_itp_directory(baseDataManager.get()), ==, itpDirectory.get());
    g_assert_cmpstr(webkit_website_data_manager_get_service_worker_registrations_directory(baseDataManager.get()), ==, swRegistrationsDirectory.get());
    g_assert_cmpstr(webkit_website_data_manager_get_dom_cache_directory(baseDataManager.get()), ==, domCacheDirectory.get());
    g_assert_cmpstr(webkit_website_data_manager_get_disk_cache_directory(baseDataManager.get()), ==, diskCacheDirectory.get());

    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

static void ephemeralViewloadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewloadChanged), test);
    test->quitMainLoop();
}

static void testWebsiteDataEphemeral(WebViewTest* test, gconstpointer)
{
#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> session = adoptGRef(webkit_network_session_new_ephemeral());
    GRefPtr<WebKitWebsiteDataManager> manager = webkit_network_session_get_website_data_manager(session.get());
#else
    GRefPtr<WebKitWebsiteDataManager> manager = adoptGRef(webkit_website_data_manager_new_ephemeral());
    g_assert_true(webkit_website_data_manager_is_ephemeral(manager.get()));
#endif
    g_assert_null(webkit_website_data_manager_get_base_data_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_base_cache_directory(manager.get()));

#if !ENABLE(2022_GLIB_API)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    g_assert_null(webkit_website_data_manager_get_local_storage_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_disk_cache_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_offline_application_cache_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_indexeddb_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_hsts_cache_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_itp_directory(manager.get()));
    g_assert_null(webkit_website_data_manager_get_service_worker_registrations_directory(manager.get()));
    ALLOW_DEPRECATED_DECLARATIONS_END

    // Configuration is ignored when is-ephemeral is used.
    manager = adoptGRef(WEBKIT_WEBSITE_DATA_MANAGER(g_object_new(WEBKIT_TYPE_WEBSITE_DATA_MANAGER, "base-data-directory", Test::dataDirectory(), "is-ephemeral", TRUE, nullptr)));
    g_assert_true(webkit_website_data_manager_is_ephemeral(manager.get()));
    g_assert_null(webkit_website_data_manager_get_base_data_directory(manager.get()));
#endif

#if ENABLE(2022_GLIB_API)
    webkit_network_session_set_itp_enabled(session.get(), TRUE);
    g_assert_true(webkit_network_session_get_itp_enabled(session.get()));
#else
    webkit_website_data_manager_set_itp_enabled(manager.get(), TRUE);
    g_assert_true(webkit_website_data_manager_get_itp_enabled(manager.get()));
#endif

    // Non persistent data can be queried in an ephemeral manager.
#if ENABLE(2022_GLIB_API)
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(test->m_webView),
        "network-session", session.get(),
        nullptr));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == session.get());
#else
    GRefPtr<WebKitWebContext> webContext = adoptGRef(webkit_web_context_new_with_website_data_manager(manager.get()));
    g_assert_true(webkit_web_context_is_ephemeral(webContext.get()));
    auto webView = Test::adoptView(Test::createWebView(webContext.get()));
    g_assert_true(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) == manager.get());
#endif

    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(ephemeralViewloadChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/empty").data());
    g_main_loop_run(test->m_mainLoop);

    GUniquePtr<char> itpDirectory(g_build_filename(Test::dataDirectory(), "itp", nullptr));
    g_assert(!g_file_test(itpDirectory.get(), G_FILE_TEST_IS_DIR));

    webkit_website_data_manager_fetch(manager.get(), WEBKIT_WEBSITE_DATA_MEMORY_CACHE, nullptr, [](GObject* manager, GAsyncResult* result, gpointer userData) {
        auto* test = static_cast<WebViewTest*>(userData);
        GList* dataList = webkit_website_data_manager_fetch_finish(WEBKIT_WEBSITE_DATA_MANAGER(manager), result, nullptr);
        g_assert_nonnull(dataList);
        g_assert_cmpuint(g_list_length(dataList), ==, 1);
        WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
        g_assert_nonnull(data);
        WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
        g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
        webkit_security_origin_unref(origin);
        g_list_free_full(dataList, reinterpret_cast<GDestroyNotify>(webkit_website_data_unref));
        test->quitMainLoop();
    }, test);
    g_main_loop_run(test->m_mainLoop);

#if ENABLE(2022_GLIB_API)
    webkit_network_session_set_itp_enabled(session.get(), FALSE);
#else
    webkit_website_data_manager_set_itp_enabled(manager.get(), FALSE);
#endif
}

static void testWebsiteDataCache(WebsiteDataTest* test, gconstpointer)
{
    static const WebKitWebsiteDataTypes cacheTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    GList* dataList = test->fetch(cacheTypes);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();

    // Disk cache delays the storing of initial resources for 1 second to avoid
    // affecting early page load. So, wait 1 second here to make sure resources
    // have already been stored.
    test->wait(1);

    dataList = test->fetch(cacheTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, cacheTypes);
    // Memory cache size is unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_MEMORY_CACHE), ==, 0);
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_DISK_CACHE), >, 0);

    // Try again but only getting disk cache.
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DISK_CACHE);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    g_assert_true(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_DISK_CACHE);
    g_assert_false(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_MEMORY_CACHE);

    GUniquePtr<char> fileURL(g_strdup_printf("file://%s/simple.html", Test::getResourcesDir(Test::WebKit2Resources).data()));
    test->loadURI(fileURL.get());
    test->waitUntilLoadFinished();

    fileURL.reset(g_strdup_printf("file://%s/simple2.html", Test::getResourcesDir(Test::WebKit2Resources).data()));
    test->loadURI(fileURL.get());
    test->waitUntilLoadFinished();

    // Local files are grouped.
    dataList = test->fetch(cacheTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 2);
    GList* itemList = g_list_find_custom(dataList, nullptr, [](gconstpointer item, gconstpointer) -> int {
        WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(const_cast<gpointer>(item));
        return g_strcmp0(webkit_website_data_get_name(data), "Local files");
    });
    g_assert_nonnull(itemList);
    data = static_cast<WebKitWebsiteData*>(itemList->data);
    g_assert_nonnull(data);
    g_assert_true(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_MEMORY_CACHE);
    // Local files are never stored in disk cache.
    g_assert_false(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_DISK_CACHE);

    // Clear data modified since the last microsecond should not clear anything.
    // Use disk-cache because memory cache ignores the modified since.
    test->clear(WEBKIT_WEBSITE_DATA_DISK_CACHE, 1);
    dataList = test->fetch(cacheTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 2);

    // Remove memory cache only for local files.
    itemList = g_list_find_custom(dataList, nullptr, [](gconstpointer item, gconstpointer) -> int {
        WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(const_cast<gpointer>(item));
        return g_strcmp0(webkit_website_data_get_name(data), "Local files");
    });
    g_assert_nonnull(itemList);
    GList removeList = { itemList->data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_MEMORY_CACHE, &removeList);
    dataList = test->fetch(cacheTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_true(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_DISK_CACHE);

    // Clear all.
    test->clear(cacheTypes, 0);
    dataList = test->fetch(cacheTypes);
    g_assert_null(dataList);
}

static void testWebsiteDataStorage(WebsiteDataTest* test, gconstpointer)
{
    static const WebKitWebsiteDataTypes storageTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_SESSION_STORAGE | WEBKIT_WEBSITE_DATA_LOCAL_STORAGE);
    GList* dataList = test->fetch(storageTypes);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/sessionstorage").data());
    test->waitUntilLoadFinished();

    test->loadURI(kServer->getURIForPath("/localstorage").data());
    test->waitUntilLoadFinished();

    // Local storage uses a 1 second timer to update the database.
    test->wait(1);

    dataList = test->fetch(storageTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, storageTypes);
    // Storage sizes are unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_SESSION_STORAGE), ==, 0);
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_LOCAL_STORAGE), ==, 0);

    // Get also cached data, and clear it.
    static const WebKitWebsiteDataTypes cacheAndStorageTypes = static_cast<WebKitWebsiteDataTypes>(storageTypes | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    dataList = test->fetch(cacheAndStorageTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, cacheAndStorageTypes);
    test->clear(static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE), 0);

    // Get all types again, but only storage is retrieved now.
    dataList = test->fetch(cacheAndStorageTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, storageTypes);

    // Remove the session storage.
    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_SESSION_STORAGE, &removeList);
    dataList = test->fetch(storageTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_false(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_SESSION_STORAGE);
    g_assert_true(webkit_website_data_get_types(data) & WEBKIT_WEBSITE_DATA_LOCAL_STORAGE);

    // Clear all.
    test->clear(cacheAndStorageTypes, 0);
    dataList = test->fetch(cacheAndStorageTypes);
    g_assert_null(dataList);
}

static void testWebsiteDataDatabases(WebsiteDataTest* test, gconstpointer)
{
    static const WebKitWebsiteDataTypes databaseTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES);
    GList* dataList = test->fetch(databaseTypes);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();
    test->runJavaScriptAndWaitUntilFinished("window.indexedDB.open('TestDatabase');", nullptr);

    test->wait(1);

    dataList = test->fetch(databaseTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES);
    // Database sizes are unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES), ==, 0);

    test->runJavaScriptAndWaitUntilFinished("db = openDatabase(\"TestDatabase\", \"1.0\", \"TestDatabase\", 1);", nullptr);
    dataList = test->fetch(databaseTypes);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, databaseTypes);
    // Database sizes are unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES), ==, 0);

    // Remove all databases at once.
    GList removeList = { data, nullptr, nullptr };
    test->remove(databaseTypes, &removeList);
    dataList = test->fetch(databaseTypes);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndDatabaseTypes = static_cast<WebKitWebsiteDataTypes>(databaseTypes | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(cacheAndDatabaseTypes, 0);
    dataList = test->fetch(cacheAndDatabaseTypes);
    g_assert_null(dataList);
}

static void testWebsiteDataAppcache(WebsiteDataTest* test, gconstpointer)
{
    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/appcache").data());
    test->waitUntilLoadFinished();

    test->wait(1);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE);
    // Appcache size is unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE), ==, 0);

    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndAppcacheTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(cacheAndAppcacheTypes, 0);
    dataList = test->fetch(cacheAndAppcacheTypes);
    g_assert_null(dataList);
}

#if SOUP_CHECK_VERSION(2, 67, 91)
static void prepopulateHstsData()
{
    // HSTS headers will be ignored in this test because the spec forbids STS policies from being honored for hosts with
    // an IP address instead of a domain. In order to be able to test the data manager API with HSTS website data, we
    // prepopulate the HSTS storage using the libsoup API directly.

    GUniquePtr<char> hstsDatabase(g_build_filename(Test::dataDirectory(), "hsts-storage.sqlite", nullptr));
    GRefPtr<SoupHSTSEnforcer> enforcer = adoptGRef(soup_hsts_enforcer_db_new(hstsDatabase.get()));
    GUniquePtr<SoupHSTSPolicy> policy(soup_hsts_policy_new("webkitgtk.org", 3600, true));
    soup_hsts_enforcer_set_policy(enforcer.get(), policy.get());

    policy.reset(soup_hsts_policy_new("webkit.org", 3600, true));
    soup_hsts_enforcer_set_policy(enforcer.get(), policy.get());
}

static void testWebsiteDataHsts(WebsiteDataTest* test, gconstpointer)
{
    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_HSTS_CACHE);
    g_assert_cmpuint(g_list_length(dataList), ==, 2);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_HSTS_CACHE);
    // HSTS data size is unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_HSTS_CACHE), ==, 0);

    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_HSTS_CACHE, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_HSTS_CACHE);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_HSTS_CACHE);

    // Remove all HSTS data.
    test->clear(WEBKIT_WEBSITE_DATA_HSTS_CACHE, 0);
    g_assert_null(test->fetch(WEBKIT_WEBSITE_DATA_HSTS_CACHE));
}
#endif

static void testWebsiteDataCookies(WebsiteDataTest* test, gconstpointer)
{
    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_COOKIES);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();

    dataList = test->fetch(WEBKIT_WEBSITE_DATA_COOKIES);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, "127.0.0.1");
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_COOKIES);
    // Cookies size is unknown.
    g_assert_cmpuint(webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_COOKIES), ==, 0);

    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_COOKIES, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_COOKIES);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndCookieTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_COOKIES | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(cacheAndCookieTypes, 0);
    dataList = test->fetch(cacheAndCookieTypes);
    g_assert_null(dataList);
}

static void testWebsiteDataDeviceIdHashSalt(WebsiteDataTest* test, gconstpointer)
{
    WebKitSettings* settings = webkit_web_view_get_settings(test->m_webView);
    gboolean enabled = webkit_settings_get_enable_media_stream(settings);
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_mock_capture_devices(settings, TRUE);

    test->clear(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT, 0);

    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/enumeratedevices").data());
    test->waitUntilTitleChangedTo("Finished");

    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    g_assert_nonnull(dataList);

    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);
    g_assert_cmpuint(webkit_website_data_get_types(data), ==, WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);

    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    g_assert_null(dataList);

    test->loadURI("about:blank");
    test->waitUntilTitleChanged();

    // Test removing the cookies.
    test->loadURI(kServer->getURIForPath("/enumeratedevices").data());
    test->waitUntilTitleChangedTo("Finished");

    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    g_assert_nonnull(dataList);
    data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);

    GList removeCookieList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_COOKIES, &removeCookieList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndAppcacheTypes = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);
    test->clear(cacheAndAppcacheTypes, 0);
    dataList = test->fetch(cacheAndAppcacheTypes);
    g_assert_null(dataList);

    webkit_settings_set_enable_media_stream(settings, enabled);
    webkit_settings_set_enable_mock_capture_devices(settings, enabled);
}

static void testWebsiteDataITP(WebsiteDataTest* test, gconstpointer)
{
    GUniquePtr<char> itpDirectory(g_build_filename(Test::dataDirectory(), "itp", nullptr));
    GUniquePtr<char> itpDatabaseFile(g_build_filename(itpDirectory.get(), "observations.db", nullptr));

    // Delete any previous data.
    g_unlink(itpDatabaseFile.get());
    g_rmdir(itpDirectory.get());

#if ENABLE(2022_GLIB_API)
    g_assert_false(webkit_network_session_get_itp_enabled(test->m_networkSession.get()));
#else
    g_assert_false(webkit_website_data_manager_get_itp_enabled(test->m_manager));
#endif
    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();

#if ENABLE(2022_GLIB_API)
    webkit_network_session_set_itp_enabled(test->m_networkSession.get(), TRUE);
    g_assert_true(webkit_network_session_get_itp_enabled(test->m_networkSession.get()));
#else
    webkit_website_data_manager_set_itp_enabled(test->m_manager, TRUE);
    g_assert_true(webkit_website_data_manager_get_itp_enabled(test->m_manager));
#endif
    g_assert_false(g_file_test(itpDirectory.get(), G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(itpDatabaseFile.get(), G_FILE_TEST_IS_REGULAR));

    test->loadURI(kServer->getURIForPath("/empty").data());
    test->waitUntilLoadFinished();
    test->assertFileIsCreated(itpDatabaseFile.get());
    g_assert_true(g_file_test(itpDirectory.get(), G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(itpDatabaseFile.get(), G_FILE_TEST_IS_REGULAR));

    // Give some time for the database to be updated.
    test->wait(0.5);

    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_ITP);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    auto* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);

    // Remove the registration.
    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_ITP, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_ITP);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndITP = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_ITP | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(cacheAndITP, 0);

#if ENABLE(2022_GLIB_API)
    webkit_network_session_set_itp_enabled(test->m_networkSession.get(), FALSE);
    g_assert_false(webkit_network_session_get_itp_enabled(test->m_networkSession.get()));
#else
    webkit_website_data_manager_set_itp_enabled(test->m_manager, FALSE);
    g_assert_false(webkit_website_data_manager_get_itp_enabled(test->m_manager));
#endif

    g_rmdir(itpDirectory.get());
}

static void testWebsiteDataServiceWorkerRegistrations(WebsiteDataTest* test, gconstpointer)
{
    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/service/register.html").data());
    test->waitUntilLoadFinished();

    dataList = test->fetch(WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    g_assert_nonnull(dataList);
    auto* data = static_cast<WebKitWebsiteData*>(dataList->data);
    g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/service/register.html").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);

    // Remove the registration.
    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes cacheAndRegistrations = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(cacheAndRegistrations, 0);
    dataList = test->fetch(cacheAndRegistrations);
    g_assert_null(dataList);
}

static void testWebsiteDataDOMCache(WebsiteDataTest* test, gconstpointer)
{
    GList* dataList = test->fetch(WEBKIT_WEBSITE_DATA_DOM_CACHE);
    g_assert_null(dataList);

    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    test->runJavaScriptAndWaitUntilFinished("let domCacheOpened = false; window.caches.open('TestDOMCache').then(() => { domCacheOpened = true});", nullptr);

    // Make sure the cache was opened before asking for it
    test->assertJavaScriptBecomesTrue("domCacheOpened");

    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DOM_CACHE);
    g_assert_nonnull(dataList);
    g_assert_cmpuint(g_list_length(dataList), ==, 1);
    auto* data = static_cast<WebKitWebsiteData*>(dataList->data);
g_assert_nonnull(data);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(kServer->getURIForPath("/").data());
    g_assert_cmpstr(webkit_website_data_get_name(data), ==, webkit_security_origin_get_host(origin));
    webkit_security_origin_unref(origin);

    // Remove the cache.
    GList removeList = { data, nullptr, nullptr };
    test->remove(WEBKIT_WEBSITE_DATA_DOM_CACHE, &removeList);
    dataList = test->fetch(WEBKIT_WEBSITE_DATA_DOM_CACHE);
    g_assert_null(dataList);

    // Clear all.
    static const WebKitWebsiteDataTypes caches = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_DOM_CACHE | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE);
    test->clear(caches, 0);
    dataList = test->fetch(caches);
    g_assert_null(dataList);
}

static void testWebViewHandleCorruptedLocalStorage(WebsiteDataTest* test, gconstpointer)
{
    const char html[] = "<html><script>let foo = (window.localStorage.length ? window.localStorage.getItem('item'):''); window.localStorage.setItem('item','value');</script></html>";
    auto waitForFooChanged = [&test](WebKitWebView* webView) {
        GUniqueOutPtr<GError> error;
        JSCValue* jscvalue;
        WebKitJavascriptResult* result = test->runJavaScriptAndWaitUntilFinished("foo;", &error.outPtr(), webView);
        g_assert_no_error(error.get());
        jscvalue = webkit_javascript_result_get_js_value(result);
        GUniquePtr<char> fooValue(jsc_value_to_string(jscvalue));
        return fooValue;
    };

    // Create corrupted database file for local storage of "http://example.com".
    const GUniquePtr<char> localStorageDirPath(g_build_filename(Test::dataDirectory(), "local-storage", nullptr));
    const GUniquePtr<char> localStorageFilePath(g_build_filename(localStorageDirPath.get(), "http_example.com_0.localstorage", nullptr));
    g_mkdir_with_parents(localStorageDirPath.get(), 0755);
    g_file_set_contents(localStorageFilePath.get(), "GARBAGE", -1, nullptr);

    g_assert_true(g_file_test(localStorageFilePath.get(), G_FILE_TEST_EXISTS));

    // Loading a web page to store an item in localStorage.
    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    auto fooValue(waitForFooChanged(test->m_webView));
    g_assert_cmpstr(fooValue.get(), ==, "");

    // Creating a second web view and loading a web page with the same url to read the item from localStorage.
    auto webView = Test::adoptView(Test::createWebView(test->m_webContext.get()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView.get()));
    webkit_web_view_load_html(webView.get(), html, "http://example.com");
    test->waitUntilLoadFinished(webView.get());
    fooValue = waitForFooChanged(webView.get());
    g_assert_cmpstr(fooValue.get(), ==, "value");
}

class MemoryPressureTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(MemoryPressureTest, setup, teardown);

    static void setup()
    {
        WebKitMemoryPressureSettings* settings = webkit_memory_pressure_settings_new();
        webkit_memory_pressure_settings_set_memory_limit(settings, 1);
        webkit_memory_pressure_settings_set_poll_interval(settings, 0.001);
        webkit_memory_pressure_settings_set_kill_threshold(settings, 1);
        webkit_memory_pressure_settings_set_strict_threshold(settings, 0.75);
        webkit_memory_pressure_settings_set_conservative_threshold(settings, 0.5);
#if ENABLE(2022_GLIB_API)
        webkit_network_session_set_memory_pressure_settings(settings);
#else
        webkit_website_data_manager_set_memory_pressure_settings(settings);
#endif
        webkit_memory_pressure_settings_free(settings);
    }

    static void teardown()
    {
#if ENABLE(2022_GLIB_API)
        webkit_network_session_set_memory_pressure_settings(nullptr);
#else
        webkit_website_data_manager_set_memory_pressure_settings(nullptr);
#endif
    }

    static gboolean loadFailedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError* error, MemoryPressureTest* test)
    {
        g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadFailedCallback), test);
        g_main_loop_quit(test->m_mainLoop);

        return TRUE;
    }

    void waitUntilLoadFailed()
    {
        g_signal_connect(m_webView, "load-failed", G_CALLBACK(MemoryPressureTest::loadFailedCallback), this);
        g_main_loop_run(m_mainLoop);
    }
};

static void testMemoryPressureSettings(MemoryPressureTest* test, gconstpointer)
{
    // We have set a memory limit of 1MB with a kill threshold of 1 and a poll interval of 0.001.
    // The network process will use around 2MB to load the test. The MemoryPressureHandler will
    // kill the process as soon as it detects that it's using more than 1MB, so the network process
    // won't be able to complete the resource load. This causes an internal error and the load-failed
    // signal is emitted.
    GUniquePtr<char> fileURL(g_strdup_printf("file://%s/simple.html", Test::getResourcesDir(Test::WebKit2Resources).data()));
    test->loadURI(fileURL.get());
    test->waitUntilLoadFailed();
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

#if SOUP_CHECK_VERSION(2, 67, 91)
    prepopulateHstsData();
#endif

    WebsiteDataTest::add("WebKitWebsiteData", "configuration", testWebsiteDataConfiguration);
    WebViewTest::add("WebKitWebsiteData", "ephemeral", testWebsiteDataEphemeral);
    WebsiteDataTest::add("WebKitWebsiteData", "cache", testWebsiteDataCache);
    WebsiteDataTest::add("WebKitWebsiteData", "storage", testWebsiteDataStorage);
    WebsiteDataTest::add("WebKitWebsiteData", "databases", testWebsiteDataDatabases);
    WebsiteDataTest::add("WebKitWebsiteData", "appcache", testWebsiteDataAppcache);
    WebsiteDataTest::add("WebKitWebsiteData", "cookies", testWebsiteDataCookies);
#if SOUP_CHECK_VERSION(2, 67, 91)
    WebsiteDataTest::add("WebKitWebsiteData", "hsts", testWebsiteDataHsts);
#endif
    WebsiteDataTest::add("WebKitWebsiteData", "deviceidhashsalt", testWebsiteDataDeviceIdHashSalt);
    WebsiteDataTest::add("WebKitWebsiteData", "itp", testWebsiteDataITP);
    WebsiteDataTest::add("WebKitWebsiteData", "service-worker-registrations", testWebsiteDataServiceWorkerRegistrations);
    WebsiteDataTest::add("WebKitWebsiteData", "dom-cache", testWebsiteDataDOMCache);
    WebsiteDataTest::add("WebKitWebsiteData", "handle-corrupted-local-storage", testWebViewHandleCorruptedLocalStorage);
    MemoryPressureTest::add("WebKitWebsiteData", "memory-pressure", testMemoryPressureSettings);
}

void afterAll()
{
    delete kServer;
}
