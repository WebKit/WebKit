/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2014 Collabora Ltd.
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
#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <WebCore/SoupVersioning.h>
#include <glib/gstdio.h>
#include <wtf/glib/GRefPtr.h>

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
#include <wpe/extensions/audio.h>
#endif

class IsPlayingAudioWebViewTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(IsPlayingAudioWebViewTest);

    static void isPlayingAudioChanged(GObject*, GParamSpec*, IsPlayingAudioWebViewTest* test)
    {
        g_signal_handlers_disconnect_by_func(test->m_webView, reinterpret_cast<void*>(isPlayingAudioChanged), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    void waitUntilIsPlayingAudioChanged()
    {
        g_signal_connect(m_webView, "notify::is-playing-audio", G_CALLBACK(isPlayingAudioChanged), this);
        g_main_loop_run(m_mainLoop);
    }

    void periodicallyCheckIsPlayingForAWhile()
    {
        m_tickCount = 0;
        g_timeout_add(50, [](gpointer userData) -> gboolean {
            auto* test = static_cast<IsPlayingAudioWebViewTest*>(userData);
            g_assert_true(webkit_web_view_is_playing_audio(test->m_webView));
            test->m_tickCount++;
            if (test->m_tickCount >= 10) {
                test->quitMainLoop();
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }, this);
        g_main_loop_run(m_mainLoop);
    }

private:
    uint32_t m_tickCount { 0 };
};

static WebKitTestServer* gServer;

static void testWebViewWebContext(WebViewTest* test, gconstpointer)
{
    g_assert_true(webkit_web_view_get_context(test->m_webView) == test->m_webContext.get());
    g_assert_true(webkit_web_context_get_default() != test->m_webContext.get());

    // Check that a web view created with g_object_new has the default context.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        nullptr));
    g_assert_true(webkit_web_view_get_context(webView.get()) == webkit_web_context_get_default());

    // Check that a web view created with a related view has the related view context.
    webView = Test::adoptView(Test::createWebView(test->m_webView));
    g_assert_true(webkit_web_view_get_context(webView.get()) == test->m_webContext.get());

    // Check that a web context given as construct parameter is ignored if a related view is also provided.
    Test::removeLogFatalFlag(G_LOG_LEVEL_CRITICAL);
    webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", webkit_web_context_get_default(),
        "related-view", test->m_webView,
        nullptr));
    Test::addLogFatalFlag(G_LOG_LEVEL_CRITICAL);
    g_assert_true(webkit_web_view_get_context(webView.get()) == test->m_webContext.get());
}

static void testWebViewWebContextLifetime(WebViewTest* test, gconstpointer)
{
    WebKitWebContext* webContext = webkit_web_context_new();
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webContext));

    auto* webView = Test::createWebView(webContext);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView));

#if PLATFORM(GTK)
    g_object_ref_sink(webView);
#endif
    g_object_unref(webContext);

    // Check that the web view still has a valid context.
    WebKitWebContext* tmpContext = webkit_web_view_get_context(WEBKIT_WEB_VIEW(webView));
    g_assert_true(WEBKIT_IS_WEB_CONTEXT(tmpContext));
    g_object_unref(webView);

    WebKitWebContext* webContext2 = webkit_web_context_new();
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webContext2));

    auto* webView2 = Test::createWebView(webContext2);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView2));

#if PLATFORM(GTK)
    g_object_ref_sink(webView2);
#endif
    g_object_unref(webView2);

    // Check that the context is still valid.
    g_assert_true(WEBKIT_IS_WEB_CONTEXT(webContext2));
    g_object_unref(webContext2);
}

static void testWebViewCloseQuickly(WebViewTest* test, gconstpointer)
{
    auto webView = Test::adoptView(Test::createWebView());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView.get()));
    g_idle_add([](gpointer userData) -> gboolean {
        static_cast<WebViewTest*>(userData)->quitMainLoop();
        return G_SOURCE_REMOVE;
    }, test);
    g_main_loop_run(test->m_mainLoop);
    webView = nullptr;
}

#if PLATFORM(WPE)
static void testWebViewWebBackend(Test* test, gconstpointer)
{
    static struct wpe_view_backend_interface s_testingInterface = {
        // create
        [](void*, struct wpe_view_backend*) -> void* { return nullptr; },
        // destroy
        [](void*) { },
        // initialize
        [](void*) { },
        // get_renderer_host_fd
        [](void*) -> int { return -1; },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    // User provided backend with default deleter (we don't have a way to check the backend will be actually freed).
    GRefPtr<WebKitWebView> webView = adoptGRef(webkit_web_view_new(webkit_web_view_backend_new(wpe_view_backend_create_with_backend_interface(&s_testingInterface, nullptr), nullptr, nullptr)));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView.get()));
    auto* viewBackend = webkit_web_view_get_backend(webView.get());
    g_assert_nonnull(viewBackend);
    auto* wpeBackend = webkit_web_view_backend_get_wpe_backend(viewBackend);
    g_assert_nonnull(wpeBackend);
    webView = nullptr;

    // User provided backend with destroy notify.
    wpeBackend = wpe_view_backend_create_with_backend_interface(&s_testingInterface, nullptr);
    webView = adoptGRef(webkit_web_view_new(webkit_web_view_backend_new(wpeBackend, [](gpointer userData) {
        auto* backend = *static_cast<struct wpe_view_backend**>(userData);
        wpe_view_backend_destroy(backend);
        *static_cast<struct wpe_view_backend**>(userData) = nullptr;
    }, &wpeBackend)));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView.get()));
    webView = nullptr;
    g_assert_null(wpeBackend);

    // User provided backend owned by another object with destroy notify.
    static bool hasInstance = false;
    struct BackendOwner {
        BackendOwner(struct wpe_view_backend* backend)
            : backend(backend)
        {
            hasInstance = true;
        }

        ~BackendOwner()
        {
            wpe_view_backend_destroy(backend);
            hasInstance = false;
        }

        struct wpe_view_backend* backend;
    };
    auto* owner = new BackendOwner(wpe_view_backend_create_with_backend_interface(&s_testingInterface, nullptr));
    g_assert_true(hasInstance);
    webView = adoptGRef(webkit_web_view_new(webkit_web_view_backend_new(owner->backend, [](gpointer userData) {
        delete static_cast<BackendOwner*>(userData);
    }, owner)));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView.get()));
    g_assert_true(hasInstance);
    webView = nullptr;
    g_assert_false(hasInstance);
}
#endif // PLATFORM(WPE)

static void ephemeralViewloadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewloadChanged), test);
    test->quitMainLoop();
}

static void testWebViewEphemeral(WebViewTest* test, gconstpointer)
{
#if ENABLE(2022_GLIB_API)
    auto* networkSession = webkit_web_view_get_network_session(test->m_webView);
    g_assert_false(webkit_network_session_is_ephemeral(networkSession));
    auto* manager = webkit_network_session_get_website_data_manager(networkSession);
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));
    GRefPtr<WebKitNetworkSession> ephemeralSession = adoptGRef(webkit_network_session_new_ephemeral());
#else
    g_assert_false(webkit_web_view_is_ephemeral(test->m_webView));
    g_assert_false(webkit_web_context_is_ephemeral(webkit_web_view_get_context(test->m_webView)));
    auto* manager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));
    g_assert_true(webkit_web_view_get_website_data_manager(test->m_webView) == manager);
#endif
    webkit_website_data_manager_clear(manager, WEBKIT_WEBSITE_DATA_DISK_CACHE, 0, nullptr, [](GObject* manager, GAsyncResult* result, gpointer userData) {
        webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(manager), result, nullptr);
        static_cast<WebViewTest*>(userData)->quitMainLoop();
    }, test);
    g_main_loop_run(test->m_mainLoop);

    // A WebView on a non ephemeral context can be ephemeral.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", webkit_web_view_get_context(test->m_webView),
#if ENABLE(2022_GLIB_API)
        "network-session", ephemeralSession.get(),
#else
        "is-ephemeral", TRUE,
#endif
        nullptr));
#if ENABLE(2022_GLIB_API)
    g_assert_true(webkit_network_session_is_ephemeral(webkit_web_view_get_network_session(webView.get())));
    g_assert_true(webkit_web_view_get_network_session(webView.get()) != networkSession);
#else
    g_assert_true(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_false(webkit_web_context_is_ephemeral(webkit_web_view_get_context(webView.get())));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) != manager);
#endif

    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(ephemeralViewloadChanged), test);
    webkit_web_view_load_uri(webView.get(), gServer->getURIForPath("/").data());
    g_main_loop_run(test->m_mainLoop);

    // Disk cache delays the storing of initial resources for 1 second to avoid
    // affecting early page load. So, wait 1 second here to make sure resources
    // have already been stored.
    test->wait(1);

    webkit_website_data_manager_fetch(manager, WEBKIT_WEBSITE_DATA_DISK_CACHE, nullptr, [](GObject* manager, GAsyncResult* result, gpointer userData) {
        auto* test = static_cast<WebViewTest*>(userData);
        g_assert_null(webkit_website_data_manager_fetch_finish(WEBKIT_WEBSITE_DATA_MANAGER(manager), result, nullptr));
        test->quitMainLoop();
    }, test);
    g_main_loop_run(test->m_mainLoop);
}

static void testWebViewCustomCharset(WebViewTest* test, gconstpointer)
{
    test->loadURI(gServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_null(webkit_web_view_get_custom_charset(test->m_webView));
    webkit_web_view_set_custom_charset(test->m_webView, "utf8");
    // Changing the charset reloads the page, so wait until reloaded.
    test->waitUntilLoadFinished();
    g_assert_cmpstr(webkit_web_view_get_custom_charset(test->m_webView), ==, "utf8");

    // Go back to the default charset and wait until reloaded.
    webkit_web_view_set_custom_charset(test->m_webView, nullptr);
    test->waitUntilLoadFinished();
    g_assert_null(webkit_web_view_get_custom_charset(test->m_webView));
}

static void testWebViewSettings(WebViewTest* test, gconstpointer)
{
    WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
    g_assert_nonnull(defaultSettings);
    g_assert_true(webkit_settings_get_enable_javascript(defaultSettings));

    GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
    g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", FALSE, NULL);
    webkit_web_view_set_settings(test->m_webView, newSettings.get());

    WebKitSettings* settings = webkit_web_view_get_settings(test->m_webView);
    g_assert_true(settings != defaultSettings);
    g_assert_false(webkit_settings_get_enable_javascript(settings));

    auto webView2 = Test::adoptView(Test::createWebView());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView2.get()));
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webView2.get()), settings);
    g_assert_true(webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView2.get())) == settings);

    GRefPtr<WebKitSettings> newSettings2 = adoptGRef(webkit_settings_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings2.get()));
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webView2.get()), newSettings2.get());
    settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView2.get()));
    g_assert_true(settings == newSettings2.get());
    g_assert_true(webkit_settings_get_enable_javascript(settings));

    auto webView3 = Test::adoptView(Test::createWebView(newSettings2.get()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView3.get()));
    g_assert_true(webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView3.get())) == newSettings2.get());
}

