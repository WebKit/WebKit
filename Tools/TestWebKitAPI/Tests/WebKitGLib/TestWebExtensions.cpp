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

#include "WebViewTest.h"
#include <gio/gunixfdlist.h>
#include <wtf/URL.h>
#include <wtf/glib/GRefPtr.h>

static GUniquePtr<char> scriptDialogResult;

#define INPUT_ID "input-id"
#define FORM_ID "form-id"
#define FORM2_ID "form2-id"

#define FORM_SUBMISSION_TEST_ID "form-submission-test-id"

static void checkTitle(WebViewTest* test, GDBusProxy* proxy, const char* expectedTitle)
{
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy,
        "GetTitle",
        g_variant_new("(t)", webkit_web_view_get_page_id(test->m_webView)),
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert_nonnull(result);

    const char* title;
    g_variant_get(result.get(), "(&s)", &title);
    g_assert_cmpstr(title, ==, expectedTitle);
}

static void testWebExtensionGetTitle(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><head><title>WebKitGTK Web Extensions Test</title></head><body></body></html>", "http://bar.com");
    test->waitUntilLoadFinished();

    auto proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "WebKitGTK Web Extensions Test");
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
    test->showInWindow();
    test->loadHtml("<html><body id='body'><input id='input'></input><textarea id='textarea'></textarea></body></html>", nullptr);
    test->waitUntilLoadFinished();

    auto proxy = test->extensionProxy();

    uint64_t pageID = webkit_web_view_get_page_id(test->m_webView);
    g_assert_false(inputElementIsUserEdited(proxy.get(), pageID, "input"));
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('input').focus()", nullptr);
    test->keyStroke(GDK_KEY_a);
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
    GUniquePtr<char> resultString;
    do {
        auto* result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('input').value", nullptr);
        resultString.reset(WebViewTest::javascriptResultToCString(result));
    } while (g_strcmp0(resultString.get(), "a"));
    g_assert_true(inputElementIsUserEdited(proxy.get(), pageID, "input"));

    g_assert_false(inputElementIsUserEdited(proxy.get(), pageID, "textarea"));
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('textarea').focus()", nullptr);
    test->keyStroke(GDK_KEY_b);
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
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
    auto proxy = test->extensionProxy();

    GDBusConnection* connection = g_dbus_proxy_get_connection(proxy.get());
    guint id = g_dbus_connection_signal_subscribe(connection,
        nullptr,
        "org.webkit.gtk.WebExtensionTest",
        "DocumentLoaded",
        "/org/webkit/gtk/WebExtensionTest",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        reinterpret_cast<GDBusSignalCallback>(documentLoadedCallback),
        test,
        nullptr);
    g_assert_cmpuint(id, !=, 0);

    test->loadHtml("<html><head><title>WebKitGTK Web Extensions Test</title></head><body></body></html>", nullptr);
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
    test->loadHtml("<html></html>", nullptr);
    test->waitUntilLoadFinished();

    g_signal_connect_after(test->m_webView, "web-process-terminated",
        G_CALLBACK(webProcessTerminatedCallback), test);

    test->m_expectedWebProcessCrash = true;

    auto proxy = test->extensionProxy();
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(
        proxy.get(),
        "AbortProcess",
        0,
        G_DBUS_CALL_FLAGS_NONE,
        -1, 0, 0));
    g_assert_null(result);
    Test::removeLogFatalFlag(G_LOG_LEVEL_WARNING);
    g_main_loop_run(test->m_mainLoop);
    Test::addLogFatalFlag(G_LOG_LEVEL_WARNING);
    test->m_expectedWebProcessCrash = false;
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
    auto proxy = test->extensionProxy();
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
    auto proxy = test->extensionProxy();
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(proxy.get(), "RemoveAVPluginsFromGSTRegistry",
        nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr));

    test->showInWindow();

    gulong permissionRequestSignalID = g_signal_connect(test->m_webView, "permission-request", G_CALLBACK(permissionRequestCallback), test);
    // FIXME: the base URI needs to finish with / to work, that shouldn't happen.
    GUniquePtr<char> baseURI(g_strconcat("file://", Test::getResourcesDir(Test::WebKit2Resources).data(), "/", nullptr));
    test->loadHtml("<html><body><video src=\"test.mp4\" autoplay></video></body></html>", baseURI.get());
    g_main_loop_run(test->m_mainLoop);
    g_signal_handler_disconnect(test->m_webView, permissionRequestSignalID);
}
#endif // PLATFORM(GTK)

