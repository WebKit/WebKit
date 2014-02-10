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
#include "WebKitTestBus.h"
#include <webkit2/webkit2.h>
#include <wtf/Vector.h>

static const unsigned numViews = 2;
static guint32 nextInitializationId = 1;
static unsigned initializeWebExtensionsSignalCount;
static WebKitTestBus* bus;

class MultiprocessTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(MultiprocessTest);

    MultiprocessTest()
        : m_mainLoop(g_main_loop_new(nullptr, TRUE))
        , m_webViewBusNames(numViews)
        , m_webViews(numViews) { }

    static void loadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, MultiprocessTest* test)
    {
        if (loadEvent != WEBKIT_LOAD_FINISHED)
            return;
        g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadChanged), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    void loadWebViewAndWaitUntilLoaded(unsigned index)
    {
        g_assert_cmpuint(index, <, numViews);

        m_webViewBusNames[index] = GUniquePtr<char>(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", nextInitializationId));

        m_webViews[index] = WEBKIT_WEB_VIEW(webkit_web_view_new());
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webViews[index].get()));

        webkit_web_view_load_html(m_webViews[index].get(), "<html></html>", nullptr);
        g_signal_connect(m_webViews[index].get(), "load-changed", G_CALLBACK(loadChanged), this);
        g_main_loop_run(m_mainLoop);
    }

    unsigned webProcessPid(unsigned index)
    {
        g_assert_cmpuint(index, <, numViews);

        GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(m_webViewBusNames[index].get(),
            "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", m_mainLoop));

        GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
            proxy.get(),
            "GetProcessIdentifier",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, nullptr));
        g_assert(result);

        guint32 identifier = 0;
        g_variant_get(result.get(), "(u)", &identifier);
        return identifier;
    }

    GMainLoop* m_mainLoop;
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
        test->loadWebViewAndWaitUntilLoaded(i);
        g_assert(WEBKIT_IS_WEB_VIEW(test->m_webViews[i].get()));
        g_assert(test->m_webViewBusNames[i]);
    }

    g_assert_cmpuint(initializeWebExtensionsSignalCount, ==, numViews);
    g_assert_cmpstr(test->m_webViewBusNames[0].get(), !=, test->m_webViewBusNames[1].get());
    g_assert_cmpuint(test->webProcessPid(0), !=, test->webProcessPid(1));
}

static void initializeWebExtensions(WebKitWebContext* context, gpointer)
{
    initializeWebExtensionsSignalCount++;
    webkit_web_context_set_web_extensions_directory(context, WEBKIT_TEST_WEB_EXTENSIONS_DIR);
    webkit_web_context_set_web_extensions_initialization_user_data(context,
        g_variant_new_uint32(nextInitializationId++));
}

void beforeAll()
{
    g_signal_connect(webkit_web_context_get_default(),
        "initialize-web-extensions", G_CALLBACK(initializeWebExtensions), nullptr);

    // Check that default setting is the one stated in the documentation
    g_assert_cmpuint(webkit_web_context_get_process_model(webkit_web_context_get_default()),
        ==, WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS);

    webkit_web_context_set_process_model(webkit_web_context_get_default(),
        WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

    // Check that the getter returns the newly-set value
    g_assert_cmpuint(webkit_web_context_get_process_model(webkit_web_context_get_default()),
        ==, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

    bus = new WebKitTestBus();
    if (!bus->run())
        return;

    MultiprocessTest::add("WebKitWebContext", "process-per-web-view", testProcessPerWebView);
}

void afterAll()
{
    delete bus;
    g_signal_handlers_disconnect_by_func(webkit_web_context_get_default(),
        reinterpret_cast<void*>(initializeWebExtensions), nullptr);
}