static void testWebViewZoomLevel(WebViewTest* test, gconstpointer)
{
    g_assert_cmpfloat(webkit_web_view_get_zoom_level(test->m_webView), ==, 1);
    webkit_web_view_set_zoom_level(test->m_webView, 2.5);
    g_assert_cmpfloat(webkit_web_view_get_zoom_level(test->m_webView), ==, 2.5);

    webkit_settings_set_zoom_text_only(webkit_web_view_get_settings(test->m_webView), TRUE);
    // The zoom level shouldn't change when zoom-text-only setting changes.
    g_assert_cmpfloat(webkit_web_view_get_zoom_level(test->m_webView), ==, 2.5);
}

static void testWebViewRunAsyncFunctions(WebViewTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;

    JSCValue* value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(42); });", nullptr, nullptr, &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 42);

    GVariantDict dict;
    g_variant_dict_init(&dict, nullptr);
    g_variant_dict_insert(&dict, "count", "u", 42);
    auto* args = g_variant_dict_end(&dict);
    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(count); });", args, nullptr, &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 42);

    g_variant_dict_init(&dict, nullptr);
    g_variant_dict_insert(&dict, "motto", "s", "Never gonna give you up");
    args = g_variant_dict_end(&dict);
    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(motto); });", args, nullptr, &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "Never gonna give you up");

    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise(function(resolve, reject) { setTimeout(function(){ reject('Rejected!') }, 0); })", nullptr, nullptr, &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
    g_assert_true(g_strstr_len(error->message, strlen(error->message), "Rejected!") != nullptr);

    g_variant_dict_init(&dict, nullptr);
    g_variant_dict_insert(&dict, "countt", "u", 42);
    args = g_variant_dict_end(&dict);
    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(count); });", args, nullptr, &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);

    g_variant_dict_init(&dict, nullptr);
    g_variant_dict_insert(&dict, "count", "u", 42);
    args = g_variant_dict_end(&dict);
    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return count", args, nullptr, &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 42);

    g_variant_dict_init(&dict, nullptr);
    g_variant_dict_insert(&dict, "count", "(u)", 42);
    args = g_variant_dict_end(&dict);
    value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(count); });", args, nullptr, &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_INVALID_PARAMETER);

    {
        // Set a value in main world.
        JSCValue* value = test->runJavaScriptAndWaitUntilFinished("a = 25;", &error.outPtr());
        g_assert_nonnull(value);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
        g_assert_no_error(error.get());
        g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 25);

        // Read back value from main world.
        value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return a", nullptr, nullptr, &error.outPtr());
        g_assert_nonnull(value);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
        g_assert_no_error(error.get());
        g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 25);

        // Values of the main world are not available in the isolated one.
        value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return a", nullptr, "WebExtensionTestScriptWorld", &error.outPtr());
        g_assert_null(value);
        g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);

        // An empty string for world name is a distinct isolated world.
        value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return a", nullptr, "", &error.outPtr());
        g_assert_null(value);
        g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);

        // Running a script in a world that doesn't exist should fail.
        value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return a", nullptr, "InvalidScriptWorld", &error.outPtr());
        g_assert_null(value);
        g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
    }

    {
        // Disable JS support and expect an error when attempting to evaluate JS code.
        WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
        g_assert_nonnull(defaultSettings);
        g_assert_true(webkit_settings_get_enable_javascript(defaultSettings));

        GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", FALSE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());

        JSCValue* value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(42); });", nullptr, nullptr, &error.outPtr());
        g_assert_null(value);
        g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);

        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", TRUE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());
    }

    {
        // Disable JS markup support and expect no error when attempting to evaluate JS code.
        WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
        g_assert_nonnull(defaultSettings);
        g_assert_true(webkit_settings_get_enable_javascript_markup(defaultSettings));

        GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript-markup", FALSE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());

        JSCValue* value = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished("return new Promise((resolve) => { resolve(42); });", nullptr, nullptr, &error.outPtr());
        g_assert_nonnull(value);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
        g_assert_no_error(error.get());

        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript-markup", TRUE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());
    }
}

static void testWebViewRunJavaScript(WebViewTest* test, gconstpointer)
{
    static const char* html = "<html><body><a id='WebKitLink' href='http://www.webkitgtk.org/' title='WebKitGTK Title'>WebKitGTK Website</a></body></html>";
    test->loadHtml(html, "file:///");
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    JSCValue* value = test->runJavaScriptAndWaitUntilFinished("window.document.getElementById('WebKitLink').title;", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "WebKitGTK Title");

    value = test->runJavaScriptAndWaitUntilFinished("window.document.getElementById('WebKitLink').href;", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "http://www.webkitgtk.org/");

    value = test->runJavaScriptAndWaitUntilFinished("window.document.getElementById('WebKitLink').textContent", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "WebKitGTK Website");

    value = test->runJavaScriptAndWaitUntilFinished("a = 25;", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 25);

    value = test->runJavaScriptAndWaitUntilFinished("a = 2.5;", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 2.5);

    value = test->runJavaScriptAndWaitUntilFinished("a = true", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));

    value = test->runJavaScriptAndWaitUntilFinished("a = false", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_false(WebViewTest::javascriptResultToBoolean(value));

    value = test->runJavaScriptAndWaitUntilFinished("a = null", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultIsNull(value));

    value = test->runJavaScriptAndWaitUntilFinished("function Foo() { a = 25; } Foo();", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultIsUndefined(value));

    GUniquePtr<char> scriptFile(g_build_filename(WEBKIT_SRC_DIR, "Tools", "TestWebKitAPI", "Tests", "JavaScriptCore", "glib", "script.js", nullptr));
    GUniqueOutPtr<char> contents;
    gsize contentsSize;
    g_assert_true(g_file_get_contents(scriptFile.get(), &contents.outPtr(), &contentsSize, nullptr));
    value = test->runJavaScriptAndWaitUntilFinished(contents.get(), contentsSize, &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    value = test->runJavaScriptAndWaitUntilFinished("testStringWithNull()", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_true(JSC_IS_VALUE(value));
    g_assert_no_error(error.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_true(jsc_value_is_string(value));
    valueString.reset(jsc_value_to_string(value));
    g_assert_cmpstr(valueString.get(), ==, "String");
    GRefPtr<GBytes> valueBytes = adoptGRef(jsc_value_to_string_as_bytes(value));
    GString* expected = g_string_new("String");
    expected = g_string_append_c(expected, '\0');
    expected = g_string_append(expected, "With");
    expected = g_string_append_c(expected, '\0');
    expected = g_string_append(expected, "Null");
    GRefPtr<GBytes> expectedBytes = adoptGRef(g_string_free_to_bytes(expected));
    g_assert_true(g_bytes_equal(valueBytes.get(), expectedBytes.get()));

    value = test->runJavaScriptFromGResourceAndWaitUntilFinished("/org/webkit/glib/tests/link-title.js", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "WebKitGTK Title");

    value = test->runJavaScriptFromGResourceAndWaitUntilFinished("/wrong/path/to/resource.js", &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);

    value = test->runJavaScriptAndWaitUntilFinished("foo();", &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
    g_assert_true(g_str_has_prefix(error->message, "file:///"));

    value = test->runJavaScriptAndWaitUntilFinished("window.document.body", &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_INVALID_RESULT);

    // Values of the main world are not available in the isolated one.
    value = test->runJavaScriptInWorldAndWaitUntilFinished("a", "WebExtensionTestScriptWorld", nullptr, &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
    g_assert_true(g_str_has_prefix(error->message, "file:///"));

    value = test->runJavaScriptInWorldAndWaitUntilFinished("a = 50", "WebExtensionTestScriptWorld", nullptr, &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 50);

    // Values of the isolated world are not available in the normal one.
    value = test->runJavaScriptAndWaitUntilFinished("a", &error.outPtr());
    g_assert_nonnull(value);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
    g_assert_no_error(error.get());
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(value), ==, 25);

    // Running a script in a world that doesn't exist should fail.
    value = test->runJavaScriptInWorldAndWaitUntilFinished("a", "InvalidScriptWorld", "foo:///bar", &error.outPtr());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
    g_assert_true(g_str_has_prefix(error->message, "foo:///bar"));

    {
        // Disable JS support and expect an error when attempting to evaluate JS code.
        WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
        g_assert_nonnull(defaultSettings);
        g_assert_true(webkit_settings_get_enable_javascript(defaultSettings));

        GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", FALSE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());

        JSCValue* value = test->runJavaScriptAndWaitUntilFinished("console.log(\"Hi\");", &error.outPtr());
        g_assert_null(value);
        g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);

        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", TRUE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());
    }

    {
        // Disable JS markup support and expect no error when attempting to evaluate JS code.
        WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
        g_assert_nonnull(defaultSettings);
        g_assert_true(webkit_settings_get_enable_javascript_markup(defaultSettings));

        GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript-markup", FALSE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());

        JSCValue* value = test->runJavaScriptAndWaitUntilFinished("console.log(\"Hi\");", &error.outPtr());
        g_assert_nonnull(value);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(value));
        g_assert_no_error(error.get());

        g_object_set(G_OBJECT(newSettings.get()), "enable-javascript-markup", TRUE, NULL);
        webkit_web_view_set_settings(test->m_webView, newSettings.get());
    }
}

class FullScreenClientTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FullScreenClientTest);

    enum FullScreenEvent {
        None,
        Enter,
        Leave
    };

    static gboolean viewEnterFullScreenCallback(WebKitWebView*, FullScreenClientTest* test)
    {
        test->m_event = Enter;
        g_main_loop_quit(test->m_mainLoop);
        return FALSE;
    }

    static gboolean viewLeaveFullScreenCallback(WebKitWebView*, FullScreenClientTest* test)
    {
        test->m_event = Leave;
        g_main_loop_quit(test->m_mainLoop);
        return FALSE;
    }

    FullScreenClientTest()
        : m_event(None)
    {
        webkit_settings_set_enable_fullscreen(webkit_web_view_get_settings(m_webView), TRUE);
        g_signal_connect(m_webView, "enter-fullscreen", G_CALLBACK(viewEnterFullScreenCallback), this);
        g_signal_connect(m_webView, "leave-fullscreen", G_CALLBACK(viewLeaveFullScreenCallback), this);
    }

    ~FullScreenClientTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void requestFullScreenAndWaitUntilEnteredFullScreen()
    {
        m_event = None;
        runJavaScriptAndWait("document.documentElement.webkitRequestFullScreen();");
    }

    static gboolean leaveFullScreenIdle(FullScreenClientTest* test)
    {
#if PLATFORM(GTK)
        test->keyStroke(GDK_KEY_Escape);
#else
        test->keyStroke(WPE_KEY_Escape);
#endif
        return FALSE;
    }

    void leaveFullScreenAndWaitUntilLeftFullScreen()
    {
        m_event = None;
        g_idle_add(reinterpret_cast<GSourceFunc>(leaveFullScreenIdle), this);
        g_main_loop_run(m_mainLoop);
    }

    FullScreenEvent m_event;
};

