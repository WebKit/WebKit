/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebViewTest.h"
#include <wtf/gobject/GRefPtr.h>

static void testWebViewDefaultContext(WebViewTest* test, gconstpointer)
{
    g_assert(webkit_web_view_get_context(test->m_webView) == webkit_web_context_get_default());
}

static void testWebViewCustomCharset(WebViewTest* test, gconstpointer)
{
    g_assert(!webkit_web_view_get_custom_charset(test->m_webView));
    webkit_web_view_set_custom_charset(test->m_webView, "utf8");
    g_assert_cmpstr(webkit_web_view_get_custom_charset(test->m_webView), ==, "utf8");
    // Go back to the default charset.
    webkit_web_view_set_custom_charset(test->m_webView, 0);
    g_assert(!webkit_web_view_get_custom_charset(test->m_webView));
}

static void testWebViewsShareClients(Test* test, gconstpointer)
{
    GRefPtr<GtkWidget> webView1 = webkit_web_view_new();
    GRefPtr<GtkWidget> webView2 = webkit_web_view_new();
    WebKitWebLoaderClient* client1 = webkit_web_view_get_loader_client(WEBKIT_WEB_VIEW(webView1.get()));
    WebKitWebLoaderClient* client2 = webkit_web_view_get_loader_client(WEBKIT_WEB_VIEW(webView2.get()));
    g_assert(client1);
    g_assert(client1 == client2);
}

static void testWebViewSettings(WebViewTest* test, gconstpointer)
{
    WebKitSettings* defaultSettings = webkit_web_view_get_settings(test->m_webView);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultSettings));
    g_assert(defaultSettings);
    g_assert(webkit_settings_get_enable_javascript(defaultSettings));

    GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
    g_object_set(G_OBJECT(newSettings.get()), "enable-javascript", FALSE, NULL);
    webkit_web_view_set_settings(test->m_webView, newSettings.get());

    WebKitSettings* settings = webkit_web_view_get_settings(test->m_webView);
    g_assert(settings != defaultSettings);
    g_assert(!webkit_settings_get_enable_javascript(settings));

    GRefPtr<GtkWidget> webView2 = webkit_web_view_new();
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webView2.get()));
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webView2.get()), settings);
    g_assert(webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView2.get())) == settings);

    GRefPtr<WebKitSettings> newSettings2 = adoptGRef(webkit_settings_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings2.get()));
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webView2.get()), newSettings2.get());
    settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView2.get()));
    g_assert(settings == newSettings2.get());
    g_assert(webkit_settings_get_enable_javascript(settings));
}

class UIClientTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(UIClientTest);

    enum WebViewEvents {
        Create,
        ReadyToShow,
        Close
    };

    static void viewClose(WebKitWebView* webView, UIClientTest* test)
    {
        g_assert(webView != test->m_webView);

        test->m_webViewEvents.append(Close);
        g_object_unref(webView);

        g_main_loop_quit(test->m_mainLoop);
    }

    static void viewReadyToShow(WebKitWebView* webView, UIClientTest* test)
    {
        g_assert(webView != test->m_webView);

        test->m_webViewEvents.append(ReadyToShow);
    }

    static GtkWidget* viewCreate(WebKitWebView* webView, UIClientTest* test)
    {
        g_assert(webView == test->m_webView);

        GtkWidget* newWebView = webkit_web_view_new_with_context(webkit_web_view_get_context(webView));
        g_object_ref_sink(newWebView);

        test->m_webViewEvents.append(Create);

        g_signal_connect(newWebView, "ready-to-show", G_CALLBACK(viewReadyToShow), test);
        g_signal_connect(newWebView, "close", G_CALLBACK(viewClose), test);

        return newWebView;
    }

    UIClientTest()
    {
        webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(m_webView), TRUE);
        g_signal_connect(m_webView, "create", G_CALLBACK(viewCreate), this);
    }

    ~UIClientTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void waitUntilMainLoopFinishes()
    {
        g_main_loop_run(m_mainLoop);
    }

    Vector<WebViewEvents> m_webViewEvents;
};

static void testWebViewCreateReadyClose(UIClientTest* test, gconstpointer)
{
    test->loadHtml("<html><body onLoad=\"window.open().close();\"></html>", 0);
    test->waitUntilMainLoopFinishes();

    Vector<UIClientTest::WebViewEvents>& events = test->m_webViewEvents;
    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, UIClientTest::Create);
    g_assert_cmpint(events[1], ==, UIClientTest::ReadyToShow);
    g_assert_cmpint(events[2], ==, UIClientTest::Close);
}

void beforeAll()
{
    WebViewTest::add("WebKitWebView", "default-context", testWebViewDefaultContext);
    WebViewTest::add("WebKitWebView", "custom-charset", testWebViewCustomCharset);
    Test::add("WebKitWebView", "webviews-share-clients", testWebViewsShareClients);
    WebViewTest::add("WebKitWebView", "settings", testWebViewSettings);
    UIClientTest::add("WebKitWebView", "create-ready-close", testWebViewCreateReadyClose);
}

void afterAll()
{
}
