/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "WebKitTestBus.h"
#include "WebViewTest.h"
#include <wtf/gobject/GRefPtr.h>

static const char* webExtensionsUserData = "Web Extensions user data";
static WebKitTestBus* bus;

static void testWebExtensionGetTitle(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><head><title>WebKitGTK+ Web Extensions Test</title></head><body></body></html>", 0);
    test->waitUntilLoadFinished();

    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest" , "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "GetTitle",
        g_variant_new("(t)", webkit_web_view_get_page_id(test->m_webView)),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert(result);

    const char* title;
    g_variant_get(result.get(), "(&s)", &title);
    g_assert_cmpstr(title, ==, "WebKitGTK+ Web Extensions Test");
}

static void documentLoadedCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant*, WebViewTest* test)
{
    g_main_loop_quit(test->m_mainLoop);
}

static void testDocumentLoadedSignal(WebViewTest* test, gconstpointer)
{
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GDBusConnection* connection = g_dbus_proxy_get_connection(proxy.get());
    guint id = g_dbus_connection_signal_subscribe(connection,
        0,
        "org.webkit.gtk.WebExtensionTest",
        "DocumentLoaded",
        "/org/webkit/gtk/WebExtensionTest",
        0,
        G_DBUS_SIGNAL_FLAGS_NONE,
        reinterpret_cast<GDBusSignalCallback>(documentLoadedCallback),
        test,
        0);
    g_assert(id);

    test->loadHtml("<html><head><title>WebKitGTK+ Web Extensions Test</title></head><body></body></html>", 0);
    g_main_loop_run(test->m_mainLoop);
    g_dbus_connection_signal_unsubscribe(connection, id);
}

static gboolean webProcessCrashedCallback(WebKitWebView*, WebViewTest* test)
{
    test->quitMainLoop();

    return FALSE;
}

static void testWebKitWebViewProcessCrashed(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html></html>", 0);
    test->waitUntilLoadFinished();

    g_signal_connect(test->m_webView, "web-process-crashed",
        G_CALLBACK(webProcessCrashedCallback), test);

    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "AbortProcess",
        0,
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert(!result);
    g_main_loop_run(test->m_mainLoop);
}

static void testWebExtensionWindowObjectCleared(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><header></header><body></body></html>", 0);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("window.echo('Foo');", &error.outPtr());
    g_assert(javascriptResult);
    g_assert(!error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "Foo");
}

static gboolean scriptDialogCallback(WebKitWebView*, WebKitScriptDialog* dialog, char** result)
{
    g_assert_cmpuint(webkit_script_dialog_get_dialog_type(dialog), ==, WEBKIT_SCRIPT_DIALOG_ALERT);
    g_assert(!*result);
    *result = g_strdup(webkit_script_dialog_get_message(dialog));
    return TRUE;
}

static void runJavaScriptInIsolatedWorldFinishedCallback(GDBusProxy* proxy, GAsyncResult* result, WebViewTest* test)
{
    g_dbus_proxy_call_finish(proxy, result, 0);
    g_main_loop_quit(test->m_mainLoop);
}

static void testWebExtensionIsolatedWorld(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><header></header><body><div id='console'></div></body></html>", 0);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<char> result;
    gulong scriptDialogID = g_signal_connect(test->m_webView, "script-dialog", G_CALLBACK(scriptDialogCallback), &result.outPtr());

    static const char* mainWorldScript =
        "top.foo = 'Foo';\n"
        "document.getElementById('console').innerHTML = top.foo;\n"
        "window.open = function () { alert('Main World'); }\n"
        "document.open(1, 2, 3);";
    test->runJavaScriptAndWaitUntilFinished(mainWorldScript, 0);
    g_assert_cmpstr(result.get(), ==, "Main World");

    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementById('console').innerHTML", 0);
    g_assert(javascriptResult);
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "Foo");

    static const char* isolatedWorldScript =
        "document.getElementById('console').innerHTML = top.foo;\n"
        "window.open = function () { alert('Isolated World'); }\n"
        "document.open(1, 2, 3);";
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest" , "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    g_dbus_proxy_call(proxy.get(),
        "RunJavaScriptInIsolatedWorld",
        g_variant_new("(t&s)", webkit_web_view_get_page_id(test->m_webView), isolatedWorldScript),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0,
        reinterpret_cast<GAsyncReadyCallback>(runJavaScriptInIsolatedWorldFinishedCallback),
        test);
    g_main_loop_run(test->m_mainLoop);
    g_assert_cmpstr(result.get(), ==, "Isolated World");

    // Check that 'top.foo' defined in main world is not visible in isolated world.
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementById('console').innerHTML", 0);
    g_assert(javascriptResult);
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "undefined");

    g_signal_handler_disconnect(test->m_webView, scriptDialogID);
}

static void testWebExtensionInitializationUserData(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html></html>", 0);
    test->waitUntilLoadFinished();

    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy("org.webkit.gtk.WebExtensionTest",
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "GetInitializationUserData",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert(result);

    const gchar* userData = nullptr;
    g_variant_get(result.get(), "(&s)", &userData);
    g_assert_cmpstr(userData, ==, webExtensionsUserData);
}

static void initializeWebExtensions(WebKitWebContext* context, gpointer)
{
    webkit_web_context_set_web_extensions_directory(context, WEBKIT_TEST_WEB_EXTENSIONS_DIR);
    webkit_web_context_set_web_extensions_initialization_user_data(context,
        g_variant_new("&s", webExtensionsUserData));
}

void beforeAll()
{
    g_signal_connect(webkit_web_context_get_default(),
        "initialize-web-extensions", G_CALLBACK(initializeWebExtensions), nullptr);

    bus = new WebKitTestBus();
    if (!bus->run())
        return;

    WebViewTest::add("WebKitWebContext", "initialization-user-data", testWebExtensionInitializationUserData);
    WebViewTest::add("WebKitWebExtension", "dom-document-title", testWebExtensionGetTitle);
    WebViewTest::add("WebKitWebExtension", "document-loaded-signal", testDocumentLoadedSignal);
    WebViewTest::add("WebKitWebView", "web-process-crashed", testWebKitWebViewProcessCrashed);
    WebViewTest::add("WebKitWebExtension", "window-object-cleared", testWebExtensionWindowObjectCleared);
    WebViewTest::add("WebKitWebExtension", "isolated-world", testWebExtensionIsolatedWorld);
}

void afterAll()
{
    delete bus;
}