#if ENABLE(FULLSCREEN_API)
static void testWebViewFullScreen(FullScreenClientTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml("<html><body>FullScreen test</body></html>", 0);
    test->waitUntilLoadFinished();
    test->requestFullScreenAndWaitUntilEnteredFullScreen();
    g_assert_cmpint(test->m_event, ==, FullScreenClientTest::Enter);
    test->leaveFullScreenAndWaitUntilLeftFullScreen();
    g_assert_cmpint(test->m_event, ==, FullScreenClientTest::Leave);
}
#endif

static void testWebViewCanShowMIMEType(WebViewTest* test, gconstpointer)
{
    // Supported MIME types.
    g_assert_true(webkit_web_view_can_show_mime_type(test->m_webView, "text/html"));
    g_assert_true(webkit_web_view_can_show_mime_type(test->m_webView, "text/plain"));
    g_assert_true(webkit_web_view_can_show_mime_type(test->m_webView, "image/jpeg"));

    // Unsupported MIME types.
    g_assert_false(webkit_web_view_can_show_mime_type(test->m_webView, "text/vcard"));
    g_assert_false(webkit_web_view_can_show_mime_type(test->m_webView, "application/zip"));
    g_assert_false(webkit_web_view_can_show_mime_type(test->m_webView, "application/octet-stream"));

#if ENABLE(NETSCAPE_PLUGIN_API)
    // Plugins are only supported when enabled.
    webkit_web_context_set_additional_plugins_directory(webkit_web_view_get_context(test->m_webView), WEBKIT_TEST_PLUGIN_DIR);
    g_assert_true(webkit_web_view_can_show_mime_type(test->m_webView, "application/x-webkit-test-netscape"));
    webkit_settings_set_enable_plugins(webkit_web_view_get_settings(test->m_webView), FALSE);
    g_assert_false(webkit_web_view_can_show_mime_type(test->m_webView, "application/x-webkit-test-netscape"));
#endif
}

#if PLATFORM(GTK)
class FormClientTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FormClientTest);

    static void submitFormCallback(WebKitWebView*, WebKitFormSubmissionRequest* request, FormClientTest* test)
    {
        test->submitForm(request);
    }

    FormClientTest()
        : m_submitPositionX(0)
        , m_submitPositionY(0)
    {
        g_signal_connect(m_webView, "submit-form", G_CALLBACK(submitFormCallback), this);
    }

    ~FormClientTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void submitForm(WebKitFormSubmissionRequest* request)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));
        m_request = request;
        webkit_form_submission_request_submit(request);
        quitMainLoop();
    }

#if !USE(GTK4)
    GHashTable* getTextFieldsAsHashTable()
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        return webkit_form_submission_request_get_text_fields(m_request.get());
#pragma GCC diagnostic pop
    }