static void didAssociateFormControlsCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* result, GUniqueOutPtr<char>* formIds)
{
    g_variant_get(result, "(s)", &formIds->outPtr());
}

static void testWebExtensionFormControlsAssociated(WebViewTest* test, gconstpointer)
{
    GUniqueOutPtr<char> formIds;
    auto proxy = test->extensionProxy();
    GDBusConnection* connection = g_dbus_proxy_get_connection(proxy.get());
    guint id = g_dbus_connection_signal_subscribe(connection,
        nullptr,
        "org.webkit.gtk.WebExtensionTest",
        "FormControlsAssociated",
        "/org/webkit/gtk/WebExtensionTest",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        reinterpret_cast<GDBusSignalCallback>(didAssociateFormControlsCallback),
        &formIds,
        nullptr);
    g_assert_cmpuint(id, !=, 0);

    test->loadHtml("<!DOCTYPE html><head><title>WebKitGTK Web Extensions Test</title></head><div id=\"placeholder\"/>", 0);
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

    test->runJavaScriptAndWaitUntilFinished(addFormScript, nullptr);
    while (!formIds)
        g_main_context_iteration(nullptr, TRUE);
    g_assert_true(!g_strcmp0(formIds.get(), FORM_ID FORM2_ID) || !g_strcmp0(formIds.get(), FORM2_ID FORM_ID));

    // GUniqueOutPtr doesn't have a clear().
    GUniquePtr<char> deleter(formIds.release());

    static const char* moveFormElementScript =
        "var form = document.getElementById(\"" FORM_ID "\");"
        "var form2 = document.getElementById(\"" FORM2_ID "\");"
        "var input = document.getElementById(\"" INPUT_ID "\");"
        "form.removeChild(input);"
        "form2.appendChild(input);";

    test->runJavaScriptAndWaitUntilFinished(moveFormElementScript, nullptr);
    while (!formIds)
        g_main_context_iteration(nullptr, TRUE);
    g_assert_cmpstr(formIds.get(), ==, INPUT_ID);

    g_dbus_connection_signal_unsubscribe(connection, id);
}

class FormSubmissionTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FormSubmissionTest);

    FormSubmissionTest()
    {
        m_proxy = extensionProxy();
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

static void webViewPageIDChanged(WebKitWebView* webView, GParamSpec*, bool* pageIDChangedEmitted)
{
    *pageIDChangedEmitted = true;
    g_assert_nonnull(Test::s_dbusConnectionPageMap.get(webkit_web_view_get_page_id(webView)));
}

static void testWebExtensionPageID(WebViewTest* test, gconstpointer)
{
    auto pageID = webkit_web_view_get_page_id(test->m_webView);
    g_assert_cmpuint(pageID, >=, 1);

    bool pageIDChangedEmitted = false;
    g_signal_connect(test->m_webView, "notify::page-id", G_CALLBACK(webViewPageIDChanged), &pageIDChangedEmitted);

    test->loadHtml("<html><head><title>Title1</title></head><body></body></html>", "http://foo.com");
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    auto proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title1");

    test->loadHtml("<html><head><title>Title2</title></head><body></body></html>", "http://foo.com/bar");
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    checkTitle(test, proxy.get(), "Title2");

    test->loadHtml("<html><head><title>Title3</title></head><body></body></html>", "http://bar.com");
    test->waitUntilLoadFinished();
    g_assert_true(pageIDChangedEmitted);
    pageIDChangedEmitted = false;
    g_assert_cmpuint(pageID, <, webkit_web_view_get_page_id(test->m_webView));
    pageID = webkit_web_view_get_page_id(test->m_webView);
    proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title3");

    test->loadHtml("<html><head><title>Title4</title></head><body></body></html>", "http://bar.com/foo");
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    checkTitle(test, proxy.get(), "Title4");

    // Register a custom URI scheme to test history navigation.
    webkit_web_context_register_uri_scheme(test->m_webContext.get(), "foo",
        [](WebKitURISchemeRequest* request, gpointer) {
            URL url = URL(String::fromLatin1(webkit_uri_scheme_request_get_uri(request)));
            GRefPtr<GInputStream> inputStream = adoptGRef(g_memory_input_stream_new());
            char* html = g_strdup_printf("<html><head><title>%s</title></head><body></body></html>", url.host() == "host5"_s ? "Title5" : "Title6");
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), html, strlen(html), g_free);
            webkit_uri_scheme_request_finish(request, inputStream.get(), strlen(html), "text/html");
        }, nullptr, nullptr);

    test->loadURI("foo://host5/");
    test->waitUntilLoadFinished();
    g_assert_true(pageIDChangedEmitted);
    pageIDChangedEmitted = false;
    g_assert_cmpuint(pageID, <, webkit_web_view_get_page_id(test->m_webView));
    pageID = webkit_web_view_get_page_id(test->m_webView);
    proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title5");

    test->loadURI("foo://host6/");
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    pageIDChangedEmitted = false;
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    pageID = webkit_web_view_get_page_id(test->m_webView);
    proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title6");

    test->goBack();
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    pageIDChangedEmitted = false;
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    pageID = webkit_web_view_get_page_id(test->m_webView);
    proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title5");

    test->goForward();
    test->waitUntilLoadFinished();
    g_assert_false(pageIDChangedEmitted);
    pageIDChangedEmitted = false;
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(test->m_webView));
    pageID = webkit_web_view_get_page_id(test->m_webView);
    proxy = test->extensionProxy();
    checkTitle(test, proxy.get(), "Title6");
}

class UserMessageTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(UserMessageTest);

    static gboolean webViewUserMessageReceivedCallback(WebKitWebView*, WebKitUserMessage* message, UserMessageTest* test)
    {
        return test->viewUserMessageReceived(message);
    }

    static gboolean webContextUserMessageReceivedCallback(WebKitWebContext*, WebKitUserMessage* message, UserMessageTest* test)
    {
        return test->contextUserMessageReceived(message);
    }

    UserMessageTest()
    {
        g_signal_connect(m_webContext.get(), "user-message-received", G_CALLBACK(webContextUserMessageReceivedCallback), this);
        g_signal_connect(m_webView, "user-message-received", G_CALLBACK(webViewUserMessageReceivedCallback), this);
    }

    ~UserMessageTest()
    {
        g_signal_handlers_disconnect_matched(m_webContext.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    WebKitUserMessage* sendMessage(WebKitUserMessage* message, GError** error = nullptr)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(message));
        m_receivedViewMessages = { };
        webkit_web_view_send_message_to_page(m_webView, message, nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
            auto* test = static_cast<UserMessageTest*>(userData);
            test->m_receivedViewMessages.append(adoptGRef(webkit_web_view_send_message_to_page_finish(test->m_webView, result, &test->m_receivedError.outPtr())));
            if (auto receivedMessage = test->m_receivedViewMessages.first())
                test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(receivedMessage.get()));
            else
                g_assert_nonnull(test->m_receivedError.get());
            test->quitMainLoop();
        }, this);
        g_main_loop_run(m_mainLoop);
        if (error)
            *error = m_receivedError.get();
        return m_receivedViewMessages.first().get();
    }

    void sendMessageToAllExtensions(WebKitUserMessage* message)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(message));
        webkit_web_context_send_message_to_all_extensions(m_webContext.get(), message);
    }

    bool viewUserMessageReceived(WebKitUserMessage* message)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(message));
        if (m_expectedViewMessageNames.isEmpty())
            return false;

        if (m_expectedViewMessageNames.contains(webkit_user_message_get_name(message))) {
            m_receivedViewMessages.append(message);
            if (m_receivedViewMessages.size() == m_expectedViewMessageNames.size())
                quitMainLoop();
            return true;
        }
        return false;
    }

    bool contextUserMessageReceived(WebKitUserMessage* message)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(message));
        if (!g_strcmp0(m_expectedContextMessageName.data(), webkit_user_message_get_name(message))) {
            m_receivedContextMessage = message;
            quitMainLoop();

            return true;
        }

        return false;
    }

    const Vector<GRefPtr<WebKitUserMessage>>& waitUntilViewMessagesReceived(Vector<CString>&& messageNames)
    {
        m_expectedViewMessageNames = WTFMove(messageNames);
        m_receivedViewMessages = { };
        g_main_loop_run(m_mainLoop);
        m_expectedViewMessageNames = { };
        return m_receivedViewMessages;
    }

    WebKitUserMessage* waitUntilViewMessageReceived(const char* messageName)
    {
        return waitUntilViewMessagesReceived({ messageName }).first().get();
    }

    WebKitUserMessage* waitUntilContextMessageReceived(const char* messageName)
    {
        m_expectedContextMessageName = messageName;
        g_main_loop_run(m_mainLoop);
        m_expectedContextMessageName = { };
        return m_receivedContextMessage.get();
    }

    Vector<GRefPtr<WebKitUserMessage>> m_receivedViewMessages;
    GRefPtr<WebKitUserMessage> m_receivedContextMessage;
    GUniqueOutPtr<GError> m_receivedError;
    Vector<CString> m_expectedViewMessageNames;
    CString m_expectedViewMessageName;
    CString m_expectedContextMessageName;
};

