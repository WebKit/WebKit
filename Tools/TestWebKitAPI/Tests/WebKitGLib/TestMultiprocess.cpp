/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "TestMain.h"
#include "WebViewTest.h"
#include <wtf/Vector.h>

static const unsigned numViews = 2;

class MultiprocessTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(MultiprocessTest);

    MultiprocessTest()
        : m_mainLoop(g_main_loop_new(nullptr, TRUE))
        , m_initializeWebExtensionsSignalCount(0)
        , m_webViewBusNames(numViews)
        , m_webViews(numViews)
    {
        webkit_web_context_set_process_model(m_webContext.get(), WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
    }

    void initializeWebExtensions() override
    {
        Test::initializeWebExtensions();
        m_initializeWebExtensionsSignalCount++;
    }

    static void loadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, MultiprocessTest* test)
    {
        if (loadEvent != WEBKIT_LOAD_FINISHED)
            return;
        g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadChanged), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    void loadWebViewAndWaitUntilPageCreated(unsigned index)
    {
        g_assert_cmpuint(index, <, numViews);

        m_webViews[index] = Test::adoptView(Test::createWebView(m_webContext.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webViews[index].get()));

        webkit_web_view_load_html(m_webViews[index].get(), "<html></html>", nullptr);
        g_idle_add([](gpointer userData) -> gboolean {
            auto* test = static_cast<MultiprocessTest*>(userData);
            if (!s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(test->m_webViews[test->m_initializeWebExtensionsSignalCount - 1].get())))
                return TRUE;

            g_main_loop_quit(test->m_mainLoop);
            return FALSE;
        }, this);
        g_main_loop_run(m_mainLoop);
    }

    unsigned webProcessPid(unsigned index)
    {
        g_assert_cmpuint(index, <, numViews);

        GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_sync(s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(m_webViews[index].get())),
            static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
            nullptr, nullptr, "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", nullptr, nullptr));
        GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
            proxy.get(),
            "GetProcessIdentifier",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, nullptr));
        g_assert_nonnull(result);

        guint32 identifier = 0;
        g_variant_get(result.get(), "(u)", &identifier);
        return identifier;
    }

#if PLATFORM(GTK)
    void destroyWebViewAndWaitUntilWebProcessFinishes(unsigned index)
    {
        g_assert_cmpuint(index, <, numViews);

        auto* connection = s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(m_webViews[index].get()));
        g_assert_nonnull(connection);
        g_signal_connect_swapped(connection, "closed", G_CALLBACK(g_main_loop_quit), m_mainLoop);
#if USE(GTK4)
        g_object_run_dispose(G_OBJECT(m_webViews[index].get()));
#else
        gtk_widget_destroy(GTK_WIDGET(m_webViews[index].get()));
#endif
        g_main_loop_run(m_mainLoop);
    }
#endif

    GMainLoop* m_mainLoop;
    unsigned m_initializeWebExtensionsSignalCount;
    Vector<GUniquePtr<char>, numViews> m_webViewBusNames;
    Vector<GRefPtr<WebKitWebView>, numViews> m_webViews;
};

static void testProcessPerWebView(MultiprocessTest* test, gconstpointer)
{
    // Create two web views. As we are in multiprocess mode, there must be
    // two web processes, running an instance of the web extension each.
    // The initialize-web-extensions must have been called twice, and the
    // identifiers generated for them must be different (and their reported
    // process identifiers).

    for (unsigned i = 0; i < numViews; i++) {
        test->loadWebViewAndWaitUntilPageCreated(i);
        g_assert_true(WEBKIT_IS_WEB_VIEW(test->m_webViews[i].get()));
        g_assert_nonnull(Test::s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(test->m_webViews[i].get())));
    }

    g_assert_cmpuint(test->m_initializeWebExtensionsSignalCount, ==, numViews);
    g_assert_false(Test::s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(test->m_webViews[0].get())) == Test::s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(test->m_webViews[1].get())));
    g_assert_cmpuint(test->webProcessPid(0), !=, test->webProcessPid(1));

