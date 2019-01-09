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
#include <wtf/glib/GRefPtr.h>

static WebKitTestBus* bus;
static GUniquePtr<char> scriptDialogResult;

#define INPUT_ID "input-id"
#define FORM_ID "form-id"
#define FORM2_ID "form2-id"

#define FORM_SUBMISSION_TEST_ID "form-submission-test-id"

static void testWebExtensionGetTitle(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><head><title>WebKitGTK+ Web Extensions Test</title></head><body></body></html>", 0);
    test->waitUntilLoadFinished();

    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "GetTitle",
        g_variant_new("(t)", webkit_web_view_get_page_id(test->m_webView)),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert_nonnull(result);

    const char* title;
    g_variant_get(result.get(), "(&s)", &title);
    g_assert_cmpstr(title, ==, "WebKitGTK+ Web Extensions Test");
}

#if PLATFORM(GTK)
static gboolean inputElementIsUserEdited(GDBusProxy* proxy, uint64_t pageID, const char* elementID)
{
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy,
        "InputElementIsUserEdited",
        g_variant_new("(t&s)", pageID, elementID),
        G_DBUS_CALL_FLAGS_NONE,
        -1, nullptr, nullptr));
    g_assert_nonnull(result);

    gboolean retval;
    g_variant_get(result.get(), "(b)", &retval);
    return retval;
}

static void testWebExtensionInputElementIsUserEdited(WebViewTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();
    test->loadHtml("<html><body id='body'><input id='input'></input><textarea id='textarea'></textarea></body></html>", nullptr);
    test->waitUntilLoadFinished();

    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));

    uint64_t pageID = webkit_web_view_get_page_id(test->m_webView);
    g_assert_false(inputElementIsUserEdited(proxy.get(), pageID, "input"));
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('input').focus()", nullptr);
    test->keyStroke(GDK_KEY_a);
    while (gtk_events_pending())
        gtk_main_iteration();
    GUniquePtr<char> resultString;
    do {
        auto* result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('input').value", nullptr);
        resultString.reset(WebViewTest::javascriptResultToCString(result));
    } while (g_strcmp0(resultString.get(), "a"));
    g_assert_true(inputElementIsUserEdited(proxy.get(), pageID, "input"));

    g_assert_false(inputElementIsUserEdited(proxy.get(), pageID, "textarea"));
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('textarea').focus()", nullptr);
    test->keyStroke(GDK_KEY_b);
    while (gtk_events_pending())
        gtk_main_iteration();
    do {
        auto* result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('textarea').value", nullptr);
        resultString.reset(WebViewTest::javascriptResultToCString(result));
    } while (g_strcmp0(resultString.get(), "b"));
    g_assert_true(inputElementIsUserEdited(proxy.get(), pageID, "textarea"));

    g_assert_false(inputElementIsUserEdited(proxy.get(), pageID, "body"));
}
#endif

static void documentLoadedCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant*, WebViewTest* test)
{
    g_main_loop_quit(test->m_mainLoop);
}

static void testDocumentLoadedSignal(WebViewTest* test, gconstpointer)
{
    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
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
    g_assert_cmpuint(id, !=, 0);

    test->loadHtml("<html><head><title>WebKitGTK+ Web Extensions Test</title></head><body></body></html>", 0);
    g_main_loop_run(test->m_mainLoop);
    g_dbus_connection_signal_unsubscribe(connection, id);
}

static gboolean webProcessTerminatedCallback(WebKitWebView*, WebKitWebProcessTerminationReason reason, WebViewTest* test)
{
    g_assert_cmpuint(reason, ==, WEBKIT_WEB_PROCESS_CRASHED);
    test->quitMainLoop();

    return FALSE;
}

static void testWebKitWebViewProcessCrashed(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html></html>", 0);
    test->waitUntilLoadFinished();

    g_signal_connect_after(test->m_webView, "web-process-terminated",
        G_CALLBACK(webProcessTerminatedCallback), test);

    test->m_expectedWebProcessCrash = true;

    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "AbortProcess",
        0,
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert_null(result);
    g_main_loop_run(test->m_mainLoop);
    test->m_expectedWebProcessCrash = false;
}