static bool readFileDescriptor(int fd, char** contents, gsize* length)
{
    GString* bufferString = g_string_new(nullptr);
    while (true) {
        gchar buffer[4096];
        gssize bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;

            g_string_free(bufferString, TRUE);

            return false;
        }

        if (!bytesRead)
            break;

        g_string_append_len(bufferString, buffer, bytesRead);
    }

    *length = bufferString->len;
    *contents = g_string_free(bufferString, FALSE);

    return true;
}

static void testWebExtensionUserMessages(UserMessageTest* test, gconstpointer)
{
    // Normal message with a reply.
    auto* message = webkit_user_message_new("Test.Hello", g_variant_new("s", "WebProcess"));
    g_assert_cmpstr(webkit_user_message_get_name(message), ==, "Test.Hello");
    auto* parameters = webkit_user_message_get_parameters(message);
    g_assert_nonnull(parameters);
    const char* parameter = nullptr;
    g_variant_get(parameters, "&s", &parameter);
    g_assert_cmpstr(parameter, ==, "WebProcess");
    g_assert_null(webkit_user_message_get_fd_list(message));
    auto* reply = test->sendMessage(message);
    g_assert_true(WEBKIT_IS_USER_MESSAGE(reply));
    g_assert_cmpstr(webkit_user_message_get_name(reply), ==, "Test.Hello");
    parameters = webkit_user_message_get_parameters(reply);
    g_assert_nonnull(parameters);
    parameter = nullptr;
    g_variant_get(parameters, "&s", &parameter);
    g_assert_cmpstr(parameter, ==, "UIProcess");
    g_assert_null(webkit_user_message_get_fd_list(reply));

    // Message with no parameters.
    message = webkit_user_message_new("Test.Ping", nullptr);
    g_assert_null(webkit_user_message_get_parameters(message));
    reply = test->sendMessage(message);
    g_assert_true(WEBKIT_IS_USER_MESSAGE(reply));
    g_assert_cmpstr(webkit_user_message_get_name(reply), ==, "Test.Pong");
    g_assert_null(webkit_user_message_get_parameters(reply));

    // Message with maybe type in parameters.
    message = webkit_user_message_new("Test.Optional", g_variant_new("(sms)", "Hello", "World"));
    reply = test->sendMessage(message);
    g_assert_true(WEBKIT_IS_USER_MESSAGE(reply));
    g_assert_cmpstr(webkit_user_message_get_name(reply), ==, "Test.Optional");
    parameters = webkit_user_message_get_parameters(reply);
    g_assert_nonnull(parameters);
    g_variant_get(parameters, "&s", &parameter);
    g_assert_cmpstr(parameter, ==, "World");
    message = webkit_user_message_new("Test.Optional", g_variant_new("(sms)", "Hello", nullptr));
    reply = test->sendMessage(message);
    g_assert_true(WEBKIT_IS_USER_MESSAGE(reply));
    g_assert_cmpstr(webkit_user_message_get_name(reply), ==, "Test.Optional");
    parameters = webkit_user_message_get_parameters(reply);
    g_assert_nonnull(parameters);
    g_variant_get(parameters, "&s", &parameter);
    g_assert_cmpstr(parameter, ==, "NULL");

    // Message with file descriptors.
    GUniquePtr<char> filename(g_build_filename(Test::getResourcesDir().data(), "simple.json", nullptr));
    reply = test->sendMessage(webkit_user_message_new("Test.OpenFile", g_variant_new("s", filename.get())));
    g_assert_true(WEBKIT_IS_USER_MESSAGE(reply));
    parameters = webkit_user_message_get_parameters(reply);
    g_assert_nonnull(parameters);
    gint32 handle;
    g_variant_get(parameters, "h", &handle);
    g_assert_cmpint(handle, ==, 0);
    auto* fdList = webkit_user_message_get_fd_list(reply);
    g_assert_true(G_IS_UNIX_FD_LIST(fdList));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(fdList));
    g_assert_cmpint(g_unix_fd_list_get_length(fdList), ==, 1);
    GUniqueOutPtr<GError> error;
    int fd = g_unix_fd_list_get(fdList, handle, &error.outPtr());
    g_assert_cmpint(fd, !=, -1);
    g_assert_no_error(error.get());
    GUniqueOutPtr<char> fdContents;
    gsize fdContentsLength;
    g_assert_true(readFileDescriptor(fd, &fdContents.outPtr(), &fdContentsLength));
    close(fd);
    GUniqueOutPtr<char> fileContents;
    gsize fileContentsLength;
    g_assert_true(g_file_get_contents(filename.get(), &fileContents.outPtr(), &fileContentsLength, nullptr));
    g_assert_cmpmem(fdContents.get(), fdContentsLength, fileContents.get(), fileContentsLength);

    // Unhandled message.
    GError* messageError = nullptr;
    reply = test->sendMessage(webkit_user_message_new("Test.Invalid", nullptr), &messageError);
    g_assert_null(reply);
    g_assert_error(messageError, WEBKIT_USER_MESSAGE_ERROR, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE);

    // Message that is never replied.
    GRefPtr<WebKitWebView> webView = WEBKIT_WEB_VIEW(Test::createWebView(test->m_webContext.get()));
    webkit_web_view_send_message_to_page(webView.get(), webkit_user_message_new("Test.Infinite", nullptr), nullptr,
        [](GObject* object, GAsyncResult* result, gpointer userData) {
            auto* test = static_cast<UserMessageTest*>(userData);
            GUniqueOutPtr<GError> error;
            g_assert_null(webkit_web_view_send_message_to_page_finish(WEBKIT_WEB_VIEW(object), result, &error.outPtr()));
            g_assert_error(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED);
            test->quitMainLoop();
        }, test);
    g_main_loop_run(test->m_mainLoop);

    // Wait for received message.
    test->loadHtml("<html><body></body></html>", nullptr);
    message = test->waitUntilViewMessageReceived("DocumentLoaded");
    g_assert_true(WEBKIT_IS_USER_MESSAGE(message));
    g_assert_cmpstr(webkit_user_message_get_name(message), ==, "DocumentLoaded");

    // Reply to a message received from the web process.
    webkit_web_view_send_message_to_page(test->m_webView, webkit_user_message_new("Test.AsyncPing", nullptr), nullptr, nullptr, nullptr);
    message = test->waitUntilViewMessageReceived("Test.Ping");
    g_assert_true(WEBKIT_IS_USER_MESSAGE(message));
    g_assert_cmpstr(webkit_user_message_get_name(message), ==, "Test.Ping");
    webkit_user_message_send_reply(message, webkit_user_message_new("Test.Pong", nullptr));
    message = test->waitUntilViewMessageReceived("Test.AsyncPong");
    g_assert_true(WEBKIT_IS_USER_MESSAGE(message));
    g_assert_cmpstr(webkit_user_message_get_name(message), ==, "Test.AsyncPong");

    // Create a new page and wait for page created message.
    webView = WEBKIT_WEB_VIEW(Test::createWebView(test->m_webContext.get()));
    webkit_web_view_load_html(webView.get(), "<html><body></body></html>", nullptr);
    message = test->waitUntilContextMessageReceived("PageCreated");
    g_assert_true(WEBKIT_IS_USER_MESSAGE(message));
    g_assert_cmpstr(webkit_user_message_get_name(message), ==, "PageCreated");
    parameters = webkit_user_message_get_parameters(message);
    g_assert_nonnull(parameters);
    guint64 pageID;
    g_variant_get(parameters, "(t)", &pageID);
    g_assert_cmpuint(pageID, ==, webkit_web_view_get_page_id(webView.get()));

    // Request to start a ping to all processes.
    test->sendMessageToAllExtensions(webkit_user_message_new("Test.RequestPing", nullptr));
    // We should received two ping requests.
    GRefPtr<WebKitUserMessage> ping1 = test->waitUntilContextMessageReceived("Ping");
    GRefPtr<WebKitUserMessage> ping2 = test->waitUntilContextMessageReceived("Ping");
    webkit_user_message_send_reply(ping1.get(), webkit_user_message_new("Pong", nullptr));
    test->waitUntilContextMessageReceived("Test.FinishedPingRequest");
    webkit_user_message_send_reply(ping2.get(), webkit_user_message_new("Pong", nullptr));
    test->waitUntilContextMessageReceived("Test.FinishedPingRequest");
}