#endif

    GPtrArray* getTextFieldNames()
    {
        GPtrArray* names;
        webkit_form_submission_request_list_text_fields(m_request.get(), &names, nullptr);
        return names;
    }

    GPtrArray* getTextFieldValues()
    {
        GPtrArray* values;
        webkit_form_submission_request_list_text_fields(m_request.get(), nullptr, &values);
        return values;
    }

    static gboolean doClickIdleCallback(FormClientTest* test)
    {
        test->clickMouseButton(test->m_submitPositionX, test->m_submitPositionY, 1);
        return FALSE;
    }

    void submitFormAtPosition(int x, int y)
    {
        m_submitPositionX = x;
        m_submitPositionY = y;
        g_idle_add(reinterpret_cast<GSourceFunc>(doClickIdleCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    int m_submitPositionX;
    int m_submitPositionY;
    GRefPtr<WebKitFormSubmissionRequest> m_request;
};

static void testWebViewSubmitForm(FormClientTest* test, gconstpointer)
{
    test->showInWindow();

    const char* formHTML =
        "<html><body>"
        " <form action='#'>"
        "  <input type='text' name='text1' value='value1'>"
        "  <input type='text' name='text2' value='value2'>"
        "  <input type='text' value='value3'>"
        "  <input type='text' name='text2'>"
        "  <input type='password' name='password' value='secret'>"
        "  <textarea cols='5' rows='5' name='textarea'>Text</textarea>"
        "  <input type='hidden' name='hidden1' value='hidden1'>"
        "  <input type='submit' value='Submit' style='position:absolute; left:1; top:1' size='10'>"
        " </form>"
        "</body></html>";

    test->loadHtml(formHTML, "file:///");
    test->waitUntilLoadFinished();

#if !USE(GTK4)
    test->submitFormAtPosition(5, 5);
    GHashTable* tableValues = test->getTextFieldsAsHashTable();
    g_assert_nonnull(tableValues);
    g_assert_cmpuint(g_hash_table_size(tableValues), ==, 4);
    g_assert_cmpstr(static_cast<char*>(g_hash_table_lookup(tableValues, "text1")), ==, "value1");
    g_assert_cmpstr(static_cast<char*>(g_hash_table_lookup(tableValues, "")), ==, "value3");
    g_assert_cmpstr(static_cast<char*>(g_hash_table_lookup(tableValues, "text2")), ==, "");
    g_assert_cmpstr(static_cast<char*>(g_hash_table_lookup(tableValues, "password")), ==, "secret");
#endif

    GPtrArray* names = test->getTextFieldNames();
    g_assert_nonnull(names);
    g_assert_cmpuint(names->len, ==, 5);
    g_assert_cmpstr(static_cast<char*>(names->pdata[0]), ==, "text1");
    g_assert_cmpstr(static_cast<char*>(names->pdata[1]), ==, "text2");
    g_assert_cmpstr(static_cast<char*>(names->pdata[2]), ==, "");
    g_assert_cmpstr(static_cast<char*>(names->pdata[3]), ==, "text2");
    g_assert_cmpstr(static_cast<char*>(names->pdata[4]), ==, "password");

    GPtrArray* values = test->getTextFieldValues();
    g_assert_nonnull(values);
    g_assert_cmpuint(values->len, ==, 5);
    g_assert_cmpstr(static_cast<char*>(values->pdata[0]), ==, "value1");
    g_assert_cmpstr(static_cast<char*>(values->pdata[1]), ==, "value2");
    g_assert_cmpstr(static_cast<char*>(values->pdata[2]), ==, "value3");
    g_assert_cmpstr(static_cast<char*>(values->pdata[3]), ==, "");
    g_assert_cmpstr(static_cast<char*>(values->pdata[4]), ==, "secret");
}
#endif // PLATFORM(GTK)

class SaveWebViewTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SaveWebViewTest);

    SaveWebViewTest()
        : m_tempDirectory(g_dir_make_tmp("WebKit2SaveViewTest-XXXXXX", 0))
    {
    }

    ~SaveWebViewTest()
    {
        if (G_IS_FILE(m_file.get()))
            g_file_delete(m_file.get(), 0, 0);

        if (G_IS_INPUT_STREAM(m_inputStream.get()))
            g_input_stream_close(m_inputStream.get(), 0, 0);

        if (m_tempDirectory)
            g_rmdir(m_tempDirectory.get());
    }

    static void webViewSavedToStreamCallback(GObject* object, GAsyncResult* result, SaveWebViewTest* test)
    {
        GUniqueOutPtr<GError> error;
        test->m_inputStream = adoptGRef(webkit_web_view_save_finish(test->m_webView, result, &error.outPtr()));
        g_assert_true(G_IS_INPUT_STREAM(test->m_inputStream.get()));
        g_assert_no_error(error.get());

        test->quitMainLoop();
    }

    static void webViewSavedToFileCallback(GObject* object, GAsyncResult* result, SaveWebViewTest* test)
    {
        GUniqueOutPtr<GError> error;
        g_assert_true(webkit_web_view_save_to_file_finish(test->m_webView, result, &error.outPtr()));
        g_assert_no_error(error.get());

        test->quitMainLoop();
    }

    void saveAndWaitForStream()
    {
        webkit_web_view_save(m_webView, WEBKIT_SAVE_MODE_MHTML, 0, reinterpret_cast<GAsyncReadyCallback>(webViewSavedToStreamCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    void saveAndWaitForFile()
    {
        m_saveDestinationFilePath.reset(g_build_filename(m_tempDirectory.get(), "testWebViewSaveResult.mht", NULL));
        m_file = adoptGRef(g_file_new_for_path(m_saveDestinationFilePath.get()));
        webkit_web_view_save_to_file(m_webView, m_file.get(), WEBKIT_SAVE_MODE_MHTML, 0, reinterpret_cast<GAsyncReadyCallback>(webViewSavedToFileCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    GUniquePtr<char> m_tempDirectory;
    GUniquePtr<char> m_saveDestinationFilePath;
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<GFile> m_file;
};

static void testWebViewSave(SaveWebViewTest* test, gconstpointer)
{
    test->loadHtml("<html>"
        "<body>"
        "  <p>A paragraph with plain text</p>"
        "  <p>"
        "    A red box: <img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3AYWDTMVwnSZnwAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFklEQVQI12P8z8DAwMDAxMDAwMDAAAANHQEDK+mmyAAAAABJRU5ErkJggg=='></br>"
        "    A blue box: <img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3AYWDTMvBHhALQAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFklEQVQI12Nk4PnPwMDAxMDAwMDAAAALrwEPPIs1pgAAAABJRU5ErkJggg=='>"
        "  </p>"
        "</body>"
        "</html>", 0);
    test->waitUntilLoadFinished();

    // Write to a file and to an input stream.
    test->saveAndWaitForFile();
    test->saveAndWaitForStream();

    // We should have exactly the same amount of bytes in the file
    // than those coming from the GInputStream. We don't compare the
    // strings read since the 'Date' field and the boundaries will be
    // different on each case. MHTML functionality will be tested by
    // Layout tests, so checking the amount of bytes is enough.
    GUniqueOutPtr<GError> error;
    gchar buffer[512] = { 0 };
    gssize readBytes = 0;
    gssize totalBytesFromStream = 0;
    while ((readBytes = g_input_stream_read(test->m_inputStream.get(), &buffer, 512, 0, &error.outPtr()))) {
        g_assert_no_error(error.get());
        totalBytesFromStream += readBytes;
    }

    // Check that the file exists and that it contains the same amount of bytes.
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(test->m_file.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE, static_cast<GFileQueryInfoFlags>(0), 0, 0));
    g_assert_cmpint(g_file_info_get_size(fileInfo.get()), ==, totalBytesFromStream);
}

// To test page visibility API. Currently only 'visible', 'hidden' and 'prerender' states are implemented fully in WebCore.
// See also http://www.w3.org/TR/2011/WD-page-visibility-20110602/ and https://developers.google.com/chrome/whitepapers/pagevisibility
static void testWebViewPageVisibility(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><title></title>"
        "<body><p>Test Web Page Visibility</p>"
        "<script>"
        "document.addEventListener(\"visibilitychange\", onVisibilityChange, false);"
        "function onVisibilityChange() {"
        "    document.title = document.visibilityState;"
        "}"
        "</script>"
        "</body></html>",
        0);

    // Wait until the page is loaded. Initial visibility should be 'prerender'.
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    JSCValue* value = test->runJavaScriptAndWaitUntilFinished("document.visibilityState;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "hidden");

    value = test->runJavaScriptAndWaitUntilFinished("document.hidden;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));

    // Show the page. The visibility should be updated to 'visible'.
    test->showInWindow();
    test->waitUntilTitleChangedTo("visible");

    value = test->runJavaScriptAndWaitUntilFinished("document.visibilityState;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "visible");

    value = test->runJavaScriptAndWaitUntilFinished("document.hidden;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_false(WebViewTest::javascriptResultToBoolean(value));

    // Hide the page. The visibility should be updated to 'hidden'.
    test->hideView();
    test->waitUntilTitleChangedTo("hidden");

    value = test->runJavaScriptAndWaitUntilFinished("document.visibilityState;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "hidden");

    value = test->runJavaScriptAndWaitUntilFinished("document.hidden;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));
}

static void testWebViewDocumentFocus(WebViewTest* test, gconstpointer)
{
    if (!g_strcmp0(g_getenv("UNDER_XVFB"), "yes")) {
        g_test_skip("This tests doesn't work under Xvfb");
        return;
    }

    test->showInWindow();
    test->loadHtml("<html><title></title>"
        "<body onload='document.getElementById(\"editable\").focus()'>"
        "<input id='editable'></input>"
        "<script>"
        "document.addEventListener(\"visibilitychange\", onVisibilityChange, false);"
        "function onVisibilityChange() {"
        "    document.title = document.visibilityState;"
        "}"
        "</script>"
        "</body></html>",
        nullptr);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    JSCValue* value = test->runJavaScriptAndWaitUntilFinished("document.hasFocus();", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));

    // Hide the view to make it lose the focus, the window is still the active one though.
    test->hideView();
    test->waitUntilTitleChangedTo("hidden");
    value = test->runJavaScriptAndWaitUntilFinished("document.hasFocus();", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_false(WebViewTest::javascriptResultToBoolean(value));
}

#if PLATFORM(GTK)
class SnapshotWebViewTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SnapshotWebViewTest);

#if !USE(GTK4)
    ~SnapshotWebViewTest()
    {
        if (m_snapshot)
            cairo_surface_destroy(m_snapshot);
    }
#endif

    static void onSnapshotReady(WebKitWebView* webView, GAsyncResult* result, SnapshotWebViewTest* test)
    {
        GUniqueOutPtr<GError> error;
#if USE(GTK4)
        test->m_snapshot = adoptGRef(webkit_web_view_get_snapshot_finish(webView, result, &error.outPtr()));
#else
        test->m_snapshot = webkit_web_view_get_snapshot_finish(webView, result, &error.outPtr());
#endif
        g_assert_true(!test->m_snapshot || !error.get());
        if (error)
            g_assert_error(error.get(), WEBKIT_SNAPSHOT_ERROR, WEBKIT_SNAPSHOT_ERROR_FAILED_TO_CREATE);
        test->quitMainLoop();
    }

#if USE(GTK4)
    GdkTexture* getSnapshotAndWaitUntilReady(WebKitSnapshotRegion region, WebKitSnapshotOptions options)
#else
    cairo_surface_t* getSnapshotAndWaitUntilReady(WebKitSnapshotRegion region, WebKitSnapshotOptions options)
#endif
    {
#if !USE(GTK4)
        if (m_snapshot)
            cairo_surface_destroy(m_snapshot);
#endif
        m_snapshot = nullptr;
        webkit_web_view_get_snapshot(m_webView, region, options, nullptr, reinterpret_cast<GAsyncReadyCallback>(onSnapshotReady), this);
        g_main_loop_run(m_mainLoop);
#if USE(GTK4)
        return m_snapshot.get();
#else
        return m_snapshot;
#endif
    }

    static void onSnapshotCancelledReady(WebKitWebView* webView, GAsyncResult* result, SnapshotWebViewTest* test)
    {
        GUniqueOutPtr<GError> error;
#if USE(GTK4)
        test->m_snapshot = adoptGRef(webkit_web_view_get_snapshot_finish(webView, result, &error.outPtr()));
#else
        test->m_snapshot = webkit_web_view_get_snapshot_finish(webView, result, &error.outPtr());
#endif
        g_assert_null(test->m_snapshot);
        g_assert_error(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED);
        test->quitMainLoop();
    }

    gboolean getSnapshotAndCancel()
    {
#if !USE(GTK4)
        if (m_snapshot)
            cairo_surface_destroy(m_snapshot);
#endif
        m_snapshot = nullptr;
        GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());
        webkit_web_view_get_snapshot(m_webView, WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE, cancellable.get(), reinterpret_cast<GAsyncReadyCallback>(onSnapshotCancelledReady), this);
        g_cancellable_cancel(cancellable.get());
        g_main_loop_run(m_mainLoop);

        return true;
    }

#if USE(GTK4)
    static cairo_surface_t* snapshotToSurface(GdkTexture* snapshot)
    {
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gdk_texture_get_width(snapshot), gdk_texture_get_height(snapshot));
        gdk_texture_download(snapshot, cairo_image_surface_get_data(surface), cairo_image_surface_get_stride(surface));
        cairo_surface_mark_dirty(surface);
        return surface;
    }
#else
    static cairo_surface_t* snapshotToSurface(cairo_surface_t* snapshot)
    {
        return cairo_surface_reference(snapshot);
    }
#endif

#if USE(GTK4)
    GRefPtr<GdkTexture> m_snapshot;
#else
    cairo_surface_t* m_snapshot { nullptr };
#endif
};

static void testWebViewSnapshot(SnapshotWebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><head><style>html { width: 200px; height: 100px; } ::-webkit-scrollbar { display: none; }</style></head><body><p>Whatever</p></body></html>", nullptr);
    test->waitUntilLoadFinished();

    // WEBKIT_SNAPSHOT_REGION_VISIBLE returns a null snapshot when the view is not visible.
    auto* snapshot1 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE);
    g_assert_null(snapshot1);

    // WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT works even if the window is not visible.
    snapshot1 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT, WEBKIT_SNAPSHOT_OPTIONS_NONE);
    g_assert_nonnull(snapshot1);
#if USE(GTK4)
    g_assert_true(GDK_IS_MEMORY_TEXTURE(snapshot1));
    g_assert_cmpint(gdk_texture_get_width(snapshot1), ==, 200);
    g_assert_cmpint(gdk_texture_get_height(snapshot1), ==, 100);
#else
    g_assert_cmpuint(cairo_surface_get_type(snapshot1), ==, CAIRO_SURFACE_TYPE_IMAGE);
    g_assert_cmpint(cairo_image_surface_get_width(snapshot1), ==, 200);
    g_assert_cmpint(cairo_image_surface_get_height(snapshot1), ==, 100);
#endif

    // Show the WebView in a popup widow of 50x50 and try again with WEBKIT_SNAPSHOT_REGION_VISIBLE.
    test->showInWindow(50, 50);
    snapshot1 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE);
    g_assert_nonnull(snapshot1);
    auto* surface1 = SnapshotWebViewTest::snapshotToSurface(snapshot1);
#if USE(GTK4)
    GRefPtr<GdkTexture> protectSnapshot1 = snapshot1;
    g_assert_true(GDK_IS_MEMORY_TEXTURE(snapshot1));
    g_assert_cmpint(gdk_texture_get_width(snapshot1), ==, 50);
    g_assert_cmpint(gdk_texture_get_height(snapshot1), ==, 50);
#else
    g_assert_cmpuint(cairo_surface_get_type(snapshot1), ==, CAIRO_SURFACE_TYPE_IMAGE);
    g_assert_cmpint(cairo_image_surface_get_width(snapshot1), ==, 50);
    g_assert_cmpint(cairo_image_surface_get_height(snapshot1), ==, 50);
#endif

    // Select all text in the WebView, request a snapshot ignoring selection.
    test->selectAll();
    auto* snapshot2 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE);
    g_assert_nonnull(snapshot2);
    auto* surface2 = SnapshotWebViewTest::snapshotToSurface(snapshot2);
    g_assert_true(Test::cairoSurfacesEqual(surface1, surface2));
    cairo_surface_destroy(surface2);

    // Request a new snapshot, including the selection this time. The size should be the same but the result
    // must be different to the one previously obtained.
    snapshot2 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING);
    g_assert_nonnull(snapshot2);
    surface2 = SnapshotWebViewTest::snapshotToSurface(snapshot2);
#if USE(GTK4)
    g_assert_true(GDK_IS_MEMORY_TEXTURE(snapshot2));
    g_assert_cmpint(gdk_texture_get_width(snapshot1), ==, gdk_texture_get_width(snapshot2));
    g_assert_cmpint(gdk_texture_get_height(snapshot1), ==, gdk_texture_get_height(snapshot2));
#else
    g_assert_cmpuint(cairo_surface_get_type(snapshot2), ==, CAIRO_SURFACE_TYPE_IMAGE);
    g_assert_cmpint(cairo_image_surface_get_width(snapshot1), ==, cairo_image_surface_get_width(snapshot2));
    g_assert_cmpint(cairo_image_surface_get_height(snapshot1), ==, cairo_image_surface_get_height(snapshot2));
#endif
    g_assert_false(Test::cairoSurfacesEqual(surface1, surface2));
    cairo_surface_destroy(surface2);

    // Get a snpashot with a transparent background, the result must be different.
    snapshot2 = test->getSnapshotAndWaitUntilReady(WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND);
    g_assert_nonnull(snapshot2);
    surface2 = SnapshotWebViewTest::snapshotToSurface(snapshot2);
#if USE(GTK4)
    g_assert_true(GDK_IS_MEMORY_TEXTURE(snapshot2));
    g_assert_cmpint(gdk_texture_get_width(snapshot1), ==, gdk_texture_get_width(snapshot2));
    g_assert_cmpint(gdk_texture_get_height(snapshot1), ==, gdk_texture_get_height(snapshot2));
#else
    g_assert_cmpuint(cairo_surface_get_type(snapshot2), ==, CAIRO_SURFACE_TYPE_IMAGE);
    g_assert_cmpint(cairo_image_surface_get_width(snapshot1), ==, cairo_image_surface_get_width(snapshot2));
    g_assert_cmpint(cairo_image_surface_get_height(snapshot1), ==, cairo_image_surface_get_height(snapshot2));
#endif
    g_assert_false(Test::cairoSurfacesEqual(surface1, surface2));
    cairo_surface_destroy(surface2);
    cairo_surface_destroy(surface1);

    // Test that cancellation works.
    g_assert_true(test->getSnapshotAndCancel());
}
#endif // PLATFORM(GTK)

#if ENABLE(NOTIFICATIONS)
class NotificationWebViewTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(NotificationWebViewTest, setup, teardown);

    static void setup()
    {
        WebViewTest::shouldInitializeWebViewInConstructor = false;
    }

    static void teardown()
    {
        WebViewTest::shouldInitializeWebViewInConstructor = true;
    }

    enum NotificationEvent {
        None,
        Permission,
        Shown,
        Clicked,
        OnClicked,
        Closed,
        OnClosed,
    };

    static gboolean permissionRequestCallback(WebKitWebView*, WebKitPermissionRequest *request, NotificationWebViewTest* test)
    {
        g_assert_true(WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request));
        g_assert_true(test->m_isExpectingPermissionRequest);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        test->m_event = Permission;

        webkit_permission_request_allow(request);

        g_main_loop_quit(test->m_mainLoop);

        return TRUE;
    }

    static gboolean notificationClosedCallback(WebKitNotification* notification, NotificationWebViewTest* test)
    {
        g_assert_true(test->m_notification == notification);
        test->m_notification = nullptr;
        test->m_event = Closed;
        if (g_main_loop_is_running(test->m_mainLoop))
            g_main_loop_quit(test->m_mainLoop);
        return TRUE;
    }

    static gboolean notificationClickedCallback(WebKitNotification* notification, NotificationWebViewTest* test)
    {
        g_assert_true(test->m_notification == notification);
        test->m_event = Clicked;
        return TRUE;
    }

    static gboolean showNotificationCallback(WebKitWebView*, WebKitNotification* notification, NotificationWebViewTest* test)
    {
        g_assert_null(test->m_notification);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(notification));
        test->m_notification = notification;
        g_signal_connect(notification, "closed", G_CALLBACK(notificationClosedCallback), test);
        g_signal_connect(notification, "clicked", G_CALLBACK(notificationClickedCallback), test);
        test->m_event = Shown;
        g_main_loop_quit(test->m_mainLoop);
        return TRUE;
    }