static void testWebExtensionWindowObjectCleared(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><header></header><body></body></html>", 0);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("window.echo('Foo');", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "Foo");
}

static gboolean scriptDialogCallback(WebKitWebView*, WebKitScriptDialog* dialog, gpointer)
{
    g_assert_cmpuint(webkit_script_dialog_get_dialog_type(dialog), ==, WEBKIT_SCRIPT_DIALOG_ALERT);
    scriptDialogResult.reset(g_strdup(webkit_script_dialog_get_message(dialog)));
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

    gulong scriptDialogID = g_signal_connect(test->m_webView, "script-dialog", G_CALLBACK(scriptDialogCallback), nullptr);

    static const char* mainWorldScript =
        "top.foo = 'Foo';\n"
        "document.getElementById('console').innerHTML = top.foo;\n"
        "window.open = function () { alert('Main World'); }\n"
        "window.open();";
    test->runJavaScriptAndWaitUntilFinished(mainWorldScript, 0);
    g_assert_cmpstr(scriptDialogResult.get(), ==, "Main World");

    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementById('console').innerHTML", 0);
    g_assert_nonnull(javascriptResult);
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "Foo");

    static const char* isolatedWorldScript =
        "document.getElementById('console').innerHTML = top.foo;\n"
        "window.open = function () { alert('Isolated World'); }\n"
        "window.open();";
    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest" , "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    g_dbus_proxy_call(proxy.get(),
        "RunJavaScriptInIsolatedWorld",
        g_variant_new("(t&s)", webkit_web_view_get_page_id(test->m_webView), isolatedWorldScript),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0,
        reinterpret_cast<GAsyncReadyCallback>(runJavaScriptInIsolatedWorldFinishedCallback),
        test);
    g_main_loop_run(test->m_mainLoop);
    g_assert_cmpstr(scriptDialogResult.get(), ==, "Isolated World");

    // Check that 'top.foo' defined in main world is not visible in isolated world.
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementById('console').innerHTML", 0);
    g_assert_nonnull(javascriptResult);
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "undefined");

    g_signal_handler_disconnect(test->m_webView, scriptDialogID);
}

#if PLATFORM(GTK)
static gboolean permissionRequestCallback(WebKitWebView*, WebKitPermissionRequest* request, WebViewTest* test)
{
    if (!WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request))
        return FALSE;

    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));
    WebKitInstallMissingMediaPluginsPermissionRequest* missingPluginsRequest = WEBKIT_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request);
    g_assert_nonnull(webkit_install_missing_media_plugins_permission_request_get_description(missingPluginsRequest));
    webkit_permission_request_deny(request);
    test->quitMainLoop();

    return TRUE;
}

static void testInstallMissingPluginsPermissionRequest(WebViewTest* test, gconstpointer)
{
    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(proxy.get(), "RemoveAVPluginsFromGSTRegistry",
        nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr));

    test->showInWindowAndWaitUntilMapped();

    gulong permissionRequestSignalID = g_signal_connect(test->m_webView, "permission-request", G_CALLBACK(permissionRequestCallback), test);
    // FIXME: the base URI needs to finish with / to work, that shouldn't happen.
    GUniquePtr<char> baseURI(g_strconcat("file://", Test::getResourcesDir(Test::WebKit2Resources).data(), "/", nullptr));
    test->loadHtml("<html><body><video src=\"test.mp4\" autoplay></video></body></html>", baseURI.get());
    g_main_loop_run(test->m_mainLoop);
    g_signal_handler_disconnect(test->m_webView, permissionRequestSignalID);
}
#endif // PLATFORM(GTK)

static void didAssociateFormControlsCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* result, WebViewTest* test)
{
    const char* formIds;
    g_variant_get(result, "(&s)", &formIds);
    g_assert_true(!g_strcmp0(formIds, FORM_ID FORM2_ID) || !g_strcmp0(formIds, FORM2_ID FORM_ID) || !g_strcmp0(formIds, INPUT_ID));

    test->quitMainLoop();
}

