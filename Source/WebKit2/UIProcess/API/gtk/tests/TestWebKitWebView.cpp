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
#include <wtf/HashSet.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/StringHash.h>

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

static const char* kAlertDialogMessage = "WebKitGTK+ alert dialog message";
static const char* kConfirmDialogMessage = "WebKitGTK+ confirm dialog message";
static const char* kPromptDialogMessage = "WebKitGTK+ prompt dialog message";
static const char* kPromptDialogReturnedText = "WebKitGTK+ prompt dialog returned text";

class UIClientTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(UIClientTest);

    enum WebViewEvents {
        Create,
        ReadyToShow,
        Close
    };

    enum ScriptType {
        Alert,
        Confirm,
        Prompt
    };

    class WindowProperties {
    public:
        WindowProperties()
            : m_toolbarVisible(true)
            , m_statusbarVisible(true)
            , m_scrollbarsVisible(true)
            , m_menubarVisible(true)
            , m_locationbarVisible(true)
            , m_resizable(true)
            , m_fullscreen(false)
        {
            memset(&m_geometry, 0, sizeof(GdkRectangle));
        }

        WindowProperties(WebKitWindowProperties* windowProperties)
            : m_toolbarVisible(webkit_window_properties_get_toolbar_visible(windowProperties))
            , m_statusbarVisible(webkit_window_properties_get_statusbar_visible(windowProperties))
            , m_scrollbarsVisible(webkit_window_properties_get_scrollbars_visible(windowProperties))
            , m_menubarVisible(webkit_window_properties_get_menubar_visible(windowProperties))
            , m_locationbarVisible(webkit_window_properties_get_locationbar_visible(windowProperties))
            , m_resizable(webkit_window_properties_get_resizable(windowProperties))
            , m_fullscreen(webkit_window_properties_get_fullscreen(windowProperties))
        {
            webkit_window_properties_get_geometry(windowProperties, &m_geometry);
        }

        WindowProperties(GdkRectangle* geometry, bool toolbarVisible, bool statusbarVisible, bool scrollbarsVisible, bool menubarVisible,
                         bool locationbarVisible, bool resizable, bool fullscreen)
            : m_geometry(*geometry)
            , m_toolbarVisible(toolbarVisible)
            , m_statusbarVisible(statusbarVisible)
            , m_scrollbarsVisible(scrollbarsVisible)
            , m_menubarVisible(menubarVisible)
            , m_locationbarVisible(locationbarVisible)
            , m_resizable(resizable)
            , m_fullscreen(fullscreen)
        {
        }

        bool operator==(const WindowProperties& other) const
        {
            return m_geometry.x == other.m_geometry.x
                && m_geometry.y == other.m_geometry.y
                && m_geometry.width == other.m_geometry.width
                && m_geometry.height == other.m_geometry.height
                && m_toolbarVisible == other.m_toolbarVisible
                && m_statusbarVisible == other.m_statusbarVisible
                && m_scrollbarsVisible == other.m_scrollbarsVisible
                && m_menubarVisible == other.m_menubarVisible
                && m_locationbarVisible == other.m_locationbarVisible
                && m_resizable == other.m_resizable
                && m_fullscreen == other.m_fullscreen;
        }

    private:
        GdkRectangle m_geometry;

        bool m_toolbarVisible;
        bool m_statusbarVisible;
        bool m_scrollbarsVisible;
        bool m_menubarVisible;
        bool m_locationbarVisible;

        bool m_resizable;
        bool m_fullscreen;
    };

    static void windowPropertiesNotifyCallback(GObject*, GParamSpec* paramSpec, UIClientTest* test)
    {
        test->m_windowPropertiesChanged.add(g_param_spec_get_name(paramSpec));
    }

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

        WebKitWindowProperties* windowProperties = webkit_web_view_get_window_properties(webView);
        g_assert(windowProperties);
        g_assert(WindowProperties(windowProperties) == test->m_windowProperties);

        test->m_webViewEvents.append(ReadyToShow);
    }

    static GtkWidget* viewCreate(WebKitWebView* webView, UIClientTest* test)
    {
        g_assert(webView == test->m_webView);

        GtkWidget* newWebView = webkit_web_view_new_with_context(webkit_web_view_get_context(webView));
        g_object_ref_sink(newWebView);

        test->m_webViewEvents.append(Create);

        WebKitWindowProperties* windowProperties = webkit_web_view_get_window_properties(WEBKIT_WEB_VIEW(newWebView));
        g_assert(windowProperties);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(windowProperties));
        test->m_windowPropertiesChanged.clear();
        g_signal_connect(windowProperties, "notify", G_CALLBACK(windowPropertiesNotifyCallback), test);

        g_signal_connect(newWebView, "ready-to-show", G_CALLBACK(viewReadyToShow), test);
        g_signal_connect(newWebView, "close", G_CALLBACK(viewClose), test);

        return newWebView;
    }

    static gboolean scriptAlert(WebKitWebView*, const char* message, UIClientTest* test)
    {
        switch (test->m_scriptType) {
        case UIClientTest::Alert:
            g_assert_cmpstr(message, ==, kAlertDialogMessage);
            break;
        case UIClientTest::Confirm:
            g_assert(test->m_scriptDialogConfirmed);
            g_assert_cmpstr(message, ==, "confirmed");

            break;
        case UIClientTest::Prompt:
            g_assert_cmpstr(message, ==, kPromptDialogReturnedText);
            break;
        }

        g_main_loop_quit(test->m_mainLoop);

        return TRUE;
    }

    static gboolean scriptConfirm(WebKitWebView*, const char* message, gboolean* confirmed, UIClientTest* test)
    {
        g_assert_cmpstr(message, ==, kConfirmDialogMessage);
        g_assert(confirmed);
        test->m_scriptDialogConfirmed = !test->m_scriptDialogConfirmed;
        *confirmed = test->m_scriptDialogConfirmed;

        return TRUE;
    }

    static gboolean scriptPrompt(WebKitWebView*, const char* message, const char* defaultText, char **text, UIClientTest* test)
    {
        g_assert_cmpstr(message, ==, kPromptDialogMessage);
        g_assert_cmpstr(defaultText, ==, "default");
        g_assert(text);
        *text = g_strdup(kPromptDialogReturnedText);

        return TRUE;
    }

    UIClientTest()
        : m_scriptType(Alert)
        , m_scriptDialogConfirmed(true)
    {
        webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(m_webView), TRUE);
        g_signal_connect(m_webView, "create", G_CALLBACK(viewCreate), this);
        g_signal_connect(m_webView, "script-alert", G_CALLBACK(scriptAlert), this);
        g_signal_connect(m_webView, "script-confirm", G_CALLBACK(scriptConfirm), this);
        g_signal_connect(m_webView, "script-prompt", G_CALLBACK(scriptPrompt), this);
    }

    ~UIClientTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void waitUntilMainLoopFinishes()
    {
        g_main_loop_run(m_mainLoop);
    }

    void setExpectedWindowProperties(const WindowProperties& windowProperties)
    {
        m_windowProperties = windowProperties;
    }

    Vector<WebViewEvents> m_webViewEvents;
    ScriptType m_scriptType;
    bool m_scriptDialogConfirmed;
    WindowProperties m_windowProperties;
    HashSet<WTF::String> m_windowPropertiesChanged;
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