#if ENABLE(2022_GLIB_API)
    static void notificationsMessageReceivedCallback(WebKitUserContentManager* userContentManager, JSCValue* result, NotificationWebViewTest* test)
#else
    static void notificationsMessageReceivedCallback(WebKitUserContentManager* userContentManager, WebKitJavascriptResult* result, NotificationWebViewTest* test)
#endif
    {
        GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(result));

        if (g_str_equal(valueString.get(), "clicked"))
            test->m_event = OnClicked;
        else if (g_str_equal(valueString.get(), "closed"))
            test->m_event = OnClosed;

        g_main_loop_quit(test->m_mainLoop);
    }

    void initialize()
    {
        initializeWebView();
        g_signal_connect(m_webView, "permission-request", G_CALLBACK(permissionRequestCallback), this);
        g_signal_connect(m_webView, "show-notification", G_CALLBACK(showNotificationCallback), this);
#if !ENABLE(2022_GLIB_API)
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "notifications");
#else
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "notifications", nullptr);
#endif
        g_signal_connect(m_userContentManager.get(), "script-message-received::notifications", G_CALLBACK(notificationsMessageReceivedCallback), this);
    }

    ~NotificationWebViewTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        g_signal_handlers_disconnect_matched(m_userContentManager.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);

#if !ENABLE(2022_GLIB_API)
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "notifications");
#else
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "notifications", nullptr);
#endif
    }

    bool hasPermission()
    {
        auto* result = runJavaScriptAndWaitUntilFinished("Notification.permission;", nullptr);
        g_assert_nonnull(result);
        GUniquePtr<char> value(javascriptResultToCString(result));
        return !g_strcmp0(value.get(), "granted");
    }

    void requestPermissionAndWaitUntilGiven()
    {
        m_event = None;
        m_isExpectingPermissionRequest = true;
        runJavaScriptAndWait("Notification.requestPermission();");
    }

    void requestNotificationAndWaitUntilShown(const char* title, const char* body)
    {
        m_event = None;

        GUniquePtr<char> jscode(g_strdup_printf("n = new Notification('%s', { body: '%s'});", title, body));
        runJavaScriptAndWait(jscode.get());
    }

    void requestNotificationAndWaitUntilShown(const char* title, const char* body, const char* tag)
    {
        m_event = None;

        GUniquePtr<char> jscode(g_strdup_printf("n = new Notification('%s', { body: '%s', tag: '%s'});", title, body, tag));
        runJavaScriptAndWait(jscode.get());
    }

    void clickNotificationAndWaitUntilClicked()
    {
        m_event = None;
        runJavaScriptAndWaitUntilFinished("n.onclick = function() { window.webkit.messageHandlers.notifications.postMessage('clicked'); }", nullptr);
        webkit_notification_clicked(m_notification);
        g_assert_cmpint(m_event, ==, Clicked);
        g_main_loop_run(m_mainLoop);
    }

    void closeNotificationAndWaitUntilClosed()
    {
        m_event = None;
        runJavaScriptAndWait("n.close()");
    }

    void closeNotificationAndWaitUntilOnClosed()
    {
        g_assert_nonnull(m_notification);
        m_event = None;
        runJavaScriptAndWaitUntilFinished("n.onclose = function() { window.webkit.messageHandlers.notifications.postMessage('closed'); }", nullptr);
        webkit_notification_close(m_notification);
        g_assert_cmpint(m_event, ==, Closed);
        g_main_loop_run(m_mainLoop);
    }

    NotificationEvent m_event { None };
    WebKitNotification* m_notification { nullptr };
    bool m_isExpectingPermissionRequest { false };
    bool m_hasPermission { false };
};

static void testWebViewNotification(NotificationWebViewTest* test, gconstpointer)
{
    test->initialize();

    // Notifications don't work with local or special schemes.
    test->loadURI(gServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_false(test->hasPermission());

    test->requestPermissionAndWaitUntilGiven();
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Permission);
    g_assert_true(test->hasPermission());

    static const char* title = "This is a notification";
    static const char* body = "This is the body.";
    static const char* tag = "This is the tag.";
    test->requestNotificationAndWaitUntilShown(title, body, tag);

    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Shown);
    g_assert_nonnull(test->m_notification);
    g_assert_cmpstr(webkit_notification_get_title(test->m_notification), ==, title);
    g_assert_cmpstr(webkit_notification_get_body(test->m_notification), ==, body);
    g_assert_cmpstr(webkit_notification_get_tag(test->m_notification), ==, tag);

    test->clickNotificationAndWaitUntilClicked();
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::OnClicked);

    test->closeNotificationAndWaitUntilClosed();
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Closed);

    test->requestNotificationAndWaitUntilShown(title, body);
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Shown);
    g_assert_cmpstr(webkit_notification_get_tag(test->m_notification), ==, nullptr);

    test->closeNotificationAndWaitUntilOnClosed();
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::OnClosed);

    // The first notification should be closed automatically because the tag is
    // the same. It will crash in showNotificationCallback on failure.
    test->requestNotificationAndWaitUntilShown(title, body, tag);
    test->requestNotificationAndWaitUntilShown(title, body, tag);
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Shown);
}

static void setInitialNotificationPermissionsAllowedCallback(WebKitWebContext* context, NotificationWebViewTest* test)
{
    GList* allowedOrigins = g_list_prepend(nullptr, webkit_security_origin_new_for_uri(gServer->baseURL().string().utf8().data()));
    webkit_web_context_initialize_notification_permissions(test->m_webContext.get(), allowedOrigins, nullptr);
    g_list_free_full(allowedOrigins, reinterpret_cast<GDestroyNotify>(webkit_security_origin_unref));
}

static void setInitialNotificationPermissionsDisallowedCallback(WebKitWebContext* context, NotificationWebViewTest* test)
{
    GList* disallowedOrigins = g_list_prepend(nullptr, webkit_security_origin_new_for_uri(gServer->baseURL().string().utf8().data()));
    webkit_web_context_initialize_notification_permissions(test->m_webContext.get(), nullptr, disallowedOrigins);
    g_list_free_full(disallowedOrigins, reinterpret_cast<GDestroyNotify>(webkit_security_origin_unref));
}