static void testWebExtensionFormControlsAssociated(WebViewTest* test, gconstpointer)
{
    GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
    GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
        "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", test->m_mainLoop));
    GDBusConnection* connection = g_dbus_proxy_get_connection(proxy.get());
    guint id = g_dbus_connection_signal_subscribe(connection,
        nullptr,
        "org.webkit.gtk.WebExtensionTest",
        "FormControlsAssociated",
        "/org/webkit/gtk/WebExtensionTest",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        reinterpret_cast<GDBusSignalCallback>(didAssociateFormControlsCallback),
        test,
        nullptr);
    g_assert_cmpuint(id, !=, 0);

    test->loadHtml("<!DOCTYPE html><head><title>WebKitGTK+ Web Extensions Test</title></head><div id=\"placeholder\"/>", 0);
    test->waitUntilLoadFinished();

    static const char* addFormScript =
        "var input = document.createElement(\"input\");"
        "input.id = \"" INPUT_ID "\";"
        "input.type = \"password\";"
        "var form = document.createElement(\"form\");"
        "form.id = \"" FORM_ID "\";"
        "form.appendChild(input);"
        "var form2 = document.createElement(\"form\");"
        "form2.id = \"" FORM2_ID "\";"
        "var placeholder = document.getElementById(\"placeholder\");"
        "placeholder.appendChild(form);"
        "placeholder.appendChild(form2);";

    webkit_web_view_run_javascript(test->m_webView, addFormScript, nullptr, nullptr, nullptr);
    g_main_loop_run(test->m_mainLoop);

    static const char* moveFormElementScript =
        "var form = document.getElementById(\"" FORM_ID "\");"
        "var form2 = document.getElementById(\"" FORM2_ID "\");"
        "var input = document.getElementById(\"" INPUT_ID "\");"
        "form.removeChild(input);"
        "form2.appendChild(input);";

    webkit_web_view_run_javascript(test->m_webView, moveFormElementScript, nullptr, nullptr, nullptr);
    g_main_loop_run(test->m_mainLoop);

    g_dbus_connection_signal_unsubscribe(connection, id);
}

class FormSubmissionTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FormSubmissionTest);

    FormSubmissionTest()
    {
        GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", s_webExtensionID));
        m_proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
            "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", m_mainLoop));
        GDBusConnection* connection = g_dbus_proxy_get_connection(m_proxy.get());

        m_willSendDOMEventCallbackID = g_dbus_connection_signal_subscribe(connection,
            nullptr,
            "org.webkit.gtk.WebExtensionTest",
            "FormSubmissionWillSendDOMEvent",
            "/org/webkit/gtk/WebExtensionTest",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            reinterpret_cast<GDBusSignalCallback>(willSendDOMEventCallback),
            this,
            nullptr);
        g_assert_cmpuint(m_willSendDOMEventCallbackID, !=, 0);

        m_willCompleteCallbackID = g_dbus_connection_signal_subscribe(connection,
            nullptr,
            "org.webkit.gtk.WebExtensionTest",
            "FormSubmissionWillComplete",
            "/org/webkit/gtk/WebExtensionTest",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            reinterpret_cast<GDBusSignalCallback>(willCompleteCallback),
            this,
            nullptr);
        g_assert_cmpuint(m_willCompleteCallbackID, !=, 0);
    }

    ~FormSubmissionTest()
    {
        GDBusConnection* connection = g_dbus_proxy_get_connection(m_proxy.get());
        g_dbus_connection_signal_unsubscribe(connection, m_willSendDOMEventCallbackID);
        g_dbus_connection_signal_unsubscribe(connection, m_willCompleteCallbackID);
    }

    static void testFormSubmissionResult(GVariant* result)
    {
        const char* formID;
        const char* concatenatedTextFieldNames;
        const char* concatenatedTextFieldValues;
        gboolean targetFrameIsMainFrame;
        gboolean sourceFrameIsMainFrame;
        g_variant_get(result, "(&s&s&sbb)", &formID, &concatenatedTextFieldNames, &concatenatedTextFieldValues, &targetFrameIsMainFrame, &sourceFrameIsMainFrame);

        g_assert_cmpstr(formID, ==, FORM_SUBMISSION_TEST_ID);
        g_assert_cmpstr(concatenatedTextFieldNames, ==, "foo,bar,");
        g_assert_cmpstr(concatenatedTextFieldValues, ==, "first,second,");
        g_assert_false(targetFrameIsMainFrame);
        g_assert_true(sourceFrameIsMainFrame);
    }

    static void willSendDOMEventCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* result, FormSubmissionTest* test)
    {
        test->m_willSendDOMEventCallbackExecuted = true;
        testFormSubmissionResult(result);
    }

    static void willCompleteCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* result, FormSubmissionTest* test)
    {
        test->m_willCompleteCallbackExecuted = true;
        testFormSubmissionResult(result);
        test->quitMainLoop();
    }

    void runJavaScriptAndWaitUntilFormSubmitted(const char* js)
    {
        webkit_web_view_run_javascript(m_webView, js, nullptr, nullptr, nullptr);
        g_main_loop_run(m_mainLoop);
    }

    GRefPtr<GDBusProxy> m_proxy;
    guint m_willSendDOMEventCallbackID { 0 };
    guint m_willCompleteCallbackID { 0 };
    bool m_willSendDOMEventCallbackExecuted { false };
    bool m_willCompleteCallbackExecuted { false };
};