static void testWebViewJavaScriptDialogs(UIClientTest* test, gconstpointer)
{
    static const char* htmlOnLoadFormat = "<html><body onLoad=\"%s\"></body></html>";
    static const char* jsAlertFormat = "alert('%s')";
    static const char* jsConfirmFormat = "do { confirmed = confirm('%s'); } while (!confirmed); alert('confirmed');";
    static const char* jsPromptFormat = "alert(prompt('%s', 'default'));";

    test->m_scriptType = UIClientTest::Alert;
    GOwnPtr<char> alertDialogMessage(g_strdup_printf(jsAlertFormat, kAlertDialogMessage));
    GOwnPtr<char> alertHTML(g_strdup_printf(htmlOnLoadFormat, alertDialogMessage.get()));
    test->loadHtml(alertHTML.get(), 0);
    test->waitUntilMainLoopFinishes();

    test->m_scriptType = UIClientTest::Confirm;
    GOwnPtr<char> confirmDialogMessage(g_strdup_printf(jsConfirmFormat, kConfirmDialogMessage));
    GOwnPtr<char> confirmHTML(g_strdup_printf(htmlOnLoadFormat, confirmDialogMessage.get()));
    test->loadHtml(confirmHTML.get(), 0);
    test->waitUntilMainLoopFinishes();

    test->m_scriptType = UIClientTest::Prompt;
    GOwnPtr<char> promptDialogMessage(g_strdup_printf(jsPromptFormat, kPromptDialogMessage));
    GOwnPtr<char> promptHTML(g_strdup_printf(htmlOnLoadFormat, promptDialogMessage.get()));
    test->loadHtml(promptHTML.get(), 0);
    test->waitUntilMainLoopFinishes();
}

static void testWebViewWindowProperties(UIClientTest* test, gconstpointer)
{
    static const char* windowProrpertiesString = "left=100,top=150,width=800,height=600,location=no,menubar=no,status=no,toolbar=no,scrollbars=no";
    GdkRectangle geometry = { 100, 150, 800, 600 };
    test->setExpectedWindowProperties(UIClientTest::WindowProperties(&geometry, false, false, false, false, false, true, false));

    GOwnPtr<char> htmlString(g_strdup_printf("<html><body onLoad=\"window.open('', '', '%s').close();\"></body></html>", windowProrpertiesString));
    test->loadHtml(htmlString.get(), 0);
    test->waitUntilMainLoopFinishes();

    static const char* propertiesChanged[] = {
        "geometry", "locationbar-visible", "menubar-visible", "statusbar-visible", "toolbar-visible", "scrollbars-visible"
    };
    for (size_t i = 0; i < G_N_ELEMENTS(propertiesChanged); ++i)
        g_assert(test->m_windowPropertiesChanged.contains(propertiesChanged[i]));

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
    UIClientTest::add("WebKitWebView", "javascript-dialogs", testWebViewJavaScriptDialogs);
    UIClientTest::add("WebKitWebView", "window-properties", testWebViewWindowProperties);
}

void afterAll()
{
}