static void testWebViewNotificationInitialPermissionAllowed(NotificationWebViewTest* test, gconstpointer)
{
    g_signal_connect(test->m_webContext.get(), "initialize-notification-permissions", G_CALLBACK(setInitialNotificationPermissionsAllowedCallback), test);
    test->initialize();

    test->loadURI(gServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_true(test->hasPermission());

    test->requestNotificationAndWaitUntilShown("This is a notification", "This is the body.");
    g_assert_cmpint(test->m_event, ==, NotificationWebViewTest::Shown);
}

static void testWebViewNotificationInitialPermissionDisallowed(NotificationWebViewTest* test, gconstpointer)
{
    g_signal_connect(test->m_webContext.get(), "initialize-notification-permissions", G_CALLBACK(setInitialNotificationPermissionsDisallowedCallback), test);
    test->initialize();

    test->loadURI(gServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_false(test->hasPermission());
}
#endif // ENABLE(NOTIFICATIONS)

static void testWebViewIsPlayingAudio(IsPlayingAudioWebViewTest* test, gconstpointer)
{
    // The web view must be realized for the video to start playback and
    // trigger changes in WebKitWebView::is-playing-audio.
    test->showInWindow();

    // Initially, web views should always report no audio being played.
    g_assert_false(webkit_web_view_is_playing_audio(test->m_webView));
    g_assert_false(webkit_web_view_get_is_muted(test->m_webView));

    GUniquePtr<char> resourcePath(g_build_filename(Test::getResourcesDir(Test::WebKit2Resources).data(), "file-with-video.html", nullptr));
    GUniquePtr<char> resourceURL(g_filename_to_uri(resourcePath.get(), nullptr, nullptr));
    webkit_web_view_load_uri(test->m_webView, resourceURL.get());
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_playing_audio(test->m_webView));

    test->runJavaScriptAndWaitUntilFinished("playVideo();", nullptr);
    if (!webkit_web_view_is_playing_audio(test->m_webView))
        test->waitUntilIsPlayingAudioChanged();
    g_assert_true(webkit_web_view_is_playing_audio(test->m_webView));

    // Mute the page, webkit_web_view_is_playing_audio() should still return TRUE.
    webkit_web_view_set_is_muted(test->m_webView, TRUE);
    g_assert_true(webkit_web_view_get_is_muted(test->m_webView));
    test->periodicallyCheckIsPlayingForAWhile();
    g_assert_true(webkit_web_view_is_playing_audio(test->m_webView));
    webkit_web_view_set_is_muted(test->m_webView, FALSE);
    g_assert_false(webkit_web_view_get_is_muted(test->m_webView));
    g_assert_true(webkit_web_view_is_playing_audio(test->m_webView));

    // Pause the video, and check again.
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('test-video').pause();", nullptr);
    if (webkit_web_view_is_playing_audio(test->m_webView))
        test->waitUntilIsPlayingAudioChanged();
    g_assert_false(webkit_web_view_is_playing_audio(test->m_webView));
}

static void testWebViewIsAudioMuted(WebViewTest* test, gconstpointer)
{
    g_assert_false(webkit_web_view_get_is_muted(test->m_webView));
    webkit_web_view_set_is_muted(test->m_webView, TRUE);
    g_assert_true(webkit_web_view_get_is_muted(test->m_webView));
    webkit_web_view_set_is_muted(test->m_webView, FALSE);
    g_assert_false(webkit_web_view_get_is_muted(test->m_webView));
}

static void testWebViewAutoplayPolicy(WebViewTest* test, gconstpointer)
{
    WebKitWebsitePolicies* policies = webkit_web_view_get_website_policies(test->m_webView);
    g_assert_cmpint(webkit_website_policies_get_autoplay_policy(policies), ==, WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND);
}

static void testWebViewIsWebProcessResponsive(WebViewTest* test, gconstpointer)
{
    static const char* hangHTML =
        "<html>"
        " <body>"
        "  <script>"
        "   setTimeout(function() {"
        "    var start = new Date().getTime();"
        "    var end = start;"
        "     while(end < start + 4000) {"
        "      end = new Date().getTime();"
        "     }"
        "    }, 500);"
        "  </script>"
        " </body>"
        "</html>";

    g_assert_true(webkit_web_view_get_is_web_process_responsive(test->m_webView));
    test->loadHtml(hangHTML, nullptr);
    test->waitUntilLoadFinished();
    // Wait 1 second, so the js while loop kicks in and blocks the web process. Then try to load a new
    // page. As the web process is busy this won't work, and after 3 seconds the web process will be marked
    // as unresponsive.
    test->wait(1);
    test->loadHtml("<html></html>", nullptr);
    test->waitUntilIsWebProcessResponsiveChanged();
    g_assert_false(webkit_web_view_get_is_web_process_responsive(test->m_webView));
    // 500ms after the web process is marked as unresponsive, the js while loop will finish and the process
    // will be responsive again, finishing the pending load.
    test->waitUntilLoadFinished();
    g_assert_true(webkit_web_view_get_is_web_process_responsive(test->m_webView));
}

static void testWebViewBackgroundColor(WebViewTest* test, gconstpointer)
{
#if PLATFORM(GTK)
#define ColorType GdkRGBA
#elif PLATFORM(WPE)
#define ColorType WebKitColor
#endif

    // White is the default background.
    ColorType rgba;
    webkit_web_view_get_background_color(test->m_webView, &rgba);
    g_assert_cmpfloat(rgba.red, ==, 1);
    g_assert_cmpfloat(rgba.green, ==, 1);
    g_assert_cmpfloat(rgba.blue, ==, 1);
    g_assert_cmpfloat(rgba.alpha, ==, 1);

    // Set a different (semi-transparent red).
    rgba.red = 1;
    rgba.green = 0;
    rgba.blue = 0;
    rgba.alpha = 0.5;
    webkit_web_view_set_background_color(test->m_webView, &rgba);
    g_assert_cmpfloat(rgba.red, ==, 1);
    g_assert_cmpfloat(rgba.green, ==, 0);
    g_assert_cmpfloat(rgba.blue, ==, 0);
    g_assert_cmpfloat(rgba.alpha, ==, 0.5);

#if PLATFORM(WPE)
    ColorType color;
    g_assert(webkit_color_parse(&color, "red"));
    g_assert_cmpfloat(color.red, ==, 1);
    webkit_web_view_set_background_color(test->m_webView, &color);
    webkit_web_view_get_background_color(test->m_webView, &rgba);
    g_assert_cmpfloat(rgba.red, ==, 1);
    g_assert_cmpfloat(rgba.green, ==, 0);
    g_assert_cmpfloat(rgba.blue, ==, 0);
    g_assert_cmpfloat(rgba.alpha, ==, 1);
#endif

    // The actual rendering can't be tested using unit tests, use
    // MiniBrowser --bg-color="<color-value>" for manually testing this API.
}

#if PLATFORM(GTK)
static void testWebViewPreferredSize(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html style='width: 325px; height: 615px'></html>", nullptr);
    test->waitUntilLoadFinished();
    test->showInWindow();
    GtkRequisition minimunSize, naturalSize;
    gtk_widget_get_preferred_size(GTK_WIDGET(test->m_webView), &minimunSize, &naturalSize);
    g_assert_cmpint(minimunSize.width, ==, 0);
    g_assert_cmpint(minimunSize.height, ==, 0);
    g_assert_cmpint(naturalSize.width, ==, 325);
    g_assert_cmpint(naturalSize.height, ==, 615);
}
#endif

class WebViewTitleTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebViewTitleTest);

    static void titleChangedCallback(WebKitWebView* view, GParamSpec*, WebViewTitleTest* test)
    {
        test->m_webViewTitles.append(webkit_web_view_get_title(view));
    }

    WebViewTitleTest()
    {
        g_signal_connect(m_webView, "notify::title", G_CALLBACK(titleChangedCallback), this);
    }

    Vector<CString> m_webViewTitles;
};

static void testWebViewTitleChange(WebViewTitleTest* test, gconstpointer)
{
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 0);

    test->loadHtml("<head><title>Page Title</title></head>", nullptr);
    test->waitUntilTitleChanged();
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 1);
    g_assert_cmpstr(test->m_webViewTitles[0].data(), ==, "Page Title");

    test->loadHtml("<head><title>Another Page Title</title></head>", nullptr);
    test->waitUntilTitleChanged();
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 2);
    g_assert_cmpstr(test->m_webViewTitles[1].data(), ==, "");
    test->waitUntilTitleChanged();
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 3);
    /* Page title should be immediately unset when loading a new page. */
    g_assert_cmpstr(test->m_webViewTitles[2].data(), ==, "Another Page Title");

    test->loadHtml("<p>This page has no title!</p>", nullptr);
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 4);
    g_assert_cmpstr(test->m_webViewTitles[3].data(), ==, "");

    test->loadHtml("<script>document.title = 'one'; document.title = 'two'; document.title = 'three';</script>", nullptr);
    test->waitUntilTitleChanged();
    g_assert_cmpint(test->m_webViewTitles.size(), ==, 5);
    g_assert_cmpstr(test->m_webViewTitles[4].data(), ==, "three");
}

#if PLATFORM(WPE)
class FrameDisplayedTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FrameDisplayedTest);

    static void titleChangedCallback(WebKitWebView* view, GParamSpec*, WebViewTitleTest* test)
    {
        test->m_webViewTitles.append(webkit_web_view_get_title(view));
    }

    FrameDisplayedTest()
        : m_id(webkit_web_view_add_frame_displayed_callback(m_webView, [](WebKitWebView*, gpointer userData) {
            auto* test = static_cast<FrameDisplayedTest*>(userData);
            if (!test->m_maxFrames)
                return;

            if (++test->m_frameCounter == test->m_maxFrames)
                RunLoop::main().dispatch([test] { test->quitMainLoop(); });
        }, this, nullptr))
    {
        g_assert_cmpuint(m_id, >, 0);
    }

    ~FrameDisplayedTest()
    {
        webkit_web_view_remove_frame_displayed_callback(m_webView, m_id);
    }

    void waitUntilFramesDisplayed(unsigned framesCount = 1)
    {
        m_maxFrames = framesCount;
        m_frameCounter = 0;
        g_main_loop_run(m_mainLoop);
    }

    unsigned m_id { 0 };
    unsigned m_frameCounter { 0 };
    unsigned m_maxFrames { 0 };
};