#if PLATFORM(GTK)
    // Check that web processes finish when the web view is destroyed even when it's not finalized.
    // See https://bugs.webkit.org/show_bug.cgi?id=129783.
    for (unsigned i = 0; i < numViews; i++) {
        GRefPtr<WebKitWebView> webView = test->m_webViews[i];
        test->destroyWebViewAndWaitUntilWebProcessFinishes(i);
    }
#endif
}

class UIClientMultiprocessTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(UIClientMultiprocessTest);

    enum WebViewEvents {
        Create,
        ReadyToShow,
        Close
    };

    static WebKitWebView* viewCreateCallback(WebKitWebView* webView, WebKitNavigationAction*, UIClientMultiprocessTest* test)
    {
        return test->viewCreate(webView);
    }

    static void viewReadyToShowCallback(WebKitWebView* webView, UIClientMultiprocessTest* test)
    {
        test->viewReadyToShow(webView);
    }

    static void viewCloseCallback(WebKitWebView* webView, UIClientMultiprocessTest* test)
    {
        test->viewClose(webView);
    }

    UIClientMultiprocessTest()
        : m_mainLoop(g_main_loop_new(nullptr, TRUE))
        , m_initializeWebExtensionsSignalCount(0)
    {
        webkit_web_context_set_process_model(m_webContext.get(), WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
        m_webView = WEBKIT_WEB_VIEW(Test::createWebView(m_webContext.get()));
#if PLATFORM(GTK)
        g_object_ref_sink(m_webView);
#endif
        webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(m_webView), TRUE);

        g_signal_connect(m_webView, "create", G_CALLBACK(viewCreateCallback), this);
    }

    ~UIClientMultiprocessTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        g_object_unref(m_webView);
    }

    void initializeWebExtensions() override
    {
        Test::initializeWebExtensions();
        m_initializeWebExtensionsSignalCount++;
    }

    WebKitWebView* viewCreate(WebKitWebView* webView)
    {
        g_assert_true(webView == m_webView);

        auto* newWebView = Test::createWebView(webView);
#if PLATFORM(GTK)
        g_object_ref_sink(newWebView);
#endif
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newWebView));
        m_webViewEvents.append(Create);

        g_signal_connect(newWebView, "ready-to-show", G_CALLBACK(viewReadyToShowCallback), this);
        g_signal_connect(newWebView, "close", G_CALLBACK(viewCloseCallback), this);

        return WEBKIT_WEB_VIEW(newWebView);
    }

    void viewReadyToShow(WebKitWebView* webView)
    {
        g_assert_true(m_webView != webView);
        m_webViewEvents.append(ReadyToShow);
    }

    void viewClose(WebKitWebView* webView)
    {
        g_assert_true(m_webView != webView);

        m_webViewEvents.append(Close);
        g_object_unref(webView);
        g_main_loop_quit(m_mainLoop);
    }

    void waitUntilNewWebViewClose()
    {
        g_main_loop_run(m_mainLoop);
    }

    WebKitWebView* m_webView;
    GMainLoop* m_mainLoop;
    unsigned m_initializeWebExtensionsSignalCount;
    Vector<WebViewEvents> m_webViewEvents;
};

static void testMultiprocessWebViewCreateReadyClose(UIClientMultiprocessTest* test, gconstpointer)
{
    webkit_web_view_load_html(test->m_webView, "<html><body onLoad=\"window.open().close();\"></html>", nullptr);
    test->waitUntilNewWebViewClose();

    Vector<UIClientMultiprocessTest::WebViewEvents>& events = test->m_webViewEvents;
    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, UIClientMultiprocessTest::Create);
    g_assert_cmpint(events[1], ==, UIClientMultiprocessTest::ReadyToShow);
    g_assert_cmpint(events[2], ==, UIClientMultiprocessTest::Close);

    g_assert_cmpuint(test->m_initializeWebExtensionsSignalCount, ==, 1);
}

void beforeAll()
{
    // Check that default setting is the one stated in the documentation
    g_assert_cmpuint(webkit_web_context_get_process_model(webkit_web_context_get_default()), ==, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

    MultiprocessTest::add("WebKitWebContext", "process-per-web-view", testProcessPerWebView);
    UIClientMultiprocessTest::add("WebKitWebView", "multiprocess-create-ready-close", testMultiprocessWebViewCreateReadyClose);
}

void afterAll()
{
}