static void testWebExtensionFormSubmissionSteps(FormSubmissionTest* test, gconstpointer)
{
    test->loadHtml("<form id=\"" FORM_SUBMISSION_TEST_ID "\" target=\"target_frame\">"
        "<input type=\"text\" name=\"foo\" value=\"first\">"
        "<input type=\"text\" name=\"bar\" value=\"second\">"
        "<input type=\"submit\" id=\"submit_button\">"
        "</form>"
        "<iframe name=\"target_frame\"></iframe>", nullptr);
    test->waitUntilLoadFinished();

    static const char* submitFormScript =
        "var form = document.getElementById(\"" FORM_SUBMISSION_TEST_ID "\");"
        "form.submit();";
    test->runJavaScriptAndWaitUntilFormSubmitted(submitFormScript);
    // Submit must not be emitted when the form is submitted via JS.
    // https://developer.mozilla.org/en-US/docs/Web/API/HTMLFormElement/submit
    g_assert_false(test->m_willSendDOMEventCallbackExecuted);
    g_assert_true(test->m_willCompleteCallbackExecuted);
    test->m_willCompleteCallbackExecuted = false;

    static const char* manuallySubmitFormScript =
        "var button = document.getElementById(\"submit_button\");"
        "button.click();";
    test->runJavaScriptAndWaitUntilFormSubmitted(manuallySubmitFormScript);
    g_assert_true(test->m_willSendDOMEventCallbackExecuted);
    g_assert_true(test->m_willCompleteCallbackExecuted);
    test->m_willSendDOMEventCallbackExecuted = false;
    test->m_willCompleteCallbackExecuted = false;

    test->loadHtml("<form id=\"" FORM_SUBMISSION_TEST_ID "\" target=\"target_frame\">"
        "</form>"
        "<iframe name=\"target_frame\"></iframe>", nullptr);
    test->waitUntilLoadFinished();
}

void beforeAll()
{
    bus = new WebKitTestBus();
    if (!bus->run())
        return;

    WebViewTest::add("WebKitWebExtension", "dom-document-title", testWebExtensionGetTitle);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitWebExtension", "dom-input-element-is-user-edited", testWebExtensionInputElementIsUserEdited);
#endif
    WebViewTest::add("WebKitWebExtension", "document-loaded-signal", testDocumentLoadedSignal);
    WebViewTest::add("WebKitWebView", "web-process-crashed", testWebKitWebViewProcessCrashed);
    WebViewTest::add("WebKitWebExtension", "window-object-cleared", testWebExtensionWindowObjectCleared);
    WebViewTest::add("WebKitWebExtension", "isolated-world", testWebExtensionIsolatedWorld);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitWebView", "install-missing-plugins-permission-request", testInstallMissingPluginsPermissionRequest);
#endif
    WebViewTest::add("WebKitWebExtension", "form-controls-associated-signal", testWebExtensionFormControlsAssociated);
    FormSubmissionTest::add("WebKitWebExtension", "form-submission-steps", testWebExtensionFormSubmissionSteps);
}

void afterAll()
{
    delete bus;
}