static void testWebViewFrameDisplayed(FrameDisplayedTest* test, gconstpointer)
{
    test->showInWindow();

    test->loadHtml("<html></html>", nullptr);
    test->waitUntilFramesDisplayed();

    test->loadHtml("<html><head><style>@keyframes fadeIn { from { opacity: 0; } }</style></head><p style='animation: fadeIn 1s infinite alternate;'>Foo</p></html>", nullptr);
    test->waitUntilFramesDisplayed(10);

    bool secondCallbackCalled = false;
    auto id = webkit_web_view_add_frame_displayed_callback(test->m_webView, [](WebKitWebView*, gpointer userData) {
        auto* secondCallbackCalled = static_cast<bool*>(userData);
        *secondCallbackCalled = true;
    }, &secondCallbackCalled, nullptr);
    test->waitUntilFramesDisplayed();
    g_assert_true(secondCallbackCalled);

    secondCallbackCalled = false;
    webkit_web_view_remove_frame_displayed_callback(test->m_webView, id);
    test->waitUntilFramesDisplayed();
    g_assert_false(secondCallbackCalled);

    id = webkit_web_view_add_frame_displayed_callback(test->m_webView, [](WebKitWebView* webView, gpointer userData) {
        auto* id = static_cast<unsigned*>(userData);
        webkit_web_view_remove_frame_displayed_callback(webView, *id);
    }, &id, [](gpointer userData) {
        auto* id = static_cast<unsigned*>(userData);
        *id = 0;
    });
    test->waitUntilFramesDisplayed();
    g_assert_cmpuint(id, ==, 0);

    auto id2 = webkit_web_view_add_frame_displayed_callback(test->m_webView, [](WebKitWebView* webView, gpointer userData) {
        auto* id = static_cast<unsigned*>(userData);
        if (*id) {
            webkit_web_view_remove_frame_displayed_callback(webView, *id);
            *id = 0;
        }
    }, &id, nullptr);

    secondCallbackCalled = false;
    id = webkit_web_view_add_frame_displayed_callback(test->m_webView, [](WebKitWebView* webView, gpointer userData) {
        auto* secondCallbackCalled = static_cast<bool*>(userData);
        *secondCallbackCalled = true;
    }, &secondCallbackCalled, nullptr);
    test->waitUntilFramesDisplayed();
    g_assert_cmpuint(id, ==, 0);
    g_assert_false(secondCallbackCalled);

    webkit_web_view_remove_frame_displayed_callback(test->m_webView, id2);
}
#endif

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
enum class RenderingState {
    Unknown,
    Started,
    Paused,
    Stopped
};

class AudioRenderingWebViewTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(AudioRenderingWebViewTest, setup, teardown);

    static void setup()
    {
    }

    static void teardown()
    {
        wpe_audio_register_receiver(nullptr, nullptr);
    }

    AudioRenderingWebViewTest()
    {
        wpe_audio_register_receiver(&m_audioReceiver, this);
    }

    void handleStart(uint32_t id, int32_t channels, const char* layout, int32_t sampleRate)
    {
        g_assert(m_state == RenderingState::Unknown);
        g_assert_false(m_streamId.has_value());
        g_assert_cmpuint(id, ==, 0);
        m_streamId = id;
        m_state = RenderingState::Started;
        g_assert_cmpint(channels, ==, 2);
        g_assert_cmpstr(layout, ==, "S16LE");
        g_assert_cmpint(sampleRate, ==, 44100);
    }

    void handleStop(uint32_t id)
    {
        g_assert_cmpuint(*m_streamId, ==, id);
        g_assert(m_state != RenderingState::Unknown);
        m_state = RenderingState::Stopped;
        g_main_loop_quit(m_mainLoop);
        m_streamId.reset();
    }

    void handlePause(uint32_t id)
    {
        g_assert_cmpuint(*m_streamId, ==, id);
        g_assert(m_state != RenderingState::Unknown);
        m_state = RenderingState::Paused;
    }

    void handleResume(uint32_t id)
    {
        g_assert_cmpuint(*m_streamId, ==, id);
        g_assert(m_state == RenderingState::Paused);
        m_state = RenderingState::Started;
    }

    void handlePacket(struct wpe_audio_packet_export* packet_export, uint32_t id, int32_t fd, uint32_t size)
    {
        g_assert_cmpuint(*m_streamId, ==, id);
        g_assert(m_state == RenderingState::Started || m_state == RenderingState::Paused);
        g_assert_cmpuint(size, >, 0);
        wpe_audio_packet_export_release(packet_export);
    }

    void waitUntilStarted()
    {
        g_timeout_add(200, [](gpointer userData) -> gboolean {
            auto* test = static_cast<AudioRenderingWebViewTest*>(userData);
            if (test->state() == RenderingState::Started) {
                test->quitMainLoop();
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }, this);
        g_main_loop_run(m_mainLoop);
    }

    void waitUntilPaused()
    {
        g_timeout_add(200, [](gpointer userData) -> gboolean {
            auto* test = static_cast<AudioRenderingWebViewTest*>(userData);
            if (test->state() == RenderingState::Paused) {
                test->quitMainLoop();
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }, this);
        g_main_loop_run(m_mainLoop);
    }

    void waitUntilEOS()
    {
        g_main_loop_run(m_mainLoop);
    }

    RenderingState state() const { return m_state; }

private:
    static const struct wpe_audio_receiver m_audioReceiver;
    RenderingState m_state { RenderingState::Unknown };
    std::optional<uint32_t> m_streamId;
};

const struct wpe_audio_receiver AudioRenderingWebViewTest::m_audioReceiver = {
    [](void* data, uint32_t id, int32_t channels, const char* layout, int32_t sampleRate) { static_cast<AudioRenderingWebViewTest*>(data)->handleStart(id, channels, layout, sampleRate); },
    [](void* data, struct wpe_audio_packet_export* packet_export, uint32_t id, int32_t fd, uint32_t size) { static_cast<AudioRenderingWebViewTest*>(data)->handlePacket(packet_export, id, fd, size); },
    [](void* data, uint32_t id) { static_cast<AudioRenderingWebViewTest*>(data)->handleStop(id); },
    [](void* data, uint32_t id) { static_cast<AudioRenderingWebViewTest*>(data)->handlePause(id); },
    [](void* data, uint32_t id) { static_cast<AudioRenderingWebViewTest*>(data)->handleResume(id); }
};

static void testWebViewExternalAudioRendering(AudioRenderingWebViewTest* test, gconstpointer)
{
    GUniquePtr<char> resourcePath(g_build_filename(Test::getResourcesDir(Test::WebKit2Resources).data(), "file-with-video.html", nullptr));
    GUniquePtr<char> resourceURL(g_filename_to_uri(resourcePath.get(), nullptr, nullptr));
    webkit_web_view_load_uri(test->m_webView, resourceURL.get());
    test->waitUntilLoadFinished();

    test->runJavaScriptAndWaitUntilFinished("playVideo();", nullptr);
    test->waitUntilStarted();
    g_assert(test->state() == RenderingState::Started);
    test->runJavaScriptAndWaitUntilFinished("pauseVideo();", nullptr);
    test->waitUntilPaused();
    g_assert(test->state() == RenderingState::Paused);

    test->runJavaScriptAndWaitUntilFinished("playVideo(); seekNearTheEnd();", nullptr);
    test->waitUntilEOS();
    g_assert(test->state() == RenderingState::Stopped);
}
#endif

class WebViewTerminateWebProcessTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebViewTerminateWebProcessTest);

    static void webProcessTerminatedCallback(WebKitWebView* webView, WebKitWebProcessTerminationReason reason, WebViewTerminateWebProcessTest* test)
    {
        test->m_terminationReason = reason;
    }

    WebViewTerminateWebProcessTest()
    {
        g_signal_connect_after(m_webView, "web-process-terminated", G_CALLBACK(WebViewTerminateWebProcessTest::webProcessTerminatedCallback), this);
    }

    ~WebViewTerminateWebProcessTest()
    {
        g_signal_handlers_disconnect_by_data(m_webView, this);
    }

    WebKitWebProcessTerminationReason m_terminationReason { WEBKIT_WEB_PROCESS_CRASHED };
};

static void testWebViewTerminateWebProcess(WebViewTerminateWebProcessTest* test, gconstpointer)
{
    test->loadHtml("<html></html>", nullptr);
    test->waitUntilLoadFinished();
    test->m_expectedWebProcessCrash = true;
    webkit_web_view_terminate_web_process(test->m_webView);
    g_assert_cmpuint(test->m_terminationReason, ==, WEBKIT_WEB_PROCESS_TERMINATED_BY_API);
    g_assert_true(webkit_web_view_get_is_web_process_responsive(test->m_webView));
}

static void testWebViewTerminateUnresponsiveWebProcess(WebViewTerminateWebProcessTest* test, gconstpointer)
{
    static const char* hangHTML =
        "<html>"
        " <body>"
        "  <script>"
        "   setTimeout(function() {"
        "     while(true) { }"
        "    }, 500);"
        "  </script>"
        " </body>"
        "</html>";

    test->loadHtml(hangHTML, nullptr);
    test->waitUntilLoadFinished();
    g_assert_true(webkit_web_view_get_is_web_process_responsive(test->m_webView));
    // Wait 1 second, so the js while loop kicks in and blocks the web process, and try to load a new page.
    // As the web process is busy this won't work, and after 3 seconds the web process will be marked
    // as unresponsive.
    test->wait(1);
    test->loadHtml("<html></html>", nullptr);
    test->waitUntilIsWebProcessResponsiveChanged();
    g_assert_false(webkit_web_view_get_is_web_process_responsive(test->m_webView));

    // Now that the process is unresponsive, terminate it.
    test->m_expectedWebProcessCrash = true;
    test->m_terminationReason = WEBKIT_WEB_PROCESS_CRASHED;
    webkit_web_view_terminate_web_process(test->m_webView);
    g_assert_cmpuint(test->m_terminationReason, ==, WEBKIT_WEB_PROCESS_TERMINATED_BY_API);
    g_assert_true(webkit_web_view_get_is_web_process_responsive(test->m_webView));
}