static void testWebExtensionWindowObjectCleared(UserMessageTest* test, gconstpointer)
{
    test->loadHtml("<html><header></header><body></body></html>", 0);

    auto messages = test->waitUntilViewMessagesReceived({ "WindowObjectCleared", "WindowObjectClearedIsolatedWorld" });
    g_assert_cmpuint(messages.size(), ==, 2);

    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("window.echo('Foo');", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "Foo");

    javascriptResult = test->runJavaScriptAndWaitUntilFinished("var f = new GFile('.'); f.path();", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    GUniquePtr<char> currentDirectory(g_get_current_dir());
    g_assert_cmpstr(valueString.get(), ==, currentDirectory.get());
}

void beforeAll()
{
    WebViewTest::add("WebKitWebExtension", "dom-document-title", testWebExtensionGetTitle);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitWebExtension", "dom-input-element-is-user-edited", testWebExtensionInputElementIsUserEdited);
#endif
    WebViewTest::add("WebKitWebExtension", "document-loaded-signal", testDocumentLoadedSignal);
    WebViewTest::add("WebKitWebView", "web-process-crashed", testWebKitWebViewProcessCrashed);
    UserMessageTest::add("WebKitWebExtension", "window-object-cleared", testWebExtensionWindowObjectCleared);
    WebViewTest::add("WebKitWebExtension", "isolated-world", testWebExtensionIsolatedWorld);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitWebView", "install-missing-plugins-permission-request", testInstallMissingPluginsPermissionRequest);
#endif
    WebViewTest::add("WebKitWebExtension", "form-controls-associated-signal", testWebExtensionFormControlsAssociated);
    FormSubmissionTest::add("WebKitWebExtension", "form-submission-steps", testWebExtensionFormSubmissionSteps);
    WebViewTest::add("WebKitWebExtension", "page-id", testWebExtensionPageID);
    UserMessageTest::add("WebKitWebExtension", "user-messages", testWebExtensionUserMessages);
}

void afterAll()
{
}