static void testWebViewCORSAllowlist(WebViewTest* test, gconstpointer)
{
    webkit_web_context_register_uri_scheme(test->m_webContext.get(), "foo",
        [](WebKitURISchemeRequest* request, gpointer userData) {
            GRefPtr<GInputStream> inputStream = adoptGRef(g_memory_input_stream_new());
            const char* data = "<p>foobar!</p>";
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), data, strlen(data), nullptr);
            webkit_uri_scheme_request_finish(request, inputStream.get(), strlen(data), "text/html");
        }, nullptr, nullptr);

    char html[] = "<html><script>let foo = 0; fetch('foo://bar/baz').then(response => { foo = response.status; }).catch(err => { foo = -1; });</script></html>";

    auto waitForFooChanged = [&test]() {
        GUniqueOutPtr<GError> error;
        JSCValue* jscvalue;
        int value;
        do {
            jscvalue = test->runJavaScriptAndWaitUntilFinished("foo;", &error.outPtr());
            g_assert_no_error(error.get());
            value = jsc_value_to_int32(jscvalue);
        } while (!value);
        return value;
    };

    // Request is not allowed, foo should be 0.
    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, -1);

    // Allowlisting host alone does not work. Path is also required. foo should remain 0.
    GUniquePtr<char*> allowlist(g_new(char*, 2));
    allowlist.get()[0] = g_strdup("foo://*");
    allowlist.get()[1] = nullptr;
    webkit_web_view_set_cors_allowlist(test->m_webView, allowlist.get());

    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, -1);

    // Finally let's properly allow our scheme. foo should now change to 42 when the request succeeds.
    allowlist.reset(g_new(char*, 2));
    allowlist.get()[0] = g_strdup("foo://*/*");
    allowlist.get()[1] = nullptr;
    webkit_web_view_set_cors_allowlist(test->m_webView, allowlist.get());

    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, 200);
}

static void testWebViewDefaultContentSecurityPolicy(WebViewTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    JSCValue* value;

    // Sanity check that eval works normally.
    value = test->runJavaScriptAndWaitUntilFinished("eval('\"allowed\"')", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    GUniquePtr<char> evalValue(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(evalValue.get(), ==, "allowed");

    // Create a new web view with a policy that blocks eval().
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "default-content-security-policy", "script-src 'self'",
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        nullptr));

    // Ensure JavaScript still functions.
    value = test->runJavaScriptAndWaitUntilFinished("'allowed'", &error.outPtr(), webView.get());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    GUniquePtr<char> strValue(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(strValue.get(), ==, "allowed");

    // Then ensure eval is blocked.
    value = test->runJavaScriptAndWaitUntilFinished("eval('\"allowed\"')", &error.outPtr(), webView.get());
    g_assert_null(value);
    g_assert_error(error.get(), WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED);
}

static void testWebViewWebExtensionMode(WebViewTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    JSCValue* value;
    static const char* html =
        "<html>"
        "  <head>"
        "    <title>unset</title>"
        "    <meta http-equiv=\"Content-Security-Policy\" content=\"script-src 'unsafe-inline';\">"
        "    <script>document.title = 'set';</script>"
        "  </head>"
        "</html>";

    // Sanity check that this HTML works as expected.
    test->loadHtml(html, nullptr);
    test->waitUntilLoadFinished();
    value = test->runJavaScriptAndWaitUntilFinished("document.title == 'set';", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));

    // Create a new web view with an extension mode that blocks the unsafe-inline keyword.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-extension-mode", WEBKIT_WEB_EXTENSION_MODE_MANIFESTV3,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        nullptr));
    test->loadHtml(html, nullptr, webView.get());
    test->waitUntilLoadFinished(webView.get());
    value = test->runJavaScriptAndWaitUntilFinished("document.title == 'unset';", &error.outPtr(), webView.get());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_true(WebViewTest::javascriptResultToBoolean(value));
}

static void testWebViewDisableWebSecurity(WebViewTest* test, gconstpointer)
{
    webkit_web_context_register_uri_scheme(test->m_webContext.get(), "foo",
        [](WebKitURISchemeRequest* request, gpointer userData) {
            GRefPtr<GInputStream> inputStream = adoptGRef(g_memory_input_stream_new());
            const char* data = "<p>foobar!</p>";
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), data, strlen(data), nullptr);
            webkit_uri_scheme_request_finish(request, inputStream.get(), strlen(data), "text/html");
        }, nullptr, nullptr);

    char html[] = "<html><script>let foo = 0; fetch('foo://bar/baz').then(response => { foo = response.status; }).catch(err => { foo = -1; });</script></html>";

    auto waitForFooChanged = [&test]() {
        int fooValue;
        do {
            GUniqueOutPtr<GError> error;
            JSCValue* jscvalue = test->runJavaScriptAndWaitUntilFinished("foo;", &error.outPtr());
            g_assert_no_error(error.get());
            fooValue = jsc_value_to_int32(jscvalue);
        } while (!fooValue);
        return fooValue;
    };

    // By default web security is enabled, request is not allowed, foo should be -1.
    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, -1);

    WebKitSettings* settings = webkit_web_view_get_settings(test->m_webView);
    // Disable web security, now we can request forbidden content
    webkit_settings_set_disable_web_security(settings, TRUE);

    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, 200);
}

static void testWebViewEnableHTML5Database(WebViewTest* test, gconstpointer)
{
    char html[] = "<html><script>let foo = 0; try { indexedDB.open(\"library\"); foo = 200; } catch { foo = -1; };</script></html>";

    auto waitForFooChanged = [&test]() {
        int fooValue;
        do {
            GUniqueOutPtr<GError> error;
            JSCValue* jscvalue = test->runJavaScriptAndWaitUntilFinished("foo;", &error.outPtr());
            g_assert_no_error(error.get());
            fooValue = jsc_value_to_int32(jscvalue);
        } while (!fooValue);
        return fooValue;
    };

    // By default indexedDB API is enabled, there should not be an exception after calling indexedDB API, foo should be 200.
    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, 200);

    WebKitSettings* settings = webkit_web_view_get_settings(test->m_webView);
    // Disable indexedDB API, any call with indexedDB API will throw an exception, so foo should be -1.
    webkit_settings_set_enable_html5_database(settings, FALSE);

    webkit_web_view_load_html(test->m_webView, html, "http://example.com");
    test->waitUntilLoadFinished();
    g_assert_cmpint(waitForFooChanged(), ==, -1);
}

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

    if (g_str_equal(path, "/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        soup_message_body_complete(soup_server_message_get_response_body(message));
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

void beforeAll()
{
    gServer = new WebKitTestServer();
    gServer->run(serverCallback);

    WebViewTest::add("WebKitWebView", "web-context", testWebViewWebContext);
    WebViewTest::add("WebKitWebView", "web-context-lifetime", testWebViewWebContextLifetime);
    WebViewTest::add("WebKitWebView", "close-quickly", testWebViewCloseQuickly);
#if PLATFORM(WPE)
    Test::add("WebKitWebView", "backend", testWebViewWebBackend);
#endif
    WebViewTest::add("WebKitWebView", "ephemeral", testWebViewEphemeral);
    WebViewTest::add("WebKitWebView", "custom-charset", testWebViewCustomCharset);
    WebViewTest::add("WebKitWebView", "settings", testWebViewSettings);
    WebViewTest::add("WebKitWebView", "zoom-level", testWebViewZoomLevel);
    WebViewTest::add("WebKitWebView", "run-javascript", testWebViewRunJavaScript);
    WebViewTest::add("WebKitWebView", "run-async-js-functions", testWebViewRunAsyncFunctions);
#if ENABLE(FULLSCREEN_API)
    FullScreenClientTest::add("WebKitWebView", "fullscreen", testWebViewFullScreen);
#endif
    WebViewTest::add("WebKitWebView", "can-show-mime-type", testWebViewCanShowMIMEType);
    // FIXME: implement mouse clicks in WPE.
#if PLATFORM(GTK)
    FormClientTest::add("WebKitWebView", "submit-form", testWebViewSubmitForm);
#endif
    SaveWebViewTest::add("WebKitWebView", "save", testWebViewSave);
    // FIXME: View is initially visible in WPE and has a fixed hardcoded size.
#if PLATFORM(GTK)
    SnapshotWebViewTest::add("WebKitWebView", "snapshot", testWebViewSnapshot);
#endif
    WebViewTest::add("WebKitWebView", "page-visibility", testWebViewPageVisibility);
    WebViewTest::add("WebKitWebView", "document-focus", testWebViewDocumentFocus);
#if ENABLE(NOTIFICATIONS)
    NotificationWebViewTest::add("WebKitWebView", "notification", testWebViewNotification);
    NotificationWebViewTest::add("WebKitWebView", "notification-initial-permission-allowed", testWebViewNotificationInitialPermissionAllowed);
    NotificationWebViewTest::add("WebKitWebView", "notification-initial-permission-disallowed", testWebViewNotificationInitialPermissionDisallowed);
#endif
    IsPlayingAudioWebViewTest::add("WebKitWebView", "is-playing-audio", testWebViewIsPlayingAudio);
    WebViewTest::add("WebKitWebView", "background-color", testWebViewBackgroundColor);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitWebView", "preferred-size", testWebViewPreferredSize);
#endif
    WebViewTitleTest::add("WebKitWebView", "title-change", testWebViewTitleChange);
#if PLATFORM(WPE)
    FrameDisplayedTest::add("WebKitWebView", "frame-displayed", testWebViewFrameDisplayed);
#endif
    WebViewTest::add("WebKitWebView", "is-audio-muted", testWebViewIsAudioMuted);
    WebViewTest::add("WebKitWebView", "autoplay-policy", testWebViewAutoplayPolicy);
#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
    AudioRenderingWebViewTest::add("WebKitWebView", "external-audio-rendering", testWebViewExternalAudioRendering);
#endif
    WebViewTest::add("WebKitWebView", "is-web-process-responsive", testWebViewIsWebProcessResponsive);
    WebViewTerminateWebProcessTest::add("WebKitWebView", "terminate-web-process", testWebViewTerminateWebProcess);
    WebViewTerminateWebProcessTest::add("WebKitWebView", "terminate-unresponsive-web-process", testWebViewTerminateUnresponsiveWebProcess);
    WebViewTest::add("WebKitWebView", "cors-allowlist", testWebViewCORSAllowlist);
    WebViewTest::add("WebKitWebView", "default-content-security-policy", testWebViewDefaultContentSecurityPolicy);
    WebViewTest::add("WebKitWebView", "web-extension-mode", testWebViewWebExtensionMode);
    WebViewTest::add("WebKitWebView", "disable-web-security", testWebViewDisableWebSecurity);
    WebViewTest::add("WebKitWebView", "enable-html5-database", testWebViewEnableHTML5Database);
}

void afterAll()
{
}
